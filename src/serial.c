/* serial.c - Mikey UART / ComLynx serial port. See serial.h.
 *
 * The model is deliberately simple because transmission is instantaneous from
 * the recompiled game's point of view: a byte written to SERDAT is handed
 * straight to the host link hook, so the transmitter is always ready (TXRDY +
 * TXEMPTY stay set). Reception is a small FIFO fed by lynx_serial_rx_push();
 * RXRDY reflects "FIFO non-empty", and reading SERDAT pops the next byte. The
 * UART interrupt shares Timer 4's latch bit (the real hardware does too).
 */
#include "lynxrecomp/serial.h"
#include "lynxrecomp/timer.h"
#include <string.h>

#define SERIAL_IRQ_BIT  (1u << 4)   /* ComLynx UART shares Timer 4's interrupt */
#define RXFIFO_SIZE     256         /* power of two; TCP delivers bytes in bursts */

void (*lynx_serial_tx_hook)(uint8_t byte) = 0;

static struct {
    uint8_t ctrl;            /* last SERCTL control bits written (TXINTEN/RXINTEN/..) */
    uint8_t status_err;      /* sticky error bits (OVERRUN/PARERR/..) until RESETERR  */
    uint8_t rx_fifo[RXFIFO_SIZE];
    uint16_t rx_head, rx_tail;   /* head == tail => empty */
} s;

static int rx_empty(void) { return s.rx_head == s.rx_tail; }
static int rx_count(void) { return (uint16_t)(s.rx_tail - s.rx_head) & (RXFIFO_SIZE - 1); }

void lynx_serial_init(void) {
    memset(&s, 0, sizeof(s));
    lynx_serial_tx_hook = 0;
}

/* Re-evaluate the receive interrupt: if a byte is waiting and RX interrupts are
 * armed, raise the shared serial/Timer-4 latch bit. */
static void rx_service(void) {
    if (!rx_empty() && (s.ctrl & SERCTL_RXINTEN))
        lynx_irq_raise(SERIAL_IRQ_BIT);
}

void lynx_serial_rx_push(uint8_t byte) {
    uint16_t next = (uint16_t)(s.rx_tail + 1) & (RXFIFO_SIZE - 1);
    if (next == s.rx_head) {            /* FIFO full: the unread byte is lost */
        s.status_err |= SERCTL_OVERRUN;
        return;
    }
    s.rx_fifo[s.rx_tail] = byte;
    s.rx_tail = next;
    rx_service();
}

uint8_t lynx_serial_read(uint8_t off) {
    if (off == 0x8D) {                  /* SERDAT: pop the next received byte */
        if (rx_empty()) return 0x00;
        uint8_t b = s.rx_fifo[s.rx_head];
        s.rx_head = (uint16_t)(s.rx_head + 1) & (RXFIFO_SIZE - 1);
        /* Another byte still queued -> keep the receiver flagged + interrupting. */
        rx_service();
        return b;
    }
    /* SERCTL status read: transmitter is always ready (instant), plus RXRDY and
     * any sticky error bits. */
    uint8_t st = SERCTL_TXRDY | SERCTL_TXEMPTY;
    if (!rx_empty()) st |= SERCTL_RXRDY;
    st |= (s.status_err & (SERCTL_OVERRUN | SERCTL_PARERR | SERCTL_FRAMERR | SERCTL_RXBRK));
    return st;
}

void lynx_serial_write(uint8_t off, uint8_t val) {
    if (off == 0x8D) {                  /* SERDAT: transmit a byte */
        if (lynx_serial_tx_hook) lynx_serial_tx_hook(val);
        /* Transmit completes immediately; if TX interrupts are armed, the
         * transmit-ready condition raises the shared serial/Timer-4 interrupt. */
        if (s.ctrl & SERCTL_TXINTEN) lynx_irq_raise(SERIAL_IRQ_BIT);
        return;
    }
    /* SERCTL control write. */
    s.ctrl = val;
    if (val & SERCTL_RESETERR) s.status_err = 0;
    /* Arming RX interrupts while a byte is already waiting fires immediately. */
    rx_service();
}

/* ---- save-state ---- */
size_t lynx_serial_state_size(void) {
    /* ctrl(1) + status_err(1) + rx_head(2) + rx_tail(2) + FIFO. */
    return 1 + 1 + 2 + 2 + RXFIFO_SIZE;
}

size_t lynx_serial_state_save(uint8_t *buf) {
    uint8_t *p = buf;
    *p++ = s.ctrl;
    *p++ = s.status_err;
    *p++ = (uint8_t)(s.rx_head & 0xFF); *p++ = (uint8_t)(s.rx_head >> 8);
    *p++ = (uint8_t)(s.rx_tail & 0xFF); *p++ = (uint8_t)(s.rx_tail >> 8);
    memcpy(p, s.rx_fifo, RXFIFO_SIZE); p += RXFIFO_SIZE;
    return (size_t)(p - buf);
}

size_t lynx_serial_state_load(const uint8_t *buf) {
    const uint8_t *p = buf;
    s.ctrl = *p++;
    s.status_err = *p++;
    s.rx_head = (uint16_t)(p[0] | (p[1] << 8)); p += 2;
    s.rx_tail = (uint16_t)(p[0] | (p[1] << 8)); p += 2;
    memcpy(s.rx_fifo, p, RXFIFO_SIZE); p += RXFIFO_SIZE;
    return (size_t)(p - buf);
}
