# Perfect Paul ][ - hardware reference (74LVC revision)

Hardware reference for the 3.3 V level-translating interface, and the only
interface this project supports. An earlier revision-2 design used 5 V 74LS
parts with series current-limiting resistors; it is retired and its document has
been removed. Comparisons to it below are kept because they explain why this
design has no series resistors, not because it remains an option. The old
reference is still in git history if you need it.

## Electrical approach

Everything on the card except the Apple bus itself runs at **3.3 V**, taken from
the Pico's `3V3_OUT` pin (physical pin 36):

- U2: `74LVC245A`, V_CC = Pico 3V3
- U3: `74LVC32`, V_CC = Pico 3V3

The LVC family is the right part for this job for one specific reason: its
inputs have **no clamp diode to V_CC**. The datasheet specifies the I/O pins to
5.5 V independent of V_CC, including V_CC = 0 (the `Ioff` / partial-power-down
specification). Feeding 5 V Apple TTL into a 3.3 V-powered LVC input is
therefore the manufacturer-supported case, not a tolerated abuse — unlike 74HC
or 74LV, whose input clamp diodes would conduct into the 3.3 V rail.

Consequences worth stating explicitly, because they are what changed from the
74LS revision:

- **No series resistors anywhere.** The '245's B outputs are push-pull CMOS on
  the same 3.3 V rail as the Pico's inputs. There is no overvoltage to limit and
  nothing to protect against.
- **No power-sequencing hazard.** The 5 V tolerance holds at V_CC = 0, so it
  does not matter whether the Apple's +5 V or the Pico's 3.3 V comes up first.
  The 74LS design needed the resistors largely to survive exactly that case.
- **Lighter load on the Apple data bus.** LVC input current is a few
  microamps per line against the 74LS245's 0.2 mA I_IL — a real improvement on a
  machine with several cards sharing the bus.
- **Faster and cleaner edges.** 4.7 kOhm into ~15 pF of pad and trace
  capacitance cost roughly 70 ns of rise time inside a `/DEVSEL` window only
  about 500 ns wide. Direct LVC drive replaces that with ~5 ns of propagation
  delay.

Voltage thresholds line up without any translation trick. LVC at V_CC = 3.0-3.6 V
specifies V_IH = 2.0 V and V_IL = 0.8 V, so Apple TTL levels (>=2.4 V high,
<=0.5 V low) sit comfortably inside them.

## Signal flow

```
Apple II data bus              3.3 V translating buffer            Pico

D0 --------------------------> U2 A1   U2 B1 --------------------> GP0
D1 --------------------------> U2 A2   U2 B2 --------------------> GP1
D2 --------------------------> U2 A3   U2 B3 --------------------> GP2
D3 --------------------------> U2 A4   U2 B4 --------------------> GP3
D4 --------------------------> U2 A5   U2 B5 --------------------> GP4
D5 --------------------------> U2 A6   U2 B6 --------------------> GP5
D6 --------------------------> U2 A7   U2 B7 --------------------> GP6
D7 --------------------------> U2 A8   U2 B8 --------------------> GP7

Apple /DEVSEL --+-- R10 10k --> 3V3
                 \
                  gate 1 of U3 74LVC32 OR -------------------------> GP8
                 /
Apple R/W ------+-- R11 10k --> 3V3

/WRSEL = /DEVSEL OR R/W
```

`/WRSEL` is low only for a write to the selected slot's `$C0n0-$C0nF` device
window. The Pico only receives bus signals and never drives the Apple II data
bus.

## The two pull-ups are not optional

R10 and R11 are the one genuinely new part of this revision, and skipping them
produces a confusing failure.

In the 74LS design U3 was powered from the Apple's +5 V, so with the Apple off
the gate was dead, its output was floating, and the Pico's internal pull-up on
GP8 held the strobe inactive. That is no longer how it works. **U3 now runs from
the Pico's 3.3 V rail, so it is live whenever the Pico is** — including on the
bench over USB, and including with the card installed while the Apple II is
switched off. In that state `/DEVSEL` and `R/W` float, two floating LVC inputs
can easily settle low, and `/WRSEL` asserts on its own. The PIO then captures
garbage continuously and the synthesiser speaks noise.

