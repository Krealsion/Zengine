// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_KEYMAP_JOIN_HPP
#define ZENGINE_WORKSHOP_KEYMAP_JOIN_HPP

// What a weaver's keymap file and a declarer's rows must be to come into force: the renamed
// and retired ids, override admission, and a pane's and an application's rows joined under
// the collision law. A part of `workshop/keymap.hpp`, which includes it after the keymap
// value; nothing else includes it.
// Workshop law: agents/workshop/keyboard.md (+1 register; agents/workshop.md routes)

#include "pane_vocabulary.hpp" // PaneActionRow -- the rows a pane weave declares
#include "property.hpp"        // Written -- the one refusal-with-reason shape this package has

#include "component/text_box.hpp"
#include "input/vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// Action ids whose owner changed, and the id an authored row for each is read as now: the owner
/// moving changed no meaning, so the old id's row applies to the new one unless that is authored
/// itself, and the load names the rename. `workshop.picker` is not here: its meaning changed.
// WL-KEY-06 -- agents/workshop/keyboard.md
struct RenamedAction {
    const char* was;
    const char* now;
};
inline constexpr RenamedAction kRenamedActions[] = {
    {"workshop.terminal", "desktop.terminal"},
    {"workshop.hotkeys", "desktop.hotkeys"},
};

/// The id an authored row for `was` is read as now, or nullptr.
inline const char* renamed_to(std::string_view was) noexcept {
    for (const RenamedAction& r : kRenamedActions) {
        if (was == r.was) {
            return r.now;
        }
    }
    return nullptr;
}

/// Action ids that retired with what they acted on. An authored row for one is kept byte for byte
/// and the load says what retired; nothing answers it and no pane may declare it -- though a pane
/// naming one as the row it stands in for is admitted, standing in for nothing.
// WL-KEY-06 -- agents/workshop/keyboard.md
struct RetiredAction {
    const char* id;
    const char* with;    ///< what retired and took the action with it, in a weaver's words
    const char* instead; ///< what a weaver reaches for now, or empty when nothing took its place
};
/// WHERE THE PICKER'S AND THE HOST PANE MANAGER'S ACTS WENT, said once each for the rows below.
inline constexpr const char* kToPaneManager =
    "the desktop's Pane Manager (`desktop.panes`) shows and hides panes";
inline constexpr const char* kToPaneManagerAndInfo =
    "the desktop's Pane Manager (`desktop.panes`) shows and hides panes; Info inspects one";
inline constexpr const char* kToArranging = "arranging a pane (`workshop.manage`) orders it";
inline constexpr const char* kToInfoRows = "Info's own rows commit and cancel its edits";

inline constexpr RetiredAction kRetiredActions[] = {
    {"document.save", "the object document", ""},
    {"document.open", "the object document", ""},
    {"object.new", "the object canvas", ""},
    {"object.delete", "the object canvas", ""},
    {"object.left", "the object canvas", ""},
    {"object.down", "the object canvas", ""},
    {"object.up", "the object canvas", ""},
    {"object.right", "the object canvas", ""},
    {"object.narrower", "the object canvas", ""},
    {"object.taller", "the object canvas", ""},
    {"object.shorter", "the object canvas", ""},
    {"object.wider", "the object canvas", ""},
    {"object.next", "the object canvas", ""},
    {"workspace.narrower", "the object canvas", ""},
    {"workspace.wider", "the object canvas", ""},
    {"workshop.picker", "the `p` picker", kToPaneManager},
    {"picker.up", "the `p` picker", kToPaneManager},
    {"picker.down", "the `p` picker", kToPaneManager},
    {"picker.choose", "the `p` picker", kToPaneManager},
    {"picker.close", "the `p` picker", kToPaneManager},
    {"pane-editor.up", "the host's Pane Manager", kToPaneManagerAndInfo},
    {"pane-editor.down", "the host's Pane Manager", kToPaneManagerAndInfo},
    {"pane-editor.choose", "the host's Pane Manager", kToPaneManagerAndInfo},
    {"pane-editor.switch", "the host's Pane Manager", kToPaneManagerAndInfo},
    {"pane-editor.open", "the host's Pane Manager", kToPaneManagerAndInfo},
    {"pane-editor.front", "the host's Pane Manager", kToArranging},
    {"pane-editor.back", "the host's Pane Manager", kToArranging},
    {"pane-editor.raise", "the host's Pane Manager", kToArranging},
    {"pane-editor.lower", "the host's Pane Manager", kToArranging},
    {"draft.commit", "the host's Pane Manager", kToInfoRows},
    {"draft.cancel", "the host's Pane Manager", kToInfoRows},
};

