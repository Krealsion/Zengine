// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_HOST_SURFACE_HPP
#define ZENGINE_OPERATOR_HOST_SURFACE_HPP

// The host's side of the operator seam, two objects: `OperatorHostSurface`, one catalog wearing
// host_abi.h's C table, and `OperatorOffer`, that table offered to one artifact image for the
// length of one load and taken back after. The surface is declared before the Kernel and
// outlives everything; an offer is scoped around one ordinary `zen.LoadWeave`, placed before the
// load is asked for, since a weave's first need can be inside `create()`, where no host can get
// between.
// Reference: docs/reference/operator-host.md.

// The host opens the image itself: Loom has no public door to a second exported symbol of a
// kernel-loaded image. Both opens name one image (the loader refcounts), and the offer's share
// is released before the Kernel's, so `unload` unmaps exactly when it would have. The offer is
// scoped rather than held because every hazard of a held image -- an unload that closes
// nothing, a reload finding the old mapping, a slot still full -- is a state it cannot be in.

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

/// The host's operator power, in the shape a loaded image can hold: it borrows a catalog and
/// owns a table of function pointers at its own thunks. Both verbs read the catalog the host's
/// native code reads, at the moment of the call -- no copy, snapshot or second store.
/// Non-copyable and non-movable: this object's address is the `ctx` every offered table carries.
/// Every consumer's copy of the table names `this`, so it must outlive every artifact it was
/// offered to: declare it before the Kernel and let destruction order do the rest.
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
    /// No exception crosses this seam: the kernel adapter's discipline pointing the other way,
    /// and why `ZENGINE_OP_ERR_HOST_FAILED` exists.
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
            // Derived from the definition `evaluate` resolves, and nothing else: a hand-written
            // descriptor could disagree with what it describes.
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
            // The catalog's own bytes door: the admission happens inside, so a loaded consumer
            // reads an in-process caller's sentence about a bad pack, character for character.
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

/// Why an offer ended the way it did: four states, the first two both ordinary.
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
    /// The image could not be opened. The offer does nothing and says so; the load produces the
    /// authoritative error, since the loader owns that sentence.
    NotOpened,
};

/// The host's operator power, offered to one artifact image for one load: construct it before
/// asking for the load and destroy it after, and only in between can an instance created from
/// that image take the offer. It holds one share of the image, solely to resolve a second
/// exported symbol, never the share that keeps the artifact alive. Withdrawal is unconditional:
/// the destructor offers `nullptr` whatever happened, so the slot is empty outside this scope.
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
            // An ordinary weave, not a diagnostic; the destructor still releases the share.
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
        // The version is read before anything else about the table: it decides what every other
        // field means.
        if (table->abi_version != ZENGINE_OPERATOR_ABI_VERSION) {
            // Refused, not guessed (`fetch_abi`'s rule). The version rides in a field rather than
            // the symbol's name, since a failed lookup would be indistinguishable from an
            // ordinary weave.
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
        // `share_` closes itself as a member, which also covers what a destructor cannot: a
        // foreign `offer` throwing out of the constructor, when this body never runs.
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
    /// One share of one image, owned from the first moment it is open. `ImageShare` holds the
    /// platform calls and Loom's own flags, shared with provider mounting so both open the image
    /// the way the Kernel does. Every refusal above just returns and the handle closes with this
    /// member: no path forgets to close, closes twice, or leaks the mapping. First member, so it
    /// exists before the constructor's body runs and goes last -- what makes the throwing path
    /// safe rather than merely unlikely.
    ImageShare share_;
    ZengineOperatorStatus (*offer_)(const ZengineOperatorHostApiV1*) = nullptr;
    OfferOutcome outcome_ = OfferOutcome::NotAConsumer;
    std::string reason_;
};

} // namespace zengine::op

#endif // ZENGINE_OPERATOR_HOST_SURFACE_HPP
