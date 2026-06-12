/* serial.h - Mikey UART / ComLynx serial port ($FD8C SERCTL, $FD8D SERDAT).
 *
 * The Lynx's multiplayer link was ComLynx: a 3-wire serial bus joining up to
 * ~8 separate handhelds. Each unit's game drives Mikey's UART directly - it
 * writes a byte to SERDAT to transmit, reads SERDAT on receive, and arms
 * TX/RX-ready interrupts via SERCTL. The byte clock is Timer 4 (which is why
 * Timer 4 is reserved for the link), and the UART's interrupt is delivered on
 * Timer 4's interrupt line (latch bit 4).
 *
 * This module models that register interface. Transmission is abstracted behind
 * a host hook: a host with a real/emulated link (the SDL frontend's ComLynx
 * netplay) sets lynx_serial_tx_hook to forward transmitted bytes, and calls
 * lynx_serial_rx_push() to deliver bytes that arrived from a peer. With no hook
 * set, TX bytes are simply dropped and nothing is received (a lone unit).
 */
#ifndef LYNXRECOMP_SERIAL_H
#define LYNXRECOMP_SERIAL_H

#include <stdint.h>
#include <stddef.h>

/* SERCTL write bits (control). */
#define SERCTL_TXINTEN  0x80   /* transmit-ready raises the serial interrupt   */
#define SERCTL_RXINTEN  0x40   /* receive-ready raises the serial interrupt    */
#define SERCTL_PAREN    0x10   /* parity enable                                */
#define SERCTL_RESETERR 0x08   /* write 1 to clear the error flags             */
#define SERCTL_TXOPEN   0x04   /* open-collector TX (bus arbitration)          */
#define SERCTL_TXBRK    0x02   /* transmit a break                             */
#define SERCTL_PAREVEN  0x01   /* even parity                                  */

/* SERCTL read bits (status). */
#define SERCTL_TXRDY    0x80   /* ready to accept a byte to transmit           */
#define SERCTL_RXRDY    0x40   /* a received byte is waiting in SERDAT         */
#define SERCTL_TXEMPTY  0x20   /* transmitter idle (shift register empty)      */
#define SERCTL_PARERR   0x10   /* parity error on the received byte            */
#define SERCTL_OVERRUN  0x08   /* a received byte was overwritten unread       */
#define SERCTL_FRAMERR  0x04   /* framing error                                */
#define SERCTL_RXBRK    0x02   /* a break was received                         */
#define SERCTL_PARBIT   0x01   /* 9th (parity) bit of the received byte        */

void lynx_serial_init(void);

/* Register-level access, dispatched from lynx_mikey_read/write for $FD8C/$FD8D. */
uint8_t lynx_serial_read(uint8_t off);          /* off = SERCTL(0x8C) or SERDAT(0x8D) */
void    lynx_serial_write(uint8_t off, uint8_t val);

/* Host transmit hook: called with each byte the game writes to SERDAT. Set by
 * the frontend to forward bytes over the ComLynx link; NULL = no link. */
extern void (*lynx_serial_tx_hook)(uint8_t byte);

/* Deliver a byte received from the link into the UART's receive path: queues it,
 * sets RXRDY, and (if RXINTEN is armed) raises the serial interrupt. */
void lynx_serial_rx_push(uint8_t byte);

/* save-state: serialize the UART control/status + receive FIFO. */
size_t lynx_serial_state_size(void);
size_t lynx_serial_state_save(uint8_t *buf);
size_t lynx_serial_state_load(const uint8_t *buf);

#endif /* LYNXRECOMP_SERIAL_H */
