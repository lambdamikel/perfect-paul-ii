/* Perfect Paul ][ -- SP0256-AL2 firmware for the Apple II slot card.
   SPDX-License-Identifier: GPL-3.0-or-later

   An alternative UF2 for the same card as the DECtalk firmware: same slot
   interface, same 74LVC245, same audio stage, same stop byte. Instead of
   DECtalk it runs a cycle-accurate emulation of the General Instrument
   SP0256-AL2 -- the chip in the Apple II's own Echo II and Street Electronics
   Cricket, and in the Amstrad SSA-1 -- rebuilt from its mask ROM by LPC
   synthesis rather than played back from samples.

   The slot front end, the audio setup and the GP0-GP8 warnings below are
   adapted from this card's MIT-licensed DECtalk main.c, which remains
   available under MIT in the parent directory. This file and the binary built
   from it are GPL-3: the SP0256 core is GPL-3, which is also precisely why
   this firmware is a separate executable that contains no DECtalk at all.

   See pp2_sp0256.h for the Apple II byte protocol. */

#include <assert.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "apple2_slot_rx.pio.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "pp2_audio.h"
#include "pp2_sp0256.h"

#define APPLE2_DATA_BASE_GPIO 0u
#define APPLE2_WRITE_STROBE_GPIO 8u
#define APPLE2_DATA_WIDTH 8u

_Static_assert(APPLE2_WRITE_STROBE_GPIO ==
                   APPLE2_DATA_BASE_GPIO + APPLE2_DATA_WIDTH,
               "PIO expects /WRSEL immediately after D7");

/* The 74LVC245 is permanently enabled and drives GP0-GP7 push-pull from the
   same 3.3 V rail as the Pico, with no series resistors. Anything that turns
   one of those pins into an output therefore creates a direct rail-to-rail
   fight between two CMOS drivers. GP0/GP1 are the default UART0 TX/RX, so UART
   stdio is the realistic way to trip over this. CMakeLists disables it
   deliberately; fail the build loudly if that ever changes. */
#if defined(LIB_PICO_STDIO_UART) && LIB_PICO_STDIO_UART
#if defined(PICO_DEFAULT_UART_TX_PIN) &&                    \
    PICO_DEFAULT_UART_TX_PIN >= APPLE2_DATA_BASE_GPIO &&    \
    PICO_DEFAULT_UART_TX_PIN <= APPLE2_WRITE_STROBE_GPIO
#error "UART stdio would drive a pin the 74LVC245 also drives. Keep pico_enable_stdio_uart(<target> 0), or move the UART off the bus pins."
#endif
#endif

#ifndef SP0256_SPEAK_STARTUP_BANNER
#define SP0256_SPEAK_STARTUP_BANNER 0
#endif

/* "Perfect Paul Two ready", by hand, because an SP0256 has no dictionary and
   no letter-to-sound rules -- every allophone is a decision. The choices worth
   knowing: KK2 rather than KK1 or KK3 for the /k/ of "perfect", because KK2 is
   the datasheet's final/pre-consonant variant; TT2 for both /t/ sounds, TT1
   being the weaker one used after a fricative; UW2 for the long /u:/ of "two",
   UW1 being the short one in "to". PA2 between words and PA5 to end. */
#ifndef SP0256_STARTUP_BANNER_TEXT
#define SP0256_STARTUP_BANNER_TEXT \
    "PP ER1 FF IH KK2 TT2 PA2 PP AO LL PA2 TT2 UW2 PA3 " \
    "RR1 EH DD1 IY PA5"
#endif

typedef struct {
    PIO pio;
    uint sm;
    uint offset;
} apple2_bus_t;

static struct audio_buffer_pool *audio_pool;

/* Core 1 owns the audio path and nothing else, because core 0 must be free to
   drain the slot FIFO. The handshake exists so the write receiver is not
   exposed until the chip can actually be heard. */
static void core1_audio_task(void) {
    audio_pool = pp2_audio_init();
    pp2_sp_init();

    multicore_fifo_push_blocking(1u);

    for (;;) {
        pp2_audio_fill(audio_pool);
    }
}

