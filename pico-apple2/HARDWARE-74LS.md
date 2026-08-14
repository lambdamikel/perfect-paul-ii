# Apple IIe to Raspberry Pi Pico hardware (74LS version)

This is the hardware reference for the through-hole, low-component-count
interface used by the firmware in this directory.

## Electrical approach

The Apple II side uses ordinary 5 V TTL:

- U2: `74LS245N`, powered from Apple II +5 V
- U3: `74LS32N`, powered from Apple II +5 V

The RP2040 is not specified as 5 V tolerant. This design deliberately follows
the requested minimal-hardware approach: the 74LS outputs feed the Pico only
through 4.7 kOhm series resistors. Those resistors limit clamp/injection current
if an LS output rises above the Pico I/O rail or if the two power rails sequence
differently. This is a pragmatic prototype interface, not a manufacturer-
guaranteed 5 V level translator.

## Signal flow

```
Apple II data bus                 5 V TTL buffer          series protection       Pico

D0 ----------------------------> U2 A1   U2 B1 ---------> 4.7k -----------------> GP0
D1 ----------------------------> U2 A2   U2 B2 ---------> 4.7k -----------------> GP1
D2 ----------------------------> U2 A3   U2 B3 ---------> 4.7k -----------------> GP2
D3 ----------------------------> U2 A4   U2 B4 ---------> 4.7k -----------------> GP3
D4 ----------------------------> U2 A5   U2 B5 ---------> 4.7k -----------------> GP4
D5 ----------------------------> U2 A6   U2 B6 ---------> 4.7k -----------------> GP5
D6 ----------------------------> U2 A7   U2 B7 ---------> 4.7k -----------------> GP6
D7 ----------------------------> U2 A8   U2 B8 ---------> 4.7k -----------------> GP7

Apple /DEVSEL ----\
                   OR in U3 74LS32 ----> 4.7k -------------------------------> GP8
Apple R/W --------/

/WRSEL = /DEVSEL OR R/W
```

`/WRSEL` is low only for a write to the selected slot's `$C0n0-$C0nF`
device window. The Pico only receives bus signals and never drives the Apple II
data bus.

## Exactly where the 8 x 4.7 kOhm array goes

RN1 is placed **after the 74LS245 B outputs and before the Pico data inputs**:

```
U2 B1 -- RN1 resistor 1 -- GP0
U2 B2 -- RN1 resistor 2 -- GP1
U2 B3 -- RN1 resistor 3 -- GP2
U2 B4 -- RN1 resistor 4 -- GP3
U2 B5 -- RN1 resistor 5 -- GP4
U2 B6 -- RN1 resistor 6 -- GP5
U2 B7 -- RN1 resistor 7 -- GP6
U2 B8 -- RN1 resistor 8 -- GP7
```

Use an **isolated** resistor network containing eight independent 4.7 kOhm
resistors, normally a 16-pin DIP/SIP-style network, or simply use eight
individual 4.7 kOhm resistors. There is no connection from RN1 to +5 V, 3.3 V,
or ground.

Do **not** use a commoned or bussed 9-pin pull-up network. In that type, all
resistors share one pin; it would connect the eight data lines together.
Consult the resistor-network datasheet for its exact internal pin pairing.

R9 is a ninth, separate 4.7 kOhm series resistor:

```
U3 pin 3 (1Y, /WRSEL) -- R9 4.7k -- Pico GP8
```

The 4.7 kOhm parts are series current limiters, not voltage dividers. The Pico
inputs are high impedance, so they do not materially reduce normal TTL high
voltage. At Apple II bus speed, the resulting input rise time remains suitable
for a short PCB trace and normal Pico input capacitance.

## U2: 74LS245N pin-by-pin wiring

The table uses the standard 20-pin DIP `74LS245` pinout. Verify the exact part
datasheet before layout.

| U2 pin | Name | Connect to |
|---:|---|---|
| 1 | DIR | Apple +5 V; selects A-to-B direction |
| 2 | A1 | Apple D0 |
| 3 | A2 | Apple D1 |
| 4 | A3 | Apple D2 |
| 5 | A4 | Apple D3 |
| 6 | A5 | Apple D4 |
| 7 | A6 | Apple D5 |
| 8 | A7 | Apple D6 |
| 9 | A8 | Apple D7 |
| 10 | GND | Common ground |
| 11 | B8 | RN1 channel 8, then Pico GP7 |
| 12 | B7 | RN1 channel 7, then Pico GP6 |
| 13 | B6 | RN1 channel 6, then Pico GP5 |
| 14 | B5 | RN1 channel 5, then Pico GP4 |
| 15 | B4 | RN1 channel 4, then Pico GP3 |
| 16 | B3 | RN1 channel 3, then Pico GP2 |
| 17 | B2 | RN1 channel 2, then Pico GP1 |
| 18 | B1 | RN1 channel 1, then Pico GP0 |
| 19 | /OE | Ground; U2 is permanently enabled |
| 20 | VCC | Apple +5 V |

