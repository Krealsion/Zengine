# Third-party notices

Bundled third-party material actually present in this repository:

| Component | Location | Copyright | License | License text |
|---|---|---|---|---|
| doctest | `tests/third_party/doctest.h` | Copyright (c) 2016-2023 Viktor Kirilov | MIT | stated in the file's own header; canonical text at <https://opensource.org/licenses/MIT> |
| Airstream font | `reference/Resources/TTFs/Airstream.ttf` | Nick's Fonts | Nick's Fonts freeware EULA | `reference/Resources/TTFs/Nick's Fonts License.txt` |
| Basic font | `reference/Resources/TTFs/Basic-Regular.ttf` | its original authors (unverified) | no adjacent notice; terms unverified | — |
| Elounda font | `reference/Resources/TTFs/Elounda-Regular.otf` | its original authors (unverified) | no adjacent notice; terms unverified | — |

The two unattributed fonts are quarry material from the prior V1 engine tree;
they are excluded from the repository's first-party MPL-2.0 claim, and their
provenance is recorded as unresolved in [LICENSING.md](LICENSING.md).

Not bundled, fetched at build time only (and therefore not distributed by
this repository): the pinned static SDL3 used when `ZENGINE_SDL_SKIN=ON`
(zlib license, fetched via CMake FetchContent with a checksum). Its license
accompanies its own distribution.
