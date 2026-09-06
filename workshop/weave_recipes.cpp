// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// PICK SOMETHING BUILDABLE, AND LOAD IT (PICK-1, LOAD-IT) -- the maker points at a place
// and Workshop authors what the two authored files need to say about it.
//
// THE CHOOSER IS A MODE, LIKE `+ panel`. Files hands over exactly what `u` hands over -- a
// location and its listing -- and judges nothing; the chooser enumerates ONCE, at the
// gesture, what that place can at least TRY to build: a source file, or a directory that
// holds a configured CMake tree. A source tree with only a `CMakeLists.txt` is not a row,
// because a recipe of the target kind names a CONFIGURED tree and re-configuring somebody
// else's project is a policy that is not Workshop's to decide. Painting the chooser walks
// nothing; the candidates are held until the mode closes.
//
// A CHOICE ASKS FOR WHAT NOTHING CAN DETECT. A recipe row needs facts no listing carries --
// a name, the artifact stem, a package prefix and link targets for a source, a target
// name for a tree -- so the chooser asks for them one field at a time on the authoring
// prompt, the pane creator's name prompt one context over. Return advances, Escape
// cancels the whole authoring, and nothing is written until the last field commits.
//
// THE WRITE IS THE HOST'S. This file composes a DRAFT of strings and hands it to
// `HostContext::author_recipe`; the recipe law, the file the row goes into, the atomic
// save and the one-seam install are the host's, so the law that judges a row is spelled
// once and not here. LOAD IT is the same shape for the other file: the chosen recipe's
// artifact, a role the maker types, and `HostContext::append_plan_row` deciding.
// Workshop law: agents/workshop/authoring.md

#include "weave.hpp"

