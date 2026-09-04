// SPDX-License-Identifier: MIT
// Standalone bring-up test for the DECtalkMini Pico port.
//
// No Apple II and no serial input required: this speaks a fixed set of phrases
// in a loop, so you can confirm that the synthesiser and the audio path work at
// all before the rest of the hardware exists.
//
// The audio backend follows DECTALK_AUDIO_I2S exactly as the Apple II firmware
// does, so an I2S build of this test exercises the same library, PIO instance,
// pins and system clock the firmware will use. Hardcoding one backend here
// would let the self test pass while the firmware's audio path stays untested.
//
// Everything runs on core 0. The Apple II firmware puts TTS on core 1 because
// core 0 is busy servicing the slot; here there is nothing to service, and one
// core means one less thing that can be wrong during bring-up.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "epsonapi.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

#if DECTALK_AUDIO_I2S
#include "pico/audio_i2s.h"
#else
#include "pico/audio_pwm.h"
#endif

#define AUDIO_SAMPLE_RATE 11025u
#define AUDIO_BUFFER_SAMPLES 71u
#define MAX_LINE_LEN 256u

#if !DECTALK_AUDIO_I2S
// pico_audio_pwm's PIO program is written for a 48 MHz PIO clock ("136
// clocks/cycle frequency 352941 / 16 = 22058" in audio_pwm.pio) and the library
// never calls sm_config_set_clkdiv. Every pico-playground app that uses it
// therefore calls set_sys_clock_48mhz(). At the SDK's default 125 MHz the audio
// plays 125/48 = 2.6x too fast.
//
// Dropping the whole chip to 48 MHz would leave DECtalk very little to work
// with, so instead run the core at 96 MHz and divide the PIO clock by exactly
// two. The divider is computed from the clock we actually got, so this stays
// correct even if the 96 MHz request is refused.
//
// The I2S path derives its own dividers from clk_sys and needs none of this, so
// it runs at the SDK default clock, exactly as the Apple II firmware does.
#define PWM_PIO_CLOCK_HZ 48000000u
#define SYS_CLOCK_KHZ 96000u

#define AUDIO_PIO (PICO_AUDIO_PWM_PIO ? pio1 : pio0)

#ifndef DECTALK_PWM_GPIO
#define DECTALK_PWM_GPIO 28u
#endif
#endif

// Seconds to wait before the first utterance, so a USB terminal has time to
// attach and catch the banner.
#define STARTUP_DELAY_MS 3000u

// Pause between phrases and between passes.
#define PHRASE_GAP_MS 700u
#define PASS_GAP_MS 3000u

static struct audio_buffer_pool *audio_pool;

static const char *const phrases[] = {
    "Deck talk mini is alive.",
    "The quick brown fox jumps over the lazy dog.",
    "Apple two speech synthesizer, self test.",
    "Zero one two three four five six seven eight nine.",
};

#define PHRASE_COUNT (sizeof(phrases) / sizeof(phrases[0]))

static struct audio_buffer_pool *init_audio(void) {
    static audio_format_t audio_format = {
        .format = AUDIO_BUFFER_FORMAT_PCM_S16,
        .sample_freq = AUDIO_SAMPLE_RATE,
        .channel_count = 1,
    };

    static struct audio_buffer_format producer_format = {
        .format = &audio_format,
        .sample_stride = 2,
    };

    struct audio_buffer_pool *producer_pool =
        audio_new_producer_pool(&producer_format, 3, AUDIO_BUFFER_SAMPLES);
    if (producer_pool == NULL) {
        panic("Unable to allocate Pico audio producer pool");
    }

#if DECTALK_AUDIO_I2S
    // The same PIO instance, DMA channel and pins the Apple II firmware uses,
    // so a pass here is evidence about that firmware and not just about this
    // test.
    const struct audio_i2s_config config = {
        .data_pin = PICO_AUDIO_I2S_DATA_PIN,
        .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE,
        .dma_channel = 0,
        .pio_sm = 0,
    };

    if (audio_i2s_setup(&audio_format, &config) == NULL) {
        panic("Unable to set up I2S audio");
    }
    if (!audio_i2s_connect(producer_pool)) {
        panic("Unable to connect I2S audio");
    }
    audio_i2s_set_enabled(true);
#else
    // Mono PWM drives exactly one pin: config.core.base_pin.
    struct audio_pwm_channel_config config = default_mono_channel_config;
    config.core.base_pin = DECTALK_PWM_GPIO;

    const struct audio_format *output_format =
        audio_pwm_setup(&audio_format, -1, &config);
    if (output_format == NULL) {
        panic("Unable to set up PWM audio");
    }

