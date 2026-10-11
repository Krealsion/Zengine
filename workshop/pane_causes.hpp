// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_CAUSES_HPP
#define ZENGINE_WORKSHOP_PANE_CAUSES_HPP

// What a press may cause and the modes a declaration says, judged by their form: the rules
// Workshop admits a pane's or the application's declaration by beside its rows, which a declarer
// may ask of its own before it sends. Workshop judges the form and carries the declaration as it
// was said; it resolves nothing, and no cause is read from anywhere but the row.
// Reference: workshop/docs/workshop-panes.md#what-a-press-may-cause.

#include "workshop/pane_parts.hpp"
#include "workshop/pane_vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// The words a cause's subject opens with: the part a press lands on, or one pane always.
inline constexpr std::string_view kCauseOnPart = "on ";
inline constexpr std::string_view kCauseToPane = "to ";

/// WHAT IS WRONG WITH A MODE'S NAME, or nullptr: 1 to `kMaxPaneModeLen` bytes of lowercase ASCII
/// letters, digits and `-`, the spelling a census and a manual name it by.
// WL-CAUSE-02 -- agents/workshop/action-causes.md
inline const char* pane_mode_problem(std::string_view mode) noexcept {
    if (mode.empty()) return "a mode's name cannot be empty";
    if (mode.size() > kMaxPaneModeLen) return "a mode's name is too long";
    for (const char c : mode) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) {
            return "a mode's name is lowercase letters, digits and `-`";
        }
    }
    return nullptr;
}

namespace detail {

/// Printable ASCII with no space: a shape's name, an office, a pane key.
inline bool cause_word(std::string_view s) noexcept {
    if (s.empty()) return false;
    for (const char c : s) {
        const auto byte = static_cast<unsigned char>(c);
        if (byte <= 0x20u || byte >= 0x7Fu) return false;
    }
    return true;
}

} // namespace detail

/// WHAT IS WRONG WITH ONE CAUSE, or "": a shape's name of 1 to `kMaxCauseShapeLen` printable bytes
/// and no space, a version Loom can carry (1 or more), and a subject that is empty, `on <prefix>`
/// with a prefix a part's name could open with, or `to <office>/<pane>`.
// WL-CAUSE-01 -- agents/workshop/action-causes.md
inline std::string action_cause_problem(const ActionCause& c) {
    if (c.shape.size() > kMaxCauseShapeLen || !detail::cause_word(c.shape)) {
        return "a cause names a shape by 1 to " + std::to_string(kMaxCauseShapeLen) +
               " printable bytes with no space";
    }
    if (c.version < 1 || c.version > (std::numeric_limits<std::uint32_t>::max)()) {
        return "`" + c.shape + "`: a cause's version is 1 or more";
    }
    if (c.subject.empty()) return std::string();
    if (c.subject.size() > kMaxCauseSubjectLen) {
        return "`" + c.shape + "`: a cause's subject is too long";
    }
    const std::string_view subject = c.subject;
    if (subject.substr(0, kCauseOnPart.size()) == kCauseOnPart) {
        const std::string_view prefix = subject.substr(kCauseOnPart.size());
        if (pane_part_name_problem(prefix) != nullptr || prefix.front() == ' ') {
            return "`" + c.shape + "`: `on` names the opening of a part's name";
        }
        return std::string();
    }
    if (subject.substr(0, kCauseToPane.size()) == kCauseToPane) {
        const std::string_view pair = subject.substr(kCauseToPane.size());
        const std::size_t slash = pair.find('/');
        if (slash == std::string_view::npos || !detail::cause_word(pair.substr(0, slash)) ||
            !detail::cause_word(pair.substr(slash + 1)) ||
            pair.substr(slash + 1).find('/') != std::string_view::npos) {
            return "`" + c.shape + "`: `to` names one pane, `<office>/<pane>`";
        }
        return std::string();
    }
    return "`" + c.shape + "`: a cause's subject is empty, `on <prefix>` or `to <office>/<pane>`";
}

/// WHAT IS WRONG WITH ONE LIST OF CAUSES, or "": at most `bound`, each in form, none twice.
// WL-CAUSE-01 -- agents/workshop/action-causes.md
inline std::string action_causes_problem(const std::vector<ActionCause>& causes,
                                         std::size_t bound) {
    if (causes.size() > bound) {
        return "declares " + std::to_string(causes.size()) + " causes, more than " +
               std::to_string(bound);
    }
    for (std::size_t i = 0; i < causes.size(); ++i) {
        if (std::string wrong = action_cause_problem(causes[i]); !wrong.empty()) return wrong;
        for (std::size_t j = 0; j < i; ++j) {
            if (causes[j].shape == causes[i].shape && causes[j].version == causes[i].version &&
                causes[j].subject == causes[i].subject) {
                return "declares the cause `" + causes[i].shape + "` twice";
            }
        }
    }
    return std::string();
}

