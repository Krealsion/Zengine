// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "inventory/codec.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

// All item and metadata schemas come from the envelope; no sample types are compiled here.
int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: zengine-inventory-read <pair.bin>\n";
        return 2;
    }
    try {
        std::ifstream file(argv[1], std::ios::binary);
        if (!file) {
            throw std::runtime_error("cannot open pair file");
        }
        const std::string bytes((std::istreambuf_iterator<char>(file)), {});
        if (file.bad()) {
            throw std::runtime_error("cannot read pair file");
        }
        const auto pair = zengine::inventory::decode_pair(bytes);
        std::cout << "{\"item\":" << loom::compat::serialize(pair.item) << ",\"metadata\":[";
        for (std::size_t i = 0; i < pair.metadata.size(); ++i) {
            if (i != 0) {
                std::cout << ',';
            }
            std::cout << loom::compat::serialize(pair.metadata[i]);
        }
        std::vector<std::shared_ptr<const loom::Schema>> roots{pair.item.schema_ptr()};
        for (const auto& metadata : pair.metadata) {
            roots.push_back(metadata.schema_ptr());
        }
        std::cout << "],\"schemas\":"
                  << loom::compat::serialize(loom::encode_accepted_shapes(roots)) << "}\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "inventory read refused: " << error.what() << '\n';
        return 1;
    }
}
