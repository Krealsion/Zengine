// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "flow/example.hpp"
#include "flow/generate.hpp"
#include "operator/primitives.hpp"
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    try {
        if (argc != 2) return 2;
        zengine::op::Catalog catalog;
        zengine::op::publish_primitives(catalog);
        const auto project = zengine::flow::thermostat(catalog);
        std::ofstream out(argv[1], std::ios::binary);
        out << zengine::flow::generate_cpp(project.definition);
        out.close();
        return out ? 0 : 1;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
