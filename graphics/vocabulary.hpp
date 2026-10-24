// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_VOCABULARY_HPP
#define ZENGINE_GRAPHICS_VOCABULARY_HPP

// The graphics vocabulary: one primitive (a node), the element record beside it, and the messages
// between an owner, the window that shows its area, and the directory that introduces the two.
// An owner draws nodes into its window, or says an element once and then sends only its changes,
// which code the window loads turns into nodes. The directory stays off that path. A window also
// answers for its platform: the clipboard, its place on the desktop, a close, and its last picture.
// Reference: graphics/docs/graphics.md.

#include "graphics/role.hpp"

#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::graphics {

/// The directory's role: which windows exist, which areas each shows, who draws in each, which
/// skin paints it and which code presents each element type.
inline constexpr const char* kGraphicsRole = "zengine.graphics";
/// A window's office is this prefix and its name: `zengine.window.terminal`.
inline constexpr const char* kWindowOfficePrefix = "zengine.window.";
/// The terminal window's name: one terminal to a process, so one terminal window.
inline constexpr const char* kTerminalWindow = "terminal";
/// The OS window's name: a window of light on the desktop, drawn through SDL.
inline constexpr const char* kSdlWindow = "sdl";

/// The bounds one area keeps in its window. A drawing past any of them is refused whole, in words
/// sent to its drawer.
inline constexpr std::size_t kMaxNodesPerArea = 4096;
inline constexpr std::size_t kMaxElementsPerArea = 1024;
inline constexpr std::size_t kMaxPathBytes = 256;
inline constexpr std::size_t kMaxPathDepth = 16;
inline constexpr std::size_t kMaxArgsBytes = 64 * 1024;
inline constexpr std::size_t kMaxAreas = 256;
/// The most runs of text one reading of an area says (`AreaShown`): an area showing more is refused
/// its reading, in words, rather than said in part.
inline constexpr std::size_t kMaxAreaWords = 4096;
/// The pixels of one terminal cell: a window of characters floors each edge to its cell.
inline constexpr std::int64_t kCellPx = 12;

/// How a window sets text, in pixels: the advance from one character to the next and the height
/// of a line. A window of light says its face's; a window of characters says the cell's, so text
/// stands one character a cell there.
struct TextMeasure {
    std::int64_t advance = kCellPx;
    std::int64_t line = kCellPx;
    friend bool operator==(const TextMeasure&, const TextMeasure&) = default;
};

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

/// What a named part of a drawing says of itself, as its record's value beside the action it
/// names: whether what the action does applies now. A window says it in its reading of the area
/// (`AreaShown`); a part said with any other record, or none, reads available.
struct PartFace {
    bool available = true;
    ZEN_SHAPE(PartFace, 1, ZEN_FIELD(available));
};

