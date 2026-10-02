// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_MAKER_FIXTURE_HPP
#define ZENGINE_TESTS_MAKER_FIXTURE_HPP

// HIGH-WATER, AUTHORED AS DATA -- the forcing case the maker package is built for: hw.State v1
// { high }, a trigger on hw.Sample { value } writing high <- math.max(high, value) and emitting
// hw.HighWater { high }. Nothing here is a ZEN_SHAPE or a weave class: shapes come from
// `loom::SchemaBuilder`, the body from `op::Builder` over the host's catalog, and the whole is a
// `maker::Definition`, a Value in native bytes. Shared by the suite and the fresh-process author
// program, because the definition both write must be one definition.

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

/// The schema edit's successor state, `label` FIRST, so a conversion that copied by position
/// would be caught (VM-FIX-05).
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

// ---- the tally, the fold's forcing case ------------------------------------------------------

/// `tally.panel.Count { start, limit, step }`, the message a tally is asked to count.
inline std::shared_ptr<const loom::Schema> count_schema() {
    static const auto s = loom::SchemaBuilder("tally.panel.Count", 1)
                              .field("start", loom::Kind::Int)
                              .field("limit", loom::Kind::Int)
                              .field("step", loom::Kind::Int)
                              .build();
    return s;
}

inline loom::Value count(std::int64_t start, std::int64_t limit, std::int64_t step) {
    loom::Value v(count_schema());
    v.set("start", loom::Cell::integer(start));
    v.set("limit", loom::Cell::integer(limit));
    v.set("step", loom::Cell::integer(step));
    return v;
}

/// The tally: tally.State v1 { total }, a trigger on tally.panel.Count folding math.add from
/// start toward limit by step into total, the count to `rhs` and the accumulator to `lhs`, and the
/// emit tally.Total { total }.
inline maker::Definition tally(const op::Catalog& catalog) {
    maker::Definition d;
    d.name = "tally";
    d.state = loom::SchemaBuilder("tally.State", 1).field("total", loom::Kind::Int).build();
    d.accepts = {count_schema()};
    const auto said = loom::SchemaBuilder("tally.Total", 1).field("total", loom::Kind::Int).build();
    d.emits = {said};
    maker::On on;
    on.message = count_schema();
    op::Builder b(catalog, d.trigger_identity(on), pack_ports(*d.state, *on.message));
    const op::Builder::Ref total = b.fold(op::kAddInt, "rhs", "lhs",
        {b.input("start"), b.input("limit"), b.input("step"), b.constant(std::int64_t{0})});
    on.body = *std::move(b).result("value", total).composition();
    on.output = "total";
    maker::Emit e;
    e.message = said;
    e.fields.push_back(maker::FieldSource{"total", std::string("total"), std::nullopt});
    on.emits.push_back(std::move(e));
    d.on.push_back(std::move(on));
    return d;
}

} // namespace hwfix

#endif // ZENGINE_TESTS_MAKER_FIXTURE_HPP
