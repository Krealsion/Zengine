// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PROPERTY_HPP
#define ZENGINE_WORKSHOP_PROPERTY_HPP

// The typed connection between an editor and the property it presents.
//
// It exists because of a measured pain point rather than a design instinct: the
// historical builder
// (reference/src/apps/builder/build_state.cpp, archaeology only) hand-wrote,
// per property, a label plus a current-value read plus an input filter plus a
// parse plus a setter callback plus two refreshes -- dozens of times, with the
// conversion logic copied into every one of them. The cost was not the typing.
// The cost was that each copy could be subtly different, and several were.
//
// THE SHAPE, and why each piece is here:
//
//   Written        the outcome of a write: accepted, or refused WITH A REASON.
//                  A setter that can only succeed cannot carry an invariant, and
//                  an invariant that cannot be stated cannot be shown to a maker.
//
//   Property<T>    a read and a write over ONE property of ONE thing, as typed
//                  operations. Two std::functions, never an address: it closes
//                  over the SEMANTIC surface (document.hpp's operations), so a
//                  write goes through whatever validation, invalidation or
//                  ownership that operation carries. There is no path through
//                  this type to a struct member.
//
//   TextForm<T>    how a T becomes text and text becomes a T -- written ONCE per
//                  semantic type. This is the piece that deletes the old
//                  builder's duplication: `Width` and `Height` are both extents,
//                  so they share every line of conversion, and a third extent
//                  property would cost a single call.
//
//   Row            one inspector line: a label, a typed property, and an editor
//                  DRAFT. Type-erased, so the inspector holds a mixed list of
//                  rows without a per-type row class and without the caller
//                  knowing what T was.
//
// WHAT THIS DELIBERATELY IS NOT:
//   - not reflection. Nothing enumerates a type's fields; a property exists
//     because someone wrote it down.
//   - not a registry. There is no global table of properties; a Row is a value
//     the inspector happens to be holding.
//   - not persistence. Nothing here serializes, and inspectability implies no
//     save (the phase's §E: inspectable, editable and serializable are three
//     different promises, and this file makes exactly one of them).
//   - not universal. The whole file is Workshop-local by choice: no shape here
//     crosses the bus, nothing lower than Workshop knows it exists, and if the
//     idea turns out to be wrong it can be deleted without a migration.

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "vocabulary.hpp"

#include "ui/vocabulary.hpp"

namespace zengine::workshop {

/// An authored context reference, in the spelling a maker reads and types.
///
/// It is a distinct type and not a bare `std::int64_t` for one reason and it is
/// not aesthetics: `TextForm<std::int64_t>` is already spoken for by X and Y,
/// and a context is not a number a maker should be typing. `root` and `#4` are
/// the two things it can be, and `#4` is deliberately not spelled `4` -- the
/// `#` is the same mark the object list and the Identity row already use, so a
/// maker writes an IDENTITY rather than an ordinal. "The fourth object" is the
/// mistake this whole vocabulary is arranged to make unsayable.
///
/// It is a PRESENTATION type, Workshop-local, and it is not what gets stored:
/// `ui::Element::context` is the authored fact and it is a plain identity. This
/// is one of the two directions that identity is spoken in (the other is the
/// file's, in persist.hpp), and both are translations of the same number.
struct ContextRef {
    std::int64_t id = ui::kRootContext;

    friend bool operator==(const ContextRef&, const ContextRef&) = default;
};

/// The outcome of an attempted write. A refusal carries its reason, in words a
/// maker can act on -- the reason IS the feature, so there is no bare `false`.
struct Written {
    bool accepted = false;
    std::string refusal;

    static Written ok() { return Written{true, {}}; }
    static Written no(std::string why) { return Written{false, std::move(why)}; }
};

/// A typed connection to one property of one thing: read it, or try to write it.
///
/// Holds operations, not storage. Constructing one is the act of saying "this
/// label names that pair of semantic calls", and it is the only thing an editor
/// ever needs to know about a property.
template <class T>
class Property {
public:
    Property(std::function<T()> read, std::function<Written(T)> write)
        : read_(std::move(read)), write_(std::move(write)) {}