/// What one drawer changed in one area, applied together. `whole` first removes everything the
/// drawer may draw there. A node put again changes in its place; a new one goes after its
/// siblings; removing a node removes its subtree and its records. `more` says more of the same
/// beat's drawing follows from this drawer, so the window shows once, after the last. `number` is
/// the drawer's own for what its drawing means, 0 for none: the window says it with each press
/// that lands there (`AreaButtonChanged::drawing`).
struct Drawn {
    std::string area;
    bool whole = false;
    std::vector<Node> put;
    std::vector<std::string> removed;
    std::vector<ElementRecord> said;
    std::vector<std::string> unsaid;
    bool more = false;
    std::int64_t number = 0;
    ZEN_SHAPE(Drawn, 2, ZEN_FIELD(area), ZEN_FIELD(whole), ZEN_FIELD(put), ZEN_FIELD(removed),
              ZEN_FIELD(said), ZEN_FIELD(unsaid), ZEN_FIELD(more), ZEN_FIELD(number));
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

/// Window -> the area's owner: perform this action, named by the element a left press landed in,
/// in an area not placed to hear the pointer.
struct ActionRequested {
    std::string area;
    std::string action;
    ZEN_SHAPE(ActionRequested, 1, ZEN_FIELD(area), ZEN_FIELD(action));
};

// ---- the pointer in an area ---------------------------------------------------------------
//
// A window gives the owner of an area placed to hear the pointer every pointer moment there, as
// the moment it was, with the area it landed in and its point in that area's pixels; `modifiers`
// are the input vocabulary's bits. A press holds its button for its area until that button's
// release: the release, and every move while it is held, go to that area wherever the pointer is,
// at a point that may lie outside it. A press, the moves it holds and its release carry one number,
// the press's, as their delivery's correlation, and a continuation an owner sends about the press
// cites it; a release no hold names carries none.
//
// Every moment says whose hand made it: `local` for the weaver's own, which the window reads from
// its platform, and else `actor`, the participant the input office names for an injected moment
// (`input::AttributedInput`). Each hand holds its own presses.

/// Window -> the owner of an area placed to hear the pointer: a button pressed or let go there,
/// 1 left, 2 middle, 3 right; and what the window drew under the hand when it landed. `under` is
/// the area that showed it -- this one, or where this one shows nothing there the first area
/// behind it that does, "" for none -- `part` the part named there (the action of the record over
/// the top node, "" for none) and `drawing` that area's drawer's number for its drawing
/// (`Drawn::number`).
struct AreaButtonChanged {
    std::string area;
    std::int64_t button = 0;
    bool pressed = false;
    std::int64_t x = 0, y = 0;
    std::int64_t modifiers = 0;
    bool local = true;
    std::int64_t actor = 0;
    std::string under{};
    std::string part{};
    std::int64_t drawing = 0;
    ZEN_SHAPE(AreaButtonChanged, 3, ZEN_FIELD(area), ZEN_FIELD(button), ZEN_FIELD(pressed),
              ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(modifiers), ZEN_FIELD(local),
              ZEN_FIELD(actor), ZEN_FIELD(under), ZEN_FIELD(part), ZEN_FIELD(drawing));
};

/// Window -> the owner of an area placed to hear the pointer: the pointer moved there, or moved
/// anywhere while a press in the area is held; moves read together go as one, at the last point,
/// their deltas, in pixels, summed.
struct AreaPointerMoved {
    std::string area;
    std::int64_t x = 0, y = 0;
    std::int64_t dx = 0, dy = 0;
    std::int64_t modifiers = 0;
    bool local = true;
    std::int64_t actor = 0;
    ZEN_SHAPE(AreaPointerMoved, 2, ZEN_FIELD(area), ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(dx),
              ZEN_FIELD(dy), ZEN_FIELD(modifiers), ZEN_FIELD(local), ZEN_FIELD(actor));
};

/// Window -> the owner of an area placed to hear the pointer and the wheel: the wheel turned there,
/// +1.0 a notch away from the weaver, `dx` a horizontal wheel's.
struct AreaWheelTurned {
    std::string area;
    double dx = 0, dy = 0;
    std::int64_t x = 0, y = 0;
    std::int64_t modifiers = 0;
    bool local = true;
    std::int64_t actor = 0;
    ZEN_SHAPE(AreaWheelTurned, 2, ZEN_FIELD(area), ZEN_FIELD(dx), ZEN_FIELD(dy), ZEN_FIELD(x),
              ZEN_FIELD(y), ZEN_FIELD(modifiers), ZEN_FIELD(local), ZEN_FIELD(actor));
};

/// Window -> the arranger following an area placed to hear the pointer (`AreaPlaced::followed`),
/// when another office owns it: a button pressed or let go in that area, at the window's own point
/// in pixels, under the press's number, whose hand pressed it, and what the window drew under the
/// hand, as the owner is told it (`AreaButtonChanged`). Where presses land is the arranger's to
/// know, as a desktop raises what was clicked; the moment itself is the owner's.
struct AreaPressed {
    std::string window;
    std::string area;
    std::int64_t button = 0;
    bool pressed = false;
    std::int64_t x = 0, y = 0;
    bool local = true;
    std::int64_t actor = 0;
    std::string under{};
    std::string part{};
    std::int64_t drawing = 0;
    ZEN_SHAPE(AreaPressed, 3, ZEN_FIELD(window), ZEN_FIELD(area), ZEN_FIELD(button),
              ZEN_FIELD(pressed), ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(local), ZEN_FIELD(actor),
              ZEN_FIELD(under), ZEN_FIELD(part), ZEN_FIELD(drawing));
};

/// Window -> the arranger of an area placed for the weaver's hand alone (`AreaPlaced::weaver_only`):
/// a guest's press or wheel landed there and was refused, given to nobody, at the window's own
/// point in pixels. The arranger says why; the moves and the release that follow it are dropped.
struct AreaHandRefused {
    std::string window;
    std::string area;
    std::int64_t actor = 0;
    std::int64_t x = 0, y = 0;
    ZEN_SHAPE(AreaHandRefused, 1, ZEN_FIELD(window), ZEN_FIELD(area), ZEN_FIELD(actor),
              ZEN_FIELD(x), ZEN_FIELD(y));
};

/// A window, as its office, to the area's owner and to its arranger when another office following
/// it: the press held for `area` ended with no release, because the area was withdrawn or handed to
/// another owner, or an area placed to hear the pointer now stands in front of it where the press
/// was made. Its button's release, when it comes, goes where it lands. No other office hears it,
/// and an office that keeps no gesture need not accept it.
struct AreaHoldEnded {
    std::string area;
    std::int64_t button = 0;
    ZEN_SHAPE(AreaHoldEnded, 1, ZEN_FIELD(area), ZEN_FIELD(button));
};

/// A window, as its office, to the area's owner and to its arranger when another office: the
/// pointer, moving with no button held, left `area`, an area placed to hear the pointer that its
/// last move landed in. Told as `AreaHoldEnded` is.
struct AreaPointerLeft {
    std::string area;
    ZEN_SHAPE(AreaPointerLeft, 1, ZEN_FIELD(area));
};

// ---- the keys in an area ------------------------------------------------------------------
//
// A window gives every key it reads, its release and the text it types to the owner of each area
// placed to hear that moment (`AreaPlaced::keys`, `releases`, `text`), front to back, naming that
// area and the hand, as a pointer moment does:
// `local` for the weaver's own, which the window reads from its platform. A guest's keys reach no
// window: the input office says them, attributed (`input::AttributedInput`). `scancode`, `name`,
// `modifiers` and `text` are the input vocabulary's (`input::KeyPressed`, `input::TextEntered`).

/// Window -> the owner of an area placed to hear keys (`AreaPlaced::keys`): a key went down.
struct AreaKeyPressed {
    std::string area;
    std::int64_t scancode = 0;
    std::string name;
    std::int64_t modifiers = 0;
    bool local = true;
    std::int64_t actor = 0;
    ZEN_SHAPE(AreaKeyPressed, 1, ZEN_FIELD(area), ZEN_FIELD(scancode), ZEN_FIELD(name),
              ZEN_FIELD(modifiers), ZEN_FIELD(local), ZEN_FIELD(actor));
};

/// Window -> the owner of an area placed to hear their releases (`AreaPlaced::releases`): a key came
/// up.
struct AreaKeyReleased {
    std::string area;
    std::int64_t scancode = 0;
    std::string name;
    std::int64_t modifiers = 0;
    bool local = true;
    std::int64_t actor = 0;
    ZEN_SHAPE(AreaKeyReleased, 1, ZEN_FIELD(area), ZEN_FIELD(scancode), ZEN_FIELD(name),
              ZEN_FIELD(modifiers), ZEN_FIELD(local), ZEN_FIELD(actor));
};

/// Window -> the owner of an area placed to hear text (`AreaPlaced::text`): the text a key typed, as
/// UTF-8.
struct AreaTextEntered {
    std::string area;
    std::string text;
    bool local = true;
    std::int64_t actor = 0;
    ZEN_SHAPE(AreaTextEntered, 1, ZEN_FIELD(area), ZEN_FIELD(text), ZEN_FIELD(local),
              ZEN_FIELD(actor));
};

// ---- the directory ------------------------------------------------------------------------

/// A window, as its own office, to whoever follows it -- the directory, and an arranger laying out
/// in its room -- on each activation: its name, its room in pixels (a terminal's cells, `kCellPx`
/// pixels each; none where it has no terminal to measure), its incarnation, its device pixels in
/// one cell (`cell_px`: `kCellPx` where it shows every pixel, none where its unit is the cell, as a
/// terminal's), and how it sets text, a character's advance and a line's height in pixels: the
/// face it draws with, measured, or the cell. A window started afresh holds nothing, so its owners
/// draw whole again.
struct WindowOffered {
    std::string window;
    std::int64_t width = 0, height = 0;
    std::int64_t epoch = 0;
    std::int64_t cell_px = 0;
    std::int64_t text_advance_px = 0, text_line_px = 0;
    ZEN_SHAPE(WindowOffered, 3, ZEN_FIELD(window), ZEN_FIELD(width), ZEN_FIELD(height),
              ZEN_FIELD(epoch), ZEN_FIELD(cell_px), ZEN_FIELD(text_advance_px),
              ZEN_FIELD(text_line_px));
};

/// The longest title a window takes, in bytes; a longer one is cut where a character begins.
inline constexpr std::size_t kMaxWindowTitleBytes = 256;

/// An arranger -> a window's office, as the office that arranges an area in that window: the
/// window's title, and its size in pixels. The window takes it only from an office that arranges in
/// it and only when it names that window; anything else changes nothing. An OS window sets both,
/// keeping a size past its display's usable area within it, a side of 0 keeping that side as it is,
/// and a title cut to `kMaxWindowTitleBytes`, an empty one keeping its own; a terminal keeps its
/// own size and title. A window
/// whose size changed offers itself again, and its areas grow it back only by reaching further.
struct WindowShaped {
    std::string window;
    std::string title;
    std::int64_t width = 0, height = 0;
    ZEN_SHAPE(WindowShaped, 1, ZEN_FIELD(window), ZEN_FIELD(title), ZEN_FIELD(width),
              ZEN_FIELD(height));
};

/// The arranger -> the directory: where an area stands, in which window, whose office draws in it,
/// its order among the window's areas (a higher order stands in front), whether the window gives
/// the owner each key pressed (`keys`), each key's release (`releases`) and the text keys type
/// (`text`) -- an owner places its areas for the moments it takes, and is given no other --
/// whether it gives the owner its pointer (`AreaButtonChanged` and the rest), whether the weaver's
/// own hand alone reaches it: a guest's press there is refused (`AreaHandRefused`), whether an area
/// that hears the pointer hears the wheel and the third button too: one that does not lets them
/// through to the area behind it that does, and stands in front of no third-button press held
/// there, and whether its arranger follows it: told where each press and release there landed while
/// another office owns it (`AreaPressed`), that a hold ended or the pointer left (`AreaHoldEnded`,
/// `AreaPointerLeft`), and each time what it shows settles (`AreaSettled`). An arranger that takes
/// none of these places its areas without.
struct AreaPlaced {
    std::string area;
    std::string window;
    std::string owner;
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    std::int64_t order = 0;
    bool keys = false;
    bool pointer = false;
    bool weaver_only = false;
    bool wheel = true;
    bool right = true;
    bool followed = false;
    bool releases = false;
    bool text = false;
    ZEN_SHAPE(AreaPlaced, 7, ZEN_FIELD(area), ZEN_FIELD(window), ZEN_FIELD(owner), ZEN_FIELD(x),
              ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h), ZEN_FIELD(order), ZEN_FIELD(keys),
              ZEN_FIELD(pointer), ZEN_FIELD(weaver_only), ZEN_FIELD(wheel), ZEN_FIELD(right),
              ZEN_FIELD(followed), ZEN_FIELD(releases), ZEN_FIELD(text));
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
/// `library`; `arranger` is the office that placed it, as its placing was authored, empty for
/// personal speech; `weaver_only`, `wheel`, `right`, `followed`, `releases` and `text` as it was
/// placed. Said again whenever any of it changes, to a window that offers itself again, and when
/// the owner asks its room; the window answers each with the owner's room.
struct AreaIntroduced {
    std::string area;
    std::string owner;
    std::string arranger;
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    std::int64_t order = 0;
    bool keys = false;
    bool pointer = false;
    std::string skin;
    std::string library;
    bool weaver_only = false;
    bool wheel = true;
    bool right = true;
    bool followed = false;
    bool releases = false;
    bool text = false;
    ZEN_SHAPE(AreaIntroduced, 8, ZEN_FIELD(area), ZEN_FIELD(owner), ZEN_FIELD(arranger),
              ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h), ZEN_FIELD(order),
              ZEN_FIELD(keys), ZEN_FIELD(pointer), ZEN_FIELD(skin), ZEN_FIELD(library),
              ZEN_FIELD(weaver_only), ZEN_FIELD(wheel), ZEN_FIELD(right), ZEN_FIELD(followed),
              ZEN_FIELD(releases), ZEN_FIELD(text));
};

