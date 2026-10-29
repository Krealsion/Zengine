// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PRESENTER_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_PRESENTER_VOCABULARY_HPP

// The seam between Workshop and the participant that presents a pane's menu, installed beside the
// pane protocol so a presenter built against the package can hold `zengine.presenter` and be
// replaced like any loaded weave
// (workshop/docs/workshop-panes.md#the-menu-presenter-and-replacing-it).
// The requester owns what its rows mean and every operation; Workshop owns the keys, the popup's
// place and frame, one menu at a time, and the standard rows it adds beneath the requester's -- its
// own pane menu, which it spends itself; the presenter owns the menu's presentation and lifetime,
// draws it in an area of its own of the window that shows the desk, hears the pointer there, and
// answers the requester exactly once per grant (`PaneMenuAnswered`, as this office). A menu's
// number is Workshop's, minted per grant; a word about any other number moves nothing.

#include "workshop/pane_menu.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "graphics/canvas.hpp"
#include "graphics/pixels.hpp"
#include "graphics/role.hpp"
#include "graphics/vocabulary.hpp"

#include <zen/weave.hpp>
#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>
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
/// apart to say whose they are. `area` is where the menu stands: an area of the window that shows
/// the desk, named for this menu alone and owned by the presenter's office, which Workshop places
/// inside the popup's frame once the presenter says how much of it the menu takes (`MenuShown`).
/// A grant while another menu is open replaces it: the older one was withdrawn first.
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
    std::string area;
    ZEN_SHAPE(MenuGranted, 3, ZEN_FIELD(menu), ZEN_FIELD(office), ZEN_FIELD(pane),
              ZEN_FIELD(subject), ZEN_FIELD(rows), ZEN_FIELD(room_rows), ZEN_FIELD(room_columns),
              ZEN_FIELD(standard), ZEN_FIELD(pane_name), ZEN_FIELD(area));
};

/// Presenter -> Workshop: menu `menu` is drawn in its area `area`, `rows` lines of at most
/// `columns` characters each, so Workshop sizes the area to it, inside the room granted. The lines
/// themselves are the presenter's, drawn in the area and read from the window. `picture` numbers
/// what the lines mean -- kept by a highlight moving, changed when a line would now choose
/// something else.
struct MenuShown {
    std::int64_t menu = 0;
    std::int64_t picture = 0;
    std::string area;
    std::int64_t rows = 0;
    std::int64_t columns = 0;
    ZEN_SHAPE(MenuShown, 3, ZEN_FIELD(menu), ZEN_FIELD(picture), ZEN_FIELD(area), ZEN_FIELD(rows),
              ZEN_FIELD(columns));
};

/// WHAT KIND OF ACT A `MenuInput` CARRIES. A press inside the menu is the presenter's own, heard
/// in its area from the window (`graphics::AreaButtonChanged`).
namespace menu_input {
inline constexpr std::int64_t kKey = 1;     ///< a key went down: `verb`, `scancode`, `modifiers`
inline constexpr std::int64_t kOutside = 4; ///< a press outside the menu, spent on it: `button`
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

/// Workshop -> presenter: the weaver did this to menu `menu`, in the order read: a key, or a press
/// outside the menu. `input` counts the weaver's acts; `line` is -1; `picture` is the number of
/// the presenter's own that Workshop last took (`MenuShown`).
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
/// `input` (0 when no act ended it). Ended by a press in the menu's area, it is sent under that
/// press's number, which names the act, and `input` is 0. A choice is recorded as that act's
/// continuation. `standard` is the id of the granted standard row chosen at that act, "" for none;
/// the requester is then answered unchosen, and Workshop spends the row. Not `MenuReturned`:
/// "answered" and "not mine to carry" are different facts.
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
/// everything needed to answer the requester and show the menu again, in the area it stands in;
/// `menu` 0 is none.
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
    std::string area;
    ZEN_SHAPE(HeldMenu, 3, ZEN_FIELD(menu), ZEN_FIELD(correlation), ZEN_FIELD(office),
              ZEN_FIELD(pane), ZEN_FIELD(subject), ZEN_FIELD(rows), ZEN_FIELD(cursor),
              ZEN_FIELD(room_rows), ZEN_FIELD(room_columns), ZEN_FIELD(standard),
              ZEN_FIELD(pane_name), ZEN_FIELD(area));
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
    if (g.area.empty()) {
        return "no area to present the menu in";
    }
    return std::string();
}