    T read() const { return read_(); }
    Written write(T value) const { return write_(std::move(value)); }

private:
    std::function<T()> read_;
    std::function<Written(T)> write_;
};

/// How a semantic type becomes text and text becomes it again. One
/// specialization per type, and every property of that type shares it.
///
/// `parse` returning nullopt means "this text is not a T at all" -- which is a
/// DIFFERENT fact from a T the property refuses, and the two are kept apart all
/// the way to the maker's eyes (see Commit below). `expected()` is what a valid
/// draft looks like, so a refusal can say what would have worked instead of only
/// that something did not.
template <class T> struct TextForm;

template <> struct TextForm<std::string> {
    static std::string format(const std::string& v) { return v; }
    static std::optional<std::string> parse(std::string_view text) {
        return std::string(text); // any text is a string; whether it is an
                                  // ACCEPTABLE one is the property's to say
    }
    static const char* expected() { return "any text"; }
};

template <> struct TextForm<std::int64_t> {
    static std::string format(std::int64_t v) { return std::to_string(v); }
    static std::optional<std::int64_t> parse(std::string_view text) {
        if (text.empty()) {
            return std::nullopt;
        }
        std::size_t i = 0;
        bool negative = false;
        if (text[0] == '-') {
            negative = true;
            i = 1;
            if (text.size() == 1) {
                return std::nullopt;
            }
        }
        std::int64_t value = 0;
        for (; i < text.size(); ++i) {
            const char c = text[i];
            if (c < '0' || c > '9') {
                return std::nullopt;
            }
            if (value > (9223372036854775807LL - (c - '0')) / 10) {
                return std::nullopt; // a number too big to be one
            }
            value = value * 10 + (c - '0');
        }
        return negative ? -value : value;
    }
    static const char* expected() { return "a whole number"; }
};

template <> struct TextForm<ui::Extent> {
    /// The canonical spelling: `12` for cells, `70%` for a share.
    static std::string format(const ui::Extent& v) {
        std::string out = std::to_string(v.amount);
        if (v.mode == ui::kExtentPercent) {
            out += '%';
        }
        return out;
    }

    /// Accepts BOTH `70%` and `70p`, and the second one is not a convenience --
    /// it is the only spelling a maker can currently type.
    ///
    /// The reason is worth keeping in the source: the Input package's locked
    /// vocabulary carries a SCANCODE, which is a physical key, and it has no
    /// modifier vocabulary at all (input/vocabulary.hpp says so). `%` is
    /// Shift+5. So the character this type FORMATS is a character no maker can
    /// ENTER through the current input path -- the display form of a value was
    /// not reachable by the input that edits it. `p` is the smallest honest
    /// bridge, and it stays until modifiers exist, at which point this branch
    /// and this comment go together.
    static std::optional<ui::Extent> parse(std::string_view text) {
        if (text.empty()) {
            return std::nullopt;
        }
        std::int64_t mode = ui::kExtentCells;
        if (text.back() == '%' || text.back() == 'p') {
            mode = ui::kExtentPercent;
            text.remove_suffix(1);
        }
        const std::optional<std::int64_t> amount = TextForm<std::int64_t>::parse(text);
        if (!amount) {
            return std::nullopt;
        }
        return ui::Extent{mode, *amount};
    }

    /// `70%`, because that is what a maker types -- the platform reports the
    /// character, so no workaround spelling has to be recommended. A refusal that
    /// recommends a workaround for a hole that has been filled is worse than no
    /// recommendation. `70p` is still ACCEPTED (whether
    /// this parser keeps a convenience spelling is its own question, and not
    /// one persistence gets to answer); it is simply no longer advertised.
    static const char* expected() { return "cells (12) or a share (70%)"; }
};

template <> struct TextForm<ContextRef> {
    /// `root`, or `#4`.
    static std::string format(const ContextRef& v) {
        return v.id == ui::kRootContext ? std::string("root")
                                        : "#" + std::to_string(v.id);
    }

    /// A CLOSED set of two spellings, for the same reason persist.hpp keeps a
    /// closed set of extent modes: an unrecognised draft must be "that is not a
    /// context" rather than something quietly defaulted. In particular a bare
    /// number is NOT accepted -- `4` would read as an ordinal to anyone who has
    /// used a layer panel, and the whole point of the field is that it is not
    /// one -- and `#0` is not accepted either, because 0 is not an identity any
    /// object can carry (see ui::kRootContext); the maker who means the root
    /// says `root`.
    static std::optional<ContextRef> parse(std::string_view text) {
        if (text == "root") {
            return ContextRef{ui::kRootContext};
        }
        if (text.size() < 2 || text.front() != '#') {
            return std::nullopt;
        }
        text.remove_prefix(1);
        const std::optional<std::int64_t> id = TextForm<std::int64_t>::parse(text);
        if (!id || *id <= ui::kRootContext) {
            return std::nullopt;
        }
        return ContextRef{*id};
    }

    /// Whether the identity EXISTS is not this type's question -- that is the
    /// document's, and it answers it with a refusal naming the number. Parsing
    /// says what a context looks like; the property says what this document will
    /// accept. The same division `TextForm<ui::Extent>` and `doc::check_extent`
    /// already keep.
    static const char* expected() { return "root or an identity (#1)"; }
};

/// What a commit attempt did. Three outcomes, not two, because a maker needs to
/// tell "that is not a width" from "that is a width and it is not allowed":
/// the first is answered by retyping, the second by wanting something else.
enum class Commit {
    Accepted,    ///< parsed, and the property took it
    Unparseable, ///< the draft is not a value of this type at all
    Refused      ///< it is a value of this type, and the property said no
};

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
///
/// AN INDEX RATHER THAN AN ERASE, since HD-3. `erase_one_character` used to be the only
/// consumer and did its own walking; a caret needs the same walk to MOVE without erasing, and
/// a second copy of the loop would be a second answer to "what is a character" -- right in one
/// place and wrong in the other the first day somebody improved one of them.
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
/// It also cannot overshoot a caret: a caret is always on a character boundary
/// (`TerminalInput`'s invariant), so a forward snap from at or before it lands at or before
/// it. That is what lets the two rules compose in either order.
inline std::size_t character_boundary_at_or_after(const std::string& line,
                                                  std::size_t at) noexcept {
    std::size_t i = at < line.size() ? at : line.size();
    while (i < line.size() && is_continuation_byte(line[i])) {
        ++i;
    }
    return i;
}

/// ERASE ONE CHARACTER FROM THE END OF A TYPED LINE, not one byte.
///
/// This is the whole of Workshop's Unicode editing: transport is honest, erase is
/// character-shaped, and NOTHING here claims grapheme clusters, combining marks or display
/// width.
///
/// IT IS A FREE FUNCTION because Workshop has two places a maker types -- an inspector draft
/// and the terminal overlay's command line (WT-1). The terminal's line reaches it through
/// `TerminalInput::backspace`, which erases before a CARET rather than at the end; both spend
/// `character_before`, which is why they cannot disagree.
inline void erase_one_character(std::string& line) {
    line.erase(character_before(line, line.size()));
}

/// One inspector line over one property, with an editor draft.
///
/// THE DRAFT IS NOT THE PROPERTY, and the whole class is arranged around that:
/// `type()` and `backspace()` touch only `draft_`, the property is read for
/// display and written by `commit()` alone, and a commit that does not succeed
/// leaves the property exactly as it was while KEEPING the draft, so the maker
/// can see and fix what they typed. `display()` marks a live draft so it can
/// never be mistaken for a committed value.
///
/// Type-erased on purpose: `edit<T>()` is the only place T appears, and it is
/// where TextForm<T> gets bound in. Everything after that is text, so the
/// inspector holds `std::vector<Row>` with rows of different types in it and
/// contains no per-type code whatsoever. That absence is the old builder's
/// duplication, gone.
class Row {
public:
    /// An editable row over a typed property. The one generic factory: the
    /// conversion plumbing comes from TextForm<T>, so a second property of the
    /// same type costs this one call and nothing else.
    template <class T> static Row edit(std::string label, Property<T> property) {
        Row row;
        row.label_ = std::move(label);
        row.editable_ = true;
        row.expected_ = TextForm<T>::expected();
        row.read_ = [property] { return TextForm<T>::format(property.read()); };
        row.commit_ = [property](const std::string& draft) -> std::pair<Commit, std::string> {
            const std::optional<T> parsed = TextForm<T>::parse(draft);
            if (!parsed) {
                return {Commit::Unparseable, {}};
            }
            const Written written = property.write(*parsed);
            if (!written.accepted) {
                return {Commit::Refused, written.refusal};
            }
            return {Commit::Accepted, {}};
        };
        return row;
    }

