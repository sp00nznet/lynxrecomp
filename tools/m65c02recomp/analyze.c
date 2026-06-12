/* analyze.c - linear sweep + recursive-descent function discovery. See analyze.h. */
#include "analyze.h"
#include <string.h>

size_t analyze_linear(const uint8_t *rom, size_t rom_size, uint16_t base,
                      size_t start_off, size_t count,
                      insn_cb cb, void *user) {
    size_t off = start_off;
    size_t n   = 0;
    while (off < rom_size && (count == 0 || n < count)) {
        insn_t in;
        /* Guard the 1-2 trailing operand bytes against running off the end. */
        uint8_t tmp[3] = {0, 0, 0};
        size_t avail = rom_size - off;
        tmp[0] = rom[off];
        if (avail > 1) tmp[1] = rom[off + 1];
        if (avail > 2) tmp[2] = rom[off + 2];

        m65c02_decode(tmp, (uint16_t)(base + off), &in);
        if (in.len > avail) break;     /* truncated final instruction */
        if (cb) cb(&in, user);
        off += in.len;
        n++;
    }
    return n;
}

/* ---- recursive-descent discovery ---- */

typedef struct {
    uint16_t       base;
    const uint8_t *rom;
    size_t         rom_size;
    uint16_t       work[MAX_FUNCS];
    int            nwork;
    uint8_t        seen[0x10000];   /* function-start seen bitmap */
} disc_t;

static int in_image(const disc_t *d, uint16_t a) {
    return a >= d->base && (size_t)(a - d->base) < d->rom_size;
}

static void push_func(disc_t *d, func_table_t *t, uint16_t a) {
    if (!in_image(d, a) || d->seen[a]) return;
    if (d->nwork >= MAX_FUNCS || t->nfuncs >= MAX_FUNCS) return;
    d->seen[a] = 1;
    d->work[d->nwork++] = a;
}

static void add_ext(func_table_t *t, uint16_t a) {
    for (int i = 0; i < t->next; i++) if (t->ext[i] == a) return;
    if (t->next < MAX_FUNCS) t->ext[t->next++] = a;
}

static void add_label(func_t *f, uint16_t a) {
    for (int i = 0; i < f->nlabels; i++) if (f->labels[i] == a) return;
    if (f->nlabels < MAX_LABELS) f->labels[f->nlabels++] = a;
}

/* Decode a single function starting at `fs`, extending past intra-function
 * jumps until a terminator is reached with no pending forward branch targets. */
static void discover_one(disc_t *d, func_table_t *t, uint16_t fs) {
    if (t->nfuncs >= MAX_FUNCS) return;
    func_t *f = &t->funcs[t->nfuncs];
    memset(f, 0, sizeof(*f));
    f->start = fs;

    uint16_t addr = fs;
    uint16_t maxt = fs;     /* highest forward target that must be included */
    uint16_t end  = fs;

    while (in_image(d, addr) && (uint16_t)(addr - fs) < 0x1000) {
        size_t off = (size_t)(addr - d->base);
        uint8_t tmp[3] = {0, 0, 0};
        size_t avail = d->rom_size - off;
        tmp[0] = d->rom[off];
        if (avail > 1) tmp[1] = d->rom[off + 1];
        if (avail > 2) tmp[2] = d->rom[off + 2];

        insn_t in;
        m65c02_decode(tmp, addr, &in);
        if (in.len > avail) break;

        uint16_t next = (uint16_t)(addr + in.len);
        if (next > end) end = next;

        int terminator = 0;
        switch (in.cflow) {
            case CF_CALL:
                if (in_image(d, in.target)) push_func(d, t, in.target);
                else                        add_ext(t, in.target);
                break;
            case CF_BRANCH:
                if (in_image(d, in.target)) {
                    add_label(f, in.target);
                    if (in.target > maxt) maxt = in.target;
                }
                break;
            case CF_JMP:
                if (in.target != 0 && in_image(d, in.target)) {
                    add_label(f, in.target);          /* intra-function goto */
                    if (in.target > maxt) maxt = in.target;
                } else if (in.target != 0) {
                    add_ext(t, in.target);            /* tail jump out of image */
                }
                terminator = 1;
                break;
            case CF_RET:
                f->reaches_ret = 1;
                terminator = 1;
                break;
            case CF_BREAK:
            case CF_STOP:
                terminator = 1;
                break;
            default:
                break;
        }

        if (terminator && next > maxt)
            break;                         /* function complete */
        addr = next;
    }

    f->end = end;
    t->nfuncs++;
}

int analyze_discover(const uint8_t *rom, size_t rom_size, uint16_t base,
                     const uint16_t *seeds, size_t nseeds,
                     func_table_t *out) {
    static disc_t d;                      /* large (seen[]); keep off stack */
    memset(&d, 0, sizeof(d));
    d.base = base; d.rom = rom; d.rom_size = rom_size;
    memset(out, 0, sizeof(*out));

    for (size_t i = 0; i < nseeds; i++) push_func(&d, out, seeds[i]);

    while (d.nwork > 0)
        discover_one(&d, out, d.work[--d.nwork]);

    /* sort labels within each function (simple insertion sort) */
    for (int i = 0; i < out->nfuncs; i++) {
        func_t *f = &out->funcs[i];
        for (int a = 1; a < f->nlabels; a++) {
            uint16_t v = f->labels[a];
            int b = a - 1;
            while (b >= 0 && f->labels[b] > v) { f->labels[b + 1] = f->labels[b]; b--; }
            f->labels[b + 1] = v;
        }
    }
    return out->nfuncs;
}

const func_t *func_table_find(const func_table_t *t, uint16_t addr) {
    for (int i = 0; i < t->nfuncs; i++)
        if (t->funcs[i].start == addr) return &t->funcs[i];
    return NULL;
}