/// What retired with `id`, or nullptr when it is not a retired Workshop action.
inline const char* retired_with(std::string_view id) noexcept {
    for (const RetiredAction& r : kRetiredActions) {
        if (id == r.id) {
            return r.with;
        }
    }
    return nullptr;
}

/// ...AND WHAT A WEAVER REACHES FOR NOW, or nullptr for an id that is not retired.
inline const char* retired_instead(std::string_view id) noexcept {
    for (const RetiredAction& r : kRetiredActions) {
        if (id == r.id) {
            return r.instead;
        }
    }
    return nullptr;
}

/// Whether the component's editable-text vocabulary owns this gesture wherever text has
/// the keyboard.
// WL-KEY-08 -- agents/workshop/keyboard.md
inline bool component_owns_gesture(const Gesture& g) noexcept {
    for (const component::EditingGesture& row : component::kEditingVocabulary) {
        if (row.scancode == g.scancode && row.modifiers == g.modifiers) {
            return true;
        }
    }
    return false;
}

/// APPLY AUTHORED OVERRIDE ROWS TO A CANDIDATE KEYMAP -- the format-independent half of
/// admission, shared by the file reader and by any suite staging overrides directly.
// WL-KEY-08 -- agents/workshop/keyboard.md
inline Written apply_overrides(
    const std::vector<std::pair<std::string, std::string>>& rows, std::int64_t legend,
    Keymap& out) {
    Keymap candidate;
    candidate.legend = legend;
    for (const std::pair<std::string, std::string>& row : rows) {
        candidate.authored.push_back(AuthoredOverride{row.first, row.second});
        const ActionRow* declared = row_of_id(row.first);
        if (declared == nullptr) {
            // Preserved with its authored intent whole -- see AuthoredOverride.
            continue;
        }
        const ParsedGesture parsed = parse_gesture(row.second);
        if (!parsed.accepted) {
            return Written::no("`" + row.first + "`: " + parsed.refusal);
        }
        // SEVERAL ROWS FOR ONE ID ARE ONE ACTION WITH SEVERAL KEYS (WL-KEY-08). What is refused
        // is a set that says nothing coherent: the same gesture twice, or `none` beside a key.
        for (const std::pair<Act, Gesture>& already : candidate.overrides) {
            if (already.first != declared->act) {
                continue;
            }
            if (already.second == parsed.gesture) {
                return Written::no("`" + row.first + "` is authored twice with `" + row.second +
                                   "` -- one row per key");
            }
            if (!is_bound(already.second) || !is_bound(parsed.gesture)) {
                return Written::no("`" + row.first +
                                   "` is authored both as `none` and as a key -- disable it "
                                   "or bind it, not both");
            }
        }
        // A kNoText row is active only where no text has the keyboard, so a bare
        // printable or an editing chord on one can never be swallowed by a field -- which
        // is why the two guards below are the global rows' alone.
        if (declared->context == KeyContext::kGlobal ||
            declared->context == KeyContext::kUnlessOwned) {
            if (parsed.gesture.modifiers == mod::kNone &&
                !expected_text_of(parsed.gesture.scancode, parsed.gesture.modifiers)
                     .empty()) {
                return Written::no(
                    "`" + row.first + "`: `" + row.second +
                    "` is a bare printable, and a bare printable cannot be global once "
                    "anything on the screen can take text");
            }
            if (component_owns_gesture(parsed.gesture)) {
                return Written::no(
                    "`" + row.first + "`: `" + row.second +
                    "` is the editing vocabulary's own gesture, which every text field "
                    "would consume first");
            }
        }
        candidate.overrides.emplace_back(declared->act, parsed.gesture);
        const char* gap = posix_gap(parsed.gesture);
        if (gap != nullptr) {
            if (!candidate.note.empty()) {
                candidate.note += "; ";
            }
            candidate.note += gap_sentence(row.second, row.first, gap);
        }
    }
    // The collision check runs over the effective map: an override can land on another action's
    // default. Rows of one action are one meaning.
    for (std::size_t i = 0; i < kActionCatalogCount; ++i) {
        for (std::size_t j = i + 1; j < kActionCatalogCount; ++j) {
            const ActionRow& a = kActionCatalog[i];
            const ActionRow& b = kActionCatalog[j];
            if (a.act == b.act || !contexts_intersect(a.context, b.context)) {
                continue;
            }
            // Two actions that answer to no key hold no gesture between them.
            for (const Gesture& ga : candidate.row_gestures(a)) {
                if (!is_bound(ga)) {
                    continue;
                }
                if (candidate.row_answers(b, ga)) {
                    return Written::no(collision_sentence(ga, a.id, b.id));
                }
            }
        }
    }
    out = std::move(candidate);
    return Written::ok();
}