10 kOhm to 3V3 on each gate input fixes it. When the Apple is on it drives those
lines hard and 10 kOhm is invisible; when it is off they float high, so
`/WRSEL` reads idle.

The residual concern is that with the card installed and the Apple II off, these
pull-ups put 3.3 V onto two dead bus lines through 10 kOhm — bounded at roughly
0.66 mA total. That only arises if the Pico is separately USB-powered in an
otherwise dead machine, which is the configuration the deployment instructions
already tell you to avoid.

## U2: 74LVC245A pin-by-pin wiring

Standard 20-pin pinout, identical to the 74LS245 it replaces. Verify against the
exact part datasheet before layout.

| U2 pin | Name | Connect to |
|---:|---|---|
| 1 | DIR | **Pico 3V3**, not Apple +5 V; selects A-to-B direction |
| 2 | A1 | Apple D0 |
| 3 | A2 | Apple D1 |
| 4 | A3 | Apple D2 |
| 5 | A4 | Apple D3 |
| 6 | A5 | Apple D4 |
| 7 | A6 | Apple D5 |
| 8 | A7 | Apple D6 |
| 9 | A8 | Apple D7 |
| 10 | GND | Common ground |
| 11 | B8 | Pico GP7 |
| 12 | B7 | Pico GP6 |
| 13 | B6 | Pico GP5 |
| 14 | B5 | Pico GP4 |
| 15 | B4 | Pico GP3 |
| 16 | B3 | Pico GP2 |
| 17 | B2 | Pico GP1 |
| 18 | B1 | Pico GP0 |
| 19 | /OE | Ground; U2 is permanently enabled |
| 20 | VCC | **Pico 3V3** (pin 36) |

Place C1, 100 nF ceramic, directly between U2 pins 20 and 10.

DIR must go to the 3.3 V rail. LVC would tolerate 5 V on it, but tying a control
input to a rail the part is not powered from is pointless and confuses anyone
reading the board later.

Keeping `/OE` low is intentional. U2 always observes the Apple data bus, but its
direction is permanently Apple-to-Pico, so its A side is always an input and it
cannot drive the Apple bus. The PIO captures data only while `/WRSEL` is low.

## U3: 74LVC32 pin-by-pin wiring

The `74LVC32` uses the standard 14-pin `'32` pinout, identical to the `74LS32N`
it replaces, so gate 1 is wired exactly as before. Only two things change: V_CC
moves from Apple +5 V to the Pico's 3V3, and R9 (the 4.7 kOhm series resistor
between the gate output and GP8) is deleted.

| U3 pin | Name | Connect to |
|---:|---|---|
| 1 | 1A | Apple `/DEVSEL`, and R10 10 kOhm to 3V3 |
| 2 | 1B | Apple `R/W`, and R11 10 kOhm to 3V3 |
| 3 | 1Y | Pico GP8, **directly — no series resistor** |
| 4, 5 | 2A, 2B | Ground |
| 6 | 2Y | No connection |
| 7 | GND | Common ground |
| 8 | 3Y | No connection |
| 9, 10 | 3A, 3B | Ground |
| 11 | 4Y | No connection |
| 12, 13 | 4A, 4B | Ground |
| 14 | VCC | **Pico 3V3** (pin 36), not Apple +5 V |

Place C2, 100 nF ceramic, directly between U3 pins 14 and 7.

Tying the unused inputs matters more here than it did with the 74LS32. A
floating LS input pulls itself high and merely wastes power; a floating CMOS
input can sit near mid-rail, drawing crowbar current through both output
transistors and oscillating. Ground all six unused inputs as tabulated. Ground
rather than 3V3 is an arbitrary but consistent choice — it leaves the three
unused outputs low, and they go nowhere.

