// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_TESTS_SOURCE_TRANSFER_SAMPLES_HPP
#define ZENGINE_TESTS_SOURCE_TRANSFER_SAMPLES_HPP

// THE VALUE WHOSE STRINGS A PLAIN C++ LITERAL CANNOT KEEP -- one definition, read by suite
// `source_transfer` (which pins the generator's exact output for it as
// `source_transfer_string_bytes.generated.hpp`) and by program `source_transfer_cpp` (which compiles
// that output, calls it and compares its schema and bytes with this value). Every string here is
// built with its length spelled out, never through a literal operator, so it does not share the
// mechanism the generated code uses.
//
// Loom's schema admits these names (only an empty or a repeated field name is refused), so the
// generator has to carry them: a line break and a NUL in the schema's name, a NUL in a field's.

#include <zen/schema.hpp>
#include <zen/value.hpp>

#include <memory>
#include <string>
#include <string_view>

namespace source_transfer_samples {

inline std::shared_ptr<const loom::Schema> string_bytes_schema() {
    static const auto schema = loom::SchemaBuilder(std::string("Editor\nMaterials\0Bytes", 22), 1)
                                   .field("lead", loom::Kind::Text)
                                   .field("inner", loom::Kind::Text)
                                   .field("tail", loom::Kind::Text)
                                   .field("octal", loom::Kind::Text)
                                   .field("neighbours", loom::Kind::Text)
                                   .field("only", loom::Kind::Text)
                                   .field("plain", loom::Kind::Text)
                                   .field("empty", loom::Kind::Text)
                                   .field(std::string("na\0me", 5), loom::Kind::Text)
                                   .field("raw", loom::Kind::Bytes)
                                   .build();
    return schema;
}

inline loom::Value string_bytes_sample() {
    loom::Value v(string_bytes_schema());
    v.set("lead", loom::Cell::text(std::string("\0lead", 5)));
    v.set("inner", loom::Cell::text(std::string("A\0B", 3))); // the review's three bytes
    v.set("tail", loom::Cell::text(std::string("tail\0", 5)));
    // A NUL before an octal digit, before a digit that is not one, twice in a row, and before `0`:
    // an escape that took fewer than three digits would swallow the character after it.
    v.set("octal", loom::Cell::text(std::string("\0" "7" "\0" "8" "\0" "\0" "0", 7)));
    // A NUL beside every byte the escaping treats specially, a UTF-8 character, a comment's end
    // and a trigraph's spelling (`?\?=` is `??=`, written so that this file does not form one).
    v.set("neighbours", loom::Cell::text(std::string("\"" "\0" "\\" "\n" "\0" "caf\xc3\xa9" "\0" "*/?\?=", 16)));
    v.set("only", loom::Cell::text(std::string(3, '\0')));
    v.set("plain", loom::Cell::text("no NUL here"));
    v.set("empty", loom::Cell::text(std::string()));
    v.set(std::string_view("na\0me", 5), loom::Cell::text("x"));
    v.set("raw", loom::Cell::bytes(loom::Bytes{0, 1, 0, 255}));
    return v;
}

} // namespace source_transfer_samples

#endif