#include "files.hpp"
#include "panel.hpp"
#include "persist.hpp"
#include "screen.hpp"

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace zengine::workshop {

namespace {

/// IS THIS NAME A SOURCE FILE'S? The three spellings CMake compiles as C++ by default.
bool source_name(const std::string& name) {
    for (const char* suffix : {".cpp", ".cc", ".cxx"}) {
        const std::string s(suffix);
        if (name.size() > s.size() && name.compare(name.size() - s.size(), s.size(), s) == 0) {
            return true;
        }
    }
    return false;
}

std::string stem_of(const std::string& name) {
    const std::size_t dot = name.rfind('.');
    return dot == std::string::npos || dot == 0 ? name : name.substr(0, dot);
}

std::string trimmed(const std::string& text) {
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

std::vector<std::string> split_list(const std::string& text) {
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

/// THE FIELDS A CHOSEN CANDIDATE STILL NEEDS, in the order they are asked. A required
/// field refuses an empty answer and re-asks; an optional one accepts it.
struct Field {
    const char* name;
    bool required;
};

constexpr Field kSourceFields[] = {{"recipe name", true},
                                   {"artifact stem", true},
                                   {"package prefix (comma-separated)", true},
                                   {"link targets (comma-separated)", true}};
constexpr Field kTreeFields[] = {{"recipe name", true},
                                 {"cmake target", true},
                                 {"artifact stem", true},
                                 {"artifact directory (optional)", false}};
constexpr std::size_t kFieldCount = 4;

const Field& field_at(bool tree, std::size_t step) {
    return tree ? kTreeFields[step] : kSourceFields[step];
}

} // namespace

// WL-AUTH-01 -- agents/workshop/authoring.md; WL-FILES-15 -- agents/workshop/files.md
void WorkshopWeave::files_pick_buildable(loom::Mail& mail) {
    ensure_marks();
    const FilesPane& pane = session_.panels.files;
    if (!pane.listing.known) {
        say("nothing is listed here -- nothing to pick from", true);
        return;
    }
    const std::string dir = files_dir();
    if (dir.empty()) {
        say("this run began nowhere -- nothing to pick from", true);
        return;
    }
    if (!host_->author_recipe) {
        say("this host cannot author recipes -- nothing to pick from", true);
        return;
    }
    // THE ENUMERATION, ONCE. The listing's rows are what the browser already walked; the
    // one thing added is an existence probe per DIRECTORY row, for the one file that says
    // "configured". Nothing here opens, reads or judges a file's contents.
    RecipeChooser chooser;
    chooser.dir = dir;
    for (const FileRow& row : pane.listing.rows) {
        if (!row.openable) {
            continue;
        }
        if (row.directory) {
            std::error_code ec;
            const bool configured = std::filesystem::exists(
                std::filesystem::path(dir) / row.name / "CMakeCache.txt", ec);
            if (configured && !ec) {
                chooser.candidates.push_back(BuildCandidate{row.name, true});
            }
        } else if (source_name(row.name)) {
            chooser.candidates.push_back(BuildCandidate{row.name, false});
        }
    }
    if (chooser.candidates.empty()) {
        say("nothing buildable in " + dir +
                " -- no source file, and no directory holding a configured CMake tree (a "
                "CMakeLists.txt alone is a source tree)",
            true);
        return;
    }
    chooser.open = true;
    session_.recipe_chooser = std::move(chooser);
    say("pick something buildable -- " + hotkey(Act::kRecipeUp) + "/" +
            hotkey(Act::kRecipeDown) + " chooses, " + hotkey(Act::kRecipeChoose) +
            " authors a recipe for it, " + hotkey(Act::kRecipeClose) + " cancels",
        false);
    repaint(mail);
}

void WorkshopWeave::recipe_chooser_key(const zengine::input::KeyPressed& k, loom::Mail& mail) {
    RecipeChooser& chooser = session_.recipe_chooser;
    const std::size_t total = chooser.candidates.size();
    if (chooser.cursor >= total) {
        chooser.cursor = total == 0 ? 0 : total - 1;
    }
    switch (session_.keymap.action_for(KeyContext::kRecipeChooser, k.scancode, k.modifiers)) {
    case Act::kRecipeUp:
        if (chooser.cursor > 0) {
            --chooser.cursor;
        }
        repaint(mail);
        break;
    case Act::kRecipeDown:
        if (chooser.cursor + 1 < total) {
            ++chooser.cursor;
        }
        repaint(mail);
        break;
    case Act::kRecipeChoose: recipe_choose(mail); break;
    case Act::kRecipeClose:
        session_.recipe_chooser = RecipeChooser{};
        say("no recipe was authored", false);
        repaint(mail);
        break;
    default: break;
    }
}

// WL-AUTH-01 -- agents/workshop/authoring.md
void WorkshopWeave::recipe_choose(loom::Mail& mail) {
    RecipeChooser& chooser = session_.recipe_chooser;
    if (chooser.cursor >= chooser.candidates.size()) {
        session_.recipe_chooser = RecipeChooser{};
        say("no recipe was authored", false);
        repaint(mail);
        return;
    }
    AuthoringPrompt a;
    a.open = true;
    a.chosen = chooser.candidates[chooser.cursor];
    a.dir = chooser.dir;
    session_.authoring = std::move(a);
    session_.recipe_chooser = RecipeChooser{};
    // THE FIRST FIELD, WITH ITS DEFAULT ON THE LINE: a name is the one thing a place
    // suggests, and a suggestion a maker can see and change is not a hidden input.
    AuthoringPrompt& live = session_.authoring;
    const std::string suggested = live.chosen.tree ? live.chosen.name : stem_of(live.chosen.name);
    live.prompt = std::string(field_at(live.chosen.tree, 0).name) + "> ";
    live.line.set(suggested, suggested.size());
    say(std::string(live.chosen.tree ? "a configured tree" : "a source file") + ": " +
            live.chosen.name + " -- " + hotkey(Act::kAuthoringCommit) + " commits a field, " +
            hotkey(Act::kAuthoringCancel) + " cancels the whole recipe",
        false);
    repaint(mail);
}

void WorkshopWeave::authoring_key(const zengine::input::KeyPressed& k, loom::Mail& mail) {
    AuthoringPrompt& a = session_.authoring;
    if (a.line.consume(k.scancode, k.modifiers, session_.clipboard)) {
        repaint(mail);
        return;
    }
    switch (session_.keymap.action_for(KeyContext::kAuthoring, k.scancode, k.modifiers)) {
    case Act::kAuthoringCommit: authoring_commit(mail); break;
    case Act::kAuthoringCancel: {
        const bool was_role = a.for_role; // read before the prompt is reset
        close_authoring();
        say(was_role ? "nothing was loaded and nothing was written" : "no recipe was written",
            false);
        repaint(mail);
        break;
    }
    default: break;
    }
}

void WorkshopWeave::close_authoring() { session_.authoring = AuthoringPrompt{}; }

// WL-AUTH-01, WL-AUTH-02 -- agents/workshop/authoring.md
void WorkshopWeave::authoring_commit(loom::Mail& mail) {
    AuthoringPrompt& a = session_.authoring;
    const std::string typed = trimmed(a.line.text());
    if (a.for_role) {
        // THE ROLE, THEN THE HOST. An empty role is refused here in the plan's own words
        // (`check_weave_role` says the same), and everything else -- the stem, the
        // duplicate, the executor's state, the file -- is the host's to say.
        if (typed.empty()) {
            say("a weave declaration needs a role -- nothing was loaded; " +
                    hotkey(Act::kAuthoringCancel) + " cancels",
                true);
            return;
        }
        const std::string stem = a.stem;
        const std::string recipe = a.recipe;
        const HostContext::PlanAppend done = host_->append_plan_row(stem, typed, recipe);
        close_authoring();
        const std::string written =
            done.path.empty() ? std::string() : "; written to " + done.path;
        if (!done.accepted) {
            say("not loaded: " + done.refusal, true);
        } else if (!done.product.empty()) {
            // THE INTENT FINISHES: the row is the frontier and its product is already
            // built, so this is the button's own act -- the finished build's recipe asked
            // for again with the second intention aboard, to the same office, under the
            // same grant -- and the row lands resolved without a second key. No new route:
            // the tool confirms the build, offers, and the owner decides in its words.
            (void)mail.send_to_role(zengine::builder::kBuilderRole,
                                    zengine::builder::BuildRequested{recipe, true});
            if (session_.panels.has(panel::kBuilder)) {
                session_.panels.builder.awaiting = true;
                session_.panels.builder.awaiting_realization = true;
            }
            say("loaded `" + stem + "` as " + typed + " -- " + done.detail +
                    "; its product is built, loading it now -- Workshop stays live while the "
                    "incremental build confirms it" +
                    written,
                false);
        } else {
            // NOTHING TO FINISH: resolved already, authored behind another frontier, or
            // the frontier with nothing built yet -- in which case the arm covers it, and
            // the sentence names the key that builds and loads the frontier.
            say("loaded `" + stem + "` as " + typed + " -- " + done.detail +
                    (done.frontier ? "; nothing is built yet -- " +
                                         hotkey(Act::kBuildFrontier) + " builds and loads it"
                                   : std::string()) +
                    written,
                false);
        }
        repaint(mail);
        return;
    }
    const Field& field = field_at(a.chosen.tree, a.step);
    if (field.required && typed.empty()) {
        say(std::string(field.name) + " is required -- nothing was written; " +
                hotkey(Act::kAuthoringCancel) + " cancels",
            true);
        return;
    }
    a.answers.push_back(typed);
    ++a.step;
    if (a.step < kFieldCount) {
        // THE NEXT FIELD, with the one default a previous answer suggests.
        const Field& next = field_at(a.chosen.tree, a.step);
        std::string suggested;
        if (!a.chosen.tree && a.step == 1) {
            suggested = a.answers[0]; // the artifact is the recipe's name unless said otherwise
        } else if (a.chosen.tree && a.step == 2) {
            suggested = a.answers[1]; // the artifact is the target's name unless said otherwise
        }
        a.prompt = std::string(next.name) + "> ";
        a.line.set(suggested, suggested.size());
        repaint(mail);
        return;
    }
    // THE LAST FIELD COMMITTED: compose the draft and hand it over. The place is the
    // browser's own spelling of the candidate under the location the gesture was made in.
    HostContext::RecipeDraft draft;
    draft.tree = a.chosen.tree;
    draft.id = a.answers[0];
    const std::string place = persist::resolved_against(a.dir, a.chosen.name);
    if (!draft.tree) {
        draft.artifact = a.answers[1];
        draft.source = place;
        draft.packages = split_list(a.answers[2]);
        draft.links = split_list(a.answers[3]);
    } else {
        draft.target = a.answers[1];
        draft.artifact = a.answers[2];
        draft.build_dir = place;
        draft.artifact_dir = a.answers[3];
    }
    const HostContext::RecipeSwap done = host_->author_recipe(draft);
    close_authoring();
    if (!done.accepted) {
        say("no recipe was written: " + done.refusal + "; still using " +
                (done.path.empty() ? std::string("no catalog") : done.path),
            true);
        repaint(mail);
        return;
    }
    // THE CATALOG IN FORCE MOVED, THROUGH THE ONE SEAM `u` SPENDS: the same projection,
    // the same republication, so the Builder pane shows the new row the way it shows a
    // chosen catalog.
    session_.recipes_moved_to = done.path;
    say("authored recipe `" + draft.id + "` -> " + draft.artifact + " in " + done.path + " (" +
            std::to_string(done.recipes) + " recipes)",
        false);
    if (session_.panels.has(panel::kBuilder)) {
        (void)mail.send_to_role(zengine::builder::kBuilderRole,
                                zengine::builder::StatusRequested{});
    }
    repaint(mail);
}

// WL-AUTH-02 -- agents/workshop/authoring.md
void WorkshopWeave::load_it(loom::Mail& mail) {
    if (!session_.panels.has(panel::kBuilder)) {
        return; // an unbound key with no Builder panel open, exactly as `b` is
    }
    const BuilderPane& pane = session_.panels.builder;
    if (!pane.heard) {
        say("the Builder has not said what it builds yet -- nothing to load", true);
        return;
    }
    if (pane.known.recipes.empty()) {
        say("this project has no build recipes -- nothing to load", true);
        return;
    }
    if (!host_->append_plan_row || !host_->plan_names) {
        say("this host cannot author plan rows -- nothing to load", true);
        return;
    }
    const std::size_t at =
        pane.chosen < pane.known.recipes.size() ? pane.chosen : std::size_t{0};
    const std::string stem = pane.known.recipes[at].artifact;
    if (host_->plan_names(stem)) {
        say("`" + stem + "` is already in this project's plan -- " +
                hotkey(Act::kBuildRealize) + " builds and loads it",
            true);
        return;
    }
    AuthoringPrompt a;
    a.open = true;
    a.for_role = true;
    a.stem = stem;
    a.recipe = pane.known.recipes[at].recipe;
    a.prompt = "role for " + stem + "> ";
    a.line.set(std::string(), 0);
    session_.authoring = std::move(a);
    say("load `" + stem + "` -- type the role it holds; " + hotkey(Act::kAuthoringCommit) +
            " loads it, " + hotkey(Act::kAuthoringCancel) + " cancels",
        false);
    repaint(mail);
}

} // namespace zengine::workshop
