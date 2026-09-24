// ---- Zengine: C++ that builds Editor\012Materials\000Bytes v1, generated from a dropped value ----
// It builds the typed value only: it sends nothing. Nothing was saved or built.
// Needs #include <zen/schema.hpp> and #include <zen/value.hpp> (Loom's loom::core).
// Needs #include <string> too: a string holding a NUL is written "..."s, which keeps its length.
// This document lacks <zen/schema.hpp>, <zen/value.hpp> and <string>: add them where your includes live.
// The schema below must stay exactly Editor\012Materials\000Bytes v1's, or a receiver refuses the value.
inline loom::Value make_editor_materials_bytes_v1() {
    using namespace std::string_literals;
    static const std::shared_ptr<const loom::Schema> schema =
        loom::SchemaBuilder("Editor\012Materials\000Bytes"s, 1)
            .field("lead", loom::Kind::Text)
            .field("inner", loom::Kind::Text)
            .field("tail", loom::Kind::Text)
            .field("octal", loom::Kind::Text)
            .field("neighbours", loom::Kind::Text)
            .field("only", loom::Kind::Text)
            .field("plain", loom::Kind::Text)
            .field("empty", loom::Kind::Text)
            .field("na\000me"s, loom::Kind::Text)
            .field("raw", loom::Kind::Bytes)
            .build();
    loom::Value value(schema);
    value.set("lead", loom::Cell::text("\000lead"s));
    value.set("inner", loom::Cell::text("A\000B"s));
    value.set("tail", loom::Cell::text("tail\000"s));
    value.set("octal", loom::Cell::text("\0007\0008\000\0000"s));
    value.set("neighbours", loom::Cell::text("\"\000\\\012\000caf\303\251\000*/?\?="s));
    value.set("only", loom::Cell::text("\000\000\000"s));
    value.set("plain", loom::Cell::text("no NUL here"));
    value.set("empty", loom::Cell::text(""));
    value.set("na\000me"s, loom::Cell::text("x"));
    value.set("raw", loom::Cell::bytes(loom::Bytes{0, 1, 0, 255}));
    return value;
}
// ---- end of generated C++ ----
