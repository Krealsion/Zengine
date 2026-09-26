// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_COMPONENT_TEXT_BOX_HPP
#define ZENGINE_COMPONENT_TEXT_BOX_HPP

// Editable text with a caret, a selection and a window onto it: the component a single-line
// draft is edited with, and the character and word rules it is built from. It owns no policy
// and no medium -- no SDL, terminal, cell, pixel or font metric; no commit, validation, focus,
// blink or drawing -- and is not an entity: a member of whatever owns it, dying with it. The
// capacity is always an argument, and the `Clipboard` it operates on is the owner's.
// Reference: docs/reference/component.md.
//
// The unit is a byte and the steps are characters: every presentation counts one column per
// byte, so a caret is a byte index, and the operations never stop inside a UTF-8 character.
// Not Unicode width, grapheme clusters, combining marks or double-width glyphs.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

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
/// A line a maker types is a byte string, so stepping back one BYTE over `é` would land
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
/// rather than a marker the maker did not paste. Tabs too, since each column is one byte.
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

/// Text a maker copied, and counts of what this process did with the clipboard: a plain value,
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
// `consume` binds are named: declining a key is `default:`, not knowledge.

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

// ---- The editing vocabulary, as declaration rows ----------------------------------------
//
// Exactly the gestures `TextBox::consume` answers `true` to, one row per gesture, so a help
// surface can show the vocabulary without re-spelling it; the suite asserts table and
// `consume` agree in both directions. Not remappable (the executable truth is `consume`), and
// not a context, command id or file. Shift-transparent spellings follow the bare ones, so a
// bounded help surface loses the duplicates before a meaning.

/// One consumed editing gesture: the key, the EXACT modifiers, and its human meaning.
struct EditingGesture {
    std::int64_t scancode = 0;
    std::int64_t modifiers = 0;
    const char* label = "";
};

inline constexpr EditingGesture kEditingVocabulary[] = {
    {key::kC, mod::kCtrl, "copy"},
    {key::kX, mod::kCtrl, "cut"},
    {key::kV, mod::kCtrl, "paste"},
    {key::kA, mod::kCtrl, "select all"},
    {key::kZ, mod::kCtrl, "undo"},
    {key::kZ, mod::kCtrl | mod::kShift, "redo"},
    {key::kY, mod::kCtrl, "redo"},
    {key::kLeft, mod::kNone, "left"},
    {key::kRight, mod::kNone, "right"},
    {key::kLeft, mod::kShift, "select left"},
    {key::kRight, mod::kShift, "select right"},
    {key::kHome, mod::kNone, "start"},
    {key::kEnd, mod::kNone, "end"},
    {key::kHome, mod::kShift, "select to start"},
    {key::kEnd, mod::kShift, "select to end"},
    {key::kLeft, mod::kCtrl, "word left"},
    {key::kRight, mod::kCtrl, "word right"},
    {key::kLeft, mod::kCtrl | mod::kShift, "select word left"},
    {key::kRight, mod::kCtrl | mod::kShift, "select word right"},
    {key::kBackspace, mod::kNone, "erase left"},
    {key::kDelete, mod::kNone, "erase right"},
    {key::kBackspace, mod::kCtrl, "erase word left"},
    {key::kDelete, mod::kCtrl, "erase word right"},
    // -- the transparent and collapsed spellings: same meanings, said with a modifier the
    //    vocabulary deliberately ignores on these keys ---------------------------------
    {key::kBackspace, mod::kShift, "erase left"},
    {key::kDelete, mod::kShift, "erase right"},
    {key::kBackspace, mod::kCtrl | mod::kShift, "erase word left"},
    {key::kDelete, mod::kCtrl | mod::kShift, "erase word right"},
    {key::kHome, mod::kCtrl, "start"},
    {key::kEnd, mod::kCtrl, "end"},
    {key::kHome, mod::kCtrl | mod::kShift, "select to start"},
    {key::kEnd, mod::kCtrl | mod::kShift, "select to end"},
};

inline constexpr std::size_t kEditingVocabularyCount =
    sizeof(kEditingVocabulary) / sizeof(kEditingVocabulary[0]);

