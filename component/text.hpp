// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_COMPONENT_TEXT_HPP
#define ZENGINE_COMPONENT_TEXT_HPP

// The core the text pieces share: what a character and a word are, a selection as two places in a
// text, and a copy of what lies between them. The text box (`text_box.hpp`) edits one line over
// it and the text area (`text_area.hpp`) shows long text, read only, over it. No medium, no
// policy: a place is a byte between two characters, and every rule here keeps one on a boundary.
// Reference: component/docs/component.md.
//
// The unit is a byte and the steps are characters: every presentation counts one column per
// byte, so a place is a byte index, and the operations never stop inside a UTF-8 character.
// Not Unicode width, grapheme clusters, combining marks or double-width glyphs.

#include <cstddef>
#include <cstdint>
#include <string>

namespace zengine::component {

// ---- What a character is, in this application -------------------------------------------
//
// Free functions over the string, beside their main consumer, so anything that needs to know
// where a character starts asks here rather than walking UTF-8 for itself.

/// IS THIS BYTE THE MIDDLE OF A CHARACTER? UTF-8 continuation bytes are 10xxxxxx, and this
/// one predicate is the whole of what "not a character boundary" means in this application.
inline bool is_continuation_byte(char b) noexcept {
    return (static_cast<unsigned char>(b) & 0xC0u) == 0x80u;
}

/// THE BYTE INDEX ONE CHARACTER BEFORE `at`, and 0 when there is none.
///
/// A line a weaver types is a byte string, so stepping back one BYTE over `é` would land
/// between its two bytes -- a position that is not anywhere in the text, and from which an
/// erase leaves half a character behind. Continuation bytes go with their lead byte.
inline std::size_t character_before(const std::string& line, std::size_t at) noexcept {
    std::size_t i = at < line.size() ? at : line.size();
    while (i > 0 && is_continuation_byte(line[i - 1])) {
        --i;
    }
    return i > 0 ? i - 1 : 0;
}

/// THE BYTE INDEX ONE CHARACTER AFTER `at`, and `line.size()` when there is none. The other
/// direction of `character_before`, same rule, same reason.
inline std::size_t character_after(const std::string& line, std::size_t at) noexcept {
    std::size_t i = at;
    if (i >= line.size()) {
        return line.size();
    }
    ++i;
    while (i < line.size() && is_continuation_byte(line[i])) {
        ++i;
    }
    return i;
}

/// THE NEAREST CHARACTER BOUNDARY AT OR BEFORE `at`, clamped into the line.
///
/// The one place a position that came from OUTSIDE the text -- a pointer press resolved to a
/// column -- is made into a position the text actually has. Snapping backwards rather than
/// forwards is what makes a press on the second byte of a two-byte character mean the
/// character it landed on rather than the one after it.
inline std::size_t character_boundary(const std::string& line, std::size_t at) noexcept {
    std::size_t i = at < line.size() ? at : line.size();
    while (i > 0 && is_continuation_byte(line[i])) {
        --i;
    }
    return i;
}

/// The nearest character boundary at or after `at`, clamped into the line: the direction a
/// viewport needs. A horizontally scrolled line must not begin mid-character, and snapping the
/// first visible byte backwards would move the window's right edge back with it and could push
/// the caret off its row; snapping forwards only shortens the window at the left. It cannot
/// overshoot a caret, which is always on a boundary, so the two rules compose in either order.
inline std::size_t character_boundary_at_or_after(const std::string& line,
                                                  std::size_t at) noexcept {
    std::size_t i = at < line.size() ? at : line.size();
    while (i < line.size() && is_continuation_byte(line[i])) {
        ++i;
    }
    return i;
}

// ---- What a word is, in this application ------------------------------------------------
//
// A word is a maximal run of non-space bytes, and the one separator is the space: a shell's
// word -- no identifier or punctuation class, no locale, no Unicode categories. Every answer
// lands on a character boundary by construction. The run is the primitive: `word_before` and
// `word_after` add the separator walk a keyboard gesture means, and `word_at` is the two scans
// meeting at one position, which is what a pointer means -- one definition of a word.

/// THE START OF THE RUN OF WORD BYTES REACHING BACK FROM `at`, or `at` itself when the byte
/// behind it is a separator. A position is BETWEEN bytes, so a leftward scan reads what is
/// behind it and never `at` itself.
inline std::size_t word_run_begin(const std::string& line, std::size_t at) noexcept {
    std::size_t i = at < line.size() ? at : line.size();
    while (i > 0 && line[i - 1] != ' ') {
        --i;
    }
    return i;
}

/// THE END OF THE RUN OF WORD BYTES REACHING FORWARD FROM `at`, or `at` itself when the byte
/// at it is a separator — the same between-the-bytes rule read the other way.
inline std::size_t word_run_end(const std::string& line, std::size_t at) noexcept {
    std::size_t i = at < line.size() ? at : line.size();
    while (i < line.size() && line[i] != ' ') {
        ++i;
    }
    return i;
}

/// THE START OF THE WORD BEFORE `at`: back over spaces, then back over the word.
inline std::size_t word_before(const std::string& line, std::size_t at) noexcept {
    std::size_t i = at < line.size() ? at : line.size();
    while (i > 0 && line[i - 1] == ' ') {
        --i;
    }
    return word_run_begin(line, i);
}

/// THE START OF THE WORD AFTER `at`: forward over the word, then over the spaces — so
/// repeated presses walk word starts, which is the gesture's conventional meaning, and the
/// last press lands at the end of the line.
inline std::size_t word_after(const std::string& line, std::size_t at) noexcept {
    std::size_t i = word_run_end(line, at);
    while (i < line.size() && line[i] == ' ') {
        ++i;
    }
    return i;
}

/// Which bytes a position's word occupies, begin inclusive, end exclusive. Empty is an answer:
/// a position with a separator on both sides is in no word, and nothing invents a nearest one.
/// A position on either edge of a run belongs to that run, including the end of the text.
struct WordSpan {
    std::size_t begin = 0;
    std::size_t end = 0; ///< exclusive
    bool present() const noexcept { return end > begin; }
};

inline WordSpan word_at(const std::string& line, std::size_t at) noexcept {
    return WordSpan{word_run_begin(line, at), word_run_end(line, at)};
}

// ---- What foreign text becomes in a one-line box ----------------------------------------

/// A clipboard's bytes as one line. Typed text never carries a control byte, but a clipboard is
/// foreign bytes, so the one door they enter a single-line box through flattens them: a CRLF
/// pair becomes one space, and every other byte below 0x20 (a lone CR or LF, a tab) and 0x7F
/// one space each -- a space rather than deletion, which would join two lines into one word, and
/// rather than a marker the weaver did not paste. Tabs too, since each column is one byte.
inline std::string pasteable_line(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char b = static_cast<unsigned char>(text[i]);
        if (b == '\r' && i + 1 < text.size() && text[i + 1] == '\n') {
            out.push_back(' ');
            ++i;
            continue;
        }
        if (b < 0x20u || b == 0x7Fu) {
            out.push_back(' ');
            continue;
        }
        out.push_back(text[i]);
    }
    return out;
}

