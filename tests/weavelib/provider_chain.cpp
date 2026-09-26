// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The three-level provider witness: `prov.function.1` = f2(f2(x)) names only function.2,
// `prov.function.2` = f3(f3(x)) names only function.3, and `prov.function.3` = x * 2 is the only
// code. When another provider covers function.3, function.1 changes with nothing rewritten or
// rebound. One source, four libraries: a, the chain; b, function.3 alone as x + 100 at the same
// signature, a test of resolution; b-wrong, that name at another signature, which an overlay must
// refuse; abi, a surface version the host does not speak. Not a weave: no Kernel loads it.

#include "operator/catalog.hpp"
#include "operator/operator.hpp"
#include "operator/primitives.hpp"
#include "operator/provider.hpp"

#include <zen/kind.hpp>
#include <zen/schema.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace {

namespace op = zengine::op;

constexpr const char* kF1 = "prov.function.1";
constexpr const char* kF2 = "prov.function.2";
constexpr const char* kF3 = "prov.function.3";
constexpr const char* kPort = "value";
constexpr const char* kResult = "result";

#if defined(PROV_CHAIN_A)
/// Provider A's leaf. Namespace scope, because a block-scope lambda cannot be a
/// `make_operator<&F>` argument at all.
std::int64_t doubled(std::int64_t value) { return value * 2; }

/// One composite: `inner(inner(x))`, whose graph names `inner` and nothing else.
op::OperatorDef twice(const op::Catalog& against, const char* identity, const char* inner) {
    op::Builder rule(against, identity,
                     {loom::Field{kPort, loom::type_of(loom::Kind::Int), true}});
    const op::Builder::Ref once = rule.call(inner, {rule.input(kPort)});
    const op::Builder::Ref again = rule.call(inner, {once});
    return std::move(rule).result(kResult, again);
}
#elif defined(PROV_CHAIN_B)
/// Provider B's leaf: the SAME power, differently. An add rather than a multiply, so
/// no arrangement of the chain can produce one provider's numbers by accident.
std::int64_t plus_hundred(std::int64_t value) { return value + 100; }
#elif defined(PROV_CHAIN_WRONG)
/// A different power wearing the same name.
bool nonempty(const std::string& value) { return !value.empty(); }
#endif

#if !defined(PROV_CHAIN_ABI)
std::vector<op::OperatorDef> chain() {
    std::vector<op::OperatorDef> defs;
#if defined(PROV_CHAIN_WRONG)
    defs.push_back(op::make_operator<&nonempty>(kF3, {kPort}, kResult));
#elif defined(PROV_CHAIN_B)
    defs.push_back(op::make_operator<&plus_hundred>(kF3, {kPort}, kResult));
#else
    // THE AUTHORING CATALOG IS SCAFFOLDING and dies at the closing brace. Every
    // step is resolved and signature-snapshotted as it is written, which is what
    // lets each composite's OUTPUT type be derived rather than declared -- and
    // what it leaves behind is a graph of identities, not a graph of pointers.
    op::Catalog against;
    op::OperatorDef f3 = op::make_operator<&doubled>(kF3, {kPort}, kResult);
    against.publish(f3);
    op::OperatorDef f2 = twice(against, kF2, kF3);
    against.publish(f2);
    op::OperatorDef f1 = twice(against, kF1, kF2);
    defs.push_back(std::move(f3));
    defs.push_back(std::move(f2));
    defs.push_back(std::move(f1));
#endif
    return defs;
}
#endif

} // namespace

#if defined(PROV_CHAIN_ABI)
extern "C" {
static ZengineOperatorStatus prov_stale_describe(void*, uint32_t, ZenByteSink) {
    return ZENGINE_OP_ERR_PROVIDER_FAILED;
}
static ZengineOperatorStatus prov_stale_invoke(void*, uint32_t, const uint8_t*, size_t,
                                               ZenByteSink, ZenByteSink) {
    return ZENGINE_OP_ERR_PROVIDER_FAILED;
}
/// A NON-EMPTY, NON-NULL TABLE at a version this host does not speak, and that is
/// the sharp part: `count` is one and both verbs are real function pointers, so the
/// ONLY thing wrong with this artifact is the number. A host that read past the
/// version would get as far as calling `describe`, and the refusal would then name
/// something other than the version -- which is exactly what the witness watches
/// for.
ZEN_KERNEL_EXPORT const ZengineOperatorProviderV1* zengine_operator_provider(void) {
    static const ZengineOperatorProviderV1 table = {
        ZENGINE_OPERATOR_PROVIDER_ABI_VERSION + 1u, nullptr, "zengine.provider.from.another.era",
        1u, prov_stale_describe, prov_stale_invoke};
    return &table;
}
}
#elif defined(PROV_CHAIN_WRONG)
ZENGINE_OPERATOR_PROVIDER("zengine.provider.b.wrong", chain)
#elif defined(PROV_CHAIN_B)
ZENGINE_OPERATOR_PROVIDER("zengine.provider.b", chain)
#else
ZENGINE_OPERATOR_PROVIDER("zengine.provider.a", chain)
#endif
