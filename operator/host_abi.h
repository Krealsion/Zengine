// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_HOST_ABI_H
#define ZENGINE_OPERATOR_HOST_ABI_H

/*
 * The operator host seam: how a dynamically loaded consumer spends the host's operator truth
 * without being handed the catalog that holds it. A consumer image may optionally export one
 * symbol saying "I can receive an operator host"; a Zengine host about to load it offers a
 * narrow C table. Nothing about the catalog crosses -- no object, callable or index -- only the
 * ability to ask the host to spend its current truth, and two answers in bytes.
 * Reference: docs/reference/operator-host.md.
 *
 * The conventions are `zen/kernel/abi.h`'s: opaque context, plain function pointers, inputs
 * valid only for the call, `ZenByteSink` for every return, a version field the reader refuses
 * rather than guesses at, and `ZEN_KERNEL_EXPORT` on the declaration (MSVC counts it as linkage,
 * C2375). Values cross as serialized bytes and are re-admitted through the one gate. Nothing
 * here enumerates, authors, hands out a callable, subscribes or touches a bus.
 *
 * This header is valid C and C++.
 */

#include <zen/kernel/abi.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The seam's own version, distinct from ZEN_ABI_VERSION and any schema version. Both sides
 * carry and check it: a host meeting a consumer built against another version refuses the
 * handoff and says so, and the consumer loads as an ordinary weave with no operator host. */
#define ZENGINE_OPERATOR_ABI_VERSION 1u

/* Status codes across this seam: `ZenStatus`'s style (int32, 0 == OK, negatives are errors, no
 * exception crosses) in a space of their own, since these answer about an operator, not a
 * routing target. The first two never come from a host: a missing host is a question about how
 * this image was loaded, a missing operator one about what the host publishes. */
typedef int32_t ZengineOperatorStatus;
enum {
    ZENGINE_OP_OK = 0,
    /* Consumer-side only: nothing was offered to this instance. The ordinary state of an
     * ordinary weave, not an error. */
    ZENGINE_OP_ERR_NO_HOST = -1,
    /* The two sides do not agree on this seam's version. Whoever noticed refuses
     * the handoff; nothing is stored and nothing is called. */
    ZENGINE_OP_ERR_ABI = -2,
    /* The host publishes no operator under that identity. */
    ZENGINE_OP_ERR_NOT_FOUND = -3,
    /* The operator refused: a bad argument pack, an unresolved step, a step not at its
     * authored signature, or an answer its own output schema refuses. The reason sink carries
     * the refusal verbatim, in the words of whoever owns it. */
    ZENGINE_OP_ERR_REFUSED = -4,
    /* The bytes handed across were not a well-formed Zen envelope at all, so
     * nothing could be said about which schema they claim. Distinct from
     * REFUSED, which is a value that arrived and was judged. */
    ZENGINE_OP_ERR_MALFORMED = -5,
    /* The host itself failed while answering — contained at the seam rather than
     * thrown across it, exactly as the kernel's adapter contains a library's
     * throw. It says nothing about the request. */
    ZENGINE_OP_ERR_HOST_FAILED = -6,
    /* A provider failed while describing or spending one of its own contributions
     * (provider_abi.h). One status space for answers about operators, so a reader chasing "who
     * could not do this" need not know which table produced the number. */
    ZENGINE_OP_ERR_PROVIDER_FAILED = -7
};

/* What a loaded consumer may ask of the host's operators: learn an operator's contract, and
 * spend it. `ctx` is the host's, passed back to every call, and the host outlives every consumer
 * it offered this to; `identity` is NUL-terminated and valid only for the call. Neither verb
 * touches a bus: evaluation is computation over values the caller already holds.
 *
 * describe(identity, sink): `zengine.OperatorDesc v1` bytes -- the identity, both port schemas
 * and the post-order closure of what they nest, derived from the `OperatorDef` `evaluate`
 * resolves, in `zen.SchemaDesc v1`, a manifest's codec.
 *
 * evaluate(identity, args, args_len, answer, reason): `args` are bytes at the operator's input
 * schema; the host admits them through the one gate and evaluates. The answer's bytes go to
 * `answer`, or on ZENGINE_OP_ERR_REFUSED the refusal prose to `reason`, never both. A sink, not
 * a buffer: a truncated refusal is a sentence nobody wrote. A null `write` declines that half. */
typedef struct ZengineOperatorHostApiV1 {
    uint32_t abi_version;
    void* ctx;
    ZengineOperatorStatus (*describe)(void* ctx, const char* identity, ZenByteSink sink);
    ZengineOperatorStatus (*evaluate)(void* ctx, const char* identity, const uint8_t* args,
                                      size_t args_len, ZenByteSink answer, ZenByteSink reason);
} ZengineOperatorHostApiV1;

/* The optional surface a consumer image exports: the whole of what makes an artifact
 * operator-aware. One symbol, the version in a field (`zen_weave_abi`'s shape), so a mismatch
 * can be reported: a versioned symbol name gives one bit, and an absent symbol already means an
 * ordinary weave.
 *
 * offer(api): the host hands its table to this image for the instance it is about to create. A
 * null `api` withdraws, and the host always withdraws, so the storage behind this is empty
 * except during one load and two instances each receive their own offer. The consumer checks
 * `api->abi_version` before storing anything and answers ZENGINE_OP_ERR_ABI to a number it does
 * not know. */
typedef struct ZengineOperatorConsumerV1 {
    uint32_t abi_version;
    ZengineOperatorStatus (*offer)(const ZengineOperatorHostApiV1* api);
} ZengineOperatorConsumerV1;

/* The symbol name a host looks up, spelled once: the host writes it as a string, the consumer
 * as an identifier. */
#define ZENGINE_OPERATOR_CONSUMER_SYMBOL "zengine_operator_consumer"

/* The one symbol an operator-aware consumer library exports: a pointer to a static descriptor
 * the host never frees (`zen_weave_abi`'s contract). Absent is the common answer: a weave that
 * never wants an operator exports nothing and loads as it always did. `ZEN_KERNEL_EXPORT` is
 * reused so this declaration and `zen_weave_abi`'s agree on every platform, MinGW included. */
ZEN_KERNEL_EXPORT const ZengineOperatorConsumerV1* zengine_operator_consumer(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ZENGINE_OPERATOR_HOST_ABI_H */
