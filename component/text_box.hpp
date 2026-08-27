// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_COMPONENT_TEXT_BOX_HPP
#define ZENGINE_COMPONENT_TEXT_BOX_HPP

// EDITABLE TEXT, WITH A CARET AND A WINDOW ONTO IT — the first foundational Zen component.
//
// WHY IT EXISTS, and the answer is a measurement rather than a roadmap. Two working Workshop
// tools reached the same editing machinery from opposite ends:
//
//   the Terminal's command line (HD-3, HD-4)    text, a movable caret, character-safe edits,
//                                               a horizontal window, a pointer -> a position
//   an Inspector property draft (HD-5)          the same text and the same character-safe
//                                               edits -- and NO caret, NO window and no way
//                                               to reach a value longer than its row
//
// HD-4 traced the second consumer on all nine axes and declined to extract anything, because
// at that point the two shared only the character walk they were ALREADY sharing as free
// functions: a `TextBox` would have renamed `TerminalInput` and deleted no duplication. HD-5
// is the day the property editor genuinely needs the caret, the window and the pointer
// arithmetic, which is the day the extraction is the SMALLER repair rather than a larger one.
//
// TEXT-0 is the day the component earns the REST of what a text box means to the hand that
// uses one. Four consumers existed by then (the Terminal line, the Inspector draft, the
// setup-name editor, the Composer's field drafts) and every one of them had copied the same
// six-case scancode switch to reach the six operations above — so the key VOCABULARY moved in
// here (`consume`, below), and with one owner for the vocabulary the ordinary expectations
// stopped being per-consumer projects: a selection, the clipboard gestures over it, and a
// bounded local undo are component mechanics now, because "a text box that cannot select,
// copy or undo" is not a smaller component — it is a surprising one.
//
// WHAT A COMPONENT IS HERE, and each line of this is enforced by what this file does not
// include:
//
//   * A component has meaning independent of the medium that presents it. There is no SDL
//     here, no terminal, no cell, no pixel and no font metric -- the consumer supplies the
//     room it has, in whatever unit it measures prose in, as an ARGUMENT.
//   * A component is not an entity. This class has no identity, no registry entry, no
//     lifetime of its own and nothing to clean up: it is a member of whatever owns it and it
//     dies with that owner. `TerminalPane` owns one; `workshop::Row` owns one.
//   * A component owns no policy. It does not commit, validate, refuse, parse, complete,
//     submit, focus, blink or draw. What a draft MEANS is the consumer's, which is what lets
//     one implementation serve tools whose commit models have nothing in common (the
//     Terminal submits a line to a participant; a property row parses, writes and may be
//     refused with a reason). The same split holds for the clipboard: the OPERATIONS are
//     here, and the `Clipboard` they operate on is the owner's — where its text goes beyond
//     this process (a platform clipboard, a bus message, nowhere) is custody this class has
//     no opinion about.
//
// WHAT IS DELIBERATELY ABSENT, because no consumer has asked for it: more than one line, a
// maximum length, a filter, a prompt, a label, completion, a theme, a blink and a focus flag.
// The pre-Zen `Zen::TextBox` (reference/src/zengine/ui/text_box.h, archaeology only) carried a
// filter, a focus flag, a blink timer, two signals and a child Text entity -- and could not
// move its caret, could not scroll, and erased one BYTE at a time. It is not this component's
// ancestor in anything but the name, and the name is the only part worth keeping.
//
// THE UNIT IS A BYTE AND THE STEPS ARE CHARACTERS (HD-3). Every consumer of this counts one
// column per byte, because that is what draws the text -- `workshop::detail::fit` cuts at a
// byte and `surface::project_text_regions` cuts "on a byte boundary: one cell per byte, as
// ever". So a caret is a byte index: a codepoint caret would be measuring the line
// differently from the thing that paints it and would drift from the picture on the first
// multi-byte character. What the OPERATIONS refuse to do is stop half-way through one.
//
// Byte-oriented columns with character-safe boundaries; NOT Unicode width, not grapheme
// clusters, not combining marks, not double-width glyphs. A two-byte character occupies two
// columns because that is what the projection draws. That limit is inherited from HD-3/HD-4
// unchanged and is not this phase's to move.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::component {

