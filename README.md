<!-- Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com> -->
<!-- SPDX-License-Identifier: MIT-0 -->
<!-- scspell-id: c0fa2810-585c-11f1-bf4f-80ee73e9b8e7 -->

# LZPACK

LZPACK is a PopCom!-compatible CP/M-80 executable compressor that
runs on CP/M-80, CP/M-86, MS-DOS, UNIX, and many other platforms.

```
LZPACK v0.3 - PopCom!-compatible 48K CP/M-80 executable compressor
Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>

Usage:
  lzpack [-e] [-8] <file>  compress (-e: extra, -8: 8080 stub; default Z80)
  lzpack -R <file>         restore (decompress)
  lzpack -L <file>         list stored sizes
  lzpack -o <name>         set output name
```

* [*The Worst 8080 Assembler Ever™*](stubasm.c) is also included, and used
during the build process.
