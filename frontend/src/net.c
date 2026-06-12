/* net.c - ComLynx UART <-> netplay transport bridge. See net.h. */
#include "lynxrecomp/net.h"
#include "lynxrecomp/serial.h"
#include "lynxrecomp/mp_session.h"

/* Transmit hook: a byte the game wrote to SERDAT goes straight onto the link.
 * mp_link_send is a no-op (returns false) when no peer is connected, so an
 * offline unit just drops its link traffic - exactly like an unplugged cable. */
static void tx_to_link(uint8_t byte) {
    mp_link_send(&byte, 1);
}

void lynx_net_attach(void) {
    lynx_serial_tx_hook = tx_to_link;
}

void lynx_net_pump(void) {
    if (mp_get_state() != MP_CONNECTED) return;
    uint8_t buf[256];
    for (;;) {
        int n = mp_link_recv(buf, sizeof(buf));
        if (n <= 0) break;                       /* 0 = nothing pending, -1 = dropped */
        for (int i = 0; i < n; i++) lynx_serial_rx_push(buf[i]);
        if (n < (int)sizeof(buf)) break;          /* drained for this frame */
    }
}