// ---- The component ----------------------------------------------------------------------

/// A line of editable text, the insertion point in it, the selection around that point, and
/// which part of it is on screen: four facts that move together, so the operations are the only
/// way any of them changes, and each re-establishes the invariant before it returns.
///
///     always, after every operation      0 <= first_visible <= caret <= size()
///                                        0 <= anchor <= size()
///                                        first_visible, caret and anchor are on
///                                        character boundaries
///
///     after keep_caret_visible(N)        caret - first_visible <= N
///                                        first_visible <= max(0, size() - N)
///
/// The selection is the anchor and the caret: no selection is `anchor == caret`, so there is no
/// flag to fall out of step. Undo is bounded local snapshots of {text, caret, anchor}, with
/// same-kind keystrokes coalescing (`remember`); `set` and `clear` open and close a draft and
/// wipe it, so a new draft never resurrects an old one's text. `first_visible` is component
/// state because no consumer shows one draft at two widths; one that does moves it out.
class TextBox {
public:
    const std::string& text() const noexcept { return text_; }
    std::size_t caret() const noexcept { return caret_; }
    bool empty() const noexcept { return text_.empty(); }
    std::size_t size() const noexcept { return text_.size(); }

    /// THE BYTE THE VISIBLE PART OF THE TEXT BEGINS AT. Zero for everything that fits, which
    /// is what makes a short line's presentation byte-for-byte what it was before there was
    /// a window at all.
    std::size_t first_visible() const noexcept { return first_; }

    /// Is the caret at the end? The one question about a caret that a CONSUMER's policy can
    /// legitimately turn on -- the Terminal's completer answers about the end of a line, so
    /// this is what decides whether it may be asked. The component itself draws no
    /// conclusion from it.
    bool at_end() const noexcept { return caret_ == text_.size(); }

    // ---- The selection, read ------------------------------------------------------------

    /// THE SELECTION'S OTHER END. Equal to the caret exactly when there is no selection.
    std::size_t anchor() const noexcept { return anchor_; }
    bool has_selection() const noexcept { return anchor_ != caret_; }

    /// The selected range in reading order — begin inclusive, end exclusive, byte indices of
    /// the whole text. Which end the CARET is at is a separate fact (`caret()`), and it is
    /// the anchor/caret split that keeps a leftward and a rightward selection two different
    /// states that show one range.
    std::size_t selection_begin() const noexcept {
        return caret_ < anchor_ ? caret_ : anchor_;
    }
    std::size_t selection_end() const noexcept {
        return caret_ > anchor_ ? caret_ : anchor_;
    }

    /// The selected bytes, empty when nothing is selected. Character-whole by construction:
    /// both ends of a selection are on character boundaries, always.
    std::string selected_text() const {
        return text_.substr(selection_begin(), selection_end() - selection_begin());
    }

    /// THE PART OF THE SELECTION A ROW OF `columns` COLUMNS IS SHOWING, in columns of the
    /// visible slice — the one answer both a painter and a hit test may spend, computed here
    /// because `selection - first_visible` written twice is two answers to one question, and
    /// the second one is right until the first line long enough to scroll (`caret_column`'s
    /// own argument, for a range). `end` may equal the capacity: a selection ending at the
    /// last visible character covers it, and covering is a span, not a position.
    struct VisibleSpan {
        std::int64_t begin = 0;
        std::int64_t end = 0; ///< exclusive; `end > begin` is what "some is visible" means
        bool present() const noexcept { return end > begin; }
    };
    VisibleSpan visible_selection(std::int64_t columns) const noexcept {
        if (!has_selection() || columns <= 0) {
            return VisibleSpan{};
        }
        const std::size_t room = static_cast<std::size_t>(columns);
        const std::size_t left = text_.size() - first_;
        const std::size_t shown_end = first_ + (left < room ? left : room);
        const std::size_t lo = selection_begin() > first_ ? selection_begin() : first_;
        const std::size_t hi = selection_end() < shown_end ? selection_end() : shown_end;
        if (lo >= hi) {
            return VisibleSpan{};
        }
        return VisibleSpan{static_cast<std::int64_t>(lo - first_),
                           static_cast<std::int64_t>(hi - first_)};
    }