/// THE GESTURES A WEAVER'S FILE AUTHORED FOR ONE ID, in authored order, judged as a set: every
/// row parses, no gesture twice, and `none` stands alone. `moved` says the file named the id at
/// all; an id it did not name keeps its declared default. With `renamed_too`, rows written for
/// an id's OLD name are read for it when the new name is not authored (`kRenamedActions`).
// WL-KEY-08 -- agents/workshop/keyboard.md
struct AuthoredGestures {
    bool moved = false;
    std::vector<Gesture> gestures;
    Written outcome = Written::ok();
};

inline AuthoredGestures authored_gestures_for(const std::vector<AuthoredOverride>& authored,
                                              std::string_view id, bool renamed_too) {
    AuthoredGestures out;
    const auto take = [&out, id](const AuthoredOverride& o, const std::string& spelled_as) {
        const ParsedGesture parsed = parse_gesture(o.gesture);
        if (!parsed.accepted) {
            out.outcome = Written::no("`" + spelled_as + "`: " + parsed.refusal);
            return false;
        }
        for (const Gesture& already : out.gestures) {
            if (already == parsed.gesture) {
                out.outcome = Written::no("`" + std::string(id) + "` is authored twice with `" +
                                          o.gesture + "` -- one row per key");
                return false;
            }
            if (!is_bound(already) || !is_bound(parsed.gesture)) {
                out.outcome = Written::no("`" + std::string(id) +
                                          "` is authored both as `none` and as a key -- "
                                          "disable it or bind it, not both");
                return false;
            }
        }
        out.gestures.push_back(parsed.gesture);
        out.moved = true;
        return true;
    };
    for (const AuthoredOverride& o : authored) {
        if (o.action == id && !take(o, std::string(id))) {
            return out;
        }
    }
    if (!out.moved && renamed_too) {
        for (const AuthoredOverride& o : authored) {
            const char* now = renamed_to(o.action);
            if (now == nullptr || id != now) {
                continue;
            }
            if (!take(o, o.action + " (read as `" + std::string(id) + "`)")) {
                return out;
            }
        }
    }
    return out;
}

// ---- A pane's declared rows, joined under the same law ------------------------------------

/// A DECLARED ROW'S TWO STRINGS, judged the way a descriptor's are (`check_pane_text`,
/// setup_reconcile.hpp): in bytes, before anything is kept, naming the field. An id is what a
/// keymap file spells and a legend never prints, so it may hold no space; a label is what
/// a legend prints, so a space is fine and a control byte is not.
// WL-KEY-15 -- agents/workshop/keyboard.md
inline Written check_pane_action_text(const std::string& text, const char* which,
                                      std::size_t limit, bool spaces_allowed) {
    if (text.empty()) {
        return Written::no(std::string("a pane action's ") + which + " cannot be empty");
    }
    if (text.size() > limit) {
        return Written::no(std::string("a pane action's ") + which + " is at most " +
                           std::to_string(limit) + " bytes");
    }
    bool anything = false;
    for (const char c : text) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte < 0x20u || byte >= 0x7Fu) {
            return Written::no(std::string("a pane action's ") + which +
                               " must be printable ASCII");
        }
        if (byte == ' ' && !spaces_allowed) {
            return Written::no(std::string("a pane action's ") + which +
                               " cannot contain a space");
        }
        if (byte != ' ') {
            anything = true;
        }
    }
    if (!anything) {
        return Written::no(std::string("a pane action's ") + which +
                           " needs more than spaces in it");
    }
    return Written::ok();
}

