#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Joshua DeMoss
#
# THE CHARACTER WIDTH TABLES -- which code points take no column and which take two, written as
# component/unicode_width.hpp from the Unicode data of the Python that runs it. The rule the
# tables serve is component/text.hpp's `character_columns`; component/docs/component.md says it.
#
#   no column   general categories Mn (a combining mark), Me (an enclosing mark) and Cf (a format
#               character: a joiner, a direction mark), but U+00AD, the soft hyphen, which shows;
#               and the Hangul jamo vowels and final consonants U+1160..U+11FF and U+D7B0..U+D7FF,
#               which join the syllable before them
#   two columns East Asian Width W (wide) or F (fullwidth), emoji shown as emoji among them; and
#               every code point of the planes and blocks the Unicode standard reserves as wide,
#               assigned or not (U+3400..U+4DBF, U+4E00..U+9FFF, U+F900..U+FAFF, U+20000..U+2FFFD,
#               U+30000..U+3FFFD), so a character newer than these tables still takes its two
#
# Every other code point takes one. Run it again when the Unicode version should move; the header
# names the version it was written from.
#
#   python3 tools/unicode-width.py component/unicode_width.hpp

import sys
import unicodedata

RESERVED_WIDE = [(0x3400, 0x4DBF), (0x4E00, 0x9FFF), (0xF900, 0xFAFF), (0x20000, 0x2FFFD),
                 (0x30000, 0x3FFFD)]
JOINING_JAMO = [(0x1160, 0x11FF), (0xD7B0, 0xD7FF)]


def width(cp):
    if any(a <= cp <= b for a, b in JOINING_JAMO):
        return 0
    ch = chr(cp)
    cat = unicodedata.category(ch)
    if cp != 0x00AD and cat in ('Mn', 'Me', 'Cf'):
        return 0
    if unicodedata.east_asian_width(ch) in ('W', 'F'):
        return 2
    if any(a <= cp <= b for a, b in RESERVED_WIDE):
        return 2
    return 1


def ranges(which):
    out = []
    start = None
    for cp in range(0x110000):
        if 0xD800 <= cp <= 0xDFFF:
            hit = False
        else:
            hit = width(cp) == which
        if hit and start is None:
            start = cp
        elif not hit and start is not None:
            out.append((start, cp - 1))
            start = None
    if start is not None:
        out.append((start, 0x10FFFF))
    return out


def table(name, rs):
    lines = [f'inline constexpr CodeRange {name}[] = {{']
    row = []
    for a, b in rs:
        row.append(f'{{0x{a:05X}, 0x{b:05X}}},')
        if len(row) == 4:
            lines.append('    ' + ' '.join(row))
            row = []
    if row:
        lines.append('    ' + ' '.join(row))
    lines.append('};')
    return '\n'.join(lines)


def main():
    if len(sys.argv) != 2:
        sys.exit('usage: unicode-width.py <component/unicode_width.hpp>')
    zero = ranges(0)
    wide = ranges(2)
    text = f'''// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// Written by tools/unicode-width.py from Unicode {unicodedata.unidata_version}: run it again, never
// edit this file. Which code points take no column and which take two; `character_columns`
// (component/text.hpp) reads them, and every other code point takes one.

#ifndef ZENGINE_COMPONENT_UNICODE_WIDTH_HPP
#define ZENGINE_COMPONENT_UNICODE_WIDTH_HPP

#include <cstdint>

namespace zengine::component::unicode_width {{

/// The Unicode version these tables were written from.
inline constexpr const char* kUnicodeVersion = "{unicodedata.unidata_version}";

/// The code points from `first` to `last`, both included.
struct CodeRange {{
    std::uint32_t first;
    std::uint32_t last;
}};

/// The code points that take no column: marks, format characters and joining jamo.
{table('kNone', zero)}

/// The code points that take two columns.
{table('kTwo', wide)}

}} // namespace zengine::component::unicode_width

#endif // ZENGINE_COMPONENT_UNICODE_WIDTH_HPP
'''
    with open(sys.argv[1], 'w', newline='\n', encoding='utf-8') as f:
        f.write(text)
    print(f'unicode-width: {len(zero)} ranges of none, {len(wide)} of two, '
          f'Unicode {unicodedata.unidata_version}')


main()
