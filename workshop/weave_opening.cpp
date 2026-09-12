// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// EXPERIMENTAL (editor-managed-open-slice): the bodies of `opening.hpp`, compiled once into
// `zengine-workshop-logic` beside Workshop's own, and linked by the host and the suites.

#include "opening.hpp"

namespace zengine::workshop {

namespace {

/// THE MAKER'S WORDS FOR A REFUSAL THE BUS DECIDED. `name_of` is a diagnostic spelling; a
/// maker reads what happened to their work.
std::string refusal_words(const std::string& path, loom::JointRefusal why) {
    switch (why) {
    case loom::JointRefusal::StaleRevision:
    case loom::JointRefusal::ParticipantChanged:
    case loom::JointRefusal::WrongState:
        return path + " was not opened; your work is kept -- the room or the document "
                      "changed while opening; try again";
    case loom::JointRefusal::NoClaim:
        return path + " was not opened -- no Editor and desk are present to open it";
    case loom::JointRefusal::Exhausted:
        // THE BUS'S BOUND ON RECORDS NOBODY RELEASED (editor-managed-open-slice-corrections-2):
        // this manager releases every record it consumes, so meeting it means another
        // operator on this bus is keeping its records; said as the limit it is.
        return path + " was not opened -- too many opens on this bus are still unsettled; "
                      "try again";
    default:
        return path + " was not opened -- the opening could not be arranged (" +
               loom::name_of(why) + ")";
    }
}

/// THE MAKER'S WORDS FOR A COMMITMENT AN OWNER DID NOT APPLY (editor-managed-open-slice-
/// corrections; the declined word, editor-managed-open-slice-corrections-2): the publication
/// stands, the owner is named, and what happens next is said -- a held owner waits for a
/// reload or a removal; an owner that DECLINED is fine and kept what it had, so the maker
/// simply asks again.
std::string unapplied_words(const std::string& path, const loom::JointStatus& status) {
    const std::string who = status.failed_role.empty() ? std::string("one of its owners")
                                                       : status.failed_role;
    if (status.application == loom::JointApplication::Lost) {
        return path + " was published but " + who +
               " was removed before it could apply it -- open it again once an owner is "
               "present";
    }
    if (status.application == loom::JointApplication::Declined) {
        return path + " was published but " + who +
               " did not apply it -- it kept what it had; open it again";
    }
    return path + " was published but " + who + " could not apply it (" +
           loom::name_of(status.application) +
           ") -- it is held until it is reloaded or removed; the desk shows what it applied";
}

/// The manager's own record of an unapplied commitment, by what the owner answered.
const char* unapplied_outcome(loom::JointApplication application) {
    switch (application) {
    case loom::JointApplication::Declined:
        return "committed, not applied";
    case loom::JointApplication::Lost:
        return "committed, owner removed";
    default:
        return "committed, application failed";
    }
}

} // namespace

OpeningManager::OpeningManager(std::string editor_office, std::string presentation_office,
                               PaneRef pane)
    : editor_office_(std::move(editor_office)), presentation_office_(std::move(presentation_office)),
      pane_(std::move(pane)) {
    state_.stage = "idle";
}

template <class Shape>
bool OpeningManager::ask(loom::Mail& mail, const std::string& office, const Shape& shape,
                         const char* stage) {
    flight_.stage = stage;
    flight_.awaiting = office;
    flight_.attempt = mail.as_role(kOpeningRole).send_to_role(office, shape, flight_.op);
    state_.stage = stage;
    state_.awaiting = office;
    state_.attempt = static_cast<std::int64_t>(flight_.attempt.seq);
    return flight_.attempt.valid();
}

void OpeningManager::progress(loom::Mail& mail, bool pending) {
    (void)mail.as_role(kOpeningRole)
        .send_to_role(presentation_office_,
                      ManagedOpenProgress{static_cast<std::int64_t>(flight_.op), flight_.path,
                                          flight_.stage, flight_.awaiting, pending});
}

/// AN OWNER'S REFUSAL, RE-READ AGAINST THE BUS'S OWN RECORD OF THE OPERATION: an owner that
/// could not offer says so in its own words, but when the bus had already ended the operation
/// because a bound participant was replaced or removed (a reload, an unload), that is the
/// fact a maker needs, and only the operator can read it.
std::string OpeningManager::refusal_of(const std::string& said, loom::Mail& mail) const {
    const loom::JointStatus status = mail.joint_status(authority_, flight_.op);
    if (status.state == loom::JointState::Aborted &&
        status.reason == loom::JointRefusal::ParticipantChanged) {
        return flight_.path +
               " was not opened -- the Editor or the desk was replaced while opening; try again";
    }
    return said;
}

bool OpeningManager::answers_flight(const loom::Mail& mail, std::int64_t op,
                                    const char* stage) const {
    // LOOM'S WORD FIRST: this is THE answer to a request this weave sent. Then which
    // request: the correlation is the operation, and the stage says which ask of it.
    return mail.answers_ask() && flight_.live && op == static_cast<std::int64_t>(flight_.op) &&
           mail.correlation() == flight_.op && flight_.stage == stage;
}

void OpeningManager::on(const OpenSourceRequested& asked, loom::Mail& mail) {
    if (mail.authored_role().empty()) {
        return; // the door's rule: opening a maker's source for anonymous speech is nobody's act
    }
    if (flight_.live) {
        // SUPERSEDED, EXPLICITLY: the newer intent ends the older one. The bus releases
        // its offers; both owners hear it ended; its requester hears why. A flight that
        // has already committed and is waiting on its owners' application is NOT
        // superseded -- nothing of it is the manager's to cancel -- and the newer request
        // waits its turn: refused now, in words, rather than queued.
        if (flight_.stage == "apply") {
            (void)mail.answer(SourceOpened{
                false, asked.path + " was not opened -- " + flight_.path +
                           " was just published and its owners are still applying it; "
                           "try again"});
            return;
        }
        (void)mail.cancel_joint(authority_, flight_.op);
        settle(false, false,
               flight_.path + " was not opened -- superseded by a newer request to open " +
                   asked.path,
               mail);
        state_.last_outcome = "superseded";
    }
    const loom::JointBegin begun = mail.begin_joint(
        authority_, {loom::claim_key<EditorDocument>(std::string_view(editor_office_)),
                     loom::claim_key<PanePresentation>(std::string_view(presentation_office_))});
    if (!begun.ok) {
        ++state_.refused;
        state_.last_outcome = "refused";
        state_.last_refusal = refusal_words(asked.path, begun.why);
        (void)mail.answer(SourceOpened{false, state_.last_refusal});
        return;
    }
    flight_ = Flight{};
    flight_.live = true;
    flight_.op = begun.op;
    flight_.path = asked.path;
    flight_.answer = mail.defer_answer();
    state_.op = static_cast<std::int64_t>(begun.op);
    state_.path = asked.path;
    state_.requester = static_cast<std::int64_t>(mail.sender().value);
    state_.last_outcome.clear();
    state_.last_refusal.clear();
    if (!ask(mail, presentation_office_,
             PresentationTrialRequested{static_cast<std::int64_t>(begun.op), pane_.provider,
                                        pane_.pane},
             "trial")) {
        // NOTHING WAS QUEUED: this weave could not author the ask (it does not hold the
        // opening office it speaks as). Not silence, and not a standalone mode: a refusal,
        // now, in words. (An unheld desk office is a DISPATCH refusal, heard later through
        // `zen.DispatchRefused`; `begin_joint` already refused it as NoClaim before this.)
        (void)mail.cancel_joint(authority_, flight_.op);
        settle(false, false,
               asked.path + " was not opened -- the desk could not be asked to show it", mail);
        return;
    }
    progress(mail, true);
}

void OpeningManager::on(const PresentationTrial& said, loom::Mail& mail) {
    if (!answers_flight(mail, said.op, "trial")) {
        return;
    }
    if (!said.ok) {
        (void)mail.cancel_joint(authority_, flight_.op);
        settle(false, false, said.refusal, mail);
        return;
    }
    if (!ask(mail, editor_office_,
             PrepareSourceRequested{static_cast<std::int64_t>(flight_.op), flight_.path,
                                    said.rows, said.columns},
             "prepare")) {
        (void)mail.cancel_joint(authority_, flight_.op);
        settle(false, false,
               flight_.path + " was not opened -- the Editor could not be asked to prepare it",
               mail);
        return;
    }
    progress(mail, true);
}

void OpeningManager::on(const SourcePrepared& said, loom::Mail& mail) {
    if (!answers_flight(mail, said.op, "prepare")) {
        return;
    }
    if (!said.ok) {
        const std::string refusal = refusal_of(said.refusal, mail);
        (void)mail.cancel_joint(authority_, flight_.op);
        settle(false, false, refusal, mail);
        return;
    }
    PresentationAdmitRequested admit;
    admit.op = static_cast<std::int64_t>(flight_.op);
    admit.provider = pane_.provider;
    admit.pane = pane_.pane;
    admit.generation = said.generation;
    admit.rows = said.rows;
    admit.caret_row = said.caret_row;
    admit.caret_col = said.caret_col;
    admit.sel_begin_row = said.sel_begin_row;
    admit.sel_begin_col = said.sel_begin_col;
    admit.sel_end_row = said.sel_end_row;
    admit.sel_end_col = said.sel_end_col;
    if (!ask(mail, presentation_office_, admit, "admit")) {
        (void)mail.cancel_joint(authority_, flight_.op);
        settle(false, false,
               flight_.path + " was not opened -- the desk could not be asked to admit it", mail);
        return;
    }
    progress(mail, true);
}

void OpeningManager::on(const PresentationAdmitted& said, loom::Mail& mail) {
    if (!answers_flight(mail, said.op, "admit")) {
        return;
    }
    if (!said.ok) {
        const std::string refusal = refusal_of(said.refusal, mail);
        (void)mail.cancel_joint(authority_, flight_.op);
        settle(false, false, refusal, mail);
        return;
    }
    // THE COMMITMENT. Everything either owner prepared is already offered; the bus
    // revalidates the exact participants, both revisions and both offers, and either
    // exchanges both claims in one step or changes nothing.
    const loom::JointResult done = mail.commit_joint(authority_, flight_.op);
    if (!done.ok) {
        settle(false, false, refusal_words(flight_.path, done.why), mail);
        return;
    }
    // PUBLISHED. What "opened" still needs is both owners' APPLICATION of it, which the bus
    // establishes when it shows each its published claim -- before that owner's next
    // delivery -- and tells this manager (`zen.JointApplied`). The one delivery each owner
    // needs is this progress word, said to both; the requester is answered from the bus's
    // record, afterwards, and never from the commit alone (editor-managed-open-slice-
    // corrections).
    flight_.stage = "apply";
    flight_.awaiting = editor_office_ + " and " + presentation_office_;
    flight_.attempt = loom::Ticket{};
    state_.stage = "apply";
    state_.awaiting = flight_.awaiting;
    state_.attempt = 0;
    const ManagedOpenProgress applying{static_cast<std::int64_t>(flight_.op), flight_.path,
                                       flight_.stage, flight_.awaiting, true};
    (void)mail.as_role(kOpeningRole).send_to_role(editor_office_, applying);
    (void)mail.as_role(kOpeningRole).send_to_role(presentation_office_, applying);
}

void OpeningManager::on(const loom::JointApplied& said, loom::Mail& mail) {
    // THE BUS'S WORD THAT THE SHOWINGS SETTLED. The shape is ordinary speech; the record is
    // the fact, re-read through the operator's own door, and only it decides.
    const std::uint64_t op = said.applied_op();
    if (op == 0) {
        return;
    }
    if (!flight_.live || op != flight_.op) {
        // A LATE WORD ABOUT AN OPERATION THAT ALREADY SETTLED (editor-managed-open-slice-
        // corrections-2): the record this manager RETAINED for a commitment an owner could
        // not apply has re-settled -- the held owner was reloaded and its successor was shown
        // the value. What the successor ANSWERED is the fact, re-read from the record this
        // manager kept for exactly this word: applied after repair, or NOT applied after
        // repair, because the successor kept its own state. A repaired owner is not proof
        // that its old operation applied. The requester's earlier answer stands either way;
        // the record is released once the word is recorded. A notice about anything this
        // manager does not hold consults nothing and decides nothing.
        if (retained_ == 0 || op != retained_) {
            return;
        }
        const loom::JointStatus status = mail.joint_status(authority_, op);
        if (status.state != loom::JointState::Committed) {
            release_retained(mail); // Missing: released elsewhere, nothing more to say
            return;
        }
        switch (status.application) {
        case loom::JointApplication::Applied:
            state_.last_outcome = "committed, applied after repair";
            state_.last_refusal.clear();
            release_retained(mail);
            return;
        case loom::JointApplication::Declined:
            state_.last_outcome =
                "committed, not applied after repair -- " +
                (status.failed_role.empty() ? std::string("its owner") : status.failed_role) +
                " kept its own state";
            state_.last_refusal = unapplied_words(retained_path_, status);
            release_retained(mail);
            return;
        case loom::JointApplication::Lost:
            state_.last_outcome = "committed, owner removed after failing";
            release_retained(mail);
            return;
        case loom::JointApplication::Failed:
            return; // the successor failed too: still held, still retained, told again later
        case loom::JointApplication::Pending:
        case loom::JointApplication::None:
            return;
        }
        return;
    }
    if (flight_.stage != "apply") {
        return; // nothing of this flight is published yet: a forged or stale notice
    }
    const loom::JointStatus status = mail.joint_status(authority_, op);
    if (status.state != loom::JointState::Committed) {
        return;
    }
    switch (status.application) {
    case loom::JointApplication::Applied:
        settle(true, true, std::string(), mail, status.application);
        return;
    case loom::JointApplication::Failed:
    case loom::JointApplication::Lost:
    case loom::JointApplication::Declined:
        settle(true, false, unapplied_words(flight_.path, status), mail, status.application);
        return;
    case loom::JointApplication::Pending:
    case loom::JointApplication::None:
        return; // still owed somewhere: the record, not the words, decides
    }
}

void OpeningManager::on(const loom::DispatchRefused& refused, loom::Mail& mail) {
    // LOOM'S LATER ATTESTATION that an addressed attempt of THIS weave was refused before
    // any handler ran. The shape alone is ordinary speech; the provenance is the fact, and
    // the attempt must be the one this flight is waiting on. (A held owner's refusal of the
    // `apply` word is met here too -- with no attempt awaited it is ignored, because the
    // bus's `zen.JointApplied` is the word that decides that stage.)
    if (!mail.dispatch_refused() || !flight_.live || !flight_.attempt.valid() ||
        refused.refused_attempt().seq != flight_.attempt.seq) {
        return;
    }
    (void)mail.cancel_joint(authority_, flight_.op);
    settle(false, false,
           flight_.path + " was not opened -- " + flight_.awaiting +
               " could not be reached (" + refused.reason + ")",
           mail);
}

void OpeningManager::on(const loom::JointEnded& ended, loom::Mail& mail) {
    // THE BUS ENDED THE OPERATION -- a bound participant was replaced or removed, or a bound
    // claim moved -- and an answer this manager awaits may have died with it. The shape is
    // ordinary speech; the bus's own record is the fact, and only it decides.
    if (!flight_.live || ended.ended_op() != flight_.op) {
        return;
    }
    const loom::JointStatus status = mail.joint_status(authority_, flight_.op);
    if (status.state != loom::JointState::Aborted) {
        return; // a forged or stale notice: the record says otherwise
    }
    settle(false, false,
           status.reason == loom::JointRefusal::ParticipantChanged
               ? flight_.path +
                     " was not opened -- the Editor or the desk was replaced while opening; try again"
               : refusal_words(flight_.path, status.reason),
           mail);
}

void OpeningManager::settle(bool committed, bool applied, const std::string& refusal,
                            loom::Mail& mail, loom::JointApplication application) {
    if (!flight_.live) {
        return;
    }
    const std::uint64_t op = flight_.op;
    const ManagedOpenSettled outcome{static_cast<std::int64_t>(op), committed, applied, refusal,
                                     flight_.path};
    // BOTH OWNERS HEAR IT ENDED, AFTER THE FACT. On a commitment each has already been
    // shown its published claim before this arrives; on a refusal each drops what it
    // prepared; an owner that could not apply is held by the bus and hears nothing, which
    // is exactly right. Neither message is the commitment.
    (void)mail.as_role(kOpeningRole).send_to_role(editor_office_, outcome);
    (void)mail.as_role(kOpeningRole).send_to_role(presentation_office_, outcome);
    progress(mail, false);
    // A NEWER REQUEST SETTLING RETIRES THE RECORD KEPT FOR AN OLDER ONE'S LATE WORD:
    // `last_outcome` describes this flight from here, so the older repair could no longer
    // be reported truthfully. The bound, said as one: at most one record is retained.
    if (retained_ != 0 && retained_ != op) {
        release_retained(mail);
    }
    state_.last_op = static_cast<std::int64_t>(op);
    if (committed && applied) {
        ++state_.committed;
        state_.last_outcome = "committed";
        state_.last_refusal.clear();
    } else if (committed) {
        ++state_.unapplied;
        state_.last_outcome = unapplied_outcome(application);
        state_.last_refusal = refusal;
    } else {
        ++state_.refused;
        state_.last_outcome = "refused";
        state_.last_refusal = refusal;
    }
    if (flight_.answer.valid()) {
        const loom::Ticket told = loom::answer_deferred(flight_.answer, mail,
                                                        SourceOpened{committed && applied, refusal});
        if (!told.valid()) {
            // THE REQUESTER IS NOT WHO ASKED ANY MORE (replaced, reloaded, gone). The
            // outcome stands; only its report was lost, and that is counted here.
            ++state_.answers_lost;
            state_.last_outcome += ", answer lost";
        }
    }
    const std::string path = flight_.path;
    flight_ = Flight{};
    state_.op = 0;
    state_.stage = "idle";
    state_.awaiting.clear();
    state_.attempt = 0;
    // THE RECORD (editor-managed-open-slice-corrections-2): consumed here, so released
    // here -- unless an owner is HELD under it. That record is retained, at most one, for
    // the late word about the owner's repair; nothing else is promised about it.
    if (committed && application == loom::JointApplication::Failed) {
        retained_ = op;
        retained_path_ = path;
        state_.retained = static_cast<std::int64_t>(op);
    } else {
        (void)mail.release_joint(authority_, op);
    }
}

void OpeningManager::release_retained(loom::Mail& mail) {
    if (retained_ == 0) {
        return;
    }
    (void)mail.release_joint(authority_, retained_);
    retained_ = 0;
    retained_path_.clear();
    state_.retained = 0;
}

} // namespace zengine::workshop
