// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// THE CENSUS OF SHAPES: every shape this tree publishes, nested ones included, pinned by content
// id under its name and version in tests/shapes.txt. A weave library publishes what its manifest
// declares -- its doors, state, claims and sentences, and every shape they nest -- and a header
// publishes the shapes no library declares (a file's format, a value carried inside another).
// A change that moves a content id under the same name and version fails here, naming the shape;
// a new version is a new pin, and the pins' diff names every enclosing shape the change touched.

#include "doctest.h"

#include "census_headers.hpp"
#include "operator/image.hpp"

#include <zen/kernel/abi.h>
#include <zen/kernel/schema_codec.hpp>
#include <zen/registry.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

using Key = std::pair<std::string, std::uint32_t>;
using Shape = std::shared_ptr<const loom::Schema>;

std::string key_text(const Key& k) { return k.first + " v" + std::to_string(k.second); }

std::string id_text(loom::ContentId id) {
    char out[19];
    std::snprintf(out, sizeof out, "0x%016llx", static_cast<unsigned long long>(id));
    return out;
}

/// Every (name, version) a source declares: each `ZEN_SHAPE(<name>, <version>,`.
std::vector<Key> shapes_declared_in(const std::string& text) {
    static const std::regex shape(R"(\bZEN_SHAPE\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*,\s*([0-9]+)\s*[,)])");
    std::vector<Key> out;
    for (auto it = std::sregex_iterator(text.begin(), text.end(), shape);
         it != std::sregex_iterator(); ++it) {
        out.emplace_back((*it)[1].str(), static_cast<std::uint32_t>(std::stoul((*it)[2].str())));
    }
    return out;
}

/// EVERY SHAPE THE TREE'S OWN COMPONENTS DECLARE, with the file declaring it. The tests keep
/// published copies, build trees and vendored code are not this tree's, and the examples built by
/// projects of their own (`tower-defense`, `workshop-probe`) publish nothing this build does.
std::map<Key, std::string> declared_shapes(const std::filesystem::path& root) {
    namespace fs = std::filesystem;
    std::map<Key, std::string> out;
    for (auto it = fs::recursive_directory_iterator(root); it != fs::recursive_directory_iterator();
         ++it) {
        const std::string name = it->path().filename().string();
        if (it->is_directory()) {
            const bool top = it.depth() == 0;
            const bool own_project = it.depth() == 1 &&
                                     it->path().parent_path().filename() == "examples" &&
                                     (name == "tower-defense" || name == "workshop-probe");
            if (name == ".git" || name == "third_party" || own_project ||
                (top && (name == "tests" || name == "docs" || name == "quarry" ||
                         name == "build" || name.rfind("build-", 0) == 0 ||
                         name.rfind("cmake-build", 0) == 0 || name == "_install"))) {
                it.disable_recursion_pending();
            }
            continue;
        }
        const std::string ext = it->path().extension().string();
        if (ext != ".hpp" && ext != ".cpp" && ext != ".h" && ext != ".ipp" && ext != ".inl") {
            continue;
        }
        std::ifstream in(it->path(), std::ios::binary);
        const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        for (const Key& k : shapes_declared_in(text)) {
            out.emplace(k, fs::relative(it->path(), root).generic_string());
        }
    }
    return out;
}

void sink_write(void* ctx, const std::uint8_t* data, std::size_t len) {
    static_cast<std::string*>(ctx)->append(reinterpret_cast<const char*>(data), len);
}

/// ONE WEAVE LIBRARY'S MANIFEST, read as the Kernel reads it at load: its doors, state, claims
/// and sentences, and the referenced shapes they nest. Empty, with `why`, when it cannot be read.
std::vector<Shape> manifest_shapes(const std::string& path, std::string& why) {
    zengine::op::ImageShare image{path};
    if (!image.open()) {
        why = "does not open";
        return {};
    }
    using Abi = const ZenWeaveAbi* (*)();
    const auto entry = reinterpret_cast<Abi>(image.symbol("zen_weave_abi"));
    const ZenWeaveAbi* abi = entry != nullptr ? entry() : nullptr;
    if (abi == nullptr) {
        why = "exports no zen_weave_abi";
        return {};
    }
    void* instance = abi->create();
    if (instance == nullptr) {
        why = "creates no weave to describe";
        return {};
    }
    std::string bytes;
    const ZenStatus status = abi->describe(instance, ZenByteSink{&bytes, &sink_write});
    abi->destroy(instance);
    if (status != ZEN_OK) {
        why = "describe failed";
        return {};
    }
    const loom::Admission admitted = loom::admit(loom::parse(bytes), loom::manifest_schema());
    if (!admitted.ok()) {
        why = "manifest refused: " + admitted.first_error().message();
        return {};
    }
    const loom::Value& manifest = admitted.value();
    loom::Registry deps;
    loom::SchemaClaimScope scope;
    loom::decode_referenced(manifest, deps, scope);
    std::vector<Shape> out;
    for (const char* section : {"accepted", "claims", "emits"}) {
        if (const loom::Cell* list = manifest.get(section)) {
            for (const loom::Cell& c : list->as_list()) {
                out.push_back(loom::decode_schema(*c.as_message(), deps));
            }
        }
    }
    out.push_back(loom::decode_schema(*manifest.get("state")->as_message(), deps));
    return out;
}