    /// How many columns from the start of the visible slice the caret sits at: computed here so
    /// no consumer subtracts for itself. It may equal the capacity, since a caret is between
    /// characters and the position after the last visible one is a real column.
    std::size_t caret_column() const noexcept { return caret_ - first_; }

    /// The byte a column of the visible slice names, clamped into the text: `first_visible +
    /// column`, the pointer's arithmetic. A column at or before the start gives `first_visible`
    /// (what the maker could see); one past the last byte gives `size()`, the end of the whole
    /// text -- honest, since after `keep_caret_visible` blank room at the right means the text
    /// ended there. The result is not snapped to a character boundary; `place()` does that.
    std::size_t position_at_column(std::int64_t column) const noexcept {
        const std::size_t from = first_ < text_.size() ? first_ : text_.size();
        if (column <= 0) {
            return from;
        }
        const std::uint64_t ahead = static_cast<std::uint64_t>(column);
        return ahead < static_cast<std::uint64_t>(text_.size() - from)
                   ? from + static_cast<std::size_t>(ahead)
                   : text_.size();
    }

    /// The part of the text a row of `columns` columns is showing: a slice, never a mutation.
    /// The right-hand cut is a byte cut, as every presentation makes it; snapping it to a
    /// character boundary would shorten the row while the caret's column is computed from the
    /// window. The left edge is a position this class chooses; the right edge is a cut.
    std::string visible(std::int64_t columns) const {
        if (columns <= 0 || first_ >= text_.size()) {
            return {};
        }
        const std::size_t room = static_cast<std::size_t>(columns);
        const std::size_t left = text_.size() - first_;
        return text_.substr(first_, left < room ? left : room);
    }

    /// Move the window as little as it takes to see the caret, given the row's room: no
    /// animation, recentring or margin. The rules, in order: (1) no blank room on the right while
    /// text is hidden on the left, which is what makes deleting recover freed room; (2) the caret
    /// is not left of the window; (3) nor past its right; (4) the window does not begin inside a
    /// character. Afterwards `first_visible <= size() - columns` whenever the text is longer
    /// than the row, so blank room at the right means the text ended there.
    void keep_caret_visible(std::int64_t columns) noexcept {
        const std::size_t room = columns > 0 ? static_cast<std::size_t>(columns) : 0;
        const std::size_t furthest = text_.size() > room ? text_.size() - room : 0;
        if (first_ > furthest) {
            first_ = furthest;
        }
        if (first_ > caret_) {
            first_ = caret_;
        }
        if (caret_ - first_ > room) {
            first_ = caret_ - room;
        }
        first_ = character_boundary_at_or_after(text_, first_);
    }

    // ---- The editing operations ---------------------------------------------------------

    /// Insert what the platform said the maker typed, at the caret, and step over it. WHILE
    /// TEXT IS SELECTED, TYPING REPLACES IT — the selection is what the next keystroke acts
    /// on, which is the whole meaning a maker attaches to having made one.
    void type(const std::string& utf8) {
        if (utf8.empty()) {
            return; // nothing happened, so nothing to remember either
        }
        if (has_selection()) {
            remember(EditKind::kStructural);
            erase_selection_bytes();
        } else {
            remember(EditKind::kTyping);
        }
        text_.insert(caret_, utf8);
        caret_ += utf8.size();
        anchor_ = caret_;
        settle();
    }

    /// Erase the character immediately BEFORE the caret, and follow it back — or, while text
    /// is selected, erase the SELECTION: the maker named what to remove, and removing one
    /// character beside it instead would be answering a different gesture.
    void backspace() {
        if (has_selection()) {
            remember(EditKind::kStructural);
            erase_selection_bytes();
            settle();
            return;
        }
        if (caret_ == 0) {
            return;
        }
        remember(EditKind::kEraseBack);
        const std::size_t from = character_before(text_, caret_);
        text_.erase(from, caret_ - from);
        caret_ = from;
        anchor_ = caret_;
        settle();
    }

