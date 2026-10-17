// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_FILES_RECIPE_AUTHORING_HPP
#define ZENGINE_FILES_RECIPE_AUTHORING_HPP

// The Files pane's two authoring modes: the members of `FilesWeave` that pick something
// buildable, walk the recipe form's fields, ask for the write, and offer and draw both modes,
// with the pure helpers only they read. A part of `files/files.cpp`, which includes it after
// `FilesWeave` and its aliases; nothing else includes it.
// Files law: agents/workshop/files.md

#include "files/files.hpp"
#include "files/vocabulary.hpp"
#include "workshop/actor_scope.hpp"
#include "workshop/pane_menu.hpp"
#include "workshop/pane_seam_vocabulary.hpp"
#include "workshop/pane_text.hpp"
#include "workshop/persist.hpp"

#include "component/list_window.hpp"
#include "component/text_box.hpp"
#include "surface/vocabulary.hpp"

#include <zen/weave.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

// ---- Small pure helpers the recipe chooser needs ------------------------------------------

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

inline std::vector<std::string> split_list(const std::string& text) {
    std::vector<std::string> out;
    std::string item;
    for (const char c : text) {
        if (c == ',') {
            const std::string t = trimmed(item);
            if (!t.empty()) {
                out.push_back(t);
            }
            item.clear();
        } else {
            item.push_back(c);
        }
    }
    const std::string t = trimmed(item);
    if (!t.empty()) {
        out.push_back(t);
    }
    return out;
}

inline bool source_name(const std::string& name) {
    for (const char* suffix : {".cpp", ".cc", ".cxx"}) {
        const std::string s(suffix);
        if (name.size() > s.size() && name.compare(name.size() - s.size(), s.size(), s) == 0) {
            return true;
        }
    }
    return false;
}

inline std::string stem_of(const std::string& name) {
    const std::size_t dot = name.rfind('.');
    return dot == std::string::npos || dot == 0 ? name : name.substr(0, dot);
}

/// Whether a configured CMake build tree needs `cmake --build --config` to say which
/// configuration it means: a multi-config generator writes `CMAKE_CONFIGURATION_TYPES` into its
/// cache, a single-config one never does. Read, never invoked; this package configures nothing.
inline bool cache_is_multi_config(const std::filesystem::path& cache_file) {
    std::ifstream in(cache_file);
    if (!in) {
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("CMAKE_CONFIGURATION_TYPES:", 0) == 0) {
            const std::size_t eq = line.find('=');
            return eq != std::string::npos && eq + 1 < line.size();
        }
    }
    return false;
}

using zengine::workshop::pane_text::fitted_label;

/// The floor the active field's value never gives up, however long its label: in a thirty-column
/// body `package prefix (comma-separated)> ` alone fills the room, and typing was accepted and
/// never shown. Twelve columns is enough to read what was typed, not a promise nothing scrolls.
constexpr std::int64_t kMinFieldValueColumns = 12;

// ---- The chooser's typed fields -----------------------------------------------------------

// The fifth field is offered only when the tree says several configurations coexist there
// (`cache_is_multi_config`): ask for the few things nothing can detect. `name` carries the
// why and is pane-local text, clipped to the pane's width; `menu_label` carries the what and
// crosses as a `PaneMenuRow`, where one row over 64 bytes makes the presenter refuse the
// whole offer (`workshop/menu-presenter/presenter.cpp`'s `refusal_of`) -- every menu this form offers.
inline const FilesWeave::Field& FilesWeave::field_at(bool tree, bool multi_config,
                                                     std::size_t step) {
    static constexpr Field kSource[] = {{"recipe name", true},
                                        {"artifact stem", true},
                                        {"package prefix (comma-separated)", true},
                                        {"link targets (comma-separated)", true}};
    static constexpr Field kTree[] = {{"recipe name", true},
                                      {"cmake target", true},
                                      {"artifact stem", true},
                                      {"artifact directory (optional)", false}};
    static constexpr Field kConfig = {"configuration (this tree builds several; cmake "
                                      "--build needs one)",
                                      true, "configuration"};
    if (tree && multi_config && step == 4) {
        return kConfig;
    }
    return tree ? kTree[step] : kSource[step];
}

