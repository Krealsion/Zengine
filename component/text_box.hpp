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
//     one implementation serve two tools whose commit models have nothing in common (the
//     Terminal submits a line to a participant; a property row parses, writes and may be
//     refused with a reason).
//
// WHAT IS DELIBERATELY ABSENT, because neither consumer has asked for it: a selection range,
// a clipboard, undo/redo, more than one line, a maximum length, a filter, a prompt, a label,
// completion, a theme, a blink and a focus flag. The pre-Zen `Zen::TextBox`
// (reference/src/zengine/ui/text_box.h, archaeology only) carried a filter, a focus flag, a
// blink timer, two signals and a child Text entity -- and could not move its caret, could not
// scroll, and erased one BYTE at a time. It is not this component's ancestor in anything but
// the name, and the name is the only part worth keeping.
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

// ---- The component ----------------------------------------------------------------------

/// A LINE OF EDITABLE TEXT, THE INSERTION POINT IN IT, AND WHICH PART OF IT IS ON SCREEN.
///
/// THREE FACTS THAT MOVE TOGETHER, WHICH IS THE WHOLE ARGUMENT FOR A CLASS. The text, the
/// caret and the window are not three fields a consumer keeps beside each other; they are
/// one state, and every one of them can be invalidated by a change to either of the others.
/// A public `std::string` with a caret index beside it makes forgetting free -- once per call
/// site, silently -- and the day one call site forgets, the caret is inside a character or
/// the window is past the end of the line and nothing says so. So the operations are the ONLY
/// way any of the three changes, and each of them re-establishes the invariant before it
/// returns. There is no `fix_it()` to call and no way to reach a state that would need one.
///
/// THE INVARIANT, IN TWO HALVES, AND THE SPLIT IS WHICH HALF NEEDS TO KNOW HOW MUCH ROOM
/// THERE IS:
///
///     always, after every operation      0 <= first_visible <= caret <= size()
///                                        first_visible is on a character boundary
///                                        caret is on a character boundary
///
///     after keep_caret_visible(N)        caret - first_visible <= N
///                                        first_visible <= max(0, size() - N)
///
/// The first half needs nothing, so `settle()` re-establishes it after every mutator -- which
/// makes the LEFTWARD scroll free, because the smallest window start that shows a caret to
/// the left of the window IS the caret. The second half needs the room, so it is the
/// consumer's to ask for, once per repaint, with the capacity it resolved.
///
/// THE CAPACITY IS AN ARGUMENT AND NEVER A MEMBER. This class never asks how wide anything
/// is: `keep_caret_visible(columns)` and `visible(columns)` take it, exactly the way
/// `place(at)` takes a position resolved from outside. That is not tidiness -- the Terminal's
/// row and an Inspector row are different widths in the same running application, and a
/// component that remembered one of them would be remembering the wrong one for the other.
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

    /// Insert what the platform said the maker typed, at the caret, and step over it.
    void type(const std::string& utf8) {
        text_.insert(caret_, utf8);
        caret_ += utf8.size();
        settle();
    }

    /// Erase the character immediately BEFORE the caret, and follow it back.
    void backspace() {
        if (caret_ == 0) {
            return;
        }
        const std::size_t from = character_before(text_, caret_);
        text_.erase(from, caret_ - from);
        caret_ = from;
        settle();
    }

    /// Erase the character AT the caret. The caret does not move -- the text after it comes
    /// back to meet it, which is what makes this a different gesture from backspace rather
    /// than the same one aimed differently.
    void erase_forward() {
        if (caret_ >= text_.size()) {
            return;
        }
        text_.erase(caret_, character_after(text_, caret_) - caret_);
        settle();
    }

    void left() noexcept {
        caret_ = character_before(text_, caret_);
        settle();
    }
    void right() noexcept {
        caret_ = character_after(text_, caret_);
        settle();
    }
    void home() noexcept {
        caret_ = 0;
        settle();
    }
    void end() noexcept {
        caret_ = text_.size();
        settle();
    }

    /// PUT THE CARET AT A POSITION THAT CAME FROM OUTSIDE THE TEXT — a pointer press,
    /// resolved to a column and then to a byte by whoever knows where the window is.
    /// Clamped into the text and snapped to a character boundary, so there is no index a
    /// press can produce that this type will hold.
    void place(std::size_t at) noexcept {
        caret_ = character_boundary(text_, at);
        settle();
    }

    void clear() noexcept {
        text_.clear();
        caret_ = 0;
        first_ = 0;
    }

    /// REPLACE THE WHOLE TEXT, SAYING WHERE THE CARET GOES. The one door for an edit that is
    /// not a keystroke -- accepting a completion candidate, opening a draft on a property's
    /// current value -- and the reason it takes two arguments is that the alternative is a
    /// call site that changes the text and forgets the caret.
    ///
    /// THE WINDOW STARTS OVER, because an offset into the text that WAS there is not a fact
    /// about the text that is. The next `keep_caret_visible` then scrolls to wherever the
    /// caller put the caret, so a long value opened for editing with the caret at its end
    /// arrives with its tail on screen.
    void set(std::string line, std::size_t at) {
        text_ = std::move(line);
        caret_ = character_boundary(text_, at);
        first_ = 0;
    }

private:
    /// THE HALF OF THE INVARIANT THAT NEEDS NO CAPACITY, re-established after every edit: the
    /// window never begins past the end of the text, never begins after the caret, and never
    /// begins inside a character.
    ///
    /// "Never after the caret" IS the leftward scroll, and it is minimal by construction --
    /// the smallest window start that shows a caret to the left of the window is the caret
    /// itself. So Left, Home, a press and a backspace at the window's edge scroll here rather
    /// than in `keep_caret_visible`, and they do it without being told how wide anything is.
    ///
    /// EVERY MUTATOR ENDS WITH IT, including the two it cannot possibly change (`right` and
    /// `end` only move the caret away from the window's start). "Every operation
    /// re-establishes the invariant" is a rule a reader checks by eye; "these four do and
    /// those two need not" is one they have to re-derive, and the difference costs three
    /// integer comparisons.
    void settle() noexcept {
        if (first_ > text_.size()) {
            first_ = text_.size();
        }
        if (first_ > caret_) {
            first_ = caret_;
        }
        first_ = character_boundary_at_or_after(text_, first_);
    }

    std::string text_;
    std::size_t caret_ = 0;
    std::size_t first_ = 0;
};

} // namespace zengine::component

#endif // ZENGINE_COMPONENT_TEXT_BOX_HPP
