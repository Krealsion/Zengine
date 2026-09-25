// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PRESENTER_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_PRESENTER_VOCABULARY_HPP

// The seam between Workshop and the participant that presents a pane's menu, installed beside the
// pane protocol so a presenter built against the package can hold `zengine.presenter` and be
// replaced like any loaded weave
// (docs/reference/workshop-panes.md#the-menu-presenter-and-replacing-it).
// The requester owns what its rows mean and every operation; Workshop owns input custody, the
// popup and one menu at a time; the presenter owns the menu's presentation and lifetime, and
// answers the requester exactly once per grant (`PaneMenuAnswered`, as this office). A menu's
// number is Workshop's, minted per grant; a word about any other number moves nothing.

#include "workshop/pane_vocabulary.hpp"

#include "surface/vocabulary.hpp"

#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// Display bounds on one menu's lines, so a presenter cannot make the host hold or paint without
/// limit; the popup's room (`MenuGranted::room_rows`, `room_columns`) is narrower still.
inline constexpr std::size_t kMaxMenuLines = 64;
inline constexpr std::size_t kMaxMenuLineLen = 256;

/// Workshop -> presenter: present this offer as menu `menu`, under the request's correlation.
/// `office`/`pane` name the requester as Loom authenticated it; `subject` and `rows` are the
/// request's, unread by the host; the room is the most the popup can show now. A grant while
/// another menu is open replaces it: the older one was withdrawn first.
struct MenuGranted {
    std::int64_t menu = 0;
    std::string office;
    std::string pane;
    std::string subject;
    std::vector<PaneMenuRow> rows;
    std::int64_t room_rows = 0;
    std::int64_t room_columns = 0;
    ZEN_SHAPE(MenuGranted, 1, ZEN_FIELD(menu), ZEN_FIELD(office), ZEN_FIELD(pane),
              ZEN_FIELD(subject), ZEN_FIELD(rows), ZEN_FIELD(room_rows), ZEN_FIELD(room_columns));
};

/// Presenter -> Workshop: menu `menu` shows these lines now, top to bottom; a press on line i is
/// forwarded as line i. `picture` numbers what the lines mean -- kept by a highlight moving,
/// changed when a line would now choose something else.
struct MenuShown {
    std::int64_t menu = 0;
    std::int64_t picture = 0;
    std::vector<surface::SurfaceTextRow> lines;
    ZEN_SHAPE(MenuShown, 1, ZEN_FIELD(menu), ZEN_FIELD(picture), ZEN_FIELD(lines));
};

/// WHAT KIND OF ACT A `MenuInput` CARRIES.
namespace menu_input {
inline constexpr std::int64_t kKey = 1;     ///< a key went down: `verb`, `scancode`, `modifiers`
inline constexpr std::int64_t kPress = 2;   ///< a button went down on the popup: `button`, `line`
inline constexpr std::int64_t kRelease = 3; ///< ...and came up, wherever the hand is: `line` or -1
inline constexpr std::int64_t kOutside = 4; ///< a press outside the popup, spent on it: `button`
} // namespace menu_input

/// What the maker's keymap calls a key while a menu is open (the contextual rows); what each means
/// is the presenter's. `kNone` still carries its scancode, for a presenter reading keys of its own.
namespace menu_verb {
inline constexpr std::int64_t kNone = 0;
inline constexpr std::int64_t kUp = 1;
inline constexpr std::int64_t kDown = 2;
inline constexpr std::int64_t kChoose = 3;
inline constexpr std::int64_t kBack = 4;
} // namespace menu_verb

/// Workshop -> presenter: the maker did this to menu `menu`, in the order read. `input` counts the
/// maker's acts (a release carries its press's number); `line` is -1 for none; `picture` is what
/// the medium held when the act was read, so a press aimed at replaced lines can be refused.
struct MenuInput {
    std::int64_t menu = 0;
    std::int64_t input = 0;
    std::int64_t kind = 0;
    std::int64_t verb = 0;
    std::int64_t scancode = 0;
    std::int64_t modifiers = 0;
    std::int64_t button = 0;
    std::int64_t line = -1;
    std::int64_t picture = 0;
    ZEN_SHAPE(MenuInput, 1, ZEN_FIELD(menu), ZEN_FIELD(input), ZEN_FIELD(kind), ZEN_FIELD(verb),
              ZEN_FIELD(scancode), ZEN_FIELD(modifiers), ZEN_FIELD(button), ZEN_FIELD(line),
              ZEN_FIELD(picture));
};

/// Presenter -> Workshop: menu `menu` is over and its requester answered -- `chosen` at act
/// `input` (0 when no act ended it). A choice is recorded as that act's continuation. Not
/// `MenuReturned`: "answered" and "not mine to carry" are different facts.
struct MenuClosed {
    std::int64_t menu = 0;
    bool chosen = false;
    std::int64_t input = 0;
    ZEN_SHAPE(MenuClosed, 1, ZEN_FIELD(menu), ZEN_FIELD(chosen), ZEN_FIELD(input));
};

/// Presenter -> Workshop: this image was handed an interaction for a menu it does not hold, and
/// returns responsibility for any answer still owed; Workshop settles the requester unchosen,
/// quoting `why`. It says nothing about earlier answers. For a withdrawal, send it while handling
/// that withdrawal: once the host's fence retires the record, a later give-back settles nothing.
struct MenuReturned {
    std::int64_t menu = 0;
    std::string why;
    ZEN_SHAPE(MenuReturned, 1, ZEN_FIELD(menu), ZEN_FIELD(why));
};

/// Workshop -> presenter: the host ended menu `menu` (`why` says how). The presenter answers its
/// requester unchosen, in these words; if Loom refuses this, Workshop answers itself.
struct MenuWithdrawn {
    std::int64_t menu = 0;
    std::string why;
    ZEN_SHAPE(MenuWithdrawn, 1, ZEN_FIELD(menu), ZEN_FIELD(why));
};

/// Presenter -> Workshop, on every activation: I hold the office, carrying menu `menu` (0 for
/// none). Workshop keeps an open menu this holder names, and ends one it does not, answering
/// its requester itself.
struct PresenterReady {
    std::int64_t menu = 0;
    ZEN_SHAPE(PresenterReady, 1, ZEN_FIELD(menu));
};

/// The menu the shipped presenters carry across a reload, and so their handoff contract: an image
/// keeping it may replace another that keeps it and continue the maker's interaction. It holds
/// everything needed to answer the requester and show the menu again; `menu` 0 is none.
struct HeldMenu {
    std::int64_t menu = 0;
    std::int64_t correlation = 0;
    std::string office;
    std::string pane;
    std::string subject;
    std::vector<PaneMenuRow> rows;
    std::int64_t cursor = 0;
    std::int64_t room_rows = 0;
    std::int64_t room_columns = 0;
    ZEN_SHAPE(HeldMenu, 1, ZEN_FIELD(menu), ZEN_FIELD(correlation), ZEN_FIELD(office),
              ZEN_FIELD(pane), ZEN_FIELD(subject), ZEN_FIELD(rows), ZEN_FIELD(cursor),
              ZEN_FIELD(room_rows), ZEN_FIELD(room_columns));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PRESENTER_VOCABULARY_HPP
