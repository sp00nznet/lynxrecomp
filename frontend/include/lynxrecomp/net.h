/*
 * net.h - glue between the runtime's ComLynx UART and the netplay transport.
 *
 * This is the one piece that ties the two halves together: it points the
 * runtime's serial transmit hook (lynx_serial_tx_hook) at mp_link_send, and
 * each frame drains mp_link_recv into lynx_serial_rx_push. The result is that a
 * recompiled game's SERDAT writes travel to the peer unit and the peer's bytes
 * arrive on this unit's UART - a ComLynx cable made of TCP.
 */
#ifndef LYNXRECOMP_NET_H
#define LYNXRECOMP_NET_H

#ifdef __cplusplus
extern "C" {
#endif

/* Install the serial<->link bridge (sets lynx_serial_tx_hook). Idempotent. */
void lynx_net_attach(void);

/* Pump the link once per frame: deliver any bytes the peer sent into the local
 * UART receive path. Safe to call when not connected (does nothing). */
void lynx_net_pump(void);

#ifdef __cplusplus
}
#endif

#endif /* LYNXRECOMP_NET_H */
