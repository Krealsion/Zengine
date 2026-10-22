// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// zengine-tally-app-fixture: writes the tally app's `tally.view` and `tally.flow` into a folder,
// the bytes the View Builder's Save and Flow's Export write (examples/tally-app/README.md).
//   zengine-tally-app-fixture <folder>

#include "tally.hpp"

#include "operator/primitives.hpp"

#include <cstdio>
#include <exception>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fputs("usage: zengine-tally-app-fixture <folder>\n", stderr);
        return 2;
    }
    const std::string folder = argv[1];
    try {
        zengine::op::Catalog catalog;
        zengine::op::publish_primitives(catalog);
        zengine::view::save_description(folder + "/tally.view",
                                        zengine::examples::tally_app::panel());
        zengine::flow::save_project(folder + "/tally.flow",
                                    zengine::examples::tally_app::project(catalog));
    } catch (const std::exception& error) {
        std::fprintf(stderr, "zengine-tally-app-fixture: %s\n", error.what());
        return 1;
    }
    std::printf("wrote %s/tally.view and %s/tally.flow\n", folder.c_str(), folder.c_str());
    return 0;
}
