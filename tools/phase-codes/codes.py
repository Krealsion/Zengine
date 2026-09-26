# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# What a plan code is, in a case name and in a document, and what a bare label is: the grammar
# the census, the applier's proof and the lane check share. tests/check_law_register.cmake states
# the same grammar in CMake; the census compares the two answers over the whole tree.

import re

# An id-shaped token: upper-case parts joined by dashes, the last one maybe followed by one
# lower-case letter (`BLD-1a`). Bounded by anything that is not a letter, digit or underscore,
# so the token inside `pre-WUX-12` and `SEM-0's` is found.
TOKEN = re.compile(r"(?<![A-Za-z0-9_])[A-Z][A-Z0-9]*(?:-[A-Z0-9]+)+[a-z]?(?![A-Za-z0-9_])")
# Families a name or a document may cite: this repository's law registers and Loom's published
# law families. A cited law has a two-digit number; a family's letters with one digit are a
# phase tag.
LAW_FAMILIES = ("WL", "MW", "VM", "TIMER", "ANS", "GATE", "HANDOFF", "KERN", "LIFE", "MSG",
                "POP", "PR", "SENSE")
# Standards and ordinary hyphenated words written in capitals for emphasis: shaped like a token,
# never a code.
NOT_CODES = ("UTF-8", "UTF-16", "UTF-32", "MPL-2", "FNV-1a", "SHA-1", "SHA-256", "ISO-8601",
             "HAND-WRITTEN", "WEAVE-ONLY", "PROVIDER-ONLY")


def is_law_id(token):
    family = token.split("-", 1)[0]
    return family in LAW_FAMILIES and re.search(r"-[0-9][0-9]+$", token) is not None


def case_codes(name):
    """The plan codes in a case name: every token that is neither a cited law nor a word."""
    return [t for t in TOKEN.findall(name) if t not in NOT_CODES and not is_law_id(t)]


def is_law_led(name):
    """A name that opens with a public law id and carries no code."""
    m = TOKEN.match(name)
    return m is not None and is_law_id(m.group(0)) and not case_codes(name)


def document_ids(text):
    """The phase ids in a document's text: numbered tokens (a digit in the last part, `EDIT-W1`
    and `ZOOM-P2` included) that are not cited laws and not standards. An unnumbered capitalised
    compound in prose is a word."""
    out = []
    for t in TOKEN.findall(text):
        if t in NOT_CODES or is_law_id(t):
            continue
        if re.search(r"-[A-Z]*[0-9][A-Z0-9]*[a-z]?$", t):
            out.append(t)
    return out


# A bare label: a name that opens with a lone letter or a number and then `:`, `.` or `)`, the
# way a plan's steps are lettered (`b: `, `2: `, `(a) `). A word the name is about (`sdl: `) is
# not one.
BARE_LABEL = re.compile(r"^\(?(?:[A-Za-z]|[0-9]+)[:.)] ")


def is_bare_label(name):
    return BARE_LABEL.match(name) is not None
