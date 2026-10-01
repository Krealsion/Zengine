// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// A provider whose bytes are `zengine.OperatorContribution` version 1, which carries no
// description and which a host still reads. It fills the provider table by hand rather than with
// `ZENGINE_OPERATOR_PROVIDER`, whose describe writes the current version, and spends its one
// native power through `ProviderDefinitions`. Not a weave; it supplies one power.

#include "operator/operator.hpp"
#include "operator/provider.hpp"
#include "operator/provider_abi.h"

#include <zen/serialize.hpp>
#include <zen/value.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace {

std::int64_t twice(std::int64_t value) { return value * 2; }

/// Namespace scope, as the macro's would be: one authoring, held for the image's life.
const zengine::op::ProviderDefinitions& authored() {
    static const zengine::op::ProviderDefinitions defs([] {
        std::vector<zengine::op::OperatorDef> out;
        out.push_back(zengine::op::make_operator<&twice>("prov.v1.twice", {"value"}, "result"));
        return out;
    }());
    return defs;
}

/// The contribution at version 1: every field version 1 declares, read off the current encoding,
/// under version 1's own schema, so the bytes are what a provider of that time wrote.
std::string v1_bytes() {
    const loom::Value now = zengine::op::encode_contribution(
        zengine::op::make_operator<&twice>("prov.v1.twice", {"value"}, "result"));
    loom::Value then(zengine::op::operator_contribution_v1_schema());
    for (const loom::Field& field : then.schema().fields()) {
        if (const loom::Cell* cell = now.get(field.name); cell != nullptr) {
            then.set(field.name, *cell);
        }
    }
    return loom::serialize(then);
}

} // namespace

extern "C" {

static ZengineOperatorStatus v1_describe(void*, uint32_t index, ZenByteSink sink) {
    if (index != 0) {
        return ZENGINE_OP_ERR_NOT_FOUND;
    }
    try {
        const std::string bytes = v1_bytes();
        if (sink.write != nullptr) {
            sink.write(sink.ctx, reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
        }
        return ZENGINE_OP_OK;
    } catch (...) {
        return ZENGINE_OP_ERR_PROVIDER_FAILED;
    }
}

static ZengineOperatorStatus v1_invoke(void*, uint32_t index, const uint8_t* args,
                                       size_t args_len, ZenByteSink answer, ZenByteSink reason) {
    return authored().invoke(index, args, args_len, answer, reason);
}

ZEN_KERNEL_EXPORT const ZengineOperatorProviderV1* zengine_operator_provider(void) {
    static const ZengineOperatorProviderV1 table = [] {
        ZengineOperatorProviderV1 t{};
        t.abi_version = ZENGINE_OPERATOR_PROVIDER_ABI_VERSION;
        t.ctx = nullptr;
        t.identity = "zengine.provider.v1";
        t.count = 1u;
        t.describe = &v1_describe;
        t.invoke = &v1_invoke;
        return t;
    }();
    return &table;
}

} // extern "C"
