// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "flow/generate.hpp"
#include "flow_fixture.hpp"
#include "maker_fixture.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::invalid_argument("expected output directory");
        zengine::op::Catalog catalog;
        flowfix::operators(catalog);
        std::filesystem::create_directories(argv[1]);
        auto write = [&](const std::string& stem, const zengine::maker::Definition& definition) {
            std::ofstream output(std::filesystem::path(argv[1]) / (stem + ".cpp"), std::ios::binary);
            output << zengine::flow::generate_cpp(definition);
            output.close();
            if (!output) throw std::runtime_error("failed writing generated source");
        };
        write("thermostat", zengine::flow::thermostat(catalog).definition);
        write("boundaries", flowfix::boundaries(catalog));
        write("high_water", hwfix::high_water(catalog));
        auto empty = hwfix::high_water(catalog); empty.on.clear();
        write("empty", empty);
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
