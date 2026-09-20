// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "flow/compiled.hpp"
#include "flow/example.hpp"
#include "operator/primitives.hpp"
#include <zen/kernel/kernel.hpp>
#include <iostream>

int main(int argc, char** argv) {
    try {
        if (argc != 2) return 2;
        zengine::op::Catalog catalog;
        zengine::op::publish_primitives(catalog);
        auto module = zengine::flow::CompiledModule::open(catalog, argv[1]);
        const auto mounted = module->mount();
        if (!mounted) throw std::runtime_error(mounted.reason);
        loom::Switchboard bus;
        loom::Kernel kernel(bus);
        const auto offer = module->offer();
        const auto loaded = kernel.load("thermostat", argv[1], "thermostat",
            zengine::maker::default_grant(module->definition()));
        if (!loaded.ok) throw std::runtime_error(loaded.error);
        const auto project = zengine::flow::thermostat(catalog);
        if (!bus.swap_state(loaded.id, loom::serialize(project.state)).revived)
            throw std::runtime_error("state restore refused");
        auto send = [&](std::int64_t degrees, bool expected) {
            loom::Value message(project.definition.accepts[0]);
            message.set("degrees", loom::Cell::integer(degrees));
            bus.send(loaded.id, loom::Message(std::move(message)));
            for (unsigned turn = 0; turn < 8 && bus.pending() != 0; ++turn) bus.pump_pending();
            if (bus.pending() != 0 || bus.weave(loaded.id)->snapshot().get("heating")->as_bool() != expected)
                throw std::runtime_error("installed native thermostat answered incorrectly");
        };
        send(17, true); send(20, true); send(24, false); send(20, false);
        const auto* original = catalog.find(zengine::op::kSelectBool);
        zengine::op::OperatorDef changed(original->identity(), original->inputs(), original->outputs(),
            [](const loom::Value&) { return loom::Cell::boolean(false); });
        if (!catalog.mount("stranger.overlay", {changed}, zengine::op::MountMode::Overlay)) return 1;
        send(10, false);
        catalog.unmount("stranger.overlay");
        send(10, true);
        std::cout << "Flow installed native loop: hysteresis, state restore, live overlay and reveal passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
