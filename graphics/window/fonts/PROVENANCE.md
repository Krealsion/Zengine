# `graphics/window/fonts/` — the faces the GUI window sets its text in

Four files, two licences' texts, one reason: every character drawn.

| | |
|---|---|
| Files | `JetBrainsMono-Regular.ttf`, `JetBrainsMono-Bold.ttf` |
| Family | JetBrains Mono, Regular and Bold |
| Version | 2.304 |
| Upstream | <https://github.com/JetBrains/JetBrainsMono>, release `v2.304`, asset `JetBrainsMono-2.304.zip`, members `fonts/ttf/JetBrainsMono-Regular.ttf` and `fonts/ttf/JetBrainsMono-Bold.ttf` |
| SHA-256 | Regular `a0bf60ef0f83c5ed4d7a75d45838548b1f6873372dfac88f71804491898d138f`; Bold `5590990c82e097397517f275f430af4546e1c45cff408bde4255dad142479dcb` |
| Size | Regular 273,900 bytes; Bold 277,828 bytes |
| Copyright | Copyright 2020 The JetBrains Mono Project Authors |
| Licence | SIL Open Font License 1.1 — the upstream text, verbatim, in [OFL.txt](OFL.txt) |
| Reserved Font Name | **none declared** — the upstream copyright line carries no "with Reserved Font Name" clause |
| Modified here | **no.** The bytes are upstream's, unaltered; each file's name table says version 2.304. |

| | |
|---|---|
| Files | `Unifont-17.0.05.otf`, `UnifontUpper-17.0.05.otf` |
| Family | GNU Unifont and Unifont Upper, Regular |
| Version | 17.0.05 |
| Upstream | <https://unifoundry.com/pub/unifont/unifont-17.0.05/font-builds/>, files `unifont-17.0.05.otf` and `unifont_upper-17.0.05.otf`, each with its detached signature (`.sig`) |
| SHA-256 | Unifont `85701ab9b1e251ee16f4df00b13f22eac311d72b7dab427a7d975fe7f5064702`; Upper `f4fd6d5d752726d384feef175bb780c9f29382cd4941c9e1e6990d7c3822a090` |
| Size | Unifont 5,321,628 bytes; Upper 6,138,788 bytes |
| Copyright | Copyright (C) 1998-2026 Roman Czyborra, Paul Hardy, Qianqian Fang, Andrew Miller, Johnnie Weaver, David Corbett, Élla Chiana Moskopp, Rebecca Bettencourt, Ho-Seok Ee, et al. |
| Licence | dual: the SIL Open Font License 1.1, or the GNU GPL 2 or later with the GNU Font Embedding Exception, as each file's name table says; taken here under the OFL 1.1, its text with Unifont's copyright in [OFL-Unifont.txt](OFL-Unifont.txt) |
| Modified here | **no.** The bytes are upstream's, unaltered, renamed only. |

`.gitattributes` marks `*.ttf` and `*.otf` as `-text`, so nothing normalizes them on a Windows
checkout.

## Why these faces

The GUI window shows every pixel, so it sets text in faces it measures: a column's advance and a
line's height in pixels at each size, which it offers with its room so every owner places its
text by them. JetBrains Mono was drawn for reading code at small sizes, and its advance is uniform
at every point size measured, so a row holds exactly as many columns as its width over the
advance; its Bold shares that advance. It holds Latin, Greek and Cyrillic, not the scripts past
them, so a character it lacks is drawn from the platform's own fonts and, where none holds it,
from Unifont, which draws nearly every character of the Basic Multilingual Plane, and Unifont
Upper, which draws the planes past it (emoji among them): every character is drawn somewhere, in
its own columns.

## How the runtime gets them

Embedded, not staged and not discovered. `graphics/CMakeLists.txt` turns each face's bytes into a
C++ array at build time (`cmake/EmbedBinary.cmake`) and compiles them into `zengine-window-sdl`,
which opens them with `TTF_OpenFontIO(SDL_IOFromConstMem(...))` as each size is first asked. A
face that will not open says so on standard error, and the window sets its text in SDL's debug
face, measured as SDL states that face's character. See
[`../window_sdl_face.hpp`](../window_sdl_face.hpp).

## Obligations this repository carries

The OFL requires the copyright notice and the licence to travel with the Font Software,
including where it is bundled inside other software. `OFL.txt` and `OFL-Unifont.txt` beside the
fonts are that, the installed package carries both, and
[`../../../THIRD_PARTY_NOTICES.md`](../../../THIRD_PARTY_NOTICES.md) records the rows. The fonts
are **not** first-party material and are excluded from this repository's AGPL-3.0-only claim, per
[`../../../LICENSING.md`](../../../LICENSING.md).
