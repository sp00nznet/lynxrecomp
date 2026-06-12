/*
 * mp_session - ComLynx link transport for lynxrecomp netplay.
 *
 * The Atari Lynx had no second controller: multiplayer meant several SEPARATE
 * handhelds, each with its own cart and its own controls, daisy-chained over
 * ComLynx - a 3-wire serial link (up to ~8 units) clocked by Mikey's timer 4.
 * Games implement their own protocol over Mikey's UART (SERCTL/SERDAT,
 * $FD8C/$FD8D); a unit only ever reads its OWN joystick.
 *
 * So netplay here is NOT a "two pads into one console" sync (that's the SNES
 * model). It is a ComLynx *bridge*: each peer runs its own recompiled game with
 * its own local input, and this module relays the serial bytes between them -
 * what one unit's Mikey transmits, the other unit's Mikey receives. Both peers
 * start from a save-state the host sends on connect so the link comes up in a
 * known shared state.
 *
 * Transport: TCP (reliable + ordered). Connect/accept run on a background
 * thread so the UI never blocks; once connected the byte relay is non-blocking
 * and pumped each frame from the main loop, matching ComLynx's asynchronous
 * nature (the link was never frame-locked).
 */
#ifndef LYNXRECOMP_MP_SESSION_H
#define LYNXRECOMP_MP_SESSION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MP_IDLE,          /* no session */
    MP_HOSTING,       /* host: listening for a peer */
    MP_CONNECTING,    /* client: connecting */
    MP_CONNECTED,     /* connected (ready to sync + link) */
    MP_DISCONNECTED   /* peer dropped / error */
} MpState;

void mp_init(void);       /* one-time (Winsock startup). Safe to call repeatedly. */
void mp_shutdown(void);

/* Begin hosting on `port` / joining `ip`:`port`. Returns false on immediate
 * setup failure; otherwise the connection completes asynchronously (poll
 * mp_get_state() for MP_CONNECTED). The host is unit 1, the client unit 2. */
bool mp_host(int port);
bool mp_join(const char *ip, int port);
void mp_disconnect(void);

MpState     mp_get_state(void);
bool        mp_is_host(void);
const char *mp_status_text(void);   /* short human-readable status for the menu */

/* Initial state sync over the connected socket (host sends, client receives).
 * Call once after MP_CONNECTED, before relaying any serial bytes, so both units
 * begin the link from the same machine state. */
bool mp_send_blob(const uint8_t *data, int size);
bool mp_recv_blob(uint8_t **data, int *size);   /* allocates *data; caller frees */

/* ---- ComLynx byte relay (the link itself) ----
 * mp_link_send: ship `n` bytes this unit's Mikey transmitted to the peer.
 * mp_link_recv: pull up to `cap` bytes the peer transmitted into `buf`; the
 *               caller feeds them into the local Mikey serial-receive path.
 * Both are non-blocking; recv returns the count read (0 when nothing pending),
 * or -1 on disconnect (state becomes MP_DISCONNECTED). */
bool mp_link_send(const uint8_t *bytes, int n);
int  mp_link_recv(uint8_t *buf, int cap);

#ifdef __cplusplus
}
#endif

#endif /* LYNXRECOMP_MP_SESSION_H */
