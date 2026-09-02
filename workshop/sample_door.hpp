// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SAMPLE_DOOR_HPP
#define ZENGINE_WORKSHOP_SAMPLE_DOOR_HPP

// THE ONE OFFICE THAT MAY CAUSE A SOURCE TO BE EVALUATED.
//
//     SampleRequested{identity}  ->  op::sample(catalog, identity)
//                                ->  render_value(...)
//                                ->  SourceSampled{identity, ok, reason, lines}
//
// ---- WHY IT IS NOT THE ARRANGEMENT DOOR ----------------------------------------
//
// `workshop/arrangement.hpp` states its own charter in a sentence this phase was
// not willing to spend: *it answers two shapes and says nothing else, ever ... it
// cannot mount, unmount, overlay, EVALUATE, load, unload, reload or replace
// anything.* Adding a third Accept to it would have saved this file and cost that
// sentence forever, and the sentence is the more valuable of the two: *which office
// can cause evaluation* deserves a one-word answer, and after this phase it still
// has one.
//
// So the two doors are two answers to two questions that are genuinely different in
// kind:
//
//     zengine.arrangement   describe what is here            RUNS NOTHING
//     zengine.sources       run this one Source, now         RUNS EXACTLY ONE BODY
//
// It is NOT a generic host-RPC door and must not become one. There is no method
// name, no argument pack, no envelope, no dispatch table and no registration: one
// shape in, one shape out, and the shape in carries an identity because a Source's
// whole input contract is empty.
//
// ---- IT HOLDS A REFERENCE AND OWNS NOTHING -------------------------------------
//
// `const op::Catalog&`, and `Catalog::evaluate` is `const` -- so the existing
// const-reference custody the arrangement door already proves is exactly enough.
// The catalog is `main`'s local and outlives this door by declaration order
// (workshop.cpp says so where it mounts it, and `host_sources.hpp` makes the same
// argument one layer in about the Sources' own owners).
//
// ---- AND IT REMEMBERS NOTHING ---------------------------------------------------
//
// No provider, no `OperatorDef`, no callable, no schema, no rendered answer and no
// previous sample. Two samples of one identity resolve current catalog truth twice
// and run the body twice; if the owner state moved between them, the two answers
// differ, and there is nowhere in this file for a memo to hide. That is the
// property `op::sample`'s own header claims and this door is the first consumer
// that could have broken it.
//
// ---- WHAT IT DOES NOT DECIDE ---------------------------------------------------
//
// Not whether an identity exists (the catalog's sentence), not whether it is a
// Source (`op::sample`'s), not whether the pack is admissible (the gate's), and not
// what a maker should do about any of it. Every refusal a maker reads here was
// written by the layer that detected it, quoted whole -- there is no second wording
// of one refusal anywhere in this phase.

#include "sample_presentation.hpp"
#include "sample_vocabulary.hpp"

#include "operator/catalog.hpp"
#include "operator/operator.hpp"
#include "operator/source.hpp"

#include <zen/weave.hpp>

#include <cstdint>

namespace zengine::workshop {

/// WHAT THIS DOOR HAS DONE, and it is all counters.
///
/// NO ANSWER AND NO IDENTITY IS IN HERE. `ArrangementDoorState`'s reason exactly: a
/// value kept between asks would be the cache this phase exists not to build, and
/// the cache is the copy that goes stale. What survives a snapshot is how many
/// samples this office spent, how many the catalog refused, and how many asks
/// arrived without an office to be answerable to.
///
/// `spent` COUNTS ASKS THIS DOOR ACTED ON, not evaluator bodies. The body count is
/// `op::invocations()`, which is the operator package's own observability and the
/// number a suite compares deltas of; two numbers for one fact would eventually
/// disagree, and the one that can lie is the one kept here.
struct SampleDoorState {
    std::int64_t spent = 0;
    std::int64_t refusals = 0; ///< samples the catalog or the Source seam declined
    std::int64_t refused = 0;  ///< asks that were not authored as any office
    ZEN_EXPOSE();
    ZEN_SHAPE(SampleDoorState, 1, ZEN_FIELD(spent), ZEN_FIELD(refusals), ZEN_FIELD(refused));
};

/// THE HOST'S ONE EVALUATION DOOR.
class SampleDoor : public loom::WeaveBase<SampleDoor, SampleDoorState,
                                          loom::Accept<SampleRequested>,
                                          loom::Emit<SourceSampled>> {
public:
    explicit SampleDoor(const op::Catalog& catalog) : catalog_(&catalog) {}

    /// SAMPLE ONE SOURCE, BECAUSE SOMEBODY EXPLICITLY ASKED.
    ///
    /// THE GESTURE IS THE AUTHORITY AND THIS DOOR MERELY ROUTES IT. Nothing here
    /// decides that a sample is a good idea, when to take one, or whether to take
    /// another: there is no timer, no subscription, no watcher, no re-ask on any
    /// event, and no path by which this handler runs except an ask arriving.
    ///
    /// RESOLVED AT THE SPEND. `op::sample` looks the identity up NOW -- so a
    /// provider unmounted between a maker reading a row and pressing it produces
    /// the catalog's own `nothing supplies` sentence rather than a stale answer,
    /// and a stale identity that now resolves as a parameterized Operator produces
    /// the Source seam's own sentence. Neither is re-worded here.
    ///
    /// RENDERED WHERE THE SCHEMA IS. The admitted `loom::Value` never leaves this
    /// process's host half: `render_value` projects it to lines and the LINES are
    /// what cross (sample_presentation.hpp says why at length).
    ///
    /// ANSWERED, NOT SENT. `mail.answer` is Loom's own door -- the recipient and
    /// the correlation are the bus's, so the answer cannot be aimed elsewhere or
    /// relabelled, and the asker reads `answers_ask()` off provenance no payload
    /// can write. One ask, one sample, one answer.
    void on(const SampleRequested& asked, loom::Mail& mail) {
        if (mail.authored_role().empty()) {
            // ANONYMOUS SPEECH MAY NOT CAUSE EVALUATION. The arrangement door's
            // exact rule and its exact honesty about what that is not: it names
            // nobody, so a tool added tomorrow asks with no edit here; and it is
            // NOT containment, because the loader binds `allow_any()` to every
            // library it opens. What it buys is that every body this office ever
            // ran was run for a named office, which is a fact a report can state.
            ++state_.refused;
            return;
        }
        ++state_.spent;
        const op::Evaluation done = op::sample(*catalog_, asked.identity);
        SourceSampled said;
        said.identity = asked.identity;
        said.ok = done.ok();
        if (done.ok()) {
            said.lines = render_value(done.value());
        } else {
            ++state_.refusals;
            said.reason = done.reason();
        }
        (void)mail.answer(said);
        // AND THE VALUE GOES OUT OF SCOPE HERE, which is what makes "this door
        // caches no answer" a fact about this function rather than a promise about
        // the class.
    }

private:
    const op::Catalog* catalog_;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_SAMPLE_DOOR_HPP