// ---- What a character is, in this application -------------------------------------------
//
// ONE ANSWER, AND IT LIVES BESIDE ITS ONLY CONSUMER. These were born in
// `workshop/property.hpp` because a property draft was the first thing that could be
// backspaced; they were then spent by the Terminal's caret too, which is what made them free
// functions rather than methods. After HD-5 both of those consumers ARE this component, so
// leaving generic text-boundary arithmetic owned by property machinery would have been the
// filing accident outliving the reason for it. They are still free functions and still take
// the string: a component is not the only thing that could ever need to know where a
// character starts, and the day something else does, it asks here rather than walking UTF-8
// for itself.

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

/// THE NEAREST CHARACTER BOUNDARY AT OR AFTER `at`, clamped into the line — the other
/// direction, and it exists because a VIEWPORT cannot use the one above (HD-4).
///
/// A horizontally scrolled line begins at a byte the presentation chose, and that byte must
/// not be the middle of a character: half a character at the left edge is a mark a maker
/// cannot read. Which way to snap looks like taste and is not. The caret has to stay inside
/// the window, and the window's right edge is `first_visible + columns` — so snapping the
/// FIRST VISIBLE byte BACKWARDS moves that edge back with it and can push the caret one to
/// three columns off the end of the row it is supposed to be sitting on. Snapping forwards
/// can only ever make the window shorter at the left, which costs at most one character of
/// text and cannot cost the caret.
///
/// It also cannot overshoot a caret: a caret is always on a character boundary (this
/// component's invariant), so a forward snap from at or before it lands at or before it.
/// That is what lets the two rules compose in either order.
inline std::size_t character_boundary_at_or_after(const std::string& line,
                                                  std::size_t at) noexcept {
    std::size_t i = at < line.size() ? at : line.size();
    while (i < line.size() && is_continuation_byte(line[i])) {
        ++i;
    }
    return i;
}

// ---- What a word is, in this application (TEXT-0) ---------------------------------------
//
// THE SMALLEST RULE THAT IS A RULE: a word is a maximal run of non-space bytes, and the one
// separator is the space (0x20). It is a shell's word, not an editor's — there is no
// identifier class, no punctuation class, no locale and no Unicode category table, because a
// single-line command, name, or property value is the material this component holds and a
// lexical framework would be machinery for text this class is deliberately unable to contain.
// Both functions land on character boundaries by construction: a space is a single-byte
// character, so the position after one — and 0, and `line.size()` — are boundaries already,
// and a multi-byte character is all non-space bytes and is never split.

/// THE START OF THE WORD BEFORE `at`: back over spaces, then back over the word.
inline std::size_t word_before(const std::string& line, std::size_t at) noexcept {
    std::size_t i = at < line.size() ? at : line.size();
    while (i > 0 && line[i - 1] == ' ') {
        --i;
    }
    while (i > 0 && line[i - 1] != ' ') {
        --i;
    }
    return i;
}

/// THE START OF THE WORD AFTER `at`: forward over the word, then over the spaces — so
/// repeated presses walk word starts, which is the gesture's conventional meaning, and the
/// last press lands at the end of the line.
inline std::size_t word_after(const std::string& line, std::size_t at) noexcept {
    std::size_t i = at < line.size() ? at : line.size();
    while (i < line.size() && line[i] != ' ') {
        ++i;
    }
    while (i < line.size() && line[i] == ' ') {
        ++i;
    }
    return i;
}

// ---- What foreign text becomes in a one-line box (TEXT-0) -------------------------------

/// A CLIPBOARD'S BYTES AS ONE LINE. Typed text arrives through the platform's own layout and
/// never contains a control byte (every backend excludes them — input/vocabulary.hpp's
/// "editing controls are not entered text"); a clipboard is foreign bytes from anywhere, so
/// the one door they enter a single-line component through flattens them: a CRLF pair
/// becomes one space, and every other byte below 0x20 — a lone CR or LF, a tab — and 0x7F
/// becomes one space each. A space rather than deletion, because silently concatenating two
/// pasted lines into one word would manufacture a token the maker never had; and a space
/// rather than a marker, because the paste is visible at the caret and a marker would be a
/// byte the maker did not paste either. Tabs are flattened for the cell grid's reason: every
/// presentation of this component counts one column per byte, and 0x09 is the one printable-
/// looking byte that moves a terminal's cursor by more than one cell.
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

