<!-- Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com> -->
<!-- SPDX-License-Identifier: MIT-0 -->
<!-- scspell-id: a631c7ac-5d68-11f1-844d-80ee73e9b8e7 -->

# LZPACK

**LZPACK** is an executable compressor for CP/M‑80 binaries.  It shrinks 8080
and Z80 `.COM` programs to roughly half their size while leaving them directly
executable: every packed file is a *self‑extracting* `.COM` that decompresses
itself and runs without any separate unpacker, and requires no changes to how
the program is invoked.

It works very much like Yoshihiko Mino's classic *PopCom!* utility, but can
compress tighter and extracts faster thanks to smaller hand‑optimized
decompression stubs.  The program itself as well as the packed executables it
generates run in far more places, such as non‑Z80 CP/M‑80 systems with
8080 or 8085 processors that *PopCom!* doesn't support, while maintaining the
same ~47K TPA footprint.  Running the compressor on a system without CP/M‑80's
tight memory constraints (such as on MS‑DOS, Windows, Linux, or in a
UNIX‑like environment) gives even better compression results.

<!-- toc -->

- [Overview](#overview)
  * [Compression results](#compression-results)
  * [Decompression stubs](#decompression-stubs)
  * [Operation](#operation)
    + [Compressors](#compressors)
  * [Decompressors](#decompressors)
- [Usage](#usage)
- [Downloads](#downloads)
  * [Choosing a build](#choosing-a-build)
- [Building from source](#building-from-source)
  * [Build targets](#build-targets)
  * [Developer notes](#developer-notes)
- [Security](#security)
- [License](#license)

<!-- tocstop -->

## Overview

LZPACK is a single, ultra‑portable ANSI C89 program.  The *compressor* runs on
just about anything with a C compiler.  You can pack CP/M‑80 programs from a
modern UNIX (even ELKS), Windows, MS‑DOS system, or pack natively on the
CP/M‑80 target.  The *decompressor* that is embedded in each packed executable
is hand‑written and highly optimized 8080 or Z80 assembly.

Pre‑compiled binaries are provided for **CP/M‑80** (8080 and Z80), **CP/M‑86**,
**MS‑DOS** (8086/8088 real‑mode and 386 DPMI), **ELKS**, and **Windows** (both
32‑ and 64‑bit versions).  The CP/M‑80 builds also runs on **MSX‑DOS** (and so
do the packed executables it generates).

LZPACK's `-R` (restore) and `-L` (list) commands recognize both LZPACK and
PopCom!‑packed files, making it simple to decompress (or recompress) already
packed executables.

LZPACK (and LZPACK‑packed binaries) can run on a plain 8080, not just the Z80.
LZPACK analyzes the file to be packed and automatically detects if the program
actually uses Z80 instructions and picks a matching extractor.  Users can also
specify to `-8` explicitly use the 8080 stub, or `-Z` to forces the Z80 stub.
While packed 8080 programs runs on 8080/8085 system, they can sometimes be
packed smaller with the Z80 stub, at the cost of a 8080 compatibility.

LZPACK also includes a hand‑written and optimized 8086 assembly decompressor
used for the `-R` (restore) feature when built for 8086/8088 targets such as
CP/M‑86, real‑mode MS‑DOS, and ELKS, which is not only faster than the ANSI C
version but smaller, which leaves more memory available for compression.

For extremely memory constrained systems, custom builds can be created that
completely exclude the `-R` decompression code, which might save a few
precious bytes.

LZPACK should build easily anywhere from source code, and requires only an
ANSI C89‑conforming compiler, without using any external assemblers.  The
source repository does not include any binary blobs.  Instead, the 8080 and
Z80 stubs are assembled from their included sources during the build process
using an included custom assembler, StubASM, also written in portable C89.

### Compression results

The table below compares LZPACK against PopCom! 1.0 (the most popular CP/M‑80
packer) on a few real‑world CP/M‑80 executables.

|   Program | Original | PopCom! | LZPACK/Linux | LZPACK/Linux `-e` | LZPACK/CP/M‑80 | LZPACK/CP/M‑80 `-e` |
|----------:|---------:|--------:|-------------:|------------------:|---------------:|--------------------:|
| `BLS`     | `19210`  | `12160` | `12542`      | `11884`           | `12552`        | `12150`             |
| `FORTH80` | `8136`   | `6272`  | `6454`       | `6093`            | `6458`         | `6139`              |
| `M80`     | `20023`  | `13952` | `14528`      | `13702`           | `14545`        | `14024`             |
| `MBASIC`  | `24313`  | `19456` | `20139`      | `19178`           | `20160`        | `19569`             |
| `PILOT`   | `30902`  | `13184` | `13582`      | `12792`           | `13588`        | `13277`             |
| `SARGON`  | `14592`  | `8704`  | `9183`       | `8593`            | `9178`         | `8689`              |
| `VDT1398` | `17443`  | `13056` | `13633`      | `12874`           | `13639`        | `13117`             |
| `VDT139Z` | `16485`  | `12544` | `12894`      | `12325`           | `12902`        | `12561`             |
| `VDT232Z` | `24304`  | `18688` | `19363`      | `18430`           | `19375`        | `18893`             |
| `WS30`    | `15872`  | `11648` | `12221`      | `11425`           | `12224`        | `11539`             |
| `ZORK1`   | `8426`   | `5376`  | `5542`       | `5276`            | `5546`         | `5338`              |

* The above files were "trimmed" to "near‑exact" length (determined up
  discarding up to, but *not* including, the final `0x00` or `0x1A` bytes
  in the last 128‑byte "record") on the Linux host systems used for testing.

* On CP/M‑80 2.2 systems, files do not have exact lengths but instead occupy
  fixed‑size records of 1024‑bits (128‑bytes).  When LZPACK is operating on
  CP/M‑Plus (CP/M-80 or CP/M‑86 3+) or **DOS‑PLUS** (CP/M‑86 4+), the
  [LRBC](https://www.seasip.info/Cpm/bytelen.html) (Last Record Byte Count)
  metadata is used to determine how many bytes of the final record should be
  packed.  On CP/M 2.2 systems, all bytes in the final record are packed.
  PopCom! does not support sizing via the LRBC and compresses all records.

* Because the `tnylpo` and `cpm` emulators used for testing do *not* emulate
  CP/M‑Plus (and thus do not provide LRBC metadata) so any file not ending on
  an exact record boundary is automatically padded to the size of the next
  full record.  This applies to both LZPACK and PopCom! results in the table.

### Decompression stubs

Because every packed program must include a copy of the decompression stub,
it's vital that code is as small (and fast) as possible.  The table below
compares the LZPACK decompression stubs against those from the PopCom! packer.

|      CPU | LZPACK          | PopCom! 1.0 |
|---------:|:----------------|:------------|
| **Z80**  | **`187` bytes** | `230` bytes |
| **8080** | **`256` bytes** | `---------` |

LZPACK's Z80 code is just **187 bytes** versus PopCom!'s 230 bytes, nearly
**20% smaller**.  PopCom! has no 8080 support at all,  and LZPACK's pure 8080
decompressor comes in at only about 11% larger than the PopCom! code.  In any
cases, no LZPACK stub is larger than 256 bytes, or two CP/M‑80 disk records.

### Operation

When a packed program is invoked, the CP/M loader places it at `0x100` and a
`JP` at the entry redirects control to the decompression stub, which then:

1. Restores the 16 original header bytes the packer has saved,
2. Relocates the compressed payload and the decompression stub into the high
   end of the TPA, so stub can run without overwriting itself,
3. Decompresses in‑place into the TPA, writing output from `0x110` upward, and,
4. Jumps back to `0x100` to run the unpacked executable image.

#### Compressors

LZPACK actually includes **three** independent **compression** engines, which
differ in how much effort is expended compressing and how much memory
is required:

1. The **standard** in‑memory compression engine loads the entire file into
   memory and uses a fast hash‑chained‑based L77 match finder.

2. The **extra** compression engine (`-e`) is a cost‑optimal shortest‑path
   parser that looks for the smallest possible encodings.  It needs more
   memory, so it is only available on CP/M‑80 builds that target machines with
   **`56K`** or larger TPAs.

3. The **streaming** compression engine reads the input through a sliding
   window and writes the compressed data to a temporary file, so the working
   memory size is independent of file input size, allowing memory constrained
   systems pack large executables.

#### Decompressors

LZPACK includes **four** independent (but equivalent) **decompression**
engines, differing in execution speed, code size, and memory usage:

1. The **standard** portable decompression engine is written in pure ANSI C89.

2. The **8080** assembly‑language decompression engine (built by StubASM).

3. The **Z80** assembly‑language decompression engine (also built by StubASM).

4. The **8086** assembly‑language decompression engine, which is used for
   the `-R` restore option on 8086/8088 systems (*i.e.*, CP/M‑86, MS‑DOS,
   ELKS).  It's source code is automatically generated by the build system,
   which works by transforming a generic assembly routine into the proper
   dialect for the target, currently GNU&nbsp;`as`, Watcom&nbsp;`wasm`, or
   Aztec `#asm`, so no additional assembler is required when cross‑compiling.

#### Performance

* While LZPACK‑generated executables can pack smaller, are more compatible,
  and always *decompress* faster than than those produced by PopCom!, the
  LZPACK *compressor* is much slower than PopCom!, especially on vintage
  hardware, because the PopCom! compression engine is written in
  highly‑optimized Z80 assembly vs. LZPACK's ANSI C.

* While compression speed (and ratios) should improve in the future,
  development focused on providing the fastest possible *unpacking*, since
  it happens *every time a program is executed*, often on vintage hardware,
  while the *packing* operation is performed rarely and can be run on more
  modern hardware (which almost everyone has now, in the year 2026).

## Usage

```
LZPACK v0.99983 - 48K CP/M-80 (8080 and Z80) executable compressor
Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>

Usage:
  lzpack [-e] [-8|-Z] <file>  compress (-e: extra, -8/-Z: force 8080/Z80 stub)
  lzpack -R <file>            restore (decompress)
  lzpack -L <file>            list stored sizes
  lzpack -O <name>            set output name
  lzpack -V                   show LZPACK information
```

## Downloads

|                                                                                          File  | Size        | Platform                                  |
|-----------------------------------------------------------------------------------------------:|:------------|:------------------------------------------|
| [LZPCKI80.ARC](https://github.com/johnsonjh/lzpack/raw/refs/heads/master/bindist/LZPCKI80.ARC) | 16&nbsp;KiB |**CP/M‑80**&nbsp;(8080)                    |
| [LZPCKZ80.ARC](https://github.com/johnsonjh/lzpack/raw/refs/heads/master/bindist/LZPCKZ80.ARC) | 16&nbsp;KiB |**CP/M‑80**&nbsp;(Z80)                     |
| [LZPKOI80.ARC](https://github.com/johnsonjh/lzpack/raw/refs/heads/master/bindist/LZPKOI80.ARC) | 16&nbsp;KiB |**CP/M‑80**&nbsp;(8080&nbsp;≥56K&nbsp;TPA) |
| [LZPKOZ80.ARC](https://github.com/johnsonjh/lzpack/raw/refs/heads/master/bindist/LZPKOZ80.ARC) | 16&nbsp;KiB |**CP/M‑80**&nbsp;(Z80&nbsp;≥56K&nbsp;TPA)  |
| [LZPCK86C.ARC](https://github.com/johnsonjh/lzpack/raw/refs/heads/master/bindist/LZPCK86C.ARC) | 16&nbsp;KiB |**CP/M‑86**&nbsp;(8086/8088)               |
| [LZPCK86R.ZIP](https://github.com/johnsonjh/lzpack/raw/refs/heads/master/bindist/LZPCK86R.ZIP) | 20&nbsp;KiB |**MS‑DOS**&nbsp;(8086/8088)                |
| [LZPCK86P.ZIP](https://github.com/johnsonjh/lzpack/raw/refs/heads/master/bindist/LZPCK86P.ZIP) | 84&nbsp;KiB |**MS‑DOS**&nbsp;(80386 DPMI)               |
| [LZPCKW32.ZIP](https://github.com/johnsonjh/lzpack/raw/refs/heads/master/bindist/LZPCKW32.ZIP) | 36&nbsp;KiB |**Windows**&nbsp;(32-bit&nbsp;MSVCRT)      |
| [LZPCKW64.ZIP](https://github.com/johnsonjh/lzpack/raw/refs/heads/master/bindist/LZPCKW64.ZIP) | 24&nbsp;KiB |**Windows**&nbsp;(64-bit&nbsp;UCRT)        |
| [LZPCKELK.Z](https://github.com/johnsonjh/lzpack/raw/refs/heads/master/bindist/LZPCKELK.Z)     | 20&nbsp;KiB |**ELKS**&nbsp;(8086/8088)                  |

### Choosing a build

Several builds are offered for memory‑constrained platforms because *every
single byte counts* when the compressor has to compete with its own sliding
compression window for memory.  A smaller compression window will find fewer
matches and achieve lower compression ratios, so the best build to use
depends on how much memory is available on the system that will actually run
the LZPACK program.

* **CP/M‑80**  The **extra** compression engine needs a large block of working
  memory that, on a 48K TPA machine, only can fit by shrinking the compression
  window down so far that it actually compresses **much worse** than the
  **standard** engine, so the plain CP/M‑80 builds, for systems with
  <56K&nbsp;TPA
  ([`LZPCKI80.ARC`](https://github.com/johnsonjh/lzpack/raw/refs/heads/master/bindist/LZPCKI80.ARC)
  for 8080, and
  [`LZPCKZ80.ARC`](https://github.com/johnsonjh/lzpack/raw/refs/heads/master/bindist/LZPCKZ80.ARC)
  for Z80), are **streaming** engine only.  The **extra** compression engine
  ships as a separate build for systems with ≥56K&nbsp;TPA
  ([`LZPKOI80.ARC`](https://github.com/johnsonjh/lzpack/raw/refs/heads/master/bindist/LZPKOI80.ARC)
  for 8080, and
  [`LZPKOZ80.ARC`](https://github.com/johnsonjh/lzpack/raw/refs/heads/master/bindist/LZPKOZ80.ARC)
  for Z80).  It is *highly recommended* that you use a build appropriate to
  your TPA size.  While the 56K TPA builds will likely still *run* on a machine
  with less memory, compression will suffer greatly vs. the "lesser" 48K TPA
  build.

* **MS‑DOS**  The real‑mode MS‑DOS build
  ([`LZPCK86R.ZIP`](https://github.com/johnsonjh/lzpack/raw/refs/heads/master/bindist/LZPCK86R.ZIP))
  is designed to run on any 8086/8088 PC with as little as 48K of memory using
  the **streaming** engine.  The protected‑mode DPMI MS‑DOS build
  ([`LZPCK86P.ZIP`](https://github.com/johnsonjh/lzpack/raw/refs/heads/master/bindist/LZPCK86P.ZIP))
  requires an 80386 or better but offers the **standard** and **extra**
  compression engines.

## Building from source

LZPACK needs **only an ANSI C90 compiler** to build on any UNIX‑like system.

To build a native binary just run `make` (or `gmake`), which builds StubASM,
assembles the stubs, and then compiles `lzpack`:

```sh
make
```

### Build targets

The following targets build various `lzpack` binaries.

Most users will only be interested in the native build.

| Make Target | Description            | Toolchain                                    |
|------------:|:-----------------------|:---------------------------------------------|
| `all`       | Native&nbsp;binary     | ANSI&nbsp;C89                                |
| `cpm`       | CP/M‑80&nbsp;8080+Z80  | z88dk&nbsp;(20250501+)                       |
| `cpm-opt`   | CP/M‑80&nbsp;8080+Z80  | z88dk&nbsp;(20250501+)                       |
| `cpm86`     | CP/M‑86&nbsp;8086/8088 | cross‑Aztec&nbsp;C&nbsp;v4.2&nbsp;(tsupplis) |
| `msdos`     | MS‑DOS&nbsp;8086/8088  | Open&nbsp;Watcom&nbsp;V2.0                   |
| `djgpp`     | MS‑DOS&nbsp;80386      | DJGPP&nbsp;+&nbsp;CWSDPMI                    |
| `elks`      | ELKS&nbsp;8086/8088    | IA16‑GCC                                     |
| `windows`   | Windows&nbsp;32/64‑bit | MinGW‑w64&nbsp;GCC                           |

The following targets will likely only be of interest to developers:

| Make Target | Description                                                                                      |
|------------:|:-------------------------------------------------------------------------------------------------|
| `stubs`     | Builds&nbsp;only&nbsp;StubASM&nbsp;and&nbsp;the&nbsp;8080&nbsp;+&nbsp;Z80&nbsp;stubs             |
| `test`      | Runs&nbsp;a&nbsp;comprehensive&nbsp;end‑to‑end&nbsp;multiplatform&nbsp;test&nbsp;suite           |
| `lint`      | Source‑code&nbsp;quality&nbsp;checks&nbsp;(linting&nbsp;and&nbsp;static&nbsp;analysis)           |
| `tags`      | Builds&nbsp;source&nbsp;code&nbsp;tags&nbsp;(`etags`,&nbsp;`ctags`,&nbsp;`gtags`,&nbsp;`cscope`) |

The CP/M‑80 build targets support running **z88dk** in the usual way or via
Docker.  Setting the environment variable `CPM_BACKEND=local` forces a
standard build and setting `CPM_BACKEND=docker` forces the Docker‑ized build.
If the `CPM_BACKEND` environment variable is unset, a proper **z88dk**
invocation will be automatically determined by the build system.

### Developer notes

* `make lint` needs only a POSIX shell to run (plus whichever linters and
  static analysis tools it invokes).  You'll be informed of any missing
  prerequisites as well as any optional tools when you invoke `make lint`.

* `make test` requires `python3`, several emulators, and many cross‑toolchains
  installed if you want to run *all* the tests (of which there are about 400).
  At a minimum, you you have Georg Brein's
  [`tnylpo`](https://gitlab.com/gbrein/tnylpo) and Joe Hallen's
  [CPM](https://github.com/jhallen/cpm) emulators installed.

* If you would like to contribute to LZPACK development, it's extremely
  important that you have *all* of the optional linters, static analysis
  tools, emulators, and cross‑toolchains installed and that that **both**
  `make lint` and `make test` pass completely clean, as this is a prerequisite
  for any change.

* Usage of AI (artificial intelligence) tools by contributors is currently
  permitted, subject to the
  [LLVM AI Tool Use Policy](https://llvm.org/docs/AIToolPolicy.html), but
  this permission may be withdrawn at any time and without notice.

## Security

* The canonical home of this software is
  [`https://github.com/johnsonjh/lzpack`](https://github.com/johnsonjh/lzpack).
* This software is intended to be **secure** 🛡️.
* If you find any security‑related problems, please don't hesitate to
  [open a GitHub Issue](https://github.com/johnsonjh/lzpack/issues/new).

## License

This software is distributed under the terms of the permissive
[MIT No Attribution (MIT-0)](LICENSE) license.

<!--
Local Variables:
mode: markdown
indent-tabs-mode: nil
fill-column: 80
eval: (setq-local display-fill-column-indicator-column 80)
eval: (display-fill-column-indicator-mode 1)
End:
-->

<!-- vim: set ft=markdown expandtab cc=80 : -->
<!-- EOF -->