std::vector<std::string> lines_of(const std::string& path) {
    std::ifstream in(path);
    std::vector<std::string> out;
    for (std::string line; std::getline(in, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) out.push_back(line);
    }
    return out;
}

} // namespace

TEST_CASE("every shape this tree publishes is pinned by content id under its name and version") {
    // ⚔ MUTATION: a field added to a published shape at the same version -- its content id moves,
    // and so does every enclosing shape's, and the census names each.
    //
    // THE READER, MADE TO SAY YES AND NO before it is believed.
    CHECK(shapes_declared_in("ZEN_SHAPE(Alpha, 1, ZEN_FIELD(x));\nZEN_SHAPE( Beta , 12);") ==
          std::vector<Key>{{"Alpha", 1}, {"Beta", 12}});
    CHECK(shapes_declared_in("NOT_ZEN_SHAPE(Gamma, 1); ZEN_SHAPE(Delta, x);").empty());

    // WHAT THE TREE PUBLISHES: every product weave's manifest, and the headers' shapes, each with
    // every shape it nests.
    std::vector<Shape> roots = census_header_shapes();
    std::size_t libraries = 0;
    for (const std::string& path : lines_of(ZENGINE_CENSUS_WEAVES)) {
        CAPTURE(path);
        std::string why;
        const std::vector<Shape> shapes = manifest_shapes(path, why);
        CHECK_MESSAGE(why.empty(), path << " " << why);
        roots.insert(roots.end(), shapes.begin(), shapes.end());
        ++libraries;
    }
    REQUIRE(libraries > 20);
    std::vector<Shape> every;
    for (const Shape& s : roots) {
        loom::collect_referenced(*s, every);
        every.push_back(s);
    }

    // ONLY THE TREE'S OWN, and every one of them: a shape Loom declares is Loom's to pin.
    const std::map<Key, std::string> declared = declared_shapes(ZENGINE_SOURCE_DIR);
    REQUIRE(declared.size() > 300);
    std::map<Key, loom::ContentId> census;
    for (const Shape& s : every) {
        const Key k{s->name(), s->version()};
        if (declared.count(k) == 0) continue;
        const auto [at, fresh] = census.emplace(k, s->content_id());
        CHECK_MESSAGE((fresh || at->second == s->content_id()),
                      key_text(k) << " is published with two contents: " << id_text(at->second)
                                  << " and " << id_text(s->content_id()));
    }
    for (const auto& [k, file] : declared) {
        CHECK_MESSAGE(census.count(k) == 1,
                      key_text(k) << ", declared in " << file
                                  << ", is in no library's manifest and no census header: add it "
                                     "to tests/census_headers.hpp");
    }

    // THE PINS: one line a shape, `<name> v<version> <content id>`, and nothing pinned that is not
    // published. The census as it stands is written beside the build, ready to diff.
    std::map<Key, std::string> pinned;
    for (const std::string& line : lines_of(ZENGINE_SOURCE_DIR "/tests/shapes.txt")) {
        if (line[0] == '#') continue;
        std::istringstream words(line);
        std::string name, version, id;
        words >> name >> version >> id;
        REQUIRE_MESSAGE((version.size() > 1 && version[0] == 'v'), "a pin reads: " << line);
        pinned[Key{name, static_cast<std::uint32_t>(std::stoul(version.substr(1)))}] = id;
    }
    std::ofstream now(ZENGINE_CENSUS_NOW, std::ios::binary);
    for (const auto& [k, id] : census) {
        now << k.first << " v" << k.second << " " << id_text(id) << "\n";
        const auto pin = pinned.find(k);
        if (pin == pinned.end()) {
            CHECK_MESSAGE(false, key_text(k) << " is published and not pinned: add `" << k.first
                                             << " v" << k.second << " " << id_text(id)
                                             << "` to tests/shapes.txt");
            continue;
        }
        CHECK_MESSAGE(pin->second == id_text(id),
                      key_text(k) << " is " << id_text(id) << " where it was pinned as " << pin->second
                                  << ": a changed shape is a new version, and so is every shape "
                                     "enclosing it");
    }
    for (const auto& [k, id] : pinned) {
        CHECK_MESSAGE(census.count(k) == 1,
                      key_text(k) << " is pinned and published no more: its pin goes with it");
    }
    MESSAGE("census: " << census.size() << " shapes from " << libraries << " libraries; as it stands: "
                       << ZENGINE_CENSUS_NOW);
}
