// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_OPENING_HPP
#define ZENGINE_WORKSHOP_OPENING_HPP

// THE OPENING MANAGER (WL-OPEN, agents/workshop/opening.md).
//
// One focused owner for the OPEN OPERATION, and nothing else: it carries a requester's
// intent through the two owners' preparation, commits the joint publication, and reports
// the outcome it can establish. It holds no document, no rows, no room and no focus; it
// never receives a pointer into either owner; it learns what it knows from answers and
// from the bus, and it can be inspected (`zen.PokeRead` on `OpeningState`) to say which
// open is pending, who it is waiting on, and what failure was actually observed.
//
// THE CONVERSATION is `open_seam_vocabulary.hpp`'s, in this order:
//
//     OpenSourceRequested            (a requester, as an office)
//       begin_joint                   the exact Editor and presentation owner are bound
//       PresentationTrialRequested    -> PresentationTrial{rows, columns}
//       PrepareSourceRequested        -> SourcePrepared{generation, rows, caret}
//       PresentationAdmitRequested    -> PresentationAdmitted
//       commit_joint                  THE COMMITMENT (the claims say B from here)
//       ManagedOpenProgress{apply}    to both owners: each is shown its claim before it
//       zen.JointApplied              the bus's word of what the showings came to
//       ManagedOpenSettled            to both owners, afterwards
//       SourceOpened                  to the requester, afterwards
//
// WHAT IT ESTABLISHES, EXACTLY. `commit_joint` returning ok is the publication: both claims
// changed in one protected step and every reader after it sees both. "Opened" -- the
// terminal answer -- is that AND both owners' application of it, which the bus records when
// it shows each owner its published claim and tells this manager (`zen.JointApplied`,
// re-read from the bus's own record). A refusal at any step, an abort by the bus (a revision
// moved, a participant was replaced or removed -- said to this manager as `zen.JointEnded`),
// a refused enqueue, an authenticated dispatch refusal of an outstanding attempt, or a
// competing request all end the operation with nothing published. A commitment an owner
// could not apply ends it with the publication standing and the answer saying WHICH owner
// failed: that owner is held by the bus until it is reloaded or removed, and nothing here is
// rolled back or reinterpreted. Delivered silence is NOT ended by this manager: the operation
// stays pending, bounded to one, and says so -- before the commitment (an owner that never
// answers) and after it (an owner never shown).
//
// ⚠ ONE FLIGHT AT A TIME, SUPERSEDED EXPLICITLY. A second request while one is pending
// cancels the first (the bus releases its offers), answers its requester "superseded", and
// begins the new one. There is no queue of intents and no retry.
//
// ⚠ A LOST TERMINAL ANSWER DOES NOT UNDO A COMMITMENT. A requester replaced between its ask
// and the outcome loses its answer right; the manager counts the loss and the published
// facts stand. Losing the answer is a fact about the requester, not about the open.
//
// ⚠ THE RECORD IS KEPT UNTIL THIS MANAGER RELEASES IT (WL-OPEN-06). The bus keeps an operation's record -- committed with its application,
// aborted with its reason -- until its operator releases it, so the notice that wakes this
// manager always finds the record it names, and an unrelated coordination begun meanwhile
// takes nothing from it (publication is not the end of an outcome's lifetime; a queued
// notification is not consumption). This manager releases every record when its flight
// settles, except a commitment an owner could not apply: that one it RETAINS
// (`OpeningState::retained`, at most one) for the late word about the held owner's repair --
// "applied after repair", or "not applied after repair" when the successor kept its own
// state -- and releases it once that word is recorded, or when a newer request settles,
// after which no late word about it can be reported. A repaired owner is not proof that its
// old operation applied: the record, re-read, is.

#include "open_seam_vocabulary.hpp"
#include "pane_seam_vocabulary.hpp"
#include "setup.hpp"

#include <zen/weave.hpp>
#include <zen/weave/dispatch_refusal.hpp>

#include <cstdint>
#include <string>

