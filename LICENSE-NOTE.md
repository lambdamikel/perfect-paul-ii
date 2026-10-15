# DECtalkMini upstream licensing note

This note is not legal advice.

The supplied `DECtalkMini-dectalk-develop` archive contains no top-level
`LICENSE`, `COPYING`, or `NOTICE` file and the README states no repository-wide
license grant.

More importantly, many core source files contain affirmative proprietary
notices rather than open-source terms. Examples include:

- `src/cmd_init.c`: "This software is proprietary ... Possession, use, or
  copying ... is authorized only pursuant to a valid written license from
  Force or an authorized sublicensor."
- `src/spc.c`: equivalent language naming Fonix Corporation.
- `src/decstd97.c`: "Possession, use, duplication or dissemination ... is
  authorized only pursuant to a valid written license from Digital Equipment
  Corporation."

Accordingly, public availability of the archive is not, by itself, a license
to copy, modify, distribute, or sell the DECtalk core or firmware binaries
containing it. Treat the core as all-rights-reserved/proprietary unless the
project owner can provide a separate valid license or a documented chain of
permission from the relevant rightsholders. Distribution and commercial use
are especially problematic without that provenance.

The Pico SDK import helper included in the archive carries a BSD 3-Clause
license, and some third-party files such as miniaudio and stb have their own
permissive terms. Those component licenses do not relicense the DECtalk core.

`LICENSE-ADAPTER.txt` applies only to the new Apple II/Pico adapter files in
this directory.

`sp0256/` is a different matter and is kept physically separate for that
reason. It holds an emulation of the SP0256-AL2 under **GPL-3** (with its own
`LICENSE` and `vendor/README-VENDOR.md`) and builds a **separate executable**
that contains no DECtalk at all. That separation is required, not tidy: GPL-3
obliges the distributor to provide complete corresponding source for the whole
work, which cannot be done for the DECtalk core, so a single binary linking
both would be undistributable by anybody. The two UF2s are built by two
independent CMake projects and never share object code.
