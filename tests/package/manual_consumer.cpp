// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// A stranger's weave carries its manual as Zengine's do: the page `tests/package/kitchen-oven.md`,
// compiled in by the installed `zengine_manual()` as `manuals/kitchen.oven.hpp`, read back here,
// beside the door a role answers for the shapes it accepts (`manual/vocabulary.hpp`), from
// `zengine::manual` alone.

#include "manual/vocabulary.hpp"

#include "manuals/kitchen.oven.hpp"

#include <cstdio>
#include <string_view>

int main() {
    const zengine::manual::Manual& page = zengine::manual::pages::kitchen_oven::kManual;
    const zengine::manual::Section* about = page.find("about");
    const zengine::manual::Section* use = page.find("use");
    zengine::manual::ShapeDocumentRequested asked;
    asked.shape = "kitchen.Bake";
    if (page.role != "kitchen.oven" || page.name != "the Oven" || about == nullptr ||
        use == nullptr || about->text.find("Timer service") == std::string_view::npos ||
        page.content_id.size() != 64 || asked.shape != "kitchen.Bake") {
        std::fprintf(stderr, "manual: the page compiled in is not the page written\n");
        return 1;
    }
    std::printf("manual: %.*s carries %zu sections of its own page\n",
                static_cast<int>(page.role.size()), page.role.data(), page.count);
    return 0;
}
