// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_VOCABULARY_HPP

// The Workshop host weave's STATE SHAPE -- what a same-shape revive restores.
//
// ⭐ IT WAS THE PROTOTYPE OBJECT DOCUMENT (`WorkshopDoc` v2: the authored objects and their
// identity mint) UNTIL THAT RETIRED WITH ITS CANVAS. Nothing the host holds now is weave state:
// the desk, the keymap, the preferences, the session and a maker-made pane are FILES a maker
// owns, and everything else is `Session` -- what a maker is doing, which a revive is entitled to
// keep in place and a new process starts fresh. So the shape is empty, and says so, rather than
// carrying a field nobody reads.
//
// AN OLD OBJECT DOCUMENT ON DISK IS NOT READ AS ANYTHING ELSE. `workshop.json`, or the file a
// `--document` names, is left exactly as it is and said once at startup
// (`HostContext::retired_document`): no door of this host opens, rewrites or deletes it.

#include <zen/weave/shape.hpp>

namespace zengine::workshop {

/// THE HOST WEAVE'S STATE: nothing -- see above.
struct WorkshopState {
    ZEN_SHAPE(WorkshopState, 1);
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_VOCABULARY_HPP
