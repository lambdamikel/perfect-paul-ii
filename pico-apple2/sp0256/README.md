# Perfect Paul ][ — SP0256-AL2 firmware

An alternative firmware for the **same card**. Flash this `.uf2` instead of the
DECtalk one and the Apple II gains a **General Instrument SP0256-AL2** — the
chip in the **Echo ][**, the Street Electronics **Cricket**, the Amstrad
**SSA-1** and the Radio Shack speech module — emulated by LPC synthesis from
its own mask ROM, not played back from samples.

No hardware change. Same slot interface, same 74LVC245, same audio stage, same
stop byte. The card does not know which firmware it is running.

It speaks two ways:

* **English text**, converted in firmware by the **NRL letter-to-sound rules**
  (NRL Report 7948, 1976). POKE a sentence, hear a sentence.
* **Allophones**, by name (`HH1 EH LL OW`) or by number — the authentic
  SP0256 interface, and what every 1980s allophone listing is written in.

## Why this is a separate build, and why it is the first one that can ship

The SP0256 core is **GPL-3**. The DECtalk core carries affirmative proprietary
notices from DEC, Fonix and Force. A single binary holding both could not be
distributed **by anybody**, because GPL-3 requires complete corresponding
source for the whole work and that source cannot be given for DECtalk.

So two executables is not a convenience, it is the only lawful arrangement —
and it has an upside. Perfect Paul ][ has never published a firmware binary.
**This one can be**, source and `.uf2`, under GPL-3.

| | licence |
|---|---|
| `vendor/sp0256.c`, `sp0256.h` | GPL-2.0-or-later — Joe Zbiciak's jzIntv LPC engine |
| `vendor/sp0256_al2.*`, `types.h` | GPL-3 — Michel Bernard, `GmEsoft/SP0256_CTS256A-AL2` |
| `core/`, `pp2_sp0256.*`, `nrl.*`, `main_sp0256.c` | GPL-3 |
| `../apple2_slot_rx.pio` | MIT, shared with the DECtalk firmware |
| `pico_sdk_import.cmake`, `pico_extras_import.cmake` | BSD-3-Clause |
| **this build as a whole** | **GPL-3** |

`vendor/README-VENDOR.md` records the two local changes to the vendored core
and why each was necessary. The build needs **no DECtalkMini checkout**:
everything it compiles is in this directory.

## The protocol

One write register, as before. In BASIC, slot 4 is `SP = -16192`; for another
slot, `SP = -16256 + 16 * SL`.

| byte | meaning | BASIC |
|---|---|---|
| `$B0` 176 | **text mode** for every line from now on (the default) | `POKE SP,176` |
| `$B1` 177 | **allophone mode** for every line from now on | `POKE SP,177` |
| `$20`–`$7E` | text, buffered | `POKE SP,ASC(C$)` |
| `$0D` 13 | speak the buffered line | `POKE SP,13` |
| `$C0`–`$FF` | **one raw allophone**, code = byte AND 63, spoken at once | `POKE SP,192+A` |
| `$08`, `$7F` | backspace | |
| `$03`, `$90` 144 | immediate stop, queue dropped | `POKE SP,144` |

The mode byte is **sticky, not first-byte-only**, so a program can narrate in
English and drop into allophones for an effect without resetting the card. The
raw `$C0` channel works in **either** mode, because it is a separate channel
rather than a line.

Text mode is the default on purpose: it makes this firmware a drop-in for the
DECtalk one for any program that simply POKEs a sentence. `SP.ELIZA` on the
demo disk is the DECtalk `ELIZA` with four lines changed, and three of those
are comments — it only ever sent plain text.

In allophone mode a line is space- or comma-separated tokens, each either one
of the 64 datasheet mnemonics (any case) or a decimal number 0–63. These three
lines are **byte-identical** in the rendered audio, which the host test checks:

```
HH1 EH LL OW
hh1 eh ll ow
27 7 45 53
```

### No status, so no handshake

The slot interface is write-only — the PIO program captures writes and the card
never drives the data bus — so unlike the real chip there is **no LRQ or SBY to
poll**. Depth substitutes for flow control: **4096 allophones**, about five
minutes, are buffered ahead of the chip. Applesoft POKEs at roughly 3.5 ms per
character and the chip consumes one allophone every ~70 ms, so a program gains
on the chip and can get about fifteen seconds ahead before the stage overflows.
Long demos pace themselves; `SP.PHRASES` shows how, with waits measured on a PC
rather than guessed.

Dropped and rejected counts are printed over USB CDC when they change. A
dropped allophone means pacing; a rejected one means a token that was not an
allophone. Both are otherwise silent, and both are easy to mistake for a
hardware fault.

### What high ASCII costs

Allophone codes are 0–63 and printable ASCII starts at `$20`, so codes
`$20`–`$3F` collide with space, the digits and the punctuation. The DECtalk
firmware masks bit 7 defensively; this one spends the high bits on the raw
channel instead. **`$80`–`$BF` are ignored and `$C0`–`$FF` are allophones, not
letters.** Applesoft's `ASC()` returns 7-bit values, which is what the demos
send, so this costs nothing in practice — but a driver that sets bit 7 on text
will not be understood.

## Text to speech

`nrl.c` implements NRL Report 7948, *Automatic Translation of English Text to
Phonetic Codes* (Elovitz, Johnson, McHugh and Shore, 1976) — a US Government
work. It is written from the report's own notation, not lifted from a binary:
the version Amstrad shipped in 1985 as the SSA-1's `|SAY` is a self-relocating
Z80 blob with no licence attached.

Two stages, separately testable:

1. **letter to sound** — longest-match rules scanned by first letter, each with
   a left and right context in the report's classes (`#` vowels, `^` a
   consonant, `%` a suffix, `@` the long-U consonants, and so on).
