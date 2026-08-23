// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_HOST_SURFACE_HPP
#define ZENGINE_OPERATOR_HOST_SURFACE_HPP

// THE HOST'S SIDE OF THE OPERATOR SEAM (OPH-0) — two objects, and neither of
// them is a framework.
//
//     OperatorHostSurface   one Catalog, wearing host_abi.h's C table.
//     OperatorOffer         that table, offered to ONE artifact image for the
//                           length of ONE load, and taken back afterwards.
//
// A host writes:
//
//     op::OperatorHostSurface operators(catalog);          // outlives everything below
//     {
//         op::OperatorOffer offer(operators, host.so("my-tool"));
//         boot("my-tool", kMyRole);                        // ordinary LoadWeave
//         bus.pump();
//     }                                                    // offer withdrawn, share closed
//
// WHY THE HOST OPENS THE IMAGE ITSELF, and it is the phase's one unavoidable
// duplication. Loom's `LoadedLibrary` is forward-declared in the public header
// and defined in the .cpp; `lib_symbol` is a file-local helper with exactly one
// caller, for exactly one name. There is NO public door to a second exported
// symbol of a kernel-loaded image, so a host that wants one opens the same file
// again — which LOG-R1 measured: the loader refcounts, both opens name ONE
// image, and the offer's share is released before the Kernel's, so `unload`
// still unmaps exactly when it always did. The alternative was a generic
// symbol-lookup door in Loom, which would buy back these twenty lines at the
// price of a change to the substrate for an application's convenience.
//
// AND WHY THE OFFER IS SCOPED RATHER THAN HELD. Everything that could go wrong
// with "the host keeps an image open" comes from keeping it open: an unload that
// closes nothing, a reload that finds the old mapping, a module slot still full
// when the next instance is created. A scoped offer has none of those states,
// because outside its scope the host holds nothing at all.
//
// THE OFFER IS BEFORE THE LOAD, deliberately. A weave's first legitimate need
// for an operator can be inside `create()` — which the Kernel calls, and which
// a host cannot get between. So the offer is placed before the load is even
// asked for, and taken back after; a consumer's constructor is inside that
// window whether the load came from a direct `Kernel::load` or from a
// `zen.LoadWeave` message travelling through the Weave Manager, which is the
// path a real Zengine host actually uses.

#include "operator/catalog.hpp"
#include "operator/host.hpp"
#include "operator/host_abi.h"
#include "operator/image.hpp"
#include "operator/operator.hpp"