inline const char* FilesWeave::field_name(bool tree, bool multi_config, std::size_t step) {
    return field_at(tree, multi_config, step).name;
}

/// THE SHORT FORM FOR A MENU ROW -- `field.menu_label` when the field declared one, `name`
/// otherwise. The one place the fallback is spelled; every menu-row site reads through this,
/// never `field.name` or `.menu_label` directly.
inline const char* FilesWeave::field_menu_label(bool tree, bool multi_config,
                                                std::size_t step) {
    const Field& field = field_at(tree, multi_config, step);
    return field.menu_label ? field.menu_label : field.name;
}

inline std::size_t FilesWeave::field_count(bool tree, bool multi_config) {
    return (tree && multi_config) ? 5 : 4;
}

/// THE CANDIDATE THE AUTHORING FORM IS OPEN ON OWNS ITS OWN FIELD COUNT -- every call site
/// below reads `authoring_.chosen`, so this is the one place that says so.
inline std::size_t FilesWeave::field_count() const {
    return field_count(authoring_.chosen.tree, authoring_.chosen.multi_config);
}

// ---- Pick buildable, and the recipe form --------------------------------------------------

inline void FilesWeave::pick_buildable(loom::Mail& mail) {
    if (!listing_.known || state_.current_dir.empty()) {
        notice_ = "nothing is listed here -- nothing to pick from";
        say(mail);
        return;
    }
    Chooser chooser;
    chooser.dir = state_.current_dir;
    for (const FileRow& row : listing_.rows) {
        // A candidate's name is typed into the recipe form, whose line is measured in bytes,
        // so only a name the screen draws as itself is offered.
        if (!row.openable || row.name != ws::shown_name(row.name)) {
            continue;
        }
        if (row.directory) {
            std::error_code ec;
            const std::filesystem::path cache =
                std::filesystem::path(state_.current_dir) / row.name / "CMakeCache.txt";
            const bool configured = std::filesystem::exists(cache, ec);
            if (configured && !ec) {
                chooser.candidates.push_back(
                    BuildCandidate{row.name, true, cache_is_multi_config(cache)});
            }
        } else if (source_name(row.name)) {
            chooser.candidates.push_back(BuildCandidate{row.name, false});
        }
    }
    if (chooser.candidates.empty()) {
        notice_ = "nothing buildable in " + state_.current_dir +
                  " -- no source file, and no directory holding a configured CMake tree";
        say(mail);
        return;
    }
    chooser.open = true;
    chooser_ = std::move(chooser);
    declare(mail); // this mode's own rows, and no others
    notice_ = "pick something buildable -- Return authors a recipe, Escape cancels";
    say(mail);
}

inline void FilesWeave::chooser_choose(loom::Mail& mail) {
    if (chooser_.cursor >= chooser_.candidates.size()) {
        chooser_ = Chooser{};
        declare(mail);
        say(mail);
        return;
    }
    Authoring a;
    a.open = true;
    a.chosen = chooser_.candidates[chooser_.cursor];
    a.dir = chooser_.dir;
    a.values.assign(field_count(a.chosen.tree, a.chosen.multi_config), std::string());
    a.filled.assign(field_count(a.chosen.tree, a.chosen.multi_config), false);
    authoring_ = std::move(a);
    chooser_ = Chooser{};
    field_hint_ = 0;
    load_field(0);
    // THE LINE TAKES THE KEYBOARD: the mode's own rows are declared, and everything else
    // reaches the line as an ordinary key.
    declare(mail);
    notice_ = std::string(authoring_.chosen.tree ? "a configured tree: " : "a source file: ") +
              authoring_.chosen.name + " -- Return commits a field, Escape cancels";
    say(mail);
}

/// What a field is seeded with when it has never been answered: the candidate's stem for the
/// recipe name, and the artifact stem following whatever names the thing built. An answered
/// field is seeded with the weaver's answer, never re-suggested over it.
inline std::string FilesWeave::suggestion_for(std::size_t which) const {
    const bool tree = authoring_.chosen.tree;
    if (which == 0) {
        return tree ? authoring_.chosen.name : stem_of(authoring_.chosen.name);
    }
    if (!tree && which == 1) {
        return authoring_.values[0];
    }
    if (tree && which == 2) {
        return authoring_.values[1];
    }
    return std::string();
}

