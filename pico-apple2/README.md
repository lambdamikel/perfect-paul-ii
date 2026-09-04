# Perfect Paul ][

A DECtalk speech synthesizer card for the Apple II, built on a Raspberry Pi
Pico. Named for DECtalk's default voice, `[:np]`.

This directory is a drop-in Pico SDK target for the supplied DECtalkMini tree.
It keeps DECtalk synthesis and audio on core 1, while core 0 receives bytes
from an Apple II slot write register using RP2040 PIO.

## Status

**Working.** The PWM build has been verified end to end in a real Apple II. It
speaks plain text, sings in DECtalk's phoneme mode, and holds an interactive
session — see the two demo programs below. The tested card is a fabricated
prototype PCB carrying both audio backends at once; a further revision is
planned.

Both the PWM and the I2S build have been run on hardware. See `VALIDATION.md`
for exactly what has and has not been verified.

This revision uses 3.3 V LVC logic as a proper level translator:

- `74LVC245A` for Apple D0-D7 to Pico GP0-GP7
- `74LVC32` to form the active-low selected-write strobe
- both powered from the Pico's 3.3 V rail, not the Apple's +5 V
- two 10 kOhm pull-ups on the strobe gate's Apple-side inputs
- **no series resistors anywhere**

The complete pin-by-pin wiring, power diagram and PCB notes live in the
**Hardware reference** section of the repository README, at
<https://github.com/lambdamikel/perfect-paul-ii>. The earlier 5 V 74LS interface
is retired and no longer documented; 74LVC is the only supported build.

## Important electrical point

RP2040 GPIO is **not specified as 5 V tolerant**, so the Apple's 5 V bus has to
be translated rather than merely attenuated. The LVC family does this properly:
its inputs carry **no clamp diode to V_CC** and are specified to 5.5 V
independent of V_CC, including V_CC = 0. Powering a `74LVC245A` from the Pico's
3.3 V rail and feeding it 5 V Apple TTL is therefore the manufacturer-supported
configuration, and its outputs swing 0-3.3 V into the Pico on the same rail.

That is what makes the series resistors unnecessary: there is no overvoltage to
limit in either direction, and no power-sequencing case to survive, because the
5 V tolerance holds with the part unpowered. The firmware protocol does not
depend on the buffer family, but this interface — unlike the 74LS revision it
replaces — is level translation by the datasheet rather than by approximation.

## Minimal bus interface

```
Apple D0-D7 ---> U2 74LVC245A A1-A8

U2 74LVC245A B1-B8 ---> Pico GP0-GP7        (direct, no resistors)

Apple /DEVSEL --+--> R10 10k --> 3V3
                 \
                  U3 74LVC32 gate 1 OR ---> Pico GP8 (/WRSEL)
                 /
Apple R/W ------+--> R11 10k --> 3V3

U2 DIR = 3V3        (A-to-B, Apple-to-Pico)
U2 /OE = GND        (permanently enabled)
U2/U3 VCC = Pico 3V3_OUT pin 36
All grounds common
```

`/WRSEL = /DEVSEL OR R/W`, so `/WRSEL` is low only when `/DEVSEL` is low and
`R/W` is low: a write to that slot's 16-byte I/O window. The Pico never drives
the Apple II data bus. A0-A3 are intentionally not decoded, so all 16 addresses
in the window are mirrors of the same write-only byte register.

R10 and R11 are not optional. U3 is powered from the Pico now, so it stays live
with the Apple II switched off; without pull-ups its floating inputs can assert
`/WRSEL` on their own and the card will speak noise. This is the one genuinely
new requirement relative to the 74LS revision, where an Apple-powered gate
simply went dead along with the machine.

Place a 100 nF capacitor at each IC. Feed U2 and U3 from the Pico's `3V3_OUT`.
Feed the Pico's `VSYS` through a Schottky diode from Apple +5 V so USB power
cannot back-feed the Apple II while the computer is off. Do not connect Apple
+5 V to the Pico `3V3_OUT` pin — with this revision that rail now feeds real
logic, so a mistake there takes both ICs with it.

A compact BOM is:

