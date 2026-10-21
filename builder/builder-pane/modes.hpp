// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_BUILDER_PANE_MODES_HPP
#define ZENGINE_BUILDER_PANE_MODES_HPP

// The Builder pane's two modes of its own: the recipe list, where a weaver looks through the
// catalog and takes a recipe as the choice, and the role line `load it` opens (WL-AUTH-02). The
// members of `BuilderPaneWeave` that open, steer, commit and say them.
// A part of `builder/builder-pane/pane.cpp`, which includes it after `BuilderPaneWeave` and its
// aliases; nothing else includes it.
// Builder law: agents/realization.md

#include "builder/builder-pane/vocabulary.hpp"
#include "builder/vocabulary.hpp"
#include "workshop/builder_seam_vocabulary.hpp"
#include "workshop/pane_seam_vocabulary.hpp"
#include "workshop/pane_text.hpp"

#include "component/list_window.hpp"
#include "component/text_box.hpp"
#include "graphics/role.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

// ---- The recipe list (the pane's own mode) -------------------------------------------

/// OPEN THE CATALOG AS ROWS, standing on the recipe the pane is already showing. Looking is
/// not choosing: nothing this list does moves `chosen` until `builder.recipe-choose`.
inline void BuilderPaneWeave::open_recipes(loom::Mail& mail) {
    if (!has_recipe("there is nothing to choose between")) {
        say(mail);
        return;
    }
    choosing_ = Choosing{};
    choosing_.open = true;
    choosing_.name = known_.recipes[cursor_row()].recipe;
    declare(mail);
    notice_ = "choose a recipe -- Return takes it, Escape leaves the choice as it was";
    say(mail);
}

inline void BuilderPaneWeave::close_recipes(loom::Mail& mail, std::string said) {
    choosing_ = Choosing{};
    declare(mail);
    notice_ = std::move(said);
    say(mail);
}

/// WHERE THE LIST'S CURSOR STANDS, by name and re-derived every time: a catalog that moved
/// while the list was open moves the cursor with the recipe it named, and one that dropped
/// the recipe leaves the cursor on the row the pane is showing.
inline std::size_t BuilderPaneWeave::list_row() const {
    const std::size_t at = named_row(choosing_.name);
    return at < known_.recipes.size() ? at : cursor_row();
}

inline void BuilderPaneWeave::choose_in_list(const std::string& id, loom::Mail& mail) {
    if (id == pane::kActionRecipesUp || id == pane::kActionRecipesDown) {
        if (known_.recipes.empty()) {
            say(mail);
            return;
        }
        const std::int64_t held = static_cast<std::int64_t>(known_.recipes.size());
        std::int64_t to = static_cast<std::int64_t>(list_row()) +
                          (id == pane::kActionRecipesUp ? -1 : 1);
        to = to < 0 ? 0 : (to >= held ? held - 1 : to);
        choosing_.name = known_.recipes[static_cast<std::size_t>(to)].recipe;
        say(mail);
    } else if (id == pane::kActionRecipeChoose) {
        take_choice(mail);
    } else if (id == pane::kActionRecipesClose) {
        close_recipes(mail, "the list is closed -- the choice is unchanged");
    } else if (id == pane::kActionEditSource) {
        // The list's own row, acting on the list's cursor: `edit_source` reads `list_row()`
        // while the list is open, so the recipe opened is the one the row named, and the
        // committed choice is not touched (WL-PROJ-14: looking is not choosing).
        edit_source(mail);
    } else if (id == pane::kActionMenu) {
        offer_here(mail, mail.correlation());
        say(mail);
    }
}

/// TAKE THE ROW THE LIST IS STANDING ON AS THE WEAVER'S PICK. The same two writes `c` makes
/// -- the choice and the record that it was PICKED (WL-PROJ-14) -- so the frontier action
/// reads a choice made here exactly as it reads one made by the key.
inline void BuilderPaneWeave::take_choice(loom::Mail& mail) {
    const std::size_t at = list_row();
    if (at >= known_.recipes.size()) {
        close_recipes(mail, "that recipe is not in the catalog any more -- nothing was chosen");
        return;
    }
    state_.chosen = known_.recipes[at].recipe;
    picked_ = state_.chosen;
    close_recipes(mail, "build recipe: " + known_.recipes[at].recipe + " -> " +
                            known_.recipes[at].artifact);
}

inline std::vector<BuilderPaneWeave::ControlRow> BuilderPaneWeave::list_controls() const {
    const std::size_t at = list_row();
    std::vector<ControlRow> controls;
    controls.push_back(ControlRow{pane::kActionMenu, "menu", true});
    controls.push_back(ControlRow{pane::kActionRecipeChoose, "choose this recipe",
                                  at < known_.recipes.size()});
    // (!!) NEITHER FACE NAMES THE RECIPE, AND THAT IS DELIBERATE. What they act on is the
    // list's CURSOR, which the `> ` marker says and only the weaver's own act moves -- and
    // a face that named it would move this strip's spans on every row the weaver looked at,
    // so the picture fence would refuse the second press of an ordinary double-click. The
    // subject a face NAMES is checked (`perform_on`); the subject a face points at is the
    // one the weaver can see.
    controls.push_back(ControlRow{pane::kActionEditSource, "edit this recipe's source",
                                  at < known_.recipes.size()});
    controls.push_back(ControlRow{pane::kActionRecipesClose, "close the list", true});
    return controls;
}

