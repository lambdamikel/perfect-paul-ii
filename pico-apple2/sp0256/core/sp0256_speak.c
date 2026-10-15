/* LambdaSpeak 4, GPL-3. */
#include "sp0256_speak.h"
#include "sp0256_rom.h"
#include "sp0256.h"

#define LRQ_READY 0x8000u   /* ivoice_rd(0) returns lrq; set = ready for ALD */

static uint8_t  queue[SP0256_QUEUE_LEN];
static volatile uint32_t head, tail;

void sp0256_speak_init(void)
{
    head = tail = 0;
    ivoice_init(sp0256_al2_rom());
}

int sp0256_speak_push(uint8_t allophone)
{
    uint32_t next = (head + 1u) % SP0256_QUEUE_LEN;
    if (next == tail) return 0;
    queue[head] = allophone;
    head = next;
    return 1;
}

int sp0256_speak_queued(void)
{
    return (int)((head - tail + SP0256_QUEUE_LEN) % SP0256_QUEUE_LEN);
}

int sp0256_speak_idle(void)
{
    return sp0256_speak_queued() == 0 && sp0256_halted();
}

int16_t sp0256_speak_next_sample(void)
{
    /* Load the next allophone exactly when the chip raises LRQ, which is what
       the real SSA-1 and DK'tronics driver code does. */
    if (head != tail && (sp0256_getStatus() & LRQ_READY)) {
        sp0256_sendCommand(queue[tail]);
        tail = (tail + 1u) % SP0256_QUEUE_LEN;
    }
    return (int16_t)sp0256_getNextSample();
}

int sp0256_speak_lrq(void)
{
    /* Report "busy" while our own queue still holds allophones, so the CPC
       sees one continuous utterance rather than the gaps between our loads. */
    if (head != tail) return 0;
    return (sp0256_getStatus() & LRQ_READY) ? 1 : 0;
}

int sp0256_speak_sby(void)
{
    return sp0256_speak_idle() ? 1 : 0;
}
