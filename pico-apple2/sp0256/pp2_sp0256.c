/* Perfect Paul ][ -- SP0256-AL2 firmware: the Apple II byte protocol.
   SPDX-License-Identifier: GPL-3.0-or-later

   See pp2_sp0256.h for the protocol. No Pico SDK here, on purpose. */

#include "pp2_sp0256.h"
#include "sp0256_speak.h"
#include "sp0256_rom.h"
#include "nrl.h"

#include <string.h>

#define STAGE_MASK (PP2_SP_STAGE_LEN - 1u)
_Static_assert((PP2_SP_STAGE_LEN & STAGE_MASK) == 0u,
               "PP2_SP_STAGE_LEN must be a power of two");

/* The stage is single-producer / single-consumer: the Apple II core only ever
   advances `head`, the audio core only ever advances `tail`. Both run free and
   are masked on use, so uint32 wraparound is harmless for a power-of-two
   length. The fences are what make the data store visible before the index
   store on a second core; they cost nothing on the host. */
#if defined(__GNUC__)
#define RELEASE_FENCE() __atomic_thread_fence(__ATOMIC_RELEASE)
#define ACQUIRE_FENCE() __atomic_thread_fence(__ATOMIC_ACQUIRE)
#else
#define RELEASE_FENCE() do { } while (0)
#define ACQUIRE_FENCE() do { } while (0)
#endif

static uint8_t           stage[PP2_SP_STAGE_LEN];
static volatile uint32_t stage_head;   /* producer only */
static volatile uint32_t stage_tail;   /* consumer only */
static volatile uint32_t stop_req;     /* producer increments */
static volatile uint32_t stop_ack;     /* consumer copies */

#ifndef PP2_SP_DEFAULT_MODE
#define PP2_SP_DEFAULT_MODE PP2_SP_TEXT
#endif

/* Producer-side state: only the Apple II / USB core touches it. */
static pp2_sp_mode_t line_mode = PP2_SP_DEFAULT_MODE;

void          pp2_sp_set_mode(pp2_sp_mode_t mode) { line_mode = mode; }
pp2_sp_mode_t pp2_sp_get_mode(void)               { return line_mode; }

static volatile uint32_t n_dropped;
static volatile uint32_t n_rejected;
static volatile uint32_t n_spoken;

void pp2_sp_init(void)
{
    stage_head = stage_tail = 0u;
    stop_req = stop_ack = 0u;
    n_dropped = n_rejected = n_spoken = 0u;
    sp0256_speak_init();
}

int pp2_sp_push(uint8_t allophone)
{
    if (allophone >= SP0256_ALLOPHONE_COUNT) {
        ++n_rejected;
        return 0;
    }
    if ((uint32_t)(stage_head - stage_tail) >= PP2_SP_STAGE_LEN) {
        ++n_dropped;
        return 0;
    }
    stage[stage_head & STAGE_MASK] = allophone;
    RELEASE_FENCE();
    ++stage_head;
    ++n_spoken;
    return 1;
}

static void request_stop(void)
{
    RELEASE_FENCE();
    ++stop_req;
}

int pp2_sp_stop_pending(void)
{
    return stop_req != stop_ack;
}

void pp2_sp_service(void)
{
    ACQUIRE_FENCE();

    const uint32_t req = stop_req;
    if (req != stop_ack) {
        /* Dropping the stage is the consumer writing its own index, which is
           why it is safe without a lock. Reading a head the producer has since
           advanced simply leaves its newest byte or two queued -- the firmware
           closes that window by waiting on pp2_sp_stop_pending(). */
        stage_tail = stage_head;
        sp0256_speak_init();   /* halts the chip and empties its own queue */
        stop_ack = req;
        return;
    }

    /* Keep the chip's 256-deep queue as full as it will go. Advance tail only
       on a successful push, so a full queue costs nothing but a retry. */
    while (stage_tail != stage_head) {
        if (!sp0256_speak_push(stage[stage_tail & STAGE_MASK])) {
            break;
        }
        ++stage_tail;
    }
}

int16_t pp2_sp_sample(void)
{
    return sp0256_speak_next_sample();
}

uint32_t pp2_sp_staged(void)   { return (uint32_t)(stage_head - stage_tail)
                                      + (uint32_t)sp0256_speak_queued(); }
uint32_t pp2_sp_stage_depth(void) { return (uint32_t)(stage_head - stage_tail); }
uint32_t pp2_sp_dropped(void)  { return n_dropped;  }
uint32_t pp2_sp_rejected(void) { return n_rejected; }
uint32_t pp2_sp_spoken(void)   { return n_spoken;   }

/* ---------------------------------------------------------------- parsing */

static int is_separator(char c)
{
    return c == ' ' || c == '\t' || c == ',' || c == ';';
}

/* The longest AL2 mnemonic is three characters; anything longer cannot be one,
   and refusing it here keeps the copy below bounded. */
#define MNEMONIC_MAX 3u

