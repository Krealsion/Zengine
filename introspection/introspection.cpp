// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Introspection provider — a loadable weave that offers Workshop three
// read-only panes showing what the system it is running in actually is.
//
// IT IS A TOOL AND NOT A FIXTURE. `tests/weavelib/workshop_hello.cpp` proved the
// pane seam works through the real ABI and says of itself that it is built by
// `tests/` and named in no host's boot list; this one is built beside the Timer,
// booted by `zengine-workshop` beside the Skin and the reader, and its rows are
// facts about the running system rather than an echo of the room it was granted.
//
// ---- THREE QUESTIONS, THREE OWNERS, THREE PANES (INTR-1) --------------------
//
// INTR-0 shipped the first. INTR-1 added two, and did NOT widen the first to hold
// them -- because the three have different populations and different authorities,
// and one merged table would have had to invent a row kind that is none of them.
//
//     loaded        which WEAVES this Loom's Kernel has loaded, and each one's
//                   role.        OWNER: the Kernel's `loaded()` map.
//     arrangement   which AUTHORED ARTIFACT PARTICIPATIONS this project asked
//                   for, and what resolved from each.
//                                OWNER: LOAD-0's `PlanExecutor` rows + the plan.
//     powers        which OPERATOR POWERS this host currently resolves, and whose
//                   contribution satisfies each.
//                                OWNER: PROV-0's live `op::Catalog`.
//
// THEY DISAGREE ON PURPOSE, and the disagreement is the clearest evidence any of
// them is honest. `zengine-operators-basic` is a provider and not a weave: it is a
// row of `arrangement` and it is absent from `loaded`, because no Kernel loaded it
// and it has no `WeaveId` and no role. A build in which both panes listed it would
// be a build in which one of them had started guessing.
//
// ---- WHAT IT KNOWS, AND HOW IT COMES TO KNOW IT -----------------------------
//
//     the facts     the three above, and NOT ONE OF THEM IS KEPT HERE. Every one
//                   is asked for, spent building rows, and dropped
//     the paths     zen.ListLoaded -> the Weave Manager -> zen.ListLibraries ->
//                   the control door -> the Kernel's live map -> zen.Result
//                   ArrangementRequested / PowersRequested -> zengine.arrangement
//                   -> the host's own executor rows and catalog -> an ANSWER
//     currency      a SNAPSHOT, re-read on every room grant. Loom offers a
//                   participant no arrival or departure event and there is no
//                   provider-mount event either, so there is nothing to subscribe
//                   to and nothing here polls. The powers pane is the one whose
//                   subject can change mid-run: an overlay mounted since the last
//                   grant is in the next reading, with nobody having been told
//     absence       an empty map is an observed zero; a question that has not
//                   been answered yet produces NO content at all, so Workshop's
//                   own `(waiting for the provider)` says it rather than a zero
//
// ---- THE SECOND SEAM, AND WHY IT LOOKS LIKE THE FIRST -----------------------
//
// The arrangement and the powers live in the HOST's `main` -- a `LoadPlan`, a
// `PlanExecutor` and an `op::Catalog`, none of which may cross into a dynamically
// loaded image as anything but a value. So this weave asks the same way it already
// asks the Kernel: a shape to an OFFICE, an answer back, values only. What is new
// is that the answer arrives through Loom's own ANSWER door, so `mail.answers_ask()`
// is Loom attesting that this delivery answers a question THIS weave asked -- a
// stronger bound than the correlation match `zen.ListLoaded` has to make do with,
// and it costs nothing.
//
// ---- AND SINCE SEL-0 IT KNOWS ONE MORE THING, WHICH IS NOT A FACT ABOUT THE LOOM
//
//     the gesture   which of the entries IT IS CURRENTLY SHOWING a maker pressed.
//                   That is not an observation of the running system -- it is a
//                   fact about this pane, owned by this pane, and it is the only
//                   thing in here whose author is a person
//     it publishes  `LoadedSelected`, an ordinary Loom message stating what was
//                   selected, and then stops. Nothing in this build listens, no
//                   listener is required to exist, and this weave neither knows
//                   nor asks whether one did
//     it decides    nothing else. It opens no pane, addresses no weave, sends the
//                   selected library nothing, and holds no policy about what a
//                   selection ought to cause. SELECTION IS A FACT, NOT A COMMAND
//
// ---- WHAT IT CANNOT DO ------------------------------------------------------
//
// It writes no file, starts no process, opens no socket, holds no timer, reads no
// Sense, publishes no canvas, and commands no lifecycle. Its whole outbound
// vocabulary is SIX shapes: two sentences of the pane protocol, the one question
// `zen.ListLoaded` -- which is the enumeration half of the control vocabulary and
// NOT the load capability -- its own `LoadedSelected`, and INTR-1's two questions
// `ArrangementRequested` and `PowersRequested`. `zen.LoadWeave`, `zen.SwapWeave`,
// `zen.ReloadWeave`, `zen.UnloadLibrary` and `zen.UnloadRole` are
// absent from its Emit set and from every grant a host would write for it, and a
// grant is per (shape, version, target), so being able to ask what is loaded is
// not being able to load anything -- and being able to say which one a maker
// pointed at is not being able to reach it.
//
// AND THE TWO NEW QUESTIONS ARE THE SHARPEST CASE OF THAT RULE IN THIS FILE. This
// weave can now name every provider mounted in the process and say which
// contribution currently satisfies each power. It cannot mount one, unmount one,
// overlay one, evaluate one, or ask anybody else to: there is no shape for any of
// it in either direction, the answers are inert values, and a value arriving in a
// message has never been a grant. KNOWLEDGE OF A POWER IS NOT AUTHORITY TO
// REPLACE IT.
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
#include "resolved.hpp"
#include "vocabulary.hpp"