    // audio_pwm_setup() ends with pio_sm_init(), which resets the divider to
    // 1.0, so this has to happen after setup and before the state machine runs.
    const float clkdiv =
        (float)clock_get_hz(clk_sys) / (float)PWM_PIO_CLOCK_HZ;
    pio_sm_set_clkdiv(AUDIO_PIO, PICO_AUDIO_PWM_MONO_PIO_SM, clkdiv);
    printf("PIO clkdiv: %.4f (targeting %u Hz PIO clock)\n",
           (double)clkdiv, (unsigned)PWM_PIO_CLOCK_HZ);

    if (!audio_pwm_default_connect(producer_pool, false)) {
        panic("Unable to connect PWM audio");
    }
    audio_pwm_set_enabled(true);
#endif

    return producer_pool;
}

// DECtalk hands us one buffer of samples at a time.
static short *write_wav(short *samples, long length, int phoneme) {
    (void)phoneme;

    struct audio_buffer *buffer;
    while ((buffer = take_audio_buffer(audio_pool, false)) == NULL) {
        tight_loop_contents();
    }

    if (length > (long)AUDIO_BUFFER_SAMPLES) {
        length = AUDIO_BUFFER_SAMPLES;
    }

    memcpy(buffer->buffer->bytes, samples, (size_t)length * sizeof(short));
    buffer->sample_count = (uint32_t)length;
    give_audio_buffer(audio_pool, buffer);
    return samples;
}

static void set_led(bool on) {
#ifdef PICO_DEFAULT_LED_PIN
    gpio_put(PICO_DEFAULT_LED_PIN, on ? 1 : 0);
#else
    (void)on;
#endif
}

static void init_led(void) {
#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif
    set_led(false);
}

// The LED is on for the duration of each utterance, so the board still shows
// that the loop is running even if nothing is audible.
static void say(const char *text) {
    char line[MAX_LINE_LEN];

    // A trailing vertical tab tells DECtalk to synthesise what it has buffered.
    snprintf(line, sizeof(line), "%s\x0b", text);

    printf("  speaking: %s\n", text);
    set_led(true);
    TextToSpeechStart(line, NULL, WAVE_FORMAT_1M16);
    TextToSpeechSync();
    set_led(false);
}

int main(void) {
#if !DECTALK_AUDIO_I2S
    // Must happen before stdio_init_all() so the UART divisors are derived from
    // the final clock. USB CDC is unaffected either way; it runs off the USB PLL.
    const bool clock_ok = set_sys_clock_khz(SYS_CLOCK_KHZ, false);
#endif

    stdio_init_all();
    init_led();

#if DECTALK_AUDIO_I2S
    printf("\nDECtalkMini I2S speech self test\n");
    printf("System clock: %lu Hz (SDK default)\n",
           (unsigned long)clock_get_hz(clk_sys));
    printf("Audio: I2S, mono, %u Hz on BCLK GP%u, LRCLK GP%u, data GP%u\n",
           (unsigned)AUDIO_SAMPLE_RATE,
           (unsigned)PICO_AUDIO_I2S_CLOCK_PIN_BASE,
           (unsigned)PICO_AUDIO_I2S_CLOCK_PIN_BASE + 1u,
           (unsigned)PICO_AUDIO_I2S_DATA_PIN);
#else
    printf("\nDECtalkMini PWM speech self test\n");
    printf("System clock: %lu Hz (%u kHz requested: %s)\n",
           (unsigned long)clock_get_hz(clk_sys), (unsigned)SYS_CLOCK_KHZ,
           clock_ok ? "ok" : "refused, falling back");
    printf("Audio: 1-bit PWM, mono, %u Hz on GP%u\n",
           (unsigned)AUDIO_SAMPLE_RATE, (unsigned)DECTALK_PWM_GPIO);
#endif

    // Blink through the startup delay so a board with no audio attached still
    // shows a sign of life.
    for (unsigned i = 0; i < STARTUP_DELAY_MS / 200u; ++i) {
        set_led(i % 2u == 0u);
        sleep_ms(200);
    }
    set_led(false);

    audio_pool = init_audio();
    printf("Audio initialised\n");

    TextToSpeechInit(write_wav, NULL);
    printf("DECtalk initialised, starting loop\n");

    for (uint32_t pass = 1u;; ++pass) {
        char banner[MAX_LINE_LEN];

        printf("pass %lu\n", (unsigned long)pass);
        snprintf(banner, sizeof(banner), "Pass %lu.", (unsigned long)pass);
        say(banner);
        sleep_ms(PHRASE_GAP_MS);

        for (size_t i = 0; i < PHRASE_COUNT; ++i) {
            say(phrases[i]);
            sleep_ms(PHRASE_GAP_MS);
        }

        sleep_ms(PASS_GAP_MS);
    }
}
