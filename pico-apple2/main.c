// SPDX-License-Identifier: MIT
// Apple II slot front-end for DECtalkMini's Raspberry Pi Pico SDK port.
// This file only covers the adapter/front-end code. The DECtalkMini core has
// separate upstream copyright and licensing terms; see LICENSE-NOTE.md.

#include <assert.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "apple2_slot_rx.pio.h"
#include "epsonapi.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"

#if DECTALK_AUDIO_I2S
#include "pico/audio_i2s.h"
#else
#include "pico/audio_pwm.h"
#endif

#define APPLE2_DATA_BASE_GPIO 0u
#define APPLE2_WRITE_STROBE_GPIO 8u
#define APPLE2_DATA_WIDTH 8u

_Static_assert(APPLE2_WRITE_STROBE_GPIO ==
                   APPLE2_DATA_BASE_GPIO + APPLE2_DATA_WIDTH,
               "PIO expects /WRSEL immediately after D7");

// The 74LVC245 is permanently enabled and drives GP0-GP7 push-pull from the
// same 3.3 V rail as the Pico, with no series resistors. Anything that turns
// one of those pins into an output therefore creates a direct rail-to-rail
// fight between two CMOS drivers; the retired 74LS revision's 4.7 kOhm
// resistors used to hold that under a milliamp, and nothing limits it now.
// GP0/GP1 are the default UART0 TX/RX, so UART stdio is the realistic way to
// trip over this. CMakeLists disables it deliberately; fail the build loudly
// if that ever changes.
#if defined(LIB_PICO_STDIO_UART) && LIB_PICO_STDIO_UART
#if defined(PICO_DEFAULT_UART_TX_PIN) &&                    \
    PICO_DEFAULT_UART_TX_PIN >= APPLE2_DATA_BASE_GPIO &&    \
    PICO_DEFAULT_UART_TX_PIN <= APPLE2_WRITE_STROBE_GPIO
#error "UART stdio would drive a pin the 74LVC245 also drives. Keep pico_enable_stdio_uart(<target> 0), or move the UART off the bus pins."
#endif
#endif

#define MAX_LINE_LEN 256u
#define LINE_QUEUE_DEPTH 8u
#define AUDIO_SAMPLE_RATE 11025u
#define AUDIO_BUFFER_SAMPLES 71u

#ifndef DECTALK_PWM_GPIO
#define DECTALK_PWM_GPIO 28u
#endif

#if !DECTALK_AUDIO_I2S
// pico_audio_pwm's PIO program assumes a 48 MHz PIO clock and the library never
// sets a divider, so at the SDK's default 125 MHz everything plays 2.6x too
// fast. Run the core at 96 MHz and divide the PIO clock by exactly two, which
// leaves DECtalk twice the budget that set_sys_clock_48mhz() would. The I2S
// path derives its own dividers from clk_sys and needs none of this.
#define PWM_PIO_CLOCK_HZ 48000000u
#define PWM_SYS_CLOCK_KHZ 96000u
#define AUDIO_PIO (PICO_AUDIO_PWM_PIO ? pio1 : pio0)
#endif

#ifndef DECTALK_SPEAK_STARTUP_BANNER
#define DECTALK_SPEAK_STARTUP_BANNER 0
#endif

typedef struct {
    char text[MAX_LINE_LEN];
    bool sync;
} tts_line_t;

typedef struct {
    PIO pio;
    uint sm;
    uint offset;
} apple2_bus_t;

static queue_t line_queue;
static struct audio_buffer_pool *audio_pool;
static volatile bool stop_requested = false;

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

    const struct audio_format *output_format = NULL;
    bool ok = false;

#if DECTALK_AUDIO_I2S
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
    ok = audio_i2s_connect(producer_pool);
    assert(ok);
    audio_i2s_set_enabled(true);
#else
    struct audio_pwm_channel_config config = default_mono_channel_config;
    config.core.base_pin = DECTALK_PWM_GPIO;
    output_format = audio_pwm_setup(&audio_format, -1, &config);
    if (output_format == NULL) {
        panic("Unable to set up PWM audio");
    }

    // audio_pwm_setup() ends with pio_sm_init(), which resets the divider to
    // 1.0, so this has to happen after setup and before the machine runs.
    pio_sm_set_clkdiv(AUDIO_PIO, PICO_AUDIO_PWM_MONO_PIO_SM,
                      (float)clock_get_hz(clk_sys) / (float)PWM_PIO_CLOCK_HZ);

    ok = audio_pwm_default_connect(producer_pool, false);
    assert(ok);
    audio_pwm_set_enabled(true);
