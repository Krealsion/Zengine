// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_ARRANGEMENT_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_ARRANGEMENT_VOCABULARY_HPP

// ASKING A HOST WHAT IT RESOLVED (INTR-1). Two questions, two answers, one office.
//
//     ArrangementRequested   asker    -> the door   "what did this project ask for,
//                                                    and what came of it?"
//     ResolvedArrangement    the door -> asker      the authored rows, paired with
//                                                    what the executor made of them
//
//     PowersRequested        asker    -> the door   "which operator powers resolve
//                                                    here, and whose code satisfies
//                                                    each one?"
//     ResolvedPowers         the door -> asker      every identity the catalog
//                                                    resolves, with its whole stack
//
// ---- WHY ONE OFFICE ANSWERS BOTH ---------------------------------------------
//
// They are two readings of ONE act. The plan mounted the providers whose
// contributions the catalog now layers, and the artifact that supplied a power is a
// row of the arrangement -- so a second office would be a second door onto one desk,
// and the two doors would eventually be opened against different moments.
//
// They are NOT one answer, and that is the other half of the same decision. The
// populations differ (authored artifacts vs. logical powers), the owners differ
// (`load::PlanExecutor`'s rows vs. `op::Catalog`'s store), and the currencies differ
// -- an arrangement is settled at startup in this build, while a power's active
// contribution changes the moment a provider is mounted over it. One shape carrying
// both would make a reader who wanted one pay for the other and would put two
// different freshnesses behind one field.
//
// ---- WHAT CROSSES, AND WHAT CANNOT --------------------------------------------
//
// VALUES. Every field below is Text, Int, Bool or a List of those -- the ordinary
// Loom wire, admitted at the reader's own schema. No `op::Catalog`, no
// `load::PlanExecutor`, no `ResolvedArtifact`, no `Contribution`, no
// `std::shared_ptr`, no `OperatorDef`, no index into anybody's store, and no host
// address of any kind. The C++ objects that own these facts stay where they are; a
// reader gets a picture of them and can do nothing to them.
//
// AND THE PICTURE CONFERS NOTHING. A weave that reads `zengine.operators.basic
// supplies math.max` has learned two strings. It has NOT thereby been permitted to
// mount, unmount, overlay, evaluate, load, unload or replace anything: a grant in
// this Loom is per `(shape, version, target)`, and a value arriving in a message is
// not one and can never become one. KNOWLEDGE OF A POWER IS NOT AUTHORITY TO REPLACE
// IT -- the rule `LoadedSelected` already lives under (introspection/vocabulary.hpp),
// one seam further in, where the temptation is strongest because a reference to a
// powerful thing looks like a handle on it.
//
// ---- AUTHORED AND RESOLVED ARE DIFFERENT TRUTHS, AND THE SHAPE SAYS WHICH -------
//
// `ArtifactParticipation` carries both, in fields named for which kind each is. The
// authored half is what a person wrote in the plan file and would be true again
// tomorrow; the resolved half is what this run's executor and this run's Kernel made
// of it and would be a lie tomorrow. A shape that carried a resolved provider
// identity in a field called `provider` beside an authored role in a field called
// `role` would be inviting exactly the confusion LOAD-0's two lists were removed for.
//
// ---- WHAT IS DELIBERATELY NOT HERE ---------------------------------------------
//
// No mutation shape of any kind: nothing here says mount, unmount, overlay, load,
// unload, reload, enable, disable or replace, and there is no room to add one
// without a new name a host would have to grant. No metadata bag -- these are
// structural runtime truths and `metadata["active_provider"]` would be a second,
// weaker spelling of a field that already exists. No history, no arrival or
// departure event, no timestamp and no clock: the answer is what is true when it is
// asked, and a reader that wants to know whether it changed asks again.
//
// No schema, content id, port list or ABI number rides along either. `OperatorDef`
// has all of them; a browser for them is a different tool, and putting them here
// would make every reader of "who supplies this" carry an operator documentation
// surface.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// THE OFFICE THE HOST'S OBSERVATION DOOR HOLDS -- the only address anything reaches
/// it by, and a ROLE for `kIntrospectionRole`'s reason: it survives its holder being
/// replaced, and a loaded artifact can name it without ever learning a `WeaveId`.
///
/// A HOST THAT MOUNTS NO DOOR HOLDS NO SUCH OFFICE, and an ask sent to it then
/// reaches nobody. That is the correct answer rather than a hole: `zengine-snake`
/// has no authored arrangement to describe and owes no answer about one.
inline constexpr const char* kArrangementRole = "zengine.arrangement";