### The one thing that would damage the Pico

Do not swap the '245 for LVC, delete the resistors, and leave U3 as a
**5 V-powered 74LS32 wired straight to GP8**. That was the one connection in the
74LS design where the series resistor was doing real work, and GP8 is not 5 V
tolerant. Either move U3 to 3.3 V as above, or — if you ever revert to a 5 V
gate — put R9 back.

## Packages, and finding pin 1

The reference card was built with `74LVC245AD` and `74LVC32AD`. The `D` suffix
is SOIC, at 1.27 mm pitch, so on a 0.1 inch prototyping board both need
SOIC-to-DIP breakout adapters. **The two are different widths** — the SO14 '32
is narrow body at about 3.9 mm, while the SO20 '245 is wide body at about
7.5 mm. Measure the plastic body excluding the leads before ordering adapters.
Pin numbering is identical to DIP in every standard package, so nothing in the
wiring tables changes.

SOIC parts usually have no notch. Three ways to orient them, in increasing
order of certainty:

1. **Text orientation.** Hold the part so the marking reads normally. Pin 1 is
   then at the lower left, and numbering runs counterclockwise.
2. **A molded dimple** at the pin 1 corner, often invisible head-on. Tilt it
   under a light at a shallow angle.
3. **Diode test.** Every pin has an ESD clamp diode to GND with its anode on
   the GND rail, so the red probe on GND reads about 0.65 V forward against
   every other pin. No other pin behaves that way. That is pin 7 on the '32 and
   pin 10 on the '245, both corner pins, which fixes the whole numbering.

Method 3 doubles as a counterfeit check. Reverse the probes — red on an input,
black on VCC — and a genuine LVC part reads **open**, because LVC deliberately
omits the upper clamp diode to VCC. That missing diode is the 5 V tolerance
this entire design depends on. A reading near 0.6 V there means you have an HC,
HCT, or LV part, which would inject Apple +5 V straight into the 3.3 V rail.
Check both chips; it takes half a minute.

Orient by the text rule, solder to the adapter, then diode-test on the adapter's
DIP pins, which are far easier to probe than 1.27 mm leads. Only then apply
power: a SOIC soldered 180 degrees out puts VCC on the GND pin and the part
usually dies the instant it is powered.

## Pico connections

These are the only Pico pins the card uses. Everything else is left unconnected.

| Function | Pico GPIO | Pico physical pin | From / to |
|---|---:|---:|---|
| D0 | GP0 | 1 | U2 pin 18 (B1) |
| D1 | GP1 | 2 | U2 pin 17 (B2) |
| D2 | GP2 | 4 | U2 pin 16 (B3) |
| D3 | GP3 | 5 | U2 pin 15 (B4) |
| D4 | GP4 | 6 | U2 pin 14 (B5) |
| D5 | GP5 | 7 | U2 pin 13 (B6) |
| D6 | GP6 | 9 | U2 pin 12 (B7) |
| D7 | GP7 | 10 | U2 pin 11 (B8) |
| `/WRSEL` | GP8 | 11 | U3 pin 3 (1Y) |
| I2S BCLK | GP20 | 26 | MAX98357A BCLK |
| I2S LRCLK / WS | GP21 | 27 | MAX98357A LRC |
| I2S data | GP22 | 29 | MAX98357A DIN |
| PWM audio alternative | GP28 | 34 | RC low-pass to line out |
| 3.3 V supply out | 3V3_OUT | 36 | U2 pins 1 and 20, U3 pin 14, R10, R11 |
| Card power in | VSYS | 39 | `CARD_5V`, the cathode of D1 |
| Reset button | RUN | 30 | Momentary switch to GND, optional |
| Bus signal ground | GND | 3, 8, 13 | Common ground |
| Power / audio ground | GND | 28, 38 | Common ground |