    /// Erase the character AT the caret. The caret does not move -- the text after it comes
    /// back to meet it, which is what makes this a different gesture from backspace rather
    /// than the same one aimed differently. A selection is erased whole, `backspace`'s rule.
    void erase_forward() {
        if (has_selection()) {
            remember(EditKind::kStructural);
            erase_selection_bytes();
            settle();
            return;
        }
        if (caret_ >= text_.size()) {
            return;
        }
        remember(EditKind::kEraseForward);
        text_.erase(caret_, character_after(text_, caret_) - caret_);
        anchor_ = caret_;
        settle();
    }

    /// ERASE FROM THE START OF THE WORD BEFORE THE CARET TO THE CARET — the word-grain
    /// backspace. A selection is erased whole instead, the same precedence every erase has.
    void erase_word_before() {
        if (has_selection()) {
            remember(EditKind::kStructural);
            erase_selection_bytes();
            settle();
            return;
        }
        if (caret_ == 0) {
            return;
        }
        remember(EditKind::kStructural);
        const std::size_t from = word_before(text_, caret_);
        text_.erase(from, caret_ - from);
        caret_ = from;
        anchor_ = caret_;
        settle();
    }

    /// ERASE FROM THE CARET TO THE START OF THE NEXT WORD — the word-grain delete.
    void erase_word_after() {
        if (has_selection()) {
            remember(EditKind::kStructural);
            erase_selection_bytes();
            settle();
            return;
        }
        if (caret_ >= text_.size()) {
            return;
        }
        remember(EditKind::kStructural);
        text_.erase(caret_, word_after(text_, caret_) - caret_);
        anchor_ = caret_;
        settle();
    }

    // ---- Movement. Plain movement COLLAPSES a selection; the select_ variants EXTEND one.
    //
    // Left and Right with a selection live do not step a character: they collapse to the
    // selection's matching end, because after sweeping a range the arrow keys mean "put me
    // at this side of it" — stepping past it would overshoot the range the maker just drew.
    // Every movement also ends the current undo group (`remember`'s rule): type, move,
    // type is two edits a maker made in two places, not one.

    void left() noexcept {
        caret_ = has_selection() ? selection_begin() : character_before(text_, caret_);
        anchor_ = caret_;
        last_edit_ = EditKind::kNone;
        settle();
    }
    void right() noexcept {
        caret_ = has_selection() ? selection_end() : character_after(text_, caret_);
        anchor_ = caret_;
        last_edit_ = EditKind::kNone;
        settle();
    }
    void home() noexcept {
        caret_ = 0;
        anchor_ = caret_;
        last_edit_ = EditKind::kNone;
        settle();
    }
    void end() noexcept {
        caret_ = text_.size();
        anchor_ = caret_;
        last_edit_ = EditKind::kNone;
        settle();
    }
    void word_left() noexcept {
        caret_ = word_before(text_, caret_);
        anchor_ = caret_;
        last_edit_ = EditKind::kNone;
        settle();
    }
    void word_right() noexcept {
        caret_ = word_after(text_, caret_);
        anchor_ = caret_;
        last_edit_ = EditKind::kNone;
        settle();
    }

    void select_left() noexcept {
        caret_ = character_before(text_, caret_);
        last_edit_ = EditKind::kNone;
        settle();
    }
    void select_right() noexcept {
        caret_ = character_after(text_, caret_);
        last_edit_ = EditKind::kNone;
        settle();
    }
    void select_home() noexcept {
        caret_ = 0;
        last_edit_ = EditKind::kNone;
        settle();
    }
    void select_end() noexcept {
        caret_ = text_.size();
        last_edit_ = EditKind::kNone;
        settle();
    }
    void select_word_left() noexcept {
        caret_ = word_before(text_, caret_);
        last_edit_ = EditKind::kNone;
        settle();
    }
    void select_word_right() noexcept {
        caret_ = word_after(text_, caret_);
        last_edit_ = EditKind::kNone;
        settle();
    }

    /// SELECT EVERYTHING, with the caret at the end — where the next ordinary keystroke
    /// belongs, and where both media already know how to show an active end.
    void select_all() noexcept {
        anchor_ = 0;
        caret_ = text_.size();
        last_edit_ = EditKind::kNone;
        settle();
    }

