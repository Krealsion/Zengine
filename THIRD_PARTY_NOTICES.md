# Third-party notices

Bundled third-party material actually present in this repository:

| Component | Location | Copyright | License | License text |
|---|---|---|---|---|
| doctest | `tests/third_party/doctest.h` | Copyright (c) 2016-2023 Viktor Kirilov | MIT | stated in the file's own header; canonical text at <https://opensource.org/licenses/MIT> |
| JetBrains Mono 2.304, Regular | `surface/fonts/JetBrainsMono-Regular.ttf` | Copyright 2020 The JetBrains Mono Project Authors | SIL Open Font License 1.1 (no Reserved Font Name declared) | `surface/fonts/OFL.txt`, verbatim from upstream |
| Airstream font | `reference/Resources/TTFs/Airstream.ttf` | Nick's Fonts | Nick's Fonts freeware EULA | `reference/Resources/TTFs/Nick's Fonts License.txt` |
| Basic font | `reference/Resources/TTFs/Basic-Regular.ttf` | its original authors (unverified) | no adjacent notice; terms unverified | — |
| Elounda font | `reference/Resources/TTFs/Elounda-Regular.otf` | its original authors (unverified) | no adjacent notice; terms unverified | — |

The two unattributed fonts are quarry material from the prior V1 engine tree;
they are excluded from the repository's first-party MPL-2.0 claim, and their
provenance is recorded as unresolved in [LICENSING.md](LICENSING.md).

**JetBrains Mono is different in kind from those three, and deliberately so.** It
is the face the graphical Skin sets its text in, it is unmodified upstream bytes,
its provenance is written down in full beside it
([surface/fonts/PROVENANCE.md](surface/fonts/PROVENANCE.md) — upstream release,
SHA-256, size), and it is **distributed by this repository**: the build compiles
its bytes into `zengine-skin-sdl`, so a copy of that weave carries a copy of the
font. The OFL permits exactly that and asks that the copyright notice and the
licence travel with it; `surface/fonts/OFL.txt` is that, and this row is the
notice.

Not bundled, fetched at build time only (and therefore not distributed by
this repository) — all three used when `ZENGINE_SDL_SKIN=ON`, all three fetched
via CMake FetchContent against a pinned URL and a SHA-256, each accompanied by
its own license in its own distribution:

| Component | Version | License |
|---|---|---|
| SDL3 | 3.4.12 | Zlib |
| SDL_ttf | 3.2.2 | Zlib |
| FreeType | 2.14.3 | FreeType License (BSD-style with attribution) or GPLv2, at the recipient's choice — SDL_ttf builds it as SDL_ttf's vendored dependency |

A binary built from this repository with the SDL skin enabled therefore links
FreeType, whose license asks that its use be acknowledged in the documentation of
such a binary. This paragraph is that acknowledgement for anything built here.
