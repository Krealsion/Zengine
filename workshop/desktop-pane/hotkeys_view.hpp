// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_DESKTOP_PANE_HOTKEYS_VIEW_HPP
#define ZENGINE_DESKTOP_PANE_HOTKEYS_VIEW_HPP

// The Hotkeys pane: every key in force as the host resolved it, in sections, a section a pane --
// the desk's panes first, Workshop's own next, panes off the desk grey at the foot -- each shut at
// first and counted; a cursor over the sections and their keys, and a binding's menu that edits it
// through the host's keymap edit door. Which sections are open is the pane's state. It keeps no
// gesture of its own to print; the weave routes its messages here and declares its rows when its
// mode moves.
// Workshop law: agents/workshop/keymap-edit.md

#include "workshop/desktop-pane/desktop_common.hpp"
#include "workshop/desktop-pane/vocabulary.hpp"

#include "workshop/desktop_seam_vocabulary.hpp"
#include "workshop/pane_rows.hpp"
#include "workshop/pane_causes.hpp"
#include "workshop/pane_menu.hpp"
#include "workshop/pane_parts.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "component/accordion.hpp"
#include "component/columns.hpp"
#include "component/held_choice.hpp"
#include "component/list_window.hpp"
#include "component/row_map.hpp"
#include "component/text_box.hpp"
#include "graphics/vocabulary.hpp"
#include "input/vocabulary.hpp"

#include <zen/weave.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace zengine::desktop_pane {

// ---- The Hotkeys table's rows, read backwards --------------------------------------------------

namespace keys_row {
constexpr std::int64_t kHeading = 0;
constexpr std::int64_t kGroup = 1; ///< a section's header; `ref` names the section
constexpr std::int64_t kBinding = 2;
constexpr std::int64_t kMarker = 3;
constexpr std::int64_t kFooter = 4;
constexpr std::int64_t kPeek = 5; ///< a key a peek shows over what follows; `ref` names the section
} // namespace keys_row

struct KeysMeaning {
    std::int64_t kind = keys_row::kHeading;
    std::size_t index = 0; ///< the line, for a binding
    /// THE BINDING'S STABLE IDENTITY (`group\x1fid\x1fkey`), for a binding row; empty otherwise.
    /// In the meaning for the same reason as the Pane Manager's `ref`: a different binding at the
    /// same table line is a changed subject, so the picture moves and an old-picture press is
    /// refused rather than resolved against the binding that took the line.
    std::string ref;
    bool operator==(const KeysMeaning& o) const {
        return kind == o.kind && index == o.index && ref == o.ref;
    }
};

/// ONE LINE OF THE TABLE: a section's header or a binding of an open section, and its identity
/// for the cursor -- a header's its section's, a binding's its group, id and the key it lists,
/// joined -- so two keys of one action are two rows.
struct KeysLine {
    bool binding = false;
    std::size_t shown = 0;   ///< index into the keymap's rows, for a binding
    std::string key;         ///< the cursor's identity
    std::size_t section = 0; ///< index into the sections, for either
};

inline std::string binding_key(const ws::ShownBinding& b) {
    return join_subject({b.group, b.id, b.gesture});
}

/// ONE SECTION OF THE TABLE: a pane's keys, or one of the places Workshop answers a key; its
/// identity, what its header says, whether its pane is off the desk, and its bindings.
struct KeysSection {
    std::string key;     ///< `pane:<office>/<pane>`, or `group:<words>` for Workshop's own
    std::string title;   ///< where its keys answer, made drawable
    bool absent = false; ///< a pane the desk does not hold now: grey, at the foot
    std::vector<std::size_t> bindings; ///< indices into the keymap's rows, in its order
};

/// A SECTION'S IDENTITY, from one of its bindings: a pane's by the pane it belongs to, so it is
/// the same section whatever the pane is called; Workshop's own by the words of its group.
inline std::string section_key(const ws::ShownBinding& b) {
    return b.pane.empty() ? "group:" + b.group : "pane:" + b.office + "/" + b.pane;
}

/// THE CURSOR'S IDENTITY FOR A SECTION'S HEADER, apart from every binding's.
inline std::string header_key(const std::string& section) { return "section:" + section; }

/// THE WEAVE'S DECLARATION OF THIS PANE'S ROWS, asked for when its mode moves: the weave numbers
/// every declaration its image makes, so the pane says when to declare and never how.
using DeclareKeys = std::function<void(loom::Mail&)>;

class HotkeysView {
public:
    /// A VIEW OVER THE OPEN SECTIONS `open` HOLDS: its pane's state (`DesktopState::hotkeys_open`),
    /// which every fold reads and writes.
    explicit HotkeysView(std::vector<std::string>& open) : groups_(open) {}

    /// THE HOTKEYS PANE'S MODE, as its declaration names it.
    const char* keys_mode() const {
        return capture_.active  ? kKeysCaptureMode
               : typing_.active ? kKeysSpellMode
                                : kKeysTableMode;
    }

    /// THE HOTKEYS PANE'S ROWS, BY MODE. Reading: a row cursor, its two ends, and the menu key.
    /// Capturing a key: ONLY cancel, so every other key arrives as a key -- the one being
    /// captured. Taking a spelling: commit and cancel, and every other key reaches the line. The
    /// captured key itself is not a row: it reaches the pane as a key.
    static std::vector<ws::v3::PaneActionRow> keys_rows(const std::string& mode) {
        using ws::v3::PaneActionRow;
        if (mode == kKeysCaptureMode) {
            return {PaneActionRow{kActionKeysCancel, "cancel", input::scan::kEscape,
                                  input::mod::kNone, {}, {}, kKeysTableMode}};
        }
        if (mode == kKeysSpellMode) {
            return {PaneActionRow{kActionKeysCommit, "use this spelling", input::scan::kReturn,
                                  input::mod::kNone, {},
                                  {ws::cause_of<ws::KeymapEditRequested>()}, kKeysTableMode},
                    PaneActionRow{kActionKeysCancel, "cancel", input::scan::kEscape,
                                  input::mod::kNone, {}, {}, kKeysTableMode}};
        }
        return {PaneActionRow{kActionKeysUp, "row up", input::scan::kUp, input::mod::kNone},
                PaneActionRow{kActionKeysDown, "row down", input::scan::kDown, input::mod::kNone},
                PaneActionRow{kActionKeysTop, "top", input::scan::kHome, input::mod::kNone},
                PaneActionRow{kActionKeysBottom, "bottom", input::scan::kEnd, input::mod::kNone},
                PaneActionRow{kActionKeysOpen, "open a section", input::scan::kRight,
                              input::mod::kNone},
                PaneActionRow{kActionKeysShut, "shut a section", input::scan::kLeft,
                              input::mod::kNone},
                PaneActionRow{kActionKeysShutAll, "shut every section", input::scan::kLeft,
                              input::mod::kShift},
                PaneActionRow{kActionKeysMenu, "row menu", input::scan::kM, input::mod::kNone}};
    }