/// The directory -> a window: this area is no longer yours.
struct AreaWithdrawn {
    std::string area;
    ZEN_SHAPE(AreaWithdrawn, 1, ZEN_FIELD(area));
};

/// A window -> the owner of an area it was introduced, as the window's office: draw into this
/// window, as of this incarnation, in this room; `elements` are the references of the owner's
/// elements the window still presents there, and `empty` says the window holds none of the
/// owner's drawing there, as when it shows the area afresh or gives it to this owner. `cell_px`,
/// `text_advance_px` and `text_line_px` are the window's, as it offers them, so an owner sets its
/// text by the measure of the window that shows it. An owner draws whole only when the window, its
/// incarnation, its size or its measure changed or the area is empty, and says again only an
/// element the window does not hold.
struct AreaRoom {
    std::string area;
    std::string window;
    std::int64_t epoch = 0;
    std::int64_t w = 0, h = 0;
    std::vector<std::int64_t> elements;
    bool empty = false;
    std::int64_t cell_px = 0;
    std::int64_t text_advance_px = kCellPx, text_line_px = kCellPx;
    ZEN_SHAPE(AreaRoom, 4, ZEN_FIELD(area), ZEN_FIELD(window), ZEN_FIELD(epoch), ZEN_FIELD(w),
              ZEN_FIELD(h), ZEN_FIELD(elements), ZEN_FIELD(empty), ZEN_FIELD(cell_px),
              ZEN_FIELD(text_advance_px), ZEN_FIELD(text_line_px));