/// STAND THE LINE ON A FIELD: its prompt, its text, the caret at the end of it.
inline void FilesWeave::load_field(std::size_t which) {
    authoring_.step = which;
    const Field& field = field_at(authoring_.chosen.tree, authoring_.chosen.multi_config, which);
    const std::string text =
        authoring_.filled[which] ? authoring_.values[which] : suggestion_for(which);
    authoring_.prompt = std::string(field.name) + "> ";
    authoring_.line.set(text, text.size());
}

/// KEEP WHAT THE LINE HOLDS, WITHOUT JUDGING IT. Leaving a field is not writing anything,
/// so an empty required field is kept empty here and refused where it matters -- at the
/// write. Judging it here would make a weaver unable to look at the next field.
inline void FilesWeave::stash_field() {
    const std::string typed = trimmed(authoring_.line.text());
    authoring_.values[authoring_.step] = typed;
    if (!typed.empty()) {
        authoring_.filled[authoring_.step] = true;
    }
}

/// Commit the field the line is standing on, refusing an empty required one. Returns whether
/// it took.
inline bool FilesWeave::record_field(loom::Mail& mail) {
    const std::string typed = trimmed(authoring_.line.text());
    const Field& field =
        field_at(authoring_.chosen.tree, authoring_.chosen.multi_config, authoring_.step);
    if (field.required && typed.empty()) {
        notice_ = std::string(field.name) + " is required -- nothing was written";
        say(mail);
        return false;
    }
    authoring_.values[authoring_.step] = typed;
    authoring_.filled[authoring_.step] = true;
    return true;
}

/// STAND ON ANOTHER FIELD, keeping what the current one holds.
inline void FilesWeave::edit_field(std::size_t which, loom::Mail& mail) {
    if (!authoring_.open || which >= field_count()) {
        return;
    }
    if (which != authoring_.step) {
        stash_field();
        load_field(which);
    }
    say(mail);
}

/// IS EVERY REQUIRED FIELD ANSWERED? Read for the write control's face, and asked again by
/// the write itself.
inline bool FilesWeave::filled_in() const {
    if (!authoring_.open) {
        return false;
    }
    for (std::size_t i = 0; i < field_count(); ++i) {
        const Field& field = field_at(authoring_.chosen.tree, authoring_.chosen.multi_config, i);
        const bool answered = i == authoring_.step
                                  ? !trimmed(authoring_.line.text()).empty()
                                  : authoring_.filled[i] && !authoring_.values[i].empty();
        if (field.required && !answered) {
            return false;
        }
    }
    return true;
}

/// COMMIT THIS FIELD AND STAND ON THE NEXT -- AND NOTHING ELSE. The whole of what the
/// `[next field]` control reads as, refusing in its own words on the last field, where the
/// face is already drawn `(next field)` to say the operation does not apply. Writing the
/// recipe is the control beside it, deliberately (`kActionNextField`).
inline void FilesWeave::next_field(loom::Mail& mail) {
    if (!authoring_.open) {
        return; // the mode closed under the gesture: no field to keep, and nothing to step
    }
    if (authoring_.step + 1 >= field_count()) {
        notice_ = std::string(field_name(authoring_.chosen.tree, authoring_.chosen.multi_config,
                                         authoring_.step)) +
                  " is the last field -- `write the recipe` writes the draft";
        say(mail);
        return;
    }
    if (!record_field(mail)) {
        return;
    }
    load_field(authoring_.step + 1);
    say(mail);
}

/// Commit this field and step to the next, and from the last one write the recipe: Return.
inline void FilesWeave::authoring_commit(loom::Mail& mail) {
    if (!authoring_.open) {
        return;
    }
    if (!record_field(mail)) {
        return;
    }
    if (authoring_.step + 1 < field_count()) {
        load_field(authoring_.step + 1);
        say(mail);
        return;
    }
    compose_recipe(mail);
}