/// THE RECIPE LIST: the catalog on rows, with the list's own cursor on one of them.
///
/// (!!) THE CURSOR IS NOT THE CHOICE, and the heading says which row IS. A list whose cursor
/// was the choice would arm the next build against whatever a weaver was merely looking at.
inline void BuilderPaneWeave::say_recipes() {
    const std::size_t held = known_.recipes.size();
    const std::size_t at = list_row();
    std::string head = "choose a recipe -- " + std::to_string(held) +
                       (held == 1 ? " recipe" : " recipes");
    if (!known_.source.empty()) {
        head += " in " + known_.source;
    }
    push_row(head, zengine::graphics::role::kAccent);
    const std::vector<ControlRow> controls = list_controls();
    const std::int64_t body_rows =
        rows() - 1 - (notice_.empty() ? 0 : 1) - room().strip_rows(controls);
    if (body_rows > 0 && held > 0) {
        const component::ListWindow win = component::cursor_window(
            held, at, choosing_.hint, static_cast<std::size_t>(body_rows));
        choosing_.hint = win.first;
        // A marker is a row of the same budget (`PaneRowsFacet::push_window`), so the control
        // strip, the one row a weaver with a mouse cannot lose, keeps its place.
        (void)room().push_window(win, fit_row(), [&](std::size_t i) {
            const builder::RecipeSummary& row = known_.recipes[i];
            const bool here = i == at;
            const bool chosen = row.recipe == state_.chosen;
            push_row(std::string(here ? "> " : "  ") + row.recipe + " -> " + row.artifact +
                         (chosen ? "   (chosen)" : ""),
                     here ? zengine::graphics::role::kAccent : zengine::graphics::role::kFill,
                     BuilderMeaning{builder_row::kRecipe, i, {}, row.recipe});
        });
    }
    say_controls(controls);
}

// ---- Load it: the role line, and the plan row it commits --------------------------------

inline std::string trimmed(const std::string& text) {
    std::size_t b = 0;
    std::size_t e = text.size();
    while (b < e && text[b] == ' ') {
        ++b;
    }
    while (e > b && text[e - 1] == ' ') {
        --e;
    }
    return text.substr(b, e - b);
}

/// THE FLOOR THE ROLE LINE'S TYPED VALUE NEVER GIVES UP -- Files' `kMinFieldValueColumns`, for
/// Files' reason: the role line's own prompt grows with the stem being loaded (`role for
/// zengine-really-long-example> `), and the same construction that hid a narrow Files field's
/// typed text applies here unless the value keeps a useful minimum (`fitted_label`).
constexpr std::int64_t kMinRoleValueColumns = 12;

/// Load it, in two beats: the chosen recipe's artifact gains the minimum plan row, with a
/// role the weaver types, so the gesture first asks the host whether the plan already names
/// the artifact and opens the line only if it does not.
inline void BuilderPaneWeave::begin_load_it(loom::Mail& mail) {
    if (!has_recipe("nothing to load")) {
        say(mail);
        return;
    }
    const builder::RecipeSummary& row = known_.recipes[cursor_row()];
    (void)asks().forget(of_kind(Act::kNames));
    BuilderAsk names = asking(Act::kNames);
    names.subject = row.recipe;
    names.choice = choice_; // nonzero only when a menu row began this
    if (asks().ask(mail, ws::kProjectRole, PlanNamesRequested{row.artifact},
                   std::move(names)) != Asks::Asked::kHeld) {
        notice_ = "`" + row.recipe + "`: nothing was added to the plan -- nothing was queued "
                  "to " + ws::kProjectRole;
        say(mail);
    }
}

inline void BuilderPaneWeave::open_role(const std::string& stem, const std::string& recipe) {
    role_ = Role{};
    role_.open = true;
    role_.stem = stem;
    role_.recipe = recipe;
    role_.line.set(std::string(), 0);
    notice_ = "load `" + stem + "` -- type the role it holds";
}

inline void BuilderPaneWeave::close_role() { role_ = Role{}; }

/// THE COMMIT WRITES A PLAN ROW AND BUILDS: the plan file takes the row, and the plan may
/// load what is built at once and then build. So the gesture asks Workshop both classes in
/// one ask, with the row captured as typed, and the line stays open until Workshop answers:
/// a refusal leaves the typed role standing to commit again (`commit_approved`).
inline void BuilderPaneWeave::commit_role(loom::Mail& mail) {
    const std::string typed = trimmed(role_.line.text());
    // THE ROLE, THEN THE HOST. An empty role is refused here in the plan's own words
    // (`check_weave_role` says the same), and everything else -- the stem, the duplicate,
    // the executor's state, the file -- is the host's to say.
    if (typed.empty()) {
        notice_ = "a weave declaration needs a role -- nothing was loaded";
        say(mail);
        return;
    }
    BuilderAsk a;
    a.act = Act::kRow;
    a.subject = role_.recipe;
    a.stem = role_.stem;
    a.role = typed;
    a.undone = "nothing was loaded and nothing was written";
    ask_approval(mail, std::move(a), mail.correlation());
}

