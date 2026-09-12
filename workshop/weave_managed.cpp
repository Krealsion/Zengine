// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// EXPERIMENTAL (editor-managed-open-slice, 2026-09-11) — THE PRESENTATION OWNER'S HALF OF A
// MANAGED OPENING, compiled once into `zengine-workshop-logic` beside Workshop's own bodies.
//
// Workshop is the presentation owner today. What it owns in this conversation: the trial
// seat (judged on a copy, nothing moved), the admitted trial content, its own latest claim
// (`PanePresentation`, derived from the live session at the end of every delivery), and the
// native application of a published claim -- membership, seat, selection, keys, room and
// rows, all at once, inside the publication hook and before anything can observe the desk.
// What it does not own: the document, the operation, the outcome. It is asked, it answers,
// it offers, and it is shown what was published.
//
// ⚠ THE ADMISSION COUNTER. Every input Workshop routes to the managed pane advances
// `routed` in its claim, so an operation whose trial was prepared before that input was
// routed cannot commit while the input is still queued behind the commitment: admitted
// work keeps its subject (the document that was showing when the maker pressed), and an
// epoch mismatch at delivery is never a reason to drop or retarget it. Conservative on
// purpose -- a caret key aborts an open that a finer guard could have let through -- and
// stated as the cost it is.

#include "weave.hpp"

#include <cstdint>
#include <string>

