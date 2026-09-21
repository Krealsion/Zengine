// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PRESENTER_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_PRESENTER_VOCABULARY_HPP

// THE SEAM BETWEEN WORKSHOP AND THE PARTICIPANT THAT PRESENTS A PANE'S OFFERED MENU -- installed
// beside the pane protocol (`zengine::pane`), so a presenter built against the package alone can
// hold the office, and one presenter can be replaced by another like any loaded weave.
//
//     MenuGranted     Workshop  -> presenter   "present this offer as menu n, for office/pane"
//     MenuShown       presenter -> Workshop    "menu n shows these lines now (picture p)"
//     MenuInput       Workshop  -> presenter   "the maker did this to menu n (their act #g)"
//     MenuClosed      presenter -> Workshop    "menu n is over: chosen at act #g, or not"
//     MenuReturned    presenter -> Workshop    "menu n is not mine to answer: take it back"
//     MenuWithdrawn   Workshop  -> presenter   "menu n is over: the host ended it, and why"
//     PresenterReady  presenter -> Workshop    "I hold the office now, carrying menu n (or 0)"
//
// ...and `PaneMenuAnswered` (pane_vocabulary.hpp), presenter -> the requesting office, AS THIS
// OFFICE, exactly once per menu it was granted.
//
// WHO OWNS WHAT -- three parties, and no question with two owners.
//
//   the REQUESTER   the office that offered the pane. What its rows mean, which subject they are
//                   about, whether a choice still applies when it arrives, and every operation.
//                   It asks WORKSHOP for a menu (`PaneMenuRequested`), continuing a gesture, and
//                   keeps a record of that ask in its own image (`pane_menu::Asked`).
//   WORKSHOP        input custody and display. It judges the ask (a continuation of the maker's
//                   latest act, from the office that offered the pane, a pane on the desk, a
//                   presenter in this office) and grants ONE menu at a time; forwards the maker's
//                   acts to it while it is open; draws the presenter's lines in a popup at the
//                   anchor; and ends it when custody moves -- a newer press elsewhere, a newer
//                   menu, the host's own menu, the pane leaving the desk or being offered again.
//                   It answers a requester itself only when nothing was presented (the ask was
//                   refused), when the presenter can no longer answer (it left -- even after
//                   the menu was withdrawn -- or the holder that replaced it does not carry the
//                   menu), which Loom tells it by refusing one of that menu's own sentences, or
//                   when the presenter GIVES THE INTERACTION BACK (`MenuReturned`).
//   the PRESENTER   the presentation and the interaction's lifetime: whether an offer can be
//                   shown, how its lines read, what a key or a press means on it, when it ends and
//                   with what outcome. It answers the requester. A choice is a fact about the
//                   maker's gesture and confers no authority on anybody.
//
// THE NUMBERS. A menu's number (`menu`) is Workshop's, minted per grant: a presenter's word about
// any other number moves nothing. The grant arrives under the REQUEST's correlation, and the
// presenter answers the requester under that same number, so the requester can tell which of its
// asks the answer settles. `MenuInput::input` is Workshop's count of the maker's acts at that
// input; a button's release carries its press's number, because the release completes the act
// the press began. `MenuClosed::input` names the act the presenter ended the menu on: Workshop
// records a choice as that act's continuation, so a requester's keyboard or management request
// continuing the choice is honored only while that act is still the maker's latest.
// `MenuShown::picture` numbers what the lines MEAN, as `v3::PaneContent::picture` does for a
// pane; `MenuInput::picture` is the picture the medium held when the act was read, so a presenter
// refuses a press aimed at lines it has since replaced.
//
// RELOAD AND REPLACEMENT. `HeldMenu` is the state the shipped presenters keep across a reload:
// any image with the same accepted set and this state may be reloaded in place of another, and
// finds the open menu -- it says `PresenterReady` naming it and shows it again, which is a
// deliberate HANDOFF of an interaction the maker is in the middle of. A holder that does not
// carry the open menu says `PresenterReady` with 0, and Workshop ends the menu and answers the
// requester itself. An interaction that reaches an image for a menu it does not hold -- an act
// or a withdrawal that overtook the arrival -- is GIVEN BACK (`MenuReturned`) rather than left
// in front of a maker with nobody to answer it. Neither rule binds a presenter that keeps
// different state; it is the policy these presenters ship.

#include "workshop/pane_vocabulary.hpp"

#include "surface/vocabulary.hpp"

#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// HOW MANY LINES A PRESENTER MAY ASK WORKSHOP TO DRAW FOR ONE MENU, and how long one may be: a
/// display bound, so a presenter cannot make the host hold or paint without limit. What the room
/// can show is narrower still (`MenuGranted::room_rows`, `room_columns`); the host fits a line to
/// the popup, and a press on a line it could not show names no line.
inline constexpr std::size_t kMaxMenuLines = 64;
inline constexpr std::size_t kMaxMenuLineLen = 256;

/// PRESENT THIS OFFER AS MENU `menu`. Workshop -> presenter, as `zengine.workshop`, under the
/// correlation of the request it judged eligible. `office` and `pane` name the requester -- the
/// office Loom authenticated on the request, and the pane it offered; `subject` and `rows` are
/// the request's, unread by the host. `room_rows`/`room_columns` are the most the popup can show
/// at its anchor on this screen now. A grant while another menu is open replaces it: Workshop has
/// withdrawn the older one first.
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

/// MENU `menu` SHOWS THESE LINES NOW. Presenter -> Workshop, as `zengine.presenter`. Each line is
/// drawn as a row of the popup, top to bottom, in its own role; a press on line i is forwarded as
/// line i. `picture` numbers what the lines mean -- kept by a change that moves only a highlight,
/// changed when a line would now choose something else. Words about a menu the host did not
/// grant, or lines past `kMaxMenuLines`, move nothing.
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