// ---- The clipboard, as a value the OWNER holds (TEXT-0) ---------------------------------

/// TEXT A MAKER COPIED, AND A COUNT OF THE TIMES THIS PROCESS PUT SOME THERE.
///
/// A plain value, deliberately: this component links nothing and talks to no platform, so
/// the clipboard it can operate on is a string somebody hands it. WHO holds that string and
/// what else its text is bridged to — the platform clipboard through a Skin, another
/// participant through a bus message, nothing at all — is the owner's custody, exactly as
/// the capacity is the owner's number.
///
/// `writes` is how an owner notices a copy happened without a callback: `copy` and `cut`
/// bump it exactly when they took text, so an owner that snapshots it before handing the
/// clipboard to `consume` can tell "the maker copied" from "the maker moved the caret" with
/// one comparison. It counts THIS process's copies and is never decremented; an owner that
/// mirrors a platform clipboard updates `text` and leaves `writes` alone, so the counter
/// keeps meaning the one thing it can honestly mean.
struct Clipboard {
    std::string text;
    std::uint64_t writes = 0;
};

// ---- The editing-key vocabulary's identities (TEXT-0) -----------------------------------
//
// THE SAME NUMBERS `input::scan::` AND `input::mod::` NAME, SPELLED LOCALLY, because this
// component includes nothing — not the Input package, whose vocabulary header carries the
// wire machinery a component must not link. The pattern is translate_sdl.hpp's: a pure file
// spells a foreign constant by hand, and the one translation unit that sees both spellings
// pins them against each other (the component suite static_asserts every value below against
// `input::scan::`/`input::mod::`), so a typo here is a red build rather than a silently
// different world. Scancodes are USB HID usage ids; the modifier bits are the Input
// package's semantic bitmask.
//
// ONLY the keys `consume` binds are named. This namespace is not a keyboard: a key the
// vocabulary does not bind needs no name here, because declining it is `default:`, not
// knowledge.

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

// ---- The component ----------------------------------------------------------------------

