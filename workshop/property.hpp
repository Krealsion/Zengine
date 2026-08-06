// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PROPERTY_HPP
#define ZENGINE_WORKSHOP_PROPERTY_HPP

// The typed connection between an editor and the property it presents.
//
// This is the one abstraction W-0 exists to test, and it exists because of a
// measured pain point rather than a design instinct: the historical builder
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

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "vocabulary.hpp"

#include "ui/vocabulary.hpp"

namespace zengine::workshop {

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

    static const char* expected() { return "cells (12) or a share (70% -- type 70p)"; }
};

/// What a commit attempt did. Three outcomes, not two, because a maker needs to
/// tell "that is not a width" from "that is a width and it is not allowed":
/// the first is answered by retyping, the second by wanting something else.
enum class Commit {
    Accepted,    ///< parsed, and the property took it
    Unparseable, ///< the draft is not a value of this type at all
    Refused      ///< it is a value of this type, and the property said no
};

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

    void backspace() {
        if (editing_ && !draft_.empty()) {
            draft_.pop_back();
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
