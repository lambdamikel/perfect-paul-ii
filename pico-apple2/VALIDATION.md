# Perfect Paul ][ - validation status

Package revision: 74LVC interface revision 3a.

## Verified on hardware

**The PWM build works end to end in a real Apple II.** A `POKE` to the card's
device window produces intelligible speech. Working first time, with no
adjustment to timing, wiring, or firmware.

The tested card is a **hand-wired prototyping board** with the SOIC parts on
DIP breakout adapters. No PCB has been designed or fabricated yet.

That single result validates the whole chain:

- The 74LVC interface. 5 V Apple TTL into `74LVC245AD` and `74LVC32AD` powered
  from the Pico's `3V3_OUT`, with no series resistors anywhere, translating
  correctly in a live machine.
- The PIO capture protocol in `apple2_slot_rx.pio`. This was the one thing that
  could not be settled analytically: at the end of a write cycle the 6502's
  data-hold time (spec minimum 10 ns) races the decoder's `/DEVSEL` de-assert
  delay, so it was possible in principle for the last sample taken inside the
  low pulse to catch a released bus. It does not. Slot bus capacitance holds
  the byte well past the datasheet minimum, which is the same reason the
  conventional '374-latched-on-`/DEVSEL`-rising card works.
- Slot connector wiring, including the data pins running backwards (D7 on slot
  pin 42, D0 on slot pin 49).
- The core-0 receive / core-1 synthesis split under real bus traffic, and the
  `\r` to `\x0b` line protocol.
- `/WRSEL = /DEVSEL OR R/W` correctly ignoring read cycles to the same window.

Also verified separately, before the card existed:

- The PWM speech self test (`dectalk_selftest.uf2`) speaks its phrase loop at
  the correct pitch, exercising DECtalk synthesis, the `NO_FILESYSTEM` embedded
  dictionary, and the PWM audio path with no Apple II involved.

Audio for both was 1-bit PWM on GP28 through a passive low-pass into a PAM8403
class-D breakout.

### Software exercised on the card

Two Applesoft programs run correctly from ProDOS on the real machine, and
between them they cover the three ways the card gets used:

- **`DAISY`** sings Daisy Bell. This exercises DECtalk's phoneme/singing mode:
  `[:phone arpa speak on]` plus explicit `<duration,pitch>` on every phoneme,
  in five utterances totalling about 23 seconds. It confirms that inline
  bracketed commands survive the slot transport intact, which plain text does
  not prove — a single corrupted byte inside `[...]` would derail the parser.
- **`ELIZA`** is interactive. This is the stronger test: sustained back-and-forth
  across a whole session rather than one scripted burst, with variable-length
  replies, and an Applesoft `GET`-based input loop running alongside the `POKE`
  stream. It exercises the firmware's line queue and the core-0/core-1 handoff
  under realistic traffic.

Together with the earlier self test, the card is now exercised on plain text,
phoneme/singing mode, and interactive use.

Note the ProDOS image carries **two** identities: the working copy in this
directory and a copy on the Floppy Emu SD card. Deploying means updating both.

## Not yet verified on hardware

- **The I2S path.** `dectalk_apple2.uf2` from the `pico-i2s-release` preset has
  never been run. It is a different audio library, a different PIO instance, a
  different set of pins, and a different system clock (125 MHz rather than the
  PWM build's 96 MHz). Nothing about the PWM result carries over to it except
  the bus interface, which is shared and now proven.
- **A PCB revision.** Everything below about signal integrity was reasoned from
  a hand-wired board and has not been re-checked in copper.
- Long-run stability, thermal behaviour, and slot current under sustained use.
- Logic-analyzer timing capture of `/DEVSEL`, `R/W`, `/WRSEL`, and D0-D7. The
  interface demonstrably works, but the actual margin at the end of the write
  window has been inferred, not measured.
- Audio quality measurement, and slot current with a particular amplifier and
  speaker.
- Any machine other than the one tested. The five signals used are common to
  the II, II+, and IIe, so the design should be model-independent, but only one
  machine has run it.

## Performed in this environment

- Reworked the documented hardware around `74LVC245A` plus `74LVC32`, both
  powered from the Pico's `3V3_OUT`, with all series resistors deleted.
- Audited the GPIO and PIO setup against the 74LVC interface: pins are inputs,
  pulls disabled on GP0-GP7, pull-up retained on GP8, `wait 0 pin 8` resolves
  to GP8 via the IN base and `jmp pin` to absolute GP8. No functional change
  was required.
- Confirmed `LIB_PICO_STDIO_UART` is absent from the compiled definitions, so
  the GP0/GP1 contention guard reflects the build as configured, and verified
  the guard fires when UART stdio is forced on.
- Cross-built both CMake presets for RP2040 with the Arm GNU toolchain.
- Compared the adapter target with the supplied DECtalkMini Pico SDK target and
  retained its core-1 synthesis/audio arrangement.
- Reviewed the PIO program instruction by instruction. It uses one state
  machine and a joined eight-word RX FIFO.
- Confirmed against the native build that DECtalk's dictionary lookup, its
  inline `[:xx]` commands, and arpabet phoneme symbols are all case-insensitive,
  so Applesoft's uppercase-only text needs no special handling.
- Measured the singing pitch scale against the native build: DECtalk note *n*
  maps to MIDI note *n*+35, so n=10 is A2 at 110 Hz.
- Established that DECtalkMini **rejects the numeric voice form `[:n0]`** and
  speaks a ~1.5 second error while otherwise working normally. The command
  table in `include/c_us_cde.h` lists voices by name only (`np`, `nb`, `nh`,
  ...). Use `[:np]` for Perfect Paul. Abbreviated forms such as
  `[:phone arpa speak on]` and `[:rate 200]` are accepted.
- Verified every `DATA` string in both BASIC programs by reading it back out of
  the ProDOS image and rendering from the read-back copy, not from the local
  source.
- Validated the Eliza engine as a Python transliteration — keyword priority,
  pronoun reflection, response rotation — before emitting the Applesoft.

## Bring-up checklist for a rebuild

The reference card passed all of these. Re-run them on any new board, and in
particular on the first PCB:

1. U2 pin 20 and U3 pin 14 sit at approximately 3.3 V, not 5 V.
2. Pico VSYS, **not VBUS**, receives the diode-isolated `CARD_5V` supply.
3. U2 A-side pins swing to about 5 V and B-side pins to about 3.3 V. Equal
   swings on both sides mean the buffer is not translating.
4. GP8 is normally high and goes low only during a write to the card's `$C0nX`
   device window.
5. With the Apple II off and the Pico on USB, GP8 still reads high. A low here
   means R10/R11 are missing and the PIO is capturing noise.
6. GP0-GP7 match the byte written by the 6502 while GP8 is low.