    /// PUT THE CARET AT A POSITION THAT CAME FROM OUTSIDE THE TEXT — a pointer press,
    /// resolved to a column and then to a byte by whoever knows where the window is.
    /// Clamped into the text and snapped to a character boundary, so there is no index a
    /// press can produce that this type will hold. A press collapses any selection: it is
    /// the gesture that STARTS one (the drag extends from here), so it cannot also be
    /// inside one.
    void place(std::size_t at) noexcept {
        caret_ = character_boundary(text_, at);
        anchor_ = caret_;
        last_edit_ = EditKind::kNone;
        settle();
    }

    /// Which word a position is in, in bytes of the whole text. Public because a consumer
    /// deciding whether two presses are a double-click compares the words they named, and that
    /// comparison belongs to whoever holds the presses; the span is `select_word_at`'s own.
    WordSpan word_at(std::size_t at) const noexcept {
        return zengine::component::word_at(text_, at);
    }

    /// Select the word a position is in, and say whether there was one: `place`'s other answer
    /// to a press. The anchor goes to the word's start and the caret to its end, `select_all`'s
    /// choice of active end. A position in no word places the caret and selects nothing.
    bool select_word_at(std::size_t at) noexcept {
        const WordSpan word = zengine::component::word_at(text_, at);
        if (!word.present()) {
            place(at);
            return false;
        }
        anchor_ = word.begin;
        caret_ = word.end;
        last_edit_ = EditKind::kNone;
        settle();
        return true;
    }

    /// Extend the selection to a column a drag reached: the press placed caret and anchor, and
    /// each motion moves only the caret. It takes a column so a drag left of the window still
    /// means something: at or before the left edge it steps the caret one character further
    /// back per motion, enough to walk a selection out of the window.
    void drag_to_column(std::int64_t column) noexcept {
        std::size_t at = position_at_column(column);
        if (column < 0 && first_ > 0) {
            at = character_before(text_, first_);
        }
        caret_ = character_boundary(text_, at);
        last_edit_ = EditKind::kNone;
        settle();
    }

    // ---- The clipboard operations -------------------------------------------------------

    /// COPY THE SELECTION INTO THE OWNER'S CLIPBOARD. With nothing selected, nothing
    /// happens — the clipboard a maker filled a minute ago is not overwritten with an empty
    /// string by a stray chord.
    void copy(Clipboard& clip) const {
        if (!has_selection()) {
            return;
        }
        clip.text = selected_text();
        ++clip.writes;
    }

    /// CUT: copy the selection, then erase it. One undo entry, because it is one gesture.
    void cut(Clipboard& clip) {
        if (!has_selection()) {
            return;
        }
        copy(clip);
        remember(EditKind::kStructural);
        erase_selection_bytes();
        settle();
    }

    /// Paste the owner's clipboard at the caret, replacing the selection, through
    /// `pasteable_line`. The owner's door: Ctrl+V records a request (`Clipboard::paste_requests`)
    /// and the owner calls this once it holds the value. An empty clipboard pastes nothing.
    void paste(const Clipboard& clip) {
        if (clip.text.empty()) {
            return;
        }
        const std::string line = pasteable_line(clip.text);
        if (line.empty() && !has_selection()) {
            return; // nothing to insert and nothing to replace: nothing happened
        }
        remember(EditKind::kStructural);
        if (has_selection()) {
            erase_selection_bytes();
        }
        text_.insert(caret_, line);
        caret_ += line.size();
        anchor_ = caret_;
        settle();
    }

    // ---- The history --------------------------------------------------------------------

    /// STEP BACK TO THE STATE BEFORE THE LAST EDIT GROUP. Answers whether anything changed,
    /// which is a fact a test wants and a consumer may ignore; an undo with no history is
    /// not an error, it is a maker asking one step too far, and the honest response is
    /// nothing.
    bool undo() {
        if (undo_.empty()) {
            return false;
        }
        redo_.push_back(Memory{std::move(text_), caret_, anchor_});
        text_ = std::move(undo_.back().text);
        caret_ = undo_.back().caret;
        anchor_ = undo_.back().anchor;
        undo_.pop_back();
        last_edit_ = EditKind::kNone;
        settle();
        return true;
    }