namespace zengine::workshop {

/// WHAT A MAKER, A PROBE OR A CASE CAN READ OF THE MANAGER: the pending open, its stage,
/// who it is waiting on and with which queued attempt, and what the last one came to.
struct OpeningState {
    std::int64_t op = 0;         ///< the live operation, or 0
    std::string path;            ///< what it opens
    std::string stage;           ///< idle | trial | prepare | admit | apply
    std::string awaiting;        ///< the office the outstanding attempt is addressed to
    std::int64_t attempt = 0;    ///< the queued attempt (Loom's sequence) awaited, or 0
    std::int64_t requester = 0;  ///< who asked (a WeaveId, diagnostic only)
    /// committed | committed, application failed | committed, not applied | committed,
    /// owner removed | committed, answer lost | committed, applied after repair |
    /// committed, not applied after repair -- <office> kept its own state | refused |
    /// superseded | ''
    std::string last_outcome;
    std::string last_refusal;    ///< the maker's sentence for the last refusal
    std::int64_t last_op = 0;    ///< the last operation that settled
    std::int64_t committed = 0;  ///< how many opens this manager established, applied and all
    std::int64_t unapplied = 0;  ///< commitments an owner did not apply (failed, declined, lost)
    std::int64_t refused = 0;
    std::int64_t answers_lost = 0; ///< outcomes a replaced requester never heard
    /// THE ONE RECORD THIS MANAGER STILL HOLDS AFTER SETTLING (WL-OPEN-06): a commitment an owner could not apply, kept at the bus for the late
    /// word about that owner's repair, or 0. Every other settled record is released at once;
    /// this one when the repair re-settles it, or when a newer request settles.
    std::int64_t retained = 0;
    ZEN_SHAPE(OpeningState, 1, ZEN_FIELD(op), ZEN_FIELD(path), ZEN_FIELD(stage),
              ZEN_FIELD(awaiting), ZEN_FIELD(attempt), ZEN_FIELD(requester),
              ZEN_FIELD(last_outcome), ZEN_FIELD(last_refusal), ZEN_FIELD(last_op),
              ZEN_FIELD(committed), ZEN_FIELD(unapplied), ZEN_FIELD(refused),
              ZEN_FIELD(answers_lost), ZEN_FIELD(retained));
};

class OpeningManager
    : public loom::WeaveBase<OpeningManager, OpeningState,
                             loom::Accept<OpenSourceRequested, PresentationTrial, SourcePrepared,
                                          PresentationAdmitted, loom::DispatchRefused,
                                          loom::JointEnded, loom::JointApplied>,
                             loom::Emit<PresentationTrialRequested, PrepareSourceRequested,
                                        PresentationAdmitRequested, ManagedOpenSettled,
                                        ManagedOpenProgress, SourceOpened>> {
public:
    /// HOST WIRING, AND ALL OF IT: the two offices it coordinates and the pane the
    /// presentation owner presents the document through; the authority the host mints for
    /// this weave's own id arrives through `set_authority` once the id exists. Nothing else
    /// reaches this class from the host.
    OpeningManager(std::string editor_office, std::string presentation_office, PaneRef pane);
    void set_authority(loom::JointAuthority authority) { authority_ = std::move(authority); }

    void on(const OpenSourceRequested& asked, loom::Mail& mail);
    void on(const PresentationTrial& said, loom::Mail& mail);
    void on(const SourcePrepared& said, loom::Mail& mail);
    void on(const PresentationAdmitted& said, loom::Mail& mail);
    void on(const loom::DispatchRefused& refused, loom::Mail& mail);
    void on(const loom::JointEnded& ended, loom::Mail& mail);
    void on(const loom::JointApplied& applied, loom::Mail& mail);

    const OpeningState& state() const { return state_; }

private:
    /// THE ONE OPERATION IN FLIGHT.
    struct Flight {
        bool live = false;
        std::uint64_t op = 0;
        std::string path;
        loom::DeferredAnswer answer;
        loom::Ticket attempt{};
        std::string stage;
        std::string awaiting;
    };

    /// Address one office as this manager's own office, remember the attempt, and say
    /// where the operation now stands. Answers whether anything was queued at all.
    template <class Shape>
    bool ask(loom::Mail& mail, const std::string& office, const Shape& shape,
             const char* stage);
    /// End the flight: tell both owners, retract the standing condition, answer the
    /// requester if the right still stands, record what came of it, and release the
    /// bus's record -- or retain it, for a commitment an owner could not apply
    /// (`application` Failed), until the late word about the repair. `committed` is the
    /// publication; `applied` is every owner's application of it; the answer is both.
    void settle(bool committed, bool applied, const std::string& refusal, loom::Mail& mail,
                loom::JointApplication application = loom::JointApplication::None);
    /// Every newer terminal outcome that takes this manager's public result retires the one
    /// retained record, unless it is the operation settling now (`settling`; 0 for an
    /// immediate refusal that settles nothing).
    void retire(loom::Mail& mail, std::uint64_t settling);
    /// Release the one retained record, if any: its late word was recorded, or can no
    /// longer be reported.
    void release_retained(loom::Mail& mail);
    void progress(loom::Mail& mail, bool pending);
    bool answers_flight(const loom::Mail& mail, std::int64_t op, const char* stage) const;
    std::string refusal_of(const std::string& said, loom::Mail& mail) const;

    loom::JointAuthority authority_;
    std::string editor_office_;
    std::string presentation_office_;
    PaneRef pane_;
    Flight flight_;
    /// The retained record's operation and path (`OpeningState::retained` mirrors the op).
    std::uint64_t retained_ = 0;
    std::string retained_path_;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_OPENING_HPP
