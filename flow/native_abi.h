// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_FLOW_NATIVE_ABI_H
#define ZENGINE_FLOW_NATIVE_ABI_H

#include "operator/host_abi.h"

// A generated artifact also exports the ordinary Loom weave and operator-consumer
// surfaces. This extra surface supplies its retained definition and compiled bodies.
// No C++ object or ownership crosses it. The host table is borrowed for one call.
#define ZENGINE_FLOW_ABI_VERSION 1u
typedef struct ZengineFlowArtifactV1 {
    uint32_t abi_version;
    ZengineOperatorStatus (*definition)(ZenByteSink bytes);
    ZengineOperatorStatus (*execute)(size_t trigger,
        const ZengineOperatorHostApiV1* steps, const uint8_t* arguments, size_t size,
        ZenByteSink answer, ZenByteSink reason);
} ZengineFlowArtifactV1;

typedef const ZengineFlowArtifactV1* (*ZengineFlowEntryV1)(void);

#endif
