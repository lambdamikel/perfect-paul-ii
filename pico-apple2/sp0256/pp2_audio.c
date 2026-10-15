/* Perfect Paul ][ SP0256 firmware: the audio path.
   SPDX-License-Identifier: GPL-3.0-or-later

   The pool setup and the PWM clock handling are adapted from this card's
   MIT-licensed DECtalk main.c, which remains available under MIT in the parent
   directory. */

#include "pp2_audio.h"
#include "pp2_sp0256.h"
#include "sp0256_resample.h"
#include "sp0256_speak.h"

#include <assert.h>
#include <stdbool.h>

#include "hardware/clocks.h"
#include "hardware/pio.h"   /* the PWM path pokes the audio SM's clock divider */
#include "pico/stdlib.h"

#if SP0256_AUDIO_I2S
#include "pico/audio_i2s.h"
#else
#include "pico/audio_pwm.h"
#endif

/* 11025 Hz and 71-sample buffers are what the DECtalk firmware runs on this
   board, proven against both backends. Kept identical rather than tuned: the
   SP0256's own rate is 10 kHz and gets resampled up, so nothing here wants a
   different output rate, and three 71-sample buffers keep the stop byte's
   latency under 20 ms. */
#define AUDIO_SAMPLE_RATE 11025u
#define AUDIO_BUFFER_SAMPLES 71u

#ifndef SP0256_PWM_GPIO
#define SP0256_PWM_GPIO 28u
#endif

#if !SP0256_AUDIO_I2S
/* pico_audio_pwm's PIO program assumes a 48 MHz PIO clock and the library
   never sets a divider, so at the SDK's default 125 MHz everything plays 2.6x
   too fast. Run the core at 96 MHz and divide the PIO clock by exactly two;
   the divider is computed from the clock actually granted, so this stays
   correct even if the 96 MHz request is refused. */
#define PWM_PIO_CLOCK_HZ 48000000u
#define PWM_SYS_CLOCK_KHZ 96000u
#define AUDIO_PIO (PICO_AUDIO_PWM_PIO ? pio1 : pio0)
#endif

void pp2_audio_set_clock(void)
{
#if !SP0256_AUDIO_I2S
    set_sys_clock_khz(PWM_SYS_CLOCK_KHZ, false);
#endif
}

struct audio_buffer_pool *pp2_audio_init(void)
{
    static audio_format_t audio_format = {
        .format = AUDIO_BUFFER_FORMAT_PCM_S16,
        .sample_freq = AUDIO_SAMPLE_RATE,
        .channel_count = 1,
    };

    static struct audio_buffer_format producer_format = {
        .format = &audio_format,
        .sample_stride = 2,
    };

    struct audio_buffer_pool *pool =
        audio_new_producer_pool(&producer_format, 3, AUDIO_BUFFER_SAMPLES);
    if (pool == NULL) {
        panic("Unable to allocate Pico audio producer pool");
    }

    const struct audio_format *output_format = NULL;
    bool ok = false;

#if SP0256_AUDIO_I2S
    const struct audio_i2s_config config = {
        .data_pin = PICO_AUDIO_I2S_DATA_PIN,
        .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE,
        .dma_channel = 0,
        .pio_sm = 0,
    };
    output_format = audio_i2s_setup(&audio_format, &config);
    if (output_format == NULL) {
        panic("Unable to set up I2S audio");
    }
    ok = audio_i2s_connect(pool);
    assert(ok);
    audio_i2s_set_enabled(true);
#else
    struct audio_pwm_channel_config config = default_mono_channel_config;
    config.core.base_pin = SP0256_PWM_GPIO;
    output_format = audio_pwm_setup(&audio_format, -1, &config);
    if (output_format == NULL) {
        panic("Unable to set up PWM audio");
    }

    /* audio_pwm_setup() ends with pio_sm_init(), which resets the divider to
       1.0, so this has to happen after setup and before the machine runs. */
    pio_sm_set_clkdiv(AUDIO_PIO, PICO_AUDIO_PWM_MONO_PIO_SM,
                      (float)clock_get_hz(clk_sys) / (float)PWM_PIO_CLOCK_HZ);

    ok = audio_pwm_default_connect(pool, false);
    assert(ok);
    audio_pwm_set_enabled(true);
#endif

    (void)output_format;
    (void)ok;
    return pool;
}

void pp2_audio_fill(struct audio_buffer_pool *pool)
{
    static sp0256_rs_t rs;
    static const uint32_t inc =
        (uint32_t)(((uint64_t)SP0256_SAMPLE_RATE << 16) / AUDIO_SAMPLE_RATE);

    struct audio_buffer *buffer = take_audio_buffer(pool, true);
    int16_t *out = (int16_t *)buffer->buffer->bytes;

    pp2_sp_service();

    /* Unconditional: the chip returns silence when halted, so there is no "not
       speaking" case to special-case -- which is also what keeps the PWM
       carrier steady instead of clicking between utterances. */
    const uint32_t n = buffer->max_sample_count;
    for (uint32_t i = 0u; i < n; ++i) {
        out[i] = sp0256_rs_next(&rs, inc, pp2_sp_sample);
    }
    buffer->sample_count = n;
    give_audio_buffer(pool, buffer);
}
