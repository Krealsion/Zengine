// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_PROVIDER_ABI_H
#define ZENGINE_OPERATOR_PROVIDER_ABI_H

/*
 * THE OPERATOR PROVIDER SEAM (PROV-0) — how a native image says "I provide these
 * powers", to a host that owns which of them is currently in force.
 *
 * THE DIRECTION IS THE WHOLE DIFFERENCE from host_abi.h. There, a loaded CONSUMER
 * is handed the ability to ask a host to spend the host's own operator truth. Here,
 * a loaded PROVIDER hands a host DEFINITIONS, and the host decides which one
 * currently satisfies each logical power. Those are opposite arrows and they are
 * deliberately two tables: an artifact may export one, the other, both, or neither,
 * and the Timer exports both because it consumes the host's rule and supplies it.
 *
 * WHAT WAS WRONG BEFORE. A host manufactured its catalog by CALLING a package's
 * authoring function — `op::Catalog operators = timer::standard_operators()` — which
 * compiled one artifact's semantic vocabulary into the host. Every power the system
 * had was a power the host's own translation unit contained, so "load a different
 * Timer with different semantics" was not a thing the arrangement could express.
 *
 * TWO VERBS, AND THE SECOND IS NOT FOR COMPOSITES.
 *
 *   describe(index, sink)
 *       Emit `zengine.OperatorContribution v1` bytes for contribution `index`: the
 *       identity, both port schemas with the post-order closure of everything they
 *       nest, and — for a composition — the GRAPH ITSELF. A composite crosses as
 *       STRUCTURE, so its nodes still name `math.max` on the far side and the host
 *       resolves those names against whatever currently provides them. Handing back
 *       an opaque callback that evaluated a private graph would be the same bytes
 *       and a different system: nothing a provider replaced underneath could ever
 *       propagate through it.
 *
 *   invoke(index, args, args_len, answer, reason)
 *       Spend a NATIVE contribution — the implementation genuinely lives in this
 *       image. `args` are serialized bytes of a value at that contribution's input
 *       schema; `answer` receives the serialized answer at its output schema. A
 *       composite is never invoked here, because the host holds its graph.
 *
 * INDEX, NOT IDENTITY, AND THE INDEX IS TRANSIENT. It is provider-local, it means
 * nothing outside this image and this mount, and it must never be written down as
 * an operator's durable meaning (LOG-R1). What is durable is the identity and the
 * two port schemas; the index is how a host that is HOLDING this image's record
 * reaches the code, for exactly as long as it holds it.
 *
 * INVOKE-BY-INDEX RATHER THAN A CALLABLE ACCESSOR, measured: LOG-R1 clocked
 * `invoke_at(index)` at 298.7 ns against a raw function pointer's 300.2 ns across a
 * real .so, which is to say the hazard of a naked pointer into another image buys
 * nothing. The entry point and the image have one lifetime, one refcount and one
 * destructor, so holding the image IS holding the code.
 *
 * A PROVIDER NEED NOT BE A WEAVE. This table is resolved by a host that opened the
 * file itself; no Kernel, no `zen_weave_abi`, no WeaveId, no role, no grant and no
 * bus is involved. `zengine-operators-basic` exports this symbol and nothing else
 * and is a perfectly ordinary provider; the Timer exports it beside two others and
 * is a provider, a consumer and a participant at once. Those are three independent
 * relationships and this seam is careful not to collapse them.
 *
 * THE CONVENTIONS ARE `zen/kernel/abi.h`'s, unchanged and not re-derived: an opaque
 * context passed back to every call, plain function pointers, `const uint8_t*` +
 * `size_t` inputs valid only for the call, `ZenByteSink` for every library-visible
 * return, a version field the reader REFUSES rather than guesses at, and
 * `ZEN_KERNEL_EXPORT` on the DECLARATION because MSVC counts the decoration as part
 * of the linkage (C2375). No exception, no STL type, no `op::Catalog`, no
 * `OperatorDef` and no callable crosses.
 *
 * This header is valid C and C++.
 */

#include "operator/host_abi.h"

#include <zen/kernel/abi.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* This seam's own version, distinct from ZEN_ABI_VERSION, from
 * ZENGINE_OPERATOR_ABI_VERSION and from any schema version. BOTH SIDES CARRY IT AND
 * THE READER CHECKS IT: a host meeting a provider built against another era must
 * refuse the mount and name both numbers rather than read a table whose shape it is
 * guessing at. An ABSENT symbol already means something else entirely — an artifact
 * that provides no operators, which is most of them — so the two facts must not
 * arrive as the same silence. */
#define ZENGINE_OPERATOR_PROVIDER_ABI_VERSION 1u

/* WHAT A PROVIDER IMAGE OFFERS A HOST.
 *
 * `identity` is the provider's own logical name, NUL-terminated and valid for the
 * image's lifetime (a string literal, in practice). It is NOT part of an operator's
 * identity: `math.max` is `math.max` whoever supplies it, and a host that qualified
 * every authored reference by its provider would have made replacement impossible
 * by construction. What the provider identity is FOR is mounting, unmounting and
 * saying which contribution is currently active.
 *
 * `count` is how many contributions this image supplies; valid indices are
 * `[0, count)`. Zero is a legitimate answer and a host should refuse the mount when
 * it sees one, because the way it happens in practice is that the provider's own
 * authoring failed and it has no other way to say so across a C seam.
 *
 * NEITHER VERB TOUCHES A BUS. Evaluating `max(-500, 0)` is computation over values
 * the caller already holds: no send, no publish, no correlation, no role and no
 * pump generation. That is not an optimisation — it is what an operator IS. */
typedef struct ZengineOperatorProviderV1 {
    uint32_t abi_version;
    void* ctx;
    const char* identity;
    uint32_t count;
    ZengineOperatorStatus (*describe)(void* ctx, uint32_t index, ZenByteSink sink);
    ZengineOperatorStatus (*invoke)(void* ctx, uint32_t index, const uint8_t* args,
                                    size_t args_len, ZenByteSink answer, ZenByteSink reason);
} ZengineOperatorProviderV1;

/* The symbol name a host looks up. Spelled once, here, because the host writes it as
 * a string and the provider writes it as an identifier and the two must be the same
 * word. */
#define ZENGINE_OPERATOR_PROVIDER_SYMBOL "zengine_operator_provider"

/* The one symbol an operator-providing library exports. Returns a pointer to a
 * static descriptor the host never frees — `zen_weave_abi`'s contract, unchanged.
 *
 * ABSENT IS A LEGITIMATE ANSWER AND THE COMMON ONE: every weave that provides no
 * operators exports nothing here and is loaded exactly as it always was. */
ZEN_KERNEL_EXPORT const ZengineOperatorProviderV1* zengine_operator_provider(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ZENGINE_OPERATOR_PROVIDER_ABI_H */