/// A LINE OF EDITABLE TEXT, THE INSERTION POINT IN IT, THE SELECTION AROUND THAT POINT, AND
/// WHICH PART OF IT IS ON SCREEN.
///
/// FOUR FACTS THAT MOVE TOGETHER, WHICH IS THE WHOLE ARGUMENT FOR A CLASS. The text, the
/// caret, the selection anchor and the window are not fields a consumer keeps beside each
/// other; they are one state, and every one of them can be invalidated by a change to any of
/// the others. A public `std::string` with indices beside it makes forgetting free -- once
/// per call site, silently -- and the day one call site forgets, the caret is inside a
/// character or the anchor is past the end of the text and nothing says so. So the
/// operations are the ONLY way any of them changes, and each re-establishes the invariant
/// before it returns. There is no `fix_it()` to call and no way to reach a state that would
/// need one.
///
/// THE SELECTION IS AN ANCHOR AND THE CARET (TEXT-0). One end of a selection is always the
/// caret — it is where the next keystroke lands, which is what a selection's "active end"
/// means — so the only new fact a selection needs is the OTHER end, and `anchor_` is it.
/// No selection is spelled `anchor_ == caret_`: there is no `has_selection` flag to fall out
/// of step, an empty selection cannot exist as a distinct state, and collapsing is one
/// assignment. Extending gestures move the caret and leave the anchor; ordinary movement
/// collapses the anchor onto wherever the caret lands, which is every text box a maker has
/// ever used.
///
/// THE HISTORY IS SNAPSHOTS, BOUNDED, AND LOCAL (TEXT-0). Undo holds copies of {text, caret,
/// anchor} — a single line is small, so a snapshot costs less than the machinery that would
/// avoid one — capped at `kUndoDepth` entries with the oldest forgotten first. Contiguous
/// same-kind keystrokes coalesce into one entry (the grouping rule is written on
/// `remember`), so undoing a typed word is one gesture rather than nine. The history is THIS
/// box's and dies with the draft: `set` and `clear` are how every consumer opens and closes
/// a draft, and both wipe it, because an undo that resurrected a PREVIOUS draft's text into
/// a new one would be this component putting one property's value under another's label.
/// There is no application-wide undo here and none may grow from this — what a COMMITTED
/// value's history means belongs to whatever owns commits.
///
/// THE INVARIANT, IN TWO HALVES, AND THE SPLIT IS WHICH HALF NEEDS TO KNOW HOW MUCH ROOM
/// THERE IS:
///
///     always, after every operation      0 <= first_visible <= caret <= size()
///                                        0 <= anchor <= size()
///                                        first_visible, caret and anchor are on
///                                        character boundaries
///
///     after keep_caret_visible(N)        caret - first_visible <= N
///                                        first_visible <= max(0, size() - N)
///
/// The first half needs nothing, so `settle()` re-establishes it after every mutator --
/// which makes the LEFTWARD scroll free, because the smallest window start that shows a
/// caret to the left of the window IS the caret. The second half needs the room, so it is
/// the consumer's to ask for, once per repaint, with the capacity it resolved.
///
/// THE CAPACITY IS AN ARGUMENT AND NEVER A MEMBER. This class never asks how wide anything
/// is: `keep_caret_visible(columns)`, `visible(columns)` and `visible_selection(columns)`
/// take it, exactly the way `place(at)` takes a position resolved from outside. That is not
/// tidiness -- the Terminal's row and an Inspector row are different widths in the same
/// running application, and a component that remembered one of them would be remembering the
/// wrong one for the other.
///
/// `first_visible` IS COMPONENT STATE, AND THE REASON IS A MEASUREMENT RATHER THAN A
/// PRINCIPLE. It is arguably presentation state: it is meaningless without a viewport, and
/// two viewports of different widths onto ONE TextBox would need two of them. Neither
/// consumer has two: a Terminal pane presents its line once and a property row presents its
/// draft once, and the reconcile that keeps the window honest has to happen after every edit
/// -- which is exactly where the text and the caret already are. Splitting model from view
/// now would buy a second object, a second lifetime and a second place to forget, for a case
/// nothing in this repository has. WHAT WOULD PRESSURE IT: the same draft shown at two widths
/// at once (a value in an Inspector row and in a wider editor beside it), or a component
/// presented in two media simultaneously. On that day `first_visible` moves out into whatever
/// holds the presentation and the operations take it by reference; until then this is the
/// smallest correct answer and the sentence above is the trigger to watch for.
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

    /// HOW MANY COLUMNS FROM THE START OF THE VISIBLE SLICE THE CARET SITS AT.
    ///
    /// The one piece of caret arithmetic both consumers need and neither should own: a
    /// presentation adds whatever its own prose begins at (a `> ` prompt, a padded label) and
    /// publishes the sum. Computed here rather than by each consumer subtracting for itself,
    /// because `caret - first_visible` written twice is two answers to one question, and the
    /// second one is right until the first line long enough to scroll.
    ///
    /// It may equal the capacity: a caret is BETWEEN characters, so the position after the
    /// last visible one is a real column and whoever draws it must leave room for it.
    std::size_t caret_column() const noexcept { return caret_ - first_; }

    /// THE BYTE A COLUMN OF THE VISIBLE SLICE NAMES, clamped into the text.
    ///
    /// The other direction of `caret_column`, and the arithmetic a POINTER needs: a
    /// presentation resolves a press to a column of its own prose, subtracts whatever its
    /// row begins with, and hands the rest here. What comes back is an index into the WHOLE
    /// text -- `first_visible + column` -- which is the one subtraction a horizontal window
    /// adds to a hit test, and the one it is right to leave out for exactly as long as
    /// nothing is long enough to scroll.
    ///
    /// The boundary answers, written down rather than left to arithmetic:
    ///
    ///     a column at or before the start   -> first_visible  (the maker aimed at what
    ///                                                          they can SEE)
    ///     a column past the last byte       -> size()         (the end of the WHOLE text,
    ///                                                          not the end of the slice)
    ///
    /// Both ends CLAMP rather than refuse, because a press that landed on the row is a
    /// statement about where in the text the maker wants to be; whether it landed on the row
    /// at all is the consumer's question and is asked first. The result is a byte index and
    /// makes no claim to be a character boundary -- `place()` is what snaps it.
    ///
    /// "PAST THE LAST BYTE MEANS THE END OF THE WHOLE TEXT" is honest rather than convenient:
    /// after `keep_caret_visible` there is blank room at the right only when the text ends
    /// inside the row, so a press in that room is a press after the last character there is.
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

    /// THE PART OF THE TEXT A ROW OF `columns` COLUMNS IS SHOWING.
    ///
    /// A slice, and emphatically not a mutation: the hidden characters are still in `text()`,
    /// still what a consumer submits or commits, and still what anything asking about the
    /// whole value is handed. Nothing is erased, rotated or marked, and no scroll indicator
    /// is part of the text.
    ///
    /// THE RIGHT-HAND CUT IS A BYTE CUT, and that is the same cut every presentation of this
    /// makes. Snapping it back to a character boundary would shorten the row by up to three
    /// columns while the caret's column is computed from the window, which is how a caret at
    /// the far right falls off a cell medium's own row. The LEFT edge is a position this
    /// class CHOOSES; the right edge is a cut.
    std::string visible(std::int64_t columns) const {
        if (columns <= 0 || first_ >= text_.size()) {
            return {};
        }
        const std::size_t room = static_cast<std::size_t>(columns);
        const std::size_t left = text_.size() - first_;
        return text_.substr(first_, left < room ? left : room);
    }

    /// MOVE THE WINDOW AS LITTLE AS IT TAKES TO SEE THE CARET, given the room the row has.
    ///
    /// Deterministic and minimal, in that order. There is no animation, no recentring and no
    /// scroll margin: a caret that walks off the right edge brings the window one character
    /// with it, and one that walks off the left edge does the same the other way. Recentring
    /// on every edit would move the whole line under a maker's eye for a keystroke that
    /// changed one character.
    ///
    /// THE FOUR RULES, IN THE ORDER THEY MUST BE APPLIED:
    ///
    ///     1  no blank room on the right while text is hidden on the left
    ///     2  the caret is not to the left of the window
    ///     3  the caret is not past the right of it
    ///     4  the window does not begin inside a character
    ///
    /// Rule 1 is what makes deleting recover the room a deletion freed -- without it,
    /// backspacing a long line back down to a short one leaves an apparently EMPTY row with
    /// the whole text hidden away to the left, which reads exactly like a tool that lost it.
    /// It also buys a property a consumer's hit test turns on: rules 2 and 3 only ever move
    /// the window to a position rule 1 already allows, so afterwards
    /// `first_visible <= size() - columns` whenever the text is longer than the row. Blank
    /// room at the right therefore means the text really did end there, and a press in it
    /// means the end of the text.
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

    /// EXTEND THE SELECTION TO A COLUMN A DRAG REACHED — `place`'s other half: the press
    /// placed the caret (and the anchor with it), and every motion after it moves only the
    /// caret, so the selection grows and shrinks under the hand without the anchor ever
    /// being restated.
    ///
    /// It takes the COLUMN rather than a byte for one reason `place` does not have: a drag
    /// that leaves the visible slice still means something. A column past the right edge
    /// already names a byte past the window (`position_at_column`'s ordinary arithmetic),
    /// and the next reconcile scrolls to it; a column at or before the left edge would clamp
    /// to `first_visible` forever, so a drag sitting left of the window instead steps the
    /// caret ONE CHARACTER further back per motion event — deterministic, minimal, and
    /// enough to walk a selection leftward out of the window a motion at a time.
    void drag_to_column(std::int64_t column) noexcept {
        std::size_t at = position_at_column(column);
        if (column < 0 && first_ > 0) {
            at = character_before(text_, first_);
        }
        caret_ = character_boundary(text_, at);
        last_edit_ = EditKind::kNone;
        settle();
    }

    // ---- The clipboard operations (TEXT-0) ----------------------------------------------

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

    /// PASTE THE OWNER'S CLIPBOARD AT THE CARET, replacing the selection if there is one.
    /// The bytes go through `pasteable_line` first — a single-line component does not hold
    /// newlines or control bytes, whoever put them on the clipboard.
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

    // ---- The history (TEXT-0) -----------------------------------------------------------

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

    // ---- The editing-key vocabulary (TEXT-0) --------------------------------------------

    /// SPEND ONE KEY TRANSITION ON THIS BOX, OR SAY IT IS NOT MINE.
    ///
    ///     if (box.consume(k.scancode, k.modifiers, clip))
    ///         return;      // the box's own vocabulary; nothing left to route
    ///     // owner policy: Return, Escape, Tab, and every chord the box declined
    ///
    /// THE BOOL IS QR-2's BOOL: true = this vocabulary owned the gesture, stop routing;
    /// false = not mine, yours. It is NOT "something changed" — a copy with nothing
    /// selected, an undo with no history and a Home at position 0 are all consumed, because
    /// each reached the layer that owns what the gesture means. That is what keeps a
    /// clipboard chord from falling through to an application binding the moment it happens
    /// to be a no-op, which is the exact accident SC-form routing exists to prevent.
    ///
    /// DECLINING IS `default:`, NOT KNOWLEDGE. The switch below matches this component's own
    /// gestures and nothing else, so every hotkey an application ever invents is declined by
    /// a branch that was never edited — a TextBox that had to enumerate application chords
    /// to refuse them would be the routing table this function exists to delete. Four
    /// consumers each carried a copy of this mapping before TEXT-0; a fifth was about to be
    /// written. Return, Escape and Tab are deliberately absent: all four consumers bind them
    /// and all four bind them differently, which is the definition of policy.
    ///
    /// A CHORD WITH ALT OR SUPER IS NEVER THIS BOX'S. Those modifiers belong to
    /// applications and window systems; consuming `Alt+Left` as `Left` would eat a gesture
    /// this vocabulary has no meaning for.
    ///
    /// Shift is transparent on the two erase keys (a maker holding Shift mid-word still
    /// means erase) and meaningful on movement (extend) and on Z (the conventional second
    /// redo spelling). Ctrl+Home/End collapse to Home/End: on one line the document's ends
    /// and the line's ends are the same two places.
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
        case key::kV: if (shift) { return false; } paste(clip); return true;
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

    /// EMPTY THE BOX AND FORGET ITS HISTORY. `clear` and `set` are how every consumer opens
    /// and closes a draft, so they are where a draft's history begins and ends — an undo
    /// surviving either would resurrect text from a DIFFERENT draft into this one, wearing
    /// its label (the accident SC-form history rules exist to make untestable-by-luck).
    void clear() noexcept {
        text_.clear();
        caret_ = 0;
        anchor_ = 0;
        first_ = 0;
        undo_.clear();
        redo_.clear();
        last_edit_ = EditKind::kNone;
    }

    /// REPLACE THE WHOLE TEXT, SAYING WHERE THE CARET GOES. The one door for an edit that is
    /// not a keystroke -- accepting a completion candidate, opening a draft on a property's
    /// current value -- and the reason it takes two arguments is that the alternative is a
    /// call site that changes the text and forgets the caret.
    ///
    /// THE WINDOW STARTS OVER, because an offset into the text that WAS there is not a fact
    /// about the text that is. The next `keep_caret_visible` then scrolls to wherever the
    /// caller put the caret, so a long value opened for editing with the caret at its end
    /// arrives with its tail on screen. THE HISTORY STARTS OVER TOO, and the selection
    /// collapses: this is a new draft, not an edit to the old one (see `clear`).
    void set(std::string line, std::size_t at) {
        text_ = std::move(line);
        caret_ = character_boundary(text_, at);
        anchor_ = caret_;
        first_ = 0;
        undo_.clear();
        redo_.clear();
        last_edit_ = EditKind::kNone;
    }

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

    /// THE HALF OF THE INVARIANT THAT NEEDS NO CAPACITY, re-established after every edit: the
    /// window never begins past the end of the text, never begins after the caret, and never
    /// begins inside a character — and the anchor is inside the text, on a character
    /// boundary, always.
    ///
    /// "Never after the caret" IS the leftward scroll, and it is minimal by construction --
    /// the smallest window start that shows a caret to the left of the window is the caret
    /// itself. So Left, Home, a press and a backspace at the window's edge scroll here rather
    /// than in `keep_caret_visible`, and they do it without being told how wide anything is.
    ///
    /// EVERY MUTATOR ENDS WITH IT, including the ones it cannot possibly change. "Every
    /// operation re-establishes the invariant" is a rule a reader checks by eye; "these do
    /// and those need not" is one they have to re-derive, and the difference costs a few
    /// integer comparisons.
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
};

} // namespace zengine::component

#endif // ZENGINE_COMPONENT_TEXT_BOX_HPP