#include "activation/activation.hpp"
#include "surface/vocabulary.hpp"
#include "workshop/arrangement_vocabulary.hpp"
#include "workshop/pane_vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace surface = zengine::surface;
namespace intro = zengine::introspection;
using zengine::introspection::kArrangementPane;
using zengine::introspection::kArrangementPaneName;
using zengine::introspection::kArrangementPaneSummary;
using zengine::introspection::kIntrospectionRole;
using zengine::introspection::kLoadedPane;
using zengine::introspection::kLoadedPaneName;
using zengine::introspection::kLoadedPaneSummary;
using zengine::introspection::kPowersPane;
using zengine::introspection::kPowersPaneName;
using zengine::introspection::kPowersPaneSummary;
using zengine::introspection::LoadedSelected;
using zengine::introspection::LoadedView;
using zengine::introspection::LoadedWeave;
using zengine::workshop::ArrangementRequested;
using zengine::workshop::kArrangementRole;
using zengine::workshop::PaneCatalogRequested;
using zengine::workshop::PaneContent;
using zengine::workshop::PaneOffered;
using zengine::workshop::PanePressed;
using zengine::workshop::PaneRoom;
using zengine::workshop::PowersRequested;
using zengine::workshop::ResolvedArrangement;
using zengine::workshop::ResolvedPowers;

/// The office Workshop holds, named as a STRING rather than reached through
/// `workshop/panel.hpp`. A provider is a stranger to Workshop's internals and must
/// be able to say who it is talking to the way a third party would.
constexpr const char* kWorkshopRole = "zengine.workshop";