    /// How the window sets text: its measure, or the cell's where it said none.
    TextMeasure measure() const {
        return TextMeasure{text_advance_px > 0 ? text_advance_px : kCellPx,
                           text_line_px > 0 ? text_line_px : kCellPx};
    }
};

/// An owner that starts again -> the directory, as its office: my room in this area. Answered by
/// the window that shows the area, once one does.
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

/// An area's arranger -> the window that shows it: what the area shows now. `since` is the
/// `version` of the last answer the arranger took, or 0; an area whose version is still that is
/// answered `same`, with nothing else. Window -> the arranger: `AreaShown`; anyone else, and an
/// area this window does not show, is refused in words.
struct AreaShowAsked {
    std::string area;
    std::int64_t since = 0;
    ZEN_SHAPE(AreaShowAsked, 1, ZEN_FIELD(area), ZEN_FIELD(since));
};

/// One run of text as an area shows it: what its text says on one line, its characters side by
/// side, from `x`, `y` in the area's pixels, each the window's advance wide and a line high. A run
/// ends where no text stands, where an area in front shows instead -- one the asker arranges aside,
/// whose cover is its own to judge -- or at the area's edge; the blanks after its last character
/// are left off.
struct AreaWord {
    std::int64_t x = 0, y = 0;
    std::string text;
    ZEN_SHAPE(AreaWord, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(text));
};

