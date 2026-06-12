/* test_serial.c - Mikey UART / ComLynx serial port (no game data, no sockets).
 *
 * Exercises the register interface and IRQ behaviour, and a loopback "link":
 * the tx hook of unit A feeds the rx of unit B, proving the byte path that the
 * frontend's ComLynx netplay relies on. */
#include <stdio.h>
#include <string.h>
#include "lynxrecomp/serial.h"
#include "lynxrecomp/mikey.h"
#include "lynxrecomp/timer.h"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("  FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

/* Loopback target: whatever unit A transmits is delivered to the local RX. */
static void loopback_tx(uint8_t b) { lynx_serial_rx_push(b); }

int main(void) {
    lynx_timer_init();
    lynx_serial_init();

    /* --- status: transmitter always ready, nothing received yet --- */
    uint8_t st = lynx_serial_read(MIKEY_SERCTL);
    CHECK(st & SERCTL_TXRDY, "TXRDY set at idle");
    CHECK(!(st & SERCTL_RXRDY), "RXRDY clear at idle");

    /* --- receive without RX interrupt armed: byte queues, no IRQ --- */
    lynx_irq_ack(0xFF);
    lynx_serial_rx_push(0x5A);
    CHECK(lynx_serial_read(MIKEY_SERCTL) & SERCTL_RXRDY, "RXRDY set after push");
    CHECK(lynx_irq_pending() == 0, "no IRQ when RXINTEN clear");
    CHECK(lynx_serial_read(MIKEY_SERDAT) == 0x5A, "received byte read back");
    CHECK(!(lynx_serial_read(MIKEY_SERCTL) & SERCTL_RXRDY), "RXRDY clears after read");

    /* --- receive with RX interrupt armed raises the serial (timer-4) IRQ --- */
    lynx_irq_ack(0xFF);
    lynx_serial_write(MIKEY_SERCTL, SERCTL_RXINTEN);
    lynx_serial_rx_push(0xC3);
    CHECK(lynx_irq_pending() != 0, "RXINTEN -> IRQ raised");
    CHECK(lynx_irq_latch & (1u << 4), "serial IRQ is on timer-4 bit");
    CHECK(lynx_serial_read(MIKEY_SERDAT) == 0xC3, "armed byte read back");

    /* --- transmit with TX interrupt armed raises the IRQ; hook sees the byte - */
    lynx_irq_ack(0xFF);
    lynx_serial_tx_hook = loopback_tx;
    lynx_serial_write(MIKEY_SERCTL, SERCTL_TXINTEN | SERCTL_RXINTEN);
    lynx_serial_write(MIKEY_SERDAT, 0x99);          /* transmit -> loopback -> rx */
    CHECK(lynx_irq_pending() != 0, "TXINTEN -> IRQ raised");
    CHECK(lynx_serial_read(MIKEY_SERCTL) & SERCTL_RXRDY, "loopback delivered the byte");
    CHECK(lynx_serial_read(MIKEY_SERDAT) == 0x99, "loopback byte matches");

    /* --- overrun: fill the FIFO past capacity sets the OVERRUN error --- */
    lynx_serial_init();
    lynx_serial_write(MIKEY_SERCTL, 0);             /* no interrupts */
    for (int i = 0; i < 300; i++) lynx_serial_rx_push((uint8_t)i);   /* > 256 FIFO */
    CHECK(lynx_serial_read(MIKEY_SERCTL) & SERCTL_OVERRUN, "overrun flagged when FIFO full");
    lynx_serial_write(MIKEY_SERCTL, SERCTL_RESETERR);
    CHECK(!(lynx_serial_read(MIKEY_SERCTL) & SERCTL_OVERRUN), "RESETERR clears overrun");

    /* --- state round-trip --- */
    lynx_serial_init();
    lynx_serial_write(MIKEY_SERCTL, SERCTL_RXINTEN);
    lynx_serial_rx_push(0x11); lynx_serial_rx_push(0x22);
    static uint8_t blob[512];
    size_t n = lynx_serial_state_save(blob);
    CHECK(n == lynx_serial_state_size(), "state size matches");
    lynx_serial_init();                              /* clobber */
    lynx_serial_state_load(blob);
    CHECK(lynx_serial_read(MIKEY_SERDAT) == 0x11, "state restored byte 1");
    CHECK(lynx_serial_read(MIKEY_SERDAT) == 0x22, "state restored byte 2");

    printf(fails ? "FAIL: %d\n" : "PASS: Mikey UART / ComLynx serial\n", fails);
    return fails ? 1 : 0;
}
