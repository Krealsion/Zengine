// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_SOURCE_HPP
#define ZENGINE_OPERATOR_SOURCE_HPP

// A SOURCE IS A PROJECTION OVER THE ONE CATALOG, NOT A SPECIES (SOURCE-0).
//
//     Operator   one or more unbound maker inputs   evaluated on YOUR arguments
//     Source     zero unbound maker inputs          evaluated on its own subject
//
// That is the whole definition, and it is a QUESTION ASKED OF A SHAPE rather than a
// kind of thing:
//
//     is_source(def)  <=>  def.inputs()->fields().empty()
//
// So there is no `SourceDef`, no `SourceCatalog`, no Source ABI, no Source provider
// protocol and no Source runtime. A zero-input native getter and a fully-bound
// composite are both ordinary `OperatorDef`s -- their difference stays exactly where
// it already was, behind `is_composite()` -- and a partially-bound composite is an
// Operator until the last maker input is gone. One store holds both, which is why
// binding an operator's last input can turn it into a Source with nothing
// re-registered.
//
// IT CLASSIFIES BY SHAPE AND NEVER BY NAME. An identity spelled `source.*` is not a
// Source and an identity spelled `math.add` could be one; what decides is whether a
// caller would have to supply anything, which the input schema already says.
//
// ---- ROUTING IS NOT EVALUATION, and that is the load-bearing half --------------
//
// Registration, mount, `find`, enumeration, description, schema inspection,
// provenance inspection and the classification above all touch a DEFINITION and never
// a body. `invoke_native` has exactly one caller -- `Catalog::run`, past admission --
// so "knowing about a Source does not sample it" is a fact about the call graph
// rather than a discipline anybody has to keep. `describe_powers` already says the
// same sentence one package out: running one to find out would be a side effect in a
// view.
//
// ---- WHAT `sample` ADDS, AND WHAT IT DELIBERATELY DOES NOT --------------------
//
// Sampling a Source through `Catalog::evaluate` by hand costs a `find` FIRST, purely
// to obtain the empty input schema the pack has to claim -- ceremony with a wrong
// answer available at every step. `sample` is that ceremony, once:
//
//     sample(catalog, "zengine.project.anchor")
//
// It resolves current catalog truth at the spend, refuses an identity nobody supplies
// IN THE CATALOG'S OWN WORDS, refuses a parameterized Operator in its own, builds the
// empty pack from the Source's OWN current input schema, and then spends the one
// evaluator. It adds no evaluation policy, no second gate, no third registration
// door and no cache: it holds no provider, no definition pointer, no callable and no
// answer between calls, so a repeated sample resolves current truth again.
//
// AND IT PROMISES NOTHING ABOUT FRESHNESS beyond the sentence a sample can honestly
// say: this is what this Source returned when it was explicitly sampled. There is no
// timestamp, no polling, no subscription, no watcher, no dirty flag and no cached
// "current value" -- and nothing in this package has anywhere to hide one, because
// nothing here retains anything at all.

#include "operator/catalog.hpp"
#include "operator/operator.hpp"

#include <zen/schema.hpp>
#include <zen/value.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::op {

/// Does spending this definition require anything of a maker?
///
/// The zero-field input schema is the enforcement and not merely the label: a Source
/// with a smuggled port would BE an operator, and the gate would refuse a pack that
/// did not carry it. Nothing derives this from an identity, a naming convention, a
/// registration door or an implementation technique.
inline bool is_source(const OperatorDef& def) noexcept { return def.inputs()->fields().empty(); }

namespace detail {

/// The pack handed to `evaluate` for an identity nothing supplies.
///
/// It is never admitted -- `evaluate` resolves before it gates, so an unresolved
/// identity is refused before this value is looked at -- and it exists only because
/// `loom::Value` has no shapeless form. It is deliberately NOT what a resolved Source
/// is sampled with: a real sample claims the Source's OWN input schema, which is a
/// different shape with a different content id even when both are empty.
inline const std::shared_ptr<const loom::Schema>& unresolvable_pack() {
    static const std::shared_ptr<const loom::Schema> s =
        loom::make_schema("zengine.UnresolvedSample", 1, std::vector<loom::Field>());
    return s;
}

/// `lhs, rhs` -- what a refusal has to name so it is not merely a complaint.
inline std::string port_list(const loom::Schema& schema) {
    std::string named;
    for (const loom::Field& f : schema.fields()) {
        named += named.empty() ? "" : ", ";
        named += f.name;
    }
    return named;
}

} // namespace detail

/// EVALUATE ONE SOURCE, NOW, and answer with the ordinary inert value it produces.
///
/// The three things it can say, and who owns each sentence:
///
///     nothing supplies that identity     `Catalog::evaluate`, quoted by resolving
///                                        again rather than by re-wording it here
///     that is an Operator, not a Source  this file -- the one failure mode the
///                                        Source seam actually owns
///     the answer, or the gate's reason   `Catalog::evaluate`, unchanged
///
/// WHAT COMES BACK IS INERT. It is the same schema-admitted `loom::Value` any
/// operator evaluation yields: it owns no authority, performs nothing later, is not a
/// live binding and is not a subscription. Scribbling on it reaches nobody, and it
/// carries no claim about the world past the moment it was produced.
inline Evaluation sample(const Catalog& catalog, std::string_view identity) {
    const OperatorDef* def = catalog.find(identity);
    if (def == nullptr) {
        // NOT THIS FILE'S SENTENCE TO SAY. The catalog already has words for an
        // identity nobody supplies, and a second wording of one refusal is a second
        // answer waiting for one of them to be edited -- the reason `evaluate` has two
        // entrances and one body. So the identity is spent, refused where it is
        // detected, and quoted verbatim.
        return catalog.evaluate(identity, loom::Value(detail::unresolvable_pack()));
    }
    if (!is_source(*def)) {
        // NAMED, NEVER GUESSED. The alternative to this refusal is manufacturing
        // arguments a maker never wrote -- a default, a zero, an empty string -- which
        // is an answer nobody authored to a question nobody asked.
        const std::vector<loom::Field>& ports = def->inputs()->fields();
        return Evaluation::refuse("'" + std::string(identity) +
                                  "' is an operator and not a source: it declares " +
                                  std::to_string(ports.size()) +
                                  (ports.size() == 1 ? " input (" : " inputs (") +
                                  detail::port_list(*def->inputs()) +
                                  "), which sampling supplies none of");
    }
    // THE SOURCE'S OWN CURRENT INPUT SCHEMA, read at the spend. An empty schema still
    // has an IDENTITY -- a name, a version and the content id the gate compares -- so
    // a generic empty pack is not interchangeable with this one and would be refused
    // by the very door it was aimed at.
    return catalog.evaluate(identity, loom::Value(def->inputs()));
}

} // namespace zengine::op

#endif // ZENGINE_OPERATOR_SOURCE_HPP