/// One named part as an area shows it: a record naming an action, which is its name. `x`, `y`,
/// `w`, `h` bound what its drawing shows, in the area's pixels; `pressed` says a press at `px`, `py`
/// reaches it, as the window lands a press, and no press reaches it where nothing of it can be
/// pressed. `available` is what its record says (`PartFace`), else true.
struct AreaPart {
    std::string name;
    bool available = true;
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    bool pressed = false;
    std::int64_t px = 0, py = 0;
    ZEN_SHAPE(AreaPart, 1, ZEN_FIELD(name), ZEN_FIELD(available), ZEN_FIELD(x), ZEN_FIELD(y),
              ZEN_FIELD(w), ZEN_FIELD(h), ZEN_FIELD(pressed), ZEN_FIELD(px), ZEN_FIELD(py));
};

/// What one area shows after the window's composition, to the office that placed it: its words
/// and its named parts in the area's pixels, as many as the area holds and no more. `version`
/// moves whenever what the area shows may have moved -- its drawing, its place, or an area that
/// meets it; `same` answers an ask whose `since` is still the version, saying nothing more.
/// `waiting` says the window told the owner a room it must draw whole and none has come since.
/// `advance` and `line` are the window's text measure.
struct AreaShown {
    std::string window;
    std::string area;
    std::int64_t epoch = 0;
    std::int64_t version = 0;
    bool same = false;
    bool waiting = false;
    std::int64_t advance = kCellPx, line = kCellPx;
    std::vector<AreaWord> words;
    std::vector<AreaPart> parts;
    ZEN_SHAPE(AreaShown, 1, ZEN_FIELD(window), ZEN_FIELD(area), ZEN_FIELD(epoch),
              ZEN_FIELD(version), ZEN_FIELD(same), ZEN_FIELD(waiting), ZEN_FIELD(advance),
              ZEN_FIELD(line), ZEN_FIELD(words), ZEN_FIELD(parts));
};

