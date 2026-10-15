/* Standalone bring-up test and demo for the Perfect Paul ][ SP0256 firmware.
   SPDX-License-Identifier: GPL-3.0-or-later

   No Apple II, no 74LVC245, no slot traffic: this speaks a fixed script in a
   loop, so the chip emulation and the audio path can be confirmed before any
   of the rest of the card exists. Flash it first on a new board -- if it is
   silent, the fault is in the audio path rather than the bus interface.

   It calls the firmware's OWN audio code through pp2_audio.h rather than a
   copy of it, so a self test that makes a sound is a firmware that makes the
   same sound. The backend follows SP0256_AUDIO_I2S, so the i2s preset yields
   an I2S self test and the pwm preset a PWM one; building one backend here
   while the firmware used the other would let this pass and prove nothing.

   Everything runs on core 0. The firmware puts audio on core 1 because core 0
   is busy servicing the slot; here there is nothing to service, and one core
   means one less thing that can be wrong during bring-up.

   The script is also the demo. Items 2 and 3 are the same sentence twice --
   once through the letter-to-sound rules, once from allophones written by hand
   -- which is the comparison worth hearing, and item 8 is every allophone the
   chip has, named on the USB console as it sounds. That tour runs on the first
   pass only: it is half the cycle by itself, so afterwards the loop settles to
   roughly twenty seconds of sentences. */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "pp2_audio.h"
#include "pp2_sp0256.h"
#include "sp0256.h"
#include "sp0256_rom.h"
#include "sp0256_speak.h"

/* One audio buffer is 71 samples at 11025 Hz, so filling one is 6.44 ms of
   real time. That makes the audio path its own clock: no timer, no sleep, and
   a gap measured in buffers cannot drift away from the sound. */
/* The diagnostics that found the ALD deadlock: a marker around the rule engine
   and a heartbeat carrying the chip's own state. Off by default -- the console
   should read as a script, not a trace -- but build with
   -DSP0256_SELFTEST_VERBOSE=ON and they come back, and they are what to reach
   for if a board speaks once and then stops. */
#ifndef SP0256_SELFTEST_VERBOSE
#define SP0256_SELFTEST_VERBOSE 0
#endif

#define GAP_BUFFERS      85u    /* ~0.55 s between items   */
#define SWEEP_BUFFERS    28u    /* ~0.18 s between sounds  */

typedef enum { ITEM_TEXT, ITEM_ALLO, ITEM_SWEEP } item_kind_t;

typedef struct {
    item_kind_t kind;
    const char *label;
    const char *body;
} item_t;

static const item_t script[] = {
    { ITEM_ALLO, "banner, by hand",
      "PP ER1 FF IH KK2 TT2 PA2 PP AO LL PA2 TT2 UW2 PA3 RR1 EH DD1 IY PA5" },

    { ITEM_TEXT, "the same sentence, by rule",
      "HELLO. I AM PERFECT PAUL TWO." },
    { ITEM_ALLO, "the same sentence, by hand",
      "HH1 EH LL OW PA4 AY PA2 AE MM PA3 PA1 PP ER1 FF IH KK2 TT2 PA2 "
      "PA1 PP AO LL PA2 PA1 TT2 UW2 PA5" },

    { ITEM_TEXT, "text to speech", "I LIVE IN AN APPLE TWO E." },
    { ITEM_TEXT, "text to speech", "MY VOICE IS A SINGLE CHIP." },
    { ITEM_TEXT, "text to speech", "SHALL WE PLAY A GAME." },
    { ITEM_TEXT, "text to speech", "TELL ME MORE ABOUT YOUR PROBLEM." },

    /* Last, and only on the first pass: it is half the cycle on its own, and
       after you have heard the tour once you want the sentences back. */
    { ITEM_SWEEP, "all 64 allophones (first pass only)", NULL },
};

#define N_ITEMS ((int)(sizeof script / sizeof script[0]))

int main(void)
{
    pp2_audio_set_clock();   /* PWM only, and before stdio_init_all() */

    stdio_init_all();

#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 1);
#endif

    printf("\nPerfect Paul ][ -- SP0256-AL2 self test\n");
    printf("build %s %s\n", __DATE__, __TIME__);
    printf("System clock: %lu Hz, audio %s\n",
           (unsigned long)clock_get_hz(clk_sys),
           SP0256_AUDIO_I2S ? "I2S" : "PWM");
    printf("No Apple II needed. Speaks in a loop.\n\n");

    struct audio_buffer_pool *pool = pp2_audio_init();
    pp2_sp_init();

    uint32_t beat  = 0u;      /* heartbeat, so a stuck loop is distinguishable
                                 from a stuck audio path */
    int      item  = -1;      /* -1 so the first idle check starts item 0 */
    int      sweep = 0;
    uint32_t wait  = 0u;
    int      pass  = 0;

    for (;;) {
        /* Advance only when the chip has actually finished, which is the one
           thing a fixed delay cannot know. */
        const int idle = (pp2_sp_staged() == 0u) && sp0256_speak_idle();

        if (idle) {
            if (wait > 0u) {
                --wait;
            } else if (item >= 0 && script[item].kind == ITEM_SWEEP &&
                       sweep < SP0256_ALLOPHONE_COUNT) {
                printf("    %2d  %s\n", sweep, sp0256_al2_label(sweep));
                pp2_sp_push((uint8_t)sweep);
                pp2_sp_push(3u);          /* PA4, so each one stands alone */
                ++sweep;
                wait = SWEEP_BUFFERS;
            } else {
                if (++item >= N_ITEMS) {
                    item = 0;
                    printf("\n--- pass %d complete ---\n\n", ++pass);
                }
                /* Skip the sweep after the first pass, so the loop settles to
                   about twenty seconds of sentences instead of fifty. */
                if (script[item].kind == ITEM_SWEEP && pass > 0) {
                    item = 0;
                    printf("\n--- pass %d complete ---\n\n", ++pass);
                }
                const item_t *it = &script[item];
                printf("%d. %s\n", item + 1, it->label);

                switch (it->kind) {
                case ITEM_TEXT:
                    printf("    \"%s\"\n", it->body);
                    pp2_sp_set_mode(PP2_SP_TEXT);
#if SP0256_SELFTEST_VERBOSE
                    printf("    [rules...]\n");
#endif
                    {
                        const int q = pp2_sp_say_english(it->body);
                        (void)q;
#if SP0256_SELFTEST_VERBOSE
                        printf("    [queued %d]\n", q);
#endif
                    }
                    break;
                case ITEM_ALLO:
                    printf("    %s\n", it->body);
                    pp2_sp_set_mode(PP2_SP_ALLOPHONE);
                    pp2_sp_say(it->body);
                    break;
                case ITEM_SWEEP:
                    sweep = 0;
                    break;
                }
                wait = GAP_BUFFERS;
            }
        }

        pp2_audio_fill(pool);

#if SP0256_SELFTEST_VERBOSE
        /* Every ~1.3 s. If these stop, the loop is blocked in the audio path;
           if they continue while nothing advances, the state says why. lrq=0000
           with halted=1 is the ALD deadlock this found -- see
           vendor/README-VENDOR.md. */
        if (++beat >= 200u) {
            beat = 0u;
            printf("hb item=%d wait=%" PRIu32 " stage=%" PRIu32 " chipq=%d "
                   "lrq=%04x halted=%d idle=%d\n",
                   item, wait, pp2_sp_stage_depth(), sp0256_speak_queued(),
                   (unsigned)sp0256_getStatus(), sp0256_halted(),
                   sp0256_speak_idle());
        }
#else
        (void)beat;
#endif
    }
}