static apple2_bus_t apple2_bus_init(void) {
    apple2_bus_t bus = {
        .pio = pio1, /* pico_audio_i2s uses PIO0 in this port */
        .sm = 0u,
        .offset = 0u,
    };

    bus.sm = pio_claim_unused_sm(bus.pio, true);
    bus.offset = pio_add_program(bus.pio, &apple2_slot_rx_program);

    /* U2 (74LVC245) drives these inputs directly from the Pico's own 3.3 V
       rail, push-pull and unresistored, so the pads must present nothing but a
       high-impedance input: clear the pull-down the RP2040 enables at reset.

       GP8 keeps a pull-up purely so the strobe reads idle when U3 is absent or
       unfitted during bring-up. It is NOT what keeps a bench-powered card
       idle -- the pull-ups on U3's /DEVSEL and R/W inputs are; see the
       hardware reference in the repository README. */
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

/* The stop byte has to be acknowledged by core 1 before core 0 accepts more,
   or an Apple II that POKEs a stop and an allophone back to back loses the
   allophone to the flush. Core 1 answers within one audio buffer, under 7 ms. */
static void settle_stop(void) {
    while (pp2_sp_stop_pending()) {
        tight_loop_contents();
    }
}

int main(void) {
    pp2_audio_set_clock();   /* PWM only, and before stdio_init_all() */

    stdio_init_all();
    init_ready_led();

    printf("\nPerfect Paul ][ -- SP0256-AL2 firmware\n");
    printf("System clock: %" PRIu32 " Hz\n", clock_get_hz(clk_sys));

    multicore_launch_core1(core1_audio_task);

    /* Do not expose the write receiver until audio and the chip are ready. */
    multicore_fifo_pop_blocking();
    const apple2_bus_t bus = apple2_bus_init();

    set_ready_led(true);
    printf("READY: 74LVC245 D0-D7 on GP0-GP7, /WRSEL on GP8 (3.3 V logic)\n");
    printf("Allophones: POKE SP,192+code  |  names: HH1 EH LL OW <CR>\n");

#if SP0256_SPEAK_STARTUP_BANNER
    /* Queued by core 0, which is the stage's only producer.

       The settle first: on this card the Pico is powered from the slot, so it
       is ready within a couple of hundred milliseconds of the Apple II coming
       up -- while the machine is still in its own reset and the audio stage has
       not settled. The DECtalk firmware never needed this because DECtalk's
       own initialisation takes long enough to hide it; the SP0256 is ready
       almost at once, and the banner lands in the power-on transient. */
    sleep_ms(400);
    const int n = pp2_sp_say(SP0256_STARTUP_BANNER_TEXT);
    printf("banner: %d allophones queued\n", n);
#endif

    pp2_sp_input_t slot = {0};
    pp2_sp_input_t usb = {0};
    uint32_t last_dropped = 0u;
    uint32_t last_rejected = 0u;

    /* Trace the first few bytes off the slot. This is the one thing no host
       test can tell you: whether the card is being written to at all, and
       whether what arrives is what the Apple II meant to send. A card that
       answers nothing and a card drowning in phantom strobes look identical
       from the outside, and a phantom $90 silences the queue. */
    uint32_t traced = 0u;

    for (;;) {
        while (!pio_sm_is_rx_fifo_empty(bus.pio, bus.sm)) {
            const uint8_t byte = (uint8_t)(pio_sm_get(bus.pio, bus.sm) & 0xffu);
            if (traced < 24u) {
                ++traced;
                printf("slot[%2" PRIu32 "] = $%02x%s\n", traced, byte,
                       (byte == 0x90u || byte == 0x03u) ? "   <- STOP" : "");
            }
            pp2_sp_feed(&slot, byte);
            settle_stop();
        }

        /* USB CDC remains a convenient bench-test input and uses the identical
           protocol: type "HH1 EH LL OW" and press Enter, Ctrl-C stops. */
        for (;;) {
            const int input = getchar_timeout_us(0u);
            if (input == PICO_ERROR_TIMEOUT) {
                break;
            }
            pp2_sp_feed(&usb, (uint8_t)input);
            settle_stop();
        }

        /* Report only on change: a dropped allophone means the Apple II got
           more than PP2_SP_STAGE_LEN ahead of the chip and needs pacing, and a
           rejected one means a token was not an allophone. Both are silent
           otherwise, and both are easy to misread as a hardware fault. */
        const uint32_t dropped = pp2_sp_dropped();
        const uint32_t rejected = pp2_sp_rejected();
        if (dropped != last_dropped || rejected != last_rejected) {
            printf("dropped=%" PRIu32 " rejected=%" PRIu32 " staged=%" PRIu32 "\n",
                   dropped, rejected, pp2_sp_staged());
            last_dropped = dropped;
            last_rejected = rejected;
        }

        tight_loop_contents();
    }
}
