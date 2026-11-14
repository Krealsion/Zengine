// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s Edit Code: a pane followed to the authored source of its running code, opened
// through the managed opening, and the Builder told which recipe that source belongs to.
// Workshop law: agents/workshop/code.md (+1 register; agents/workshop.md routes)

#include "weave.hpp"
#include "pane/operation.hpp"
#include "workshop/open_seam_vocabulary.hpp"

#include <cstdio>
#include <optional>
#include <string>
#include <utility>

namespace zengine::workshop {
using pane::PaneCanvasDrop;
using pane::PaneCanvasValueDrop;
using pane::kPresenterRole;

namespace {

/// Why the code behind a pane cannot be opened, in a weaver's words, or empty when the host named
/// exactly one recipe with one source file. The reason comes before any path, and every absence
/// is its own sentence, since each asks something different of the weaver.
// WL-CODE-02 -- agents/workshop/code.md
std::string code_refusal(const std::string& name, const HostContext::CodeSource& code) {
    if (code.weave == 0) {
        return "nobody holds " + code.role + " now, so " + name +
               " has no running code to trace -- nothing was opened";
    }
    if (code.artifact.empty()) {
        if (code.role == kWorkshopProvider) {
            return name + " is part of Workshop itself: no artifact of its own was loaded for "
                          "it, so there is no source to open";
        }
        return name + " is drawn by " + code.role + " (weave #" + std::to_string(code.weave) +
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
// WL-GUEST-09 -- agents/workshop/guests.md
void WorkshopWeave::edit_code(const PaneRef& ref, loom::Mail& mail) {
    // THE SETUP HAS ALREADY ANSWERED FOR THE REFERENCE: this is reached only through
    // `spend_pane_action`, the pane's one spend seam (WL-CTX-07), which refuses a reference that
    // left the setup before anything here runs. What is asked next is the code's owner.
    const std::optional<std::int64_t> kind = resolve_pane(ref, session_.panes);
    std::string name = kind.has_value() ? kind_name(session_.panes, *kind) : std::string();
    if (name.empty()) {
        name = ref_text(ref);
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
    // AN OPENING, judged for the hand that chose it, by what it opens (`actor_scope.hpp`).
    if (const std::string judged = judge_classes(input_actor_, {pane::act_class::kOpen}, recipe.source);
        !judged.empty()) {
        say(name + "'s code was not opened -- " + judged, true);
        return;
    }
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
    // THE MANAGED OPENING, ASKED AS A ROLE, WITH ITS TICKET KEPT (WL-OPEN-01, WL-OPEN-07): the
    // manager arranges the document and the desk together, a dirty document refuses, and
    // Loom's word that exactly this ask ended unanswered reaches `on(AskEnded)`.
    // Nothing is selected, focused or seated here; an accepted open seats the Editor's pane.
    next.attempt = mail.as_role(kWorkshopProvider)
                       .ask_role(kOpeningRole, OpenSourceRequested{next.source}, next.ask);
    if (!next.attempt.valid()) {
        // NOTHING WAS QUEUED, so nothing is awaited and an older ask this desk still holds is
        // left to its own answer: a spend that sent nothing supersedes nothing.
        say(name + "'s code was not opened -- nothing was queued to the opening role", true);
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
    // ⚠ RE-RESOLVED AT THE ANSWER. The open took turns, and in them the role may have changed
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
    opened.role = done.pane.provider;
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

// WL-CTX-10 -- agents/workshop/pane-menu.md
void WorkshopWeave::on(const loom::DispatchRefused& refused, loom::Mail& mail) {
    // Provenance first (the shape alone is speech), then the exact attempt: a refusal of any other
    // directed sentence is not this ask's, and is silence here.
    if (!mail.dispatch_refused()) {
        return;
    }
    const loom::Ticket attempt = refused.refused_attempt();
    // This reports an actual failed placement on a pane's area; it settles no current operation
    // or carry.
    const bool reference = refused.shape == PaneCanvasDrop::zen_name;
    const bool value = refused.shape == PaneCanvasValueDrop::zen_name;
    if (attempt.valid() && (reference || value) && !refused.role.empty() && refused.target.empty()) {
        say(std::string(reference ? "Reference" : "Value") + " not delivered to " + refused.role + " (attempt " +
            std::to_string(attempt.seq) + "): " + refused.reason, true);
        repaint(mail);
        return;
    }
    // ...AND A SENTENCE TO THE PRESENTER'S ROLE THAT LOOM COULD NOT DELIVER means no presenter
    // can answer the menu it was about: it left, or never held the role when the sentence
    // arrived. WHICH menu is the attempt's to say, never the role's name alone -- the open one,
    // or one already withdrawn whose requester is still owed; an older menu's refusal settles
    // nothing newer. This host answers that requester itself, unchosen (WL-CTX-10).
    if (refused.role == kPresenterRole) {
        (void)end_refused_menu(attempt, refused.reason, mail);
    }
}

// WL-CODE-03 -- agents/workshop/code.md; WL-CTX-10 -- agents/workshop/pane-menu.md
void WorkshopWeave::on(const loom::AskEnded& ended, loom::Mail& mail) {
    if (!mail.ask_ended()) {
        return; // the shape alone is anybody's speech
    }
    const loom::Ticket attempt = ended.ended_attempt();
    // A WITHDRAWAL NO PRESENTER ANSWERED -- none held the role, or its holder was silent or gone:
    // this host answers that requester itself, unchosen, once (WL-CTX-10).
    if (ended.role == kPresenterRole) {
        (void)end_refused_menu(attempt,
                               std::string(loom::stage_name(ended.stage)) + ", " +
                                   std::string(loom::reason_name(ended.stage)),
                               mail);
        return;
    }
    const std::string why = pane::no_answer(ended);
    // THE DESK'S OWN ASKS OF ITS WINDOW -- an area's reading, the key fence, a paste -- end as a
    // refusal does: the reading refused, the keys behind the fence released, the paste dropped.
    if (const std::optional<loom::PendingAsk> area = area_asks_.end(ended, mail)) {
        if (!take_key_fence(area->id, nullptr, mail)) {
            take_area_answer(mail, area->id, nullptr, why);
        }
        return;
    }
    if (const std::optional<loom::PendingAsk> paste = paste_asks_.end(ended, mail)) {
        (void)take_pending_paste(paste->id);
        return;
    }
    // ...AND OF THE LAYOUTS' OWNER, by the ask's own number: an edit is refused in those words,
    // a setting's commit not saved, and a save at quit ends the quit as a refused one does.
    if (ended.shape == LayoutEditRequested::zen_name) {
        const auto owed = edits_owed_.find(mail.correlation());
        if (owed == edits_owed_.end()) {
            return;
        }
        LayoutEditAnswered refused;
        refused.refusal = "the desk stands as it is -- " + why;
        refused.revision = session_.setup.revision;
        EditThen then = std::move(owed->second);
        edits_owed_.erase(owed);
        if (then) {
            then(refused, mail);
        } else {
            say(refused.refusal, true);
        }
        while (edits_owed_.empty() && !settle_waiters_.empty()) {
            auto next = std::move(settle_waiters_.front());
            settle_waiters_.pop_front();
            next(mail);
        }
        repaint(mail);
        return;
    }
    if (ended.shape == RoleSettingCommitRequested::zen_name) {
        if (const auto owed = own_said_.find(mail.correlation()); owed != own_said_.end()) {
            say(owed->second + "; not saved -- " + why, true);
            own_said_.erase(owed);
            repaint(mail);
        }
        return;
    }
    if (ended.shape == SessionSaveRequested::zen_name && saving_) {
        saving_ = false;
        say("the session was not saved -- " + why, true);
        std::fprintf(stderr, "zengine-workshop: the session was not saved -- %s\n", why.c_str());
        std::fflush(stderr);
        end_quit(mail);
        return;
    }
    if (!attempt.valid() || !code_open_.live || !code_open_.attempt.valid() ||
        attempt.seq != code_open_.attempt.seq) {
        return;
    }
    const std::string name = code_open_.name;
    code_open_ = CodeOpen{};
    say(name + "'s code was not opened -- " + pane::no_answer(ended), true);
    repaint(mail);
}

} // namespace zengine::workshop
