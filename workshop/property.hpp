// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PROPERTY_HPP
#define ZENGINE_WORKSHOP_PROPERTY_HPP

// The typed connection between an editor and the property it presents.
// Workshop law: agents/workshop/document.md (+1 register; agents/workshop.md routes)

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

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
// WL-DOC-02 -- agents/workshop/document.md
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

// ⭐ `TextForm<ui::Extent>` (`12`, `70%`) AND `TextForm<ContextRef>` (`root`, `#4`) WERE HERE,
// the object document's two authored spellings; they retired with it. The rows that remain --
// a pane's placement, a region's -- are strings their setters parse (`parse_face_amount`).

/// What a commit attempt did. Three outcomes, not two, because a maker needs to
/// tell "that is not a width" from "that is a width and it is not allowed":
/// the first is answered by retyping, the second by wanting something else.
enum class Commit {
    Accepted,    ///< parsed, and the property took it
    Unparseable, ///< the text is not a value of this type at all
    Refused      ///< it is a value of this type, and the property said no
};


/// ONE INSPECTOR LINE OVER ONE PROPERTY: read fresh at every look, written by finished text.
///
/// ⭐ IT HELD A DRAFT UNTIL THE LAST DRAFT ON THIS SIDE LEFT. The Inspector's, then the host Pane
/// Manager's, were `TextBox` drafts inside the row, with the dozen gestures that edited them;
/// every line a maker types a property into is an inspector's own now (Info's, in its own
/// image), and what crosses to the row is the finished text `commit_text` judges.
// WL-DOC-02 -- agents/workshop/document.md
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
        row.commit_ = [property](const std::string& text) -> std::pair<Commit, std::string> {
            const std::optional<T> parsed = TextForm<T>::parse(text);
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

    /// A read-only row: something true about the subject that is NOT one of its
    /// properties -- a resolved size, a derived count.
    // WL-DOC-02 -- agents/workshop/document.md
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
    const std::string& refusal() const { return refusal_; }
    const char* expected() const { return expected_; }

    /// The committed value, as text. Always a fresh read through the property --
    /// there is no cached copy to go stale, which is why nothing in this package has a
    /// "refresh the inspector" call after a write.
    std::string value() const { return read_(); }

    /// COMMIT FINISHED TEXT -- the one write door, where a maker's draft lives in another image
    /// and only its finished text crosses. On anything but Accepted the property is untouched and
    /// `refusal()` says why in words: "not a whole number" for text that is not a value of the
    /// row's type, the setter's own sentence for a value it will not take, and "not authored" for
    /// a row with nothing to write to.
    // WL-DOC-02 -- agents/workshop/document.md
    Commit commit_text(const std::string& text) {
        if (!editable_) {
            refusal_ = "not authored";
            return Commit::Refused;
        }
        const std::pair<Commit, std::string> result = commit_(text);
        if (result.first == Commit::Unparseable) {
            refusal_ = "not " + std::string(expected_ == nullptr ? "valid" : expected_);
        } else if (result.first == Commit::Refused) {
            refusal_ = result.second;
        } else {
            refusal_.clear();
        }
        return result.first;
    }

private:
    Row() = default;

    std::string label_;
    bool editable_ = false;
    bool section_ = false;
    const char* expected_ = nullptr;
    std::function<std::string()> read_;
    std::function<std::pair<Commit, std::string>(const std::string&)> commit_;
    std::string refusal_;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PROPERTY_HPP
