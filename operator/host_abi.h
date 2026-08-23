// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_HOST_ABI_H
#define ZENGINE_OPERATOR_HOST_ABI_H

/*
 * THE OPERATOR HOST SEAM (OPH-0) — how a dynamically loaded consumer spends the
 * host's operator truth without being handed the catalog that holds it.
 *
 * WHAT THE WALL WAS. SEM-0's independent consumer received `const op::Catalog&`,
 * which works in one image and nowhere else. A loaded weave is built by
 * `create(void)` (abi.h) and sees exactly one host table for the rest of its life
 * — Loom's `ZenHostApi` — and not one of its doors is a callable. So a loaded
 * tool had four ways to reach an operator and all four were wrong: a message per
 * evaluation (a pump generation per node), an operator door on `ZenHostApi` (a
 * Loom ABI break, and the wrong layer), its own private catalog (a second answer
 * waiting for one of them to be edited), or a C++ `Catalog*` across a module
 * boundary (an STL type across a seam that admits none).
 *
 * THE FIFTH OPTION IS THIS FILE, and it is deliberately the smallest one: a
 * consumer image may OPTIONALLY export one symbol saying "I can receive an
 * operator host". A Zengine host that is about to load such an image resolves
 * that symbol and offers a narrow C table. Nothing about the catalog crosses —
 * not the object, not a callable, not an index. What crosses is the ABILITY TO
 * ASK the host to spend its own current operator truth, and two answers in bytes.
 *
 * THE CONVENTIONS ARE `zen/kernel/abi.h`'s AND NOT A SECOND STYLE. Opaque
 * context, plain function pointers, `const uint8_t*` + `size_t` inputs valid only
 * for the call, `ZenByteSink` for every library-visible return, a version field
 * the reader REFUSES rather than guesses at, and `ZEN_KERNEL_EXPORT` on the
 * DECLARATION because MSVC counts the decoration as part of the linkage (C2375).
 * Every Zen value crosses as serialized bytes and is re-admitted through the one
 * gate on the far side, exactly as a message does.
 *
 * WHAT IS DELIBERATELY NOT HERE. No enumeration door (a consumer names the
 * operator it was authored against), no authoring door (nothing loaded may
 * publish into the host's catalog), no callable accessor (LOG-R1: invoking by
 * name costs what a raw function pointer costs, and the hazard is gone), no
 * subscription, no lifetime handle, and nothing about weaves, roles, grants,
 * senses or messages. Evaluating `max(-500, 0)` is not a conversation and must
 * not acquire the machinery of one.
 *
 * This header is valid C and C++.
 */

#include <zen/kernel/abi.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The seam's own version, distinct from ZEN_ABI_VERSION and from any schema
 * version. BOTH SIDES CARRY IT AND BOTH SIDES CHECK IT, which is the point: a
 * host meeting a consumer built against another era must refuse the handoff and
 * say so, rather than call through a table whose shape it is guessing at. The
 * consumer is still an ordinary weave when that happens — it simply has no
 * operator host, which is a state it must already be able to be in (see
 * ZENGINE_OP_ERR_NO_HOST). */
#define ZENGINE_OPERATOR_ABI_VERSION 1u

/* Status codes across this seam. The STYLE is `ZenStatus`'s — int32, 0 == OK,
 * negatives are errors, no exception ever crosses — and the VALUES are their own
 * space on purpose: these are answers about an OPERATOR, and folding them into
 * ZEN_ERR_* would put "no such operator" in the same namespace as "the host had
 * no such routing target", which sends a reader to the wrong layer.
 *
 * FIVE FAILURES, FIVE ANSWERS, and the first two never come from the host at all
 * — there is no host to answer them, which is exactly why they are distinct. A
 * missing host and a missing operator are not the same trouble: one is a
 * question about how this image was loaded, the other about what the host
 * publishes. */
typedef int32_t ZengineOperatorStatus;
enum {
    ZENGINE_OP_OK = 0,
    /* Consumer-side only: nothing was ever offered to this instance, so there is
     * nobody to ask. An ordinary weave loaded by an ordinary host is in this
     * state and it is not an error — it is the floor. */
    ZENGINE_OP_ERR_NO_HOST = -1,
    /* The two sides do not agree on this seam's version. Whoever noticed refuses
     * the handoff; nothing is stored and nothing is called. */
    ZENGINE_OP_ERR_ABI = -2,
    /* The host publishes no operator under that identity. */
    ZENGINE_OP_ERR_NOT_FOUND = -3,
    /* The operator refused: a bad argument pack, an unresolved step, a step that
     * is not the signature the composition was authored against, or an answer
     * its own output schema refuses. The reason sink carries the refusal in
     * whoever's words own it — the gate's or the catalog's — verbatim. */
    ZENGINE_OP_ERR_REFUSED = -4,
    /* The bytes handed across were not a well-formed Zen envelope at all, so
     * nothing could be said about which schema they claim. Distinct from
     * REFUSED, which is a value that arrived and was judged. */
    ZENGINE_OP_ERR_MALFORMED = -5,
    /* The host itself failed while answering — contained at the seam rather than
     * thrown across it, exactly as the kernel's adapter contains a library's
     * throw. It says nothing about the request. */
    ZENGINE_OP_ERR_HOST_FAILED = -6,
    /* The mirror image, added by PROV-0 for the seam that points the other way
     * (provider_abi.h): a PROVIDER failed while describing or spending one of its
     * own contributions. One status space for answers about operators, because a
     * reader chasing "who could not do this" should not have to know which of the
     * two tables produced the number; the sign of the failure is the same either
     * way, and only the direction differs. */
    ZENGINE_OP_ERR_PROVIDER_FAILED = -7
};

