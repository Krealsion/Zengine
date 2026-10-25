// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The presentation owner's half of a managed opening: the trial seat and the room its area would
// have, the admitted seat, this desk's own latest claim, and the native application of a published
// claim.

#include "weave.hpp"
#include "screen_canvas.hpp"

#include <cstdint>
#include <limits>
#include <string>

namespace zengine::workshop {

namespace {

/// FNV-1a over a spelling of the active setup: membership, places, sizes and ranks. Any of those
/// authored moves it, and with it the presentation claim's revision; the desk's name and a pane's
/// settings do not.
std::int64_t digest_of(const Setup& setup) {
    Fingerprint f;
    for (const SetupPane& row : setup.panes) {
        f.bytes(row.ref.provider);
        f.bytes(row.ref.pane);
        f.bytes(std::to_string(row.place.mode) + "," + std::to_string(row.place.x) + "," +
                std::to_string(row.place.y));
        f.bytes(std::to_string(row.width.mode) + "," + std::to_string(row.width.amount));
        f.bytes(std::to_string(row.height.mode) + "," + std::to_string(row.height.amount));
        f.bytes(std::to_string(row.front));
    }
    return f.value();
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

/// THE WEAVER'S WORDS FOR AN OFFER THE BUS REFUSED. The presentation this desk prepared was
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
    const RuntimePane* row = session_.panes.runtime.find(ref.provider, ref.pane);
    return row == nullptr ? kNoPaneKind : row->kind;
}

/// Every input routed to the managed pane moves `routed` in the desk's claim, so a trial prepared
/// before it cannot commit (WL-OPEN-03). Conservative: a caret key aborts an open a finer guard
/// could have let through.
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
    now.seated = kind != kNoPaneKind && session_.panes.has(kind);
    now.selected = kind != kNoPaneKind && session_.panes.selected == kind;
    now.keyboard = kind != kNoPaneKind && zengine::workshop::keyboard_pane(session_.panes) == kind;
    if (const ExternalPane* pane =
            kind == kNoPaneKind ? nullptr : session_.panes.external_pane(kind)) {
        // ITS BODY AS PLACED NOW, in the medium's rows and columns.
        const Screen sc = screen_of(session_);
        const PaneBounds where = bounds_of(session_.panes, session_.setup.active, kind, sc);
        const ExternalBodyPlace body = external_body_place(
            where.rect, sc, external_title_rows(session_.panes, kind, session_.pane_titles));
        now.rows = where.open && body.present ? body.rows : 0;
        now.columns = where.open && body.present ? body.columns : 0;
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
    if (seat_owed_) {
        // THE MANAGED OPENING'S SEAT, asked of the store; the proposal stands until it answers.
        const PaneRef ref = *seat_owed_;
        seat_owed_.reset();
        LayoutEdit seat;
        seat.op = layout_op::kSeat;
        seat.provider = ref.provider;
        seat.pane = ref.pane;
        ask_layout({seat},
                   [this](const LayoutEditAnswered& answered, loom::Mail&) {
                       if (!answered.accepted) {
                           say(answered.refusal, true);
                       }
                   },
                   mail, false);
    }
    if (settings_owed_) {
        // THE PANE'S SETTINGS BEFORE THE AREA IT WAS SEATED IN, whatever this delivery was: the
        // repaint owed below comes after them, and hands nothing its holder has already heard.
        settings_owed_ = false;
        hand_settings(mail);
    }
    if (repaint_owed_) {
        repaint_owed_ = false;
        repaint(mail);
    }
    mirror_presentation(mail);
    // ONE NOTICE A DELIVERY, after every repaint it made: the newest stands for each before it.
    // What a pane shows comes as the window says it settled (`on(AreaSettled)`).
    if (stamps_owed_) {
        stamps_owed_ = false;
        finish_desk_waits(mail);
        publish_desk_stamps(mail);
    }
}

// ---- The trial: would it seat, and what room would it get? ---------------------------------

WorkshopWeave::TrialRoom WorkshopWeave::trial_room(const Setup& candidate, std::int64_t kind,
                                                   const std::string& name) const {
    TrialRoom out;
    const Screen sc = screen_of(session_);
    // THE ROOM THE PANE'S BODY WOULD HAVE, measured on a COPY of the panes seated the way the real
    // application seats them -- every resolved row, the stack's column beginning again at its top
    // -- with the keys pointed at the pane as they will be (the keyboard-holding pane keeps its
    // title row, which changes its body). Nothing moves.
    Panes seated = session_.panes;
    (void)reconcile(seated, candidate);
    if (!seated.has(kind)) {
        out.refusal = "defect: " + name + " would not be seated";
        return out;
    }
    seated.selected = kind;
    seated.keyboard = kind_takes_keyboard(kind) ? kind : kNoPaneKind;
    const PaneBounds where = bounds_of(seated, candidate, kind, sc);
    const std::int64_t titles = external_title_rows(seated, kind, session_.pane_titles);
    const ExternalBodyPlace body = external_body_place(where.rect, sc, titles);
    if (!body.present) {
        out.refusal = "no room for a row of " + name + " on this screen";
        return out;
    }
    out.ok = true;
    out.rows = body.rows;
    out.columns = body.columns;
    // ...and the area Workshop would place for it there, under the same title rows, and how the
    // window that shows the desk sets text in it.
    const PixelRect area = where.open ? canvas_body_place(where.rect, sc, titles) : PixelRect{};
    out.room = OpeningRoom{area.w, area.h, sc.cell_px, sc.cell_px > 0 ? sc.text_advance_px : 0,
                           sc.cell_px > 0 ? sc.text_line_px : 0};
    return out;
}

void WorkshopWeave::on(const PresentationTrialRequested& asked, loom::Mail& mail) {
    if (!mail.authored_from_role(kOpeningRole)) {
        return; // a trial is a desk question, and only the manager's office asks it
    }
    const RuntimePane* row = session_.panes.runtime.find(asked.provider, asked.pane);
    if (row == nullptr) {
        (void)mail.answer(v3::PresentationTrial{asked.op, false,
                                                "no pane " + asked.provider + "/" + asked.pane +
                                                    " is offered on this desk",
                                                {}});
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
        (void)mail.answer(v3::PresentationTrial{asked.op, false, room.refusal, {}});
        return;
    }
    t.room_rows = room.rows;
    t.room_columns = room.columns;
    t.room = room.room;
    trial_ = std::move(t);
    (void)mail.answer(v3::PresentationTrial{asked.op, true, std::string(), trial_.room});
}

// ---- The admission: the trial's content, and the presentation offered -----------------------

// THE CANDIDATE IS READY: the seat the trial judged is admitted, at the candidate's generation.
void WorkshopWeave::on(const v3::PresentationAdmitRequested& asked, loom::Mail& mail) {
    if (!mail.authored_from_role(kOpeningRole)) {
        return;
    }
    if (!trial_.live || trial_.op != static_cast<std::uint64_t>(asked.op)) {
        (void)mail.answer(PresentationAdmitted{asked.op, false,
                                               "no trial stands for this opening -- try again"});
        return;
    }
    if (const std::string refusal = trial_room_stands(); !refusal.empty()) {
        (void)mail.answer(PresentationAdmitted{asked.op, false, refusal});
        return;
    }
    trial_.admitted = true;
    trial_.generation = asked.generation;
    offer_presentation(asked.op, asked.generation, mail);
}

// RE-JUDGED AGAINST THE DESK AS IT IS NOW. The trial answered for an instant; the admission is a
// later delivery, and the room -- its area's too -- may have moved between them.
std::string WorkshopWeave::trial_room_stands() {
    const TrialRoom room = trial_room(trial_.candidate, trial_.kind, trial_.name);
    const OpeningRoom& was = trial_.room;
    const bool same_area = room.room.width == was.width && room.room.height == was.height &&
                           room.room.cell_px == was.cell_px &&
                           room.room.text_advance_px == was.text_advance_px &&
                           room.room.text_line_px == was.text_line_px;
    if (room.ok && room.rows == trial_.room_rows && room.columns == trial_.room_columns &&
        same_area) {
        return std::string();
    }
    const std::string refusal =
        room.ok ? "the room for " + trial_.name + " changed while opening -- try again" : room.refusal;
    trial_ = Trial{};
    return refusal;
}

void WorkshopWeave::offer_presentation(std::int64_t op, std::int64_t generation, loom::Mail& mail) {
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
    offered.content_generation = generation;
    offered.routed = routed_;
    offered.capacity = static_cast<std::int64_t>(stack_capacity(screen_of(session_)).slots);
    offered.setup_digest = digest_of(trial_.candidate);
    offered.shown_by = op;
    const loom::JointResult offer = mail.offer(trial_.op, offered);
    if (!offer.ok) {
        const std::string name = trial_.name;
        trial_ = Trial{};
        (void)mail.answer(PresentationAdmitted{op, false, offer_refusal(name, offer.why)});
        return;
    }
    (void)mail.answer(PresentationAdmitted{op, true, std::string()});
}

// ---- The publication hook: the trial becomes the desk ----------------------------------------

// WL-OPEN-09 -- agents/workshop/opening.md
loom::Weave::PublishedClaim WorkshopWeave::on_claim_published(const PanePresentation& published) {
    // THE DESK IS A NATIVE OWNER, SO ITS APPLICATION RUNS INSIDE THE HOST'S BOUNDARY: a throw
    // from anything below is this desk's own failure -- Loom holds the desk, and the host's
    // turn tells the words kept here -- and never an exception left for the pump to explain.
    return contain_showing(&host_->showings, self_, [this, &published] {
        return show_presentation(published) ? loom::Weave::PublishedClaim::Applied
                                            : loom::Weave::PublishedClaim::Declined;
    });
}

bool WorkshopWeave::show_presentation(const PanePresentation& published) {
    if (!trial_.live || published.shown_by != static_cast<std::int64_t>(trial_.op)) {
        // Not a presentation this desk prepared (the operation bound this incarnation and its
        // trial, so it should not happen): the desk keeps what it has, says so, and answers that
        // it did not apply it.
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
    // ALL OF IT, HERE, BEFORE ANYTHING CAN OBSERVE THE DESK: the seat through the same reconcile,
    // the selection, the keys and the generation. The row is the layouts' store's to add, so it is
    // painted now over the store's desk and asked of the store when this desk next speaks.
    seat_owed_ = trial_.ref;
    (void)add_pane(session_.setup.active, trial_.ref);
    apply_setup_now();
    const std::int64_t kind = trial_.kind;
    if (!session_.panes.has(kind)) {
        // The belt under the trial: a seat that did not happen is a defect, said as one, and not
        // an application. The desk answers Declined, keeps the desk as reconciled (a published
        // claim is not its to unpublish), drops the trial, and re-claims its truth at the end of
        // the next delivery.
        say("defect: " + trial_.name + " was published as seated but the desk did not seat it",
            true);
        claimed_presentation_ = published; // the record says this; the mirror will correct it
        presentation_claimed_ = true;
        trial_ = Trial{};
        repaint_owed_ = true;
        return false;
    }
    session_.panes.selected = kind;
    session_.panes.keyboard = kind_takes_keyboard(kind) ? kind : kNoPaneKind;
    if (ExternalPane* pane = session_.panes.external_pane(kind)) {
        pane->content_generation = trial_.generation;
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
    settings_owed_ = true;
    return true;
}

// ---- What the manager says afterwards, and what a weaver can read meanwhile ----------------

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
        // PUBLISHED, AND AN OWNER COULD NOT APPLY IT.
        // This desk applied its own half in the hook -- the seat, the keys, the admitted
        // rows -- and a published claim is not this desk's to unpublish, so nothing here
        // moves back. The weaver reads which owner is held and what ends that.
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
                      graphics::role::kAccent, std::string()});
    }
    repaint(mail);
}

