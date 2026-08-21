// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_OPERATOR_STRANGER_HPP
#define ZENGINE_TESTS_OPERATOR_STRANGER_HPP

// THE INDEPENDENT CONSUMER (SEM-0 §10) — a reader that knows an operator only by
// NAME, and everything else by asking.
//
// WHAT IT KNOWS
//     a catalog                     handed to it
//     an operator identity          handed to it, as a string
//     arguments                     handed to it, as TEXT
//
// WHAT IT DOES NOT KNOW, and cannot: the Timer, `TimerServiceT`, the old
// `clamp_delay`, `timer/normalize.hpp`, the address of any primitive C++
// function, what a delay is, what a millisecond is, or that either of the words
// in `timer.normalize_delay` means anything. It reads the PORTS off the
// operator's own input schema, converts each text argument against the kind that
// schema declares, and renders whatever comes back.
//
// Its whole translation unit includes `operator/` and the standard library, and
// its link line names `zengine-operator` and nothing else. That is the honest
// extent of the fence: these are header-only packages, so nothing at link time
// could stop a later edit from reaching sideways into `timer/` — what can be
// said, and is worth saying, is that this file names no timer symbol and no
// timer string, and that the only reason it can answer at all is the catalog
// somebody else handed it.
//
// TEXT IN, TEXT OUT, on purpose. A consumer that took `std::int64_t` and `bool`
// would already know the signature, which is the thing under test. Converting a
// constant against the port's OWN declared kind is `zen.PokeWrite`'s idiom, and
// it is what makes this reader generic over operators it has never heard of.

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