| Ref | Part | Notes |
|---|---|---|
| U1 | Raspberry Pi Pico | standard RP2040 Pico target |
| U2 | 74LVC245A | 20-pin, Apple-to-Pico data receiver, on 3V3 |
| U3 | 74LVC32 | 14-pin, one OR gate used, on 3V3 |
| R10, R11 | 10 kOhm | pull-ups to 3V3 on U3's `/DEVSEL` and `R/W` inputs |
| C1, C2 | 100 nF ceramic | one at each IC |
| D1 | 1N5817 or equivalent Schottky | Apple +5 V to Pico VSYS |
| C3 | 10-47 uF | local bulk decoupling |
| U4 | MAX98357A module, optional | I2S DAC and speaker amplifier |

Before laying out an edge card, verify connector finger numbering and board-side
orientation against an Apple IIe technical reference or a known-good template.
The required slot signals are D0-D7, `R/W`, `/DEVSEL`, +5 V, and ground. No
address, clock, ROM, interrupt, or DMA lines are required.

## Pico pin map

| Function | Pico GPIO |
|---|---:|
| D0-D7 from U2 | GP0-GP7 |
| active-low selected-write strobe from U3 | GP8 |
| I2S BCLK | GP20 |
| I2S LRCLK / WS | GP21 |
| I2S data | GP22 |
| PWM audio alternative | GP28 |
| 3.3 V supply for U2 and U3 | 3V3_OUT (pin 36) |

The bus PIO state machine uses PIO1. The I2S implementation uses PIO0, so the
two functions do not contend for a state machine. Firmware disables internal
pulls on GP0-GP7 and enables an internal pull-up only on GP8.

Keep everything else off GP0-GP7. U2 is permanently enabled and drives all eight
pins push-pull from the same 3.3 V rail, so any Pico function that turns one of
them into an output now fights a CMOS driver with nothing in series to limit it
— the resistors that used to hold such a contention under a milliamp are gone.
GP0/GP1 are the default UART0 pins, which is the realistic way to hit this, so
UART stdio is disabled deliberately and `main.c` fails the build if it is
re-enabled. Debug output goes over USB CDC.

## Audio choices

### Verified: PWM

This is the path that has actually run in an Apple II. Build with the
`pico-pwm-release` preset and take GP28 through a passive low-pass and AC
coupling network into an amplified input. GP28 cannot drive an 8 Ohm speaker
directly.

DECtalk here is an 11025 Hz stream, so there is nothing above 5.5 kHz in the
audio, while `pico_audio_pwm` carries a roughly 353 kHz 1-bit carrier
(48 MHz / 136 clocks per cycle). Filter well below the old 16 kHz suggestion
and you lose nothing while rejecting far more carrier:

```
GP28 --[1k]--+--[1k]--+--||--> amplifier input
             |        |   10uF
           22nF     22nF
             |        |
            GND      GND
```

Two poles near 7 kHz put the carrier down by more than 50 dB, against roughly
27 dB for a single 1 kOhm / 10 nF pole. Many amplifier boards already include
input coupling capacitors, in which case omit the 10 uF.

Filtering this hard matters most with a **class-D** amplifier such as a
PAM8403. Those switch at a few hundred kHz themselves, and an unfiltered
353 kHz residue arriving at the input intermodulates with that switching to
produce whine inside the audio band. A class-AB amplifier or a powered speaker
is far less sensitive to it.

### Better quality: one I2S module

Verified on hardware, and the better end state. The digital-to-
analog conversion happens inside a dedicated chip receiving clean digital data,
rather than in a passive filter referenced to an Apple II ground shared with
the whole machine's switching. The audible difference shows up mainly as hiss
during the inter-word gaps, of which DECtalk has many.

- GP20 -> BCLK
- GP21 -> LRC/WS
- GP22 -> DIN
- common ground
- module VIN -> `CARD_5V`, subject to that module's documented supply range

Two module styles, same three wires and same firmware:

- **MAX98357A**, a DAC and class-D amplifier in one, drives a speaker directly.
  Speaker only across the differential outputs; neither lead to ground. This is
  the choice if a self-contained card is the point.
- **PCM5102-style line-out DAC**, feeding a powered speaker. Negligible slot
  current, and cleaner than the amplifier module because no class-D output
  stage is involved.