    /// A read-only row: something true about the object that is NOT one of its
    /// properties -- a resolved size, a derived count.
    ///
    /// It is a separate factory rather than a flag because the distinction is
    /// structural: a shown row has no Property behind it, so there is no path by
    /// which a maker could edit a resolved value under the impression they were
    /// authoring one. The phase's §10 asked that a resolved pixel value never be
    /// presented as though it were the authored property; here it cannot be,
    /// because it has nothing to write to.
    static Row show(std::string label, std::function<std::string()> read) {
        Row row;
        row.label_ = std::move(label);
        row.editable_ = false;
        row.read_ = std::move(read);
        return row;
    }

    const std::string& label() const { return label_; }
    bool editable() const { return editable_; }
    bool editing() const { return editing_; }
    const std::string& refusal() const { return refusal_; }
    const char* expected() const { return expected_; }

    /// The draft as it currently stands — empty when not editing. Exposed
    /// because it is already visible (display() shows it) and because a test
    /// that could only see the rendered form could not tell a preserved draft
    /// from a re-read value.
    const std::string& draft() const { return draft_; }

    /// The committed value, as text. Always a fresh read through the property --
    /// there is no cached copy to go stale, which is the other half of why the
    /// old builder needed a "refresh the inspector" call after every write.
    std::string value() const { return read_(); }