/// WHAT THIS PROVIDER HAS DONE, and it is all counters.
///
/// NO INVENTORY IS IN HERE, AND SINCE INTR-1 NO ARRANGEMENT AND NO POWER EITHER. The
/// loaded map belongs to the Kernel, the resolved rows belong to the host's executor
/// and the contribution stacks belong to the host's catalog; keeping a copy beside
/// any of them would be a second answer to a question that already has one, and the
/// copy is the one that goes stale. What survives a snapshot and a revive is only
/// the bookkeeping -- how many times this office spoke, was granted room, answered,
/// refused to answer somebody who was not Workshop, and said that a maker selected
/// something.
///
/// THE SHAPE DID NOT MOVE FOR THE TWO NEW PANES, and that is worth reading as a
/// measurement rather than as tidiness: two more questions and two more answers
/// needed no new durable state, because a view that keeps nothing has nothing to
/// snapshot. `readings` counts every answer that became content, whichever question
/// it answered.
///
/// AND NO SELECTION IS IN HERE EITHER, WHICH IS A DIFFERENT DECISION FROM THE FIRST.
/// The selection is transient UI state belonging to the projection currently on
/// screen -- it lives in a private member below, is not in the state shape, and is
/// therefore not snapshotted, not revived, not persisted and not in any saved setup.
/// A revived incarnation has been granted no room, is showing nothing, and has no
/// pane a selection could be OF; carrying one across would restore a mark against a
/// projection that no longer exists.
struct IntrospectionState {
    std::int64_t offers = 0;
    std::int64_t rooms = 0;
    std::int64_t readings = 0;   ///< answers that became content, from any of the three owners
    std::int64_t refused = 0;    ///< asks, rooms and presses not authored by the Workshop office
    std::int64_t selections = 0; ///< maker selections published as `LoadedSelected`
    ZEN_EXPOSE();
    ZEN_SHAPE(IntrospectionState, 2, ZEN_FIELD(offers), ZEN_FIELD(rooms), ZEN_FIELD(readings),
              ZEN_FIELD(refused), ZEN_FIELD(selections));
};