#endif

    (void)output_format;
    return producer_pool;
}

// DECtalk callback, executed on core 1.
static short *write_wav(short *samples, long length, int phoneme) {
    (void)phoneme;

    struct audio_buffer *buffer = NULL;
    while ((buffer = take_audio_buffer(audio_pool, false)) == NULL) {
        if (stop_requested) {
            TextToSpeechReset();
            return samples;
        }
        tight_loop_contents();
    }

    if (stop_requested) {
        give_audio_buffer(audio_pool, buffer);
        TextToSpeechReset();
        return samples;
    }

    if (length > (long)AUDIO_BUFFER_SAMPLES) {
        // The existing Pico port uses 71-sample buffers and its callback is
        // expected not to exceed that. Clamp defensively rather than overrun.
        length = AUDIO_BUFFER_SAMPLES;
    }

    memcpy(buffer->buffer->bytes, samples, (size_t)length * sizeof(short));
    buffer->sample_count = (uint32_t)length;
    give_audio_buffer(audio_pool, buffer);
    return samples;
}

static void core1_tts_task(void) {
    audio_pool = init_audio();
    TextToSpeechInit(write_wav, NULL);

    multicore_fifo_push_blocking(1u);

#if DECTALK_SPEAK_STARTUP_BANNER
    TextToSpeechStart((char *)"DECtalk Apple Two is ready.", NULL, WAVE_FORMAT_1M16);
    TextToSpeechSync();
#endif

    tts_line_t line;
    for (;;) {
        while (!queue_try_remove(&line_queue, &line)) {
            if (stop_requested) {
                stop_requested = false;
            }
            tight_loop_contents();
        }

        stop_requested = false;
        TextToSpeechStart(line.text, NULL, WAVE_FORMAT_1M16);

        if (line.sync) {
            TextToSpeechSync();
            if (stop_requested) {
                tts_line_t discard;
                while (queue_try_remove(&line_queue, &discard)) {
                }
            }
        }
    }
}

static void enqueue_text(char *buffer, size_t length, bool finish_utterance) {
    // Reserve one byte for DECtalk's vertical-tab flush and one for NUL.
    if (length > MAX_LINE_LEN - 2u) {
        length = MAX_LINE_LEN - 2u;
    }

    tts_line_t line = {0};
    memcpy(line.text, buffer, length);
    if (finish_utterance) {
        line.text[length++] = '\x0b';
    }
    line.text[length] = '\0';
    line.sync = finish_utterance;

    queue_add_blocking(&line_queue, &line);
}

static void request_stop(size_t *length) {
    stop_requested = true;
    *length = 0u;

    // Preserve the behavior of the existing Pico port: do not accept a new
    // utterance until core 1 has acknowledged/reset the current one.
    while (stop_requested) {
        tight_loop_contents();
    }
}

static void consume_character(uint8_t raw, char *buffer, size_t *length) {
    // Existing DECtalkMini Pico protocol uses 0x90 as an immediate stop byte.
    if (raw == 0x90u || raw == 0x03u) {
        request_stop(length);
        return;
    }

    // Apple II software sometimes carries screen characters with bit 7 set.
    // Mask it for ordinary text while preserving 0x90 above as a command.
    const uint8_t ch = raw & 0x7fu;

    switch (ch) {
        case 0x00u:
            return;

        case '\b':
        case 0x7fu:
            if (*length > 0u) {
                --(*length);
            }
            return;

        case '\r':
        case '\n':
        case '\v':
            if (*length > 0u) {
                enqueue_text(buffer, *length, true);
                *length = 0u;
            }
            return;

        case '\t':
            // DECtalk is happier with an ordinary separator than a literal tab.
            raw = (uint8_t)' ';
            break;

        default:
            if (ch < 0x20u) {
                return;
            }
            raw = ch;
            break;
    }

    if (*length >= MAX_LINE_LEN - 2u) {
        // Continue a long logical line without forcing an utterance boundary.
        enqueue_text(buffer, *length, false);
        *length = 0u;
    }

    buffer[(*length)++] = (char)raw;
}