/// DOES ANY ROW OF THIS ONE DECLARATION STAND IN FOR THIS HOST ACTION? The collision law's
/// question, asked of the pane rather than of a row, because that is the scope dispatch uses.
// WL-KEY-15 -- agents/workshop/keyboard.md
inline bool superseded_here(const std::vector<PaneRow>& rows, const std::string& action_id) {
    for (const PaneRow& row : rows) {
        if (!row.supersedes.empty() && row.supersedes == action_id) {
            return true;
        }
    }
    return false;
}

/// Join one pane's declared rows into a keymap, or say why not -- over a value, so a suite asks it
/// with no bus. In order: the row count; each id and label; each default gesture (an unbound row
/// carries `kUnknown`); no id twice and none of Workshop's own; then the weaver's overrides by id;
/// then the collision law against the host rows active while a pane holds the keys and this
/// pane's own rows. Atomic: a refusal writes nothing, so the previous rows stand. It reads a row's
/// keys alone; what a press causes is judged beside it (`pane_declaration_problem`).
// WL-KEY-15 -- agents/workshop/keyboard.md
inline Written join_pane_rows(Keymap& k, std::int64_t pane,
                              const std::vector<v3::PaneActionRow>& declared) {
    if (declared.size() > kMaxPaneActionRows) {
        return Written::no("a pane declares at most " + std::to_string(kMaxPaneActionRows) +
                           " actions -- this one declared " + std::to_string(declared.size()));
    }
    constexpr std::int64_t kKnownModifiers = mod::kCtrl | mod::kShift | mod::kAlt | mod::kSuper;
    std::vector<PaneRow> rows;
    rows.reserve(declared.size());
    for (const v3::PaneActionRow& d : declared) {
        const Written id = check_pane_action_text(d.id, "id", kMaxPaneActionIdLen, false);
        if (!id.accepted) {
            return id;
        }
        const Written label =
            check_pane_action_text(d.label, "label", kMaxPaneActionLabelLen, true);
        if (!label.accepted) {
            return Written::no("`" + d.id + "`: " + label.refusal);
        }
        if (row_of_id(d.id) != nullptr) {
            return Written::no("`" + d.id +
                               "` is Workshop's own action id -- a pane's ids live in its "
                               "own namespace");
        }
        // ...AND SO IS ONE THAT RETIRED: a weaver's authored row for it is kept, and must not
        // come to move a stranger's key because a pane borrowed the spelling.
        if (const char* with = retired_with(d.id)) {
            return Written::no("`" + d.id + "` was Workshop's own action id, retired with " +
                               with + " -- a pane's ids live in its own namespace");
        }
        for (const PaneRow& earlier : rows) {
            if (earlier.id == d.id) {
                return Written::no("`" + d.id + "` is declared twice -- one row per action");
            }
        }
        if ((d.modifiers & ~kKnownModifiers) != 0) {
            return Written::no("`" + d.id + "`: modifier bits this keymap does not know");
        }
        if (d.scancode == scan::kUnknown) {
            if (d.modifiers != mod::kNone) {
                return Written::no("`" + d.id +
                                   "`: a row with no default key cannot carry modifiers");
            }
        } else if (key_name_of(d.scancode) == nullptr) {
            return Written::no("`" + d.id + "`: scancode " + std::to_string(d.scancode) +
                               " is not a key this keymap can name");
        }
        if (!d.supersedes.empty()) {
            const ActionRow* stands_for = row_of_id(d.supersedes);
            // ...or an above-the-modes application row (WL-DESK-07), which would otherwise take a
            // gesture from a pane holding the keyboard. A default-class row is asked only where
            // the keys did not cross here, so standing in for one is refused.
            const AppRow* app_stands_for = k.app_row_of_id(d.supersedes);
            // A RETIRED ID IS STOOD IN FOR BY NOBODY: the row is the pane's own, and there is no
            // host row for it to stand down. A pane built before the retirement keeps its keys.
            if (stands_for == nullptr && app_stands_for == nullptr &&
                retired_with(d.supersedes) != nullptr) {
                rows.push_back(
                    PaneRow{d.id, d.label, Gesture{d.scancode, d.modifiers}, std::string()});
                continue;
            }
            // ...AND AN APPLICATION ROW NO WEAVE DECLARES NOW stands down for nobody yet: the row
            // keeps its claim, which stands the application row down once one declares it.
            if (stands_for == nullptr && app_stands_for == nullptr &&
                d.supersedes.rfind(kApplicationIdPrefix, 0) == 0) {
                rows.push_back(
                    PaneRow{d.id, d.label, Gesture{d.scancode, d.modifiers}, d.supersedes});
                continue;
            }
            if (stands_for == nullptr && app_stands_for == nullptr) {
                return Written::no("`" + d.id + "`: `" + d.supersedes +
                                   "` is not an action id this Workshop knows -- a row can "
                                   "only stand in for an action that exists");
            }
            if (app_stands_for != nullptr && app_stands_for->precedence != 0) {
                return Written::no("`" + d.id + "`: `" + d.supersedes +
                                   "` is answered only where the keys did not reach this "
                                   "pane, so there is nothing here to stand in for");
            }
            if (stands_for != nullptr && stands_for->context != KeyContext::kUnlessOwned) {
                return Written::no("`" + d.id + "`: `" + d.supersedes +
                                   "` is not an action a pane may own -- only the rows "
                                   "Workshop declares a pane can stand in for");
            }
        }
        rows.push_back(PaneRow{d.id, d.label, Gesture{d.scancode, d.modifiers}, d.supersedes});
    }
    // The weaver's own file, applied to the ids it names: several authored rows for one id repeat
    // the pane's row once per gesture, so dispatch answers to any and a legend spells the first.
    {
        std::vector<PaneRow> widened;
        widened.reserve(rows.size());
        for (const PaneRow& row : rows) {
            const AuthoredGestures set = authored_gestures_for(k.authored, row.id, false);
            if (!set.outcome.accepted) {
                return set.outcome;
            }
            if (!set.moved) {
                widened.push_back(row);
                continue;
            }
            for (const Gesture& g : set.gestures) {
                PaneRow moved = row;
                moved.gesture = g;
                widened.push_back(std::move(moved));
            }
        }
        rows = std::move(widened);
    }
    // THE COLLISION LAW, over the effective map: the built-in rows active in a pane's
    // context, and this pane's own rows against each other. Same words as the file's.
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (!is_bound(rows[i].gesture)) {
            continue;
        }
        for (const ActionRow& host : kActionCatalog) {
            if (!contexts_intersect(host.context, KeyContext::kPane)) {
                continue;
            }
            // ...except the ones this pane stands in for: a superseded row is not requestable
            // anywhere in the pane, so it is judged for the pane, never per row.
            if (superseded_here(rows, host.id)) {
                continue;
            }
            if (k.row_answers(host, rows[i].gesture)) {
                return Written::no(collision_sentence(rows[i].gesture, host.id, rows[i].id));
            }
        }
        for (std::size_t j = i + 1; j < rows.size(); ++j) {
            if (rows[j].id != rows[i].id && rows[j].gesture == rows[i].gesture) {
                return Written::no(collision_sentence(rows[i].gesture, rows[i].id, rows[j].id));
            }
        }
        // ...and against the application rows (WL-DESK-07): active in every context, they meet
        // every pane's rows as a `kUnlessOwned` host row does, and stand down only for a pane that
        // declares `supersedes`.
        for (const AppRow& row : k.app) {
            // A default-class row is asked only where the keys did not cross to this pane, so it
            // and this pane's row can never both fire.
            if (row.precedence != 0 || !is_bound(row.gesture) ||
                superseded_here(rows, row.id)) {
                continue;
            }
            if (row.gesture == rows[i].gesture) {
                return Written::no(collision_sentence(rows[i].gesture, row.id, rows[i].id));
            }
        }
    }
    // ...AND WHAT THE TERMINAL CANNOT PRODUCE, judged on the rows in force, the weaver's
    // overrides included, as the file's own rows are: each row stays bound as it stands, and is
    // said (WL-KEY-08).
    std::vector<std::string> gaps;
    for (const PaneRow& row : rows) {
        if (!is_bound(row.gesture)) {
            continue;
        }
        if (const char* gap = posix_gap(row.gesture)) {
            gaps.push_back(gap_sentence(gesture_word(row.gesture), row.id, gap));
        }
    }
    for (PaneRows& p : k.panes) {
        if (p.pane == pane) {
            p.rows = std::move(rows);
            p.gaps = std::move(gaps);
            return Written::ok();
        }
    }
    k.panes.push_back(PaneRows{pane, std::move(rows), std::move(gaps)});
    return Written::ok();
}