Keep speaker power modest when drawing it from the Apple II slot; an amplifier
at volume can pull most of an amp from +5 V. This is acute on a II+, whose
4116 DRAM and Language Card already load a supply rated around 2.5 A. A
line-level output to an externally powered speaker is the low-risk option.

## Apple II address and BASIC protocol

For a card in slot `S` from 1 through 7, the selected I/O window begins at:

```
$C080 + 16*S
```

Standard Applesoft BASIC does not provide the PC-style `OUT port,value`
statement. Use `POKE address,value`. Applesoft represents these addresses as
signed 16-bit values:

| Slot | Hex address | Applesoft address |
|---:|---:|---:|
| 1 | `$C090` | `-16240` |
| 2 | `$C0A0` | `-16224` |
| 3 | `$C0B0` | `-16208` |
| 4 | `$C0C0` | `-16192` |
| 5 | `$C0D0` | `-16176` |
| 6 | `$C0E0` | `-16160` |
| 7 | `$C0F0` | `-16144` |

Send ordinary ASCII bytes, followed by carriage return (`13`) to synthesize the
utterance. Byte `144` (`$90`) stops current speech.

Example for slot 4:

```basic
10 DT=-16192
20 T$="HELLO FROM DECTALK"
30 FOR I=1 TO LEN(T$):POKE DT,ASC(MID$(T$,I,1)):NEXT
40 POKE DT,13
```

DECtalk inline commands are printable text, so a string such as
`"[:np]HELLO"` can be sent in exactly the same way.

### Inline command gotcha

DECtalkMini's command table (`include/c_us_cde.h`) lists voices **by name
only** — `np`, `nb`, `nc`, `nh`, `nf`, `nd`, `nk`, `nu`, `nr`, `nw`, `nv`. The
numeric form `[:n0]` that some other DECtalk implementations accept is
**rejected, and the rejection is spoken aloud** as a roughly 1.5 second error
message while the rest of the utterance still works. Use `[:np]` for Perfect
Paul.

This failure mode is worth knowing about generally: an unrecognised command
does not fail silently or visibly, it just adds speech. Comparing two renders
that both contain the error will not reveal it. Render with the native `say`
build and compare duration against a known-good baseline instead.

Abbreviated command forms are fine — `[:phone arpa speak on]` and
`[:rate 200]` both work. Everything is case-insensitive: dictionary lookup,
inline commands, and arpabet phoneme symbols alike, so Applesoft's uppercase
costs nothing.

## Demo programs

Two Applesoft programs, both verified on hardware. Sources live here; the
ProDOS image `DAISY.dsk` has them installed and boots straight to the `]`
prompt.

| Program | Source | Preview | What it demonstrates |
|---|---|---|---|
| `DAISY` | `apple2_daisy.bas` | `daisy-reference.wav` | Singing via phoneme mode |
| `ELIZA` | `apple2_eliza.bas` | `eliza-reference.wav` | Interactive conversation |

```basic
]RUN DAISY
]RUN ELIZA
```

