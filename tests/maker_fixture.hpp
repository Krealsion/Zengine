// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_MAKER_FIXTURE_HPP
#define ZENGINE_TESTS_MAKER_FIXTURE_HPP

// HIGH-WATER, AUTHORED AS DATA -- the forcing case the maker package is built for, and the one
// witness a Workshop editor will one day produce with no C++ at all.
//
// Nothing here is a ZEN_SHAPE and nothing here is a weave class of its own: the maker's shapes
// are built through `loom::SchemaBuilder`, the trigger's body through `op::Builder` over the
// host's catalog and carried as the composition wire form, and the whole thing is a
// `maker::Definition` -- which is to say a Value, encoded to native bytes and read back by the
// interpreter. Shared by the suite and by the fresh-process author program, because the
// definition both write must be one definition.
//
//   hw.State v1     { high : Int }
//   hw.Sample v1    { value : Int }               the accepted message
//   hw.HighWater v1 { high : Int }                the emitted message
//   on hw.Sample    high <- math.max(high, value);  emit hw.HighWater { high <- high }
//
//   hw.State v2     { label : Text, high : Int }  the schema edit's successor -- `label` FIRST,
//                                                 so a conversion that copied by position would
//                                                 be caught (VM-FIX-05)
//   conversion      high <- high; label <- "high water"

#include "maker/definition.hpp"
#include "maker/write.hpp"
#include "operator/catalog.hpp"
#include "operator/primitives.hpp"

#include <zen/kind.hpp>
#include <zen/schema.hpp>
#include <zen/value.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace hwfix {

namespace op = zengine::op;
namespace maker = zengine::maker;

inline std::shared_ptr<const loom::Schema> sample_schema() {
    static const auto s =
        loom::SchemaBuilder("hw.Sample", 1).field("value", loom::Kind::Int).build();
    return s;
}

inline std::shared_ptr<const loom::Schema> high_water_schema() {
    static const auto s =
        loom::SchemaBuilder("hw.HighWater", 1).field("high", loom::Kind::Int).build();
    return s;
}

inline std::shared_ptr<const loom::Schema> state_v1() {
    static const auto s = loom::SchemaBuilder("hw.State", 1).field("high", loom::Kind::Int).build();
    return s;
}

inline std::shared_ptr<const loom::Schema> state_v2() {
    static const auto s = loom::SchemaBuilder("hw.State", 2)
                              .field("label", loom::Kind::Text)
                              .field("high", loom::Kind::Int)
                              .build();
    return s;
}

/// A sample message carrying `value`.
inline loom::Value sample(std::int64_t value) {
    loom::Value v(sample_schema());
    v.set("value", loom::Cell::integer(value));
    return v;
}

/// The pack's ports -- the state's fields, then the message's -- as the Builder's inputs.
inline std::vector<loom::Field> pack_ports(const loom::Schema& state, const loom::Schema& message) {
    std::vector<loom::Field> ports;
    for (const loom::Field& f : state.fields()) {
        ports.push_back(f);
    }
    for (const loom::Field& f : message.fields()) {
        ports.push_back(f);
    }
    return ports;
}

/// `high <- <operator>(high, value)`, authored over the catalog as a composition.
inline op::Composite two_arg_body(const op::Catalog& catalog, const std::string& identity,
                                  const loom::Schema& state, const loom::Schema& message,
                                  const char* operator_identity) {
    op::Builder b(catalog, identity, pack_ports(state, message));
    op::Builder::Ref answer = b.call(operator_identity, {b.input("high"), b.input("value")});
    const op::OperatorDef def = std::move(b).result("value", answer);
    return *def.composition();
}

/// The one emit: hw.HighWater { high <- high }.
inline maker::Emit high_water_emit() {
    maker::Emit e;
    e.message = high_water_schema();
    e.fields.push_back(maker::FieldSource{"high", std::string("high"), std::nullopt});
    return e;
}

/// High-water, revision `revision`, its trigger's body over `operator_identity` (math.max by
/// default; a behaviour edit passes another).
inline maker::Definition high_water(const op::Catalog& catalog, std::int64_t revision = 1,
                                    const char* operator_identity = op::kMaxInt) {
    maker::Definition d;
    d.name = "hw";
    d.revision = revision;
    d.state = state_v1();
    d.accepts = {sample_schema()};
    d.emits = {high_water_schema()};
    maker::On on;
    on.message = sample_schema();
    on.body = two_arg_body(catalog, d.trigger_identity(on), *d.state, *on.message, operator_identity);
    on.output = "high";
    on.emits.push_back(high_water_emit());
    d.on.push_back(std::move(on));
    return d;
}

/// The schema edit's successor: hw.State v2 with `label`, the same trigger, and the conversion
/// from v1 -- `high` copied, `label` written from a Text constant, nothing dropped.
inline maker::Definition high_water_v2(const op::Catalog& catalog, std::int64_t revision = 2) {
    maker::Definition d;
    d.name = "hw";
    d.revision = revision;
    d.state = state_v2();
    d.accepts = {sample_schema()};
    d.emits = {high_water_schema()};
    maker::On on;
    on.message = sample_schema();
    on.body = two_arg_body(catalog, d.trigger_identity(on), *d.state, *on.message, op::kMaxInt);
    on.output = "high";
    on.emits.push_back(high_water_emit());
    d.on.push_back(std::move(on));
    maker::Conversion c;
    c.from = state_v1();
    c.fields.push_back(maker::FieldSource{"high", std::string("high"), std::nullopt});
    c.fields.push_back(maker::FieldSource{"label", std::nullopt, loom::Cell::text("high water")});
    d.conversion = std::move(c);
    return d;
}

} // namespace hwfix

#endif // ZENGINE_TESTS_MAKER_FIXTURE_HPP
