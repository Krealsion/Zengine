# Licensing

Copyright (c) 2026 Joshua DeMoss.

The first-party software in this repository is licensed under the Mozilla
Public License Version 2.0 (MPL-2.0), unless a file or directory states
otherwise.

See [LICENSE](LICENSE) for the complete license text.

## What this means

You may use this software for personal, open-source, commercial, educational,
and other purposes under the terms of MPL-2.0.

MPL-2.0 applies copyleft at the file level. If you distribute modifications to
MPL-covered files, those covered files remain available under MPL-2.0.

Independent software that merely uses or combines with this software is not
required to adopt MPL-2.0 solely because it uses Loom or Zengine. Your weaves,
packages, games, and products built with Zengine are yours.

In plain language:

    What you create with Zen is yours.

This is the legal mechanism beneath the standing principle — *the Loom is
everyone's, Zengine is the default set, your weaves are yours*: Zengine is
created and owned by Joshua DeMoss; Joshua chooses to license its open core
under MPL-2.0; using it never surrenders ownership of the independent things
you create with it.

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
contrary notices) and falls under this repository's MPL-2.0 statement, but as
frozen reference material it does not carry per-file headers.

Its `Resources/TTFs/` directory contains third-party fonts that are **not**
first-party and **not** MPL-licensed: `Airstream.ttf` is covered by the
adjacent `Nick's Fonts License.txt`; `Basic-Regular.ttf` and
`Elounda-Regular.otf` carry no adjacent notice and their exact terms are
unverified — they are excluded from any ownership claim here and remain under
their original authors' terms.