2. **phoneme to allophone** — where the chip's positional variants get chosen.

Stage 2 is where most of the intelligibility lives, and three decisions carry
it:

* **The vowel+r allophones.** `OR` `AR` `YR` `XR` exist because /r/ after a
  vowel is what the chip is worst at. They are used when the r **closes** the
  syllable; an r between two vowels is the next syllable's onset and stays a
  real `RR1`, or `VERY` comes out "vairy" and `SORRY` "sarry".
* **A pause in front of every plosive**, or its burst is inaudible: `PA2`
  word-initially, `PA1` inside a word.
* **Geminate collapse.** The exception table forces short vowels by doubling
  the following consonant — `BIZZY`, `WIMMIN`, `DIJJIT` — which also asks the
  chip to say that consonant twice, and it audibly does. English has no
  geminate consonant inside a word, so the repeat is dropped. A word boundary
  between the two survives, so `BUS STOP` keeps both.

### The exception table, re-measured

The 1976 rules have no dictionary. The table of respellings comes from
[`animated-speaking-cpc-vector-head`](https://github.com/lambdamikel/animated-speaking-cpc-vector-head),
where each entry was **measured against the 1985 engine** on an emulated CPC
rather than guessed — then re-measured here, because that engine held a
cut-down ruleset in 5170 bytes of Z80 and this is the full one.

**Seventeen of its thirty-five entries turned out to be unnecessary or
harmful.** `HEAD BROUGHT SAID DOES MOTHER WHO GREAT BUILD THOUGHT THROUGH` the
full rules get right unaided — those ten were only ever working around what
1985 left out. `ONCE WHERE PEOPLE AGAIN DIGITAL DIGITIZE DIGITIZED` the rules
also get right, and the respelling made them **worse**: `WUNS` voices the final
s to a z, `WAIR` loses the `WH`, and `DIJJI TIZED` puts a word boundary in the
middle of a word. Two had to change: `HY` relied on Y counting as a vowel after
H, which it does not here, so the H was deleted and `HI` came out as "eye".

Five were added, each measured the same way, and each is the rules working
exactly as specified and still being wrong — `ABOUT YOUR VERY WANT PROBLEM`.
`nrl.c` says why for every one.

`BROWN` is deliberately **not** in the table. `OW` defaults to the vowel of
"know" and the report patches `NOW HOW DOW POW` one at a time; English spells
both sounds the same way and no respelling reaches the `AW` of "out" without
splitting the word. It says "brone". That is the ruleset being honest about a
1976 compromise, and it is the kind of thing `host/nrl_test` is for.

## Build

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
export PICO_EXTRAS_PATH=/path/to/pico-extras
cmake --preset pico-i2s-release
cmake --build --preset pico-i2s-release
```

| preset | audio | output directory |
|---|---|---|
| `pico-i2s-release` | I2S, GP20/21/22 | `build/i2s-release/` |
| `pico-pwm-release` | PWM, GP28 | `build/pwm-release/` |

Each preset builds two images:

* `perfectpaul2_sp0256.uf2` — the card firmware.
* `perfectpaul2_sp0256_selftest.uf2` — speaks a script in a loop by itself,
  needing no Apple II and no slot traffic.

| option | default | effect |
|---|---|---|
| `SP0256_AUDIO_I2S` | `ON` | I2S backend; `OFF` selects PWM |
| `SP0256_DEFAULT_TEXT_MODE` | `ON` | start in text mode rather than allophone mode |
| `SP0256_SPEAK_STARTUP_BANNER` | `ON` | say "Perfect Paul Two ready" at power-up |
| `SP0256_BUILD_SELFTEST` | `ON` | also build `perfectpaul2_sp0256_selftest` |
| `SP0256_PWM_GPIO` | `28` | GPIO for PWM audio when I2S is off |

About 69 KB of flash and 12 KB of RAM, 4 KB of which is the allophone stage.
`CMakeLists.txt` keeps `pico_enable_stdio_uart(... 0)` and `main_sp0256.c`
fails the build loudly if that changes: the 74LVC245 drives GP0–GP7 push-pull
from the same 3.3 V rail with no series resistors, and GP0/GP1 are UART0.

## Test it before it touches hardware

`host/` builds the firmware's **own** protocol and voice code on a PC, so a
mistake shows up in a WAV rather than in a slot.

```bash
cd host && make

./pp2_render -o hi.wav "HELLO THERE"          # English, the default mode
./pp2_render -a -o hi.wav "HH1 EH LL OW"      # allophone mode
./pp2_render -p 219,199,237,245 -o hi.wav     # raw POKEs, byte for byte
./pp2_render -q "..."                         # duration only, for pacing

./nrl_test HEAD LIVE BROUGHT                  # what do the rules make of it
./nrl_test -x HEAD LIVE BROUGHT               # without the exception table
./nrl_test -l                                 # list the respellings
./nrl_test -w out.wav "HELLO THERE"           # and hear it
```

`pp2_render` runs the real `pp2_sp0256.c`, the real resampler and the real
11025 Hz output rate, and a rendered phrase is **byte-identical** to
LambdaSpeak 4's independently validated renderer over the whole overlap.

`nrl_test -x` is the interesting one. A ruleset that gets `HEAD` and `HI` right
with the exception table **off** is not the NRL ruleset: it is documented to
read them as "heed" and "hih", and this implementation does.

## The self test

`perfectpaul2_sp0256_selftest.uf2` needs **no Apple II, no 74LVC245 and no slot
traffic**. It speaks a fixed script in a loop and names each item on the USB
console, so the chip emulation and the audio path can be confirmed before the
rest of the card exists. **Flash it first on a new board** — if it is silent,
the fault is in the audio path rather than the bus interface.

```
1. banner, by hand                    PP ER1 FF IH KK2 TT2 ...
2. the same sentence, by rule         "HELLO. I AM PERFECT PAUL TWO."
3. the same sentence, by hand         HH1 EH LL OW PA4 AY PA2 AE MM ...
4. text to speech                     "I LIVE IN AN APPLE TWO E."
5. text to speech                     "MY VOICE IS A SINGLE CHIP."
6. text to speech                     "SHALL WE PLAY A GAME."
7. text to speech                     "TELL ME MORE ABOUT YOUR PROBLEM."
8. all 64 allophones                  each one named as it sounds
```

So it is the demo as well as the test: items 2 and 3 are the same sentence
through the rules and then from allophones written by hand, which is the
comparison worth hearing, and item 8 is a guided tour of the chip.

Two things make it worth more than a canned loop:

* **It runs the firmware's own audio code**, through `pp2_audio.h`, rather than
  a copy of it. The DECtalk self test keeps in step with its firmware through a
  CMake function, which guarantees the same backend but not the same code; here
  the two compile the same `pp2_audio.c`, so a self test that makes a sound is
  a firmware that makes the same sound. A `pp2_configure_audio()` function
  still picks the backend, because **a self test built for the other backend is
  silent by construction and tells you nothing** — build and flash it from the
  same preset as the firmware.
* **The audio buffer is its own clock.** One buffer is 71 samples at 11025 Hz,
  so filling one is 6.44 ms of real time; the gaps between items are counted in
  buffers rather than timed, and cannot drift away from the sound. It also only
  advances once the chip is genuinely idle, which no fixed delay can know.

Everything runs on core 0. The firmware puts audio on core 1 because core 0
must drain the slot FIFO; here there is nothing to service, and one core means
one less thing that can be wrong during bring-up.

## Flash

0. Optional but recommended on a new board: flash the self test first and
   listen. No Apple II needed.
1. Power the Apple II off and remove the card.
2. Hold `BOOTSEL` while connecting USB.
3. Copy `perfectpaul2_sp0256.uf2` to the RP2 volume, or use `picotool`.
4. Refit the card and power up. The LED lights once audio is ready and the card
   says "Perfect Paul Two ready."

If it is silent at power-up the fault is the audio path, not the bus — the
banner needs no Apple II. USB CDC stays enabled and uses the identical
protocol: type `HELLO THERE` and press Enter, Ctrl-C stops.

## The demo disk

`SP0256.dsk` is a bootable 140K ProDOS disk, volume `/SP0256/`, that starts in
the menu. The same programs are also on `DAISY.dsk` beside the DECtalk demos,
which has the room — but the two firmwares are mutually exclusive, so the
dedicated disk is the one to reach for.

| program | |
|---|---|
| `SP.HELLO` | says hello three ways: names, raw POKEs, numbers. The smoke test |
| `SP.TALK` | type English, hear English |
| `SP.TYPE` | type allophones, hear allophones |
| `SP.SWEEP` | all 64 allophones with name, datasheet example word and duration, paced so the screen keeps up |
| `SP.PHRASES` | the same six sentences twice — hand-written allophones, then the rules |
| `SP.ELIZA` | Weizenbaum 1966, out loud |

Listings are in the parent directory as `apple2_sp_*.bas`. Two traps if you
rebuild the disk: **never leave a bare `REM`** — AppleCommander's tokenizer
swallows the following line, which silently un-assigns the slot address and
makes the card look dead — and verify a round trip by comparing quoted
literals, which is what caught it.

## Known limits

* **No status register.** No LRQ, no SBY, so nothing can ask whether the card
  is still talking. Depth and pacing instead.
* **A fixed pitch.** The SP0256-AL2 has no pitch control through the allophone
  interface. It cannot sing, and no amount of rule work changes that.
* **`AH` and `AX` are one sound.** The AL2 has no distinct /ʌ/, so "up" and
  "a" share a vowel.
* **1976 rules, no dictionary.** They will mispronounce things. That is the
  trade for fitting in a few kilobytes, and `host/nrl_test` plus the exception
  table are how a specific word gets fixed.
* **Never run on hardware.** Everything here is verified against the host
  renderer and against LambdaSpeak 4's. Nothing has met an Apple II yet — the
  self test is the shortest path to changing that, since it needs none of the
  bus interface.

## Also worth doing

The **Vector Head** — the animated wireframe head that speaks on the CPC — is
the obvious next thing to port, and most of what made it hard there is already
solved here. On the CPC it had to extract a Z80 speech blob, work around a
hard-wired buffer at `&9600` and an interrupt ticker that clobbered `IX`. On
the Apple II the letter-to-sound rules are **in the card**, so the host side is
a line of text and a `POKE`, and the 6502 is free to draw. The allophone
durations are in `SP.SWEEP`'s data already, which is what the mouth shapes have
to be timed against.
