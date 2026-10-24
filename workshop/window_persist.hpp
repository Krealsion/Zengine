// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_WINDOW_PERSIST_HPP
#define ZENGINE_WORKSHOP_WINDOW_PERSIST_HPP

// Workshop's window file: the room its desk had, where its window sat on the desktop and how large
// the OS window was, written at an orderly close and read at the next start -- and, once, when no
// window file exists yet, the same three facts read from a session file an older Workshop wrote.
// Workshop law: agents/workshop/window-file.md

#include "persist.hpp"
#include "screen.hpp"

#include <zen/admission.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace zengine::workshop::window_persist {

/// What a Workshop window file says it is: its own word.
inline constexpr const char* kFormat = "zengine-workshop-window";

/// The one window format version this build writes and admits.
// WL-SESSION-21 -- agents/workshop/window-file.md
inline constexpr std::int64_t kFormatVersion = 1;

/// WHERE THE WINDOW FILE LIVES: beside the session, its name the session's with `.window` before
/// the extension -- one window file a session, and none for a run that keeps no session.
// WL-SESSION-21 -- agents/workshop/window-file.md
inline std::string path_beside(const std::string& session_path) {
    if (session_path.empty()) {
        return std::string();
    }
    std::filesystem::path p(session_path);
    const std::string ext = p.extension().string();
    p.replace_extension();
    return p.string() + ".window" + (ext.empty() ? std::string(".json") : ext);
}

/// A window file is three small facts; its ceiling says so.
inline constexpr std::uintmax_t kMaxWindowBytes = 4u << 10;

// ---- The file's own shapes ----------------------------------------------------------------

/// HOW MUCH ROOM THE SURFACE HAD, in canvas pixels.
// WL-SESSION-07 -- agents/workshop/window-file.md
struct WorkshopViewport {
    std::int64_t width = 0;
    std::int64_t height = 0;

    /// Version 2: the room is canvas pixels. Same fields as version 1, which said it in cells.
    ZEN_SHAPE(WorkshopViewport, 2, ZEN_FIELD(width), ZEN_FIELD(height));
};

// ---- The placement's words, and why they are words ----------------------------------------
// WL-SESSION-08 -- agents/workshop/window-file.md

inline constexpr const char* kPlacementNone = "none";
inline constexpr const char* kPlacementDesktop = "desktop";
inline constexpr const char* kPlacementModeWords = "none or desktop";

inline constexpr const char* kWindowNormal = "normal";
inline constexpr const char* kWindowMaximized = "maximized";
inline constexpr const char* kWindowWords = "normal or maximized";

/// WHERE THE WINDOW SAT ON ITS DESKTOP, as the last medium reported it.
// WL-SESSION-08 -- agents/workshop/window-file.md
struct WorkshopPlacement {
    std::string mode;   ///< none | desktop
    std::int64_t x = 0; ///< the normal window's top-left, in the medium's desktop units
    std::int64_t y = 0;
    std::string window; ///< normal | maximized

    ZEN_SHAPE(WorkshopPlacement, 1, ZEN_FIELD(mode), ZEN_FIELD(x), ZEN_FIELD(y),
              ZEN_FIELD(window));
};

/// THE WINDOW FILE: what it is, which version, the normal window's room, where the window sat,
/// and the OS window's own normal room (0 by 0 where none was kept).
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

static_assert(WorkshopWindow::zen_version == static_cast<std::uint32_t>(kFormatVersion),
              "the window file's format version and its envelope's shape version are one number: a "
              "file of another version is refused by its number, before its fields are judged");

// ---- Would this viewport be honoured? ------------------------------------------------------

/// IS THIS A VIEWPORT THIS WORKSHOP WOULD ACTUALLY OPEN AT?
// WL-SESSION-07 -- agents/workshop/window-file.md
inline constexpr bool viewport_honoured(std::int64_t width, std::int64_t height) noexcept {
    return width >= kScreenMinW && width <= kScreenMaxW && height >= kScreenMinH &&
           height <= kScreenMaxH;
}