/// WHAT IS WRONG WITH A PANE'S DECLARATION BESIDE ITS KEYS, or "": every row's causes, and the
/// modes -- the mode in force named in form (empty only for a pane that names no other mode), at
/// most `kMaxPaneModes` other modes, each named once and never the mode in force, each with its
/// causes in form, and every row's `enters` one of them.
// WL-CAUSE-01, WL-CAUSE-02 -- agents/workshop/action-causes.md
inline std::string pane_declaration_problem(const v3::PaneActions& d) {
    for (const v3::PaneActionRow& row : d.rows) {
        if (std::string wrong = action_causes_problem(row.causes, kMaxActionCauses);
            !wrong.empty()) {
            return "`" + row.id + "`: " + wrong;
        }
    }
    if (!d.mode.empty()) {
        if (const char* wrong = pane_mode_problem(d.mode)) return wrong;
    } else if (!d.elsewhere.empty()) {
        return "a pane naming other modes names the mode its rows are in force for";
    }
    if (d.elsewhere.size() > kMaxPaneModes) {
        return "names " + std::to_string(d.elsewhere.size()) + " other modes, more than " +
               std::to_string(kMaxPaneModes);
    }
    for (std::size_t i = 0; i < d.elsewhere.size(); ++i) {
        const ModeCauses& other = d.elsewhere[i];
        if (const char* wrong = pane_mode_problem(other.mode)) return wrong;
        if (other.mode == d.mode) {
            return "`" + other.mode + "` is the mode in force, not another one";
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (d.elsewhere[j].mode == other.mode) {
                return "names the mode `" + other.mode + "` twice";
            }
        }
        if (std::string wrong = action_causes_problem(other.causes, kMaxModeCauses);
            !wrong.empty()) {
            return "mode `" + other.mode + "`: " + wrong;
        }
    }
    for (const v3::PaneActionRow& row : d.rows) {
        if (row.enters.empty()) continue;
        bool named = false;
        for (const ModeCauses& other : d.elsewhere) named = named || other.mode == row.enters;
        if (!named) {
            return "`" + row.id + "` enters `" + row.enters +
                   "`, a mode this declaration does not name";
        }
    }
    return std::string();
}

// ---- For a declarer ------------------------------------------------------------------------

/// A CAUSE NAMED BY ITS SHAPE'S OWN NAME AND VERSION, so a declaration never copies either.
template <class Shape>
ActionCause cause_of(std::string subject = {}) {
    return ActionCause{Shape::zen_name, static_cast<std::int64_t>(Shape::zen_version),
                       std::move(subject)};
}

/// The subject of a send about the part a press lands on, named `<prefix><s>`.
inline std::string on_part(std::string_view prefix) {
    return std::string(kCauseOnPart) + std::string(prefix);
}

/// The subject of a send always about one pane.
inline std::string to_pane(std::string_view office, std::string_view pane) {
    return std::string(kCauseToPane) + std::string(office) + "/" + std::string(pane);
}

/// WHAT EACH OTHER MODE MAY CAUSE, from the rows each mode declares: for every mode of `modes` but
/// `in_force`, the causes of `rows_of(mode)`, each once, in the order the rows first say them. A
/// pane that declares its rows by mode from one function says its `elsewhere` from the same one,
/// so the two cannot disagree.
template <class RowsOf>
std::vector<ModeCauses> causes_elsewhere(const std::vector<std::string>& modes,
                                         const std::string& in_force, RowsOf&& rows_of) {
    std::vector<ModeCauses> out;
    for (const std::string& mode : modes) {
        if (mode == in_force) continue;
        ModeCauses other{mode, {}};
        for (const v3::PaneActionRow& row : rows_of(mode)) {
            for (const ActionCause& c : row.causes) {
                bool seen = false;
                for (const ActionCause& had : other.causes) {
                    seen = seen || (had.shape == c.shape && had.version == c.version &&
                                    had.subject == c.subject);
                }
                if (!seen) other.causes.push_back(c);
            }
        }
        out.push_back(std::move(other));
    }
    return out;
}

/// Earlier rows as the third version says them: no causes, no mode entered.
inline v3::PaneActionRow action_row_of(const PaneActionRow& r) {
    return v3::PaneActionRow{r.id, r.label, r.scancode, r.modifiers, std::string(), {}, {}};
}
inline v3::PaneActionRow action_row_of(const v2::PaneActionRow& r) {
    return v3::PaneActionRow{r.id, r.label, r.scancode, r.modifiers, r.supersedes, {}, {}};
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_CAUSES_HPP
