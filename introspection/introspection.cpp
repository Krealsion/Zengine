// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Introspection provider — a loadable weave that offers Workshop one
// read-only pane showing what the Loom it is running in has actually loaded.
//
// IT IS A TOOL AND NOT A FIXTURE. `tests/weavelib/workshop_hello.cpp` proved the
// pane seam works through the real ABI and says of itself that it is built by
// `tests/` and named in no host's boot list; this one is built beside the Timer,
// booted by `zengine-workshop` beside the Skin and the reader, and its rows are
// facts about the running system rather than an echo of the room it was granted.
//
// ---- WHAT IT KNOWS, AND HOW IT COMES TO KNOW IT -----------------------------
//
//     the fact      which libraries this Loom's Kernel has loaded, and the role
//                   each was bound to at load
//     the owner     the Kernel. Its `loaded()` map is the authority and there is
//                   no second copy of it anywhere -- not in Workshop, not here
//     the path      zen.ListLoaded -> the Weave Manager -> zen.ListLibraries ->
//                   the control door -> the Kernel's live map -> zen.Result
//     currency      a SNAPSHOT, re-read on every room grant. Loom offers a
//                   participant no arrival or departure event, so there is
//                   nothing to subscribe to and nothing here polls
//     absence       an empty map is an observed zero; a map that has not been
//                   answered yet produces NO content at all, so Workshop's own
//                   `(waiting for the provider)` says it rather than a zero
//
// ---- WHAT IT CANNOT DO ------------------------------------------------------
//
// It writes no file, starts no process, opens no socket, holds no timer, reads no
// Sense, publishes no canvas, and commands no lifecycle. Its whole outbound
// vocabulary is three shapes: two sentences of the pane protocol, and the one
// question `zen.ListLoaded` -- which is the enumeration half of the control
// vocabulary and NOT the load capability. `zen.LoadWeave`, `zen.SwapWeave`,
// `zen.ReloadWeave`, `zen.UnloadLibrary` and `zen.UnloadRole` are absent from its
// Emit set and from every grant a host would write for it, and a grant is per
// (shape, version, target), so being able to ask what is loaded is not being able
// to load anything.
//
// AND THE LOADER IS WIDER THAN ANY OF THAT, WHICH IS REPORTED RATHER THAN HIDDEN.
// `Kernel::load` binds `Grant{}.allow_any()` to every library it opens, and a
// declared `Emit<...>` is informational rather than enforced (zen/weave/weave.hpp
// says so at the declaration). So the narrowness above is a fact about what this
// weave DOES and about what the pane protocol REACHES -- it is not a containment
// claim about the loader, and INTR-0 does not make one. An in-process dynamic
// weave shares this address space; that is the isolation tier's problem, it
// predates this tool, and it is identically true of the Skin, the reader and the
// Timer this host already boots.

#include "loaded.hpp"
#include "vocabulary.hpp"

#include "activation/activation.hpp"
#include "surface/vocabulary.hpp"
#include "workshop/pane_vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace {

namespace surface = zengine::surface;
using zengine::introspection::kIntrospectionRole;
using zengine::introspection::kLoadedPane;
using zengine::introspection::kLoadedPaneName;
using zengine::introspection::kLoadedPaneSummary;
using zengine::workshop::PaneCatalogRequested;
using zengine::workshop::PaneContent;
using zengine::workshop::PaneOffered;
using zengine::workshop::PaneRoom;

/// The office Workshop holds, named as a STRING rather than reached through
/// `workshop/panel.hpp`. A provider is a stranger to Workshop's internals and must
/// be able to say who it is talking to the way a third party would.
constexpr const char* kWorkshopRole = "zengine.workshop";

/// WHAT THIS PROVIDER HAS DONE, and it is all counters.
///
/// NO INVENTORY IS IN HERE. The loaded map belongs to the Kernel; keeping a copy
/// beside it would be a second answer to a question that already has one, and the
/// copy is the one that goes stale. What survives a snapshot and a revive is only
/// the bookkeeping -- how many times this office spoke, was granted room, answered,
/// and refused to answer somebody who was not Workshop.
struct IntrospectionState {
    std::int64_t offers = 0;
    std::int64_t rooms = 0;
    std::int64_t readings = 0; ///< answers from the Manager that became content
    std::int64_t refused = 0;  ///< asks and rooms not authored by the Workshop office
    ZEN_EXPOSE();
    ZEN_SHAPE(IntrospectionState, 1, ZEN_FIELD(offers), ZEN_FIELD(rooms), ZEN_FIELD(readings),
              ZEN_FIELD(refused));
};