GP25 is the on-board LED the firmware uses as a ready indicator. It is not
brought out to a header pin and needs no wiring.

Firmware disables pulls on GP0-GP7 and enables the internal pull-up only on GP8.

### Reset button

`RUN` is **physical pin 30**. Ground it momentarily to reset the RP2040; release
and the card reboots and speaks its ready message again. That is the whole
circuit:

```
Pico RUN (pin 30) ----o  o---- GND
                    momentary
```

`RUN` has an internal pull-up of about 50 kOhm, so no external pull-up is
needed. A 100 nF capacitor from `RUN` to GND is the usual optional addition; it
debounces the switch and is worth having on a board that lives inside a machine
full of switching noise.

Two things make this worth fitting:

- **`BOOTSEL` + reset replaces unplugging USB.** Hold `BOOTSEL`, tap reset,
  release `BOOTSEL`, and the card enumerates as the bootloader drive. On a card
  that gets reflashed often, that is the difference between a two-second
  operation and pulling it out of the slot.
- The Apple II's own reset does **not** reach the card. Slot pin 31 (`/RES`) is
  not wired to anything, so Ctrl-Reset leaves the Pico running.

Do not confuse `RUN` with pin 37, `3V3_EN`, two pins away. Grounding **that**
disables the Pico's regulator and takes `U2` and `U3` down with it, because they
are powered from `3V3_OUT`.

If you ever do wire slot `/RES` to `RUN` through one of the spare `74LVC32`
gates, keep the button on the **gate's input side**. A button straight onto
`RUN` would otherwise short the gate's output to ground whenever it is driving
high.

### Pins to leave strictly alone

- **VBUS, pin 40.** This is USB +5 V. Wiring `CARD_5V` here instead of VSYS
  defeats D1 entirely and back-feeds the Apple II's +5 V rail from USB, which
  is the exact failure the diode exists to prevent. Card power goes to **pin
  39**, one pin away. Check this twice.
- **3V3_EN, pin 37.** Leave floating. Grounding it disables the Pico's
  regulator, which now also kills U2 and U3.
- **3V3_OUT, pin 36, is an output.** Never feed it from Apple +5 V.

### Use several ground pins, not one

Grounds 3, 8, and 13 sit among GP0-GP8 and are the natural return path for the
bus signals. Tie all three. With the series resistors gone, LVC edge rates are
what they are, and a single distant ground return is the easiest way to turn
short adapter wiring into visible ringing. Keep 28 and 38 for the audio module
and power.

### Do not put anything else on GP0-GP7

U2 is permanently enabled and drives all eight pins push-pull from the same
3.3 V rail as the Pico. Any Pico function that turns one of them into an output
now fights a CMOS driver rail-to-rail, with nothing in series to limit it. The
74LS revision's 4.7 kOhm resistors held that under a milliamp; deleting them
removes that safety net.

GP0 and GP1 are the RP2040's default UART0 TX/RX, which is the realistic way to
hit this. `CMakeLists.txt` therefore calls `pico_enable_stdio_uart(<target> 0)`
deliberately, and `main.c` carries a preprocessor guard that fails the build if
UART stdio is ever re-enabled while the bus occupies those pins. Debug output
goes over USB CDC.

## Power wiring

```
Apple +5 V ---->|---- CARD_5V -----------> Pico VSYS pin 39
 (slot pin 25) D1                          MAX98357A VIN, if used
               anode -> cathode            PAM8403 5V, if used

Pico 3V3_OUT pin 36 --------------------> U2 pin 20
                                          U3 VCC
                                          R10, R11 pull-ups

Apple GND ------------------------------> U2 GND
                                          U3 GND
                                          Pico GND
                                          audio-module GND
```

