# `graphics/window/fonts/` — the face the OS window sets its text in

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

The OS window shows every pixel, so it sets text in a face it measures: a character's advance
and a line's height in pixels, which it offers with its room so every owner places its text by
them. SDL's own debug face is eight pixels of a debugging letterform; this one was drawn for
reading code at small sizes. Its advance is uniform at every point size measured, so a row of
it holds exactly as many characters as its width over the advance, which is how every owner
counts them.

## How the runtime gets it

Embedded, not staged and not discovered. `graphics/CMakeLists.txt` turns these bytes into a C++
array at build time (`cmake/EmbedBinary.cmake`) and compiles it into `zengine-window-sdl`,
which opens it with `TTF_OpenFontIO(SDL_IOFromConstMem(...))` when it takes its window and
measures it there. A face that will not open says so on standard error, and the window sets its
text in SDL's debug face, measured as SDL states that face's character. See
[`../window_sdl_face.hpp`](../window_sdl_face.hpp).

## Obligations this repository carries

The OFL requires the copyright notice and the licence to travel with the Font Software,
including where it is bundled inside other software. `OFL.txt` beside the font is that, and
[`../../../THIRD_PARTY_NOTICES.md`](../../../THIRD_PARTY_NOTICES.md) records the row. The font is
**not** first-party material and is excluded from this repository's MPL-2.0 claim, per
[`../../../LICENSING.md`](../../../LICENSING.md).