/// WRITE THE WHOLE DRAFT FROM WHEREVER THE LINE IS STANDING. The field in hand is committed
/// first, then every required one is asked for; a missing one is named and nothing is sent.
inline void FilesWeave::write_recipe(loom::Mail& mail) {
    if (!authoring_.open) {
        return; // the draft is gone: a second ask for the same write writes nothing twice
    }
    if (!record_field(mail)) {
        return;
    }
    for (std::size_t i = 0; i < field_count(); ++i) {
        const Field& field = field_at(authoring_.chosen.tree, authoring_.chosen.multi_config, i);
        if (field.required && (!authoring_.filled[i] || authoring_.values[i].empty())) {
            notice_ = std::string(field.name) + " is required -- nothing was written";
            say(mail);
            return;
        }
    }
    compose_recipe(mail);
}

/// THE DRAFT, COMPOSED AS IT STANDS AND HANDED TO THE RECIPES DOOR ONCE WORKSHOP SAYS THIS
/// GESTURE'S HAND MAY WRITE (`send_recipe`). What a weaver typed is a DRAFT; the host
/// composes, checks and installs it (WL-AUTH-01), and this pane hears the outcome. The line
/// stays open until the answer, so a refused write leaves the draft to write again.
inline void FilesWeave::compose_recipe(loom::Mail& mail) {
    const Authoring& a = authoring_;
    FilesAsk next;
    next.kind = FilesAsk::Kind::kAuthor;
    next.subject = a.chosen.name;
    next.place = a.dir;
    RecipeAuthorRequested& draft = next.draft;
    draft.tree = a.chosen.tree;
    draft.id = a.values[0];
    const std::string place = ws::persist::resolved_against(a.dir, a.chosen.name);
    if (!draft.tree) {
        draft.artifact = a.values[1];
        draft.source = place;
        draft.packages = split_list(a.values[2]);
        draft.links = split_list(a.values[3]);
    } else {
        draft.target = a.values[1];
        draft.artifact = a.values[2];
        draft.build_dir = place;
        draft.artifact_dir = a.values[3];
        if (a.chosen.multi_config) {
            draft.config = a.values[4];
        }
    }
    ask_approval(std::move(next), ws::scope::kWrite, std::string(), mail);
}

/// THE APPROVED DRAFT, sent as it stood at the ask, while the line is still open on it: the
/// line closes, the browser's rows are in force again, and the recipes door writes the row.
inline void FilesWeave::send_recipe(const FilesAsk& approved, loom::Mail& mail) {
    if (!authoring_.open || authoring_.chosen.name != approved.subject ||
        authoring_.dir != approved.place) {
        notice_ = "that draft is not open any more -- no recipe was written";
        say(mail);
        return;
    }
    authoring_ = Authoring{};
    declare(mail); // the browser's rows are in force again
    (void)asks().forget(of_kind(FilesAsk::Kind::kRecipes));
    FilesAsk recipes = asking(FilesAsk::Kind::kRecipes);
    recipes.was_author = true;
    recipes.id = approved.draft.id;
    recipes.artifact = approved.draft.artifact;
    (void)asks().ask(mail, ws::kProjectRole, approved.draft, std::move(recipes));
}

// ---- The two modes' menus (WL-CTX-09) -----------------------------------------------------

inline void FilesWeave::offer_candidate(std::int64_t row, std::int64_t column,
                                        std::uint64_t correlation, loom::Mail& mail) {
    pane_menu::Offer offer(
        files::kProjectFilesPane,
        join_subject(chooser_.dir, chooser_.cursor < chooser_.candidates.size()
                                       ? chooser_.candidates[chooser_.cursor].name
                                       : std::string()));
    offer.at(row, column);
    if (chooser_.cursor < chooser_.candidates.size()) {
        offer.row(files::kMenuChoose,
                  "author a recipe for `" + chooser_.candidates[chooser_.cursor].name + "`");
    }
    offer.row(files::kMenuCancel, "pick nothing -- back to the listing");
    asked_menu_ = offer.continuing(mail, files::kFilesRole, correlation);
}

