// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_VOCABULARY_HPP
#define ZENGINE_GRAPHICS_VOCABULARY_HPP

// The graphics vocabulary: one primitive (a node), the element record beside it, and the messages
// between an owner, the window that shows its area, and the directory that introduces the two.
// An owner draws nodes into its window, or says an element once and then sends only its changes,
// which code the window loads turns into nodes. The directory stays off that path.
// Reference: graphics/docs/graphics.md.

#include "surface/vocabulary.hpp"

#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::graphics {

/// The theme's roles are Surface's: a colour is named by its role, or given exactly beside one.
namespace role = zengine::surface::role;

/// The directory's role: which windows exist, which areas each shows, who draws in each, which
/// skin paints it and which code presents each element type.
inline constexpr const char* kGraphicsRole = "zengine.graphics";
/// A window's office is this prefix and its name: `zengine.window.terminal`.
inline constexpr const char* kWindowOfficePrefix = "zengine.window.";
/// The terminal window's name: one terminal to a process, so one terminal window.
inline constexpr const char* kTerminalWindow = "terminal";

/// The bounds one area keeps in its window. A drawing past any of them is refused whole, in words
/// sent to its drawer.
inline constexpr std::size_t kMaxNodesPerArea = 4096;
inline constexpr std::size_t kMaxElementsPerArea = 1024;
inline constexpr std::size_t kMaxPathBytes = 256;
inline constexpr std::size_t kMaxPathDepth = 16;
inline constexpr std::size_t kMaxArgsBytes = 64 * 1024;
inline constexpr std::size_t kMaxAreas = 256;
/// The pixels of one terminal cell: a window of characters floors each edge to its cell.
inline constexpr std::int64_t kCellPx = 12;

/// THE ONE PRIMITIVE. Its `path` is its owner's name for it, unique in its area, and its place: the
/// segments before the last name the node it lives in. `op` names the operator that draws it, and
/// `args` are that operator's operands, their shape implied by the operator (graphics/ops.hpp), so
/// they travel as the shape's body alone (graphics/implied.hpp).
struct Node {
    std::string path;
    std::string op;
    loom::Bytes args;
    ZEN_SHAPE(Node, 1, ZEN_FIELD(path), ZEN_FIELD(op), ZEN_FIELD(args));
};

/// The parent of a path: everything before its last '/', or "" at the area's root.
inline std::string parent_of(const std::string& path) {
    const auto at = path.rfind('/');
    return at == std::string::npos ? std::string() : path.substr(0, at);
}

/// Whether `path` is `root` or lies beneath it.
inline bool under(const std::string& path, const std::string& root) {
    return path == root || (path.size() > root.size() && path.compare(0, root.size(), root) == 0 &&
                            path[root.size()] == '/');
}

/// WHAT A DRAWN THING MEANS, beside its nodes: `type` names its record's shape and `data` is that
/// shape's value, whole; `action` is what a press anywhere in it asks of the area's owner, or "".
struct ElementRecord {
    std::string path;
    std::string type;
    loom::Bytes data;
    std::string action;
    ZEN_SHAPE(ElementRecord, 1, ZEN_FIELD(path), ZEN_FIELD(type), ZEN_FIELD(data),
              ZEN_FIELD(action));
};

/// What one drawer changed in one area, applied together. `whole` first removes everything the
/// drawer may draw there. A node put again changes in its place; a new one goes after its
/// siblings; removing a node removes its subtree and its records. `more` says more of the same
/// beat's drawing follows from this drawer, so the window shows once, after the last.
struct Drawn {
    std::string area;
    bool whole = false;
    std::vector<Node> put;
    std::vector<std::string> removed;
    std::vector<ElementRecord> said;
    std::vector<std::string> unsaid;
    bool more = false;
    ZEN_SHAPE(Drawn, 1, ZEN_FIELD(area), ZEN_FIELD(whole), ZEN_FIELD(put), ZEN_FIELD(removed),
              ZEN_FIELD(said), ZEN_FIELD(unsaid), ZEN_FIELD(more));
};

/// Window -> the drawer of a drawing or an element it refused: why, in words. What the window
/// showed stays as it was.
struct DrawnRefused {
    std::string area;
    std::string reason;
    ZEN_SHAPE(DrawnRefused, 1, ZEN_FIELD(area), ZEN_FIELD(reason));
};

// ---- elements the window presents ---------------------------------------------------------

/// A drawer -> its window: present an element of `type` at `path` of `area`, from `data`, the
/// type's own shape whole. Said once, and again only to start it over; `ref` is the drawer's own
/// reference for it in its changes. The element's subtree is its code's to draw.
struct ElementPut {
    std::string area;
    std::string path;
    std::string type;
    std::int64_t ref = 0;
    loom::Bytes data;
    bool more = false;
    ZEN_SHAPE(ElementPut, 1, ZEN_FIELD(area), ZEN_FIELD(path), ZEN_FIELD(type), ZEN_FIELD(ref),
              ZEN_FIELD(data), ZEN_FIELD(more));
};

/// A drawer -> its window: a change to the element it named `ref`. `kind` indexes the changes its
/// code takes, and `change` is that change's body alone: the element's type implies its shape.
struct ElementChange {
    std::int64_t ref = 0;
    std::int64_t kind = 0;
    loom::Bytes change;
    bool more = false;
    ZEN_SHAPE(ElementChange, 1, ZEN_FIELD(ref), ZEN_FIELD(kind), ZEN_FIELD(change),
              ZEN_FIELD(more));
};

