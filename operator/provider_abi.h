// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_PROVIDER_ABI_H
#define ZENGINE_OPERATOR_PROVIDER_ABI_H

/*
 * The operator provider seam: how a native image says "I provide these powers" to a host that
 * owns which of them is currently in force. The opposite arrow from host_abi.h, where a
 * consumer asks a host to spend the host's truth; an artifact may export either table, both
 * (the Timer consumes the host's rule and supplies it) or neither. A provider need not be a
 * weave: a host that opened the file resolves this table, with no Kernel, WeaveId, role, grant
 * or bus involved.
 * Reference: docs/reference/operator-providers.md.
 *
 * describe(index, sink) emits `zengine.OperatorContribution v1` bytes for contribution `index`:
 * the identity, both port schemas with the closure of what they nest, and for a composition
 * the graph itself, so its nodes still name `math.max` on the far side and resolve against
 * whatever provides it. invoke(index, args, args_len, answer, reason) spends a native
 * contribution; a composite is never invoked here, since the host holds its graph.
 *
 * The index is provider-local and transient, never an operator's durable meaning (that is the
 * identity and the two port schemas): it reaches the code while a host holds this image's
 * record. Invoking by index costs what a raw function pointer costs, so no callable accessor
 * exists. The conventions are `zen/kernel/abi.h`'s, as in host_abi.h; no exception, STL type,
 * `op::Catalog`, `OperatorDef` or callable crosses.
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

/* This seam's own version, distinct from ZEN_ABI_VERSION, ZENGINE_OPERATOR_ABI_VERSION and any
 * schema version. The reader checks it: a host meeting a provider built against another
 * version refuses the mount and names both numbers. An absent symbol means an artifact that
 * provides no operators, so the two facts do not arrive as the same silence. */
#define ZENGINE_OPERATOR_PROVIDER_ABI_VERSION 1u

/* What a provider image offers a host. `identity` is the provider's logical name,
 * NUL-terminated and valid for the image's lifetime; it is not part of an operator's identity
 * (`math.max` is `math.max` whoever supplies it), and it is what mounting, unmounting and
 * reporting the active contribution use. `count` contributions have indices `[0, count)`; a
 * host refuses a count of zero, the only way a provider's failed authoring shows across a C
 * seam. Neither verb touches a bus. */
typedef struct ZengineOperatorProviderV1 {
    uint32_t abi_version;
    void* ctx;
    const char* identity;
    uint32_t count;
    ZengineOperatorStatus (*describe)(void* ctx, uint32_t index, ZenByteSink sink);
    ZengineOperatorStatus (*invoke)(void* ctx, uint32_t index, const uint8_t* args,
                                    size_t args_len, ZenByteSink answer, ZenByteSink reason);
} ZengineOperatorProviderV1;

/* The symbol name a host looks up, spelled once: the host writes it as a string, the provider
 * as an identifier. */
#define ZENGINE_OPERATOR_PROVIDER_SYMBOL "zengine_operator_provider"

/* The one symbol an operator-providing library exports: a pointer to a static descriptor the
 * host never frees (`zen_weave_abi`'s contract). Absent is the common answer. */
ZEN_KERNEL_EXPORT const ZengineOperatorProviderV1* zengine_operator_provider(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ZENGINE_OPERATOR_PROVIDER_ABI_H */