    // ---- Its rooms ----------------------------------------------------------------------------

    /// THE PROSE ROOM WORKSHOP GRANTED IT, kept for a host that grants no canvas.


    /// ...AND ITS OWN CANVAS, whose lattice is the room it composes for while it holds one.


    const PaneCanvas& canvas() const { return keys_canvas_; }
    /// ...for the office to hand the room a graphics window gives the pane.
    PaneCanvas& canvas() { return keys_canvas_; }


    /// THE LAYOUT'S `help` SETTING: whether the footer's help lines show, their rows the list's
    /// when they do not. True when it moved.
    /// A BINDING'S KEY CELL: its key, or `(no key)`; and for a row that names a key for a POSIX
    /// terminal, its default with that key beside it, whichever is in force (WL-KEY-08).
    static std::string key_cell(const ws::ShownBinding& b) {
        if (!b.posix_shown.empty()) {
            return b.default_shown + "  posix " + b.posix_shown;
        }
        return b.shown.empty() ? std::string("(no key)") : b.shown;
    }

    /// THE LAYOUT'S `help`, as the pane's row was handed: whether the table is said again -- it
    /// moved, or it is the first row this life holds, before which the pane draws nothing.
    bool show_help(bool shown) {
        const bool moved = help_shown_ != shown || !held_;
        help_shown_ = shown;
        held_ = true;
        return moved;
    }

    // ---- The keymap in force ------------------------------------------------------------------

    /// THE KEYMAP IN FORCE, AS THE HOST RESOLVED IT -- the one binding truth. The table is read
    /// from it; this pane keeps no gesture of its own to print (WL-DESK-11).
    void keymap_said(const ws::KeymapShown& said, const Declared& keys_rows_decl,
                     loom::Mail& mail) {
        keymap_ = said;
        keys_heard_ = true;
        say_keys(keys_rows_decl, mail);
    }

    bool keys_heard() const { return keys_heard_; }
    const ws::KeymapShown& keymap() const { return keymap_; }

    /// WHAT AN EDIT CAME TO, in the host's own sentence: applied and written, applied for this
    /// run only, or refused with nothing changed. The table itself moves when the host republishes
    /// the keymap; the sentence is this pane's notice until the weaver's next act.
    void edit_answered(const ws::KeymapEditAnswered& answer, const Declared& keys_rows_decl,
                       loom::Mail& mail) {
        if (!mail.answers_ask() || !edit_.awaiting || mail.correlation() != edit_.pending) {
            return;
        }
        edit_ = Edit{};
        keys_notice_ = answer.sentence;
        if (answer.accepted) {
            refind_id_ = answer.action; // the row's key changed; hold the action's first row
        }
        say_keys(keys_rows_decl, mail);
    }

    // ---- The spelling line's text and clipboard -----------------------------------------------

    /// TEXT TYPED WHILE THE PANE HOLDS THE KEYBOARD -- into the spelling line, while one is open.
    void typed(const std::string& text, const Declared& keys_rows_decl, loom::Mail& mail) {
        if (typing_.active) {
            type_spelling(typing_.line, text);
            say_keys(keys_rows_decl, mail);
        }
    }

    /// WHAT THE PROCESS SAYS IT COPIED -- this pane's own copies included, which is why it is a
    /// mirror rather than a second store: a paste on a medium that cannot be read answers with it.
    void copied(const std::string& text) { clip_.text = text; }

    /// THE CLIPBOARD'S ANSWER TO THIS IMAGE'S OWN PASTE, landing only in the line that asked --
    /// the same line, still open, in the same draft (`TextBox::draft_epoch`).
    void pasted(const graphics::ClipboardText& a, const Declared& keys_rows_decl,
                loom::Mail& mail) {
        if (!mail.answers_ask() || !paste_.awaiting || mail.correlation() != paste_.pending) {
            return;
        }
        paste_.awaiting = false;
        zengine::component::TextBox* line = pasting_line();
        if (line == nullptr || line->draft_epoch() != paste_.epoch) {
            return; // the line that asked is gone; the text lands nowhere
        }
        const std::string text = a.readable ? a.text : clip_.text;
        if (text.empty()) {
            return;
        }
        type_spelling(*line, text);
        say_keys(keys_rows_decl, mail);
    }

    /// LOOM'S WORD THAT ONE OF THIS PANE'S ASKS NEVER REACHED ITS DOOR, matched against the one
    /// record still waiting (`refused_ask`): a refused paste lands nothing; a refused edit changed
    /// nothing, and the table says so. True when the refusal named either.
    bool ask_refused(const loom::DispatchRefused& refused, const Declared& keys_rows_decl,
                     loom::Mail& mail) {
        if (refused_ask(paste_.awaiting, paste_.attempt, paste_.pending, refused, mail,
                        graphics::ClipboardTextRequested::zen_name,
                        graphics::ClipboardTextRequested::zen_version, paste_.office.c_str())) {
            paste_.awaiting = false;
            return true;
        }
        // ...AND AN EDIT THAT NEVER ARRIVED: released and said, so the table is not left waiting
        // for a verdict on a request the host never handled.
        if (refused_ask(edit_.awaiting, edit_.attempt, edit_.pending, refused, mail,
                        ws::KeymapEditRequested::zen_name, ws::KeymapEditRequested::zen_version,
                        kWorkshopRole)) {
            edit_ = Edit{};
            keys_notice_ = "edit not delivered -- nothing changed (" + refused.reason + ")";
            say_keys(keys_rows_decl, mail);
            return true;
        }
        return false;
    }