Place C1, 100 nF ceramic, directly between U2 pins 20 and 10.

Keeping `/OE` low is intentional. U2 always observes the Apple data bus but its
direction is permanently Apple-to-Pico, so it cannot drive the Apple bus. The
PIO captures data only while `/WRSEL` is low.

## U3: 74LS32N pin-by-pin wiring

Use gate 1 of the standard 14-pin DIP `74LS32`:

| U3 pin | Name | Connect to |
|---:|---|---|
| 1 | 1A | Apple `/DEVSEL` |
| 2 | 1B | Apple `R/W` |
| 3 | 1Y | R9 4.7 kOhm, then Pico GP8 |
| 4, 5 | 2A, 2B | Ground |
| 6 | 2Y | No connection |
| 7 | GND | Common ground |
| 8 | 3Y | No connection |
| 9, 10 | 3A, 3B | Ground |
| 11 | 4Y | No connection |
| 12, 13 | 4A, 4B | Ground |
| 14 | VCC | Apple +5 V |

Place C2, 100 nF ceramic, directly between U3 pins 14 and 7. Do not leave
unused LS inputs floating.

## Pico connections

| Function | Pico GPIO | Pico physical pin |
|---|---:|---:|
| D0 after RN1 | GP0 | 1 |
| D1 after RN1 | GP1 | 2 |
| D2 after RN1 | GP2 | 4 |
| D3 after RN1 | GP3 | 5 |
| D4 after RN1 | GP4 | 6 |
| D5 after RN1 | GP5 | 7 |
| D6 after RN1 | GP6 | 9 |
| D7 after RN1 | GP7 | 10 |
| `/WRSEL` after R9 | GP8 | 11 |
| I2S BCLK | GP20 | 26 |
| I2S LRCLK / WS | GP21 | 27 |
| I2S data | GP22 | 29 |
| PWM audio alternative | GP28 | 34 |
| Pico power input | VSYS | 39 |
| Ground | GND | 3, 8, 13, 18, 23, 28, or 38 |

Firmware disables pulls on GP0-GP7. It enables only the Pico's internal pull-up
on GP8, keeping the active-low write strobe inactive when U3 is unpowered during
USB bench testing.

## Power wiring

```
Apple +5 V ---------------------------> U2 pin 20
        |                              U3 pin 14
        |
        +---->|---- CARD_5V ----------> Pico VSYS pin 39
             D1                       MAX98357A VIN, if used

Apple GND ----------------------------> U2 pin 10
                                       U3 pin 7
                                       Pico GND
                                       audio-module GND
```

Use a through-hole Schottky such as `1N5817`, or an equivalent part. D1 anode
goes to Apple +5 V and D1 cathode goes to `CARD_5V`/Pico VSYS. D1 prevents USB
power applied to the Pico from feeding back into the Apple II +5 V rail.

Do not put D1 in the U2/U3 supply path: ordinary 74LS logic should receive the
Apple's full +5 V. Do not connect Apple +5 V to Pico pin 36 (`3V3_OUT`).

Place 10-47 uF bulk capacitance from `CARD_5V` to ground near the Pico/audio
module. Keep speaker current modest when it is drawn from the Apple slot.

## Bill of materials for the bus interface

| Ref | Quantity | Part | Notes |
|---|---:|---|---|
| U1 | 1 | Raspberry Pi Pico | RP2040 module |
| U2 | 1 | 74LS245N | 20-pin DIP, powered from Apple +5 V |
| U3 | 1 | 74LS32N | 14-pin DIP, powered from Apple +5 V |
| RN1 | 1 | 8 x 4.7 kOhm isolated network | Eight independent series resistors; not bussed |
| R9 | 1 | 4.7 kOhm | Series resistor from U3 pin 3 to GP8 |
| C1, C2 | 2 | 100 nF ceramic | One at each LS IC |
| D1 | 1 | 1N5817 or equivalent | Schottky isolation into Pico VSYS |
| C3 | 1 | 10-47 uF | Local bulk decoupling |

A MAX98357A-style I2S amplifier module and speaker are additional audio parts,
not part of the Apple bus interface itself.

## First power-up checks

1. With the Pico unplugged, verify U2 pin 20 and U3 pin 14 are approximately
   +5 V relative to ground when the Apple is on.
2. Verify `CARD_5V` at Pico VSYS is approximately one Schottky drop below the
   Apple +5 V rail.
3. Before inserting the Pico, verify U2 B1-B8 and U3 pin 3 are not shorted to
   one another, +5 V, or ground through RN1/R9.
4. With the Pico fitted, confirm GP8 is normally high and pulses low only during
   a `POKE` to the card's slot device address.
5. Confirm one or more data lines at GP0-GP7 match the written byte before
   connecting the speaker.