// WL-SWITCH-07 -- agents/workshop/editor-switch.md
void WorkshopWeave::on(const EditorSwitchProgress& said, loom::Mail& mail) {
    if (!mail.authored_from_role(kEditorSwitchRole)) {
        return;
    }
    const std::string key = "editor-switch:" + std::to_string(said.op);
    if (!said.pending) {
        // A STATUS ANSWER ENDS NOTHING: a switch it describes keeps its condition.
        if (said.outcome != switch_outcome::kStatus) {
            session_.conditions.retract(key);
        }
        // WHAT CAME OF IT, said once where the weaver reads: the Terminal a switch is asked from shows
        // an answer's shape and not its words.
        if (!said.outcome.empty()) {
            say("editor switch" + (said.op > 0 ? " " + std::to_string(said.op) : std::string()) + ": " +
                    said.outcome + (said.detail.empty() ? std::string() : " -- " + said.detail),
                said.outcome == switch_outcome::kRefused || said.outcome == switch_outcome::kFailedAfterCommit);
        }
        repaint(mail);
        return;
    }
    const std::string op = std::to_string(said.op);
    const std::string cancel = " -- cancel: ask @" + std::string(kEditorSwitchRole) +
                               " EditorSwitchCancelled 1 op=" + op;
    std::string detail;
    if (said.stage == "awaiting-confirmation") {
        // THE LINE TO TYPE COMES FIRST, because a notice is cut to the band's width.
        const std::string confirm = "confirm: ask @" + std::string(kEditorSwitchRole) +
                                    " EditorSwitchConfirmed 1 op=" + op + " consent=" + said.consent;
        detail = confirm + cancel +
                 (said.detail.empty() ? " -- switching to " + said.destination + " would lose something"
                                      : " -- " + said.detail);
        if (said.outcome == switch_outcome::kNeedsConfirmation) {
            say("editor switch " + op + ": needs-confirmation -- " + confirm +
                    (said.detail.empty() ? std::string() : " -- " + said.detail),
                false);
        }
    } else if (said.stage == "boundary" || said.stage == "adopting") {
        detail = "the Editor holds still while " + said.awaiting + " takes the document; input to it "
                 "is refused until the switch settles" + cancel;
    } else if (said.stage == "proving" || said.stage == "retiring") {
        detail = "the switch has committed; waiting for " + said.awaiting + " to confirm it";
    } else {
        detail = "waiting for " + said.awaiting + " (" + said.stage + ") -- your work is untouched" +
                 cancel;
    }
    session_.conditions.establish(Condition{key, "switching the Editor to " + said.destination,
                                            detail, graphics::role::kAccent, std::string()});
    repaint(mail);
}

} // namespace zengine::workshop