    // ---- The keys -----------------------------------------------------------------------------

    void keys_action(const std::string& id, const Declared& keys_rows_decl,
                     const DeclareKeys& declare_keys, loom::Mail& mail) {
        // WHILE CAPTURING OR SPELLING, ONLY THE DECLARED KEYS ARE ACTS; a raced id is nothing.
        if (capture_.active) {
            if (id == kActionKeysCancel) {
                capture_ = Capture{};
                keys_notice_ = "capture cancelled";
                declare_keys(mail);
                say_keys(keys_rows_decl, mail);
            }
            return;
        }
        if (typing_.active) {
            if (id == kActionKeysCancel) {
                typing_ = Typing{};
                keys_notice_ = "spelling cancelled";
                declare_keys(mail);
                say_keys(keys_rows_decl, mail);
            } else if (id == kActionKeysCommit) {
                const Typing was = typing_;
                typing_ = Typing{};
                declare_keys(mail);
                ask_edit(mail, was.id, was.op, 0, 0, was.line.text());
                say_keys(keys_rows_decl, mail);
            }
            return;
        }
        if (id == kActionKeysUp) {
            keys_step(-1);
        } else if (id == kActionKeysDown) {
            keys_step(+1);
        } else if (id == kActionKeysTop) {
            keys_step(-static_cast<std::int64_t>(lines_.size()));
        } else if (id == kActionKeysBottom) {
            keys_step(static_cast<std::int64_t>(lines_.size()));
        } else if (id == kActionKeysOpen) {
            keys_open_here();
        } else if (id == kActionKeysShut) {
            keys_shut_here();
        } else if (id == kActionKeysShutAll) {
            keys_shut_all();
        } else if (id == kActionKeysMenu) {
            if (keys_choice_.actionable() && keys_choice_.at < lines_.size() &&
                lines_[keys_choice_.at].binding) {
                const std::int64_t row =
                    keys_map_.row_of(KeysMeaning{keys_row::kBinding, keys_choice_.at,
                                                 keys_line_ref(keys_choice_.at)});
                keys_notice_.clear();
                offer_keys_row(keys_choice_.at, row < 0 ? 0 : row, 0, mail, mail.correlation());
            } else if (keys_choice_.actionable() && keys_choice_.at < lines_.size()) {
                keys_notice_ = "a section has no menu -- open it and choose one of its keys";
            } else {
                keys_notice_ = "no row is chosen -- choose one, then open its menu";
            }
        } else {
            return;
        }
        say_keys(keys_rows_decl, mail);
    }

    /// A KEY WHILE THE HOTKEYS PANE HOLDS THE KEYBOARD: the captured key, a key into the spelling
    /// line, or nothing.
    void keys_key(const ws::PaneKey& key, const Declared& keys_rows_decl,
                  const DeclareKeys& declare_keys, loom::Mail& mail) {
        if (capture_.active) {
            if (key.scancode == input::scan::kUnknown) {
                return; // a key this build cannot name; the host would refuse it in words
            }
            const Capture was = capture_;
            capture_ = Capture{};
            declare_keys(mail);
            ask_edit(mail, was.id, was.op, key.scancode, key.modifiers, std::string());
            say_keys(keys_rows_decl, mail);
            return;
        }
        if (typing_.active && line_key(typing_.line, key, mail)) {
            say_keys(keys_rows_decl, mail);
        }
    }

    // ---- The mouse ----------------------------------------------------------------------------

    /// A PRESS ON A TABLE ROW CHOOSES IT -- visible selection, for the menu key to act on. A press
    /// on a section's header opens it or shuts it, the cursor on the header; a press on a key a
    /// peek shows opens its section, the cursor on that key.
    void keys_press(const PanePress& press, const Declared& keys_rows_decl, loom::Mail& mail) {
        if (press.moved) {
            keys_notice_ = kMovedSentence;
            say_keys(keys_rows_decl, mail);
            return;
        }
        const KeysMeaning* m = keys_map_.at(press.row, press.column);
        if (m == nullptr) {
            return;
        }
        if (m->kind == keys_row::kGroup) {
            hold_key(header_key(m->ref));
            groups_.press(m->ref);
        } else if (m->kind == keys_row::kPeek) {
            if (m->index < keymap_.rows.size()) {
                hold_key(binding_key(keymap_.rows[m->index]));
            }
            groups_.press(m->ref);
        } else if (m->kind == keys_row::kBinding) {
            keys_hold(m->index);
        } else {
            return;
        }
        keys_notice_.clear();
        say_keys(keys_rows_decl, mail);
    }

    /// THE POINTER RESTING ON THE TABLE, where the medium reports one: over a shut section's header,
    /// or the keys its peek shows, that section peeks, and over anything else none does. What it
    /// shows is drawn again only when that moved. Presentation alone: nothing is kept.
    void rest(const ws::RowCell& at, const Declared& keys_rows_decl, loom::Mail& mail) {
        const KeysMeaning* m = at.shown ? keys_map_.at(at.row, at.column) : nullptr;
        const bool over =
            m != nullptr && (m->kind == keys_row::kGroup || m->kind == keys_row::kPeek);
        if (over ? groups_.rest(m->ref) : groups_.rest_none()) {
            say_keys(keys_rows_decl, mail);
        }
    }
    /// ...AND ITS LEAVE: a peek ends.
    void pointer_left(const Declared& keys_rows_decl, loom::Mail& mail) {
        if (groups_.rest_none()) {
            say_keys(keys_rows_decl, mail);
        }
    }

    /// WHAT PANES THE DESK HOLDS, as the host's inventory says them: a section of a pane off the
    /// desk is grey and stands at the foot.
    void inventory_said(const std::vector<ws::InventoryPane>& panes,
                        const Declared& keys_rows_decl, loom::Mail& mail) {
        std::set<std::string> desk;
        std::vector<std::string> known;
        for (const ws::InventoryPane& p : panes) {
            known.push_back("pane:" + p.office + "/" + p.pane);
            if (p.open) {
                desk.insert(p.office + "/" + p.pane);
            }
        }
        const bool moved = !inventory_heard_ || desk != on_desk_;
        on_desk_ = std::move(desk);
        inventory_known_ = std::move(known);
        inventory_heard_ = true;
        if (moved) {
            say_keys(keys_rows_decl, mail);
        }
    }

