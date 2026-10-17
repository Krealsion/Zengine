// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PRESENTER_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_PRESENTER_VOCABULARY_HPP

// The seam between Workshop and the participant that presents a pane's menu, installed beside the
// pane protocol so a presenter built against the package can hold `zengine.presenter` and be
// replaced like any loaded weave
// (workshop/docs/workshop-panes.md#the-menu-presenter-and-replacing-it).
// The requester owns what its rows mean and every operation; Workshop owns input custody, the
// popup, one menu at a time, and the standard rows it adds beneath the requester's -- its own pane
// menu, which it spends itself; the presenter owns the menu's presentation and lifetime, and
// answers the requester exactly once per grant (`PaneMenuAnswered`, as this office). A menu's
// number is Workshop's, minted per grant; a word about any other number moves nothing.

#include "workshop/pane_menu.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "surface/vocabulary.hpp"

#include <zen/weave.hpp>
#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace zengine::workshop {

/// Display bounds on one menu's lines, so a presenter cannot make the host hold or paint without
/// limit; the popup's room (`MenuGranted::room_rows`, `room_columns`) is narrower still.
inline constexpr std::size_t kMaxMenuLines = 64;
inline constexpr std::size_t kMaxMenuLineLen = 256;

/// Workshop -> presenter: present this offer as menu `menu`, under the request's correlation.
/// `office`/`pane` name the requester as Loom authenticated it; `subject` and `rows` are the
/// request's, unread by the host; the room is the most the popup can show now. `standard` is the
/// host's own pane menu for that pane, shown beneath the requester's rows in the same menu: a
/// choice of one is the host's to spend (`MenuClosed::standard`), never the requester's.
/// `pane_name` is that pane's name as the weaver knows it, for whatever sets the standard rows
/// apart to say whose they are. A grant while another menu is open replaces it: the older one was
/// withdrawn first.
struct MenuGranted {
    std::int64_t menu = 0;
    std::string office;
    std::string pane;
    std::string subject;
    std::vector<PaneMenuRow> rows;
    std::int64_t room_rows = 0;
    std::int64_t room_columns = 0;
    std::vector<PaneMenuRow> standard;
    std::string pane_name;
    ZEN_SHAPE(MenuGranted, 2, ZEN_FIELD(menu), ZEN_FIELD(office), ZEN_FIELD(pane),
              ZEN_FIELD(subject), ZEN_FIELD(rows), ZEN_FIELD(room_rows), ZEN_FIELD(room_columns),
              ZEN_FIELD(standard), ZEN_FIELD(pane_name));
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

namespace v2 {

/// `MenuShown` naming its lines: a line that shows a row is named by the row's id, as a pane's
/// row is named (`PaneRowPart`, its `row` the line), under v4 content's rule -- a name once, on a
/// line it shows, inside the room granted. A marker or a rule is named nothing.
struct MenuShown {
    std::int64_t menu = 0;
    std::int64_t picture = 0;
    std::vector<surface::SurfaceTextRow> lines;
    std::vector<zengine::workshop::PaneRowPart> parts;
    ZEN_SHAPE(MenuShown, 2, ZEN_FIELD(menu), ZEN_FIELD(picture), ZEN_FIELD(lines),
              ZEN_FIELD(parts));
};

} // namespace v2

/// WHAT KIND OF ACT A `MenuInput` CARRIES.
namespace menu_input {
inline constexpr std::int64_t kKey = 1;     ///< a key went down: `verb`, `scancode`, `modifiers`
inline constexpr std::int64_t kPress = 2;   ///< a button went down on the popup: `button`, `line`
inline constexpr std::int64_t kRelease = 3; ///< ...and came up, wherever the hand is: `line` or -1
inline constexpr std::int64_t kOutside = 4; ///< a press outside the popup, spent on it: `button`
} // namespace menu_input

/// What the weaver's keymap calls a key while a menu is open (the contextual rows); what each means
/// is the presenter's. `kNone` still carries its scancode, for a presenter reading keys of its own.
namespace menu_verb {
inline constexpr std::int64_t kNone = 0;
inline constexpr std::int64_t kUp = 1;
inline constexpr std::int64_t kDown = 2;
inline constexpr std::int64_t kChoose = 3;
inline constexpr std::int64_t kBack = 4;
} // namespace menu_verb

/// Workshop -> presenter: the weaver did this to menu `menu`, in the order read. `input` counts the
/// weaver's acts (a release carries its press's number); `line` is -1 for none; `picture` is what
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
/// `input` (0 when no act ended it). A choice is recorded as that act's continuation. `standard`
/// is the id of the granted standard row chosen at that act, "" for none; the requester is then
/// answered unchosen, and Workshop spends the row. Not `MenuReturned`: "answered" and "not mine
/// to carry" are different facts.
struct MenuClosed {
    std::int64_t menu = 0;
    bool chosen = false;
    std::int64_t input = 0;
    std::string standard;
    ZEN_SHAPE(MenuClosed, 2, ZEN_FIELD(menu), ZEN_FIELD(chosen), ZEN_FIELD(input),
              ZEN_FIELD(standard));
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
/// keeping it may replace another that keeps it and continue the weaver's interaction. It holds
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
    std::vector<PaneMenuRow> standard;
    std::string pane_name;
    ZEN_SHAPE(HeldMenu, 2, ZEN_FIELD(menu), ZEN_FIELD(correlation), ZEN_FIELD(office),
              ZEN_FIELD(pane), ZEN_FIELD(subject), ZEN_FIELD(rows), ZEN_FIELD(cursor),
              ZEN_FIELD(room_rows), ZEN_FIELD(room_columns), ZEN_FIELD(standard),
              ZEN_FIELD(pane_name));
};

// ---- the presenter's side, for a presenter that keeps `HeldMenu` --------------------------------
//
// Optional helpers over the shapes above, the answer discipline the shipped presenters share, so a
// presenter keeps only what is its own: how a menu reads and what an act on it means. Each speaks
// as `kPresenterRole`, and takes Workshop's word only as authored from `zengine.workshop`.

/// WHAT A GRANT MAY BE, judged by a presenter whose line spends `marks` columns before a row's
/// label: empty when it can be shown, else the words its requester is answered with -- no rows, too
/// many, an id or label out of bounds (the requester's rows or the standard ones), or no room.
inline std::string menu_refusal_of(const MenuGranted& g, std::int64_t marks) {
    if (g.rows.empty()) {
        return "nothing to present -- the request offered no rows";
    }
    if (g.rows.size() > kMaxPaneMenuRows || g.standard.size() > kMaxPaneMenuRows) {
        return "too many rows to present";
    }
    for (const std::vector<PaneMenuRow>* run : {&g.rows, &g.standard}) {
        for (const PaneMenuRow& r : *run) {
            if (r.id.empty() || r.id.size() > kMaxPaneMenuIdLen ||
                r.label.size() > kMaxPaneMenuLabelLen) {
                return "a row's id is empty or too long, or its label is too long";
            }
        }
    }
    if (g.room_rows <= 0 || g.room_columns <= marks) {
        return "no room to present a menu here";
    }
    return std::string();
}

/// THE MENU A GRANT HOLDS, granted under `correlation`, its cursor on the first row.
inline HeldMenu held_menu_of(const MenuGranted& g, std::uint64_t correlation) {
    return HeldMenu{g.menu,
                    static_cast<std::int64_t>(correlation),
                    g.office,
                    g.pane,
                    g.subject,
                    g.rows,
                    0,
                    g.room_rows,
                    g.room_columns,
                    g.standard,
                    g.pane_name};
}

/// I HOLD THE OFFICE, CARRYING `held` (its menu 0 for none): said on every activation, so after a
/// reload Workshop keeps the menu this image carries and ends one it does not.
inline void announce_presenter_ready(loom::Mail& mail, const HeldMenu& held) {
    (void)mail.as_role(kPresenterRole)
        .send_to_role(pane_menu::kWorkshopRole, PresenterReady{held.menu});
}

/// AN INTERACTION FOR MENU `menu`, WHICH THIS IMAGE DOES NOT HOLD, HANDED BACK. It has said nothing
/// to that requester and never will, and the host, which knows who asked, settles it. Never
/// `MenuClosed`: that is an image's word that it finished the work and answered.
inline void give_back_menu(loom::Mail& mail, std::int64_t menu) {
    (void)mail.as_role(kPresenterRole)
        .send_to_role(pane_menu::kWorkshopRole,
                      MenuReturned{menu, "this image does not hold that menu"});
}

/// THE REQUESTER'S ONE ANSWER FOR `held`, under its correlation: the row `id` chosen, or nothing
/// chosen, in the words `why`.
inline void answer_menu_requester(loom::Mail& mail, const HeldMenu& held, bool chosen,
                                  const std::string& id, const std::string& why) {
    (void)mail.as_role(kPresenterRole)
        .send_to_role(held.office,
                      PaneMenuAnswered{held.pane, held.subject, chosen,
                                       chosen ? id : std::string(), chosen ? std::string() : why},
                      static_cast<std::uint64_t>(held.correlation));
}

/// END THE HELD MENU: the host first (it closes the popup and, for a choice, records the act
/// `input` that made it, or spends the standard row `standard` names), then the requester's one
/// answer. Nothing is held after.
inline void close_held_menu(loom::Mail& mail, HeldMenu& held, bool chosen, const std::string& id,
                            const std::string& why, std::int64_t input,
                            const std::string& standard = std::string()) {
    (void)mail.as_role(kPresenterRole)
        .send_to_role(pane_menu::kWorkshopRole, MenuClosed{held.menu, chosen, input, standard});
    answer_menu_requester(mail, held, chosen, id, why);
    held = HeldMenu{};
}

/// TAKE A GRANT: true when `held` now holds it, to be shown. A word not Workshop's, or for no menu,
/// is nothing. ONE MENU AT A TIME: the host withdraws an older menu before it grants a newer one,
/// so an older one still held is the unreachable case, answered all the same, because an answer
/// owed is owed. A grant the presenter judged `refusal` (`menu_refusal_of`) is answered in those
/// words at once and given straight back to the host, closed.
inline bool accept_menu_grant(const MenuGranted& g, loom::Mail& mail, HeldMenu& held,
                              const std::string& refusal) {
    if (!mail.authored_from_role(pane_menu::kWorkshopRole) || g.menu <= 0) {
        return false;
    }
    if (held.menu != 0 && held.menu != g.menu) {
        answer_menu_requester(mail, held, false, std::string(), "replaced by a newer menu");
        held = HeldMenu{};
    }
    if (!refusal.empty()) {
        (void)mail.as_role(kPresenterRole)
            .send_to_role(pane_menu::kWorkshopRole, MenuClosed{g.menu, false, 0, std::string()});
        answer_menu_requester(mail, held_menu_of(g, mail.correlation()), false, std::string(),
                              refusal);
        return false;
    }
    held = held_menu_of(g, mail.correlation());
    return true;
}

/// THE HOST ENDED MENU `w.menu`: the held menu's requester is answered unchosen in the host's
/// words, and nothing is held after. A withdrawal of a menu this image does not hold is given back,
/// as an act on one is -- the weaver's act and the host's cancellation are one interaction, and an
/// image answers both or neither -- from inside the handler, while the host still keeps the
/// withdrawn menu's record (`MenuReturned`).
inline void settle_menu_withdrawn(const MenuWithdrawn& w, loom::Mail& mail, HeldMenu& held) {
    if (!mail.authored_from_role(pane_menu::kWorkshopRole) || w.menu <= 0) {
        return;
    }
    if (w.menu != held.menu) {
        give_back_menu(mail, w.menu);
        return;
    }
    answer_menu_requester(mail, held, false, std::string(), w.why);
    held = HeldMenu{};
}

/// THE NUMBER OF THE PICTURE AN IMAGE SHOWS (`MenuShown::picture`), and the window's first entry
/// last shown, the hint for the next. Kept by the image, never in reload-kept state: a successor
/// numbers its pictures afresh. The number moves exactly when a line would now choose something
/// else -- another menu, a window that scrolled or grew or shrank, its marker above come or gone --
/// and not when only the highlight moved.
class MenuPictureTracker {
public:
    std::size_t first = 0;

    /// The window from entry `from`, `count` entries, below a marker line or not, of menu `menu`,
    /// shown now: its picture's number.
    std::int64_t shown(std::size_t from, std::size_t count, bool earlier, std::int64_t menu) {
        if (!shown_ || from != first || count != count_ || earlier != earlier_ || menu != menu_) {
            ++picture_;
        }
        shown_ = true;
        first = from;
        count_ = count;
        earlier_ = earlier;
        menu_ = menu;
        return picture_;
    }
    /// Is `in` aimed at the lines shown now, and not at lines since replaced?
    bool matches(const MenuInput& in) const noexcept { return in.picture == picture_; }

private:
    std::int64_t picture_ = 0;
    bool shown_ = false;
    std::size_t count_ = 0;
    bool earlier_ = false;
    std::int64_t menu_ = 0;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PRESENTER_VOCABULARY_HPP