/// What to say about a viewport this build will not open at, naming the value found.
inline std::string declined_viewport(std::int64_t width, std::int64_t height) {
    return "the saved window size " + std::to_string(width) + "x" + std::to_string(height) +
           " pixels is not one this Workshop opens at (" + std::to_string(kScreenMinW) + "x" +
           std::to_string(kScreenMinH) + " to " + std::to_string(kScreenMaxW) + "x" +
           std::to_string(kScreenMaxH) + ") -- opening at the default size";
}

// ---- The window's facts as values ----------------------------------------------------------

/// THE PLACEMENT, AS A VALUE -- what Workshop remembers between a medium's report and a close,
/// and what a read hands back.
// WL-SESSION-08 -- agents/workshop/window-file.md
struct Placement {
    bool known = false;     ///< a medium has reported one (this run or a restored one)
    std::int64_t x = 0;     ///< the normal window's top-left, opaque desktop units
    std::int64_t y = 0;
    bool maximized = false; ///< whether the window was maximized
    /// The OS window's normal room, in its pixels, kept beside the place whether or not one is
    /// known; 0 by 0 when none was kept.
    // WL-SESSION-20 -- agents/workshop/window-file.md
    std::int64_t room_w = 0;
    std::int64_t room_h = 0;
};

/// WHAT READING A WINDOW FILE PRODUCED: whether a file that exists was understood, whether there
/// was one, the room when this Workshop would open at it (else why not), and the placement.
struct LoadedWindow {
    Written outcome;
    bool present = false;
    std::int64_t viewport_w = 0;
    std::int64_t viewport_h = 0;
    bool honoured = false;
    std::string declined;
    Placement placement;

    static LoadedWindow no(std::string why) {
        LoadedWindow bad;
        bad.outcome = Written::no(std::move(why));
        bad.present = true;
        return bad;
    }
};

/// The placement's own admission: the two closed word sets, and the one-spelling law for the
/// absence. The coordinates are another machine's desktop truth, accepted unjudged.
// WL-SESSION-08 -- agents/workshop/window-file.md
inline Written placement_in(const WorkshopPlacement& file, Placement& out) {
    bool maximized = false;
    if (file.window == kWindowMaximized) {
        maximized = true;
    } else if (file.window != kWindowNormal) {
        return Written::no("`" + file.window + "` is not a window state (" + kWindowWords + ")");
    }
    if (file.mode == kPlacementNone) {
        if (file.x != 0 || file.y != 0 || maximized) {
            return Written::no("a placement of `none` carries no coordinates and no maximized "
                               "state -- zero its numbers and set the window to `" +
                               std::string(kWindowNormal) + "`, or set the mode to `" +
                               std::string(kPlacementDesktop) + "`");
        }
        out = Placement{};
        return Written::ok();
    }
    if (file.mode != kPlacementDesktop) {
        return Written::no("`" + file.mode + "` is not a placement mode (" + kPlacementModeWords +
                           ")");
    }
    out.known = true;
    out.x = file.x;
    out.y = file.y;
    out.maximized = maximized;
    return Written::ok();
}

/// The room judged against this build's band, and the placement, as a loaded window.
inline LoadedWindow loaded_from(std::int64_t viewport_w, std::int64_t viewport_h,
                                const Placement& place) {
    LoadedWindow loaded;
    loaded.outcome = Written::ok();
    loaded.present = true;
    loaded.placement = place;
    if (viewport_honoured(viewport_w, viewport_h)) {
        loaded.viewport_w = viewport_w;
        loaded.viewport_h = viewport_h;
        loaded.honoured = true;
    } else {
        loaded.declined = declined_viewport(viewport_w, viewport_h);
    }
    return loaded;
}

// ---- Writing ------------------------------------------------------------------------------