    /// A RIGHT PRESS on a binding a weaver may move OFFERS its menu, beside the press, continuing
    /// it; a right press on anything else (a heading, a marker, a footer, the spelling line) is
    /// handed back to the host, whose own pane menu answers.
    void second_button(const PanePress& b, const Declared& keys_rows_decl, loom::Mail& mail) {
        if (b.moved) {
            keys_notice_ = kMovedSentence;
            say_keys(keys_rows_decl, mail);
            return;
        }
        const KeysMeaning* m = keys_map_.at(b.row, b.column);
        if (m != nullptr && m->kind == keys_row::kBinding && m->index < lines_.size() &&
            keymap_.rows[lines_[m->index].shown].remappable) {
            keys_hold(m->index);
            keys_notice_.clear();
            offer_keys_row(m->index, b.row, b.column, mail, mail.correlation());
            say_keys(keys_rows_decl, mail);
            return;
        }
        (void)pane_menu::pass_back(mail, kDesktopRole, kHotkeysPane);
    }

    /// THE WHEEL WALKS THE CURSOR, one binding per notch, fractions carried (Powers' convention).
    void wheel(double dy, const Declared& keys_rows_decl, loom::Mail& mail) {
        const std::int64_t notches = take_notches(keys_wheel_, dy);
        if (notches != 0) {
            keys_step(-notches);
            say_keys(keys_rows_decl, mail);
        }
    }

    /// What a menu came to, if it answers this image's own ask (`Asked::take`), judged against
    /// the table as it is now.
    void menu_answered(const ws::PaneMenuAnswered& a, const Declared& keys_rows_decl,
                       const DeclareKeys& declare_keys, loom::Mail& mail) {
        if (!keys_asked_.take(mail, a).empty()) {
            keys_chose(a, keys_rows_decl, declare_keys, mail);
        }
    }

    // ---- What it shows ------------------------------------------------------------------------

