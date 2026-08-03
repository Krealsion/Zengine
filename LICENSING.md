# Licensing

Copyright (c) 2026 Joshua DeMoss.

The first-party software in this repository is licensed under the GNU Affero
General Public License, version 3 only (AGPL-3.0-only), unless a file or
directory states otherwise.

See [LICENSE](LICENSE) for the complete license text.

## What this means

You may use, study, change and share this software under the terms of
AGPL-3.0-only. If you share it, or a work based on it, you share it under
those terms, with its source. If you change it and let others use it over a
network, you offer them its source too.

A creations exception is being drafted: an additional permission meant to
keep what you create with Zen yours. Until it is published there is none, and
building on Zen means the AGPL's terms.

This summary is explanatory only. LICENSE is authoritative.

## Ownership

The copyright holder for first-party material identified as such is Joshua
DeMoss.

"Unknown Works" / unknownworks.net is currently a project brand and domain,
not the copyright owner and not an LLC.

## Third-party material

Files or directories carrying their own license or copyright notice remain
under those terms. Bundled third-party material is listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md); today that is the doctest
test framework and the font files inside the reference quarry (below). SDL,
when the SDL skin is enabled, is fetched at build time and is not distributed
in this repository.

## `reference/` — the quarry

`reference/` is the read-only quarry imported from the prior V1 Zen engine
working tree. Its C++ source is first-party (single-author history, no
contrary notices) and falls under this repository's AGPL-3.0-only statement, but as
frozen reference material it does not carry per-file headers.

Its `Resources/TTFs/` directory contains third-party fonts that are **not**
first-party and **not** AGPL-licensed: `Airstream.ttf` is covered by the
adjacent `Nick's Fonts License.txt`; `Basic-Regular.ttf` and
`Elounda-Regular.otf` carry no adjacent notice and their exact terms are
unverified — they are excluded from any ownership claim here and remain under
their original authors' terms.