Note that the Apple's +5 V no longer reaches any logic on the card. It supplies
only `CARD_5V` through D1, which powers the Pico via VSYS and the audio module
if fitted. Use a Schottky such as `1N5817`: anode to Apple +5 V, cathode to
`CARD_5V`. D1 prevents USB power applied to the Pico from feeding back into the
Apple II +5 V rail. Use a Schottky rather than a 1N400x: the forward drop lands
directly on the amplifier supply, and 0.4 V costs noticeably less output power
than 0.7 V does. Note this diode is **not** between two Pico pins - it is
between the slot and `VSYS`. The Pico pin to keep away from is `VBUS`, pin 40;
see "Pins to leave strictly alone" below.

`3V3_OUT` is a regulator output, and the load added here is trivial — LVC static
supply current is microamps, and switching eight channels plus a gate at Apple
bus rates stays well under a milliamp. It is nowhere near the Pico regulator's
budget.

**Do not connect Apple +5 V to `3V3_OUT`.** That warning survives unchanged from
the 74LS revision and matters more now that `3V3_OUT` is a distributed rail on
the card rather than an unused pin.

Place 10-47 uF bulk capacitance from `CARD_5V` to ground near the Pico and audio
module. Keep speaker current modest when it is drawn from the Apple slot.

## Bill of materials for the bus interface

| Ref | Quantity | Part | Notes |
|---|---:|---|---|
| U1 | 1 | Raspberry Pi Pico | RP2040 module |
| U2 | 1 | 74LVC245A | 20-pin, octal transceiver, powered from Pico 3V3 |
| U3 | 1 | 74LVC32 | 14-pin, one OR gate used, powered from Pico 3V3 |
| R10, R11 | 2 | 10 kOhm | Pull-ups to 3V3 on U3's `/DEVSEL` and `R/W` inputs |
| C1, C2 | 2 | 100 nF ceramic | One at each IC |
| D1 | 1 | 1N5817 or equivalent | Schottky isolation into Pico VSYS |
| C3 | 1 | 10-47 uF | Local bulk decoupling |

Deleted relative to the 74LS revision: RN1 (eight 4.7 kOhm series resistors), R9
(the strobe series resistor), and the 74LS245N/74LS32N themselves.

A MAX98357A-style I2S amplifier module and speaker are additional audio parts,
not part of the Apple bus interface itself.

## PWM audio output stage, as fitted on the prototype PCB

Designators in this section follow `perfectpaul2.net`, the fabricated board.
**They do not match the bus-interface BOM above**, which was written for the
hand-wired card: on the PCB the Pico is `A1`, the '245 is `U2`, the '32 is `U1`,
the two pull-ups are `R1`/`R2`, and the 3.3 V decoupling is `C4`/`C5`. `C1`,
`C2` and `C3` below are audio parts, not decoupling. Read the silkscreen, not
the older table.

```
GP28 --[R3 1k]--+--[R4 1k]--+--| |--+
                |           |   C3  |
             C1 22nF     C2 22nF   10uF        RV1 20k ---> PAM8403 L and R in
                |           |                   |
               GND         GND                 GND
```

| Ref | Value | Function |
|---|---|---|
| R3, R4 | 1 kOhm | Series elements of a two-pole RC low-pass |
| C1, C2 | 22 nF | Shunt legs. Each section corners at 1/(2*pi*1k*22n) = 7.2 kHz |
| C3 | 10 uF | DC block between the filter and the volume trimmer. **Fit 1 uF non-polarised film instead** - see below |
| RV1 | 20 kOhm | Volume trimmer, wiper to the PAM8403 input |

DECtalk here is an 11025 Hz stream with no content above 5.5 kHz, while
`pico_audio_pwm` carries a roughly 353 kHz 1-bit carrier, so there is a very
wide gap to filter in. The two sections are unbuffered and load each other, so
the real response is not a textbook 7.2 kHz two-pole, but the carrier still
lands on the order of 60 dB down. `RV1` at 20 kOhm is high enough against the
1 kOhm series elements that it does not disturb the filter much.

`C3` with `RV1` forms a high-pass at 1/(2*pi*10u*20k) = 0.8 Hz, far below the
speech band, so the 10 uF is doing DC blocking rather than shaping anything.