    /// THE HEADING, THE TABLE THROUGH A WINDOW THAT KEEPS THE CURSOR VISIBLE, THE SPELLING LINE
    /// WHILE ONE IS OPEN, AND THE FOOTER SAYING WHERE A KEY IS MOVED: the file this run reads,
    /// what reading it came to, and the one line a weaver writes. Columns are laid out once for
    /// every row, and each cell is recorded so a press can be answered by the column it is in.
    void say_keys(const Declared& keys_rows_decl, loom::Mail& mail) {
        const std::int64_t room_rows = keys_canvas_.rows();
        const std::int64_t room_columns = keys_canvas_.columns();
        if (!held_ || !keys_canvas_.granted() || room_rows <= 0 || room_columns <= 0) {
            return;
        }
        std::vector<ws::TextRow> out;
        keys_map_.begin();
        const auto push = [&out, room_rows, room_columns, this](std::string text,
                                                                std::int64_t role, KeysMeaning m,
                                                                std::int64_t mark = ws::kNoMark) {
            if (static_cast<std::int64_t>(out.size()) < room_rows) {
                keys_map_.row(static_cast<std::int64_t>(out.size()), m);
                out.push_back(ws::TextRow{fit(std::move(text), room_columns), role,
                                          graphics::role::kNone,
                                          mark < room_columns ? mark : ws::kNoMark});
            }
        };
        if (!keys_heard_) {
            push("HOTKEYS (waiting)", graphics::role::kMuted, KeysMeaning{});
            publish_keys(mail, std::move(out));
            return;
        }
        compose_lines();
        keys_find();
        std::size_t bindings = 0;
        for (const ws::ShownBinding& b : keymap_.rows) {
            bindings += b.remappable ? 1u : 0u;
        }
        push("HOTKEYS -- " + std::to_string(bindings) + " in force; * moved by your file; "
             "right-click or `m` on a row to change it",
             graphics::role::kAccent, KeysMeaning{});
        // THE SPELLING LINE, UNDER THE HEADING, while one is open, its caret where it stands.
        ws::RowsCaret caret;
        if (typing_.active) {
            const std::string prompt = "key for " + typing_.id + ": ";
            const std::int64_t room =
                room_columns - static_cast<std::int64_t>(prompt.size());
            if (room > 2) {
                const ws::RowBox box = ws::row_box(
                    typing_.line, static_cast<std::int64_t>(prompt.size()), room);
                const auto at = static_cast<std::int64_t>(out.size());
                push(prompt + std::string(static_cast<std::size_t>(room), ' '),
                     graphics::role::kAccent, KeysMeaning{keys_row::kFooter, 0, {}});
                if (static_cast<std::int64_t>(out.size()) > at) {
                    out.back().box = box;
                    caret = ws::box_caret(box, at);
                }
            } else {
                push(prompt, graphics::role::kAccent, KeysMeaning{keys_row::kFooter, 0, {}});
            }
        }
        // WHERE A KEY IS MOVED, said last and reserved first: the grammar a weaver writes, the file
        // this run reads, and what reading it came to. A small room keeps the notice alone. The
        // file and the grammar are the help lines a layout's `help` hides; what an act or a reading
        // came to is said whatever it keeps.
        std::vector<std::string> footer;
        if (!keys_notice_.empty()) {
            footer.push_back("  " + drawable(keys_notice_));
        }
        if (!keys_rows_decl.word.empty()) {
            footer.push_back("  " + drawable(keys_rows_decl.word));
        }
        if (help_shown_) {
            footer.push_back("keymap file: " + (keymap_.file.empty() ? std::string("(none)")
                                                                     : drawable(keymap_.file)));
        }
        if (!keymap_.word.empty()) {
            footer.push_back("  " + drawable(keymap_.word));
        }
        if (help_shown_) {
            footer.push_back("or write {\"action\": \"<id>\", \"gesture\": \"alt+up\"} in the "
                             "file; \"none\" disables; read at launch");
        }
        std::int64_t budget = room_rows - 1 - (typing_.active ? 1 : 0);
        // AS MANY FOOTER ROWS AS THE ROOM ALLOWS while three list rows stay -- the notice first,
        // then the reading's word, the file and the grammar; a tiny room keeps the notice alone.
        std::int64_t footer_rows = budget - 3;
        if (footer_rows > static_cast<std::int64_t>(footer.size())) {
            footer_rows = static_cast<std::int64_t>(footer.size());
        }
        if (footer_rows <= 0) {
            footer_rows = !keys_notice_.empty() && budget >= 2 ? 1 : 0;
        }
        budget -= footer_rows;
        // THE COLUMNS, LAID OUT ONCE: key, label, id, marks -- widths from the population, within
        // the room after the two-column marker.
        const std::size_t marks_want = [this] {
            std::size_t w = 0;
            for (const ws::ShownBinding& b : keymap_.rows) {
                w = std::max(w, marks_of(b).size());
            }
            return w;
        }();
        const std::vector<component::Column> asks = {
            component::Column{component::widest(keymap_.rows, key_cell), 6},
            component::Column{component::widest(keymap_.rows,
                                                [](const ws::ShownBinding& b) { return b.label; }),
                              8},
            component::Column{component::widest(keymap_.rows,
                                                [](const ws::ShownBinding& b) { return b.id; }),
                              4},
            component::Column{marks_want, marks_want == 0 ? std::size_t{0} : std::size_t{1}}};
        const std::size_t table_budget = room_columns > static_cast<std::int64_t>(kKeyIndent)
                                             ? static_cast<std::size_t>(room_columns) - kKeyIndent
                                             : 0;
        const std::vector<std::size_t> widths = component::layout_columns(asks, table_budget);
        const std::vector<std::size_t> offsets = component::column_offsets(widths);
        // ONE KEY'S CELLS, laid out once for every row: the cursor's mark, the section's indent and
        // the table line.
        const auto key_text = [&](const ws::ShownBinding& b, bool here) {
            return fit(std::string(here ? "> " : "  ") + std::string(kKeyIndent - 2, ' ') +
                           component::table_line(
                               {key_cell(b), b.label, b.id, marks_of(b)}, widths,
                               [](const std::string& text, std::int64_t width) {
                                   return fit(text, width);
                               },
                               [](std::string text, std::size_t width) {
                                   return pad(std::move(text), width);
                               }),
                       room_columns);
        };
        const component::ListWindow w = component::cursor_window(
            lines_.size(), keys_choice_.at, keys_first_,
            budget > 0 ? static_cast<std::size_t>(budget) : 0);
        keys_first_ = w.first;
        if (const std::size_t earlier = component::earlier_marker(w); earlier > 0) {
            push("  ^ " + std::to_string(earlier) + " more above", graphics::role::kMuted,
                 KeysMeaning{keys_row::kMarker, 0, {}});
        }
        std::int64_t peek_from = -1;
        std::size_t peeking = 0;
        for (std::size_t i = w.first; i < w.end(); ++i) {
            const KeysLine& line = lines_[i];
            const bool here = i == keys_choice_.at;
            if (!line.binding) {
                // A SECTION'S HEADER: its mark, where its keys answer, and, shut or peeking, how
                // many keys it holds. A pane off the desk is grey.
                const KeysSection& section = sections_[line.section];
                const component::Fold fold = groups_.fold(section.key);
                std::string text = std::string(here ? "> " : "  ") + component::fold_mark(fold) +
                                   " " + section.title;
                if (fold != component::Fold::kOpen) {
                    const std::size_t n = section.bindings.size();
                    text += "  (" + std::to_string(n) + (n == 1 ? " key)" : " keys)");
                }
                if (fold == component::Fold::kPeek) {
                    peek_from = static_cast<std::int64_t>(out.size()) + 1;
                    peeking = line.section;
                }
                push(std::move(text),
                     section.absent && fold != component::Fold::kPeek ? graphics::role::kMuted
                                                                      : graphics::role::kAccent,
                     KeysMeaning{keys_row::kGroup, i, section.key}, 2);
                continue;
            }
            const ws::ShownBinding& b = keymap_.rows[line.shown];
            const std::int64_t at = static_cast<std::int64_t>(out.size());
            const std::string text = key_text(b, here);
            const std::string ref = binding_key(b);
            push(text, here ? graphics::role::kAccent : graphics::role::kFill,
                 KeysMeaning{keys_row::kBinding, i, ref});
            // THE KEY CELL IS A CONTROL INSIDE THE ROW -- the same meaning, recorded so a press on
            // the key is a press on the binding wherever the columns put it.
            (void)keys_map_.span(
                at, static_cast<std::int64_t>(kKeyIndent + offsets[0]),
                static_cast<std::int64_t>(widths[0]),
                component::solid_columns(text, static_cast<std::size_t>(room_columns)),
                KeysMeaning{keys_row::kBinding, i, ref});
        }
        if (const std::size_t more = component::more_marker(w); more > 0) {
            push("  v " + std::to_string(more) + " more below", graphics::role::kMuted,
                 KeysMeaning{keys_row::kMarker, 1, {}});
        }
        for (std::int64_t i = 0; i < footer_rows; ++i) {
            push(footer[static_cast<std::size_t>(i)], graphics::role::kMuted,
                 KeysMeaning{keys_row::kFooter, static_cast<std::size_t>(i) + 1, {}});
        }
        // A PEEK: the section's keys drawn over what follows its header, to the room's end, taking
        // none of it; the rows beneath mean nothing while they show, and what the room cannot
        // hold is counted on its last row.
        if (peek_from >= 0 && peek_from < room_rows) {
            const KeysSection& section = sections_[peeking];
            const component::SectionSpan span =
                component::section_span(component::Fold::kPeek, 0, section.bindings.size(),
                                        static_cast<std::size_t>(room_rows - peek_from));
            const component::ListWindow shown =
                component::scroll_window(section.bindings.size(), 0, span.body);
            keys_map_.cover(peek_from, static_cast<std::int64_t>(span.body));
            if (static_cast<std::int64_t>(out.size()) < peek_from + static_cast<std::int64_t>(span.body)) {
                out.resize(static_cast<std::size_t>(peek_from) + span.body);
            }
            std::int64_t row = peek_from;
            for (std::size_t k = shown.first; k < shown.end(); ++k, ++row) {
                const std::size_t i = section.bindings[k];
                out[static_cast<std::size_t>(row)] =
                    ws::TextRow{key_text(keymap_.rows[i], false), graphics::role::kAccent};
                keys_map_.row(row, KeysMeaning{keys_row::kPeek, i, section.key});
            }
            if (const std::size_t more = component::more_marker(shown); more > 0) {
                out[static_cast<std::size_t>(row)] = ws::TextRow{
                    fit("  ... " + std::to_string(more) + " more", room_columns),
                    graphics::role::kMuted};
                keys_map_.row(row, KeysMeaning{keys_row::kPeek, keymap_.rows.size(), section.key});
            }
        }
        publish_keys(mail, std::move(out), caret);
    }

