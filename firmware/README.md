# Firmware

Ready-to-flash images for the **SP0256-AL2** firmware. Hold `BOOTSEL`, connect
USB, copy the file to the RP2 volume.

| file | |
|---|---|
| `perfectpaul2-sp0256-i2s.uf2` | the card firmware, I2S audio (GP20/21/22) |
| `perfectpaul2-sp0256-pwm.uf2` | the card firmware, PWM audio (GP28) |
| `perfectpaul2-sp0256-selftest-i2s.uf2` | self test, I2S — needs no Apple II |
| `perfectpaul2-sp0256-selftest-pwm.uf2` | self test, PWM |

**Pick the one matching the audio stage you built.** An image for the other
backend is silent by construction and tells you nothing.

On a new board, flash a **self test** first and listen: it speaks a script in a
loop with no Apple II, no 74LVC245 and no slot traffic, so if it is silent the
fault is in the audio path rather than the bus interface.

## Why these can be here at all, and why there is no DECtalk image

These are built from [`../pico-apple2/sp0256/`](../pico-apple2/sp0256/), which
is **GPL-3 throughout**. Distributing a GPL-3 binary obliges the distributor to
provide the complete corresponding source, and that source is in this
repository — so these images are distributable.

**The DECtalk firmware is not, and never will be here.** That core carries
affirmative proprietary notices from DEC, Fonix and Force, so no binary
containing it can be published. The repository's `.gitignore` ignores `*.uf2`
blanket for exactly that reason; the exception that admits these files is
written narrowly enough that it cannot match a DECtalk build. See
[`../LICENSE-NOTE.md`](../LICENSE-NOTE.md).

## Provenance

Built from the merged tree at the commit that added the firmware, both presets
clean with no warnings. The I2S firmware image is the one running on the
author's card; everything here passes `pico-apple2/sp0256/host/run_tests.sh`
(19 checks) and renders audio byte-identical to LambdaSpeak 4's independently
validated renderer.