/// ASK WHAT THIS HOST'S AUTHORED PROJECT ASKED FOR, AND WHAT CAME OF IT.
///
/// It carries nothing. A filter field would be a policy about which rows an asker
/// may see, decided here, by nobody who was asked for one -- and a view that wants
/// fewer rows already has the whole answer to choose from.
struct ArrangementRequested {
    ZEN_SHAPE(ArrangementRequested, 1);
};

/// ASK WHICH OPERATOR POWERS THIS HOST CURRENTLY RESOLVES, AND WHOSE CODE SATISFIES
/// EACH. Carries nothing, for `ArrangementRequested`'s reason exactly.
struct PowersRequested {
    ZEN_SHAPE(PowersRequested, 1);
};

// ---- The arrangement ----------------------------------------------------------

/// The tokens `ArtifactParticipation::offer` may carry -- `op::OfferOutcome`, said in
/// words a renumbering cannot move (`load_persist`'s argument about mode words, one
/// seam further out).
///
/// THEY ARE TOKENS AND NOT PROSE. A view prints them as they are, so an outcome a
/// later phase adds appears in every pane without one line of view source changing.
///
/// AND THE EMPTY ONE IS THE ONE MOST WORTH READING. `load::ResolvedArtifact::offer`
/// is `NotAConsumer` for every artifact with NO WEAVE INTENT -- because no offer was
/// ever made -- so a provider-only row that reported that token would be answering a
/// question nobody asked of it. The door writes the empty string there instead.
inline constexpr const char* kOfferNone = "";
inline constexpr const char* kOfferedToken = "offered";
inline constexpr const char* kNotAConsumerToken = "not-a-consumer";
inline constexpr const char* kVersionMismatchToken = "version-mismatch";
inline constexpr const char* kNotOpenedToken = "not-opened";

/// The tokens `ArtifactParticipation::state` may carry -- `load::RowState`, said in
/// words for `offer`'s reason exactly.
///
/// THEY EXIST BECAUSE REALIZATION IS LIVE NOW (BOOT-0). This field replaced a `bool
/// performed`, and the bool was not merely coarse: a row nobody had reached yet, a row
/// whose load was in flight at this instant, and a row that had been reached and
/// REFUSED were all `false`, so the one question a maker asks while a project is
/// coming up -- *is it still working, or did it stop?* -- had no answer in the wire at
/// all. It could not have had one before: nothing survived the executor's stack frame
/// long enough to be asked.
///
/// FIVE TOKENS, FIVE OWNERS, and the owner is why each one exists rather than the
/// word sounding useful; `load::RowState` names them and names the three that were
/// refused.
///
/// `pending` IS BLD-1's, AND IT IS A NEW TOKEN ON AN UNCHANGED SHAPE. `state` is a
/// STRING and always was, precisely so that the set of things realization can be doing
/// with a row could grow without every reader of this message having to be recompiled
/// against a new version of it. It means: this run reached the row, the host said it is
/// waiting on the maker, and nothing has been mounted, opened or commanded for it.
///
/// ⚠ IT IS NOT "the artifact is missing" AND IT IS NOT "a build is running". This
/// message says what REALIZATION has done, and what realization did is nothing --
/// deliberately, at the host's word. Whether a file is absent is the host's fact and
/// whether a build is under way is the Builder's; a reader that wants either asks their
/// owners, which is the same reason `loaded`, `arrangement` and `powers` are three
/// panes and not one table.
inline constexpr const char* kAuthoredToken = "authored";
inline constexpr const char* kPendingToken = "pending";
inline constexpr const char* kLoadingToken = "loading";
inline constexpr const char* kResolvedToken = "resolved";
inline constexpr const char* kRefusedToken = "refused";