    /// WHAT A HOTKEYS PART IS CALLED: a binding's row by its identity,
    /// `binding:<group>/<id>[/<key>]`, and the spelling line `line` while one is open.
    std::string keys_part(const KeysMeaning& m) const {
        if (m.kind == keys_row::kFooter && m.index == 0 && typing_.active) {
            return "line";
        }
        if (m.kind != keys_row::kBinding || m.index >= lines_.size() || !lines_[m.index].binding) {
            return std::string();
        }
        const ws::ShownBinding& b = keymap_.rows[lines_[m.index].shown];
        return "binding:" + b.group + "/" + b.id + (b.gesture.empty() ? "" : "/" + b.gesture);
    }

private:
    // ---- The spelling line ------------------------------------------------------------------

    /// A SPELLING IS THE KEYMAP FILE'S GRAMMAR, PLAIN ASCII ON ONE LINE: typed or pasted text that
    /// holds anything else is refused whole at the line, and said, rather than drawn or changed.
    void type_spelling(zengine::component::TextBox& line, const std::string& text) {
        if (!admissible(text)) {
            keys_notice_ = "not typed -- a key's spelling is plain ASCII on one line";
            return;
        }
        line.type(text);
    }

    /// THE LINE A PASTE LANDS IN, if one is open: the spelling line.
    zengine::component::TextBox* pasting_line() {
        return typing_.active ? &typing_.line : nullptr;
    }

    /// ONE KEY INTO ONE LINE, with the line's own copy, cut and paste. True when the line took it.
    bool line_key(zengine::component::TextBox& line, const ws::PaneKey& key, loom::Mail& mail) {
        const std::uint64_t copied_before = clip_.writes;
        const std::uint64_t pastes_before = clip_.paste_requests;
        if (!line.consume(key.scancode, key.modifiers, clip_)) {
            return false;
        }
        if (clip_.writes != copied_before) {
            mail.publish(graphics::ClipboardCopy{clip_.text});
        }
        if (clip_.paste_requests != pastes_before) {
            begin_paste(mail, line);
        }
        return true;
    }

    /// A PASTE IS AN ASK, ON ITS OWN RECORD, OF THE WINDOW THAT SHOWS THE PANE, ELSE THE SKIN:
    /// nothing queued means nothing is on its way into the line, and neither that nor Loom's
    /// refusal of it touches the edit asked. The record keeps the office asked, which a refusal
    /// names.
    void begin_paste(loom::Mail& mail, const zengine::component::TextBox& line) {
        paste_.pending = ++asks_;
        paste_.epoch = line.draft_epoch();
        paste_.office = keys_canvas_.clipboard_office();
        paste_.attempt = mail.as_role(kDesktopRole)
                             .send_to_role(paste_.office, graphics::ClipboardTextRequested{},
                                           paste_.pending);
        paste_.awaiting = paste_.attempt.valid();
    }

    // ---- The table's lines and its cursor ---------------------------------------------------

    /// THE TABLE'S SECTIONS AND ITS LINES: a section per pane and per place Workshop answers a key,
    /// in the keymap's order within each of three runs -- the desk's panes first, Workshop's own
    /// next, the panes off the desk at the foot -- and a line per section's header, then a line
    /// per key of each open section. The lines are the population the cursor and the window are
    /// over. A section the keymap and the inventory no longer hold is forgotten.
    void compose_lines() {
        sections_.clear();
        std::map<std::string, std::size_t> at;
        for (std::size_t i = 0; i < keymap_.rows.size(); ++i) {
            const ws::ShownBinding& b = keymap_.rows[i];
            const std::string key = section_key(b);
            const auto [found, fresh] = at.emplace(key, sections_.size());
            if (fresh) {
                sections_.push_back(KeysSection{key, drawable(b.group),
                                                !b.pane.empty() && inventory_heard_ &&
                                                    on_desk_.count(b.office + "/" + b.pane) == 0,
                                                {}});
            }
            sections_[found->second].bindings.push_back(i);
        }
        const auto run = [](const KeysSection& s) {
            return s.absent ? 2 : (s.key.rfind("pane:", 0) == 0 ? 0 : 1);
        };
        std::stable_sort(sections_.begin(), sections_.end(),
                         [&run](const KeysSection& a, const KeysSection& b) {
                             return run(a) < run(b);
                         });
        if (inventory_heard_) {
            std::vector<std::string> held = inventory_known_;
            for (const KeysSection& s : sections_) {
                held.push_back(s.key);
            }
            (void)groups_.keep(held, [](const std::string& key) { return key; });
        }
        lines_.clear();
        for (std::size_t s = 0; s < sections_.size(); ++s) {
            lines_.push_back(KeysLine{false, 0, header_key(sections_[s].key), s});
            if (groups_.is_open(sections_[s].key)) {
                for (const std::size_t i : sections_[s].bindings) {
                    lines_.push_back(KeysLine{true, i, binding_key(keymap_.rows[i]), s});
                }
            }
        }
    }

    static std::string key_of_line(const KeysLine& l) { return l.key; }

