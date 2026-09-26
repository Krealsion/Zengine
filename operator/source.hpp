// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_SOURCE_HPP
#define ZENGINE_OPERATOR_SOURCE_HPP

// A Source is a reading of the one catalog, not a species: an operator with zero unbound maker
// inputs, evaluated on its own subject rather than on your arguments. `is_source(def)` is
// `def.inputs()->fields().empty()`, a question asked of a shape and never of a name. A zero-input
// native getter and a fully-bound composite are both ordinary `OperatorDef`s in one store, so
// binding an operator's last input turns it into a Source with nothing re-registered.
// Reference: docs/reference/operator-sources.md.

// Routing is not evaluation: registration, mount, `find`, enumeration, description, schema and
// provenance inspection and `is_source` touch a definition, never a body -- `invoke_native` has
// one caller, `Catalog::run`, past admission. `sample` removes the ceremony of sampling by hand
// and adds nothing: no policy, second gate, registration door or cache, and no promise about
// freshness beyond "this is what the Source returned when it was sampled".

#include "operator/catalog.hpp"
#include "operator/operator.hpp"

#include <zen/schema.hpp>
#include <zen/value.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::op {

/// Does spending this definition require anything of a maker? The zero-field input schema is the
/// enforcement, not a label: a Source with a smuggled port would be an operator, and the gate
/// would refuse a pack that did not carry it.
inline bool is_source(const OperatorDef& def) noexcept { return def.inputs()->fields().empty(); }

namespace detail {

/// The pack handed to `evaluate` for an identity nothing supplies: never admitted (`evaluate`
/// resolves before it gates), and here only because `loom::Value` has no shapeless form. A
/// resolved Source is sampled with its own input schema, a different content id even when both
/// are empty.
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

/// Evaluate one Source, now, and answer with the ordinary inert value it produces. An identity
/// nothing supplies, the gate's reason and the answer are `Catalog::evaluate`'s words, quoted;
/// "an operator, not a source" is this file's one sentence. The value owns no authority,
/// performs nothing later, is no live binding or subscription, and claims nothing about the
/// world past the moment it was produced.
inline Evaluation sample(const Catalog& catalog, std::string_view identity) {
    const OperatorDef* def = catalog.find(identity);
    if (def == nullptr) {
        // Not this file's sentence: the catalog has words for an identity nobody supplies, so
        // the identity is spent, refused where it is detected, and quoted verbatim.
        return catalog.evaluate(identity, loom::Value(detail::unresolvable_pack()));
    }
    if (!is_source(*def)) {
        // Named, never guessed: the alternative is manufacturing arguments a maker never
        // wrote, an answer nobody authored.
        const std::vector<loom::Field>& ports = def->inputs()->fields();
        return Evaluation::refuse("'" + std::string(identity) +
                                  "' is an operator and not a source: it declares " +
                                  std::to_string(ports.size()) +
                                  (ports.size() == 1 ? " input (" : " inputs (") +
                                  detail::port_list(*def->inputs()) +
                                  "), which sampling supplies none of");
    }
    // The Source's own current input schema, read at the spend: an empty schema still has an
    // identity the gate compares, so a generic empty pack would be refused by the very door it
    // was aimed at.
    return catalog.evaluate(identity, loom::Value(def->inputs()));
}

} // namespace zengine::op

#endif // ZENGINE_OPERATOR_SOURCE_HPP