static apple2_bus_t apple2_bus_init(void) {
    apple2_bus_t bus = {
        .pio = pio1, // pico_audio_i2s uses PIO0 in this port
        .sm = 0u,
        .offset = 0u,
    };

    bus.sm = pio_claim_unused_sm(bus.pio, true);
    bus.offset = pio_add_program(bus.pio, &apple2_slot_rx_program);

    // U2 (74LVC245) drives these inputs directly from the Pico's own 3.3 V
    // rail, push-pull and unresistored, so the pads must present nothing but a
    // high-impedance input: clear the pull-down the RP2040 enables at reset.
    //
    // GP8 keeps a pull-up purely so the strobe reads idle when U3 is absent or
    // unfitted during bring-up; it costs ~55 uA against the gate's low drive.
    // It is NOT what keeps a bench-powered card idle any more. U3 now runs
    // from the Pico's 3.3 V rail, so it stays live with the Apple II off, and
    // its floating /DEVSEL and R/W inputs could assert /WRSEL on their own.
    // The pull-ups on those two gate inputs are what prevent that -- see
    // HARDWARE-74LVC.md.
    for (uint pin = APPLE2_DATA_BASE_GPIO;
         pin < APPLE2_WRITE_STROBE_GPIO;
         ++pin) {
        pio_gpio_init(bus.pio, pin);
        gpio_disable_pulls(pin);
    }
    pio_gpio_init(bus.pio, APPLE2_WRITE_STROBE_GPIO);
    gpio_pull_up(APPLE2_WRITE_STROBE_GPIO);

    pio_sm_config config = apple2_slot_rx_program_get_default_config(bus.offset);
    sm_config_set_in_pins(&config, APPLE2_DATA_BASE_GPIO);
    sm_config_set_jmp_pin(&config, APPLE2_WRITE_STROBE_GPIO);
    sm_config_set_in_shift(&config, false, false, 32u);
    sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_RX);

    pio_sm_set_consecutive_pindirs(
        bus.pio,
        bus.sm,
        APPLE2_DATA_BASE_GPIO,
        APPLE2_DATA_WIDTH + 1u,
        false);
    pio_sm_init(bus.pio, bus.sm, bus.offset, &config);
    pio_sm_set_enabled(bus.pio, bus.sm, true);

    return bus;
}

static void init_ready_led(void) {
#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 0);
#endif
}

static void set_ready_led(bool ready) {
#ifdef PICO_DEFAULT_LED_PIN
    gpio_put(PICO_DEFAULT_LED_PIN, ready ? 1 : 0);
#else
    (void)ready;
#endif
}

int main(void) {
#if !DECTALK_AUDIO_I2S
    // Before stdio_init_all() so UART divisors follow the final clock.
    set_sys_clock_khz(PWM_SYS_CLOCK_KHZ, false);
#endif

    stdio_init_all();
    init_ready_led();

    printf("\nDECtalkMini Apple II slot firmware\n");
    printf("System clock: %" PRIu32 " Hz\n", clock_get_hz(clk_sys));

    queue_init(&line_queue, sizeof(tts_line_t), LINE_QUEUE_DEPTH);
    multicore_launch_core1(core1_tts_task);

    // Do not expose the write receiver until audio and DECtalk are ready.
    multicore_fifo_pop_blocking();
    const apple2_bus_t bus = apple2_bus_init();

    set_ready_led(true);
    printf("READY: 74LVC245 D0-D7 on GP0-GP7, /WRSEL on GP8 (3.3 V logic)\n");

    char apple_buffer[MAX_LINE_LEN] = {0};
    size_t apple_length = 0u;
    char usb_buffer[MAX_LINE_LEN] = {0};
    size_t usb_length = 0u;

    for (;;) {
        while (!pio_sm_is_rx_fifo_empty(bus.pio, bus.sm)) {
            const uint8_t byte = (uint8_t)(pio_sm_get(bus.pio, bus.sm) & 0xffu);
            consume_character(byte, apple_buffer, &apple_length);
        }

        // USB CDC remains a convenient bench-test and debug input. It uses the
        // same line protocol as the Apple II: CR/LF speaks, Ctrl-C stops.
        for (;;) {
            const int input = getchar_timeout_us(0u);
            if (input == PICO_ERROR_TIMEOUT) {
                break;
            }
            consume_character((uint8_t)input, usb_buffer, &usb_length);
        }

        tight_loop_contents();
    }
}
