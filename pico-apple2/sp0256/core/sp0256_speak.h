/* Allophone queue and sample pump around the vendor SP0256 core.
   Shared by the host WAV renderer and the Pico firmware.
   LambdaSpeak 4, GPL-3.

   The core runs at the chip's native rate: with the usual 3.12 MHz crystal
   the SP0256 emits 3120000/2/156 = 10000 samples per second. */
#ifndef SP0256_SPEAK_H
#define SP0256_SPEAK_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define SP0256_SAMPLE_RATE 10000
#define SP0256_QUEUE_LEN   256

void    sp0256_speak_init(void);
int     sp0256_speak_push(uint8_t allophone); /* 1 = queued, 0 = queue full   */
int     sp0256_speak_queued(void);            /* allophones still waiting     */
int     sp0256_speak_idle(void);              /* queue empty and chip halted  */
int16_t sp0256_speak_next_sample(void);       /* one sample; feeds ALD itself */

/* The chip's two status lines, which CPC software polls through &FBEE:
     LRQ  -- Load ReQuest: 1 when the ALD register can take another allophone.
     SBY  -- StandBy:      1 when the chip is idle (not speaking).
   The SSA-1 reports SBY in bit 7 and LRQ in bit 6; the DK'tronics reports
   them the other way round. Stage 2 maps these onto whichever it is. */
int sp0256_speak_lrq(void);
int sp0256_speak_sby(void);

#ifdef __cplusplus
}
#endif
#endif
