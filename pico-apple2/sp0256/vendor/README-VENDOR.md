# Vendored SP0256 core

These files come from **[GmEsoft/SP0256_CTS256A-AL2](https://github.com/GmEsoft/SP0256_CTS256A-AL2)**
(GPL-3.0), whose LPC engine derives from **Joe Zbiciak's** Intellivoice
emulator in jzIntv (GPL-2.0-or-later). Both are compatible with LambdaSpeak's
GPL-3.

| File | What it is |
|---|---|
| `sp0256.c` / `sp0256.h` | the 12-pole LPC engine and SP0256 microsequencer |
| `sp0256_al2.cpp` / `.h` | the SP0256-AL2 mask ROM image and allophone labels |
| `types.h` | fixed-width integer typedefs |

The emulation is a real LPC reconstruction, not recorded samples, so pitch and
timing behave like the chip rather than like a sampler.

## Local changes

Kept to the minimum, each marked in the source:

1. **`types.h` — 64-bit correctness.** The fallback typedefs used
   `unsigned long` for `uint64_t`. That is 64 bits on a typical Linux host but
   **32 bits on RP2350 and every other 32-bit target**, which would silently
   truncate `ivoice_t::now` and `sound_current`. Any C99 compiler is now sent to
   `<stdint.h>` instead.

2. **`sp0256.c` — stack.** The interactive single-step debug block holds a
   1 KB buffer, which cost `sp0256_micro()` a **1112-byte stack frame** even
   though the code is unreachable on an embedded target — more than half of a
   Pico core's default 2 KB stack. Firmware builds define
   `SP0256_NO_STDIN_DEBUG` to compile it out; the host build keeps it.
   Verified not to change a single sample: the rendered WAVs are byte-identical
   before and after.

3. **`sp0256.c` — the second utterance.** `sp0256_getNextSample()` returns 0
   early whenever `halted` is set, without running the filter, so nothing
   decrements `filt.rpt`; and `sp0256_micro()`'s HALT branch parks the chip
   with `filt.rpt = 1`. Since `micro()` is gated on `rpt <= 0 && cnt <= 0`, it
   is never re-entered once the chip runs dry — so the **next** allophone
   written can never be picked up. `lrq` stays 0 and the chip deadlocks with a
   command pending: the first utterance plays, everything after it is silence
   forever.

   `ivoice_wr()` now clears the rest counter when it loads the ALD of a halted
   chip, which is what the real part does. It is scoped to exactly the rest
   state (`halted` and nothing left in the filter), so a pipelined write during
   speech — the normal case — is untouched, and the rendered audio is
   byte-identical before and after.

   **This bug is invisible to a host renderer that pushes every allophone up
   front and exits at idle**, which is what both `render_wav` and `pp2_render`
   do, and it survived until a self test on real hardware spoke a second
   sentence. `host/second_utterance` is the regression test.

   **It applies to LambdaSpeak 4 as well** — same vendored core, same style of
   host harness, and the `&E1`/`&E2` modes would hit it on the first CPC that
   spoke twice.

Nothing else is modified, so tracking upstream stays easy.