    /// FIND THE CURSOR'S ROW IN THE FRESH TABLE, by identity; after an accepted edit the row's
    /// key changed, so the action's first row is held instead of losing the cursor. A choice never
    /// made, or one whose row left, takes the line the marker stands on.
    void keys_find() {
        keys_choice_.find(lines_, key_of_line);
        if (keys_choice_.lost && !refind_id_.empty()) {
            for (std::size_t i = 0; i < lines_.size(); ++i) {
                if (lines_[i].binding && keymap_.rows[lines_[i].shown].id == refind_id_) {
                    keys_choice_.hold(lines_, i, key_of_line);
                    break;
                }
            }
        }
        refind_id_.clear();
        if ((!keys_choice_.chosen || keys_choice_.lost) && !lines_.empty()) {
            keys_choice_.hold(lines_, (std::min)(keys_choice_.at, lines_.size() - 1), key_of_line);
        }
    }

    void keys_hold(std::size_t line) {
        if (line < lines_.size()) {
            keys_choice_.hold(lines_, line, key_of_line);
        }
    }
    /// ...OR THE LINE `key` NAMES, found where it stands when the table is composed again.
    void hold_key(const std::string& key) {
        keys_choice_.key = key;
        keys_choice_.chosen = true;
        keys_choice_.lost = false;
    }

    /// STEP THE CURSOR `by` LINES (negative is up), headers and keys alike, clamped to the table.
    void keys_step(std::int64_t by) {
        if (lines_.empty()) {
            return;
        }
        (void)keys_choice_.step(lines_, by, key_of_line);
        keys_notice_.clear();
    }

    /// The line the cursor stands on, while it holds one.
    const KeysLine* cursor_line() const {
        return keys_choice_.actionable() && keys_choice_.at < lines_.size()
                   ? &lines_[keys_choice_.at]
                   : nullptr;
    }

    /// RIGHT: open the section whose header the cursor stands on, or, open already, step to its
    /// first key.
    void keys_open_here() {
        const KeysLine* line = cursor_line();
        if (line == nullptr || line->binding) {
            return;
        }
        if (groups_.open(sections_[line->section].key)) {
            keys_notice_.clear();
        } else if (keys_choice_.at + 1 < lines_.size() && lines_[keys_choice_.at + 1].binding) {
            keys_step(1);
        }
    }

    /// LEFT: shut the section whose header the cursor stands on, or from one of its keys step to
    /// that header.
    void keys_shut_here() {
        const KeysLine* line = cursor_line();
        if (line == nullptr) {
            return;
        }
        const std::string& section = sections_[line->section].key;
        if (line->binding) {
            hold_key(header_key(section));
        } else if (groups_.shut(section)) {
            keys_notice_.clear();
        }
    }

    /// EVERY SECTION SHUT, in one act; a cursor on a key stands on its section's header.
    void keys_shut_all() {
        if (const KeysLine* line = cursor_line(); line != nullptr && line->binding) {
            hold_key(header_key(sections_[line->section].key));
        }
        if (groups_.shut_all()) {
            keys_notice_.clear();
        }
    }

    /// THE STABLE SUBJECT OF A HOTKEYS TABLE LINE -- the binding's identity, or empty for a line
    /// that is not a binding (a heading) or off the end.
    std::string keys_line_ref(std::size_t line) const {
        if (line < lines_.size() && lines_[line].binding) {
            return binding_key(keymap_.rows[lines_[line].shown]);
        }
        return std::string();
    }

    // ---- A binding's menu and the edit it asks for ------------------------------------------

    /// THE ROWS A BINDING OFFERS. The subject is the binding's identity (group, id, key), so the
    /// answer is judged against the table as it is when it arrives.
    void offer_keys_row(std::size_t line, std::int64_t row, std::int64_t column, loom::Mail& mail,
                        std::uint64_t correlation) {
        const ws::ShownBinding& b = keymap_.rows[lines_[line].shown];
        pane_menu::Offer offer(kHotkeysPane, binding_key(b));
        offer.at(row, column);
        offer.row(kMenuModifyPress, "Modify (press a key)");
        offer.row(kMenuModifyType, "Modify (type a spelling)");
        offer.row(kMenuAddPress, "Add a key (press)");
        offer.row(kMenuAddType, "Add a key (type)");
        if (!b.gesture.empty()) {
            offer.row(kMenuRemove, "Remove `" + b.shown + "`");
        }
        offer.row(kMenuDisable, "Disable");
        offer.row(kMenuReset, "Reset to default");
        keys_asked_ = offer.continuing(mail, kDesktopRole, correlation);
    }

    /// WHAT A HOTKEYS MENU CAME TO. The subject names a binding, which must still be in the
    /// table -- the file, a declaration or another edit may have moved it while the menu was open.
    void keys_chose(const ws::PaneMenuAnswered& a, const Declared& keys_rows_decl,
                    const DeclareKeys& declare_keys, loom::Mail& mail) {
        const std::vector<std::string> parts = split_subject(a.subject);
        if (parts.size() != 3) {
            return;
        }
        const ws::ShownBinding* b = nullptr;
        for (const ws::ShownBinding& each : keymap_.rows) {
            if (each.group == parts[0] && each.id == parts[1] && each.gesture == parts[2]) {
                b = &each;
                break;
            }
        }
        if (b == nullptr) {
            keys_notice_ = "that row moved -- open its menu again";
            say_keys(keys_rows_decl, mail);
            return;
        }
        keys_notice_.clear();
        if (a.id == kMenuModifyPress || a.id == kMenuAddPress) {
            // CAPTURE: only cancel is declared, so the next key arrives as a key -- the one to
            // bind. A chord answered above every mode cannot arrive; type it instead.
            capture_ = Capture{true, b->id,
                               a.id == kMenuModifyPress ? ws::keymap_edit::kSet
                                                        : ws::keymap_edit::kAdd};
            declare_keys(mail);
            // THE EDIT NEEDS THE KEYS. The menu left them where they were, so ask for them,
            // continuing this choice -- guarded, so a newer act defeats it (WL-KEY-17).
            (void)pane_menu::take_keyboard(mail, kDesktopRole, kHotkeysPane);
            keys_notice_ = "press the key for `" + b->id + "` (Escape cancels; a chord like " +
                           "ctrl+p that opens a tool must be typed instead)";
        } else if (a.id == kMenuModifyType || a.id == kMenuAddType) {
            typing_ = Typing{};
            typing_.active = true;
            typing_.id = b->id;
            typing_.op = a.id == kMenuModifyType ? ws::keymap_edit::kSetSpelled
                                                 : ws::keymap_edit::kAddSpelled;
            typing_.line.clear();
            declare_keys(mail);
            (void)pane_menu::take_keyboard(mail, kDesktopRole, kHotkeysPane);
            keys_notice_ = "type the key for `" + b->id + "` as the file spells it, then Return";
        } else if (a.id == kMenuRemove) {
            ask_edit(mail, b->id, ws::keymap_edit::kRemoveSpelled, 0, 0, b->gesture);
        } else if (a.id == kMenuDisable) {
            ask_edit(mail, b->id, ws::keymap_edit::kDisable, 0, 0, std::string());
        } else if (a.id == kMenuReset) {
            ask_edit(mail, b->id, ws::keymap_edit::kReset, 0, 0, std::string());
        }
        say_keys(keys_rows_decl, mail);
    }