### C3's polarity, worked through

The `+` terminal of `C3` must face **the filter side (GP28)**, not the
potentiometer. Deriving it from the two DC potentials, since this is easy to get
backwards:

- **Filter side.** `C1` and `C2` are capacitors, so there is no DC path from
  that node to ground; `R3` and `R4` return it to GP28. The node therefore sits
  at the DC average of the PWM output, which at idle is 50% duty into a 3.3 V
  swing: **about 1.65 V**.
- **Pot side.** `RV1` pin 3 is grounded, so pin 1 has a DC path to ground
  through the pot element. `C3` blocks any DC arriving from the filter, so no
  current flows and there is no drop across the element: **0 V**.

1.65 V against 0 V, so `+` faces the filter. In `perfectpaul2.net` it is the
other way round - pad 2 (`-` on a KiCad `CP` footprint) is on the filter node -
which leaves the part **reverse biased by about 1.65 V**.

The rule of thumb that a coupling cap's `+` faces the amplifier input is a fair
one, but it assumes the *downstream* stage carries the bias, which is the usual
case when feeding a directly-coupled input sitting at mid-rail. Here it does
not: the pot shorts that end to ground, and the bias is entirely upstream.

### Better: do not use a polarised part here at all

The bias is only 1.65 V and the audio swings around it, so this is a marginal
application for an electrolytic even when oriented correctly. `C3` with `RV1`
gives a high-pass corner of 1/(2*pi*10u*20k) = 0.8 Hz, which is about four
octaves lower than anything needed - DECtalk has no content below roughly 80 Hz.

Dropping to **1 uF** puts the corner at 8 Hz, still far below the speech band,
and 1 uF is readily available as a non-polarised film part. That removes the
polarity question permanently rather than documenting it. Fit that in preference
to a correctly-oriented electrolytic.

## Selecting between the PAM and the MAX98357A outputs

**Both are bridge-tied-load class-D amplifiers.** On both parts, the `-` speaker
terminal is not ground: it is a second actively-switching output driven
antiphase to `+`. That has two consequences, and getting either wrong destroys
an output stage:

- **Never ground a speaker output**, on either module. It shorts a half-bridge.
- **Never tie the two modules' `-` outputs together.** They are two independent
  switching outputs. Commoning them parallels two active drivers.

So the answer to "can I common the negatives and switch only `+`" is **no**. The
module *power* grounds are already commoned through the card ground and must
stay that way - but the speaker `-` lines are a different signal entirely, and
share only a name.

Use a genuine two-pole switch, one pole per speaker lead:

```
MAX98357A OUT+ ---o
                      \
PAM8403    LOUT+ ---o   o--- speaker +      pole 1
 
MAX98357A OUT- ---o
                      \
PAM8403    LOUT- ---o   o--- speaker -      pole 2
```

Specify **break-before-make**, which ordinary toggle and slide switches are.
A make-before-break part would briefly connect both amplifiers' outputs together
during the transition, which is exactly the case above. A 2x3 pin header with
two shunts works equally well and is cheaper, at the cost of moving two jumpers
instead of one switch.

Both amplifiers idle with their outputs switching even with no input, so this
selection is real work rather than a convenience: it is not enough to rely on
only one backend being built into the firmware. Leaving the unused amplifier
powered into an open circuit is harmless.

`SD` on the MAX98357A (`J2` pin 5) is currently a single-node net and is
available if you would rather also mute the unused amplifier - it needs a third
pole, or a separate jumper.

## First power-up checks

## Notes for a PCB revision

A first prototype PCB has been fabricated and works, with the SOIC parts
soldered directly rather than on the breakout adapters the hand-wired card used.
The notes below were written against that hand-wired card and are kept for the
next revision - a few things are worth designing in rather than discovering:

- **Fit the reset button.** `RUN` (pin 30) to GND through a momentary switch,
  optionally with 100 nF across it. See the Reset button section above. It is
  absent from the first prototype, where reflashing means unplugging USB.
- **Add `D1`.** It is specified in the power section but absent from
  `perfectpaul2.net`, so Apple +5 V currently reaches `VSYS` undioded.
- **Give `R5`/`R6` real values.** They are unset in the netlist; the MAX98357A
  gain table wants 100 kOhm for the through-resistor positions.
- **Make `SW1` incapable of shorting the rail.** See the section below. Note a
  **4-position** DIP is sufficient for all five gain settings: position 3 in the
  current design ties `GAIN` to an unconnected pin, which is identical to
  leaving every switch open. Dropping it costs nothing and all-open becomes the
  9 dB floating default.
- **Select the speaker between the two amplifiers with a two-pole switch.** See
  the section on that below - the `-` outputs must be switched too.
- **Replace `C3` with 1 uF non-polarised film.** As netlisted the 10 uF
  electrolytic sits reverse biased by about 1.65 V, because the bias in this
  circuit is upstream rather than downstream. 1 uF still gives an 8 Hz corner
  into `RV1`, four octaves below anything DECtalk produces, and a non-polarised
  part removes the question permanently rather than documenting it. See the
  audio output stage section above for the derivation.
- **Fit series termination footprints on all nine bus signals** and populate
  them with 0 Ohm links. LVC switches in 1-2 ns, and reflections start to
  matter beyond roughly 5 cm of unterminated trace. On a compact card you will
  probably never need them, but a footprint costs nothing at layout time and a
  respin costs a fortnight. 33-100 Ohm if they ever turn out to be needed. Do
  not confuse these with the retired 74LS revision's 4.7 kOhm current limiters;
  these are impedance matching and must stay small.
- **Use a ground plane.** It replaces the hand-wired advice about tying Pico
  grounds 3, 8 and 13 individually, and gives every bus signal a short return
  directly under its trace.
- **Keep U2 close to the Pico's GP0-GP7 pins** and U3's output close to GP8.
  The strobe is the signal where ringing would do real damage, since a double
  edge there means a duplicated byte.
- **Place C1 and C2 hard against the IC supply pins**, which is the one thing
  breakout adapters make difficult.
- **Route the PWM audio filter away from the bus signals**, and give it its own
  ground return to the audio connector. Its output is analog and referenced to
  a ground shared with the whole machine's switching.
- **Silkscreen the VSYS pin.** Card power goes to Pico pin 39, and pin 40 is
  VBUS. Labelling it on the board is cheaper than the failure.
- Verify the edge connector finger numbering against an Apple II reference
  before fabrication, remembering that the data pins run backwards: D7 on slot
  pin 42 through D0 on slot pin 49.

## The SW1 gain selector can short the 5 V rail

`SW1` is a 5-position DIP where each position ties the MAX98357A's `GAIN` pin to
a different place. In KiCad's `SW_DIP_x05`, position *N* bridges pin *N* to pin
*11-N*, which gives:

| Position | Connects `GAIN` to | MAX98357A gain |
|---:|---|---|
| 1 | GND via `R5` | 15 dB |
| 2 | GND directly | 12 dB |
| 3 | nothing (floating) | 9 dB |
| 4 | 5 V directly | 6 dB |
| 5 | 5 V via `R6` | 3 dB |

Exactly one is meant to be closed. **Closing 2 and 4 together is a dead short
from 5 V to ground** through two switch contacts, with the Apple II's supply
behind it: enough to weld the contacts or lift a trace. Today only the
silkscreen prevents it.

Combinations involving `R5` or `R6` are harmless, because those legs are
current-limited by 100 kOhm. 2+4 is the only fatal pair.

### Why a diode does not fix it

A diode blocks *reverse* current, and this fault is forward. In series with the
5 V leg it would conduct straight through the short: 5 V, anode, cathode,
`GAIN`, position 2, ground, at whatever current the rail can deliver less a
0.7 V drop. It would also pull the "tied to V_DD" level down by that 0.7 V and
blur the very threshold the gain detection depends on. Wrong tool.