static int token_to_allophone(const char *tok, size_t n)
{
    if (n == 0u) {
        return -1;
    }

    /* All digits -> the allophone by number, as printed in every published
       SP0256 table. Rejecting >63 rather than masking means a typo is counted
       instead of silently speaking the wrong sound. */
    size_t digits = 0u;
    for (size_t i = 0u; i < n; ++i) {
        if (tok[i] < '0' || tok[i] > '9') {
            break;
        }
        ++digits;
    }
    if (digits == n) {
        unsigned value = 0u;
        if (n > 2u) {
            return -1;   /* 0..63 is at most two digits */
        }
        for (size_t i = 0u; i < n; ++i) {
            value = value * 10u + (unsigned)(tok[i] - '0');
        }
        return value < SP0256_ALLOPHONE_COUNT ? (int)value : -1;
    }

    if (n > MNEMONIC_MAX) {
        return -1;
    }
    char name[MNEMONIC_MAX + 1u];
    memcpy(name, tok, n);
    name[n] = '\0';
    return sp0256_al2_lookup(name);   /* case-insensitive in the vendor bridge */
}

int pp2_sp_say(const char *text)
{
    if (text == NULL) {
        return 0;
    }

    int queued = 0;
    const char *p = text;
    while (*p != '\0') {
        while (*p != '\0' && is_separator(*p)) {
            ++p;
        }
        const char *start = p;
        while (*p != '\0' && !is_separator(*p)) {
            ++p;
        }
        const size_t n = (size_t)(p - start);
        if (n == 0u) {
            continue;
        }

        const int code = token_to_allophone(start, n);
        if (code < 0) {
            ++n_rejected;
            continue;
        }
        if (pp2_sp_push((uint8_t)code)) {
            ++queued;
        }
    }
    return queued;
}

int pp2_sp_say_english(const char *text)
{
    if (text == NULL) return 0;

    /* One line's worth. 256 allophones is about eighteen seconds of speech,
       far more than MAX_LINE characters of English can ask for. Static for the
       same reason as nrl.c's buffers: 2 KB of stack per core does not hold
       this plus the rule engine. Producer-side only, so one core touches it. */
    static uint8_t allo[256];
    const size_t n = nrl_text_to_al2(text, allo, sizeof allo);

    int queued = 0;
    for (size_t i = 0u; i < n; ++i) {
        if (pp2_sp_push(allo[i])) ++queued;
    }
    return queued;
}

static void speak_line(const char *line)
{
    if (line_mode == PP2_SP_TEXT) {
        (void)pp2_sp_say_english(line);
    } else {
        (void)pp2_sp_say(line);
    }
}

/* ------------------------------------------------------------- transport */

/* A full line buffer is flushed at the last separator, so no mnemonic is ever
   cut in half across the boundary. A single token longer than the buffer is
   not a mnemonic by definition; drop it and count it. */
static void flush_partial(pp2_sp_input_t *in)
{
    size_t cut = in->len;
    while (cut > 0u && !is_separator(in->line[cut - 1u])) {
        --cut;
    }
    if (cut == 0u) {
        in->len = 0u;
        ++n_rejected;
        return;
    }

    const char saved = in->line[cut];
    in->line[cut] = '\0';
    speak_line(in->line);
    in->line[cut] = saved;

    const size_t rest = in->len - cut;
    memmove(in->line, in->line + cut, rest);
    in->len = (uint8_t)rest;
}

void pp2_sp_feed(pp2_sp_input_t *in, uint8_t raw)
{
    /* Same stop bytes as the DECtalk firmware, so POKE SP,144 still shuts the
       card up whichever UF2 is flashed. */
    if (raw == 0x90u || raw == 0x03u) {
        in->len = 0u;
        request_stop();
        return;
    }

    /* Raw allophone: spoken as it arrives, with no terminator, which is what
       an SSA-1 does and what a POKE loop over a DATA table wants. */
    if (raw >= 0xC0u) {
        (void)pp2_sp_push((uint8_t)(raw & 0x3Fu));
        return;
    }
    /* Mode select. Sticky, and it takes effect at the next CR, so a program
       may switch as often as it likes. */
    if (raw == 0xB0u) { line_mode = PP2_SP_TEXT;      return; }
    if (raw == 0xB1u) { line_mode = PP2_SP_ALLOPHONE; return; }

    if (raw >= 0x80u) {
        return;   /* reserved; see the header on why high ASCII is not text */
    }

    switch (raw) {
        case '\r':
        case '\n':
        case '\v':
            if (in->len > 0u) {
                in->line[in->len] = '\0';
                speak_line(in->line);
                in->len = 0u;
            }
            return;

        case '\b':
        case 0x7fu:
            if (in->len > 0u) {
                --in->len;
            }
            return;

        case '\t':
            raw = (uint8_t)' ';
            break;

        default:
            if (raw < 0x20u) {
                return;
            }
            break;
    }

    if (in->len >= PP2_SP_LINE_MAX - 1u) {
        flush_partial(in);
    }
    in->line[in->len++] = (char)raw;
}
