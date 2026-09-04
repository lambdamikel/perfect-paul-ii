# Perfect Paul ][ — Pico SDK target

A DECtalk speech synthesizer card for the Apple II, built on a Raspberry Pi
Pico. Named for DECtalk's default voice, `[:np]`.

This directory is a drop-in Pico SDK target for the DECtalkMini tree. It keeps
DECtalk synthesis and audio on core 1, while core 0 receives bytes from an
Apple II slot write register using RP2040 PIO.

**This file covers building and flashing only.** The hardware reference — the
bus interface, pin-by-pin wiring, the audio output stage, PCB notes, the BASIC
protocol, the demo programs and the validation status — is in the repository
README at <https://github.com/lambdamikel/perfect-paul-ii>, which is the single
source of truth for all of it. None of it is repeated here, so the two cannot
drift apart.

## Build

Visual Studio Code can be the IDE, but Microsoft MSVC/Visual C++ does not
compile RP2040 firmware: CMake uses the Arm GNU embedded compiler that the Pico
SDK configures.

Install the Pico SDK 2.x, Pico Extras, the Arm GNU toolchain, CMake and Ninja,
plus either the Raspberry Pi Pico VS Code extension or CMake Tools. Then open
`platforms/pico-apple2` and select a supplied preset.

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
export PICO_EXTRAS_PATH=/path/to/pico-extras
cd platforms/pico-apple2
cmake --preset pico-i2s-release
cmake --build --preset pico-i2s-release
```

PowerShell:

```powershell
$env:PICO_SDK_PATH = 'C:\pico\pico-sdk'
$env:PICO_EXTRAS_PATH = 'C:\pico\pico-extras'
cd platforms\pico-apple2
cmake --preset pico-i2s-release
cmake --build --preset pico-i2s-release
```

Command Prompt:

```bat
set PICO_SDK_PATH=C:\pico\pico-sdk
set PICO_EXTRAS_PATH=C:\pico\pico-extras
cd platforms\pico-apple2
cmake --preset pico-i2s-release
cmake --build --preset pico-i2s-release
```

### Presets

| Preset | Audio backend | Output directory |
|---|---|---|
| `pico-i2s-release` | I2S, GP20/21/22 | `build/i2s-release/` |
| `pico-pwm-release` | PWM, GP28 | `build/pwm-release/` |

### What each preset builds

- `dectalk_apple2.uf2` — the card firmware.
- `dectalk_selftest.uf2` — speaks a fixed phrase loop by itself, needing no
  Apple II and no slot traffic.

`dectalk_selftest` follows `DECTALK_AUDIO_I2S`, so it always uses the same audio
backend as the firmware built beside it. **Build and flash it from the same
preset**: a self test built for the other backend is silent by construction and
tells you nothing about the path you are actually using. Flash it first on a new
board — if it is silent, the fault is in the audio path rather than the bus
interface.

### Build options

| Option | Default | Effect |
|---|---|---|
| `DECTALK_AUDIO_I2S` | `ON` | I2S backend; `OFF` selects PWM |
| `DECTALK_SPEAK_STARTUP_BANNER` | `ON` | Speak a ready message at power-up |
| `DECTALK_BUILD_SELFTEST` | `ON` | Also build `dectalk_selftest` |
| `DECTALK_PWM_GPIO` | `28` | GPIO for PWM audio when I2S is off |

Changing a default in `CMakeLists.txt` does **not** affect a build directory
that already exists: CMake's `option()` will not overwrite a cache entry that is
already set. Pass `-D<NAME>=<VALUE>` when reconfiguring, or delete the build
directory.

## Deploy

1. Power the Apple II off and remove the card for USB flashing.
2. Hold Pico `BOOTSEL` while connecting USB.
3. Copy `dectalk_apple2.uf2` to the mounted RP2 boot volume, or use `picotool`.
4. Disconnect USB, install the card, and power the Apple II.
5. The Pico's LED turns on once DECtalk and audio have initialised, and the card
   says "Perfect Paul Two ready."

Apart from that power-up message the firmware speaks only what arrives from the
slot, so it is otherwise silent on the bench. USB CDC stays enabled for testing:
type a line and press Enter, or drive it with `tools/paul-say.sh` from the
repository. Ctrl-C stops speech.

## Licensing

`LICENSE-ADAPTER.txt` covers the adapter files in this directory. It does **not**
cover the DECtalk core — read `LICENSE-NOTE.md` before redistributing source or
any UF2 built from this tree.
