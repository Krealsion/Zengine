// ---- Zengine: C++ that builds EnsureTimer v1, generated from a dropped value ----
// It builds the typed value only: it sends nothing. Nothing was saved or built.
// Needs #include <zen/schema.hpp> and #include <zen/value.hpp> (Loom's loom::core).
// This document lacks <zen/schema.hpp> and <zen/value.hpp>: add them where your includes live.
// The schema below must stay exactly EnsureTimer v1's, or a receiver refuses the value.
inline loom::Value make_ensure_timer_v1() {
    static const std::shared_ptr<const loom::Schema> schema =
        loom::SchemaBuilder("EnsureTimer", 1)
            .field("id", loom::Kind::Text)
            .field("delay_ms", loom::Kind::Int)
            .field("repeat", loom::Kind::Bool)
            .field("preferred", loom::Kind::Text)
            .field("fallback", loom::Kind::Text)
            .build();
    loom::Value value(schema);
    value.set("id", loom::Cell::text("editor-materials.beat"));
    value.set("delay_ms", loom::Cell::integer(250));
    value.set("repeat", loom::Cell::boolean(true));
    value.set("preferred", loom::Cell::text("keep-remaining"));
    value.set("fallback", loom::Cell::text("restart"));
    return value;
}
// ---- end of generated C++ ----