    /// STEP FORWARD AGAIN. Redo exists exactly until the next edit: a new keystroke makes
    /// the undone future a road not taken, and `remember` discards it.
    bool redo() {
        if (redo_.empty()) {
            return false;
        }
        undo_.push_back(Memory{std::move(text_), caret_, anchor_});
        text_ = std::move(redo_.back().text);
        caret_ = redo_.back().caret;
        anchor_ = redo_.back().anchor;
        redo_.pop_back();
        last_edit_ = EditKind::kNone;
        settle();
        return true;
    }

    /// Can an undo/redo do anything right now? Presentation may want to say so; nothing in
    /// this class turns on it.
    bool can_undo() const noexcept { return !undo_.empty(); }
    bool can_redo() const noexcept { return !redo_.empty(); }

    // ---- The editing-key vocabulary -----------------------------------------------------

    /// Spend one key transition on this box, or say it is not mine:
    ///
    ///     if (box.consume(k.scancode, k.modifiers, clip))
    ///         return;      // the box's own vocabulary; nothing left to route
    ///     // owner policy: Return, Escape, Tab, and every chord the box declined
    ///
    /// True means this vocabulary owned the gesture, not that something changed: a copy with
    /// nothing selected is consumed, so a no-op chord never falls through to an application
    /// binding. Declining is `default:`, so no application chord is ever enumerated; Return,
    /// Escape and Tab are absent because every consumer binds them differently. A chord with Alt
    /// or Super is never this box's. Shift is transparent on the erase keys and meaningful on
    /// movement and Z; Ctrl+Home/End are Home/End, since on one line they are the same places.
    bool consume(std::int64_t scancode, std::int64_t modifiers, Clipboard& clip) {
        if ((modifiers & (mod::kAlt | mod::kSuper)) != 0) {
            return false;
        }
        const bool shift = (modifiers & mod::kShift) != 0;
        if ((modifiers & mod::kCtrl) == 0) {
            switch (scancode) {
            case key::kBackspace: backspace(); return true;
            case key::kDelete: erase_forward(); return true;
            case key::kLeft: shift ? select_left() : left(); return true;
            case key::kRight: shift ? select_right() : right(); return true;
            case key::kHome: shift ? select_home() : home(); return true;
            case key::kEnd: shift ? select_end() : end(); return true;
            default: return false;
            }
        }
        switch (scancode) {
        case key::kLeft: shift ? select_word_left() : word_left(); return true;
        case key::kRight: shift ? select_word_right() : word_right(); return true;
        case key::kHome: shift ? select_home() : home(); return true;
        case key::kEnd: shift ? select_end() : end(); return true;
        case key::kBackspace: erase_word_before(); return true;
        case key::kDelete: erase_word_after(); return true;
        case key::kA: if (shift) { return false; } select_all(); return true;
        case key::kC: if (shift) { return false; } copy(clip); return true;
        case key::kX: if (shift) { return false; } cut(clip); return true;
        case key::kV:
            // A request, not a paste: the gesture is consumed whatever happens next, but the
            // value it means is the clipboard's current one, which only the owner can obtain.
            if (shift) { return false; }
            ++clip.paste_requests;
            return true;
        case key::kZ:
            if (shift) {
                (void)redo();
            } else {
                (void)undo();
            }
            return true;
        case key::kY: if (shift) { return false; } (void)redo(); return true;
        default: return false;
        }
    }

    // ---- Whole-text doors ---------------------------------------------------------------

    /// Empty the box and forget its history: `clear` and `set` are how every consumer opens and
    /// closes a draft, and an undo surviving either would resurrect another draft's text under
    /// this one's label.
    void clear() noexcept {
        text_.clear();
        caret_ = 0;
        anchor_ = 0;
        first_ = 0;
        undo_.clear();
        redo_.clear();
        last_edit_ = EditKind::kNone;
        ++draft_epoch_;
    }