/// Window -> the drawer of an element: a press landed in it that its code did not take, or a press
/// on the drawer's own nodes; `element` is the element's path, or "" where no element was pressed,
/// and `path` the node pressed. The point is in the area's pixels.
struct ElementPressed {
    std::string area;
    std::string element;
    std::string path;
    std::int64_t x = 0, y = 0;
    ZEN_SHAPE(ElementPressed, 1, ZEN_FIELD(area), ZEN_FIELD(element), ZEN_FIELD(path),
              ZEN_FIELD(x), ZEN_FIELD(y));
};

/// Window -> the area's owner: a press landed in an element naming this action.
struct ActionIssued {
    std::string area;
    std::string action;
    ZEN_SHAPE(ActionIssued, 1, ZEN_FIELD(area), ZEN_FIELD(action));
};

// ---- the directory ------------------------------------------------------------------------

/// A window -> the directory, as its own office, on each activation: its name, its size in cells,
/// and its incarnation. A window started afresh holds nothing, so its owners draw whole again.
struct WindowOffered {
    std::string window;
    std::int64_t cols = 0, rows = 0;
    std::int64_t epoch = 0;
    ZEN_SHAPE(WindowOffered, 1, ZEN_FIELD(window), ZEN_FIELD(cols), ZEN_FIELD(rows),
              ZEN_FIELD(epoch));
};

/// The arranger -> the directory: where an area stands, in which window, whose office draws in it,
/// its order among the window's areas (a higher order stands in front), and whether the window
/// gives its keys to the owner.
struct AreaPlaced {
    std::string area;
    std::string window;
    std::string owner;
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    std::int64_t order = 0;
    bool keys = false;
    ZEN_SHAPE(AreaPlaced, 1, ZEN_FIELD(area), ZEN_FIELD(window), ZEN_FIELD(owner), ZEN_FIELD(x),
              ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h), ZEN_FIELD(order), ZEN_FIELD(keys));
};

/// The arranger -> the directory: a skin is painter code in this library, from the load plan.
struct SkinOffered {
    std::string skin;
    std::string library;
    ZEN_SHAPE(SkinOffered, 1, ZEN_FIELD(skin), ZEN_FIELD(library));
};

/// The arranger -> the directory: which skin paints an area.
struct SkinChosen {
    std::string area;
    std::string skin;
    ZEN_SHAPE(SkinChosen, 1, ZEN_FIELD(area), ZEN_FIELD(skin));
};

/// The arranger -> the directory, and the directory -> each window: the code that presents an
/// element type (its data shape's name) is in this library, from the load plan.
struct ElementCodeOffered {
    std::string type;
    std::string library;
    ZEN_SHAPE(ElementCodeOffered, 1, ZEN_FIELD(type), ZEN_FIELD(library));
};

/// The directory -> a window: this area is yours, drawn by `owner`, here, painted by the skin in
/// `library`. Said again whenever any of it changes, and to a window that offers itself again.
struct AreaIntroduced {
    std::string area;
    std::string owner;
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    std::int64_t order = 0;
    bool keys = false;
    std::string skin;
    std::string library;
    ZEN_SHAPE(AreaIntroduced, 1, ZEN_FIELD(area), ZEN_FIELD(owner), ZEN_FIELD(x), ZEN_FIELD(y),
              ZEN_FIELD(w), ZEN_FIELD(h), ZEN_FIELD(order), ZEN_FIELD(keys), ZEN_FIELD(skin),
              ZEN_FIELD(library));
};

/// The directory -> a window: this area is no longer yours.
struct AreaWithdrawn {
    std::string area;
    ZEN_SHAPE(AreaWithdrawn, 1, ZEN_FIELD(area));
};

/// The directory -> the owner of an area: draw into this window, as of this incarnation, in this
/// room. An owner draws whole only when the window or its incarnation changed.
struct AreaRoom {
    std::string area;
    std::string window;
    std::int64_t epoch = 0;
    std::int64_t w = 0, h = 0;
    ZEN_SHAPE(AreaRoom, 1, ZEN_FIELD(area), ZEN_FIELD(window), ZEN_FIELD(epoch), ZEN_FIELD(w),
              ZEN_FIELD(h));
};

/// An owner that starts again -> the directory, as its office: my room in this area. Answered with
/// `AreaRoom` once a window shows the area.
struct RoomAsked {
    std::string area;
    ZEN_SHAPE(RoomAsked, 1, ZEN_FIELD(area));
};

/// The directory -> everyone, on its activation: a directory started afresh; offer, place and
/// choose again.
struct GraphicsReady {
    ZEN_SHAPE(GraphicsReady, 1);
};

// ---- what a window shows ------------------------------------------------------------------

/// Anyone -> a window: what it shows and what it has spent. Window -> the asker: `WindowShown`.
struct WindowShowAsked {
    ZEN_SHAPE(WindowShowAsked, 1);
};

/// What a window shows, row by row as text, and its spending since it started: showings (writes
/// to its terminal) and their bytes, calls of its skins and element code, and its refusals.
struct WindowShown {
    std::string window;
    std::int64_t epoch = 0;
    std::vector<std::string> rows;
    std::int64_t areas = 0;
    std::int64_t nodes = 0;
    std::int64_t elements = 0;
    std::int64_t showings = 0;
    std::int64_t device_bytes = 0;
    std::int64_t painter_calls = 0;
    std::int64_t element_calls = 0;
    std::int64_t refusals = 0;
    std::string last_refusal;
    ZEN_SHAPE(WindowShown, 1, ZEN_FIELD(window), ZEN_FIELD(epoch), ZEN_FIELD(rows),
              ZEN_FIELD(areas), ZEN_FIELD(nodes), ZEN_FIELD(elements), ZEN_FIELD(showings),
              ZEN_FIELD(device_bytes), ZEN_FIELD(painter_calls), ZEN_FIELD(element_calls),
              ZEN_FIELD(refusals), ZEN_FIELD(last_refusal));
};

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_VOCABULARY_HPP