/// The authoring menu: every field but the one in hand, then the mode's own controls. A row
/// per field is what makes a short pane workable: the room may hold only the field being typed
/// into, and the others are reachable by no press unless the menu names them. Each row carries
/// its field in its id (`files::menu_edit_field`), since one answer echoes one subject, and
/// that subject is the candidate.
inline void FilesWeave::offer_field(std::size_t which, std::int64_t row, std::int64_t column,
                                    std::uint64_t correlation, loom::Mail& mail) {
    pane_menu::Offer offer(files::kProjectFilesPane,
                           join_subject(authoring_.dir, authoring_.chosen.name));
    offer.at(row, column);
    // THE FIELD THE HAND WAS ON LEADS, and the rest follow in their own order: a menu
    // opened ON a field is still about that field first.
    const auto field_row = [&](std::size_t i) {
        offer.row(files::menu_edit_field(i),
                  std::string("type the ") +
                      field_menu_label(authoring_.chosen.tree, authoring_.chosen.multi_config,
                                      i));
    };
    if (which < field_count() && which != authoring_.step) {
        field_row(which);
    }
    for (std::size_t i = 0; i < field_count(); ++i) {
        if (i == authoring_.step || i == which) {
            continue; // the line is already standing on it, or it led
        }
        field_row(i);
    }
    // The strip's own controls, row for row, including an unavailable one: a narrow strip
    // drops what will not fit and says `+N in menu`, so the promise is kept here. On the last
    // field the row still appears, as the strip draws `(next field)`, and `next_field`
    // refuses in words there and never writes the recipe.
    if (authoring_.step + 1 < field_count()) {
        offer.row(files::kMenuNextField,
                  std::string("keep this field and type the ") +
                      field_menu_label(authoring_.chosen.tree, authoring_.chosen.multi_config,
                                      authoring_.step + 1));
    } else {
        // A short label naming no field, clear of two bounds: `refusal_of`
        // (`workshop/menu-presenter/presenter.cpp`) refuses the whole offer once any label exceeds
        // `kMaxPaneMenuLabelLen` bytes, and below that the popup clips a row to its
        // display room. The line names the field; `next_field`'s notice says the rest.
        offer.row(files::kMenuNextField, "this is the last field");
    }
    offer.row(files::kMenuWriteRecipe,
              "write the recipe for `" + authoring_.chosen.name + "`");
    offer.row(files::kMenuCancel, "abandon this recipe");
    asked_menu_ = offer.continuing(mail, files::kFilesRole, correlation);
}

// ---- The two modes' controls and pictures -------------------------------------------------

inline std::vector<FilesWeave::ControlRow> FilesWeave::chooser_controls() const {
    std::vector<ControlRow> controls;
    controls.push_back(ControlRow{files::kActionMenu, "menu", true});
    controls.push_back(ControlRow{files::kActionChoose, "author a recipe for this",
                                  chooser_.cursor < chooser_.candidates.size()});
    controls.push_back(ControlRow{files::kActionCancel, "cancel", true});
    return controls;
}

inline std::vector<FilesWeave::ControlRow> FilesWeave::authoring_controls() const {
    std::vector<ControlRow> controls;
    controls.push_back(ControlRow{files::kActionMenu, "menu", true});
    controls.push_back(ControlRow{files::kActionNextField, "next field",
                                  authoring_.step + 1 < field_count()});
    controls.push_back(ControlRow{files::kActionWriteRecipe, "write the recipe", filled_in()});
    controls.push_back(ControlRow{files::kActionCancel, "abandon", true});
    return controls;
}

inline void FilesWeave::say_chooser() {
    push_row("pick something buildable -- " + std::to_string(chooser_.candidates.size()) +
                 (chooser_.candidates.size() == 1 ? " candidate" : " candidates"),
             surface::role::kAccent);
    const std::vector<ControlRow> controls = chooser_controls();
    const std::int64_t body_rows = body_budget(room().strip_rows(controls));
    if (body_rows > 0) {
        const component::ListWindow win =
            component::cursor_window(chooser_.candidates.size(), chooser_.cursor,
                                     chooser_hint_, static_cast<std::size_t>(body_rows));
        chooser_hint_ = win.first;
        (void)room().push_window(win, fit_row(), [&](std::size_t i) {
            const bool here = i == chooser_.cursor;
            const BuildCandidate& c = chooser_.candidates[i];
            push_row(std::string(here ? "> " : "  ") + c.name +
                         (c.tree ? "/  (configured tree)" : ""),
                     here ? surface::role::kAccent : surface::role::kFill,
                     FilesMeaning{files_row::kCandidate, i, {}, c.name});
        });
    }
    say_controls(controls);
}

