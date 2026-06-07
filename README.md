<!-- Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com> -->
<!-- SPDX-License-Identifier: MIT-0 -->
<!-- scspell-id: a631c7ac-5d68-11f1-844d-80ee73e9b8e7 -->

# LZPACK

**LZPACK** is an executable compressor for CP/M‑80 binaries.

It shrinks 8080 and Z80 `.COM` programs, often to half their original size,
while leaving them directly executable: every packed file is a
*self‑extracting* `.COM` that decompresses itself and runs without any
separate unpacker and requires no changes to how the program is invoked.

It works very much like Yoshihiko Mino's classic CP/M‑80 *PopCom!* utility,
but packs tighter by using a better compression engine and decompresses
faster by using smaller hand‑optimized decompression stubs.

The **LZPACK** program and the packed executables it produces can run on a
wide range of CP/M‑80 machines, including systems with Z80, 8080, 8085, and
V20 processors, and systems with less than 48K&nbsp;TPA.

Running the compressor on a system without CP/M‑80's memory constraints (such
as on MS‑DOS, Windows, Linux, or in any UNIX‑like environment) gives even
better compression results.

[Precompiled binaries for many systems are available for download.](#downloads)

---

<!-- toc -->

- [Overview](#overview)
- [Details](#details)
  * [Compression results](#compression-results)
  * [Decompression stubs](#decompression-stubs)
  * [Operation](#operation)
    + [Compressors](#compressors)
    + [Decompressors](#decompressors)
    + [Performance](#performance)
- [Usage](#usage)
  * [Memory ceiling (`-M`)](#memory-ceiling--m)
  * [Runtime memory check (`-C`)](#runtime-memory-check--c)
  * [Runtime memory floor (`-F`)](#runtime-memory-floor--f)
- [Downloads](#downloads)
- [Building from source](#building-from-source)
  * [Build targets](#build-targets)
  * [Developer notes](#developer-notes)
- [Security](#security)
- [License](#license)

<!-- tocstop -->

## Overview

**LZPACK** is a single, ultra‑portable ANSI C89 program.

The *compressor* runs on just about anything with an ANSI C89 compiler.  You
can pack CP/M‑80 programs on any modern UNIX
(even [**ELKS**](https://github.com/ghaerr/elks)), Windows, or MS‑DOS
system without emulation, as well as pack natively on the CP/M‑80 target.

The *decompressor* that is embedded into each packed executable is hand‑written
and highly optimized 8080 or Z80 assembly.

Pre‑compiled binaries are provided for **CP/M‑80** (8080 and Z80), **CP/M‑86**,
**MS‑DOS** (8086/8088 real‑mode and 386 DPMI), **ELKS**, and **Windows** (both
32‑ and 64‑bit versions).

The CP/M‑80 builds also run on **MSX‑DOS** (and so do the packed executables
they generate).

## Details

**LZPACK**'s `-R` (restore) and `-L` (list) commands recognize both **LZPACK**
and *PopCom!*‑packed files (as they use the same container and stream format),
making it simple to decompress (and recompress) already packed executables.

**LZPACK** (and **LZPACK**‑packed binaries) can run on a *plain 8080*, not
just the Z80.  **LZPACK** analyzes the file to be packed and automatically
detects if the program actually uses Z80 instructions, and picks a matching
decompression stub.

Users can also specify `-8` to explicitly use the 8080 stub, or `-Z` to force
the Z80 stub, in case the automatic detection gets it wrong (which can happen).

While packed 8080 programs using the 8080 stub will run on any 8080 (or 8085)
system, they can sometimes be packed smaller by using the Z80 stub, at the
cost of 8080 compatibility.  If you aren't packing executables for public
distribution, you might want to use the Z80 stub unconditionally if you have
a Z80‑powered system.

**LZPACK** also includes a hand‑written and optimized 8086/8088 assembly
decompressor used for the `-R` (restore) feature when built for 8086/8088
targets such as CP/M‑86, real‑mode MS‑DOS, and ELKS.  This is not only faster
than the ANSI C89 version but also smaller, which leaves more memory available
for compression.

For extremely memory‑constrained systems, custom builds can be created that
completely exclude the `-R` decompression code, which might save a few
precious bytes.

**LZPACK** should build easily anywhere from source code, and needs only an
ANSI C89‑conforming compiler, without requiring any external assemblers.  The
source repository does not include any binary blobs.  Instead, the 8080 and
Z80 stubs are assembled from their included sources during the build process
using an included custom assembler, [**StubASM**](stubasm.c), also written in
portable C89.

It may not be the smallest executable packer, nor the most technically
impressive, but is permissively licensed, portable (able to run on machines
ranging from tiny CP/M‑80 systems to current workstations running any
operating system), and extremely compatible (without depending on undefined
behavior or undocumented functionality of any hardware or software).

### Compression results

The table below compares **LZPACK** against *PopCom!* 1.0 (the most popular
CP/M‑80 packer) on a few real‑world CP/M‑80 executables.

| Program   | Original |                  PopCom! |                 LZPACK/N |                LZPACK/N+ |                 LZPACK/C |
|:----------|---------:|-------------------------:|-------------------------:|-------------------------:|-------------------------:|
| `BLS`     | `19,210` | `12,160`&nbsp;(`‑36.7%`) | `11,890`&nbsp;(`‑38.1%`) | `11,884`&nbsp;(`‑38.1%`) | `11,945`&nbsp;(`‑37.8%`) |
| `FORTH80` |  `8,136` |  `6,272`&nbsp;(`‑22.9%`) |  `6,094`&nbsp;(`‑25.1%`) |  `6,093`&nbsp;(`‑25.1%`) |  `6,106`&nbsp;(`‑25.0%`) |
| `M80`     | `20,023` | `13,952`&nbsp;(`‑30.3%`) | `13,711`&nbsp;(`‑31.5%`) | `13,702`&nbsp;(`‑31.6%`) | `13,755`&nbsp;(`‑31.3%`) |
| `MBASIC`  | `24,313` | `19,456`&nbsp;(`‑20.0%`) | `19,182`&nbsp;(`‑21.1%`) | `19,178`&nbsp;(`‑21.1%`) | `19,239`&nbsp;(`‑20.9%`) |
| `PILOT`   | `30,902` | `13,184`&nbsp;(`‑57.3%`) | `12,798`&nbsp;(`‑58.6%`) | `12,792`&nbsp;(`‑58.6%`) | `12,876`&nbsp;(`‑58.3%`) |
| `SARGON`  | `14,592` |  `8,704`&nbsp;(`‑40.4%`) |  `8,598`&nbsp;(`‑41.1%`) |  `8,593`&nbsp;(`‑41.1%`) |  `8,619`&nbsp;(`‑40.9%`) |
| `VDT1398` | `17,443` | `13,056`&nbsp;(`‑25.2%`) | `12,876`&nbsp;(`‑26.2%`) | `12,874`&nbsp;(`‑26.2%`) | `12,914`&nbsp;(`‑26.0%`) |
| `VDT139Z` | `16,485` | `12,544`&nbsp;(`‑23.9%`) | `12,333`&nbsp;(`‑25.2%`) | `12,325`&nbsp;(`‑25.2%`) | `12,371`&nbsp;(`‑25.0%`) |
| `VDT232Z` | `24,304` | `18,688`&nbsp;(`‑23.1%`) | `18,437`&nbsp;(`‑24.1%`) | `18,430`&nbsp;(`‑24.2%`) | `18,500`&nbsp;(`‑23.9%`) |
| `WS30`    | `15,872` | `11,648`&nbsp;(`‑26.6%`) | `11,427`&nbsp;(`‑28.0%`) | `11,425`&nbsp;(`‑28.0%`) | `11,455`&nbsp;(`‑27.8%`) |
| `ZORK1`   |  `8,426` |  `5,376`&nbsp;(`‑36.2%`) |  `5,280`&nbsp;(`‑37.3%`) |  `5,276`&nbsp;(`‑37.4%`) |  `5,297`&nbsp;(`‑37.1%`) |

* The "**/N**" builds are native Linux x86_64; the "**/C**" builds are CP/M‑80.

* **LZPACK** beats *PopCom!* on **every** file in **every** configuration.

* The "**/N+**" column is the **extra compression** mode.  On a memory‑rich
  host, it parses the whole file at once and usually beats the standard
  mode by at least a few bytes (**/N+`-E`** vs. **/N**).

* The **/C** figures were measured under `tnylpo` (with a \~**63K** TPA).  On
  CP/M‑80 (or other memory‑constrained systems), the match window and
  compression ratio scale with the available memory: a small TPA means a
  small window and somewhat larger output.

* The test files were "trimmed" to their "near‑exact" length on the Linux
  host system used for testing (determined by discarding up to, but *not*
  including, the final `0x00` or `0x1A` bytes in the last 128‑byte "record").

* On CP/M&nbsp;2.2 systems, files do not have exact lengths but instead occupy
  fixed‑size records of 1024 bits (128 bytes).  When **LZPACK** is operating
  on CP/M‑Plus (CP/M‑80 or CP/M‑86&nbsp;3+) or DOS‑PLUS (CP/M‑86&nbsp;4+), the
  [LRBC](https://www.seasip.info/Cpm/bytelen.html) (Last Record Byte Count)
  metadata is used to determine how many bytes of the final record should be
  packed.  On CP/M&nbsp;2.2 systems, all bytes in the final record are packed.
  *PopCom!* does not support sizing via the LRBC and compresses all records.

* Because the [`tnylpo`](https://gitlab.com/gbrein/tnylpo) (and
  [`cpm`](https://github.com/jhallen/cpm)) emulators used for testing do *not*
  emulate CP/M‑Plus (and thus do not provide LRBC metadata), any file not
  ending at an exact record boundary would be automatically padded to the size
  of the next full record.

### Decompression stubs

Because every packed program must include a copy of the decompression stub,
it is vital that the code be as small (and fast) as possible.  The table below
compares the **LZPACK** decompression stubs against those from the
*PopCom!* packer.

|      CPU |         PopCom! | LZPACK          |
|---------:|----------------:|:----------------|
| **Z80**  | **`230` bytes** | **`187` bytes** |
| **8080** | (*Unsupported*) | **`256` bytes** |

* **LZPACK**'s Z80 code is just **187 bytes** (including setup code) versus
  *PopCom!*'s **230 bytes**, nearly **20% smaller**.
* *PopCom!* has no 8080 support at all, while **LZPACK**'s pure 8080
  decompressor weighs in at only **\~11%** larger than the *PopCom!* Z80 code.
  No **LZPACK** stub will ever be larger than two CP/M‑80 disk records
  (256 bytes).

### Operation

When a packed program is invoked, the CP/M loader places it at `0x100` and a
`JP` at the entry redirects control to the decompression stub, which then:

1. Restores the 16 original header bytes the packer has saved,
2. Relocates the compressed payload and the decompression stub into the high
   end of the TPA, so the stub can run without overwriting itself,
3. Decompresses in‑place into the TPA, writing output from `0x110` upward, and,
4. Jumps back to `0x100` to run the unpacked executable image.

#### Compressors

**LZPACK** compresses using a cost‑optimal shortest‑path parser and includes
**two implementations**:

1. The **in‑memory** implementation loads the entire file into RAM and finds
   matches with a hash‑chain over the entire file.  It is used by native,
   Windows, and DOS 386 DPMI builds.

2. The **streaming** implementation reads the input through a sliding window
   and writes the output to a temporary file, so its working memory is
   independent of the file size.  This lets memory‑constrained systems
   (*e.g.*, CP/M‑80, CP/M‑86, real‑mode MS‑DOS, ELKS) pack arbitrarily
   large executables.

Each implementation has **two modes**, which trade memory for size:

1. The **standard compression** mode uses a small parse block, keeping its
   working set tiny and leaving the most room for a large match window.

2. The **extra compression** mode (`-E`) enlarges the block for the tightest
   possible parse.

On a memory‑rich host, using `-E` trims down files by at least a few more
bytes.  On CP/M‑80 systems, due to memory constraints, the `-E` option is
not available.

#### Decompressors

**LZPACK** includes **four** independent (but equivalent) **decompression**
engines, differing in execution speed, code size, and memory usage:

1. The **standard** portable decompression engine is written in pure ANSI C89.

2. The **8080** assembly‑language decompression engine (built by **StubASM**).

3. The **Z80** assembly‑language decompression engine (also built
   by **StubASM**).

4. The **8086** assembly‑language decompression engine, used for
   the `-R` restore option on 8086/8088 systems (*i.e.*, CP/M‑86, MS‑DOS,
   ELKS).

The 8086 decompression engine source code is automatically generated by the
build system, which works by transforming a shared assembly routine into the
proper dialect for the target, currently **GNU&nbsp;`as`**,
**Watcom&nbsp;`wasm`**, or **Aztec `#asm`**, so no additional cross‑assemblers
or tools are required when cross‑compiling.

#### Performance

* While **LZPACK**‑generated executables are often smaller, more compatible,
  and always *decompress* faster than those produced by *PopCom!*, the
  **LZPACK** *compressor* is ***much*** slower than *PopCom!*'s, especially on
  vintage hardware: *PopCom!* uses hand‑written Z80 assembly, whereas
  **LZPACK** uses portable ANSI C89 to implement a cost‑optimal parser that
  does far more work per byte.

* **LZPACK** prioritizes the smallest output with the fastest possible
  *unpacking*, because decompression happens *every time the packed program
  is run*, while packing happens rarely (especially on vintage systems) and
  can be done on modern hardware (which almost everyone has now, in the
  year 2026).

## Usage

```
LZPACK v1.0-beta-4 - CP/M-80 (8080 and Z80) executable compressor
Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>

Usage:
  lzpack [-E] [-8|-Z] <file>  compress (-E: extra, -8/-Z: force 8080/Z80 stub)
  lzpack -R <file>            restore (decompress)
  lzpack -L <file>            list stored sizes
  lzpack -O <name>            set output name
  lzpack -M <top>             set memory top (default 48K)
  lzpack -C                   stub verifies memory at run time
  lzpack -F <floor>           require memory top >= floor (implies -C)
  lzpack -V                   show LZPACK information
```

The **CP/M‑80** version of **LZPACK** is split into two utilities:
  * `LZPACK.COM` for compression only, and,
  * `LZUNPACK.COM` for decompression and listing.

On all other platforms, a single `lzpack` tool is provided, as shown above.

### Memory ceiling (`-M`)

As a packed program decompresses in place on the target machine, the image
expands to its full original size at `0x100` with the relocated decompressor
sitting above it.  At packing time, **LZPACK** verifies that everything fits
below a memory ceiling (*MEMTOP*), and will refuse to produce an output file
otherwise.  The default is at `0xBDFF`, so all packed programs are guaranteed
to run on any **48K&nbsp;TPA** system, but the `-M` option can be used to
override this; for example:

* Use `-M 64` to pack programs too large for **48K&nbsp;TPA**, but the result
  *requires* a correspondingly larger TPA at run time.
* Use `-M 32` (or less) to guarantee the output runs on smaller systems, or
  to keep the unpacker away from any resident driver that might have stolen
  the top of the TPA, or to enforce a maximum image size while developing new
  software.

The `-M` option accepts an argument in three formats:

|              Format | Example                    | Description                    |
|--------------------:|:---------------------------|:-------------------------------|
| KB size (≤64)       | `-M 32` (or `-M 32K`)      | kilobytes (48 is default)      |
| hex address         | `-M 0x7DFF`                | literal *MEMTOP* address       |
| decimal address     | `-M 65023`                 | literal *MEMTOP* address (>64) |

Values below `0x1190` (**4K**) or above `0xFFFF` (**64K**) are rejected.

### Runtime memory check (`-C`)

The packing‑time checks cannot know the details of the machine the packed
program will eventually run on; for example, it might have a much smaller TPA
than the one running the packer, or it might have a resident driver that
lowers the BDOS pointer at `0x0006`, which could be silently overwritten
during decompression.  The `-C` option enhances the stub with a small
(48‑byte) runtime check.  It verifies that the highest address the unpacker
will write to lies below the BDOS base and that at least 16 bytes are clear
of the live inherited stack.  If the program does not fit, it prints `No room`
and aborts.

Because this check adds an extra 48 bytes to every packed executable, it is
disabled by default.  Enabling it does **not** consume any high memory, and it
is never relocated, so it will not change what fits with any given
`-M` setting.

### Runtime memory floor (`-F`)

The `-F` option is mostly useful to developers of CP/M‑80 software and
not end‑users.

<details>
 <summary><b><i>Expand this section for further details.</i></b></summary>

<br>

The `-C` option adds a check that refuses a TPA that the *unpacker* would
overrun, but a packed program almost always needs more memory to actually
*run* than it does to simply unpack.  With a TPA that sits between those
two bounds, the program unpacks successfully but then crashes (or silently
corrupts memory) during its own startup (which would still happen even in
the absence of any executable compression).

When the packer is informed of the actual program runtime memory requirements
via the  `-F` option, the check/verification stub (normally emitted with `-C`)
can cleanly refuse to run on a machine whose memory top lies below the
specified floor.  The argument accepts the same formats as `-M` (and
implies `-C`).

Most CP/M users wishing to save space on their disks will be packing existing
programs and will never need to use `-F`.  Developers who are creating CP/M
software (who ship packed executables), especially when working with compiled
languages, can greatly benefit.  A compiled `.COM` usually understates its
runtime footprint: uninitialized data (BSS) is not necessarily stored in the
file at all, and the language's runtime and startup code carves its stack and
heap out of high memory *before* the first line of user code (*e.g.*,
`main()`) runs.

Because the trouble happens early, no in‑program check can catch this sort
of shortfall.  By the time the `main()` function could test anything, the
runtime has already cleared BSS across the BDOS or planted a heap with a
wrapped size, or simply crashed without any useful messages displayed at all.

Finding the floor value to use is an extra step at release: read the end of
static storage from the linker's map and add the runtime's stack reserve, or
if you are cross-developing, simply measure the value empirically by using an
emulator that can dynamically shrink the TPA.

It is hoped that the **LZPACK** build can serve as an example of this process,
since the shipped CP/M‑80 binaries (`LZPACK.COM` and `LZUNPACK.COM`) are
packed with a floor derived from each tool's own map plus the stack reserve,
so on any system with a TPA large enough for them to *unpack* but too small
for them to fully *initialize*, they simply print `No room` and exit cleanly,
which would be impossible to achieve using C code alone.

The `-L` (list) command reads the check block back out of a packed file.
It reports `no -C check` for files packed without `-C`, the enforced floor
for checked files (`-C check; floor 0xBDFF`), and marks floors that were
raised beyond the unpack bound with `(-F)`.  On **CP/M‑80** systems the list
option is part of `LZUNPACK.COM`, so the embedded floor of any packed program
can be inspected on the target machine itself.

</details>

## Downloads

|                                                                                          File  | Size        | Platform                             |
|-----------------------------------------------------------------------------------------------:|:------------|:-------------------------------------|
| [LZPCKI80.ARC](https://github.com/johnsonjh/lzpack/raw/refs/heads/master/bindist/LZPCKI80.ARC) | 20&nbsp;KiB |**CP/M‑80**&nbsp;(8080)               |
| [LZPCKZ80.ARC](https://github.com/johnsonjh/lzpack/raw/refs/heads/master/bindist/LZPCKZ80.ARC) | 20&nbsp;KiB |**CP/M‑80**&nbsp;(Z80)                |
| [LZPCK86C.ARC](https://github.com/johnsonjh/lzpack/raw/refs/heads/master/bindist/LZPCK86C.ARC) | 16&nbsp;KiB |**CP/M‑86**&nbsp;(8086/8088)          |
| [LZPCK86R.ZIP](https://github.com/johnsonjh/lzpack/raw/refs/heads/master/bindist/LZPCK86R.ZIP) | 20&nbsp;KiB |**MS‑DOS**&nbsp;(8086/8088)           |
| [LZPCK86P.ZIP](https://github.com/johnsonjh/lzpack/raw/refs/heads/master/bindist/LZPCK86P.ZIP) | 84&nbsp;KiB |**MS‑DOS**&nbsp;(80386 DPMI)          |
| [LZPCKW32.ZIP](https://github.com/johnsonjh/lzpack/raw/refs/heads/master/bindist/LZPCKW32.ZIP) | 40&nbsp;KiB |**Windows**&nbsp;(32-bit&nbsp;MSVCRT) |
| [LZPCKW64.ZIP](https://github.com/johnsonjh/lzpack/raw/refs/heads/master/bindist/LZPCKW64.ZIP) | 24&nbsp;KiB |**Windows**&nbsp;(64-bit&nbsp;UCRT)   |
| [LZPCKELK.Z](https://github.com/johnsonjh/lzpack/raw/refs/heads/master/bindist/LZPCKELK.Z)     | 16&nbsp;KiB |**ELKS**&nbsp;(8086/8088)             |

If you need a CP/M ARC utility, `UNARC` is available for
[8080](.utils/unarca.com) and [Z80](.utils/unarcz.com) CP/M‑80, and
`ARCCPM` for [8086/8088](.utils/arccpm.cmd) CP/M‑86.

## Building from source

**LZPACK** needs **only an ANSI C89 compiler** to build on any UNIX‑like
system.

* To build a native binary, just run `make` (or `gmake`), which builds
  **StubASM**, assembles the stubs, and then compiles `lzpack`:

  ```sh
  make
  ```

* You can also explicitly set `CC`, `CFLAGS`, `LDFLAGS`, etc.  For example, to
  build an optimized 64‑bit binary on IBM AIX using the IBM XL C/C++ compiler
  and AIX `make`:

  ```sh
  make CC=xlc CFLAGS="-O3 -q64" LDFLAGS="-Wl,-b64"
  ```

The GNU GCC, LLVM Clang, PCC, NVIDIA HPC SDK C/C++, Oracle Studio C/C++,
DMD ImportC, CompCert C, Open64, PathScale EKOPath, IBM XL C/C++,
IBM Open XL C/C++, and МЦСТ LCC compilers are regularly tested.

### Build targets

The following targets build various `lzpack` binaries.

Most users will only be interested in the native binary build.

| Make Target | Description            | Toolchain                                                                                                                   |
|------------:|:-----------------------|:----------------------------------------------------------------------------------------------------------------------------|
| `all`       | Native&nbsp;binary     | ANSI&nbsp;C89&nbsp;compiler&nbsp;(*e.g.*,&nbsp;`c89`,&nbsp;`gcc`,&nbsp;`clang`)                                             |
| `cpm`       | CP/M‑80&nbsp;8080+Z80  | [z88dk](https://z88dk.org/)&nbsp;(**2026‑06‑08+**)                                                                          |
| `cpm86`     | CP/M‑86&nbsp;8086/8088 | [cross‑Aztec&nbsp;C86&nbsp;v4.2](https://github.com/tsupplis/cpm86-crossdev)&nbsp;([tsupplis](https://github.com/tsupplis)) |
| `msdos`     | MS‑DOS&nbsp;8086/8088  | [Open&nbsp;Watcom&nbsp;V2.0](https://github.com/open-watcom/open-watcom-v2)                                                 |
| `djgpp`     | MS‑DOS&nbsp;80386      | [DJGPP](https://www.delorie.com/djgpp/)&nbsp;+&nbsp;[CWSDPMI](https://sandmann.dotster.com/cwsdpmi/)                        |
| `elks`      | ELKS&nbsp;8086/8088    | [IA16‑GCC](https://gitlab.com/tkchia/build-ia16)                                                                            |
| `windows`   | Windows&nbsp;32/64‑bit | [MinGW‑w64](https://www.mingw-w64.org/)&nbsp;[GCC](https://gcc.gnu.org/)                                                    |

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
  At a minimum, you need Georg Brein's
  [`tnylpo`](https://gitlab.com/gbrein/tnylpo) emulator and Joe Hallen's
  [CPM](https://github.com/jhallen/cpm) emulator installed.  You should build
  these with full optimizations enabled, as the test suite is extensive with
  a lengthy runtime.

* If you would like to contribute to **LZPACK** development, it is *extremely*
  *important* that you have ***all*** of the optional linters, static analysis
  tools, emulators, and cross‑toolchains installed, and that **both**
  `make lint` and `make test` pass completely clean, as this is a
  prerequisite for any change.

* Usage of AI (artificial intelligence) tools by contributors is currently
  permitted, subject to the same conditions as the
  [LLVM AI Tool Use Policy](https://llvm.org/docs/AIToolPolicy.html), but
  this permission may be withdrawn at any time and without notice.

## Security

* The canonical home of this software is
  [`https://github.com/johnsonjh/lzpack`](https://github.com/johnsonjh/lzpack),
  with a mirror at
  [`https://gitlab.com/johnsonjh/lzpack`](https://gitlab.com/johnsonjh/lzpack).
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
