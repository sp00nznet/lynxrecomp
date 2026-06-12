/* timer.c - Mikey timer + interrupt model. See timer.h. */
#include "lynxrecomp/timer.h"
#include "lynxrecomp/mikey.h"
#include <string.h>
#include <stddef.h>

uint8_t lynx_irq_latch = 0;

/* sub-microsecond phase accumulator per timer (for clock dividers) */
static uint32_t phase[8];

static uint8_t *treg(int i, int field) { return &lynx_mikey.reg[i * 4 + field]; }

void lynx_timer_init(void) {
    lynx_irq_latch = 0;
    for (int i = 0; i < 8; i++) phase[i] = 0;
}

uint8_t lynx_irq_pending(void) { return lynx_irq_latch != 0; }
void    lynx_irq_ack(uint8_t mask)   { lynx_irq_latch &= (uint8_t)~mask; }
void    lynx_irq_raise(uint8_t mask) { lynx_irq_latch |= mask; }

/* Lynx timer link chain: a "linked" timer (clock == 7) counts on the underflow
 * of a specific predecessor, in the order 0 -> 2 -> 4 -> 1 -> 3 -> 5 -> 7.
 * link_succ[i] is the timer driven by timer i's underflow (-1 = none).
 * Timer 0 is the horizontal (HBL) timer and timer 2 the vertical (VBL); 2 being
 * linked to 0 is what makes the ~60 Hz frame interrupt fire. */
static const int link_succ[8] = { 2, 3, 4, 5, 1, 7, -1, -1 };

/* One count tick of timer i; on underflow reload/flag/interrupt and cascade
 * into the linked successor timer. */
static void tick(int i) {
    uint8_t ctlA = *treg(i, 1);
    uint8_t *cnt = treg(i, 2);
    if (*cnt == 0) {
        /* underflow */
        if (ctlA & TCTLA_RELOAD) *cnt = *treg(i, 0);
        *treg(i, 3) |= TCTLB_DONE | TCTLB_BORROWOUT;
        if (ctlA & TCTLA_INT) lynx_irq_latch |= (uint8_t)(1u << i);
        int succ = link_succ[i];
        if (succ >= 0) {
            uint8_t nA = *treg(succ, 1);
            if ((nA & TCTLA_COUNT) && (nA & TCTLA_CLOCK) == 7) tick(succ);
        }
    } else {
        (*cnt)--;
    }
}

/* ---- save-state: the timer internals not already in lynx_mikey.reg ---- */
size_t lynx_timer_state_size(void) { return sizeof(phase) + 1; }
size_t lynx_timer_state_save(uint8_t *b) {
    memcpy(b, phase, sizeof(phase));            /* 8 * 4 = 32 */
    b[sizeof(phase)] = lynx_irq_latch;
    return sizeof(phase) + 1;
}
size_t lynx_timer_state_load(const uint8_t *b) {
    memcpy(phase, b, sizeof(phase));
    lynx_irq_latch = b[sizeof(phase)];
    return sizeof(phase) + 1;
}

uint8_t lynx_timer_step(uint32_t us) {
    for (int i = 0; i < 8; i++) {
        uint8_t ctlA = *treg(i, 1);
        if (!(ctlA & TCTLA_COUNT)) continue;
        uint8_t clk = ctlA & TCTLA_CLOCK;
        if (clk == 7) continue;                 /* linked: driven by cascade */
        uint32_t period = 1u << clk;            /* microseconds per count    */
        phase[i] += us;
        while (phase[i] >= period) {
            phase[i] -= period;
            tick(i);
        }
    }
    return lynx_irq_latch;
}
