/* Regression test: speak one utterance, wait for idle, then speak a SECOND.
   SPDX-License-Identifier: GPL-3.0-or-later

   This is the test that was missing. render_wav and pp2_render both push every
   allophone up front and exit as soon as the chip goes idle, so neither ever
   spoke twice -- and the vendored core deadlocked on the second utterance,
   with a command pending in the ALD that could never be picked up. One
   sentence worked; everything after it was silence.

   Found on real hardware, reproduced here in a few lines. Keep it. */
#include <stdio.h>
#include <stdint.h>
#include "pp2_sp0256.h"
#include "sp0256_speak.h"
#include "sp0256_resample.h"
#include "sp0256.h"

#define OUT_RATE 11025u
#define BUF 71u

static int drain(const char *what) {
    sp0256_rs_t rs = {0};
    const uint32_t inc = (uint32_t)(((uint64_t)SP0256_SAMPLE_RATE << 16) / OUT_RATE);
    for (uint32_t frame = 0; frame < 1500; frame++) {
        pp2_sp_service();
        for (uint32_t i = 0; i < BUF; i++) (void)sp0256_rs_next(&rs, inc, pp2_sp_sample);
        if (pp2_sp_staged() == 0 && sp0256_speak_idle()) {
            printf("  %s: idle after %u frames\n", what, frame); return 0;
        }
    }
    printf("  %s: STUCK  stage=%u chipq=%d lrq=%04x halted=%d\n", what,
           pp2_sp_stage_depth(), sp0256_speak_queued(),
           (unsigned)sp0256_getStatus(), sp0256_halted());
    return 1;
}

int main(void) {
    pp2_sp_init();
    pp2_sp_set_mode(PP2_SP_ALLOPHONE);
    printf("utterance 1: %d allophones\n", pp2_sp_say("PP ER1 FF IH KK2 TT2 PA5"));
    int bad = drain("utterance 1");
    printf("utterance 2: %d allophones\n", pp2_sp_say("HH1 EH LL OW PA5"));
    bad |= drain("utterance 2");
    return bad;
}