/// WHAT THE MAKER'S KEYMAP CALLS A KEY WHILE A MENU IS OPEN -- the contextual rows a maker can move
/// (`context.up`, `context.down`, `context.choose`, `context.back`; the opener's own key is
/// `kBack`). The presenter decides what each MEANS on its menu; `kNone` is a key no row names,
/// still carried with its scancode for a presenter that reads keys of its own (a digit, a letter).
namespace menu_verb {
inline constexpr std::int64_t kNone = 0;
inline constexpr std::int64_t kUp = 1;
inline constexpr std::int64_t kDown = 2;
inline constexpr std::int64_t kChoose = 3;
inline constexpr std::int64_t kBack = 4;
} // namespace menu_verb

/// THE MAKER DID THIS TO MENU `menu`. Workshop -> presenter, as `zengine.workshop`, while the menu
/// is open and in the order the acts were read. `input` is the host's count of the maker's acts at
/// this one (a release carries its press's); `line` is the popup row a press or release landed
/// on, -1 for none; `picture` is the menu picture the medium held when the act was read.
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

/// MENU `menu` IS OVER AND ITS REQUESTER IS ANSWERED. Presenter -> Workshop, as
/// `zengine.presenter`, no later than the answer it gives the requester: `chosen` when a row was
/// chosen, at act `input` (0 when no act ended it -- an offer it refused at the grant). Workshop
/// closes the popup and, for a choice, records the continuation a requester's next request is
/// judged against; for a menu it had already taken off the screen, this word is what tells it
/// nothing more is owed, and it stops keeping who asked. An interaction the presenter CANNOT
/// answer is `MenuReturned`, never this one: the two are not the same fact, and a host that read
/// them as one would either lose a requester or answer it twice.
struct MenuClosed {
    std::int64_t menu = 0;
    bool chosen = false;
    std::int64_t input = 0;
    ZEN_SHAPE(MenuClosed, 1, ZEN_FIELD(menu), ZEN_FIELD(chosen), ZEN_FIELD(input));
};

/// MENU `menu` IS NOT MINE TO ANSWER -- TAKE IT BACK. Presenter -> Workshop, as
/// `zengine.presenter`: this image was handed an interaction for a menu it does not hold -- one of
/// the maker's acts, or the host's withdrawal -- and it returns responsibility for any answer
/// still outstanding. It is the other half of `MenuClosed`: that says "over, and answered", this
/// says "not mine to carry -- you settle it". Workshop settles the requester itself, unchosen,
/// whether the menu is still on the screen or already withdrawn, and `why` is this image's own
/// words, carried into what the requester is told. It names ONE grant: a menu that is over and
/// settled takes nothing from it, and neither does a newer one, which is another grant with
/// another number.
///
/// (!) IT SAYS NOTHING ABOUT WHAT WAS ANSWERED BEFORE. An image that no longer holds a menu also
/// no longer knows whether an earlier image -- or this one, before a reload -- answered that
/// requester; a give-back is this image's word that IT cannot carry the interaction NOW, and
/// Workshop settles only a record it still retains. A menu already answered, and any newer one,
/// are unaffected. Reading it as "nobody has ever answered" would be reading a claim the
/// presenter does not make and could not keep.
///
/// (!) FOR A WITHDRAWAL, THE SUPPORTED TIMING IS SYNCHRONOUS. Workshop retires its record of a
/// withdrawn menu once its fence has come round (`WithdrawalFence`), so the guarantee that a
/// give-back finds a record to settle holds for one sent while handling that withdrawal --
/// before the host retires it. A return deferred past that is not guaranteed to find one, and
/// then nothing is settled by it; a presenter that must defer owes its requester an answer of
/// its own. This is the shipped policy and not a promise of deferred settlement.
struct MenuReturned {
    std::int64_t menu = 0;
    std::string why;
    ZEN_SHAPE(MenuReturned, 1, ZEN_FIELD(menu), ZEN_FIELD(why));
};

/// MENU `menu` IS OVER, AND THE HOST ENDED IT. Workshop -> presenter, as `zengine.workshop`:
/// custody moved (`why` says how) and nothing more of the maker's reaches the menu. The presenter
/// answers its requester unchosen, in these words. One Loom refuses -- no presenter holds the
/// office, or its holder has no door for this -- Workshop answers itself, as its own office.
struct MenuWithdrawn {
    std::int64_t menu = 0;
    std::string why;
    ZEN_SHAPE(MenuWithdrawn, 1, ZEN_FIELD(menu), ZEN_FIELD(why));
};

/// I HOLD THE OFFICE NOW. Presenter -> Workshop, as `zengine.presenter`, on every activation --
/// the first load and every reload. `menu` is the menu this image carries (0 for none): Workshop
/// keeps an open menu this holder names, and ends -- answering its requester itself -- one it
/// does not.
struct PresenterReady {
    std::int64_t menu = 0;
    ZEN_SHAPE(PresenterReady, 1, ZEN_FIELD(menu));
};

/// THE MENU A PRESENTER CARRIES ACROSS A RELOAD -- the shipped presenters' state, and so the
/// handoff contract between them: an image keeping it may be reloaded in place of another that
/// keeps it (Loom requires the same accepted set and a compatible state) and continues the
/// interaction the maker is in the middle of. `menu` 0 is none. Everything a presenter needs to
/// answer the requester is here: the number to answer under, the office, pane and subject to
/// answer about, and the rows and cursor to show again.
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