// ---- The clipboard, as a value the owner holds ------------------------------------------

/// Text a weaver copied, and counts of what this process did with the clipboard: a plain value,
/// since this component links nothing, and where its text is bridged (a platform clipboard, a
/// bus message, nowhere) is the owner's custody. `writes` bumps exactly when `copy` or `cut`
/// took text, so an owner notices a copy with one comparison; mirroring a copy heard elsewhere
/// updates `text` only. `paste_requests` bumps on Ctrl+V instead of pasting: the value a paste
/// means is the clipboard's current one, which only the owner can obtain and apply (`paste`).
struct Clipboard {
    std::string text;
    std::uint64_t writes = 0;
    std::uint64_t paste_requests = 0;
};

// ---- The editing-key vocabulary's identities --------------------------------------------
//
// The numbers `input::scan::` and `input::mod::` name, spelled locally because this component
// includes nothing; the component suite static_asserts every value against them. Only the keys
// a piece's `consume` binds are named: declining a key is `default:`, not knowledge.

namespace key {
inline constexpr std::int64_t kA = 4;
inline constexpr std::int64_t kC = 6;
inline constexpr std::int64_t kV = 25;
inline constexpr std::int64_t kX = 27;
inline constexpr std::int64_t kY = 28;
inline constexpr std::int64_t kZ = 29;
inline constexpr std::int64_t kBackspace = 42;
inline constexpr std::int64_t kHome = 74;
inline constexpr std::int64_t kDelete = 76;
inline constexpr std::int64_t kEnd = 77;
inline constexpr std::int64_t kRight = 79;
inline constexpr std::int64_t kLeft = 80;
} // namespace key

namespace mod {
inline constexpr std::int64_t kNone = 0;
inline constexpr std::int64_t kShift = 1;
inline constexpr std::int64_t kCtrl = 2;
inline constexpr std::int64_t kAlt = 4;
inline constexpr std::int64_t kSuper = 8;
} // namespace mod

// ---- Places in a text, and the selection between two of them ------------------------------

/// A SELECTION AS TWO PLACES IN A TEXT: the caret, where the next gesture acts, and the anchor, the
/// selection's other end -- no selection is the two equal, so there is no flag to fall out of
/// step. A place is a byte between two characters, so a text laid out again (wrapped at another
/// width, scrolled) keeps both: neither is a row or a pixel.
struct TextPlaces {
    std::size_t anchor = 0;
    std::size_t caret = 0;

    bool has_selection() const noexcept { return anchor != caret; }
    /// The selected range in reading order, begin inclusive and end exclusive. Which end the caret
    /// is at stays a fact of its own, so a leftward and a rightward selection are two states.
    std::size_t begin() const noexcept { return caret < anchor ? caret : anchor; }
    std::size_t end() const noexcept { return caret > anchor ? caret : anchor; }
    /// Both places held inside `text`, each snapped back to the character it stands in.
    void settle(const std::string& text) noexcept {
        anchor = character_boundary(text, anchor);
        caret = character_boundary(text, caret);
    }
    bool operator==(const TextPlaces&) const = default;
};

/// THE TEXT BETWEEN TWO PLACES, AS WRITTEN: the bytes of `text` the selection covers, empty when
/// nothing is selected. What a presentation added to show it -- a wrap's break, its indent, a
/// marker -- is not in `text`, so it is never in a copy.
inline std::string selected_text(const std::string& text, const TextPlaces& places) {
    const std::size_t to = places.end() < text.size() ? places.end() : text.size();
    const std::size_t from = places.begin() < to ? places.begin() : to;
    return text.substr(from, to - from);
}

/// COPY THE SELECTION INTO THE OWNER'S CLIPBOARD. With nothing selected nothing happens: the
/// clipboard a weaver filled a minute ago is not overwritten with an empty string by a stray chord.
inline void copy_selection(const std::string& text, const TextPlaces& places, Clipboard& clip) {
    if (!places.has_selection()) {
        return;
    }
    clip.text = selected_text(text, places);
    ++clip.writes;
}

} // namespace zengine::component

#endif // ZENGINE_COMPONENT_TEXT_HPP