### One resistor fixes it

Put **1 kOhm in series with either of the two direct legs** - between 5 V and
`SW1` pin 7, or between `SW1` pin 9 and ground. Either one breaks the metallic
path, so only one is needed:

```
5V ---[1k]--- SW1 pin 7   (position 4, "direct" to VDD)
```

Worst case becomes 5 V / 1 kOhm = 5 mA, which nothing notices.

**Does it move the gain setting?** It has to be checked rather than assumed,
because the resistor sits directly in the network the part senses. `GAIN` cannot
be a plain logic input - it has to tell five states apart, one of which is
*floating* - so it must have an internal pull-up and pull-down to V_DD and GND,
equal, putting a floating pin at mid-rail. Call each one R_int. The five states
then sit at 0, a third, a half, two thirds and 1 x V_DD, and that spacing only
works if R_int is the same order as the external 100 kOhm: at 10 kOhm the
100 kOhm setting would land at 0.476 V_DD against floating's 0.5 and be
indistinguishable. So R_int is somewhere around 30-300 kOhm.

A 1 kOhm series resistor parallels the internal pull-up, giving:

| R_int | `GAIN` voltage with 1 kOhm fitted |
|---|---|
| 100 kOhm | 0.990 x V_DD |
| 30 kOhm (worst plausible) | 0.969 x V_DD |

against a threshold that must fall near 0.83 x V_DD, between the two-thirds and
full-scale states. So the setting still reads as "direct" - not because 1 kOhm
is a small fraction of 100 kOhm, but because the margin to the neighbouring
state is about 15% of V_DD and the resistor consumes 1-3% of it.

The value matters, and its error pushes toward the adjacent state:

| Series R | Fault current | `GAIN` voltage | Verdict |
|---|---|---|---|
| 100 Ohm | 50 mA, 250 mW | 0.999 x V_DD | Gain untouched, but needs a 0.5 W part |
| 1 kOhm | 5 mA, 25 mW | 0.990 x V_DD | Recommended |
| 10 kOhm | 0.5 mA | 0.917 x V_DD | Reads correctly, half the margin spent |
| 100 kOhm | - | 0.667 x V_DD | This *is* the 3 dB setting |

This is reasoned from how the pin must work, not read off the datasheet. Confirm
against the MAX98357A gain table, and verify empirically: fit the resistor,
select position 4, and check the loudness is unchanged from before it was
fitted. If in doubt, use the selector below and the question disappears.

### The structural fix

Better still, make the bad state unreachable. `GAIN` needs one of five mutually
exclusive states, which is a **selector**, not five independent switches. A
6-pin SIL header with a single jumper shunt, or a 1-pole 5-throw rotary, cannot
express "two at once" at all, and costs less board area than the DIP. That
removes the failure mode rather than current-limiting it, and is the
recommended change for the next revision.

## First power-up checks

The reference card passed all of these. Re-run them on any new build, and in
particular on the first PCB.

1. With the Apple II on and the card installed, verify U2 pin 20 and U3 VCC sit
   at approximately 3.3 V, not 5 V. A reading near 5 V means the supply is
   miswired and the parts are out of specification.
2. Verify `CARD_5V` at Pico VSYS is approximately one Schottky drop below the
   Apple +5 V rail.
3. Verify U2 A-side pins swing to about 5 V and B-side pins to about 3.3 V.
   Equal swings on both sides mean the buffer is not translating.
4. Confirm GP8 is normally high and pulses low only during a `POKE` to the
   card's slot device address.
5. With the Apple II powered off and the Pico on USB, confirm GP8 still reads
   high. If it sits low, R10/R11 are missing or misconnected and the PIO is
   capturing noise.
6. Confirm GP0-GP7 match the written byte while GP8 is low, before connecting
   the speaker.
