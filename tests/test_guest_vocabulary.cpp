// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// THE GUEST VOCABULARY WEAVE (external-host/vocabulary_weave.hpp): what an EXTERNAL Loom host boots
// so a session's Python tools can speak Workshop's guest vocabulary through a link. It must make
// exactly the shapes Workshop's owners speak resolvable -- by identity, not by name -- say nothing,
// refuse a divergent definition at the wall, and take the shapes with it when it leaves.

#include "doctest.h"

// A shared_ptr is compared through `.get()` inside these macros: doctest decomposes the
// expression and MSVC then has no way to print a shared_ptr, which is a compile error there.

#include "external-host/vocabulary_weave.hpp"
#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"
#include "workshop/guest_seam_vocabulary.hpp"

#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <memory>
#include <string>
#include <vector>

namespace {

using zengine::external_host::GuestVocabulary;

std::vector<std::shared_ptr<const loom::Schema>> guest_shapes() {
    namespace in = zengine::input;
    namespace su = zengine::surface;
    namespace ws = zengine::workshop;
    return {loom::schema_of<in::InputSessionRequested>(), loom::schema_of<in::InputSessionOpened>(),
            loom::schema_of<in::PointerMotionRequested>(), loom::schema_of<ws::PaneViewRequested>(),
            loom::schema_of<ws::PaneView>(), loom::schema_of<ws::PaneViewRow>(),
            loom::schema_of<in::InjectInput>(),           loom::schema_of<in::InjectedEvent>(),
            loom::schema_of<in::InputInjected>(),         loom::schema_of<in::InputSessionClosed>(),
            loom::schema_of<su::SurfaceCaptureRequested>(), loom::schema_of<su::SurfaceCaptured>(),
            loom::schema_of<su::SurfaceCaptureChunkRequested>(),
            loom::schema_of<su::SurfaceCaptureChunk>(),
            loom::schema_of<ws::GuestConnectionsRequested>(), loom::schema_of<ws::GuestConnections>(),
            loom::schema_of<ws::GuestConnection>()};
}

/// A participant that declares a DIFFERENT `InjectInput v1` -- the stale copy the wall exists for.
struct InjectInput {
    std::int64_t session = 0;
    std::string note;
    ZEN_SHAPE(InjectInput, 1, ZEN_FIELD(session), ZEN_FIELD(note));
};
struct Nothing {
    std::int64_t n = 0;
    ZEN_SHAPE(Nothing, 1, ZEN_FIELD(n));
};
class Divergent final
    : public loom::WeaveBase<Divergent, Nothing, loom::Accept<InjectInput>, loom::Emit<>> {
public:
    void on(const InjectInput&, loom::Mail&) {}
};

} // namespace

TEST_CASE("guest vocabulary: booted, every shape Workshop's owners speak resolves -- by identity, nested ones included") {
    loom::Switchboard bus;
    for (const auto& s : guest_shapes()) {
        const bool known = bus.resolve_schema(s->name(), s->version()) != nullptr;
        CHECK_MESSAGE(!known, s->name());
    }
    auto w = std::make_unique<GuestVocabulary>();
    GuestVocabulary* raw = w.get();
    const loom::WeaveId id = bus.register_weave(std::move(w), loom::Grant{});
    raw->zen_set_self(id);
    for (const auto& s : guest_shapes()) {
        const std::shared_ptr<const loom::Schema> resolved = bus.resolve_schema(s->name(), s->version());
        REQUIRE_MESSAGE(resolved.get() != nullptr, s->name());
        CHECK_MESSAGE(loom::same_identity(*resolved, *s), s->name());
    }
    // It declares; it does not accept (beyond the substrate doors every weave answers).
    for (const auto& accepted : bus.accepted_schemas(id)) {
        CHECK_MESSAGE(accepted->name().rfind("zen.", 0) == 0, accepted->name());
    }
}

TEST_CASE("guest vocabulary: a divergent definition of one of its shapes is refused at the wall") {
    loom::Switchboard bus;
    auto w = std::make_unique<GuestVocabulary>();
    GuestVocabulary* raw = w.get();
    raw->zen_set_self(bus.register_weave(std::move(w), loom::Grant{}));
    bool refused = false;
    try {
        (void)bus.register_weave(std::make_unique<Divergent>(), loom::Grant{});
    } catch (const std::exception&) {
        refused = true;
    }
    CHECK(refused);
    CHECK(loom::same_identity(*bus.resolve_schema("InjectInput", 1),
                              *loom::schema_of<zengine::input::InjectInput>()));
}

TEST_CASE("guest vocabulary: unloaded, it takes the shapes with it -- a tool that needs them is then refused, not guessed") {
    loom::Switchboard bus;
    auto w = std::make_unique<GuestVocabulary>();
    GuestVocabulary* raw = w.get();
    const loom::WeaveId id = bus.register_weave(std::move(w), loom::Grant{});
    raw->zen_set_self(id);
    REQUIRE(bus.resolve_schema("SurfaceCaptured", 1).get() != nullptr);
    std::unique_ptr<loom::Weave> gone = bus.unregister_weave(id);
    REQUIRE(gone != nullptr);
    CHECK(bus.resolve_schema("SurfaceCaptured", 1).get() == nullptr);
    CHECK(bus.resolve_schema("InjectInput", 1).get() == nullptr);
}