/// Window -> the office that placed an area and follows it (`AreaPlaced::followed`), unasked: what
/// the area shows settled at `version` (`AreaShown::version`), told once each time the window shows
/// with the version moved since it last told. A late or repeated one names a version already read.
struct AreaSettled {
    std::string window;
    std::string area;
    std::int64_t version = 0;
    ZEN_SHAPE(AreaSettled, 1, ZEN_FIELD(window), ZEN_FIELD(area), ZEN_FIELD(version));
};

// ---- what a medium answers for its platform -------------------------------------------------
//
// The clipboard, the window's place on its desktop, a close its desktop asked for, and the picture
// last presented: each window that shows answers them, as its office, as far as its platform can.

/// Give the medium a turn to service its platform now, such as an OS window's event queue: the
/// work its own beat does, on request, for suites and for hosts with no Timer.
struct PumpSurface {
    ZEN_SHAPE(PumpSurface, 1);
};

/// The medium was asked to close, as by a window's close box: a lifecycle fact, not an input
/// moment, so it is never confused with a weaver's quit key. Whoever hears it applies its own quit
/// policy, which may be to stay. No terminal is asked; an OS window publishes it as its office.
struct SurfaceCloseRequested {
    ZEN_SHAPE(SurfaceCloseRequested, 1);
};

/// Where the medium's window sits on its desktop, in the platform's desktop units. A follower
/// treats `x`/`y` as opaque: it may remember them and offer them back
/// (`SurfacePlacementRemembered`), never interpret them. They are always the normal (unmaximized)
/// window's top-left, remembered across a maximized stretch; `maximized` is the current state.
/// Published when it changes, before the room when both change. A medium with no desktop window
/// (every terminal) publishes nothing: `(0,0)` is a real place.
struct SurfacePlacement {
    std::int64_t x = 0;
    std::int64_t y = 0;
    bool maximized = false;
    ZEN_SHAPE(SurfacePlacement, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(maximized));
};

/// A remembered placement, offered back once by whoever restores a session: a want, not an
/// instruction. The medium judges it against the displays that exist now, keeping the window whole
/// on one, and with no display nothing moves. What comes back is the next `SurfacePlacement`, never
/// an echo; a terminal does nothing.
struct SurfacePlacementRemembered {
    std::int64_t x = 0;
    std::int64_t y = 0;
    bool maximized = false;
    ZEN_SHAPE(SurfacePlacementRemembered, 1, ZEN_FIELD(x), ZEN_FIELD(y),
              ZEN_FIELD(maximized));
};

/// A weaver copied this text. A publication, because several parties hear it: the medium sets the
/// platform clipboard as far as it can (on a terminal, OSC 52, with no claim that it took), and
/// every participant that mirrors a clipboard keeps copy-and-paste true in-process.
struct ClipboardCopy {
    std::string text;
    ZEN_SHAPE(ClipboardCopy, 1, ZEN_FIELD(text));
};

/// What does the platform clipboard hold now? Asked because a weaver pressed paste, and for no
/// other reason: nothing observes the system clipboard, and this ask's answer is the one road its
/// text has onto the bus. The asker settles the answer as any ask (`loom::AskBook`) and applies it
/// to the draft that asked, or discards it.
struct ClipboardTextRequested {
    ZEN_SHAPE(ClipboardTextRequested, 1);
};

