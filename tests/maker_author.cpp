// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// THE FRESH-PROCESS WITNESS'S FIRST HALF: a program that authors High-water as data on its own
// bus, drives `high` to 7, writes the two native files -- the definition and the state -- into
// the directory it was given, and exits. The maker suite runs it, asserts that it ran (its
// status, before anything about what it said), and reads the two files back in ITS process: the
// same bytes, a different address space, high == 7.
//
// It exists because "the state survives a process" cannot be witnessed inside one process.
//
//   zengine-maker-author <directory>      writes <directory>/hw.definition and <directory>/hw.state

#include "maker/definition.hpp"
#include "maker/files.hpp"
#include "maker/weave.hpp"
#include "maker_fixture.hpp"
#include "operator/catalog.hpp"
#include "operator/primitives.hpp"

#include <zen/serialize.hpp>
#include <zen/switchboard/message.hpp>
#include <zen/switchboard/switchboard.hpp>

#include <cstdint>
#include <cstdio>
#include <string>

namespace op = zengine::op;
namespace maker = zengine::maker;

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: zengine-maker-author <directory>\n");
        return 2;
    }
    const std::string dir = argv[1];

    // The catalog outlives the bus: the weave unmounts its bodies in its destructor.
    op::Catalog catalog;
    op::publish_primitives(catalog);
    loom::Switchboard bus;

    // Authored, encoded to the file's bytes, and read BACK before it is registered -- so the
    // weave that runs here is the one the file describes, and not a C++ object beside it.
    const std::string definition_bytes = maker::definition_bytes(hwfix::high_water(catalog));
    const maker::Admitted read = maker::read_definition(definition_bytes);
    if (!read) {
        std::fprintf(stderr, "the definition did not read back: %s\n", read.reason.c_str());
        return 3;
    }
    const maker::Registered registered = maker::register_definition(bus, catalog, read.definition);
    if (!registered) {
        std::fprintf(stderr, "the definition did not register: %s\n", registered.reason.c_str());
        return 4;
    }

    for (const std::int64_t value : {3, 7, 5}) {
        bus.send(registered.id, loom::Message(hwfix::sample(value)));
        bus.drain_until_idle();
    }
    const loom::Cell* high = registered.weave->state().get("high");
    if (high == nullptr || high->as_int() != 7) {
        std::fprintf(stderr, "high is not 7 after 3, 7, 5\n");
        return 5;
    }

    const std::string definition_error =
        maker::write_file(dir + "/hw.definition", definition_bytes);
    if (!definition_error.empty()) {
        std::fprintf(stderr, "%s\n", definition_error.c_str());
        return 6;
    }
    const std::string state_error =
        maker::write_file(dir + "/hw.state", loom::serialize(registered.weave->state()));
    if (!state_error.empty()) {
        std::fprintf(stderr, "%s\n", state_error.c_str());
        return 7;
    }
    std::printf("zengine-maker-author: hw.definition and hw.state written, high=7\n");
    return 0;
}