    /// What the maker sees: the committed value, or the live draft with a cursor
    /// on it. A draft is never displayed as if it were committed.
    std::string display() const { return editing_ ? draft_ + "_" : read_(); }

    /// Start editing from the current committed value.
    void begin() {
        if (!editable_ || editing_) {
            return;
        }
        editing_ = true;
        draft_ = read_();
        refusal_.clear();
    }

    void type(char c) {
        if (editing_) {
            draft_ += c;
        }
    }

    /// Text the platform said the maker entered, appended whole. A UTF-8
    /// character is up to four bytes and they belong together; appending it a
    /// byte at a time would be the same fact told in a way `backspace` could cut
    /// in half.
    void type(const std::string& text) {
        if (editing_) {
            draft_ += text;
        }
    }

    /// Erase one CHARACTER, not one byte -- through the one rule this tool has for it.
    void backspace() {
        if (editing_) {
            erase_one_character(draft_);
        }
    }

    /// Try to make the draft the property's value. On anything but Accepted the
    /// property is untouched, the row stays in edit with the draft intact, and
    /// `refusal()` says why in words.
    Commit commit() {
        if (!editing_) {
            return Commit::Accepted; // nothing was drafted; nothing changed
        }
        const std::pair<Commit, std::string> result = commit_(draft_);
        switch (result.first) {
        case Commit::Accepted:
            editing_ = false;
            draft_.clear();
            refusal_.clear();
            return Commit::Accepted;
        case Commit::Unparseable:
            refusal_ = "not " + std::string(expected_ == nullptr ? "valid" : expected_);
            return Commit::Unparseable;
        case Commit::Refused:
            refusal_ = result.second;
            return Commit::Refused;
        }
        return Commit::Refused; // unreachable; -Werror wants the return
    }

    /// Abandon the draft. The property was never touched, so there is nothing to
    /// undo -- which is the point of a draft being separate in the first place.
    void cancel() {
        editing_ = false;
        draft_.clear();
        refusal_.clear();
    }

private:
    Row() = default;

    std::string label_;
    bool editable_ = false;
    const char* expected_ = nullptr;
    std::function<std::string()> read_;
    std::function<std::pair<Commit, std::string>(const std::string&)> commit_;

    bool editing_ = false;
    std::string draft_;
    std::string refusal_;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PROPERTY_HPP