/* WHAT A LOADED CONSUMER MAY ASK OF THE HOST'S OPERATORS. Two verbs, because two
 * is what a generic consumer needs: learn an operator's actual contract, and
 * spend it.
 *
 * `ctx` is the host's own opaque context and is passed back to every call; its
 * lifetime is the host's guarantee (see host_surface.hpp — the host outlives
 * every consumer it offered this to). `identity` is NUL-terminated and valid
 * only for the call, like every string in ZenHostApi.
 *
 * describe(identity, sink)
 *     Emit `zengine.OperatorDesc v1` bytes: the identity, the input and output
 *     SCHEMAS, and the post-order closure of every schema those two nest. The
 *     descriptor is derived from the same `OperatorDef` that `evaluate` resolves
 *     — there is no second, hand-written description of an operator anywhere —
 *     and it travels through `zen.SchemaDesc v1`, the codec a manifest already
 *     uses, so this seam introduces no second schema language.
 *
 * evaluate(identity, args, args_len, answer, reason)
 *     `args` are serialized bytes of a value at the operator's INPUT schema; the
 *     host parses, admits them through the one gate, evaluates, and emits the
 *     answer's serialized bytes to `answer`. On ZENGINE_OP_ERR_REFUSED the
 *     `reason` sink receives the refusal prose and `answer` is untouched; on
 *     success `reason` is untouched. Never both.
 *
 *     THE REASON IS A SINK AND NOT A BUFFER, for `ZenSenseBy`'s reason: a
 *     truncated refusal is a sentence nobody wrote, and a bigger buffer only
 *     moves the lie further out. Either sink may carry a null `write`, which
 *     means the caller does not want that half.
 *
 * NEITHER VERB TOUCHES A BUS. Evaluation is computation over values the caller
 * already holds: no send, no publish, no correlation, no answer authority, no
 * role, and no pump generation. That is not an optimisation, it is what an
 * operator IS — and it is why this table is Zengine's rather than another door
 * on Loom's. */
typedef struct ZengineOperatorHostApiV1 {
    uint32_t abi_version;
    void* ctx;
    ZengineOperatorStatus (*describe)(void* ctx, const char* identity, ZenByteSink sink);
    ZengineOperatorStatus (*evaluate)(void* ctx, const char* identity, const uint8_t* args,
                                      size_t args_len, ZenByteSink answer, ZenByteSink reason);
} ZengineOperatorHostApiV1;

/* THE OPTIONAL SURFACE A CONSUMER IMAGE EXPORTS — the whole of what makes an
 * artifact operator-aware.
 *
 * A TABLE BEHIND ONE SYMBOL, and the version in a FIELD rather than in the
 * symbol's name, because that is `zen_weave_abi`'s shape and it is the shape
 * that lets a mismatch be REPORTED. A versioned symbol name gives a host exactly
 * one bit — the lookup failed — and an absent symbol already means something
 * else here (an ordinary weave, which must keep loading untouched). Two
 * different facts must not arrive as the same silence.
 *
 * offer(api)
 *     The host hands its table to THIS IMAGE for the instance it is about to
 *     create. A null `api` WITHDRAWS a previous offer, and the host always
 *     withdraws — so the storage behind this call is empty except during one
 *     load, and two instances of one image each receive their own offer rather
 *     than sharing a durable module-wide binding.
 *
 *     The consumer checks `api->abi_version` before it stores anything, and
 *     answers ZENGINE_OP_ERR_ABI if it does not know that number. */
typedef struct ZengineOperatorConsumerV1 {
    uint32_t abi_version;
    ZengineOperatorStatus (*offer)(const ZengineOperatorHostApiV1* api);
} ZengineOperatorConsumerV1;

/* The symbol name a host looks up. Spelled once, here, because the host writes
 * it as a string and the consumer writes it as an identifier and the two must
 * be the same word. */
#define ZENGINE_OPERATOR_CONSUMER_SYMBOL "zengine_operator_consumer"

/* The one symbol an operator-aware consumer library exports. Returns a pointer
 * to a static descriptor the host never frees — `zen_weave_abi`'s contract,
 * unchanged.
 *
 * ABSENT IS A LEGITIMATE ANSWER AND THE COMMON ONE. Every weave that predates
 * this seam, and every weave that will never want an operator, exports nothing
 * here and loads exactly as it always did. `ZEN_KERNEL_EXPORT` is reused rather
 * than re-derived so this declaration and `zen_weave_abi`'s agree by
 * construction on every platform ladder, including the MinGW one where
 * decorating any symbol switches off export-everything. */
ZEN_KERNEL_EXPORT const ZengineOperatorConsumerV1* zengine_operator_consumer(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ZENGINE_OPERATOR_HOST_ABI_H */
