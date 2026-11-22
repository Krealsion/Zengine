// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// What one Workshop suite's own definitions say, compiled into that suite alone: its entry name,
// and the images only some suites are given (tests/CMakeLists.txt, each suite's DEFINITIONS). The
// fixture's bodies read them from here, so they hold no suite's own definition.

#include <string>

namespace zengine::testing {

const char* workshop_suite() { return ZENGINE_WORKSHOP_SUITE; }

const char* workshop_suite_artifact(const std::string& stem) {
    (void)stem; // a suite given none of these images reads no stem
#ifdef WORKSHOP_SO_COMPOSER
    if (stem == "zengine-composer") return WORKSHOP_SO_COMPOSER;
#endif
#ifdef WORKSHOP_SO_INVENTORY_PANE
    if (stem == "zengine-inventory-pane") return WORKSHOP_SO_INVENTORY_PANE;
    if (stem == "zengine-inventory") return WORKSHOP_SO_INVENTORY;
#endif
#ifdef WORKSHOP_WASM_TALLY
    // THE TALLY PANE AS A MODULE, named by a stem the host spells as it is.
    if (stem == "zengine-example-tally.wasm") {
        return WORKSHOP_WASM_TALLY;
    }
#endif
#ifdef WORKSHOP_SO_EDITOR_SWITCH_B
    // THE EDITOR SWITCH SUITE'S OTHER CHOICES: the standard Editor built under other stems.
    if (stem == "zengine-editor-pane-b") {
        return WORKSHOP_SO_EDITOR_SWITCH_B;
    }
    if (stem == "zengine-editor-pane-silent") {
        return WORKSHOP_SO_EDITOR_SWITCH_SILENT;
    }
    if (stem == "zengine-editor-pane-refusing") {
        return WORKSHOP_SO_EDITOR_SWITCH_REFUSING;
    }
    if (stem == "zengine-editor-pane-not-live") {
        return WORKSHOP_SO_EDITOR_SWITCH_NOT_LIVE;
    }
    if (stem == "zengine-editor-pane-losing") {
        return WORKSHOP_SO_EDITOR_SWITCH_LOSING;
    }
#endif
#ifdef WORKSHOP_SO_NEOVIM_EDITOR
    // THE NEOVIM-BACKED EDITOR, for suite `workshop_neovim`.
    if (stem == "zengine-neovim-editor") {
        return WORKSHOP_SO_NEOVIM_EDITOR;
    }
#endif
    return nullptr;
}

} // namespace zengine::testing
