# Multiplayer — ComLynx, not split-screen

Lynx multiplayer is fundamentally different from a console's. There was **no
second controller**: a Lynx is a handheld with one D-pad and one set of buttons.
Multiplayer meant **ComLynx** — a 3-wire serial cable daisy-chaining up to ~8
*separate* handhelds, each running its own cartridge and reading only its own
controls. The games implement their own protocol over the link in software.

So `lynxrecomp`'s netplay is **not** the "two pads into one console" model some
recompilers use. It is a **ComLynx bridge**: each peer runs its own recompiled
game with its own local input, and the bytes one unit transmits over the serial
port are relayed to the other unit's serial port. A ComLynx cable made of TCP.

## The pieces

```
  game writes SERDAT ($FD8D)              peer's bytes arrive
        │                                        ▲
        ▼                                        │
  Mikey UART (src/serial.c)            Mikey UART receive FIFO
   lynx_serial_tx_hook ─────┐          ▲ lynx_serial_rx_push
                            │          │
                            ▼          │
              frontend/src/net.c  (lynx_net_attach / lynx_net_pump)
                            │          ▲
                            ▼          │
              frontend/src/mp_session.c  (TCP, async connect)
                       mp_link_send / mp_link_recv
```

### Mikey UART (`src/serial.c`, runtime)

Models the Lynx serial port at `SERCTL` ($FD8C) and `SERDAT` ($FD8D):

- **SERDAT** write transmits a byte (handed to `lynx_serial_tx_hook`); read pops
  the next received byte from a FIFO.
- **SERCTL** write sets the control bits (TX/RX interrupt enable, parity, error
  reset); read returns status (TXRDY/TXEMPTY always ready since transmit is
  abstracted, RXRDY when a byte is waiting, and the sticky error bits).
- The serial interrupt is delivered on **Timer 4's interrupt latch bit** — the
  real hardware shares it, which is exactly why Timer 4 is reserved as the
  link's baud-rate generator.

The UART is part of the save state (state version 2) and is unit-tested
(`tests/test_serial.c`): register/IRQ behaviour, a transmit→receive loopback, a
FIFO overrun, and a state round-trip.

### Netplay transport (`frontend/src/mp_session.c`)

TCP, since lockstep wants reliable + ordered delivery. Connect/accept run on a
background SDL thread so the UI never blocks; once connected, `mp_link_send` /
`mp_link_recv` move raw serial bytes with no per-frame framing — matching
ComLynx's asynchronous nature (the link was never frame-locked). `mp_send_blob`
/ `mp_recv_blob` carry the initial save-state the host sends on connect so both
units start the link from the same machine state.

### The bridge (`frontend/src/net.c`)

`lynx_net_attach()` points `lynx_serial_tx_hook` at `mp_link_send`;
`lynx_net_pump()` (once per frame) drains `mp_link_recv` into
`lynx_serial_rx_push`. With no peer connected both are inert — exactly like an
unplugged cable, so single-player is unaffected.

## Using it

From the menu: **Multiplayer → Connect**, then **Host (Player 1)** or enter an
IP and **Join (Player 2)**. The host is unit 1, the client unit 2. Each plays on
their own machine with their own controls; the game's own ComLynx code does the
rest.

> Exercising this end-to-end needs a Lynx game that actually uses ComLynx (e.g.
> *Warbirds*, *Checkered Flag*, *Slime World*). The byte path itself is proven by
> the loopback unit test; a real link-game bring-up is the validation step.