/// THE COMMIT, APPROVED: the row captured at the gesture is asked of the project office, and
/// its ask carries the approval to the build its answer may finish with
/// (`on(PlanRowWritten)`). A line closed while Workshop was asked is no commit any more, and
/// writes nothing.
inline void BuilderPaneWeave::commit_approved(const BuilderAsk& a, loom::Mail& mail) {
    if (!role_.open || role_.stem != a.stem || role_.recipe != a.subject) {
        say_undone(a, "the line was closed before Workshop answered", mail);
        return;
    }
    (void)asks().forget(of_kind(Act::kPlanRow));
    BuilderAsk row = a;
    row.act = Act::kPlanRow;
    if (asks().ask(mail, ws::kProjectRole, PlanRowRequested{a.stem, a.role, a.subject},
                   std::move(row)) != Asks::Asked::kHeld) {
        say_undone(a, std::string("nothing was queued to ") + ws::kProjectRole, mail);
        return;
    }
    close_role();
    declare(mail);
    say(mail);
}

inline std::vector<BuilderPaneWeave::ControlRow> BuilderPaneWeave::role_controls() const {
    std::vector<ControlRow> controls;
    controls.push_back(ControlRow{pane::kActionMenu, "menu", true});
    controls.push_back(ControlRow{pane::kActionCommit, "load it with this role",
                                  !trimmed(role_.line.text()).empty()});
    controls.push_back(ControlRow{pane::kActionCancel, "cancel -- load nothing", true});
    return controls;
}

/// THE PROMPT THE ROLE LINE DRAWS IN FRONT OF WHAT A WEAVER IS TYPING -- the label's own
/// full text, unshortened: what `active_role_prompt` shortens FROM.
inline std::string BuilderPaneWeave::role_prompt() const { return "role for " + role_.stem + "> "; }

/// THE ROLE PROMPT AS DRAWN -- `role_prompt`'s full label in every room wide enough to give
/// the typed value `kMinRoleValueColumns` beside it, shortened otherwise (`fitted_label`).
/// Read here and by the press handler alike, so painting and the caret math it feeds never
/// disagree about where the value begins (`files/recipe_authoring.hpp`'s `active_prompt`, the
/// same repair for the same construction).
inline std::string BuilderPaneWeave::active_role_prompt() const {
    return fitted_label(role_prompt(), columns(), kMinRoleValueColumns);
}

/// The columns the role line's value is drawn in beside `prompt`: one is kept for the caret.
inline std::int64_t BuilderPaneWeave::role_value_columns(const std::string& prompt) const {
    return columns() > static_cast<std::int64_t>(prompt.size()) + 1
               ? columns() - static_cast<std::int64_t>(prompt.size()) - 1
               : 1;
}

/// The byte of `shown` a press at spelled `column` names: the last character boundary whose
/// spelling (`ascii_spelling`) is no wider than the column.
inline std::int64_t BuilderPaneWeave::spelled_column_byte(const std::string& shown,
                                                          std::int64_t column) {
    std::int64_t best = 0;
    for (std::size_t b = 1; b <= shown.size(); ++b) {
        if (b < shown.size() && (static_cast<unsigned char>(shown[b]) & 0xC0u) == 0x80u) {
            continue; // inside a character
        }
        if (static_cast<std::int64_t>(ascii_spelling(shown.substr(0, b)).size()) > column) {
            break;
        }
        best = static_cast<std::int64_t>(b);
    }
    return best;
}

inline void BuilderPaneWeave::say_role() {
    const std::string prompt = active_role_prompt();
    const std::int64_t cols = role_value_columns(prompt);
    role_.line.keep_caret_visible(cols);
    const std::string shown = role_.line.visible(cols);
    if (static_cast<std::int64_t>(room().composing().size()) < rows()) {
        // THE CARET STANDS AFTER WHAT IS DRAWN BEFORE IT: the row is spelled in what a canvas
        // draws, so its column is counted in the spelled characters, not the typed bytes.
        room().caret().row = static_cast<std::int64_t>(room().composing().size());
        room().caret().column = static_cast<std::int64_t>(
            ascii_spelling(prompt + shown.substr(0, role_.line.caret_column())).size());
    }
    push_row(prompt + shown, zengine::graphics::role::kAccent,
             BuilderMeaning{builder_row::kLine, 0, {}, role_.stem});
    push_row(labelled_row("loads", role_.stem + " (built by `" + role_.recipe + "`)"),
             zengine::graphics::role::kMuted);
    say_controls(role_controls());
}

} // namespace

#endif // ZENGINE_BUILDER_PANE_MODES_HPP
