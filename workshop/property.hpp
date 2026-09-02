// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PROPERTY_HPP
#define ZENGINE_WORKSHOP_PROPERTY_HPP

// The typed connection between an editor and the property it presents.
// Workshop law: agents/workshop/document.md (+2 registers; agents/workshop.md routes)

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "vocabulary.hpp"

#include "component/text_box.hpp"
#include "ui/vocabulary.hpp"

namespace zengine::workshop {

/// An authored context reference, in the spelling a maker reads and types.
// WL-DOC-11 -- agents/workshop/document.md
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
// WL-DOC-02 -- agents/workshop/document.md
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
// WL-DOC-02, WL-DOC-04 -- agents/workshop/document.md
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

    /// Accepts BOTH `70%` and `70p`.
    // WL-DOC-04 -- agents/workshop/document.md
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

    /// `70%`, because that is what a maker types.
    // WL-DOC-04 -- agents/workshop/document.md
    static const char* expected() { return "cells (12) or a share (70%)"; }
};

template <> struct TextForm<ContextRef> {
    /// `root`, or `#4`.
    static std::string format(const ContextRef& v) {
        return v.id == ui::kRootContext ? std::string("root")
                                        : "#" + std::to_string(v.id);
    }

    /// A CLOSED set of two spellings.
    // WL-DOC-11 -- agents/workshop/document.md
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

    // WL-DOC-02, WL-DOC-11 -- agents/workshop/document.md
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


/// One inspector line over one property, with an editor draft.
// WL-DOC-02 -- agents/workshop/document.md; WL-TEXT-01 -- agents/workshop/text-box.md
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
    // WL-DOC-05 -- agents/workshop/document.md
    static Row show(std::string label, std::function<std::string()> read) {
        Row row;
        row.label_ = std::move(label);
        row.editable_ = false;
        row.read_ = std::move(read);
        return row;
    }

    /// A SECTION HEADING INSIDE A LIST OF ROWS: a label with no value, never editable.
    // WL-PED-04 -- agents/workshop/pane-manager.md
    static Row section(std::string label) {
        Row row;
        row.label_ = std::move(label);
        row.editable_ = false;
        row.section_ = true;
        row.read_ = [] { return std::string(); };
        return row;
    }

    const std::string& label() const { return label_; }
    bool editable() const { return editable_; }
    bool section() const { return section_; }
    bool editing() const { return editing_; }
    const std::string& refusal() const { return refusal_; }
    const char* expected() const { return expected_; }

    /// THE DRAFT AS A COMPONENT — the text, the caret in it, and which part of it is on
    /// screen.
    // WL-TEXT-01 -- agents/workshop/text-box.md
    const component::TextBox& editor() const { return draft_; }

    /// The draft's text as it currently stands — empty when not editing. Exposed because
    /// it is already visible (display() shows it) and because a test that could only see the
    /// rendered form could not tell a preserved draft from a re-read value.
    const std::string& draft() const { return draft_.text(); }

    /// The committed value, as text. Always a fresh read through the property --
    /// there is no cached copy to go stale, which is the other half of why the
    /// old builder needed a "refresh the inspector" call after every write.
    std::string value() const { return read_(); }

    /// What the maker sees: the committed value, or the live draft.
    // WL-INFO-05 -- agents/workshop/info-body.md
    std::string display() const { return editing_ ? draft_.text() : read_(); }

    /// Start editing from the current committed value, WITH THE CARET AT ITS END.
    // WL-INFO-05 -- agents/workshop/info-body.md; WL-TEXT-03 -- agents/workshop/text-box.md
    void begin() {
        if (!editable_ || editing_) {
            return;
        }
        editing_ = true;
        std::string value = read_();
        const std::size_t at = value.size();
        draft_.set(std::move(value), at);
        refusal_.clear();
    }

    void type(char c) {
        if (editing_) {
            draft_.type(std::string(1, c));
        }
    }

    /// Text the platform said the maker entered, inserted whole AT THE CARET. A UTF-8
    /// character is up to four bytes and they belong together; inserting it a byte at a time
    /// would be the same fact told in a way `backspace` could cut in half.
    void type(const std::string& text) {
        if (editing_) {
            draft_.type(text);
        }
    }

    // THE EDITING GESTURES, EACH ONE LINE, EACH GUARDED.
    // WL-TEXT-01 -- agents/workshop/text-box.md

    /// Erase one CHARACTER before the caret, not one byte.
    void backspace() {
        if (editing_) {
            draft_.backspace();
        }
    }
    /// Erase the character AT the caret; the text after it comes back to meet it.
    void erase_forward() {
        if (editing_) {
            draft_.erase_forward();
        }
    }
    void left() {
        if (editing_) {
            draft_.left();
        }
    }
    void right() {
        if (editing_) {
            draft_.right();
        }
    }
    void home() {
        if (editing_) {
            draft_.home();
        }
    }
    void end() {
        if (editing_) {
            draft_.end();
        }
    }

    /// PUT THE CARET WHERE A POINTER LANDED — a byte index resolved from a column by
    /// whoever knows where this row's editable region is. Clamped and snapped by the
    /// component, so there is no index a press can produce that the draft will hold.
    void place(std::size_t at) {
        if (editing_) {
            draft_.place(at);
        }
    }

    /// SELECT THE WORD A POINTER LANDED IN — `place`'s other answer to one press,
    /// forwarded under the same guard: a row nobody opened has no word to select. The bool
    /// is the component's own — false where the position was in no word at all.
    bool select_word_at(std::size_t at) { return editing_ && draft_.select_word_at(at); }

    /// EXTEND THE SELECTION TO THE COLUMN A DRAG REACHED — `place`'s other half, forwarded
    /// under the same guard. The column is of the VALUE's visible slice, resolved
    /// by whoever knows where this row's editable region is.
    void drag_to_column(std::int64_t column) {
        if (editing_) {
            draft_.drag_to_column(column);
        }
    }

    /// SPEND ONE KEY TRANSITION ON THE DRAFT'S OWN VOCABULARY, or learn it is not the
    /// draft's.
    // WL-TEXT-06 -- agents/workshop/text-box.md
    bool consume(std::int64_t scancode, std::int64_t modifiers, component::Clipboard& clip) {
        return editing_ && draft_.consume(scancode, modifiers, clip);
    }

    /// APPLY THE TEXT A CONSUMED PASTE REQUEST ASKED FOR.
    // WL-TEXT-09 -- agents/workshop/text-box.md
    void paste(const component::Clipboard& clip) {
        if (editing_) {
            draft_.paste(clip);
        }
    }

    /// RECONCILE THE DRAFT'S WINDOW AGAINST THE ROOM THIS ROW HAS, once per repaint.
    // WL-TEXT-02, WL-TEXT-03 -- agents/workshop/text-box.md
    void keep_caret_visible(std::int64_t columns) {
        if (editing_) {
            draft_.keep_caret_visible(columns);
        }
    }

    /// Try to make the draft the property's value. On anything but Accepted the
    /// property is untouched, the row stays in edit with the draft intact, and
    /// `refusal()` says why in words.
    Commit commit() {
        if (!editing_) {
            return Commit::Accepted; // nothing was drafted; nothing changed
        }
        const std::pair<Commit, std::string> result = commit_(draft_.text());
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

    /// TAKE OVER A DRAFT FROM THE ROW THIS ONE REPLACES.
    // WL-INFO-06 -- agents/workshop/info-body.md; WL-TEXT-09 -- agents/workshop/text-box.md
    void resume(const Row& previous) {
        if (!editable_ || !previous.editing_) {
            return;
        }
        editing_ = true;
        draft_ = previous.draft_;
        refusal_ = previous.refusal_;
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
    bool section_ = false;
    const char* expected_ = nullptr;
    std::function<std::string()> read_;
    std::function<std::pair<Commit, std::string>(const std::string&)> commit_;

    bool editing_ = false;
    /// THE DRAFT, AND ONLY THE DRAFT.
    // WL-TEXT-01 -- agents/workshop/text-box.md
    component::TextBox draft_;
    std::string refusal_;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PROPERTY_HPP
