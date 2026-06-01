<!-- Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com> -->
<!-- SPDX-License-Identifier: MIT-0 -->
<!-- scspell-id: c0fa2810-585c-11f1-bf4f-80ee73e9b8e7 -->

# LZPACK

LZPACK is a CP/M‑80 (8080 and Z80) executable compressor.

It runs on 48K CP/M‑80, CP/M‑86, MS‑DOS, ELKS, UNIX, and other platforms.

```
LZPACK v0.99982 - 48K CP/M-80 (8080 and Z80) executable compressor
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