/// The same join over second-version rows, widened with no causes.
// WL-KEY-15 -- agents/workshop/keyboard.md
inline Written join_pane_rows(Keymap& k, std::int64_t pane,
                              const std::vector<v2::PaneActionRow>& declared) {
    std::vector<v3::PaneActionRow> widened;
    widened.reserve(declared.size());
    for (const v2::PaneActionRow& d : declared) {
        widened.push_back(
            v3::PaneActionRow{d.id, d.label, d.scancode, d.modifiers, d.supersedes, {}, {}});
    }
    return join_pane_rows(k, pane, widened);
}

/// Join the application rows one participating owner declared (WL-DESK-07): `join_pane_rows` one
/// scope out -- the same bounds, namespace rule, overrides, collision law and atomic outcome --
/// judged against every host row and every pane's rows in force. The caller re-joins the panes
/// after, so both arrival orders end with one gesture meaning one thing.
// WL-DESK-07 -- agents/workshop/desktop.md
inline Written join_app_rows(Keymap& k, const std::vector<AppRow>& declared) {
    if (declared.size() > kMaxAppActionRows) {
        return Written::no("an application declares at most " +
                           std::to_string(kMaxAppActionRows) + " actions -- this one declared " +
                           std::to_string(declared.size()));
    }
    std::vector<AppRow> rows;
    rows.reserve(declared.size());
    for (const AppRow& d : declared) {
        const Written id = check_pane_action_text(d.id, "id", kMaxPaneActionIdLen, false);
        if (!id.accepted) {
            return id;
        }
        const Written label =
            check_pane_action_text(d.label, "label", kMaxPaneActionLabelLen, true);
        if (!label.accepted) {
            return Written::no("`" + d.id + "`: " + label.refusal);
        }
        if (row_of_id(d.id) != nullptr) {
            return Written::no("`" + d.id +
                               "` is Workshop's own action id -- an application's ids live in "
                               "its own namespace");
        }
        for (const AppRow& earlier : rows) {
            if (earlier.id == d.id) {
                return Written::no("`" + d.id + "` is declared twice -- one row per action");
            }
        }
        if (d.precedence != 0 && d.precedence != 1) {
            // A precedence this build cannot name is refused, not guessed: defaulting it would
            // give a later protocol's row the strongest position by accident.
            return Written::no("`" + d.id + "`: precedence " + std::to_string(d.precedence) +
                               " is not one this Workshop knows (0 above the modes, 1 default)");
        }
        constexpr std::int64_t kKnown = mod::kCtrl | mod::kShift | mod::kAlt | mod::kSuper;
        if ((d.gesture.modifiers & ~kKnown) != 0) {
            return Written::no("`" + d.id + "`: modifier bits this keymap does not know");
        }
        if (d.gesture.scancode == scan::kUnknown) {
            if (d.gesture.modifiers != mod::kNone) {
                return Written::no("`" + d.id +
                                   "`: a row with no default key cannot carry modifiers");
            }
        } else if (key_name_of(d.gesture.scancode) == nullptr) {
            return Written::no("`" + d.id + "`: scancode " +
                               std::to_string(d.gesture.scancode) +
                               " is not a key this keymap can name");
        }
        rows.push_back(d);
    }
    // THE WEAVER'S OWN FILE, applied to the ids it names -- including `none`, which is how a
    // weaver DISABLES an application default rather than moving it (WL-DESK-07) -- and, for an id
    // that changed owners, the row written for its old id when the new one is not authored.
    {
        std::vector<AppRow> widened;
        widened.reserve(rows.size());
        for (const AppRow& row : rows) {
            const AuthoredGestures set = authored_gestures_for(k.authored, row.id, true);
            if (!set.outcome.accepted) {
                return set.outcome;
            }
            if (!set.moved) {
                widened.push_back(row);
                continue;
            }
            for (const Gesture& g : set.gestures) {
                AppRow moved = row;
                moved.gesture = g;
                widened.push_back(std::move(moved));
            }
        }
        rows = std::move(widened);
    }
    // A ROW ANSWERED ABOVE EVERY MODE MEETS EVERY TEXT FIELD, so the file's two walls for a global
    // row are its walls too: no bare printable (every field would lose that character to it), and
    // no chord the text box owns (the field would consume it first, or never see it again).
    for (const AppRow& row : rows) {
        if (row.precedence != 0 || !is_bound(row.gesture)) {
            continue;
        }
        if (row.gesture.modifiers == mod::kNone &&
            !expected_text_of(row.gesture.scancode, row.gesture.modifiers).empty()) {
            return Written::no("`" + row.id + "`: `" + gesture_word(row.gesture) +
                               "` is a bare printable, and a bare printable cannot be answered "
                               "above every mode once anything on the screen can take text");
        }
        if (component_owns_gesture(row.gesture)) {
            return Written::no("`" + row.id + "`: `" + gesture_word(row.gesture) +
                               "` is the editing vocabulary's own gesture, which every text "
                               "field would consume first");
        }
    }
    // THE COLLISION LAW, over the effective map: these rows against the host's own, against
    // each other, and against every pane's rows in force. Same words as the file's.
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (!is_bound(rows[i].gesture)) {
            continue; // a disabled row collides with nothing -- it answers to no key
        }
        // The precedence class decides what a row can collide with. An above-the-modes row is
        // answered before the host rows and before the keys cross to a pane, so it meets all of
        // them. A default row meets none: it is asked only where the context claimed nothing and
        // no pane took the keys (`context.back` and `desktop.deselect` share Escape).
        if (rows[i].precedence == 0) {
            for (const ActionRow& host : kActionCatalog) {
                if (k.row_answers(host, rows[i].gesture)) {
                    return Written::no(
                        collision_sentence(rows[i].gesture, host.id, rows[i].id));
                }
            }
            for (const PaneRows& p : k.panes) {
                if (superseded_here(p.rows, rows[i].id)) {
                    continue; // that pane stands in for this row; inside it, it is not live
                }
                for (const PaneRow& pane_row : p.rows) {
                    if (pane_row.gesture == rows[i].gesture) {
                        return Written::no(
                            collision_sentence(rows[i].gesture, rows[i].id, pane_row.id));
                    }
                }
            }
        }
        // ...AND AGAINST EACH OTHER, IN THE SAME CLASS. Two application rows of one class on
        // one gesture are two declarations that could both fire, whichever class it is.
        for (std::size_t j = i + 1; j < rows.size(); ++j) {
            if (rows[j].id != rows[i].id && rows[j].precedence == rows[i].precedence &&
                rows[j].gesture == rows[i].gesture) {
                return Written::no(collision_sentence(rows[i].gesture, rows[i].id, rows[j].id));
            }
        }
    }
    k.app = std::move(rows);
    return Written::ok();
}

/// FORGET ONE PANE'S ROWS -- what a re-join under a new keymap file does with a pane
/// whose rows the file's own bindings now collide with.
// WL-KEY-15 -- agents/workshop/keyboard.md
inline void drop_pane_rows(Keymap& k, std::int64_t pane) {
    for (std::size_t i = 0; i < k.panes.size(); ++i) {
        if (k.panes[i].pane == pane) {
            k.panes.erase(k.panes.begin() + static_cast<std::ptrdiff_t>(i));
            return;
        }
    }
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_KEYMAP_JOIN_HPP