#include <zen/serialize.hpp>
#include <zen/value.hpp>

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace zengine::op {

namespace detail {

/// Write a whole buffer into a caller's sink, if it wanted that half.
inline void emit(ZenByteSink sink, const std::string& bytes) {
    if (sink.write != nullptr) {
        sink.write(sink.ctx, reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
    }
}

} // namespace detail

// ---- one catalog, wearing the C table --------------------------------------

/// THE HOST'S OPERATOR POWER, in the shape a loaded image can hold.
///
/// It borrows a catalog and owns a table of function pointers pointing at its
/// own thunks. Both verbs read THE SAME `Catalog` the host's native code reads,
/// resolved at the moment of the call: there is no copy, no snapshot and no
/// second store, so a loaded consumer and the host cannot come to disagree about
/// what a rule means.
///
/// NON-COPYABLE AND NON-MOVABLE, for `LoadedLibrary`'s reason: the address of
/// this object IS the `ctx` every offered table carries, so an object that could
/// move would leave live consumers pointing at where it used to be.
///
/// LIFETIME IS THE HOST'S PROMISE AND THE HOST'S ALONE. Every consumer that
/// accepted an offer holds a copy of this table, and the copy names `this`. So
/// this object must outlive every artifact it was offered to — which for a host
/// means: declare it before the Kernel, and let ordinary destruction order do
/// the rest (the Kernel goes first, taking its artifacts with it). Nothing here
/// takes shared ownership across the ABI to enforce that, because a lifetime the
/// host already controls does not need a refcount to be correct; it needs to be
/// stated, and this is where it is stated.
class OperatorHostSurface {
public:
    explicit OperatorHostSurface(const Catalog& catalog) noexcept : catalog_(&catalog) {
        api_.abi_version = ZENGINE_OPERATOR_ABI_VERSION;
        api_.ctx = this;
        api_.describe = &describe_thunk;
        api_.evaluate = &evaluate_thunk;
    }

    OperatorHostSurface(const OperatorHostSurface&) = delete;
    OperatorHostSurface& operator=(const OperatorHostSurface&) = delete;
    OperatorHostSurface(OperatorHostSurface&&) = delete;
    OperatorHostSurface& operator=(OperatorHostSurface&&) = delete;

    /// The table to offer. Valid for this object's lifetime; a consumer copies
    /// it, so the pointer itself need not outlive the offer.
    const ZengineOperatorHostApiV1* api() const noexcept { return &api_; }

    /// The catalog behind it — the host's own, not a copy of it.
    const Catalog& catalog() const noexcept { return *catalog_; }

private:
    /// NO EXCEPTION CROSSES THIS SEAM. The kernel's adapter contains a library's
    /// throw with `catch(...)` -> status; this is the same discipline pointing
    /// the other way, and it is why `ZENGINE_OP_ERR_HOST_FAILED` exists at all.
    static ZengineOperatorStatus describe_thunk(void* ctx, const char* identity,
                                                ZenByteSink sink) {
        auto* self = static_cast<OperatorHostSurface*>(ctx);
        if (self == nullptr || identity == nullptr) {
            return ZENGINE_OP_ERR_HOST_FAILED;
        }
        try {
            const OperatorDef* def = self->catalog_->find(identity);
            if (def == nullptr) {
                return ZENGINE_OP_ERR_NOT_FOUND;
            }
            // DERIVED FROM THE DEFINITION `evaluate` RESOLVES, and from nothing
            // else. There is no hand-written descriptor beside an operator here
            // and there must never be one: a description that could disagree
            // with the thing it describes is the second copy this whole
            // substrate exists to remove.
            detail::emit(sink, loom::serialize(encode_operator_desc(def->identity(),
                                                                   *def->inputs(),
                                                                   *def->outputs())));
            return ZENGINE_OP_OK;
        } catch (...) {
            return ZENGINE_OP_ERR_HOST_FAILED;
        }
    }

    static ZengineOperatorStatus evaluate_thunk(void* ctx, const char* identity,
                                                const std::uint8_t* args, std::size_t args_len,
                                                ZenByteSink answer, ZenByteSink reason) {
        auto* self = static_cast<OperatorHostSurface*>(ctx);
        if (self == nullptr || identity == nullptr) {
            return ZENGINE_OP_ERR_HOST_FAILED;
        }
        try {
            if (self->catalog_->find(identity) == nullptr) {
                // Answered before the bytes are looked at, so "there is no such
                // operator" never arrives wearing "your arguments were wrong".
                return ZENGINE_OP_ERR_NOT_FOUND;
            }
            const loom::Unverified unverified =
                loom::parse(std::string_view(reinterpret_cast<const char*>(args), args_len));
            if (!unverified.well_formed()) {
                // Not a Zen envelope at all: nothing can be said about which
                // schema it claims, so there is no refusal prose to quote.
                return ZENGINE_OP_ERR_MALFORMED;
            }
            // THE CATALOG'S OWN BYTES DOOR. The admission happens in there, so
            // the sentence a loaded consumer reads about a bad pack is
            // character-for-character the one an in-process caller reads.
            const Evaluation answered = self->catalog_->evaluate(identity, unverified);
            if (!answered) {
                detail::emit(reason, answered.reason());
                return ZENGINE_OP_ERR_REFUSED;
            }
            detail::emit(answer, loom::serialize(answered.value()));
            return ZENGINE_OP_OK;
        } catch (...) {
            return ZENGINE_OP_ERR_HOST_FAILED;
        }
    }

    const Catalog* catalog_;
    ZengineOperatorHostApiV1 api_{};
};

// ---- offering it to one image ----------------------------------------------

/// WHY AN OFFER ENDED THE WAY IT DID — four states, and the first two are both
/// ordinary.
enum class OfferOutcome : std::uint8_t {
    /// The image exports no operator surface: an ordinary weave, loaded
    /// unchanged. The common case, and not a diagnostic.
    NotAConsumer,
    /// The table was handed over. The next instance created from this image may
    /// take it.
    Offered,
    /// The image exports the surface at a version this host does not know, or
    /// the consumer refused this host's. Nothing was handed over and nothing was
    /// called; `reason()` names both numbers.
    VersionMismatch,
    /// The image could not be opened at all. The offer does nothing and says so;
    /// the LOAD is left to produce the authoritative error, because the loader
    /// is the one that owns that sentence.
    NotOpened,
};

/// THE HOST'S OPERATOR POWER, OFFERED TO ONE ARTIFACT IMAGE FOR ONE LOAD.
///
/// Construct it before asking for the load; destroy it after the load has
/// happened. Between those two points, and only between them, an instance
/// created from that image can take the offer.
///
/// WHAT IT HOLDS AND FOR HOW LONG: one share of the image, released in the
/// destructor. That share exists solely so a second exported symbol can be
/// resolved; it is never the share that keeps the artifact alive, and releasing
/// it is what leaves the Kernel's unload behaving exactly as it does for a weave
/// that never heard of operators.
///
/// WITHDRAWAL IS UNCONDITIONAL. The destructor offers `nullptr` whatever
/// happened — whether the load succeeded, refused, or was never asked for — so
/// the module's slot is empty outside this scope by construction rather than by
/// anybody remembering.
class OperatorOffer {
public:
    OperatorOffer(const OperatorHostSurface& surface, const std::string& artifact_path)
        : share_(artifact_path) {
        if (!share_.open()) {
            outcome_ = OfferOutcome::NotOpened;
            reason_ = "could not open '" + artifact_path + "' to look for an operator surface";
            return;
        }
        void* symbol = share_.symbol(ZENGINE_OPERATOR_CONSUMER_SYMBOL);
        if (symbol == nullptr) {
            // An ordinary weave. Not a diagnostic, and the share is still
            // released by the destructor like every other path.
            outcome_ = OfferOutcome::NotAConsumer;
            return;
        }
        using ConsumerFn = const ZengineOperatorConsumerV1* (*)(void);
        ConsumerFn entry = nullptr;
        std::memcpy(&entry, &symbol, sizeof(entry)); // object->function, the -Wpedantic-clean way
        const ZengineOperatorConsumerV1* table = entry();
        if (table == nullptr) {
            outcome_ = OfferOutcome::VersionMismatch;
            reason_ = "'" + artifact_path + "' exports " + ZENGINE_OPERATOR_CONSUMER_SYMBOL +
                      " and answers with no table at all";
            return;
        }
        // THE VERSION IS READ BEFORE ANYTHING ELSE ABOUT THE TABLE, because
        // every other field's MEANING is what the version decides. Judging
        // `offer` first would be reading a slot whose shape is exactly what is
        // in doubt.
        if (table->abi_version != ZENGINE_OPERATOR_ABI_VERSION) {
            // REFUSED, NOT GUESSED. A table whose shape this host cannot vouch
            // for is never called -- which is `fetch_abi`'s own rule, and the
            // reason the version rides in a FIELD rather than in the symbol's
            // name: a mismatch that arrived as a failed lookup would be
            // indistinguishable from an ordinary weave.
            outcome_ = OfferOutcome::VersionMismatch;
            reason_ = "'" + artifact_path + "' offers operator surface v" +
                      std::to_string(table->abi_version) + "; this host speaks v" +
                      std::to_string(ZENGINE_OPERATOR_ABI_VERSION);
            return;
        }
        if (table->offer == nullptr) {
            outcome_ = OfferOutcome::VersionMismatch;
            reason_ = "'" + artifact_path + "' declares operator surface v" +
                      std::to_string(table->abi_version) + " and offers no way to receive one";
            return;
        }
        const ZengineOperatorStatus taken = table->offer(surface.api());
        if (taken != ZENGINE_OP_OK) {
            outcome_ = OfferOutcome::VersionMismatch;
            reason_ = "'" + artifact_path + "' refused this host's operator surface v" +
                      std::to_string(ZENGINE_OPERATOR_ABI_VERSION) + " (status " +
                      std::to_string(static_cast<int>(taken)) + ")";
            return;
        }
        offer_ = table->offer;
        outcome_ = OfferOutcome::Offered;
    }

    ~OperatorOffer() {
        if (offer_ != nullptr) {
            offer_(nullptr);
        }
        // The image share needs no line here: `share_` is a member and closes
        // itself, which is also what covers the one path a destructor cannot --
        // a foreign `offer` that throws out of the constructor above. The object
        // is never constructed then, so this body never runs, and only a member
        // that owns its own handle is released by the unwinding.
    }

    OperatorOffer(const OperatorOffer&) = delete;
    OperatorOffer& operator=(const OperatorOffer&) = delete;
    OperatorOffer(OperatorOffer&&) = delete;
    OperatorOffer& operator=(OperatorOffer&&) = delete;

    OfferOutcome outcome() const noexcept { return outcome_; }
    bool offered() const noexcept { return outcome_ == OfferOutcome::Offered; }
    /// Empty unless something went wrong; a plain weave produces no diagnostic,
    /// because being a plain weave is not a fault.
    const std::string& reason() const noexcept { return reason_; }

private:
    /// ONE SHARE OF ONE IMAGE, OWNED FROM THE FIRST MOMENT IT IS OPEN.
    ///
    /// `operator/image.hpp` owns the three platform calls, and owns them because
    /// `mount_provider` wants exactly the same three: two copies of a dlopen with
    /// two independently maintained flag lists is two things that can drift about
    /// whether an artifact is opened the way the Kernel opens it. The flags are
    /// Loom's own, which is what makes both opens name ONE image.
    ///
    /// `LoadedLibrary`'s shape and `Kernel::load`'s discipline both: every refusal
    /// in the constructor above simply returns, and the handle closes when this
    /// member goes -- so there is no failure path that can forget to close, none
    /// that can close twice, and none (including a foreign `offer` that throws)
    /// that can leak the mapping on the way out.
    ///
    /// FIRST MEMBER, so it is constructed before the constructor's body runs and
    /// destroyed last of all -- which is what makes the throwing path above safe
    /// rather than merely unlikely.
    ImageShare share_;
    ZengineOperatorStatus (*offer_)(const ZengineOperatorHostApiV1*) = nullptr;
    OfferOutcome outcome_ = OfferOutcome::NotAConsumer;
    std::string reason_;
};

} // namespace zengine::op

#endif // ZENGINE_OPERATOR_HOST_SURFACE_HPP