class IntrospectionWeave
    : public loom::WeaveBase<
          IntrospectionWeave, IntrospectionState,
          loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom, PanePressed, loom::Result,
                       loom::Refused, ResolvedArrangement, ResolvedPowers>,
          loom::Emit<PaneOffered, PaneContent, LoadedSelected, loom::ListLoaded,
                     ArrangementRequested, PowersRequested>> {
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

    /// WORKSHOP GRANTING ONE OF THESE PANES ITS PROSE BUDGET -- and the ONLY beat on
    /// which this tool observes anything.
    ///
    /// THREE PANES, THREE ROOMS, THREE QUESTIONS, AND EACH KEEPS ITS OWN (INTR-1). A
    /// maker may have all three open at once and Workshop resolves a body budget for
    /// each separately, so one shared `rows_`/`columns_` would have made the last grant
    /// decide how the other two were projected -- an answer measured against a room
    /// nobody granted it. `Asked` is that pair plus the question outstanding for it.
    ///
    /// THE ROOM IS KEPT AND THE QUESTION IS ASKED; the content is sent later, when
    /// the owner answers. The two are separate deliveries because an enumeration
    /// is a conversation and not a call: Loom has no synchronous way for a
    /// participant to read the kernel's map or the host's catalog, and pretending
    /// otherwise would mean answering this grant with rows read before it.
    ///
    /// A ROOM IS NEVER ANSWERED FROM A PREVIOUS READING. Workshop clears its cache
    /// before every grant, so a stale projection sent now would be presented as an
    /// answer to THIS room; the honest gap is `(waiting for the provider)`, which is
    /// Workshop's own word for exactly this state and needs nothing from here.
    ///
    /// AND THE PROJECTION IS DROPPED HERE, WHICH IS WHAT KEEPS A PRESS HONEST (SEL-0).
    /// From this line until the Manager's answer arrives, Workshop is showing
    /// `(waiting for the provider)` -- there are no entry rows on the screen, so there
    /// is no row a press could truthfully name, so this pane holds no map to name one
    /// with. Keeping the previous view across a grant would let a press in that gap be
    /// read against a projection the maker is no longer looking at, which is the exact
    /// mismatch the snapshot discipline exists to prevent.
    ///
    /// THE SELECTED IDENTITY IS NOT DROPPED WITH IT. A selection is held as a library
    /// NAME rather than a row, so a resize that windows an entry out, or that has not
    /// been answered yet, loses the MARK and keeps the FACT -- and the mark returns
    /// with the entry, with no gesture, the moment a reading shows it again.
    void on(const PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return; // a forged room grants nothing and produces no content
        }
        if (room.pane == kLoadedPane) {
            ++state_.rooms;
            view_ = LoadedView{};
            ask(mail, loaded_, room, loom::kManagerRole, loom::ListLoaded{});
        } else if (room.pane == kArrangementPane) {
            ++state_.rooms;
            ask(mail, arrangement_, room, kArrangementRole, ArrangementRequested{});
        } else if (room.pane == kPowersPane) {
            ++state_.rooms;
            ask(mail, powers_, room, kArrangementRole, PowersRequested{});
        }
        // ...and a room for a pane this provider does not have is neither counted
        // nor answered. It cannot arrive from Workshop, which grants a room only for
        // a `PaneRef` it admitted from an offer; what it would be is somebody else's
        // pane key wearing this office's stamp.
    }

    /// A MAKER PRESSED A ROW OF THIS PANE -- and this is where a gesture becomes a fact.
    ///
    /// THE PRESS IS READ AGAINST THE PROJECTION CURRENTLY ON SCREEN, and nothing else
    /// happens first: no `zen.ListLoaded`, no re-parse, no re-project. That is the load
    /// bearing sentence of the whole phase. Asking the Manager again here would resolve
    /// the maker's row against a population that may have changed since the rows were
    /// drawn, and the maker would select something they were never shown -- silently,
    /// and only sometimes, which is the worst way for it to be wrong.
    ///
    /// WORKSHOP SENT A ROW OF THE ROOM IT GRANTED, so the interpretation is one lookup
    /// (`entry_at_row`) and the vocabulary that lookup uses -- heading, entry, caveat,
    /// omission marker, source line, blank -- exists nowhere but here. Workshop has
    /// none of those words and was never told any of them.
    ///
    /// A ROW THAT NAMES NO ENTRY SELECTS NOTHING AND SAYS NOTHING, and it does not
    /// clear the selection either: pressing a heading is not a deselection gesture, and
    /// inventing one out of a miss would make an unsteady hand destroy a maker's state.
    ///
    /// THE SAME ROW PRESSED TWICE PUBLISHES TWICE. `selected(entry)` is an OCCURRENCE
    /// derived from a gesture and not a `changed(old, new)` transition: a maker who
    /// presses the same weave again has selected it again, and a future trigger reading
    /// "whenever the maker selects this one" is owed both. What does NOT repeat is the
    /// picture -- the mark is already where it belongs, so no content is re-sent and no
    /// frame is republished. Two questions, two answers.
    void on(const PanePressed& press, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return; // a forged press selects nothing and publishes nothing
        }
        if (press.pane != kLoadedPane) {
            // A PRESS IN A PANE THIS OFFICE HAS NO GESTURE FOR (INTR-1). The other two
            // are read-only projections: they carry no selection, publish nothing, and
            // hold no row map a press could be resolved against. Pressing one is
            // consumed by Workshop as any press in a pane's room is, and produces no
            // sentence here -- which is `PanePressed`'s own contract, not a decline.
            return;
        }
        const LoadedWeave* entry = zengine::introspection::entry_at_row(view_, press.row);
        if (entry == nullptr) {
            return; // a heading, a note, an omission marker, a blank, or no projection at all
        }
        const bool moved = selected_ != entry->name;
        selected_ = entry->name;
        const std::string role = entry->role;
        if (moved) {
            zengine::introspection::mark_selected(view_, selected_, loaded_.columns);
            say_rows(mail, kLoadedPane, view_.rows);
        }
        ++state_.selections;
        // PUBLISHED, NOT ADDRESSED, and the difference is the phase's whole posture. A
        // directed send would require this pane to know who ought to care, which is a
        // decision about workflow that nothing here is entitled to make. A publication
        // states the fact into the room and reaches every weave that accepts the shape
        // -- today, none. AS THIS OFFICE, for the pane protocol's reason unchanged:
        // personal speech carries no verifiable author, and a fact about a maker's
        // gesture in this pane is worth exactly as much as the office it came from.
        (void)mail.as_role(kIntrospectionRole)
            .publish(LoadedSelected{kLoadedPane, selected_, role});
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
    ///
    /// A SELECTION IS CLEARED HERE AND ONLY HERE, WHEN THE ABSENCE IS ACTUALLY OBSERVED
    /// (SEL-0). This reading is the first moment this pane can know that a previously
    /// selected library is no longer loaded -- Loom offers no departure event, so
    /// nothing before now was evidence of anything. The question is asked of the whole
    /// POPULATION this answer carries, not of the rows it will fit: an entry windowed
    /// out of a short pane is present and merely unshown, and clearing a maker's
    /// selection because they shrank a panel would be a fiction about the system.
    ///
    /// CLEARING PUBLISHES NOTHING. `LoadedSelected` records maker gestures, and a
    /// library going away is not one; a listener that needs to hear about departures is
    /// asking for an arrival/departure event this Loom does not have, and answering it
    /// with an inference drawn from two snapshots would be exactly the story this file
    /// refuses to tell.
    void on(const loom::Result& r, loom::Mail& mail) {
        if (!loaded_.awaiting || mail.correlation() != loaded_.pending) {
            return; // an answer to a question this weave did not ask
        }
        loaded_.awaiting = false;
        ++state_.readings;
        const std::vector<LoadedWeave> read = zengine::introspection::parse_loaded(r.value);
        if (!zengine::introspection::names(read, selected_)) {
            selected_.clear();
        }
        view_ = zengine::introspection::project_loaded(read, loaded_.rows, loaded_.columns);
        zengine::introspection::mark_selected(view_, selected_, loaded_.columns);
        say_rows(mail, kLoadedPane, view_.rows);
    }

    /// THE HOST'S ANSWER ABOUT ITS OWN ARRANGEMENT (INTR-1).
    ///
    /// TWO CHECKS, AND THE FIRST ONE IS LOOM'S. `answers_ask()` is delivery provenance
    /// the bus attached because this delivery is THE one authorized answer to a request
    /// this weave sent -- no payload can write it, no sender can choose it, and the door
    /// could not have aimed it anywhere else. That is strictly stronger than what the
    /// `zen.Result` arm above can check, and the difference is not this tool being more
    /// careful: the Manager RELAYS its answer personally, while the arrangement door
    /// ANSWERS through Loom's own door. Where the stronger bound exists it is taken.
    ///
    /// THE CORRELATION IS STILL COMPARED, because it says WHICH grant is being answered.
    /// A second room granted before the first answer arrives replaces the correlation,
    /// so a late answer to the room before it is dropped rather than projected against a
    /// budget that is no longer in force.
    ///
    /// THE ANSWER IS SPENT AND DROPPED. Nothing below retains a row of it: the rows go
    /// to Workshop and the value goes out of scope, which is what makes the claim "no
    /// second mutable registry" a fact about this file rather than a promise about it.
    void on(const ResolvedArrangement& said, loom::Mail& mail) {
        if (!answering(mail, arrangement_)) {
            return;
        }
        ++state_.readings;
        say_rows(mail, kArrangementPane,
                 intro::project_arrangement(said, arrangement_.rows, arrangement_.columns));
    }

    /// THE HOST'S ANSWER ABOUT ITS OWN POWERS (INTR-1). `ResolvedArrangement`'s two
    /// checks, for the same two reasons.
    ///
    /// THIS IS THE PANE WHOSE SUBJECT MOVES. An arrangement is settled at startup in
    /// this build; a power's active contribution changes the moment a provider is
    /// mounted over it. Nothing here notices that happen and nothing here is told:
    /// the next room grant asks again, and the answer is whatever the catalog resolves
    /// at that moment.
    void on(const ResolvedPowers& said, loom::Mail& mail) {
        if (!answering(mail, powers_)) {
            return;
        }
        ++state_.readings;
        say_rows(mail, kPowersPane,
                 intro::project_powers(said, powers_.rows, powers_.columns));
    }

    /// AN OWNER DECLINING TO ANSWER. `zen.ListLoaded` is not a shape the control door
    /// refuses, so this arm exists for the reach itself failing -- and its answer is
    /// to send NOTHING, which leaves Workshop showing `(waiting for the provider)`.
    ///
    /// SINCE INTR-1 IT IS THE ARM FOR A HOST WITH NO ARRANGEMENT DOOR TOO, and that is
    /// a real arrangement rather than a fault: a host that mounts no door holds no
    /// `zengine.arrangement` office, an ask addressed to it reaches nobody, and the two
    /// new panes wait. This tool is loadable into any Loom host; only one in this
    /// repository answers for its own project.
    ///
    /// A REFUSAL IS NOT MADE INTO A ROW. Rendering "the Manager said no" inside a
    /// list headed `loaded weaves` would put a sentence about this tool's plumbing
    /// where a maker reads facts about their system, and the pane already has an
    /// honest word for having nothing to show.
    /// It RETIRES THE QUESTION, which is the only state it owns. Leaving `awaiting`
    /// standing would make a later answer bearing this same correlation -- a
    /// number this incarnation will not mint twice, so one that could only be
    /// somebody else's -- look like the answer to a question that was already closed.
    ///
    /// ALL THREE ARE OFFERED THE CORRELATION AND AT MOST ONE MATCHES, because one
    /// counter mints every question this incarnation asks. A refusal cannot retire the
    /// wrong pane's question by accident.
    void on(const loom::Refused&, loom::Mail& mail) {
        for (Asked* q : {&loaded_, &arrangement_, &powers_}) {
            if (q->awaiting && mail.correlation() == q->pending) {
                q->awaiting = false;
            }
        }
    }