/// The window's facts as the value that gets written: the room and the placement as the session
/// held them, nothing judged on the way out.
inline WorkshopWindow to_window(std::int64_t viewport_w, std::int64_t viewport_h,
                                const Placement& place) {
    WorkshopWindow out;
    out.format = kFormat;
    out.format_version = kFormatVersion;
    out.viewport = WorkshopViewport{viewport_w, viewport_h};
    out.placement.mode = place.known ? kPlacementDesktop : kPlacementNone;
    out.placement.x = place.known ? place.x : 0;
    out.placement.y = place.known ? place.y : 0;
    out.placement.window = place.known && place.maximized ? kWindowMaximized : kWindowNormal;
    out.window_room = WorkshopViewport{place.room_w, place.room_h};
    return out;
}

inline std::string to_text(std::int64_t viewport_w, std::int64_t viewport_h,
                           const Placement& place) {
    return loom::compat::serialize(loom::to_value(to_window(viewport_w, viewport_h, place)));
}

/// Save the window file through `persist`'s safe write.
// WL-SESSION-21 -- agents/workshop/window-file.md
inline Written save_file(const std::string& path, std::int64_t viewport_w, std::int64_t viewport_h,
                         const Placement& place) {
    return persist::write_file_making_room(path, to_text(viewport_w, viewport_h, place));
}

// ---- Reading ------------------------------------------------------------------------------

/// Text to a window. Total: every input is a window or a refusal with a reason.
// WL-SESSION-21 -- agents/workshop/window-file.md
inline LoadedWindow from_text(std::string_view bytes) {
    const loom::Unverified claim = loom::compat::parse(bytes);
    if (claim.well_formed() && claim.claimed_name() == std::string(WorkshopWindow::zen_name) &&
        claim.claimed_version() != WorkshopWindow::zen_version) {
        return LoadedWindow::no("window version " + std::to_string(claim.claimed_version()) +
                                " -- this Workshop reads version " +
                                std::to_string(kFormatVersion));
    }
    const loom::Admission admitted =
        loom::admit(claim, loom::schema_of<WorkshopWindow>(), loom::Report::FirstError);
    if (!admitted.ok()) {
        return LoadedWindow::no("not a Workshop window file: " + admitted.first_error().message());
    }
    const auto file = loom::from_value<WorkshopWindow>(admitted.value());
    if (file.format != kFormat) {
        return LoadedWindow::no("not a Workshop window file: it says it is `" + file.format + "`");
    }
    if (file.format_version != kFormatVersion) {
        return LoadedWindow::no("this window file claims version " +
                                std::to_string(kFormatVersion) +
                                " and its own format_version field says " +
                                std::to_string(file.format_version));
    }
    Placement place;
    if (const Written placed = placement_in(file.placement, place); !placed.accepted) {
        return LoadedWindow::no(placed.refusal);
    }
    // THE OS WINDOW'S ROOM IS JUDGED WHERE IT IS USED, as the viewport is, and refuses nothing.
    place.room_w = file.window_room.width;
    place.room_h = file.window_room.height;
    return loaded_from(file.viewport.width, file.viewport.height, place);
}

/// Read the window file. A missing file is no window file, not an error.
inline LoadedWindow load_file(const std::string& path) {
    std::error_code ec;
    if (path.empty() || !std::filesystem::exists(path, ec) || ec) {
        return LoadedWindow{};
    }
    const persist::FileText read = persist::read_file(path, kMaxWindowBytes, "a Workshop window");
    if (!read.outcome.accepted) {
        return LoadedWindow::no(read.outcome.refusal);
    }
    return from_text(read.text);
}

// ---- The import: the window's facts out of a session an older Workshop wrote ----------------
// WL-SESSION-21 -- agents/workshop/window-file.md

/// The session format whose room is the first said in canvas pixels, the first with a desktop
/// placement, and the first with the OS window's own room; and the last a session held them in.
inline constexpr std::int64_t kSessionPixelsVersion = 7;
inline constexpr std::int64_t kSessionPlacementVersion = 3;
inline constexpr std::int64_t kSessionWindowRoomVersion = 10;
inline constexpr std::int64_t kSessionLastWindowVersion = 11;