    /// ASK THE HOST FOR ONE EDIT, under a number of this image's own; one at a time, and the
    /// ticket kept: an invalid one means nothing was queued, so the record is released at once and
    /// the table says so.
    void ask_edit(loom::Mail& mail, const std::string& id, std::int64_t op, std::int64_t scancode,
                  std::int64_t modifiers, const std::string& text) {
        if (edit_.awaiting) {
            keys_notice_ = "edit not sent -- an earlier edit is still unanswered";
            return;
        }
        Edit asking;
        asking.awaiting = true;
        asking.pending = ++asks_;
        asking.attempt =
            mail.as_role(kDesktopRole)
                .send_to_role(kWorkshopRole,
                              ws::KeymapEditRequested{id, op, scancode, modifiers, text},
                              asking.pending);
        if (!asking.attempt.valid()) {
            keys_notice_ = "edit not submitted -- nothing was queued";
            return;
        }
        edit_ = std::move(asking);
        keys_notice_ = "asked the host to change `" + id + "`";
    }

    static std::string marks_of(const ws::ShownBinding& b) {
        std::string marks;
        if (b.authored) {
            marks += "*";
        }
        if (!b.remappable) {
            marks += marks.empty() ? "(not remappable)" : " (not remappable)";
        }
        return marks;
    }

    /// The table's rows, as the launcher's are, and the spelling line's caret beside them -- beside
    /// prose rows every time, "none" included, so a caret the spelling line took with it is gone.
    void publish_keys(loom::Mail& mail, std::vector<ws::TextRow> rows,
                      const ws::RowsCaret& caret = {}) {
        (void)keys_map_.settle();
        keys_canvas_.publish_rows(
            mail, std::move(rows),
            ws::row_parts(keys_map_, keys_canvas_.columns(),
                          [this](const KeysMeaning& m) { return keys_part(m); }),
            caret, keys_map_.picture());
    }

    // ---- State not in the shape ----------------------------------------------------------

    /// ITS ROOM, the keymap the host last said, its lines, its cursor, its window, its picture,
    /// and what it is doing with the keys right now.
    PaneCanvas keys_canvas_{kHotkeysPane, kDesktopRole};
    ws::KeymapShown keymap_;
    bool keys_heard_ = false;
    /// THE TABLE'S SECTIONS, the group over the open ones the pane's state holds, with the one the
    /// pointer peeks (`component::IndependentAccordions`), and what the inventory last said: the
    /// panes the desk holds, and every pane it knows, so a section of a pane off the desk is kept.
    std::vector<KeysSection> sections_;
    component::IndependentAccordions<std::string> groups_;
    std::set<std::string> on_desk_;
    std::vector<std::string> inventory_known_;
    bool inventory_heard_ = false;
    /// HOW FAR A KEY'S CELLS STAND IN: the cursor's mark, then a section's indent.
    static constexpr std::size_t kKeyIndent = 4;
    std::vector<KeysLine> lines_;
    component::HeldChoice<std::string> keys_choice_;
    std::size_t keys_first_ = 0;
    component::RowMap<KeysMeaning> keys_map_;
    double keys_wheel_ = 0.0;
    std::string keys_notice_;
    std::string refind_id_;
    /// WHETHER THE FOOTER'S HELP LINES SHOW: the layout's `help` setting, asked for again after a
    /// reload, so never reload state; and whether this life holds its row yet.
    bool help_shown_ = true;
    bool held_ = false;
    /// THE ONE MENU THIS PANE ASKED FOR AND HAS NOT HEARD ANSWERED. Image-local, and never in
    /// `DesktopState`: a successor must not accept its predecessor's menu (`menu_answered`).
    pane_menu::Asked keys_asked_;
    struct Capture {
        bool active = false;
        std::string id;
        std::int64_t op = 0;
    };
    Capture capture_;
    struct Typing {
        bool active = false;
        std::string id;
        std::int64_t op = 0;
        zengine::component::TextBox line;
    };
    Typing typing_;
    /// THE EDIT AWAITING ITS ANSWER -- at most one (`ask_edit`).
    struct Edit {
        bool awaiting = false;
        std::uint64_t pending = 0;
        loom::Ticket attempt{};
    };
    Edit edit_;
    /// THE NUMBER ITS NEXT ASK OF THE CLIPBOARD OR THE KEYMAP EDIT DOOR GOES OUT UNDER. One
    /// counter, and never a record: which ask an answer is about is the records' to say (`paste_`,
    /// `edit_`), so one numbered after another does not replace it.
    std::uint64_t asks_ = 0;
    /// THE CLIPBOARD MIRROR THE LINE'S COPY, CUT AND PASTE WORK AGAINST, and the paste in flight.
    zengine::component::Clipboard clip_;
    struct Paste {
        std::uint64_t pending = 0;
        loom::Ticket attempt{};
        std::uint64_t epoch = 0;
        bool awaiting = false;
        std::string office;
    };
    Paste paste_;
};

} // namespace zengine::desktop_pane

#endif // ZENGINE_DESKTOP_PANE_HOTKEYS_VIEW_HPP
