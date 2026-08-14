# `surface/fonts/` — the face the graphical Skin sets its text in

One file, one licence, one reason.

| | |
|---|---|
| File | `JetBrainsMono-Regular.ttf` |
| Family | JetBrains Mono, Regular |
| Version | 2.304 |
| Upstream | <https://github.com/JetBrains/JetBrainsMono>, release `v2.304`, asset `JetBrainsMono-2.304.zip`, member `fonts/ttf/JetBrainsMono-Regular.ttf` |
| SHA-256 | `a0bf60ef0f83c5ed4d7a75d45838548b1f6873372dfac88f71804491898d138f` |
| Size | 273,900 bytes |
| Copyright | Copyright 2020 The JetBrains Mono Project Authors |
| Licence | SIL Open Font License 1.1 — the upstream text, verbatim, in [OFL.txt](OFL.txt) |
| Reserved Font Name | **none declared** — the upstream copyright line carries no "with Reserved Font Name" clause |
| Modified here | **no.** The bytes are upstream's, unaltered, and `.gitattributes` marks `*.ttf` as `-text` so nothing normalizes them on a Windows checkout. |

## Why this face

HD-0 measured the defect precisely: the graphical Skin's own 5×5 bitmap letterform
makes `a`, `e`, `o` and `c` differ by one pixel, so `weave` reads `woave`, and probe 2
measured that **scaling that face does not fix it** — it is the letterform, not the
size. HD-1 needed a real one.

JetBrains Mono was chosen over the other permissively-licensed candidate measured
(DejaVu Sans Mono 2.37, Bitstream Vera licence) on three grounds, in order:

1. **Its advance is exactly uniform at every point size measured** (9–20 pt, through
   FreeType 2.14.3). DejaVu's is not: at 9 pt `M` measured 51 px per ten characters
   and `i` measured 50. An application that computes its columns as
   `usable_width / advance` — which is exactly what `fit_region` does, and the whole
   reason the metric travels up — is safer on a face whose advance has no rounding
   seam in it at all.
2. It is smaller: 273,900 bytes against 340,712.
3. It was drawn for reading code at small sizes, which is the reading this pane is.

## How the runtime gets it

Embedded, not staged and not discovered. `surface/CMakeLists.txt` turns these bytes
into a C++ array at build time (`cmake/EmbedBinary.cmake`) and compiles it into
`zengine-skin-sdl`, which opens it with `TTF_OpenFontIO(SDL_IOFromConstMem(...))`.

There is no file to install, no path to resolve, no directory to search, and nothing
that can half-arrive: a Skin either has its face or does not, and if it does not it
says so on stderr and falls back to the bitmap one. See
[`../skin_sdl_text.hpp`](../skin_sdl_text.hpp).

A system font was considered and refused for the reason this house names as silent:
`consola.ttf` exists on Windows and a headless Linux box may have no font at all, so
"readable on the machine that wrote it" would have been the failure mode.

## Obligations this repository carries

The OFL requires the copyright notice and the licence to travel with the Font
Software, including where it is bundled inside other software. `OFL.txt` beside the
font is that, and [`../../THIRD_PARTY_NOTICES.md`](../../THIRD_PARTY_NOTICES.md)
records the row. The font is **not** first-party material and is excluded from this
repository's MPL-2.0 claim, per [`../../LICENSING.md`](../../LICENSING.md).