/// A cell as an older session counted it, in pixels.
inline constexpr std::int64_t kSessionCellPx = 12;

/// THE WINDOW'S FACTS IN A SESSION FILE AN OLDER WORKSHOP WROTE: its room (in cells before
/// version 7), its placement from version 3, and the OS window's room from version 10, read from
/// the session's own value -- or, from version 11, its header line -- and judged as the window
/// file's own are. Nothing when the session is of a version that keeps none of them, or is not
/// read; never a refusal of the start.
inline LoadedWindow import_from_session_text(std::string_view text) {
    // The first line of a file a line a value is its header; any other file is one value.
    const std::size_t newline = text.find('\n');
    const std::string_view first = text.substr(0, newline);
    std::string_view value = text;
    const std::optional<std::string> schema = persist::json::word(persist::json::member(first, "schema"));
    if (schema && *schema == "WorkshopSessionHeader") {
        value = first;
    } else if (!(persist::json::word(persist::json::member(text, "schema")).value_or("") == "WorkshopSession")) {
        return LoadedWindow{};
    }
    const std::optional<std::int64_t> version = persist::json::integer(persist::json::member(value, "version"));
    const std::optional<std::string_view> fields = persist::json::member(value, "fields");
    if (!version || !fields || *version < 1 || *version > kSessionLastWindowVersion) {
        return LoadedWindow{};
    }
    const std::optional<std::string_view> viewport = persist::json::member(*fields, "viewport");
    std::optional<std::int64_t> w = viewport ? persist::json::integer(persist::json::member(*viewport, "width"))
                                             : std::nullopt;
    std::optional<std::int64_t> h = viewport ? persist::json::integer(persist::json::member(*viewport, "height"))
                                             : std::nullopt;
    if (!w || !h) {
        return LoadedWindow{};
    }
    if (*version < kSessionPixelsVersion) {
        *w *= kSessionCellPx;
        *h *= kSessionCellPx;
    }
    Placement place;
    if (*version >= kSessionPlacementVersion) {
        const std::optional<std::string_view> p = persist::json::member(*fields, "placement");
        WorkshopPlacement file;
        file.mode = persist::json::word(p ? persist::json::member(*p, "mode") : std::nullopt).value_or(std::string());
        file.x = persist::json::integer(p ? persist::json::member(*p, "x") : std::nullopt).value_or(0);
        file.y = persist::json::integer(p ? persist::json::member(*p, "y") : std::nullopt).value_or(0);
        file.window =
            persist::json::word(p ? persist::json::member(*p, "window") : std::nullopt).value_or(std::string());
        if (!placement_in(file, place).accepted) {
            place = Placement{};
        }
    }
    if (*version >= kSessionWindowRoomVersion) {
        const std::optional<std::string_view> room = persist::json::member(*fields, "window_room");
        place.room_w = persist::json::integer(room ? persist::json::member(*room, "width") : std::nullopt).value_or(0);
        place.room_h =
            persist::json::integer(room ? persist::json::member(*room, "height") : std::nullopt).value_or(0);
    }
    return loaded_from(*w, *h, place);
}

/// ...read from the session file at `path`, when there is one this build can read as bytes.
inline LoadedWindow import_from_session(const std::string& path, std::uintmax_t max_bytes) {
    std::error_code ec;
    if (path.empty() || !std::filesystem::exists(path, ec) || ec) {
        return LoadedWindow{};
    }
    const persist::FileText read = persist::read_file(path, max_bytes, "a Workshop session");
    if (!read.outcome.accepted) {
        return LoadedWindow{};
    }
    return import_from_session_text(read.text);
}

} // namespace zengine::workshop::window_persist

#endif // ZENGINE_WORKSHOP_WINDOW_PERSIST_HPP
