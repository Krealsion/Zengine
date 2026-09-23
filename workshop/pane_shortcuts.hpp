// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_SHORTCUTS_HPP
#define ZENGINE_WORKSHOP_PANE_SHORTCUTS_HPP
#include <zen/weave/shape.hpp>
#include <cstdint>
#include <string>
#include <vector>
namespace zengine::workshop {
// A provider proposes its active global bindings to Desktop. The desktop still owns
// application policy; Workshop still judges collisions and preserves the input actor.
struct PaneShortcut {
    std::string id, label, pane, action;
    std::int64_t scancode = 0, modifiers = 0;
    ZEN_SHAPE(PaneShortcut, 1, ZEN_FIELD(id), ZEN_FIELD(label), ZEN_FIELD(pane),
              ZEN_FIELD(action), ZEN_FIELD(scancode), ZEN_FIELD(modifiers));
};
struct PaneShortcuts {
    std::vector<PaneShortcut> rows;
    ZEN_SHAPE(PaneShortcuts, 1, ZEN_FIELD(rows));
};
struct PaneShortcutsAnswered {
    bool accepted = false;
    std::string reason;
    ZEN_SHAPE(PaneShortcutsAnswered, 1, ZEN_FIELD(accepted), ZEN_FIELD(reason));
};
struct PaneShortcutsRequested { ZEN_SHAPE(PaneShortcutsRequested, 1); };
struct PaneShortcutsWithdrawn {
    std::string reason;
    ZEN_SHAPE(PaneShortcutsWithdrawn, 1, ZEN_FIELD(reason));
};
// Only Desktop may continue its current AppActionRequested with this operation.
// The captured WeaveId excludes a different holder. Same-id code reload is distinct:
// a provider owns re-registration and must not treat a saved id as an invocation grant.
struct PaneShortcutInvoked {
    std::string office, pane, action;
    std::int64_t holder = 0;
    ZEN_SHAPE(PaneShortcutInvoked, 1, ZEN_FIELD(office), ZEN_FIELD(pane),
              ZEN_FIELD(action), ZEN_FIELD(holder));
};
} // namespace zengine::workshop
#endif
