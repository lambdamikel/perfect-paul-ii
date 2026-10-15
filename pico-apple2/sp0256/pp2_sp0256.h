/* Perfect Paul ][ -- SP0256-AL2 firmware: the Apple II byte protocol.
   SPDX-License-Identifier: GPL-3.0-or-later

   This file is deliberately free of any Pico SDK dependency, so the host
   renderer in host/ parses the identical byte stream the card will see. A
   protocol bug therefore shows up in a WAV on a PC rather than in a slot.

   THE PROTOCOL
   ============
   One write register, as in the DECtalk firmware: the Apple II POKEs bytes at
   $C0n0 and they arrive here one at a time.

     byte        meaning
     ----------  ----------------------------------------------------------
     $C0..$FF    RAW ALLOPHONE. Code = byte & $3F, queued immediately, no
                 terminator -- this is how a real SSA-1 or DK'tronics board
                 behaves, and it works in either mode below.
                 In BASIC:  POKE SP, 192 + A
     $20..$7E    text, buffered into a line
     $0D $0A $0B CR / LF / VT: speak the buffered line
     $08 $7F     backspace
     $B0         TEXT mode for every line from now on   (POKE SP,176)
     $B1         ALLOPHONE mode for every line from now on (POKE SP,177)
     $03 $90     immediate stop: silence the chip and drop everything queued
     anything    ignored
     else

   TWO MODES, SET BY A BYTE
   ========================
   A line is either English or a list of allophones, and one byte switches
   between them. It is sticky rather than first-byte-only, so a program can
   narrate in English and then drop into allophones for a sound effect without
   resetting the card. The default is TEXT, which makes this firmware a
   drop-in for the DECtalk one for any program that just POKEs a sentence.

     TEXT mode       the line is English, run through the NRL letter-to-sound
                     rules -- see nrl.h. "HELLO THERE" says hello there.
     ALLOPHONE mode  the line is allophones, as below. "HH1 EH LL OW" says
                     hello; "HELLO THERE" says nothing but rejected tokens.

   A buffered line in ALLOPHONE mode is whitespace- and comma-separated
   tokens, each either

     * one of the 64 SP0256-AL2 mnemonics from the datasheet -- PA1, AA, OY,
       HH1, KK3, TT2 ... -- in any case, or
     * a decimal number 0..63, the same allophone by number.

   So these three lines all say "hello" in allophone mode:

       HH1 EH LL OW
       hh1 eh ll ow
       27 7 45 53

   WHY THE TOP TWO BITS, AND WHAT IT COSTS
   ---------------------------------------
   Allophone codes are 0..63 and printable ASCII starts at $20, so codes
   $20..$3F collide with space, the digits and the punctuation -- a raw byte
   stream and a text line cannot be told apart in the low 64. The DECtalk
   firmware masks bit 7 because Applesoft sends plain 7-bit ASCII and some
   drivers set the high bit; this firmware spends the high bits on the raw
   channel instead. The cost is that high-ASCII text is NOT accepted here:
   $80..$BF are ignored and $C0..$FF are allophones, not letters. Applesoft's
   ASC() returns 7-bit values, which is what the demos send.

   NO STATUS, SO NO HANDSHAKE
   --------------------------
   The slot interface is write-only -- the PIO program captures writes and the
   card never drives the data bus -- so unlike the real chip there is no LRQ or
   SBY to poll. Depth substitutes for flow control: PP2_SP_STAGE_LEN allophones
   are buffered ahead of the chip. BASIC POKEs at roughly 3.5 ms per character
   while the chip consumes an allophone every ~70 ms, so a program gains on the
   chip and can run about fifteen seconds ahead before the stage overflows.
   Long demos must pace themselves; see the BASIC programs in the repository.
*/
#ifndef PP2_SP0256_H
#define PP2_SP0256_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Allophones buffered ahead of the chip. Power of two. 4096 is about five
   minutes of speech for 4 KB of RAM, which on a 264 KB RP2040 is free. */
#define PP2_SP_STAGE_LEN  4096u
#define PP2_SP_LINE_MAX   128u

/* Per-input parser state. The firmware keeps one for the slot and one for USB
   CDC so a half-typed bench line cannot splice itself into the Apple II's. */
typedef struct {
    char    line[PP2_SP_LINE_MAX];
    uint8_t len;
} pp2_sp_input_t;

/* The two line modes. TEXT is the default unless the build says otherwise. */
typedef enum { PP2_SP_TEXT = 0, PP2_SP_ALLOPHONE = 1 } pp2_sp_mode_t;

void          pp2_sp_set_mode(pp2_sp_mode_t mode);
pp2_sp_mode_t pp2_sp_get_mode(void);

/* Producer side -- the Apple II / USB core. */
void pp2_sp_init(void);
void pp2_sp_feed(pp2_sp_input_t *in, uint8_t raw);
int  pp2_sp_stop_pending(void);   /* 1 until the consumer has flushed */

/* Consumer side -- whoever fills audio buffers. pp2_sp_service() applies a
   pending stop and tops the chip's own queue up from the stage; call it once
   per audio buffer. pp2_sp_sample() returns one sample at SP0256_SAMPLE_RATE
   (10 kHz) and is what sp0256_rs_next() should be handed. */
void    pp2_sp_service(void);
int16_t pp2_sp_sample(void);

/* Diagnostics, printed over USB CDC rather than acted on. */
uint32_t pp2_sp_staged(void);       /* allophones waiting, stage + chip      */
uint32_t pp2_sp_stage_depth(void);  /* the stage alone, for diagnosis        */
uint32_t pp2_sp_dropped(void);      /* lost to a full stage                 */
uint32_t pp2_sp_rejected(void);     /* tokens that were not an allophone    */
uint32_t pp2_sp_spoken(void);       /* allophones accepted since reset      */

/* Exposed for the banner and for tests: queue one allophone directly. */
int pp2_sp_push(uint8_t allophone);

/* Queue a line of allophone names or numbers, without the transport. */
int pp2_sp_say(const char *text);

/* Queue a line of English, through the NRL rules. */
int pp2_sp_say_english(const char *text);

#ifdef __cplusplus
}
#endif
#endif