/// ONE AUTHORED PROJECT PARTICIPANT, AND WHAT THIS RUN MADE OF IT.
///
/// ONE ROW PER ARTIFACT, WHATEVER IT PARTICIPATES AS -- which is LOAD-0's central
/// result carried into observation. `zengine-timer` supplies a power AND is loaded
/// as a weave; it is one authored record and it is one row here. Splitting it into a
/// provider row and a weave row would throw away the only place the two are known to
/// be the same artifact.
///
/// ---- WHICH FIELDS ARE AUTHORED -------------------------------------------------
///
///   `artifact`           the stem a person wrote down
///   `authored_provider`  empty | `normal` | `overlay` -- `load_persist`'s own words
///   `authored_role`      the office the plan asked this artifact to be loaded into,
///                        or empty for a record with no weave participation
///
/// ---- WHICH ARE RESOLVED --------------------------------------------------------
///
///   `state`              where realization has got with this row: `authored`,
///                        `loading`, `resolved` or `refused`
///   `provider`           the identity THE ARTIFACT DECLARED ABOUT ITSELF when it
///                        was mounted -- never the stem, and never what the plan said
///   `powers`             how many contributions that mount installed
///   `weave`              the `WeaveId` THIS Kernel minted THIS RUN. Zero means no
///                        weave was loaded, which is a fact and not an error
///   `offer`              how the operator handoff around this weave's load ended
///
/// ---- ONLY A `resolved` ROW CARRIES RESOLVED FIELDS -------------------------------
///
/// A row that is `loading` may already have mounted its provider -- within one record
/// the mount happens before the load -- and this shape says nothing about it. That is
/// deliberate: `provider`, `powers`, `weave` and `offer` answer *what came of this
/// authored row*, and what came of a row still in flight is not decided. If its load
/// refuses, that mount is rolled back and the identity would have named a
/// contribution that no longer exists.
///
/// THE POWER IS STILL VISIBLE, THROUGH THE QUESTION THAT OWNS IT. `ResolvedPowers`
/// reads the live catalog at the moment of the ask, so a contribution mounted by a
/// row that is still loading appears there immediately. Two questions, two owners,
/// two currencies -- which is the same reason a provider is a row of the arrangement
/// and is absent from the Loaded pane.
///
/// ---- WHY THERE IS NO RESOLVED ROLE ---------------------------------------------
///
/// Because nothing observed one. `load::ResolvedArtifact::role` is the AUTHORED role
/// copied forward -- the executor hands it to `zen.LoadWeave` and keeps it -- so a
/// field called `resolved_role` here would be the authored string wearing a resolved
/// name. The office the Kernel actually bound is in the Kernel's own map, which is
/// what `zen.ListLoaded` answers and what the Loaded pane already shows. Two panes,
/// two questions; a copy here would be a third answer with no third owner.
struct ArtifactParticipation {
    std::string artifact;
    std::string authored_provider;
    std::string authored_role;

    /// v2 (BOOT-0): this replaced `bool performed`. It is a REPLACEMENT and not an
    /// addition, because the two would have to agree and one of them would be the
    /// copy that goes stale -- `performed` is exactly `state == kResolvedToken`.
    std::string state = kAuthoredToken;
    std::string provider;
    std::int64_t powers = 0;
    std::int64_t weave = 0;
    std::string offer;

    ZEN_SHAPE(ArtifactParticipation, 2, ZEN_FIELD(artifact), ZEN_FIELD(authored_provider),
              ZEN_FIELD(authored_role), ZEN_FIELD(state), ZEN_FIELD(provider),
              ZEN_FIELD(powers), ZEN_FIELD(weave), ZEN_FIELD(offer));
};

