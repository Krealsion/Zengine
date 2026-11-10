# Third-party notices

Bundled third-party material actually present in this repository:

| Component | Location | Copyright | License | License text |
|---|---|---|---|---|
| doctest | `tests/third_party/doctest.h` | Copyright (c) 2016-2023 Viktor Kirilov | MIT | stated in the file's own header; canonical text at <https://opensource.org/licenses/MIT> |
| JetBrains Mono 2.304, Regular and Bold | `graphics/window/fonts/JetBrainsMono-Regular.ttf`, `JetBrainsMono-Bold.ttf` | Copyright 2020 The JetBrains Mono Project Authors | SIL Open Font License 1.1 (no Reserved Font Name declared) | `graphics/window/fonts/OFL.txt`, verbatim from upstream |
| GNU Unifont and Unifont Upper 17.0.05 | `graphics/window/fonts/Unifont-17.0.05.otf`, `UnifontUpper-17.0.05.otf` | Copyright (C) 1998-2026 Roman Czyborra, Paul Hardy, Qianqian Fang, Andrew Miller, Johnnie Weaver, David Corbett, Élla Chiana Moskopp, Rebecca Bettencourt, Ho-Seok Ee, et al. | dual: SIL Open Font License 1.1, or GNU GPL 2 or later with the GNU Font Embedding Exception; taken here under the OFL 1.1 | `graphics/window/fonts/OFL-Unifont.txt` |
| Airstream font | `quarry/Resources/TTFs/Airstream.ttf` | Nick's Fonts | Nick's Fonts freeware EULA | `quarry/Resources/TTFs/Nick's Fonts License.txt` |
| Elounda Regular 1.000 | `quarry/Resources/TTFs/Elounda-Regular.otf` | Copyright (c) 2024, Silverblur_type | SIL Open Font License 1.1 (no Reserved Font Name declared) | `quarry/Resources/TTFs/OFL-Elounda.txt`, the author's license file verbatim |

Elounda Regular is quarry material, excluded from this repository's first-party
AGPL-3.0-only claim. The file is unmodified: SHA-256
`7a4135312b6e4b085a8bb830f9e28af9b0b835c0cf879f51f8bee47f4b5fdc3e`, 43464 bytes,
identical to the regular face at commit `fbe57c50f74a7230b4cc67524a4d2e4c9c89ede2`
(2024-04-23) of <https://github.com/silverblurtype/elounda>. The license file in
that commit, still the license at the root of that repository, is the SIL Open
Font License 1.1 with the copyright line above and no Reserved Font Name;
`quarry/Resources/TTFs/OFL-Elounda.txt` is that text. The font's name table
names the designer Valerio Brotto (Silverblur_type).

The Basic face is not in this tree. The copy the quarry held matched no file
Sorkin Type Co publishes (<https://github.com/SorkinType/Basic>, every revision
of its TrueType file) and no file Google Fonts publishes for the family
(<https://github.com/google/fonts>), so its terms were not verified from an
authoritative source and the file was removed.
[LICENSING.md](LICENSING.md) records the same.

**JetBrains Mono and GNU Unifont are different in kind from the quarry fonts, and deliberately
so.** They are the faces the GUI window sets its text in -- Unifont the last resort for every
character JetBrains Mono lacks -- they are unmodified upstream bytes, their provenance is
written down in full beside them
([graphics/window/fonts/PROVENANCE.md](graphics/window/fonts/PROVENANCE.md) — upstream
release, SHA-256, size), and they are **distributed by this repository**: the build compiles
their bytes into `zengine-window-sdl`, so a copy of that weave carries a copy of each font.
The OFL permits exactly that and asks that the copyright notice and the licence travel with
them; the `OFL.txt` and `OFL-Unifont.txt` beside them are that, and these rows are the notice.

Not bundled, fetched at build time only (and therefore not distributed by
this repository) — all three used unless `ZENGINE_TUI_ONLY=ON`, all three fetched
via CMake FetchContent against a pinned URL and a SHA-256, each accompanied by
its own license in its own distribution:

| Component | Version | License |
|---|---|---|
| SDL3 | 3.4.12 | Zlib |
| SDL_ttf | 3.2.2 | Zlib |
| FreeType | 2.14.3 | FreeType License (BSD-style with attribution) or GPLv2, at the recipient's choice — SDL_ttf builds it as SDL_ttf's vendored dependency |

A binary built from this repository with SDL (any build but `ZENGINE_TUI_ONLY`) therefore links
FreeType, whose license asks that its use be acknowledged in the documentation of
such a binary. This paragraph is that acknowledgement for anything built here.