class IntrospectionWeave
    : public loom::WeaveBase<
          IntrospectionWeave, IntrospectionState,
          loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom, loom::Result,
                       loom::Refused>,
          loom::Emit<PaneOffered, PaneContent, loom::ListLoaded>> {
public:
    /// FIRST BREATH, AND ONLY IF LOOM SAYS SO. `ActivationCursor` owns both halves
    /// of that sentence: the lifecycle attestation must be Loom's, and the sequence
    /// must be one this incarnation has not already acted on. An ordinary
    /// `zen.Activated` sent by anybody granted the shape announces nothing here --
    /// otherwise any weave could make a pane appear in a maker's picker.
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return;
        }
        announce(mail);
    }

    /// WORKSHOP ASKING WHO HAS PANES. Answered only when Workshop actually asked.
    ///
    /// `authored_from_role` AND NOT `sender()`: the ask arrives as a PUBLICATION,
    /// so it reaches every weave that accepts the shape and there is no addressing
    /// to read intent from. What says it was Workshop is Loom's stamp on the
    /// authorship, which no payload can write and no sender can choose.
    void on(const PaneCatalogRequested&, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return;
        }
        announce(mail);
    }

    /// WORKSHOP GRANTING THIS PANE ITS PROSE BUDGET -- and the ONLY beat on which
    /// this tool observes anything.
    ///
    /// THE ROOM IS KEPT AND THE QUESTION IS ASKED; the content is sent later, when
    /// the Manager answers. The two are separate deliveries because the enumeration
    /// is a conversation and not a call: Loom has no synchronous way for a
    /// participant to read the kernel's map, and pretending otherwise would mean
    /// answering this grant with rows read before it.
    ///
    /// A ROOM IS NEVER ANSWERED FROM A PREVIOUS READING. Workshop clears its cache
    /// before every grant, so a stale projection sent now would be presented as an
    /// answer to THIS room; the honest gap is `(waiting for the provider)`, which is
    /// Workshop's own word for exactly this state and needs nothing from here.
    void on(const PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return; // a forged room grants nothing and produces no content
        }
        if (room.pane != kLoadedPane) {
            return; // a room for a pane this provider does not have
        }
        ++state_.rooms;
        rows_ = room.rows;
        columns_ = room.columns;
        ask(mail);
    }

    /// THE MANAGER'S ANSWER, and the moment this pane learns anything.
    ///
    /// MATCHED ON THE CORRELATION THIS WEAVE MINTED, and on there being a question
    /// outstanding at all. That is what an asker can check here and it is the whole
    /// of it: the Manager relays an answer PERSONALLY, so there is no authored role
    /// on it to verify, and `send_to_role` never told this weave which incarnation
    /// its question resolved to, so there is no expected sender either.
    ///
    /// SO THE BOUND IS STATED RATHER THAN OVERSOLD: any weave able to send
    /// `zen.Result` here, carrying a correlation this weave minted and has not yet
    /// retired, could supply these rows. The correlation is private to this
    /// incarnation and is never published, and the population that could try is
    /// every dynamic weave in this process -- each of which already holds
    /// `allow_any()` from the loader. That is the process tier's problem and not a
    /// claim this seam makes.
    void on(const loom::Result& r, loom::Mail& mail) {
        if (!awaiting_ || mail.correlation() != pending_) {
            return; // an answer to a question this weave did not ask
        }
        awaiting_ = false;
        ++state_.readings;
        PaneContent said;
        said.pane = kLoadedPane;
        said.rows = zengine::introspection::project_loaded(
            zengine::introspection::parse_loaded(r.value), rows_, columns_);
        // DELIBERATELY AS THIS OFFICE. `mail.send_to_role(...)` would be PERSONAL
        // speech from a weave that happens to hold the office, and Workshop drops it
        // -- holding is never speaking-for (MSG-07).
        (void)mail.as_role(kIntrospectionRole).send_to_role(kWorkshopRole, said);
    }

    /// THE MANAGER DECLINING TO ANSWER. `zen.ListLoaded` is not a shape the door
    /// refuses, so this arm exists for the reach itself failing -- and its answer is
    /// to send NOTHING, which leaves Workshop showing `(waiting for the provider)`.
    ///
    /// A REFUSAL IS NOT MADE INTO A ROW. Rendering "the Manager said no" inside a
    /// list headed `loaded weaves` would put a sentence about this tool's plumbing
    /// where a maker reads facts about their system, and the pane already has an
    /// honest word for having nothing to show.
    /// It RETIRES THE QUESTION, which is the only state it owns. Leaving `awaiting_`
    /// standing would make a later `zen.Result` bearing this same correlation -- a
    /// number this incarnation will not mint twice, so one that could only be
    /// somebody else's -- look like the answer to a question that was already closed.
    void on(const loom::Refused&, loom::Mail& mail) {
        if (awaiting_ && mail.correlation() == pending_) {
            awaiting_ = false;
        }
    }

private:
    /// One offer, authored as this office and addressed to the Workshop office.
    ///
    /// DIRECTED RATHER THAN PUBLISHED. Workshop is the only party this concerns, and
    /// a broadcast catalog entry would be an announcement to a room that did not ask.
    void announce(loom::Mail& mail) {
        ++state_.offers;
        (void)mail.as_role(kIntrospectionRole)
            .send_to_role(kWorkshopRole,
                          PaneOffered{kLoadedPane, kLoadedPaneName, kLoadedPaneSummary});
    }

    /// ASK THE WEAVE MANAGER WHAT IS LOADED.
    ///
    /// BY ROLE, never by `WeaveId`: `zen.manager` is the address that survives its
    /// holder being replaced, and this weave never learns an id for it anyway.
    ///
    /// ONE QUESTION OUTSTANDING AT A TIME. A second grant arriving before the first
    /// answer replaces the correlation rather than queueing a second question --
    /// there is only ever one room in force, and an answer to the room before it
    /// would be projected against a budget that is no longer granted.
    void ask(loom::Mail& mail) {
        pending_ = ++asked_;
        awaiting_ = true;
        (void)mail.send_to_role(loom::kManagerRole, loom::ListLoaded{}, pending_);
    }

    zengine::ActivationCursor activation_;
    /// THE LAST ROOM GRANTED, and it is NOT state. A snapshot that carried it would
    /// revive an incarnation believing it holds a grant Workshop's own cache says it
    /// does not; the grant is re-sent whenever a valid offer refreshes the pane, so
    /// forgetting it across a revive is both correct and self-repairing.
    std::int64_t rows_ = 0;
    std::int64_t columns_ = 0;
    std::uint64_t asked_ = 0;   ///< this incarnation's correlation counter
    std::uint64_t pending_ = 0; ///< the outstanding question, if any
    bool awaiting_ = false;
};

} // namespace

ZEN_EXPORT_WEAVE(IntrospectionWeave)