/// The medium's answer. `readable=false` (every terminal) means it cannot say, and the asker
/// pastes what this process last copied; `readable=true` with empty `text` means the platform
/// holds no text, and the paste inserts nothing. Two fields, so an empty platform clipboard never
/// pastes a stale copy.
struct ClipboardText {
    bool readable = false;
    std::string text;
    ZEN_SHAPE(ClipboardText, 1, ZEN_FIELD(readable), ZEN_FIELD(text));
};

// A capture: what the medium presented, through its own medium. A picture is evidence of
// presentation at one frame and of nothing else: it proves nothing about queued work.
// `after_frame` defers the answer until a later frame is presented -- anyone's -- which is not the
// frame that shows a given input: ordering after input is the injector's host's (a fence, or a link
// ask's `settle`). One picture is retained at a time and fetched by chunk, one request may wait for
// its frame at a time, and a picture over `kMaxCaptureBytes` is refused, never cut.

/// TAKE A PICTURE, now or once a later frame has been presented. Answered `SurfaceCaptured`.
struct SurfaceCaptureRequested {
    /// Capture when `frames > after_frame`. -1 (the default) means now, whatever frame stands;
    /// a value at or beyond the current count defers the answer to the frame that passes it.
    std::int64_t after_frame = -1;
    ZEN_SHAPE(SurfaceCaptureRequested, 1, ZEN_FIELD(after_frame));
};

/// THE PICTURE'S IDENTITY AND SHAPE -- not its bytes, which are fetched by chunk. `format` is
/// the medium's honest word: `image/bmp` for a window of light (24-bit, bottom-up, as a BMP is),
/// or `text/cells` for a terminal (`height` rows of `width` bytes, each plus a newline). `cell_px`
/// is the graphical medium's cell size, so a pixel coordinate in the picture maps to the cell
/// lattice a pointer moment is spelled in; 0 on a terminal, where a cell IS the unit.
struct SurfaceCaptured {
    bool ok = false;
    std::int64_t capture = 0; ///< this picture's number; the chunk door names it
    std::int64_t frame = 0;   ///< the medium's frame count when it was taken
    std::int64_t width = 0;   ///< pixels, or cells on a terminal
    std::int64_t height = 0;
    std::int64_t cell_px = 0;
    std::string format;
    std::int64_t bytes = 0; ///< the whole picture's size, in bytes
    std::string refusal;    ///< why not, when `ok` is false
    ZEN_SHAPE(SurfaceCaptured, 1, ZEN_FIELD(ok), ZEN_FIELD(capture), ZEN_FIELD(frame),
              ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(cell_px), ZEN_FIELD(format),
              ZEN_FIELD(bytes), ZEN_FIELD(refusal));
};

/// FETCH ONE CHUNK of the retained picture, from `offset`. Answered `SurfaceCaptureChunk`, or
/// `zen.Refused` when `capture` is no longer the one retained.
struct SurfaceCaptureChunkRequested {
    std::int64_t capture = 0;
    std::int64_t offset = 0;
    ZEN_SHAPE(SurfaceCaptureChunkRequested, 1, ZEN_FIELD(capture), ZEN_FIELD(offset));
};

struct SurfaceCaptureChunk {
    std::int64_t capture = 0;
    std::int64_t offset = 0;
    std::int64_t total = 0; ///< the picture's size; `offset + data.size() == total` on the last
    loom::Bytes data;
    ZEN_SHAPE(SurfaceCaptureChunk, 1, ZEN_FIELD(capture), ZEN_FIELD(offset), ZEN_FIELD(total),
              ZEN_FIELD(data));
};

/// The most bytes one chunk carries: small enough for any crossing's frame and any bus's
/// decode budget, large enough that a screen is a few hundred round trips and not thousands.
inline constexpr std::int64_t kCaptureChunkBytes = 32 * 1024;
/// The largest picture a medium retains. A 4K window at 24 bits is under this; a larger one is
/// refused with its size, never silently cut.
inline constexpr std::int64_t kMaxCaptureBytes = 32 * 1024 * 1024;

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_VOCABULARY_HPP
