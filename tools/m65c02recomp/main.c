/* m65c02recomp - Atari Lynx static-recompiler driver (phase 1).
 *
 * Subcommands:
 *   info <file.lnx>                  parse and print the .lnx header
 *   dis  <file.lnx> [start] [count]  linear-sweep disassemble the cart image
 *   emit <file.lnx> <outdir>         write generated/recomp_funcs.{c,h} skeleton
 *
 * `start` is a byte offset into the cart image (default 0); `count` is the
 * instruction count (default: a screenful). This is the working spine of the
 * recompiler - the decoder, .lnx parser and analyzer are real; the C emitter
 * is a phase-1 placeholder (see emit.c / docs/ROADMAP.md).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lnx.h"
#include "decode.h"
#include "analyze.h"
#include "emit.h"
#include "lynxdec.h"

static uint8_t *read_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    uint8_t *buf = (uint8_t *)malloc((size_t)n);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return NULL; }
    fclose(f);
    *out_size = (size_t)n;
    return buf;
}

static void print_info(const lnx_info_t *in, size_t file_size) {
    printf("file size      : %zu bytes\n", file_size);
    if (in->valid) {
        printf("container      : BLL .lnx (header %d bytes)\n", LNX_HEADER_SIZE);
        printf("cart name      : %s\n", in->cartname);
        printf("manufacturer   : %s\n", in->manufname);
        printf("version        : %u\n", in->version);
        printf("bank0 page size: %u bytes\n", in->page_size_bank0);
        printf("bank1 page size: %u bytes\n", in->page_size_bank1);
        printf("rotation       : %u\n", in->rotation);
    } else {
        printf("container      : raw / headerless image\n");
    }
    printf("cart image     : %zu bytes\n", in->rom_size);
}

static void dis_cb(const insn_t *in, void *user) {
    (void)user;
    char text[48];
    m65c02_format(in, text, sizeof(text));
    printf("%04X  %02X %s\n", in->pc, in->opcode, text);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr,
            "m65c02recomp - Atari Lynx static recompiler\n"
            "usage:\n"
            "  %s info    <file.lnx>\n"
            "  %s dis     <file.lnx> [start_off] [count]\n"
            "  %s decrypt <file.lnx> <loader.bin>   recover the boot loader\n"
            "  %s loader  <file.lnx> [count]         decrypt + disassemble loader\n"
            "  %s recomp  <file.lnx> <outdir>        decrypt + discover + emit C\n"
            "  %s recompbin <raw.bin> <baseHex> <outdir> [seeds...]\n"
            "  %s emit    <file.lnx> <outdir>\n",
            argv[0], argv[0], argv[0], argv[0], argv[0], argv[0], argv[0]);
        return 2;
    }

    const char *cmd  = argv[1];
    const char *path = argv[2];

    size_t size = 0;
    uint8_t *data = read_file(path, &size);
    if (!data) { fprintf(stderr, "error: cannot read %s\n", path); return 1; }

    lnx_info_t info;
    if (lnx_parse(data, size, &info) != 0) {
        fprintf(stderr, "error: not a valid image\n");
        free(data);
        return 1;
    }

    int rc = 0;
    if (strcmp(cmd, "info") == 0) {
        print_info(&info, size);
    } else if (strcmp(cmd, "dis") == 0) {
        size_t start = (argc > 3) ? (size_t)strtoul(argv[3], NULL, 0) : 0;
        size_t count = (argc > 4) ? (size_t)strtoul(argv[4], NULL, 0) : 32;
        /* Optional 4th arg: the CPU base address that offset 0 maps to
         * (e.g. 0xFE00 to disassemble the boot ROM). Default 0x0200. */
        uint16_t base = (argc > 5) ? (uint16_t)strtoul(argv[5], NULL, 0) : 0x0200;
        printf("; linear sweep from offset 0x%zX, base $%04X\n", start, base);
        analyze_linear(info.rom, info.rom_size, base, start, count, dis_cb, NULL);
    } else if (strcmp(cmd, "decrypt") == 0) {
        if (argc < 4) { fprintf(stderr, "error: decrypt needs <loader.bin>\n"); rc = 2; }
        else {
            unsigned char out[5 * 50];
            int n = lynx_decrypt_loader(info.rom, info.rom_size, out, sizeof(out));
            if (n < 0) { fprintf(stderr, "error: decrypt failed\n"); rc = 1; }
            else {
                FILE *f = fopen(argv[3], "wb");
                if (!f) { fprintf(stderr, "error: cannot write %s\n", argv[3]); rc = 1; }
                else {
                    fwrite(out, 1, (size_t)n, f);
                    fclose(f);
                    printf("decrypted %d-byte loader (%d blocks) -> %s\n",
                           n, n / 50, argv[3]);
                }
            }
        }
    } else if (strcmp(cmd, "loader") == 0) {
        unsigned char out[5 * 50];
        int n = lynx_decrypt_loader(info.rom, info.rom_size, out, sizeof(out));
        if (n < 0) { fprintf(stderr, "error: decrypt failed\n"); rc = 1; }
        else {
            size_t count = (argc > 3) ? (size_t)strtoul(argv[3], NULL, 0) : 64;
            printf("; decrypted boot loader, %d bytes, entry $%04X\n",
                   n, LYNX_LOADER_BASE);
            analyze_linear(out, (size_t)n, LYNX_LOADER_BASE, 0, count, dis_cb, NULL);
        }
    } else if (strcmp(cmd, "recomp") == 0) {
        if (argc < 4) { fprintf(stderr, "error: recomp needs <outdir>\n"); rc = 2; }
        else {
            unsigned char loader[5 * 50];
            int n = lynx_decrypt_loader(info.rom, info.rom_size, loader, sizeof(loader));
            if (n < 0) { fprintf(stderr, "error: decrypt failed\n"); rc = 1; }
            else {
                /* Seeds: the boot entry ($0200) and the post-boot-ROM
                 * continuation ($020E) the boot ROM returns into. */
                static func_table_t tab;
                uint16_t seeds[2] = { LYNX_LOADER_BASE, LYNX_LOADER_BASE + 0x0E };
                int nf = analyze_discover(loader, (size_t)n, LYNX_LOADER_BASE,
                                          seeds, 2, &tab);
                if (emit_functions(argv[3], loader, (size_t)n, LYNX_LOADER_BASE,
                                   &tab, path) != 0) {
                    fprintf(stderr, "error: emit failed\n"); rc = 1;
                } else {
                    printf("recompiled %d functions (from %d-byte loader) -> %s\n",
                           nf, n, argv[3]);
                    printf("  external targets: %d\n", tab.next);
                }
            }
        }
    } else if (strcmp(cmd, "recompbin") == 0) {
        /* recompbin <raw.bin> <baseHex> <outdir> [seedHex ...]
         * Recompile a raw, already-decrypted code image (no .lnx framing). The
         * same path a full post-boot RAM image will take. `data` here is the
         * raw file (lnx_parse treated it as headerless: info.rom == data). */
        if (argc < 5) { fprintf(stderr, "error: recompbin needs <baseHex> <outdir> [seeds...]\n"); rc = 2; }
        else {
            uint16_t base = (uint16_t)strtoul(argv[3], NULL, 0);
            static func_table_t tab;
            uint16_t seeds[16]; size_t ns = 0;
            for (int i = 5; i < argc && ns < 16; i++)
                seeds[ns++] = (uint16_t)strtoul(argv[i], NULL, 0);
            if (ns == 0) seeds[ns++] = base;     /* default: entry at base */
            int nf = analyze_discover(info.rom, info.rom_size, base, seeds, ns, &tab);
            if (emit_functions(argv[4], info.rom, info.rom_size, base, &tab, path) != 0) {
                fprintf(stderr, "error: emit failed\n"); rc = 1;
            } else {
                printf("recompiled %d functions (%zu bytes @ $%04X) -> %s\n",
                       nf, info.rom_size, base, argv[4]);
            }
        }
    } else if (strcmp(cmd, "emit") == 0) {
        if (argc < 4) { fprintf(stderr, "error: emit needs <outdir>\n"); rc = 2; }
        else rc = emit_skeleton(argv[3], &info, path) == 0 ? 0 : 1;
        if (rc == 0) printf("wrote recomp_funcs.{c,h} skeleton to %s\n", argv[3]);
    } else {
        fprintf(stderr, "error: unknown command '%s'\n", cmd);
        rc = 2;
    }

    free(data);
    return rc;
}
