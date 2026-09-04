# Perfect Paul ][ - changelog

## Revision 3c - I2S working, spoken ready message

- **`dectalk_selftest` now follows `DECTALK_AUDIO_I2S` instead of always being
  built for PWM.** The `pico-i2s-release` preset previously emitted a
  `dectalk_selftest.uf2` containing no I2S code at all, so flashing it to a
  board wired for a MAX98357A produced silence by construction: it drove GP28
  as PWM and left BCLK, LRCLK and DIN idle. The directory name made the binary
  look like an I2S image when it was byte-for-byte the PWM one.
- Both targets now take their audio backend from a single
  `dectalk_configure_audio()` function in `CMakeLists.txt`, so the self test can
  no longer be built against a different backend than the firmware it vouches
  for.
- The I2S self test skips the PWM build's 96 MHz `set_sys_clock_khz()` and runs
  at the SDK default, matching the firmware, because `pico_audio_i2s` derives
  its own dividers from `clk_sys`.
- Documented the optional **reset button**: `RUN`, physical pin 30, momentary to
  GND. Also documents `BOOTSEL` + reset as the way to reach the bootloader
  without unplugging USB, and warns off pin 37 (`3V3_EN`) two pins away, which
  would take `U2` and `U3` down with the regulator.
- Collected the outstanding hardware gaps - missing `D1`, unset `R5`/`R6`, the
  `SW1` short, and `C3`'s polarity - into the PCB revision notes rather than
  leaving them scattered.
- **Retired the 74LS interface.** 74LVC is now the only supported build, and
  `HARDWARE-74LS.md` has been removed - it is still in git history. The
  comparative notes in the hardware reference are kept, since they are what
  explain why the current design has no series resistors anywhere.
- Consolidated `HARDWARE-74LVC.md` into the repository README and removed it.
  The bus interface, the audio stage and the PCB notes had been described in up
  to three places, and only some copies were being kept current.
- **The card now says "Perfect Paul Two ready." on power-up**, with "perfect"
  spelled phonemically as `[prrfihkt]`. The word is absent from
  `dic/dtalk_us.dic`, so it fell through to the letter-to-sound rules and was
  stressed as the verb, per-FECT - nearly every other English `-ect` word takes
  final stress. `paul`, `two` and `ready` are all in the dictionary and stayed
  as ordinary text. Confirmed by ear over the USB CDC console, which turned out
  to be the fastest way to audition candidate spellings without reflashing.
  `DECTALK_SPEAK_STARTUP_BANNER` now defaults to ON, and the placeholder text it
  had been carrying since it was written became the card's actual name. The
  wording lives in `DECTALK_STARTUP_BANNER_TEXT` in `main.c`. It is spoken after
  core 1 signals readiness, so the card is already accepting slot writes while
  it announces itself. Note this is a power-up message, not a reset message:
  slot pin 31 (`/RES`) is not wired to the Pico's RUN pin, so Ctrl-Reset does
  not re-trigger it.
- **The I2S build now works end to end in a real Apple II**, including the
  `DAISY` and `ELIZA` demos, through a MAX98357A on GP20/21/22. Audio quality is
  indistinguishable from the PWM build. This also clears the last open question
  about the bus, which had only ever been proven at the PWM build's 96 MHz and
  now runs at the I2S build's 125 MHz.
- The I2S self test passes on the same hardware, which is what made the firmware
  result diagnosable rather than a guess.
- Confirmed the refactor is behaviour-preserving by rebuilding the pre-change
  sources and comparing: the PWM self test is byte-identical, and both firmware
  images differ only in the 4 bytes of DECtalk's embedded `__DATE__` string.

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