namespace zengine::workshop {

namespace {

/// FNV-1a over a spelling of the active setup: membership, places, sizes and ranks. Any
/// authored change to the desk moves it, and with it the presentation claim's revision.
std::int64_t digest_of(const Setup& setup) {
    std::uint64_t h = 1469598103934665603ull;
    const auto mix = [&h](const std::string& s) {
        for (const char c : s) {
            h ^= static_cast<unsigned char>(c);
            h *= 1099511628211ull;
        }
        h ^= 0x1f;
        h *= 1099511628211ull;
    };
    for (const SetupPane& row : setup.panes) {
        mix(row.ref.provider);
        mix(row.ref.pane);
        mix(std::to_string(row.place.mode) + "," + std::to_string(row.place.x) + "," +
            std::to_string(row.place.y));
        mix(std::to_string(row.width.mode) + "," + std::to_string(row.width.amount));
        mix(std::to_string(row.height.mode) + "," + std::to_string(row.height.amount));
        mix(std::to_string(row.front));
    }
    return static_cast<std::int64_t>(h & 0x7fffffffffffffffull);
}

bool same_presentation(const PanePresentation& a, const PanePresentation& b) {
    return a.provider == b.provider && a.pane == b.pane && a.member == b.member &&
           a.seated == b.seated && a.selected == b.selected && a.keyboard == b.keyboard &&
           a.rows == b.rows && a.columns == b.columns &&
           a.content_generation == b.content_generation && a.routed == b.routed &&
           a.capacity == b.capacity && a.setup_digest == b.setup_digest &&
           a.shown_by == b.shown_by;
}

std::string tail(const std::string& path) {
    const std::size_t cut = path.find_last_of("/\\");
    return cut == std::string::npos ? path : path.substr(cut + 1);
}

/// THE MAKER'S WORDS FOR AN OFFER THE BUS REFUSED. The presentation this desk prepared was
/// for an operation that no longer stands as it was bound: the desk itself moved (a routed
/// input, a resize, an authored change), the operation was superseded, or a participant was
/// replaced. `name_of` is the diagnostic spelling, kept in the parenthesis.
std::string offer_refusal(const std::string& name, loom::JointRefusal why) {
    switch (why) {
    case loom::JointRefusal::StaleRevision:
    case loom::JointRefusal::ParticipantChanged:
    case loom::JointRefusal::WrongState:
    case loom::JointRefusal::Cancelled:
        return "the desk changed while opening " + name + " -- try again";
    default:
        return "the desk could not offer its presentation for " + name + " (" +
               loom::name_of(why) + ")";
    }
}

} // namespace

// ---- The managed pane, and what Workshop claims about it ----------------------------------

std::int64_t WorkshopWeave::managed_kind() const {
    const PaneRef& ref = host_->managed_pane;
    if (ref.provider.empty()) {
        return kNoPaneKind;
    }
    const RuntimePane* row = session_.panels.runtime.find(ref.provider, ref.pane);
    return row == nullptr ? kNoPaneKind : row->kind;
}

void WorkshopWeave::note_routed(std::int64_t kind) {
    if (kind != kNoPaneKind && kind == managed_kind()) {
        ++routed_;
    }
}

PanePresentation WorkshopWeave::derive_presentation() const {
    PanePresentation now;
    now.provider = host_->managed_pane.provider;
    now.pane = host_->managed_pane.pane;
    const std::int64_t kind = managed_kind();
    now.member = has_pane(session_.setup.active, host_->managed_pane);
    now.seated = kind != kNoPaneKind && session_.panels.has(kind);
    now.selected = kind != kNoPaneKind && session_.panels.selected == kind;
    now.keyboard = kind != kNoPaneKind && zengine::workshop::keyboard_pane(session_.panels) == kind;
    if (const ExternalPane* pane =
            kind == kNoPaneKind ? nullptr : session_.panels.external_pane(kind)) {
        now.rows = pane->granted ? pane->rows : 0;
        now.columns = pane->granted ? pane->columns : 0;
        now.content_generation = pane->content_generation;
    }
    now.routed = routed_;
    now.capacity = static_cast<std::int64_t>(stack_capacity(screen_of(session_)).slots);
    now.setup_digest = digest_of(session_.setup.active);
    now.shown_by = static_cast<std::int64_t>(shown_by_);
    return now;
}

// Silent when nothing changed -- the discipline of WL-ATTN-12, one claim over.
void WorkshopWeave::mirror_presentation(loom::Mail& mail) {
    if (host_->managed_pane.provider.empty()) {
        return; // a Workshop with no managed pane claims nothing and pays nothing
    }
    const PanePresentation now = derive_presentation();
    if (presentation_claimed_ && same_presentation(now, claimed_presentation_)) {
        return;
    }
    const loom::SenseClaimResult claimed = mail.claim(now);
    if (claimed.accepted) {
        claimed_presentation_ = now;
        presentation_claimed_ = true;
    }
}

void WorkshopWeave::after_delivery(loom::Mail& mail) {
    if (room_owed_) {
        room_owed_ = false;
        const std::int64_t kind = managed_kind();
        const RuntimePane* row =
            kind == kNoPaneKind ? nullptr : session_.panels.runtime.of_kind(kind);
        const ExternalPane* pane =
            kind == kNoPaneKind ? nullptr : session_.panels.external_pane(kind);
        if (row != nullptr && pane != nullptr && pane->granted) {
            (void)mail.as_role(kWorkshopProvider)
                .send_to_role(row->provider, PaneRoom{row->pane, pane->rows, pane->columns});
        }
    }
    if (repaint_owed_) {
        repaint_owed_ = false;
        repaint(mail);
    }
    mirror_presentation(mail);
}

// ---- The trial: would it seat, and what room would it get? ---------------------------------

WorkshopWeave::TrialRoom WorkshopWeave::trial_room(const Setup& candidate, std::int64_t kind,
                                                   const std::string& name) const {
    TrialRoom out;
    const Screen sc = screen_of(session_);
    const StackCapacity capacity = stack_capacity(sc);
    // THE PICKER'S OWN TRIAL, on the candidate setup: nothing moves.
    const Seating trial = seat_panes(candidate, session_.panels, capacity);
    for (const std::int64_t k : trial.waiting) {
        if (k == kind) {
            out.refusal = "no room for " + name +
                          " on this screen -- make the window taller, then p again";
            return out;
        }
    }
    // THE ROOM THE PANE'S BODY WOULD HAVE, measured on a COPY of the panels seated the way
    // the real application seats them, with the keys pointed at the pane as they will be
    // (the keyboard-holding pane keeps its title row, which changes its body).
    Panels seated = session_.panels;
    (void)reconcile(seated, candidate, capacity);
    if (!seated.has(kind)) {
        out.refusal = "no room for " + name + " on this screen";
        return out;
    }
    seated.selected = kind;
    seated.keyboard = kind_takes_keyboard(kind) ? kind : kNoPaneKind;
    const PanelBounds where = bounds_of(seated, candidate, kind, sc);
    if (!where.open) {
        out.refusal = "no room for " + name + " on this screen";
        return out;
    }
    const ExternalBodyPlace body = external_body_place(
        where.rect, sc, external_title_rows(seated, kind, session_.pane_titles));
    if (!body.present) {
        out.refusal = "no room for a row of " + name + " on this screen";
        return out;
    }
    out.ok = true;
    out.rows = body.rows;
    out.columns = body.columns;
    return out;
}

void WorkshopWeave::on(const PresentationTrialRequested& asked, loom::Mail& mail) {
    if (!mail.authored_from_role(kOpeningRole)) {
        return; // a trial is a desk question, and only the manager's office asks it
    }
    const RuntimePane* row = session_.panels.runtime.find(asked.provider, asked.pane);
    if (row == nullptr) {
        (void)mail.answer(PresentationTrial{asked.op, false,
                                            "no pane " + asked.provider + "/" + asked.pane +
                                                " is offered on this desk",
                                            0, 0});
        return;
    }
    Trial t;
    t.live = true;
    t.op = static_cast<std::uint64_t>(asked.op);
    t.ref = PaneRef{row->provider, row->pane};
    t.kind = row->kind;
    t.name = row->name;
    t.candidate = session_.setup.active;
    (void)add_pane(t.candidate, t.ref);
    const TrialRoom room = trial_room(t.candidate, t.kind, t.name);
    if (!room.ok) {
        trial_ = Trial{};
        (void)mail.answer(PresentationTrial{asked.op, false, room.refusal, 0, 0});
        return;
    }
    t.room_rows = room.rows;
    t.room_columns = room.columns;
    trial_ = std::move(t);
    (void)mail.answer(PresentationTrial{asked.op, true, std::string(), room.rows, room.columns});
}

// ---- The admission: the trial's content, and the presentation offered -----------------------

void WorkshopWeave::on(const PresentationAdmitRequested& asked, loom::Mail& mail) {
    if (!mail.authored_from_role(kOpeningRole)) {
        return;
    }
    if (!trial_.live || trial_.op != static_cast<std::uint64_t>(asked.op)) {
        (void)mail.answer(PresentationAdmitted{asked.op, false,
                                               "no trial stands for this opening -- try again"});
        return;
    }
    // RE-JUDGED AGAINST THE DESK AS IT IS NOW. The trial answered for an instant; the
    // admission is a later delivery, and the room may have moved between them.
    const TrialRoom room = trial_room(trial_.candidate, trial_.kind, trial_.name);
    if (!room.ok || room.rows != trial_.room_rows || room.columns != trial_.room_columns) {
        const std::string refusal =
            room.ok ? "the room for " + trial_.name + " changed while opening -- try again"
                    : room.refusal;
        trial_ = Trial{};
        (void)mail.answer(PresentationAdmitted{asked.op, false, refusal});
        return;
    }
    // THE CONTENT IS JUDGED AS ANY CONTENT IS, against the room the trial would grant.
    ExternalPane probe;
    probe.kind = trial_.kind;
    probe.rows = trial_.room_rows;
    probe.columns = trial_.room_columns;
    const Written judged = judge_content(PaneContent{asked.pane, asked.rows}, probe);
    if (!judged.accepted) {
        const std::string name = trial_.name;
        trial_ = Trial{};
        (void)mail.answer(PresentationAdmitted{asked.op, false, name + ": " + judged.refusal});
        return;
    }
    probe.shown = asked.rows;
    const Written caret = judge_caret(
        PaneCaret{asked.pane, asked.caret_row, asked.caret_col, asked.sel_begin_row,
                  asked.sel_begin_col, asked.sel_end_row, asked.sel_end_col},
        probe);
    trial_.rows = asked.rows;
    trial_.generation = asked.generation;
    trial_.caret_ok = caret.accepted;
    trial_.caret_row = asked.caret_row;
    trial_.caret_col = asked.caret_col;
    trial_.sel_begin_row = asked.sel_begin_row;
    trial_.sel_begin_col = asked.sel_begin_col;
    trial_.sel_end_row = asked.sel_end_row;
    trial_.sel_end_col = asked.sel_end_col;
    // THE PRESENTATION THIS DESK WOULD HAVE AFTER THE COMMITMENT, offered as its next claim
    // for exactly this operation. Derived the way the live claim is derived, over the
    // candidate: the same fields, the same digest, so the claim the hook then applies is
    // the claim the end of the next delivery would derive.
    PanePresentation offered;
    offered.provider = trial_.ref.provider;
    offered.pane = trial_.ref.pane;
    offered.member = true;
    offered.seated = true;
    offered.selected = true;
    offered.keyboard = kind_takes_keyboard(trial_.kind);
    offered.rows = trial_.room_rows;
    offered.columns = trial_.room_columns;
    offered.content_generation = asked.generation;
    offered.routed = routed_;
    offered.capacity = static_cast<std::int64_t>(stack_capacity(screen_of(session_)).slots);
    offered.setup_digest = digest_of(trial_.candidate);
    offered.shown_by = asked.op;
    const loom::JointResult offer = mail.offer(trial_.op, offered);
    if (!offer.ok) {
        const std::string name = trial_.name;
        trial_ = Trial{};
        (void)mail.answer(PresentationAdmitted{asked.op, false, offer_refusal(name, offer.why)});
        return;
    }
    (void)mail.answer(PresentationAdmitted{asked.op, true, std::string()});
}

// ---- The publication hook: the trial becomes the desk ----------------------------------------

bool WorkshopWeave::on_claim_published(const PanePresentation& published) {
    if (!trial_.live || published.shown_by != static_cast<std::int64_t>(trial_.op)) {
        // NOT A PRESENTATION THIS DESK PREPARED. By the mechanism this cannot happen (the
        // operation bound this exact incarnation and its trial); if it does, the desk keeps
        // what it has, says so, re-claims its own truth at the end of the next delivery --
        // and ANSWERS THAT IT DID NOT APPLY IT (editor-managed-open-slice-corrections-2). A
        // warning followed by an ordinary return used to count as Applied; the bus now
        // records Declined against this desk and this publication, holds nothing, and the
        // manager says "not applied" rather than "opened".
        say("the desk was published a presentation it did not prepare -- keeping the desk as "
            "it is",
            true);
        // THE CLAIM RECORD NOW SAYS `published`, WHICH THIS DESK DOES NOT STAND BEHIND: that
        // is what the mirror must compare the live desk against, or an unchanged desk would
        // look already claimed and the published value would stand forever.
        claimed_presentation_ = published;
        presentation_claimed_ = true;
        repaint_owed_ = true;
        return false;
    }
    // ALL OF IT, HERE, BEFORE ANYTHING CAN OBSERVE THE DESK: membership through the one
    // door the picker and a restore go through, the seat through the same reconcile, the
    // selection and the keys, and the pane's admitted room, rows and caret.
    session_.setup.active = trial_.candidate;
    apply_setup_now();
    const std::int64_t kind = trial_.kind;
    if (!session_.panels.has(kind)) {
        // THE BELT UNDER THE TRIAL: the same seating over the same capacity said yes at the
        // admission and the operation revalidated every fact since. A seat that did not
        // happen here is a defect in one of the two, and it is said as one -- and it is NOT
        // an application (editor-managed-open-slice-corrections-2): the published claim says
        // seated, selected and keyed, and this desk cannot stand behind it, so it answers
        // Declined, keeps the desk as the reconcile left it (nothing is rolled back: a
        // published claim is not this desk's to unpublish, and the membership it adopted is
        // an ordinary authored setup), drops the trial, and re-claims its own truth at the
        // end of the next delivery. Reached by no mechanism the cases know; source-argued.
        say("defect: " + trial_.name + " was published as seated but the desk did not seat it",
            true);
        claimed_presentation_ = published; // the record says this; the mirror will correct it
        presentation_claimed_ = true;
        trial_ = Trial{};
        repaint_owed_ = true;
        return false;
    }
    session_.panels.selected = kind;
    session_.panels.keyboard = kind_takes_keyboard(kind) ? kind : kNoPaneKind;
    if (ExternalPane* pane = session_.panels.external_pane(kind)) {
        pane->rows = trial_.room_rows;
        pane->columns = trial_.room_columns;
        pane->granted = true;
        pane->shown = trial_.rows;
        pane->heard = true;
        pane->awaiting = false;
        pane->clear_refusal();
        pane->content_generation = trial_.generation;
        if (trial_.caret_ok) {
            pane->caret_row = trial_.caret_row;
            pane->caret_col = trial_.caret_row == surface::kNoCaret ? 0 : trial_.caret_col;
            pane->sel_begin_row = trial_.sel_begin_row;
            pane->sel_begin_col = trial_.sel_begin_col;
            pane->sel_end_row = trial_.sel_end_row;
            pane->sel_end_col = trial_.sel_end_col;
        } else {
            pane->clear_caret();
        }
    }
    shown_by_ = trial_.op;
    say("showing " + trial_.name + " -- it opened " + tail(trial_.path) + ", and it has the keys",
        false);
    // WHAT THE BUS HOLDS UNDER THIS DESK'S KEY IS THE PUBLISHED VALUE NOW -- the mirror's
    // comparison point, so a desk that derives exactly it at the end of the next delivery
    // claims nothing again, and one that differs claims its own truth.
    claimed_presentation_ = published;
    presentation_claimed_ = true;
    trial_ = Trial{};
    repaint_owed_ = true;
    room_owed_ = true;
    return true;
}

// ---- What the manager says afterwards, and what a maker can read meanwhile ----------------

void WorkshopWeave::on(const ManagedOpenSettled& said, loom::Mail& mail) {
    if (!mail.authored_from_role(kOpeningRole)) {
        return;
    }
    session_.conditions.retract("opening:" + std::to_string(said.op));
    if (!said.committed) {
        if (trial_.live && trial_.op == static_cast<std::uint64_t>(said.op)) {
            trial_ = Trial{};
        }
        if (!said.refusal.empty()) {
            say(said.refusal, true);
        }
    } else if (!said.applied) {
        // PUBLISHED, AND AN OWNER COULD NOT APPLY IT (editor-managed-open-slice-corrections).
        // This desk applied its own half in the hook -- the seat, the keys, the admitted
        // rows -- and a published claim is not this desk's to unpublish, so nothing here
        // moves back. The maker reads which owner is held and what ends that.
        if (!said.refusal.empty()) {
            say(said.refusal, true);
        }
    }
    repaint(mail);
}

void WorkshopWeave::on(const ManagedOpenProgress& said, loom::Mail& mail) {
    if (!mail.authored_from_role(kOpeningRole)) {
        return;
    }
    const std::string key = "opening:" + std::to_string(said.op);
    if (!said.pending) {
        session_.conditions.retract(key);
    } else {
        if (trial_.live && trial_.op == static_cast<std::uint64_t>(said.op)) {
            trial_.path = said.path;
        }
        session_.conditions.establish(
            Condition{key, "opening " + tail(said.path),
                      "waiting for " + said.awaiting + " to " + said.stage + " -- your work "
                                                                             "is untouched",
                      surface::role::kAccent, std::string()});
    }
    repaint(mail);
}

} // namespace zengine::workshop
