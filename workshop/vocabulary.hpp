// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_VOCABULARY_HPP

// The Workshop host weave's declared state: what the weaver authored that is Workshop's alone and
// must outlive a reload -- the bindings and the window's room -- each as its file's own shape, so
// a file is its field written whole (`keymap_persist`, `window_persist`). Everything else Workshop
// holds is `Session`: a presenter's copies of other owners' facts, which arrive again from those
// owners, and what a hand is doing now, which a reload ends. The layout and every setting are the
// layouts' owner's state (`layouts/owner.hpp`), never this one's.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

namespace keymap_persist {

/// One override row AS WRITTEN: which action, and the gesture that now requests it. Two
/// strings, deliberately -- the action id must be able to name an action this build has
/// never heard of, and the gesture must survive that row unjudged.
struct WorkshopKeymapRow {
    std::string action;
    std::string gesture;

    ZEN_SHAPE(WorkshopKeymapRow, 1, ZEN_FIELD(action), ZEN_FIELD(gesture));
};

/// A whole saved keymap: what it is, which version of that it is, the legend preference,
/// and the override rows IN AUTHORED ORDER. FORMAT VERSION 2: the same four fields as
/// version 1, and an action may be authored on several rows -- one action, several keys
/// (WL-KEY-08). A build that reads version 1 refuses this file by its number.
struct WorkshopKeymap {
    std::string format;
    std::int64_t format_version = 0;
    std::string legend;
    std::vector<WorkshopKeymapRow> overrides;

    ZEN_SHAPE(WorkshopKeymap, 2, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(legend), ZEN_FIELD(overrides));
};

} // namespace keymap_persist

namespace window_persist {

/// HOW MUCH ROOM THE SURFACE HAD, in canvas pixels.
// WL-SESSION-07 -- agents/workshop/window-file.md
struct WorkshopViewport {
    std::int64_t width = 0;
    std::int64_t height = 0;

    /// Version 2: the room is canvas pixels. Same fields as version 1, which said it in cells.
    ZEN_SHAPE(WorkshopViewport, 2, ZEN_FIELD(width), ZEN_FIELD(height));
};

/// WHERE THE WINDOW SAT ON ITS DESKTOP, as the last medium reported it.
struct WorkshopPlacement {
    std::string mode;   ///< none | desktop
    std::int64_t x = 0; ///< the normal window's top-left, in the medium's desktop units
    std::int64_t y = 0;
    std::string window; ///< normal | maximized

    ZEN_SHAPE(WorkshopPlacement, 1, ZEN_FIELD(mode), ZEN_FIELD(x), ZEN_FIELD(y),
              ZEN_FIELD(window));
};

/// THE WINDOW FILE: what it is, which version, the normal window's room, where the window sat,
/// and the GUI window's own normal room (0 by 0 where none was kept).
// WL-SESSION-21 -- agents/workshop/window-file.md
struct WorkshopWindow {
    std::string format;
    std::int64_t format_version = 0;
    WorkshopViewport viewport;
    WorkshopPlacement placement;
    WorkshopViewport window_room;

    ZEN_SHAPE(WorkshopWindow, 1, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(viewport), ZEN_FIELD(placement), ZEN_FIELD(window_room));
};

} // namespace window_persist

/// THE HOST WEAVE'S STATE: the keymap file's bindings as authored (the legend and the override
/// rows, in authored order) and the window file's room (the normal window's room, where it sat,
/// and the GUI window's own room). Nothing is hidden: a binding and a window's place say nothing a
/// reader Workshop's grant lets poke it may not see. Nothing is writable by poke: a binding
/// changes through `KeymapEditRequested`. The effective keymap is derived from `bindings`, in
/// `Session`, as it is derived from the file.
struct WorkshopState {
    keymap_persist::WorkshopKeymap bindings;
    window_persist::WorkshopWindow room;
    ZEN_SHAPE(WorkshopState, 2, ZEN_FIELD(bindings), ZEN_FIELD(room));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_VOCABULARY_HPP