/// WHAT THIS PROJECT ASKED TO PARTICIPATE, AND WHAT RESOLVED FROM IT.
///
/// `artifacts` IS ONE ENTRY PER AUTHORED ROW, IN AUTHORED ORDER, which is what makes
/// the count honest without a denominator field: the list length is what the plan
/// declared and each row's `state` is what happened, so `3 of 6` is readable off the value
/// rather than asserted beside it.
///
/// THE ORDER IS THE PLAN'S AND IS NOT SORTED. Inter-artifact order is authored policy
/// (LOAD-0) -- it is where an overlay has to sit after the row it covers -- so a view
/// that reordered it would hide the one thing the order is for.
///
/// `plan` is the file the host read, and it is a PROVENANCE LINE rather than an
/// identity: two hosts running the same file are running the same arrangement, and so
/// is the same host restarted from a copy of it. THE AUTHORED ROWS ARE THE
/// ARRANGEMENT. It may be empty, for a host that performed a plan it did not read
/// from anywhere.
struct ResolvedArrangement {
    std::string plan;
    std::vector<ArtifactParticipation> artifacts;

    ZEN_SHAPE(ResolvedArrangement, 1, ZEN_FIELD(plan), ZEN_FIELD(artifacts));
};

// ---- The powers ---------------------------------------------------------------

/// ONE CONTRIBUTION ELIGIBLE TO SATISFY A LOGICAL POWER.
///
/// `provider` EMPTY MEANS THE HOST ITSELF PUBLISHED IT -- `op::Contribution`'s own
/// rule, carried unchanged rather than translated, because it is the same field one
/// layer out. A view writes whatever it likes for a maker; the wire carries the
/// observation.
///
/// `composite` is `OperatorDef::is_composite()` and nothing is derived to produce it.
/// It is here because it answers the question a replaced power actually raises: a
/// composite holds its leaves as IDENTITIES and resolves them at every spend, so
/// covering `math.max` changes what a composite over it computes, while covering a
/// native leaf changes only that leaf.
struct PowerContribution {
    std::string provider;
    bool composite = false;

    ZEN_SHAPE(PowerContribution, 1, ZEN_FIELD(provider), ZEN_FIELD(composite));
};

/// ONE LOGICAL POWER AND EVERY CONTRIBUTION ELIGIBLE TO SATISFY IT, ACTIVE LAST.
///
/// THE ORDER IS THE CATALOG'S AND CARRIES THE ANSWER. `op::Catalog` holds a STACK per
/// identity and `back()` is what `find` resolves, so the last entry here is the one
/// this host currently spends and everything before it is shadowed underneath. A
/// separate `active` field would be a second answer to a question the order already
/// answers, and the second answer is the one that goes stale.
///
/// A STACK IS NEVER EMPTY. An identity with no eligible contribution is not in the
/// catalog at all -- `unmount` erases the row when its last contribution goes -- so
/// there is no "unresolved power" state to spell here, and inventing one would be
/// describing a row that does not exist.
struct PowerStack {
    std::string power;
    std::vector<PowerContribution> contributions;

    ZEN_SHAPE(PowerStack, 1, ZEN_FIELD(power), ZEN_FIELD(contributions));
};

/// WHICH POWERS THIS HOST CURRENTLY RESOLVES, AND WHO IS MOUNTED HERE.
///
/// `powers` is name-ordered because the catalog's store is a map and nothing here
/// reorders it -- the same discipline `parse_loaded` keeps about the kernel's map.
///
/// `providers` IS `Catalog::providers()` VERBATIM AND IS NOT DERIVED FROM `powers`.
/// The two come out of the same store and answer different questions: one is who is
/// mounted, the other is what is supplied. Counting distinct names across the stacks
/// would give the same number today and would be a view inferring a fact whose owner
/// is one call away.
struct ResolvedPowers {
    std::vector<PowerStack> powers;
    std::vector<std::string> providers;

    ZEN_SHAPE(ResolvedPowers, 1, ZEN_FIELD(powers), ZEN_FIELD(providers));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_ARRANGEMENT_VOCABULARY_HPP