/// A ROW'S LABEL READ AS ITS WORDS AND THE KEY AFTER THE MARK (`kMenuKeyMark`), the key empty for
/// a row that names none.
struct MenuRowWords {
    std::string words;
    std::string key;
};
inline MenuRowWords menu_row_words(const std::string& label) {
    const std::size_t mark = label.find(kMenuKeyMark);
    if (mark == std::string::npos) {
        return MenuRowWords{label, std::string()};
    }
    return MenuRowWords{label.substr(0, mark), label.substr(mark + 1)};
}

/// THE COLUMNS A MENU'S ROWS ARE LAID IN after a mark `marks` columns wide, in a room `columns`
/// wide: the words, and the keys apart from them, two blanks after the widest words. A room too
/// narrow for both cuts the words column, never a key; one with no column left for a word beside
/// the widest key shows the words alone (`keys` 0).
struct MenuWidths {
    std::size_t words = 0;
    std::size_t keys = 0;
};
inline MenuWidths menu_widths(std::initializer_list<const std::vector<PaneMenuRow>*> runs,
                              std::int64_t marks, std::int64_t columns) {
    MenuWidths w;
    for (const std::vector<PaneMenuRow>* run : runs) {
        for (const PaneMenuRow& r : *run) {
            const MenuRowWords parts = menu_row_words(r.label);
            w.words = parts.words.size() > w.words ? parts.words.size() : w.words;
            w.keys = parts.key.size() > w.keys ? parts.key.size() : w.keys;
        }
    }
    if (w.keys == 0) {
        return w;
    }
    const std::int64_t room = columns - marks - 2 - static_cast<std::int64_t>(w.keys);
    if (room < 1) {
        w.keys = 0;
        return w;
    }
    w.words = w.words < static_cast<std::size_t>(room) ? w.words : static_cast<std::size_t>(room);
    return w;
}

/// ONE ROW'S TEXT AFTER ITS MARK, laid in the columns `w`: its words, and for a row that names a key
/// in a menu showing keys, its words cut to the words column, blanks to it and two more, then its
/// key.
inline std::string menu_row_text(const PaneMenuRow& row, const MenuWidths& w) {
    const MenuRowWords parts = menu_row_words(row.label);
    if (w.keys == 0 || parts.key.empty()) {
        return parts.words;
    }
    std::string out = parts.words.substr(0, w.words);
    out.resize(w.words, ' ');
    return out + "  " + parts.key;
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
                    g.pane_name,
                    g.area};
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
/// that made it -- Workshop's `input`, or the window's `press` in the menu's area -- or spends the
/// standard row `standard` names), then the requester's one answer. Nothing is held after.
inline void close_held_menu(loom::Mail& mail, HeldMenu& held, bool chosen, const std::string& id,
                            const std::string& why, std::int64_t input,
                            const std::string& standard = std::string(),
                            std::uint64_t press = 0) {
    (void)mail.as_role(kPresenterRole)
        .send_to_role(pane_menu::kWorkshopRole, MenuClosed{held.menu, chosen, input, standard},
                      press);
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

private:
    std::int64_t picture_ = 0;
    bool shown_ = false;
    std::size_t count_ = 0;
    bool earlier_ = false;
    std::int64_t menu_ = 0;
};

/// ONE LINE OF A MENU AS ITS PRESENTER DRAWS IT: its words, the role they are drawn in, and the id
/// of the row it shows, which names the line in the window ("" for a marker or a rule).
struct MenuLine {
    std::string text;
    std::int64_t role = graphics::role::kFill;
    std::string row;
    bool operator==(const MenuLine&) const = default;
};

/// THE AREA A MENU STANDS IN, AS THE PRESENTER DRAWING THERE KEEPS IT: the canvas it draws with,
/// the lines it last drew, and the line a pointer moment there falls on. One area a menu, so a
/// moment about any other area is about another menu, and nothing here. A drawing the window
/// refused leaves what it shows as it was: a refused change sends the lines again at once, whole;
/// a refused whole drawing waits for other lines.
class MenuDrawing {
public:
    explicit MenuDrawing(std::string office = kPresenterRole) : office_(std::move(office)) {}

    /// THE AREA THE HELD MENU STANDS IN ("" for none): another holds none of this drawing.
    void stand_in(const std::string& area) {
        if (area == area_) {
            return;
        }
        area_ = area;
        canvas_ = graphics::Canvas(office_, area.empty() ? office_ : area);
        lines_.clear();
        fresh_ = true;
        drew_whole_ = true;
        refused_whole_ = false;
        asked_ = false;
    }
    const std::string& area() const noexcept { return area_; }
    /// A window shows the area.
    bool shown() const noexcept { return !area_.empty() && !canvas_.window().empty(); }

