/* Perfect Paul ][ SP0256 firmware: the audio path, shared by the firmware and
   the self test. SPDX-License-Identifier: GPL-3.0-or-later

   This exists so the self test cannot vouch for an audio path the firmware
   does not use. The DECtalk side keeps the two in step through a CMake
   function, which guarantees the same backend but not the same code; here they
   are the same code -- same pool, same buffer size, same resampler state
   machine, same clock arithmetic. If the self test makes a sound, the firmware
   will make the same sound. */
#ifndef PP2_AUDIO_H
#define PP2_AUDIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PWM only, and it must happen before stdio_init_all() so the UART divisors
   follow the final clock. A no-op on the I2S path, which derives its own
   dividers from clk_sys and runs at the SDK default. */
void pp2_audio_set_clock(void);

/* Bring up the backend chosen at build time. Panics rather than returning on
   failure: there is nothing useful a speech card can do without audio. */
struct audio_buffer_pool *pp2_audio_init(void);

/* One buffer's worth, pulled from the SP0256 through the resampler. Blocks
   until the DMA wants it, which is what paces the whole firmware. Applies any
   pending stop first, so this is also the consumer side of the stop byte. */
void pp2_audio_fill(struct audio_buffer_pool *pool);

#ifdef __cplusplus
}
#endif
#endif
