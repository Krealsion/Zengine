// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SAMPLE_DOOR_HPP
#define ZENGINE_WORKSHOP_SAMPLE_DOOR_HPP

// The one office that may cause a Source to be evaluated: `SampleRequested{identity}` in, then
// `op::sample`, `render_value` and `SourceSampled` out. Not the arrangement door, which runs
// nothing and must be able to say so, and not a generic host-RPC door. It holds a const reference
// to the catalog (`main`'s local, which outlives it) and remembers nothing, so two samples run the
// body twice; every refusal is quoted whole from the layer that detected it.

#include "sample_presentation.hpp"
#include "sample_vocabulary.hpp"

#include "operator/catalog.hpp"
#include "operator/operator.hpp"
#include "operator/source.hpp"

#include <zen/weave.hpp>

#include <cstdint>

namespace zengine::workshop {

/// What this door has done: counters only, never an answer or an identity, since a value kept
/// between asks would go stale. `spent` counts asks acted on; the body count is
/// `op::invocations()`.
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

    /// Sample one Source, because somebody explicitly asked: the gesture is the authority and this
    /// door routes it, with no timer, subscription or re-ask. Resolved at the spend, so a provider
    /// unmounted since reads as the catalog's own sentence; rendered where the schema is, so only
    /// lines cross; answered through `mail.answer`, so the reply cannot be aimed elsewhere.
    void on(const SampleRequested& asked, loom::Mail& mail) {
        if (mail.authored_role().empty()) {
            // Anonymous speech may not cause evaluation. The rule names nobody and is not
            // containment (the loader binds `allow_any()` to every library it opens); it makes
            // every body this office ran one run for a named office.
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
