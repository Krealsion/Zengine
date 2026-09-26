// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_OPERATOR_STRANGER_HPP
#define ZENGINE_TESTS_OPERATOR_STRANGER_HPP

// THE INDEPENDENT CONSUMER -- a reader that knows an operator only by NAME, and the rest by
// asking: it is handed a catalog, an identity and TEXT arguments, reads the ports off the
// operator's own input schema, converts each argument against the kind that schema declares
// (`zen.PokeWrite`'s idiom), and renders what comes back. It names no timer symbol and no timer
// string -- header-only packages leave nothing at link time to enforce that -- so it answers
// only through the catalog somebody else handed it.

#include "operator/catalog.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace stranger {

/// One port, as this reader discovered it.
struct Port {
    std::string name;
    std::string kind; ///< loom's own spelling: "Int", "Bool", ...
};

/// What the reader can say about an operator without evaluating it.
struct Signature {
    bool found = false;
    bool composite = false;
    std::vector<Port> inputs;
    std::vector<Port> outputs;
};

/// What the reader got back.
struct Reading {
    bool ok = false;
    std::string reason;  ///< when !ok — whoever's words the refusal belongs to
    std::string port;    ///< the output port's name, as discovered
    std::string answer;  ///< the output datum, rendered
};

/// Ask the catalog what an operator's ports are.
Signature describe(const zengine::op::Catalog& catalog, std::string_view identity);

/// Evaluate an operator by name, with arguments named and spelled as text.
Reading ask(const zengine::op::Catalog& catalog, std::string_view identity,
            const std::vector<std::pair<std::string, std::string>>& arguments);

} // namespace stranger

#endif // ZENGINE_TESTS_OPERATOR_STRANGER_HPP
