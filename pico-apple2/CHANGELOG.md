# Perfect Paul ][ - changelog

## Revision 3b - demo software

- Added `DAISY`, which sings Daisy Bell through DECtalk's phoneme mode. The
  arrangement is the author's own `sing_daisy()` from Talker-80. Verified on
  hardware.
- Added `ELIZA`, a 21-keyword Weizenbaum-style therapist that speaks every
  reply. Verified on hardware. Its engine was validated as a Python
  transliteration before the Applesoft was emitted, since no Apple II emulator
  was available.
- Both installed on the ProDOS image, with sources (`apple2_daisy.bas`,
  `apple2_eliza.bas`) and natively rendered previews (`daisy-reference.wav`,
  `eliza-reference.wav`) kept alongside.
- Documented the `[:n0]` trap: DECtalkMini rejects the numeric voice form and
  **speaks** the error, so it is invisible to any comparison where both sides
  contain it. Use `[:np]`.
- Documented the note scale: DECtalk note *n* is MIDI note *n*+35.
- Confirmed arpabet phoneme symbols are case-insensitive, alongside dictionary
  lookup and inline commands, so Applesoft's uppercase is harmless.
- Freed the ProDOS image down to the essentials, which has the side effect of
  making `BASIC.SYSTEM` the first `.SYSTEM` file, so the disk now boots
  straight to the `]` prompt.

## Revision 3a - first working hardware

- **The PWM build works end to end in a real Apple II.** A `POKE` to the card's
  device window produces intelligible speech, first time, with no adjustment to
  timing, wiring, or firmware. Tested on a hand-wired prototyping board with the
  SOIC parts on DIP breakout adapters; no PCB yet.
- This closes the only question that could not be settled analytically: whether
  the 6502's data-hold time outlasts the decoder's `/DEVSEL` de-assert delay at
  the end of a write cycle. It does.
- Documented the audio filter actually used. DECtalk's 11025 Hz stream has no
  content above 5.5 kHz while `pico_audio_pwm` carries a ~353 kHz 1-bit
  carrier, so a two-pole filter near 7 kHz beats the previously suggested
  single pole at 16 kHz by more than 25 dB with no loss of speech. This matters
  most with a class-D amplifier, whose own switching intermodulates with any
  surviving carrier.
- Added SOIC package and pin-1 identification notes, including a diode test that
  doubles as a check that the parts are genuinely LVC rather than HC/HCT/LV.
- Added PCB layout notes.
- The I2S build remains untested on hardware.

## Revision 3 - 74LVC 3.3 V interface

- Replaced the 74LS interface with `74LVC245A` and `74LVC32`, both powered from
  the Pico's `3V3_OUT` rather than Apple +5 V.
- Deleted all nine series resistors. LVC inputs have no clamp diode to V_CC and
  are specified to 5.5 V independent of V_CC, so nothing needs limiting in
  either direction and there is no power-sequencing case to survive.
- Added R10/R11, 10 kOhm pull-ups to 3V3 on U3's `/DEVSEL` and `R/W` inputs.
  These are new and required: U3 is Pico-powered now, so it stays live with the
  Apple II off, and floating CMOS inputs would otherwise assert `/WRSEL`.
- Added `HARDWARE-74LVC.md`. Kept `HARDWARE-74LS.md` as the retired revision.
- Added a build-time guard in `main.c` against UART stdio on GP0/GP1. Without
  the series resistors, a Pico output on a bus pin is now an unlimited
  rail-to-rail fight with the permanently enabled '245.
- Changed `U2 DIR` from Apple +5 V to 3V3.
- No functional firmware change: the GPIO and PIO setup was already correct for
  a directly driven 3.3 V buffer. Comments and the READY banner were updated.

## Revision 2 - 74LS through-hole interface

- Replaced the original LVC hardware recommendation with `74LS245N` and
  `74LS32N`, powered directly from Apple +5 V.
- Added `HARDWARE-74LS.md` with exact DIP pin wiring.
- Made the resistor-network position explicit:
  `74LS245 B1-B8 -> isolated 8 x 4.7 kOhm series network -> Pico GP0-GP7`.
- Added a separate 4.7 kOhm series resistor from the `74LS32` output to GP8.
- Warned that a 9-pin commoned/bussed resistor pack is not suitable.
- Changed firmware GPIO setup: no pulls on GP0-GP7; internal pull-up on GP8.
- Updated PIO comments and USB readiness text for the 74LS interface.
- Retained the original PIO receive protocol, CMake presets, I2S/PWM choices,
  BASIC protocol, and license warning.
