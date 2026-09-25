// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_OPENING_HPP
#define ZENGINE_WORKSHOP_OPENING_HPP

// The opening manager: one owner for the open operation, carrying a requester's intent through
// the two owners' preparation, committing the joint publication and reporting the outcome it can
// establish (WL-OPEN, agents/workshop/opening.md).

#include "open_seam_vocabulary.hpp"
#include "pane_seam_vocabulary.hpp"
#include "setup.hpp"

#include <zen/weave.hpp>
#include <zen/weave/dispatch_refusal.hpp>

#include <cstdint>
#include <string>

namespace zengine::workshop {

/// What a maker, a probe or a case can read of the manager. The live operation (`op` through
/// `requester`) is set when it begins and cleared when it settles; the latest terminal result
/// (`last_path` through `last_op`) changes only when a newer outcome is recorded, or when the
/// retained record's repair rewrites its own.
struct OpeningState {
    std::int64_t op = 0;         ///< the live operation, or 0
    std::string path;            ///< what the live operation opens
    std::string stage;           ///< idle | trial | prepare | admit | apply
    std::string awaiting;        ///< the office the outstanding attempt is addressed to
    std::int64_t attempt = 0;    ///< the queued attempt (Loom's sequence) awaited, or 0
    std::int64_t requester = 0;  ///< who asked for the live operation (a WeaveId, diagnostic)
    std::string last_path;       ///< what the latest answered request asked to open
    std::int64_t last_requester = 0; ///< who asked it (a WeaveId, diagnostic only)
    /// committed | committed, application failed | committed, not applied | committed,
    /// owner removed | committed, answer lost | committed, applied after repair |
    /// committed, not applied after repair -- <office> kept its own state | refused |
    /// superseded | ''
    std::string last_outcome;
    std::string last_refusal;    ///< the maker's sentence for it, when it was not opened
    std::int64_t last_op = 0;    ///< the operation it settled, or 0: refused before one existed
    std::int64_t committed = 0;  ///< how many opens this manager established, applied and all
    std::int64_t unapplied = 0;  ///< commitments an owner did not apply (failed, declined, lost)
    std::int64_t refused = 0;
    std::int64_t answers_lost = 0; ///< outcomes a replaced requester never heard
    /// The one record this manager still holds after settling (WL-OPEN-06): a commitment an owner
    /// could not apply, kept for the late word about its repair; or 0.
    std::int64_t retained = 0;
    ZEN_SHAPE(OpeningState, 1, ZEN_FIELD(op), ZEN_FIELD(path), ZEN_FIELD(stage),
              ZEN_FIELD(awaiting), ZEN_FIELD(attempt), ZEN_FIELD(requester),
              ZEN_FIELD(last_path), ZEN_FIELD(last_requester), ZEN_FIELD(last_outcome),
              ZEN_FIELD(last_refusal), ZEN_FIELD(last_op), ZEN_FIELD(committed),
              ZEN_FIELD(unapplied), ZEN_FIELD(refused), ZEN_FIELD(answers_lost),
              ZEN_FIELD(retained));
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
        std::int64_t requester = 0;
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
    /// A REQUEST REFUSED BEFORE IT BECAME AN OPERATION -- no slot at `begin`, or a commitment
    /// still applying -- answered now, in words, and recorded as the latest terminal result
    /// under its own path and requester with no operation; the live operation is not touched.
    void refuse_request(const std::string& path, const std::string& refusal, loom::Mail& mail);
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
    /// The retained record's operation, path and requester (`OpeningState::retained` mirrors
    /// the op): the request its late word is about.
    std::uint64_t retained_ = 0;
    std::string retained_path_;
    std::int64_t retained_requester_ = 0;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_OPENING_HPP
