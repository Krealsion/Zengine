// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// LOAD IT (LOAD-IT) -- the maker points at a built artifact and Workshop authors what the
// plan needs to say about it.
//
// THE PROMPT IS A MODE, LIKE `+ panel`. `o` asks for one thing nothing can detect -- the
// ROLE the artifact holds -- and nothing is written until it commits. Return commits,
// Escape cancels.
//
// THE WRITE IS THE HOST'S. This file composes nothing: it hands the stem, the typed role
// and the chosen recipe to `HostContext::append_plan_row`, and the plan's law, the file the
// row goes into, the atomic save and the running project's own answer are all the host's,
// so the law that judges a row is spelled once and not here.
//
// PICK BUILDABLE USED TO BE HERE TOO. `a` in the browser enumerated a location's candidates
// and typed a recipe's fields through this same prompt; the browser is a weave now
// (`Zengine/files/`) and asks `zengine.recipes` to author the row instead, typing its fields
// into a line inside its own room. What is left here is the one asker command mode still
// has -- which is why the prompt carries a role and nothing else.
// Workshop law: agents/workshop/authoring.md

#include "weave.hpp"

#include "panel.hpp"
#include "screen.hpp"

#include <string>

namespace zengine::workshop {

namespace {

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

} // namespace

void WorkshopWeave::authoring_key(const zengine::input::KeyPressed& k, loom::Mail& mail) {
    AuthoringPrompt& a = session_.authoring;
    if (a.line.consume(k.scancode, k.modifiers, session_.clipboard)) {
        repaint(mail);
        return;
    }
    switch (session_.keymap.action_for(KeyContext::kAuthoring, k.scancode, k.modifiers)) {
    case Act::kAuthoringCommit: authoring_commit(mail); break;
    case Act::kAuthoringCancel:
        close_authoring();
        say("nothing was loaded and nothing was written", false);
        repaint(mail);
        break;
    default: break;
    }
}

void WorkshopWeave::close_authoring() { session_.authoring = AuthoringPrompt{}; }

// WL-AUTH-02 -- agents/workshop/authoring.md
void WorkshopWeave::authoring_commit(loom::Mail& mail) {
    AuthoringPrompt& a = session_.authoring;
    const std::string typed = trimmed(a.line.text());
    // THE ROLE, THEN THE HOST. An empty role is refused here in the plan's own words
    // (`check_weave_role` says the same), and everything else -- the stem, the duplicate,
    // the executor's state, the file -- is the host's to say.
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
        // THE INTENT FINISHES: the row is the frontier and its product is already built,
        // so this is the button's own act -- the finished build's recipe asked for again
        // with the second intention aboard, to the same office, under the same grant --
        // and the row lands resolved without a second key. No new route: the tool confirms
        // the build, offers, and the owner decides in its words.
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
        // NOTHING TO FINISH: resolved already, authored behind another frontier, or the
        // frontier with nothing built yet -- in which case the arm covers it, and the
        // sentence names the key that builds and loads the frontier.
        say("loaded `" + stem + "` as " + typed + " -- " + done.detail +
                (done.frontier ? "; nothing is built yet -- " +
                                     hotkey(Act::kBuildFrontier) + " builds and loads it"
                               : std::string()) +
                written,
            false);
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
