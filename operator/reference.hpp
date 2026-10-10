// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_REFERENCE_HPP
#define ZENGINE_OPERATOR_REFERENCE_HPP

// An operator reference as a typed value: an identity and the two content ids it was found at,
// `zengine.OperatorRef v1`, which a person carries, Inventory keeps and a composer adds as a
// step. It is what a step holds, and it grants nothing: only the evaluator spends one, resolving
// it at the spend. A reference whose operator now has other ports is stale and is refused, never
// re-bound to whatever bears the name. It names no catalog, so a pane that only shows and carries
// references links no operator target. Reference: operator/docs/operator-providers.md.

#include <zen/gate.hpp>
#include <zen/schema.hpp>
#include <zen/value.hpp>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace zengine::op {

/// The identity and the signature it was found at, the thing a step is authored against.
struct OperatorRef {
    std::string identity;
    loom::ContentId authored_in = 0;
    loom::ContentId authored_out = 0;
};

/// `zengine.OperatorRef v1`. The content ids are Loom Ints, as a step's are: compared, never
/// ordered, and spelled as integers at a Terminal.
inline std::shared_ptr<const loom::Schema> operator_ref_schema() {
    static const auto s = loom::SchemaBuilder("zengine.OperatorRef", 1)
                              .field("identity", loom::Kind::Text)
                              .field("authored_in", loom::Kind::Int)
                              .field("authored_out", loom::Kind::Int)
                              .build();
    return s;
}

inline loom::Value encode_reference(const OperatorRef& ref) {
    loom::Value v(operator_ref_schema());
    v.set("identity", loom::Cell::text(ref.identity));
    v.set("authored_in", loom::Cell::integer(static_cast<std::int64_t>(ref.authored_in)));
    v.set("authored_out", loom::Cell::integer(static_cast<std::int64_t>(ref.authored_out)));
    return v;
}

/// Is this value a reference? Its schema, by identity, and nothing in its fields.
inline bool is_reference(const loom::Value& v) {
    return loom::same_identity(v.schema(), *operator_ref_schema());
}

/// A reference back out of a value of its schema; anything else is refused in the gate's words.
inline OperatorRef decode_reference(const loom::Value& v) {
    const loom::Admission admitted = loom::admit(v, *operator_ref_schema());
    if (!admitted) {
        throw std::invalid_argument("not an operator reference: " +
                                    admitted.first_error().message());
    }
    const loom::Value& ok = admitted.value();
    return OperatorRef{ok.get("identity")->as_text(),
                       static_cast<loom::ContentId>(ok.get("authored_in")->as_int()),
                       static_cast<loom::ContentId>(ok.get("authored_out")->as_int())};
}

/// The two ways a reference is stale, in the words every composer says them.
inline std::string unsupplied_reason(const std::string& identity) {
    return "nothing supplies '" + identity + "' here now";
}
inline std::string reshaped_reason(const std::string& identity) {
    return "'" + identity +
           "' is not the operator this reference was found at: its ports changed since; find it "
           "again";
}

} // namespace zengine::op

#endif // ZENGINE_OPERATOR_REFERENCE_HPP