`DAISY` sings Daisy Bell in about 23 seconds using
`[:phone arpa speak on]` with explicit `<duration,pitch>` on each phoneme. The
arrangement comes from the author's own `sing_daisy()` in
[Talker-80](https://github.com/lambdamikel/Talker-80). DECtalk note *n* maps to
MIDI note *n*+35, so n=10 is A2 at 110 Hz — useful if you want to transpose it.

`ELIZA` is a compact Weizenbaum-style therapist: 21 keywords with rotating
responses, pronoun reflection, and generic fallbacks, speaking every reply. It
reads input with `GET` rather than `INPUT`, because `INPUT` splits on commas
and prints `EXTRA IGNORED` the moment anyone types a comma. The `GET` loop also
handles backspace, folds lowercase to uppercase, and strips punctuation and
apostrophes — the last of which lets `I CAN'T` match the `I CANT` keyword
without a duplicate table entry.

Both `.wav` files are rendered from the native build, so you can hear what the
card should sound like without booting the Apple II.

### Editing the programs

`AppleCommander` handles the ProDOS image:

```
java -jar AppleCommander-ac.jar -l DAISY.dsk           # list
java -jar AppleCommander-ac.jar -e DAISY.dsk ELIZA     # export as text
java -jar AppleCommander-ac.jar -d DAISY.dsk ELIZA     # delete
java -jar AppleCommander-ac.jar -bas DAISY.dsk ELIZA < ELIZA.txt   # import
```

Its tokenizer respects quoted strings, so Applesoft keywords inside string
literals survive intact — which matters here, since `[:PHONE ARPA SPEAK ON]`
contains `ON`. When verifying a round-trip, compare **quoted literals only**:
the detokenized listing differs cosmetically from the input, printing `-16192`
as `- 16192` and collapsing `: ` to `:`.

## Build with the Pico SDK in Visual Studio Code

There is an important terminology distinction: Visual Studio Code can be the
IDE, but Microsoft MSVC/Visual C++ does not compile RP2040 firmware. CMake uses
the Arm GNU embedded compiler configured by the Pico SDK.

Install/configure the Pico SDK, Pico Extras, Arm GNU toolchain, CMake, Ninja,
and either the Raspberry Pi Pico VS Code extension or CMake Tools. Open
`platforms/pico-apple2` in Visual Studio Code and select a supplied preset.

PowerShell example:

```powershell
$env:PICO_SDK_PATH = 'C:\pico\pico-sdk'
$env:PICO_EXTRAS_PATH = 'C:\pico\pico-extras'
cd platforms\pico-apple2
cmake --preset pico-i2s-release
cmake --build --preset pico-i2s-release
```

Command Prompt example:

```bat
set PICO_SDK_PATH=C:\pico\pico-sdk
set PICO_EXTRAS_PATH=C:\pico\pico-extras
cd platforms\pico-apple2
cmake --preset pico-i2s-release
cmake --build --preset pico-i2s-release
```

Expected I2S firmware output:

```
build/i2s-release/dectalk_apple2.uf2
```

Each preset also builds a standalone self test beside the firmware:

```
build/i2s-release/dectalk_selftest.uf2
```

It speaks a fixed phrase loop by itself, with no Apple II and no slot traffic,
over the same audio backend as the firmware built next to it. Flash it first
when bringing up a new board: if the self test is silent, the fault is in the
audio path rather than in the bus interface. Note that the firmware itself is
silent on the bench, since it only speaks bytes arriving from the slot and
`DECTALK_SPEAK_STARTUP_BANNER` is on by default, so a working card announces
itself with "Perfect Paul Two ready." when the Apple II is switched on. Build
with `-DDECTALK_SPEAK_STARTUP_BANNER=OFF` for a silent boot. The wording is
`DECTALK_STARTUP_BANNER_TEXT` at the top of `main.c`; keep the trailing `\x0b`,
which is what tells DECtalk to speak the buffer.

That string spells "perfect" phonemically on purpose - do not simplify it back
to plain text. `perfect` has no entry in `dic/dtalk_us.dic`, so it falls through
to the letter-to-sound rules and comes out as the verb, per-FECT. `paul`,
`two` and `ready` do have entries, so phoneme mode is switched straight back off
after the one word that needs it.

The card has no reset line: slot pin 31 (`/RES`) is not wired, so the message
plays on power-up, not on Ctrl-Reset.

For PWM audio, use `pico-pwm-release` instead.

## Deploy

1. Power the Apple II off and remove the card for USB flashing.
2. Hold Pico `BOOTSEL` while connecting USB.
3. Copy `dectalk_apple2.uf2` to the mounted RP2 boot volume, or use `picotool`.
4. Disconnect USB, install the card, and power the Apple II.
5. The standard Pico LED turns on after DECtalk and audio initialization.
6. Run `apple2_dectalk.bas` after changing `DT` for the occupied slot.

USB CDC remains enabled for bench testing: type a line and press Enter; Ctrl-C
stops speech.

## Throughput and limitations

The receive-only design has no status register or hardware flow control. PIO
captures each selected write and core 0 drains an eight-word RX FIFO, which is
ample for Applesoft `POKE` traffic. A tightly optimized 6502 loop that writes a
continuous high-rate stream can overrun it. Adding readable status requires a
bidirectional data interface and read-cycle handling, increasing hardware.

All 16 device addresses are mirrors and there is no slot ROM. This is
intentional: it minimizes hardware and works directly from BASIC.

See `VALIDATION.md` for the verification boundary and `LICENSE-NOTE.md` before
redistributing source or UF2 binaries.