private:
    /// ONE ROOM AND ONE OUTSTANDING QUESTION, FOR ONE PANE (INTR-1).
    ///
    /// NONE OF IT IS STATE. A snapshot that carried a room would revive an incarnation
    /// believing it holds a grant Workshop's own cache says it does not; the grant is
    /// re-sent whenever a valid offer refreshes the pane, so forgetting all three
    /// across a revive is both correct and self-repairing.
    struct Asked {
        std::int64_t rows = 0;
        std::int64_t columns = 0;
        std::uint64_t pending = 0; ///< the outstanding question for this pane, if any
        bool awaiting = false;
    };

    /// EVERY PANE THIS OFFICE HAS, OFFERED IN ONE BREATH.
    ///
    /// DIRECTED RATHER THAN PUBLISHED. Workshop is the only party this concerns, and
    /// a broadcast catalog entry would be an announcement to a room that did not ask.
    ///
    /// THREE SENTENCES AND NOT ONE WITH A LIST, because `PaneOffered` names ONE pane
    /// and a provider offering three is three offers. Workshop admits each on its own
    /// terms -- its own key law, its own catalog row, its own runtime kind -- so an
    /// offer this build gets wrong costs one pane rather than all of them.
    void announce(loom::Mail& mail) {
        offer(mail, PaneOffered{kLoadedPane, kLoadedPaneName, kLoadedPaneSummary});
        offer(mail, PaneOffered{kArrangementPane, kArrangementPaneName,
                                kArrangementPaneSummary});
        offer(mail, PaneOffered{kPowersPane, kPowersPaneName, kPowersPaneSummary});
    }

    void offer(loom::Mail& mail, const PaneOffered& said) {
        ++state_.offers;
        (void)mail.as_role(kIntrospectionRole).send_to_role(kWorkshopRole, said);
    }

    /// ASK ONE PANE'S OWNER ITS ONE QUESTION, AND REMEMBER THE ROOM IT IS FOR.
    ///
    /// BY ROLE, never by `WeaveId`: `zen.manager` and `zengine.arrangement` are the
    /// addresses that survive their holders being replaced, and this weave never
    /// learns an id for either.
    ///
    /// ONE QUESTION OUTSTANDING PER PANE. A second grant for the same pane arriving
    /// before its first answer replaces that pane's correlation rather than queueing a
    /// second question -- there is only ever one room in force for one pane, and an
    /// answer to the room before it would be projected against a budget that is no
    /// longer granted. The three panes do not share the counter's VALUES, only the
    /// counter, so no pane can retire another's question.
    ///
    /// AUTHORED AS THIS OFFICE. The Manager does not check and the arrangement door
    /// does: a host answering "what did this project resolve" answers an OFFICE, so
    /// that every answer it ever gave went somewhere nameable (workshop/arrangement.hpp
    /// says exactly what that bound is and is not). Asking as this office costs
    /// nothing and is the same deliberate authorship every pane sentence here uses.
    template <class Question>
    void ask(loom::Mail& mail, Asked& pane, const PaneRoom& room, const char* owner,
             const Question& question) {
        pane.rows = room.rows;
        pane.columns = room.columns;
        pane.pending = ++asked_;
        pane.awaiting = true;
        (void)mail.as_role(kIntrospectionRole).send_to_role(owner, question, pane.pending);
    }

    /// IS THIS DELIVERY THE ANSWER TO THE QUESTION THIS PANE IS WAITING ON?
    ///
    /// `answers_ask()` FIRST, because it is the one an asker cannot forge and a
    /// stranger cannot supply: Loom stamps it on the single authorized answer to a
    /// request this weave sent. The correlation then says WHICH grant it answers.
    bool answering(loom::Mail& mail, Asked& pane) {
        if (!pane.awaiting || !mail.answers_ask() || mail.correlation() != pane.pending) {
            return false;
        }
        pane.awaiting = false;
        return true;
    }

    /// SAY WHAT ONE PANE NOW SHOWS -- the one place content leaves this weave.
    ///
    /// DELIBERATELY AS THIS OFFICE. `mail.send_to_role(...)` would be PERSONAL speech
    /// from a weave that happens to hold the office, and Workshop drops it -- holding
    /// is never speaking-for (MSG-07).
    ///
    /// ONE FUNCTION SINCE SEL-0 AND STILL ONE SINCE INTR-1, because there are now four
    /// occasions -- a fresh reading of any of three panes, and a moved mark -- and they
    /// must be the same sentence. A second spelling would eventually differ in which
    /// office it authored as, and the symptom would be a pane that updates when the
    /// kernel changes and goes silent when a maker clicks.
    void say_rows(loom::Mail& mail, const char* pane,
                  std::vector<surface::SurfaceTextRow> rows) {
        PaneContent said;
        said.pane = pane;
        said.rows = std::move(rows);
        (void)mail.as_role(kIntrospectionRole).send_to_role(kWorkshopRole, said);
    }

    zengine::ActivationCursor activation_;
    std::uint64_t asked_ = 0; ///< this incarnation's correlation counter, for all three panes
    Asked loaded_;
    Asked arrangement_;
    Asked powers_;
    /// WHAT THIS PANE IS CURRENTLY SHOWING, and the map from its rows back to the
    /// entries they name (SEL-0).
    ///
    /// IT IS THE PRESENTATION, NOT AN INVENTORY, and loaded.hpp's header states the
    /// difference at length because this member is the one most likely to be misread
    /// as the thing INTR-0 refused to keep. It answers "which entry did the maker
    /// press" and cannot answer "what is loaded now": it holds only entries that
    /// reached a row, so it is bounded by the granted room rather than by the
    /// population; it is emptied by every room grant; and it is replaced whole by the
    /// next reading. Nothing consults it to decide what is true about the Loom.
    LoadedView view_;
    /// THE ENTRY THE MAKER SELECTED, as the library NAME the row showed.
    ///
    /// A NAME AND NOT A ROW INDEX, which is what makes selection survive a resize: a
    /// row number is a fact about a projection and stops meaning anything the moment
    /// the window moves, while a name is a fact about the thing itself. Empty means
    /// none, which is where every incarnation starts.
    ///
    /// TRANSIENT AND LOCAL. It is not in `IntrospectionState`, so it is not
    /// snapshotted or revived; it is in no setup file, no document and no Workshop
    /// field; and there is no ambient "currently selected weave" anywhere in this
    /// process. This pane owns its own selection and nothing else has one.
    std::string selected_;
};

} // namespace

ZEN_EXPORT_WEAVE(IntrospectionWeave)