/// The authoring line and the fields it walks. Every field is shown and any may be stood on;
/// what is written is still one draft the host composes, checks and installs (WL-AUTH-01,
/// WL-FILES-15). The line's caret is the medium's, its window follows the caret's column, and
/// a press places it. The fields go through the listing's own window, which keeps the field
/// being typed into on screen; where the room cannot seat a neighbour, the menu offers a row
/// for every field (`offer_field`).
inline void FilesWeave::say_authoring() {
    push_row(std::string("author `") + authoring_.chosen.name + "` -- " +
                 (authoring_.chosen.tree ? "a configured tree" : "a source file"),
             surface::role::kAccent);
    const std::vector<ControlRow> controls = authoring_controls();
    const std::int64_t body_rows = body_budget(room().strip_rows(controls));
    if (body_rows > 0) {
        const component::ListWindow win =
            component::cursor_window(field_count(), authoring_.step, field_hint_,
                                     static_cast<std::size_t>(body_rows));
        field_hint_ = win.first;
        const std::int64_t spent =
            room().push_window(win, fit_row(), [this](std::size_t i) { say_field(i); });
        // A cut the window could not reserve a marker for (`ListWindow::unsaid_cut`) still has
        // a row here, and it names the route: every field is in the menu. It wears no
        // control's face: brackets are the face vocabulary, and only `pack_controls` writes
        // them (`component/control_strip.hpp`).
        if (win.unsaid_cut() && spent < body_rows) {
            push_row("  ... " + std::to_string(win.before + win.after) +
                         " more fields -- this pane's menu names every one",
                     surface::role::kMuted);
        }
    }
    say_controls(controls);
}

/// The active field's prompt, as drawn: the full label wherever the room leaves the value
/// `kMinFieldValueColumns` beside it, and a shortened label in a narrower room, since the
/// value is what a weaver is reading. Read here and nowhere else, so painting (`say_field`)
/// and the press that turns a column back into a caret measure from the same text.
inline std::string FilesWeave::active_prompt() const {
    return fitted_label(authoring_.prompt, columns(), kMinFieldValueColumns);
}

/// ONE AUTHORING FIELD'S ROW: the line itself where the weaver is standing, and what the
/// field holds everywhere else. Either is a press target -- the line places the caret, a
/// held field stands the line on itself.
inline void FilesWeave::say_field(std::size_t i) {
    const Field& field = field_at(authoring_.chosen.tree, authoring_.chosen.multi_config, i);
    if (i == authoring_.step) {
        const std::string prompt = active_prompt();
        const std::int64_t prompt_cols = static_cast<std::int64_t>(prompt.size());
        const std::int64_t cols = columns() > prompt_cols + 1 ? columns() - prompt_cols - 1 : 1;
        authoring_.line.keep_caret_visible(cols);
        if (room().next_row() < rows()) {
            room().caret().row = room().next_row();
            room().caret().column =
                prompt_cols + static_cast<std::int64_t>(authoring_.line.caret_column());
        }
        push_row(prompt + authoring_.line.visible(cols), surface::role::kAccent,
                 FilesMeaning{files_row::kLine, i, {}, field.name});
        return;
    }
    const std::string held = authoring_.filled[i] && !authoring_.values[i].empty()
                                 ? authoring_.values[i]
                                 : std::string(field.required ? "(required)" : "(none)");
    push_row("  " + std::string(field.name) + ": " + held, surface::role::kMuted,
             FilesMeaning{files_row::kField, i, {}, field.name});
}

} // namespace

#endif // ZENGINE_FILES_RECIPE_AUTHORING_HPP
