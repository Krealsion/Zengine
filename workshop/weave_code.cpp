// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `weave.hpp`'s Edit Code section -- a pane a maker points at, followed to the
// authored source of its running code, opened through the managed opening, and the Builder
// told which recipe that source belongs to -- compiled once into `zengine-workshop-logic`.
// Workshop law: agents/workshop/code.md (+1 register; agents/workshop.md routes)

#include "weave.hpp"

#include <utility>

namespace zengine::workshop {

namespace {

/// WHY THE CODE BEHIND A PANE CANNOT BE OPENED, in a maker's words -- or empty when the host
/// named exactly one recipe with one source file. The reason comes before any path, so a row
/// cut at its width keeps it (the opening manager's rule, WL-OPEN-07's sentences).
///
/// ⚠ EVERY ABSENCE IS ITS OWN SENTENCE, because each asks something different of the maker:
/// nobody holds the office (nothing is running to trace); the office is Workshop's own or held
/// by a weave the plan did not load (no artifact is known); no recipe produces the artifact
/// (author one); several do (choose -- the Builder holds that choice); one does and names a
/// build tree with no editing entry (there is no file to open, and none is guessed from a name).
// WL-CODE-02 -- agents/workshop/code.md
std::string code_refusal(const std::string& name, const HostContext::CodeSource& code) {
    if (code.weave == 0) {
        return "nobody holds " + code.office + " now, so " + name +
               " has no running code to trace -- nothing was opened";
    }
    if (code.artifact.empty()) {
        if (code.office == kWorkshopProvider) {
            return name + " is part of Workshop itself: no artifact of its own was loaded for "
                          "it, so there is no source to open";
        }
        return name + " is drawn by " + code.office + " (weave #" + std::to_string(code.weave) +
               "), which this project's plan did not load -- no artifact names its code, so "
               "nothing was opened";
    }
    if (code.recipes.empty()) {
        return "no build recipe produces " + code.artifact + ", the artifact behind " + name +
               " -- author one for its source (pick buildable, in Files); nothing was opened";
    }
    if (code.recipes.size() > 1) {
        std::string named;
        for (const HostContext::CodeSource::Recipe& r : code.recipes) {
            named += (named.empty() ? "`" : ", `") + r.id + "`";
        }
        return std::to_string(code.recipes.size()) + " recipes build " + code.artifact + " (" +
               named + ") -- choose one in the Builder and open its source there; nothing was "
                       "opened";
    }
    const HostContext::CodeSource::Recipe& only = code.recipes.front();
    if (only.source.empty()) {
        return "recipe `" + only.id + "` is a " + only.kind + " recipe with no editing entry: it "
               "builds " + code.artifact + " from a build tree and a target and names no file to "
               "start from -- give it an `entry`, or open the file you mean from Files; nothing "
               "was opened";
    }
    return std::string();
}

/// IS THE CODE BEHIND THE PANE STILL THE CODE THE SPEND OPENED? The same holder, realized from
/// the same artifact, with still exactly one recipe -- the same one, naming the same file.
// WL-CODE-03 -- agents/workshop/code.md
bool same_code(const HostContext::CodeSource& now, const HostContext::CodeSource& then,
               const std::string& recipe, const std::string& source) {
    return now.weave == then.weave && now.artifact == then.artifact && now.recipes.size() == 1 &&
           now.recipes.front().id == recipe && now.recipes.front().source == source;
}

} // namespace

// WL-CODE-02 -- agents/workshop/code.md
void WorkshopWeave::edit_code(const PaneRef& ref, loom::Mail& mail) {
    // THE SETUP HAS ALREADY ANSWERED FOR THE REFERENCE: this is reached only through
    // `spend_pane_action`, the pane's one spend seam (WL-CTX-07), which refuses a reference that
    // left the setup before anything here runs. What is asked next is the code's owner.
    const std::optional<std::int64_t> kind = resolve_pane(ref, session_.panels);
    std::string name = kind.has_value() ? kind_name(session_.panels, *kind) : std::string();
    if (name.empty()) {
        name = ref_text(ref);
    }
    // A PANE MADE FROM DATA HAS A DEFINITION, NOT CODE: its namespace is Workshop's own and no
    // office holds it, so the sentence names what it is rather than an absent holder.
    if (ref.provider == kMakerPaneProvider) {
        say(name + " is made from data, not code: its definition is a file the Pane Creator "
                   "keeps -- nothing was opened",
            true);
        return;
    }
    if (!host_->code_source) {
        say("this Workshop cannot trace a pane to its code -- nothing was opened", true);
        return;
    }
    const HostContext::CodeSource code = host_->code_source(ref.provider);
    const std::string refused = code_refusal(name, code);
    if (!refused.empty()) {
        say(refused, true);
        return;
    }
    const HostContext::CodeSource::Recipe& recipe = code.recipes.front();
    // ONE ASK IN FLIGHT, AND A NEWER SPEND REPLACES THE RECORD. The manager supersedes an open
    // still being prepared and answers the older ask; that answer then matches nothing here,
    // so it can neither say an old sentence nor tell the Builder about a file no longer wanted.
    CodeOpen next;
    next.live = true;
    next.ask = ++code_asks_;
    next.pane = ref;
    next.name = name;
    next.code = code;
    next.recipe = recipe.id;
    next.source = recipe.source;
    // THE MANAGED OPENING, AS AN OFFICE, WITH ITS TICKET KEPT (WL-OPEN-01, WL-OPEN-07): the
    // manager arranges the document and the desk together, a dirty document refuses, and
    // Loom's later word that exactly this attempt was refused reaches `on(DispatchRefused)`.
    // Nothing is selected, focused or seated here; an accepted open seats the Editor's pane.
    next.attempt = mail.as_role(kWorkshopProvider)
                       .send_to_role(kOpeningRole, OpenSourceRequested{next.source}, next.ask);
    if (!next.attempt.valid()) {
        // NOTHING WAS QUEUED, so nothing is awaited and an older ask this desk still holds is
        // left to its own answer: a spend that sent nothing supersedes nothing.
        say(name + "'s code was not opened -- nothing was queued to the opening office", true);
        return;
    }
    code_open_ = std::move(next);
    say("opening the source of " + name + " (recipe `" + recipe.id + "` builds " + code.artifact +
            ") -- " + recipe.source,
        false);
}

// WL-CODE-03 -- agents/workshop/code.md
void WorkshopWeave::on(const SourceOpened& said, loom::Mail& mail) {
    // LOOM'S WORD FIRST, then which ask: an answer authenticated to this weave and carrying the
    // correlation of the one Edit Code open still held. A forged or stale `SourceOpened` -- or
    // the answer to an ask a newer spend replaced -- settles nothing.
    if (!mail.answers_ask() || !code_open_.live || mail.correlation() != code_open_.ask) {
        return;
    }
    const CodeOpen done = std::move(code_open_);
    code_open_ = CodeOpen{};
    if (!said.accepted) {
        say(said.refusal + " -- " + done.name + "'s code was not opened", true);
        repaint(mail);
        return;
    }
    // ⚠ RE-RESOLVED AT THE ANSWER. The open took turns, and in them the office may have changed
    // hands, the catalog may have been replaced, or a second recipe may have arrived. The file
    // is open either way -- the Editor holds it, and that stands -- but the Builder is told only
    // about code that is still the pane's, so a later answer cannot point it at a recipe that
    // no longer builds what this pane runs.
    const HostContext::CodeSource now =
        host_->code_source ? host_->code_source(done.pane.provider) : HostContext::CodeSource{};
    if (!same_code(now, done.code, done.recipe, done.source)) {
        say(done.source + " is open, but the code behind " + done.name +
                " changed while it opened -- the Builder was not pointed at `" + done.recipe +
                "`; edit code again",
            true);
        repaint(mail);
        return;
    }
    PaneSourceOpened opened;
    opened.office = done.pane.provider;
    opened.pane = done.pane.pane;
    opened.name = done.name;
    opened.artifact = done.code.artifact;
    opened.recipe = done.recipe;
    opened.source = done.source;
    (void)mail.as_role(kWorkshopProvider).publish(opened);
    const std::string next =
        done.code.reload.empty()
            ? "save, then build it in the Builder with load after build, and " + done.name +
                  " reloads in place"
            : "a rebuild will not reload in place: " + done.code.reload;
    say("opened the source of " + done.name + " -- recipe `" + done.recipe + "` builds " +
            done.code.artifact + "; " + next,
        false);
    repaint(mail);
}

// WL-CODE-03 -- agents/workshop/code.md
void WorkshopWeave::on(const loom::DispatchRefused& refused, loom::Mail& mail) {
    // PROVENANCE FIRST -- the shape alone is speech -- then the exact attempt. This desk sends
    // many directed sentences (rooms, keys, presses) and a refusal of any of those is not this
    // ask's, so it is silence here, exactly as it was before this weave accepted the notice.
    if (!mail.dispatch_refused()) {
        return;
    }
    const loom::Ticket attempt = refused.refused_attempt();
    // A SECONDARY BUTTON PRESS THIS HOST QUEUED AND LOOM REFUSED settles first: its custody is
    // dropped so the physical release sends nothing (WL-PRESS-06). This host sends many directed
    // sentences; a refusal that matches neither a held button nor the code-open ask is silence
    // here, exactly as it was before this weave accepted the notice.
    if (end_refused_button(attempt, mail)) {
        repaint(mail);
        return;
    }
    // ...AND A SENTENCE TO THE PRESENTER'S OFFICE THAT LOOM COULD NOT DELIVER means no presenter
    // can answer the menu that is open: it left, or never held the office when the grant arrived.
    // This host answers the requester itself, unchosen (WL-CTX-09).
    if (refused.role == kPresenterRole) {
        end_menu_unanswered("the presenter left -- " + refused.reason, mail);
        return;
    }
    if (!attempt.valid() || !code_open_.live || !code_open_.attempt.valid() ||
        attempt.seq != code_open_.attempt.seq) {
        return;
    }
    const std::string name = code_open_.name;
    code_open_ = CodeOpen{};
    say(name + "'s code was not opened -- the open could not reach " + kOpeningRole + " (" +
            refused.reason + ")",
        true);
    repaint(mail);
}

} // namespace zengine::workshop
