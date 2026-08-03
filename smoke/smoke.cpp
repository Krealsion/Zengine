// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Zengine smoke consumer.
//
// This proves ONE thing, and proves it by the stranger's path: a separate repository can take
// the Loom's exported package, link it, and drive a value through the REAL gate. It uses only
// the public surface — no sibling includes, no private headers.
//
// The last check is the one that makes this a proof rather than a greeting: a malformed
// candidate must be REFUSED. A gate that admits everything would pass every other assertion
// here, so the refusal is what shows the gate is real.

#include <zen/zen.hpp>

#include <iostream>
#include <string>

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    std::cout << (ok ? "  ok   " : "  FAIL ") << what << "\n";
    if (!ok) {
        ++failures;
    }
}

}  // namespace

int main() {
    using namespace loom;

    std::cout << "zengine smoke: consuming the Loom as an external repository\n";

    auto schema = SchemaBuilder("SmokeState", 1)
                      .field("hp", Kind::Int)
                      .field("name", Kind::Text)
                      .build();

    Registry registry;
    registry.register_schema(schema);

    Value v(schema);
    v.set("hp", Cell::integer(42)).set("name", Cell::text("zengine"));

    Admission live = admit(Value(v), *schema);
    check(live.ok(), "well-formed value admitted at the gate");

    // A real round trip: native bytes out, Unverified back in, re-admitted through the same
    // gate by resolving the schema claim against the registry.
    const std::string bytes = serialize(v);
    check(bytes.size() > 3 && bytes[0] == 'Z' && bytes[1] == 'N',
          "native bytes carry the ZN magic");

    Unverified candidate = parse(bytes);
    Admission revived = admit(candidate, registry);
    check(revived.ok(), "round-tripped candidate re-admitted through the gate");

    if (revived.ok()) {
        check(revived.value().get("hp")->as_int() == 42, "hp survived the round trip");
        check(revived.value().get("name")->as_text() == "zengine",
              "name survived the round trip");
    } else {
        std::cout << "  (refused: " << revived.first_error().message() << ")\n";
    }

    // The gate is a gate, not a pass-through: malformed input is refused, never repaired.
    Unverified corrupt = compat::parse(
        R"({"zen":1,"schema":"SmokeState","version":1,"fields":{"hp":"not-an-int"}})");
    Admission refused = admit(corrupt, registry);
    check(!refused.ok(), "malformed candidate REFUSED by the gate");

    std::cout << (failures == 0 ? "SMOKE PASS" : "SMOKE FAIL") << " (" << failures
              << " failure(s))\n";
    return failures == 0 ? 0 : 1;
}