    /// THE ROOM A WINDOW TELLS OF THE AREA, as that window's office: true when the window holds none
    /// of the drawing, or the area's size or the window's measure changed, so the lines go again.
    bool room(const graphics::AreaRoom& r, const loom::Mail& mail) {
        if (area_.empty() || r.area != area_ ||
            !mail.authored_from_role(std::string(graphics::kWindowOfficePrefix) + r.window) ||
            !canvas_.room(r)) {
            return false;
        }
        fresh_ = true;
        refused_whole_ = false;
        return true;
    }

    /// Whether `mail` is the word of the window that shows the area, about `about`.
    bool from_window(const std::string& about, const loom::Mail& mail) const {
        return shown() && about == area_ && mail.authored_from_role(canvas_.window_office());
    }

    /// A DRAWING THE WINDOW SHOWING THE AREA REFUSED, as that window's office: true when it was.
    bool refused(const graphics::DrawnRefused& refusal, loom::Mail& mail) {
        if (!from_window(refusal.area, mail)) {
            return false;
        }
        canvas_.forget();
        fresh_ = true;
        if (!drew_whole_) {
            send(mail);
        } else if (!lines_.empty()) {
            refused_whole_ = true;
        }
        return true;
    }

    /// DRAW `lines` IN THE AREA: its ground, then each a line of the window's text from the top, a
    /// line that shows a row named by the row's id; only what changed since the drawing the window
    /// holds. Nothing while no window shows the area, nor the same lines after a refused whole.
    void draw(const std::vector<MenuLine>& lines, loom::Mail& mail) {
        if (!shown() || (refused_whole_ && lines == lines_)) {
            return;
        }
        lines_ = lines;
        send(mail);
    }

    /// THE LINE A POINT OF THE AREA FALLS ON, said by the window that shows it, as that window's
    /// office: -1 for a point past the lines drawn, another area, or another speaker.
    std::int64_t line_at(const std::string& area, std::int64_t x, std::int64_t y,
                         const loom::Mail& mail) const {
        if (!from_window(area, mail) || x < 0 || y < 0 || x >= canvas_.width() ||
            y >= canvas_.height() || canvas_.text().line <= 0) {
            return -1;
        }
        const std::int64_t line = graphics::floor_div_px(y, canvas_.text().line);
        return line < static_cast<std::int64_t>(lines_.size()) ? line : -1;
    }

    /// WHETHER A PRESS NAMES ANOTHER LINE than the lines last drawn show where it landed, by the
    /// window's word of what it drew under the hand (as `WindowDrawing::moved` judges a pane's).
    bool moved(const graphics::AreaButtonChanged& b) const {
        if (b.under.empty() || b.under != area_) {
            return false;
        }
        return graphics::part_at(sent_, b.x, b.y, canvas_.text()) != b.part;
    }

    /// THE AREA'S ROOM ASKED of the directory, once an area, while no window has said it: a
    /// presenter carrying a menu across a reload draws it again where it stands.
    void ask_room(loom::Mail& mail) {
        if (area_.empty() || shown() || asked_) {
            return;
        }
        asked_ = true;
        (void)mail.as_role(office_).send_to_role(graphics::kGraphicsRole,
                                                 graphics::RoomAsked{area_});
    }

private:
    void send(loom::Mail& mail) {
        drew_whole_ = fresh_;
        fresh_ = false;
        refused_whole_ = false;
        const std::int64_t line = canvas_.text().line;
        graphics::Sketch sketch;
        sketch.fill_rect("ground", 0, 0, canvas_.width(), canvas_.height(),
                         graphics::by_role(graphics::role::kGround));
        for (std::size_t i = 0; i < lines_.size(); ++i) {
            const std::string node = "line" + std::to_string(i);
            sketch.text(node, 0, static_cast<std::int64_t>(i) * line, lines_[i].text,
                        graphics::by_role(lines_[i].role));
            if (!lines_[i].row.empty()) {
                sketch.action(node, lines_[i].row);
            }
        }
        canvas_.draw(sketch);
        canvas_.show(mail);
        sent_ = std::move(sketch);
    }

    std::string office_;
    std::string area_;
    graphics::Canvas canvas_{office_};
    std::vector<MenuLine> lines_;
    graphics::Sketch sent_; ///< the lines last drawn, as drawn
    bool fresh_ = true, drew_whole_ = true, refused_whole_ = false, asked_ = false;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PRESENTER_VOCABULARY_HPP