    /// Replace the whole text, saying where the caret goes: the one door for an edit that is not
    /// a keystroke (a completion accepted, a draft opened on a value). The window starts over,
    /// so the next `keep_caret_visible` scrolls to the caret; the history starts over and the
    /// selection collapses, since this is a new draft (see `clear`).
    void set(std::string line, std::size_t at) {
        text_ = std::move(line);
        caret_ = character_boundary(text_, at);
        anchor_ = caret_;
        first_ = 0;
        undo_.clear();
        redo_.clear();
        last_edit_ = EditKind::kNone;
        ++draft_epoch_;
    }

    /// Which draft this box is holding: a counter `set` and `clear` bump, so two reads with the
    /// same number are about one draft. For an owner whose paste crosses a turn: text asked for
    /// by one draft must not land in whichever draft stands later. It rides a copy of the box, so
    /// a draft carried across a rebuild (`Row::resume`) stays the same draft to a paste in flight.
    std::uint64_t draft_epoch() const noexcept { return draft_epoch_; }

private:
    /// WHAT KIND OF EDIT THE LAST MUTATION WAS — the whole of the undo grouping rule.
    /// Consecutive edits of one kind coalesce into the entry the first of them opened;
    /// `kStructural` never coalesces (a paste, a cut, a word erase and any edit that
    /// replaced a selection are each one gesture and one entry); `kNone` means the next
    /// edit starts fresh (after movement, after undo/redo, after open).
    enum class EditKind : std::uint8_t {
        kNone,
        kTyping,
        kEraseBack,
        kEraseForward,
        kStructural,
    };

    /// One remembered state. The window offset is deliberately not in it: `first_visible`
    /// is presentation, the consumer reconciles it every repaint, and restoring a stale one
    /// would scroll the row for no edit the maker made.
    struct Memory {
        std::string text;
        std::size_t caret = 0;
        std::size_t anchor = 0;
    };

    /// Enough steps that no ordinary draft runs out, small enough that eight Inspector rows
    /// carrying one apiece cost nothing worth measuring. The oldest entry is forgotten
    /// first: a bounded history's honest failure mode is forgetting the far past, never
    /// refusing the present.
    static constexpr std::size_t kUndoDepth = 100;

    /// CALLED BEFORE EVERY MUTATION, WITH THE PRE-STATE STILL CURRENT. Opens a new undo
    /// entry when the edit kind changed (or is structural), and always discards the redo
    /// stack — an edit after an undo makes the undone future a road not taken.
    void remember(EditKind kind) {
        if (kind != last_edit_ || kind == EditKind::kStructural) {
            if (undo_.size() >= kUndoDepth) {
                undo_.erase(undo_.begin());
            }
            undo_.push_back(Memory{text_, caret_, anchor_});
        }
        redo_.clear();
        last_edit_ = kind;
    }

    /// Erase the selected bytes and collapse onto where they were. Callers `remember` first;
    /// this is the shared mechanics, not a gesture.
    void erase_selection_bytes() {
        const std::size_t from = selection_begin();
        text_.erase(from, selection_end() - from);
        caret_ = from;
        anchor_ = from;
    }

    /// The half of the invariant that needs no capacity, re-established after every edit: the
    /// window never begins past the end of the text, after the caret or inside a character, and
    /// the anchor is inside the text on a boundary. "Never after the caret" is the leftward
    /// scroll, minimal by construction. Every mutator ends with it, even where it cannot change
    /// anything, so "every operation re-establishes the invariant" is checkable by eye.
    void settle() noexcept {
        if (first_ > text_.size()) {
            first_ = text_.size();
        }
        if (first_ > caret_) {
            first_ = caret_;
        }
        first_ = character_boundary_at_or_after(text_, first_);
        if (anchor_ > text_.size()) {
            anchor_ = text_.size();
        }
        anchor_ = character_boundary(text_, anchor_);
    }

    std::string text_;
    std::size_t caret_ = 0;
    std::size_t anchor_ = 0;
    std::size_t first_ = 0;
    std::vector<Memory> undo_;
    std::vector<Memory> redo_;
    EditKind last_edit_ = EditKind::kNone;
    std::uint64_t draft_epoch_ = 0; ///< bumped by set/clear -- see draft_epoch()
};

} // namespace zengine::component

#endif // ZENGINE_COMPONENT_TEXT_BOX_HPP
