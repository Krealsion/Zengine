// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_DESKTOP_PANE_LAUNCHER_VIEW_HPP
#define ZENGINE_DESKTOP_PANE_LAUNCHER_VIEW_HPP

// The Pane Manager: the host's inventory as a list whose marker holds a pane by identity, the
// mark that shows or hides a row's pane, the row's menu, `n` for the View Builder, and the
// picture a press is judged against. It keeps the host's last reading and edits none of it; the
// weave routes its messages here and owns the reload shape it writes the choice to.
// Workshop law: agents/workshop/desktop-presenting.md

#include "workshop/desktop-pane/desktop_common.hpp"
#include "workshop/desktop-pane/vocabulary.hpp"

#include "workshop/desktop_seam_vocabulary.hpp"
#include "workshop/inspection_seam_vocabulary.hpp"
#include "workshop/pane_canvas_rows.hpp"
#include "workshop/pane_causes.hpp"
#include "workshop/pane_menu.hpp"
#include "workshop/pane_parts.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "component/held_choice.hpp"
#include "component/list_window.hpp"
#include "component/row_map.hpp"
#include "graphics/role.hpp"
#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <zen/weave.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace zengine::desktop_pane {

// ---- The Pane Manager's rows, read backwards ---------------------------------------------------

namespace launcher_row {
constexpr std::int64_t kHeading = 0;
constexpr std::int64_t kMark = 2;    ///< the `[open]`/`[    ]` control: show or hide
constexpr std::int64_t kName = 3;    ///< the row: choose, or open/focus on a deliberate second press
constexpr std::int64_t kMarker = 4;  ///< `^ n more above` / `v n more below`
constexpr std::int64_t kNote = 5;
} // namespace launcher_row

struct LauncherMeaning {
    std::int64_t kind = launcher_row::kHeading;
    std::size_t index = 0; ///< the inventory row, for a mark or a name
    /// THE ROW'S STABLE SUBJECT (`office\x1fpane`), for a name or a mark; empty otherwise. It is
    /// part of the meaning so the PICTURE NUMBER moves when the subject behind a slot changes,
    /// even though the slot's kind, index and geometry did not: an inventory update that swaps two
    /// same-length names leaves the row where it was but makes it MEAN a different pane, and a
    /// press stamped with the old picture must not resolve against the pane that moved in (the
    /// review's first finding). An index alone concealed that.
    std::string ref;
    bool operator==(const LauncherMeaning& o) const {
        return kind == o.kind && index == o.index && ref == o.ref;
    }
};

/// THE ROW THE MARKER HOLDS, by its two durable keys.
struct RefKey {
    std::string office;
    std::string pane;
    bool operator==(const RefKey& o) const { return office == o.office && pane == o.pane; }
};

/// WHERE THE MARK CONTROL SITS ON A LIST ROW: after the two-column marker, six wide.
inline constexpr std::int64_t kMarkFirst = 2;
inline constexpr std::int64_t kMarkWidth = 6;

class LauncherView {
public:
    /// The Pane Manager's own rows, bare keys all. `x` closes, and is not Return's second meaning:
    /// Return only opens or focuses. `m` offers the row's menu. `n` shows the View Builder, where a
    /// pane of the weaver's own is made.
    /// Return and `x` act on the row a press lands on, `pane:<office>/<pane>` (`launcher_part`).
    static std::vector<ws::v3::PaneActionRow> pane_rows() {
        using ws::v3::PaneActionRow;
        const std::string row = ws::on_part(kLauncherRowPrefix);
        return {PaneActionRow{kActionUp, "row up", input::scan::kUp, input::mod::kNone},
                PaneActionRow{kActionDown, "row down", input::scan::kDown, input::mod::kNone},
                PaneActionRow{kActionLaunch, "show or focus", input::scan::kReturn,
                              input::mod::kNone, {}, {ws::cause_of<ws::PaneLaunchRequested>(row)}},
                PaneActionRow{kActionClose, "hide", input::scan::kX, input::mod::kNone, {},
                              {ws::cause_of<ws::PaneCloseRequested>(row)}},
                PaneActionRow{kActionMenu, "row menu", input::scan::kM, input::mod::kNone},
                PaneActionRow{kActionNew, "new pane", input::scan::kN, input::mod::kNone, {},
                              {ws::cause_of<ws::PaneLaunchRequested>(
                                  ws::to_pane(kViewBuilderOffice, kViewBuilderPane))}}};
    }

    // ---- Its rooms ----------------------------------------------------------------------------

    /// THE PROSE ROOM WORKSHOP GRANTED IT, kept for a host that grants no canvas.
    void grant_prose(const ws::PaneRoom& room) { (void)launcher_canvas_.prose_room(room); }

    /// ...AND ITS OWN CANVAS, whose lattice is the room it composes for while it holds one.
    void grant_canvas(const ws::PaneCanvasRoom& room) { launcher_canvas_.canvas_room(room); }

    const PaneCanvas& canvas() const { return launcher_canvas_; }
    /// ...for the office to hand the room a graphics window gives the pane.
    PaneCanvas& canvas() { return launcher_canvas_; }

    /// The rows and columns it composes for: its canvas's lattice while it holds one, its prose
    /// room otherwise.
    void fit_room() { launcher_canvas_.fit(); }

    // ---- What the host and Info say to it -----------------------------------------------------

    /// WHAT PANES THERE ARE, SAID BY THE HOST -- replaced WHOLE, never merged: this pane keeps no
    /// copy it edits, and finds the weaver's choice in it again by identity.
    void inventory_said(const std::vector<ws::InventoryPane>& panes, DesktopState& state,
                        const Declared& pane_rows_decl, const Declared& app, loom::Mail& mail) {
        known_ = panes;
        heard_ = true;
        find_cursor(state);
        say(state, pane_rows_decl, app, mail);
    }

    /// WHAT A LAUNCH, CLOSE OR TOGGLE CAME TO, once the weave has matched it to its latest: the
    /// host's refusal is the notice, and a request that happened needs none -- the row's mark
    /// says it when the inventory is said again.
    void launch_answered(const std::string& refusal, const DesktopState& state,
                         const Declared& pane_rows_decl, const Declared& app, loom::Mail& mail) {
        notice_ = refusal;
        say(state, pane_rows_decl, app, mail);
    }

    /// WHAT INFO MADE OF THIS IMAGE'S LATEST INSPECT ASK, and no older one: the pane inspected, or
    /// the reason it was not -- a live draft, or the host refusing the pane -- in Info's words.
    void inspect_answered(const ws::PaneSubjectActed& said, const DesktopState& state,
                          const Declared& pane_rows_decl, const Declared& app, loom::Mail& mail) {
        if (!mail.answers_ask() || inspecting_.asked == 0 ||
            mail.correlation() != inspecting_.asked) {
            return;
        }
        notice_ = said.accepted ? "Info inspects " + inspecting_.name
                                : "Info did not inspect " + inspecting_.name + " -- " + said.refusal;
        inspecting_ = Inspecting{};
        say(state, pane_rows_decl, app, mail);
    }

    /// ...AND AN INSPECT INFO NEVER HEARD -- not loaded, or not taking the ask: said. True when
    /// the refusal named that ask.
    bool inspect_refused(const loom::DispatchRefused& refused, const DesktopState& state,
                         const Declared& pane_rows_decl, const Declared& app, loom::Mail& mail) {
        if (!refused_ask(inspecting_.asked != 0, inspecting_.sent, inspecting_.asked, refused,
                         mail, ws::InspectPaneRequested::zen_name,
                         ws::InspectPaneRequested::zen_version, kInfoRole)) {
            return false;
        }
        notice_ = "Info could not be asked to inspect " + inspecting_.name + " (" +
                  refused.reason + ")";
        inspecting_ = Inspecting{};
        say(state, pane_rows_decl, app, mail);
        return true;
    }

    // ---- The keys -----------------------------------------------------------------------------

    /// ONE OF THE PANE MANAGER'S OWN ROWS, while it holds the keyboard; the list is said after.
    void launcher_action(const std::string& id, DesktopState& state, const Declared& pane_rows_decl,
                         const Declared& app, loom::Mail& mail, std::uint64_t& launches) {
        if (id == kActionUp) {
            step(-1, state);
        } else if (id == kActionDown) {
            step(+1, state);
        } else if (id == kActionLaunch) {
            launch_cursor(mail, launches);
        } else if (id == kActionClose) {
            close_cursor(mail, launches);
        } else if (id == kActionMenu) {
            // THE MENU KEY: the same rows a right press on the marked row offers, continuing
            // THIS keystroke (its number is on the envelope), beside the marked row.
            offer_launcher_menu(mail, mail.correlation());
        } else if (id == kActionNew) {
            // A PANE OF THE WEAVER'S OWN IS MADE IN THE VIEW BUILDER, shown through the host's
            // launch door like any tool: this weave holds no definition and makes nothing itself.
            launch(mail, kViewBuilderOffice, kViewBuilderPane, launches);
        } else {
            return;
        }
        say(state, pane_rows_decl, app, mail);
    }

    // ---- The mouse ----------------------------------------------------------------------------

    // WL-DESK-14 -- agents/workshop/desktop-presenting.md
    /// A PRESS ON THE MARK SHOWS OR HIDES THE ROW'S PANE; a press on the name CHOOSES the row; a
    /// DELIBERATE SECOND PRESS on the marked name, with the keys already here, is Return's meaning
    /// -- open, or focus and lift. The first press (the one that brings the keys, or one on an
    /// unmarked name) only moves the marker, so a press never means two things at once.
    void launcher_press(const PanePress& press, DesktopState& state, const Declared& pane_rows_decl,
                        const Declared& app, loom::Mail& mail, std::uint64_t& launches) {
        if (!press.current) {
            notice_ = kMovedSentence;
            say(state, pane_rows_decl, app, mail);
            return;
        }
        const LauncherMeaning* m = map_.at(press.row, press.column);
        if (m == nullptr || m->index >= known_.size()) {
            return; // a heading, a marker, a note, the name line: pointing, and nothing more
        }
        const ws::InventoryPane& p = known_[m->index];
        if (m->kind == launcher_row::kMark) {
            notice_.clear();
            if (!p.available) {
                notice_ = p.pending ? p.name + " is not here yet -- this run is still loading it"
                                    : p.name + " is not available -- its provider is not in this "
                                               "Workshop";
            } else if (p.open) {
                close(mail, p.office, p.pane, launches);
            } else {
                launch(mail, p.office, p.pane, launches);
            }
            say(state, pane_rows_decl, app, mail);
            return;
        }
        if (m->kind != launcher_row::kName) {
            return;
        }
        const bool marked_here = choice_.actionable() && choice_.at == m->index;
        if (press.keys_went_here && marked_here) {
            notice_.clear();
            launch(mail, p.office, p.pane, launches);
        } else {
            hold(static_cast<std::int64_t>(m->index), state);
            notice_.clear();
        }
        say(state, pane_rows_decl, app, mail);
    }

    /// A RIGHT PRESS on a row OFFERS that row's menu, beside the press, continuing it; a right
    /// press on anything that is not a row of this pane's own (a heading, a marker, a note) is
    /// handed back to the host, whose own pane menu answers.
    void second_button(const PanePress& b, DesktopState& state, const Declared& pane_rows_decl,
                       const Declared& app, loom::Mail& mail) {
        if (!b.current) {
            notice_ = kMovedSentence;
            say(state, pane_rows_decl, app, mail);
            return;
        }
        const LauncherMeaning* m = map_.at(b.row, b.column);
        if (m != nullptr && (m->kind == launcher_row::kName || m->kind == launcher_row::kMark) &&
            m->index < known_.size()) {
            hold(static_cast<std::int64_t>(m->index), state);
            notice_.clear();
            offer_launcher_row(m->index, b.row, b.column, mail, mail.correlation());
            say(state, pane_rows_decl, app, mail);
            return;
        }
        (void)pane_menu::pass_back(mail, kDesktopRole, kLauncherPane);
    }

    /// THE WHEEL WALKS THE MARKER, one row per notch, fractions carried (Powers' convention).
    void wheel(double dy, DesktopState& state, const Declared& pane_rows_decl, const Declared& app,
               loom::Mail& mail) {
        const std::int64_t notches = take_notches(wheel_, dy);
        if (notches != 0) {
            step(-notches, state);
            say(state, pane_rows_decl, app, mail);
        }
    }

    /// What a menu came to, if it answers this image's own ask (`Asked::take`: from the
    /// presenter's office, under the unanswered ask's number, about the pane and subject asked,
    /// once). A reloaded desktop asked nothing, so its predecessor's menus act on nothing.
    void menu_answered(const ws::PaneMenuAnswered& a, DesktopState& state,
                       const Declared& pane_rows_decl, const Declared& app, loom::Mail& mail,
                       std::uint64_t& launches) {
        if (!launcher_asked_.take(mail, a).empty()) {
            launcher_chose(a, state, pane_rows_decl, app, mail, launches);
        }
    }

    // ---- What it shows ------------------------------------------------------------------------

    /// THE HEADING, THEN THE LIST THROUGH A WINDOW THAT KEEPS THE MARKER VISIBLE, THEN THE
    /// NOTICES. The notices' rows are reserved before the list is laid out, so feedback is never
    /// cut off below a full list; each cut in the list is counted on its own row. Every row is
    /// recorded in the row map as it is written, and the composition is numbered by it.
    void say(const DesktopState& state, const Declared& pane_rows_decl, const Declared& app,
             loom::Mail& mail) {
        const std::int64_t rows = launcher_canvas_.rows();
        const std::int64_t columns = launcher_canvas_.columns();
        if (!launcher_canvas_.granted() || rows <= 0 || columns <= 0) {
            return;
        }
        std::vector<surface::SurfaceTextRow> out;
        map_.begin();
        const auto push = [&out, rows, columns, this](std::string text, std::int64_t role,
                                                      LauncherMeaning m) {
            if (static_cast<std::int64_t>(out.size()) < rows) {
                map_.row(static_cast<std::int64_t>(out.size()), m);
                out.push_back(surface::SurfaceTextRow{fit(std::move(text), columns), role});
            }
        };
        if (!heard_) {
            // THE HOST HAS NOT SAID ANYTHING YET, WHICH IS NOT THE SAME AS THERE BEING NO
            // PANES. An empty list here would read as a Workshop with no tools in it.
            push("PANES (waiting)", graphics::role::kMuted, LauncherMeaning{});
            publish_launcher(mail, std::move(out));
            return;
        }
        push("PANES -- " + std::to_string(known_.size()), graphics::role::kAccent,
             LauncherMeaning{});
        std::vector<std::string> notes;
        // THE MARKER HOLDING NOTHING IS SAID FOR AS LONG AS IT HOLDS NOTHING -- a state, not an
        // event, so a later sentence about something else cannot take its row. The name is the
        // one this image last saw on the row; an image that never saw it says the durable key.
        const std::string lost =
            choice_.lost ? (held_name_.empty() ? state.cursor_pane : held_name_) +
                               " left the list -- choose a row before Return shows anything"
                         : std::string();
        // (!) A ROW MARKED GONE IS EXPLAINED FOR AS LONG AS ONE IS, on the same terms: the host
        // supplied the fact (`InventoryPane::available`, asked of the office's holder now), and
        // what brings the tool back is the weaver's. Still to come is not missing.
        bool any_gone = false;
        for (const ws::InventoryPane& p : known_) {
            any_gone = any_gone || (!p.available && !p.pending);
        }
        const std::string gone =
            any_gone ? std::string("[gone]: build its provider, then launch again") : std::string();
        const std::string* said[] = {&lost, &notice_, &gone, &pane_rows_decl.word, &app.word};
        for (const std::string* word : said) {
            if (!word->empty()) {
                notes.push_back(drawable(*word));
            }
        }
        std::int64_t budget = rows - 1;
        // AT LEAST ONE LIST ROW STAYS, so the marker is never the thing a notice pushed out.
        const std::int64_t room_for_notes = known_.empty() ? budget : budget - 1;
        const std::int64_t note_rows =
            static_cast<std::int64_t>(notes.size()) < room_for_notes
                ? static_cast<std::int64_t>(notes.size())
                : (room_for_notes > 0 ? room_for_notes : 0);
        budget -= note_rows;
        const component::ListWindow w = component::cursor_window(
            known_.size(), choice_.at, first_, budget > 0 ? static_cast<std::size_t>(budget) : 0);
        first_ = w.first;
        if (const std::size_t earlier = component::earlier_marker(w); earlier > 0) {
            push("  ^ " + std::to_string(earlier) + " more above", graphics::role::kMuted,
                 LauncherMeaning{launcher_row::kMarker, 0, {}});
        }
        for (std::size_t i = w.first; i < w.end(); ++i) {
            const ws::InventoryPane& p = known_[i];
            const bool here = i == choice_.at;
            // (!) FOUR STATES, NOT TWO, because the host answered three questions. A closed
            // tool can be opened; an unavailable one cannot, and saying "closed" of it would
            // send a weaver pressing Return at a pane that is never going to appear; one the run
            // is still loading is neither, and `[gone]` of it would be a verdict nobody reached.
            std::string mark = p.open ? "[open]" : "[    ]";
            std::int64_t role = p.open ? graphics::role::kAccent : graphics::role::kFill;
            if (!p.available && p.pending) {
                mark = "[load]";
                role = graphics::role::kMuted;
            } else if (!p.available) {
                mark = "[gone]";
                role = graphics::role::kAlert;
            }
            // A MARKER THAT HOLDS NOTHING (its pane left the list) is `?`, not `>`.
            const char* marker = here ? (choice_.lost ? "? " : "> ") : "  ";
            const std::int64_t at = static_cast<std::int64_t>(out.size());
            const std::string text = fit(std::string(marker) + mark + " " + drawable(p.name), columns);
            const std::string ref = launcher_ref(i);
            push(text, role, LauncherMeaning{launcher_row::kName, i, ref});
            // THE MARK IS A CONTROL INSIDE THE ROW: recorded only where the cut left it whole.
            (void)map_.span(at, kMarkFirst, kMarkWidth,
                            component::solid_columns(text, static_cast<std::size_t>(columns)),
                            LauncherMeaning{launcher_row::kMark, i, ref});
        }
        if (const std::size_t more = component::more_marker(w); more > 0) {
            push("  v " + std::to_string(more) + " more below", graphics::role::kMuted,
                 LauncherMeaning{launcher_row::kMarker, 1, {}});
        }
        for (std::int64_t i = 0; i < note_rows; ++i) {
            push("  " + notes[static_cast<std::size_t>(i)], graphics::role::kAlert,
                 LauncherMeaning{launcher_row::kNote, static_cast<std::size_t>(i), {}});
        }
        publish_launcher(mail, std::move(out));
    }

    /// WHAT A PANE MANAGER PART IS CALLED: a row by the pane it lists, `pane:<office>/<pane>`, and
    /// its mark `mark:<office>/<pane>` -- the reference a setup file and Info's Identity say.
    std::string launcher_part(const LauncherMeaning& m) const {
        if (m.index >= known_.size()) {
            return std::string();
        }
        const std::string ref = known_[m.index].office + "/" + known_[m.index].pane;
        if (m.kind == launcher_row::kName) {
            return kLauncherRowPrefix + ref;
        }
        return m.kind == launcher_row::kMark ? "mark:" + ref : std::string();
    }

private:
    // ---- The cursor, held by identity -----------------------------------------------------------

    // WL-DESK-10 -- agents/workshop/desktop-presenting.md
    /// Find the row the weaver chose, in the list as the host just said it, by identity. A choice
    /// whose row left is still a choice: the keys stay in `DesktopState`, the marker holds nothing
    /// and says so, and Return and `x` wait for a new choice, here and in any image a reload hands
    /// the state to. Only a cursor never given a pane takes the row it stands on.
    void find_cursor(DesktopState& state) {
        choice_.key = RefKey{state.cursor_office, state.cursor_pane};
        choice_.chosen = !state.cursor_office.empty() || !state.cursor_pane.empty();
        choice_.at = state.cursor < 0 ? 0 : static_cast<std::size_t>(state.cursor);
        choice_.find(known_, key_of_pane);
        if (!choice_.lost && choice_.at < known_.size()) {
            held_name_ = known_[choice_.at].name;
        }
        write_choice(state);
    }

    static RefKey key_of_pane(const ws::InventoryPane& p) { return RefKey{p.office, p.pane}; }

    /// THE CHOICE, WRITTEN TO THE RELOAD SHAPE: where the marker stands and which pane it holds.
    void write_choice(DesktopState& state) {
        state.cursor = static_cast<std::int64_t>(choice_.at);
        state.cursor_office = choice_.chosen ? choice_.key.office : std::string();
        state.cursor_pane = choice_.chosen ? choice_.key.pane : std::string();
    }

    void hold(std::int64_t at, DesktopState& state) {
        if (at < 0 || static_cast<std::size_t>(at) >= known_.size()) {
            return;
        }
        choice_.hold(known_, static_cast<std::size_t>(at), key_of_pane);
        held_name_ = known_[choice_.at].name;
        write_choice(state);
    }

    void step(std::int64_t by, DesktopState& state) {
        if (known_.empty()) {
            return;
        }
        (void)choice_.step(known_, by, key_of_pane);
        held_name_ = known_[choice_.at].name;
        write_choice(state);
        notice_.clear();
    }

    void launch_cursor(loom::Mail& mail, std::uint64_t& launches) {
        if (known_.empty()) {
            return;
        }
        if (!choice_.actionable()) {
            notice_ = "Return showed nothing -- choose a row first";
            return;
        }
        notice_.clear();
        // THE IDENTITY THE MARKER HOLDS, not the index: what the weaver sees is what opens.
        launch(mail, choice_.key.office, choice_.key.pane, launches);
    }

    void close_cursor(loom::Mail& mail, std::uint64_t& launches) {
        if (known_.empty()) {
            return;
        }
        if (!choice_.actionable()) {
            notice_ = "x hid nothing -- choose a row first";
            return;
        }
        notice_.clear();
        close(mail, choice_.key.office, choice_.key.pane, launches);
    }

    // ---- Its menu -----------------------------------------------------------------------------

    /// THE STABLE SUBJECT OF A PANE MANAGER ROW, built the one way so the picture's spans and the
    /// menu-key's row lookup agree on what a row MEANS. Empty for an index off the end.
    std::string launcher_ref(std::size_t index) const {
        return index < known_.size() ? join_subject({known_[index].office, known_[index].pane})
                                     : std::string();
    }

    /// THE ROWS A PANE MANAGER ROW OFFERS: what the row's state allows, then management and
    /// inspection through their owners. The subject is the row's `PaneRef`, so the answer is
    /// judged against the inventory as it is when it arrives.
    void offer_launcher_row(std::size_t index, std::int64_t row, std::int64_t column,
                            loom::Mail& mail, std::uint64_t correlation) {
        const ws::InventoryPane& p = known_[index];
        pane_menu::Offer offer(kLauncherPane, join_subject({p.office, p.pane}));
        offer.at(row, column);
        if (p.open) {
            offer.row(kMenuFocus, "focus " + p.name);
            offer.row(kMenuClose, "hide " + p.name);
        } else if (p.available) {
            offer.row(kMenuOpen, "show " + p.name);
        }
        offer.submenu(kMenuManage, "manage " + p.name);
        offer.row(kMenuInspect, "inspect in Info");
        launcher_asked_ = offer.continuing(mail, kDesktopRole, correlation);
    }

    /// THE MENU KEY: the marked row's menu, beside the marked row.
    void offer_launcher_menu(loom::Mail& mail, std::uint64_t correlation) {
        if (!choice_.actionable() || choice_.at >= known_.size()) {
            notice_ = "no row is chosen -- choose one, then open its menu";
            return;
        }
        const std::int64_t row =
            map_.row_of(LauncherMeaning{launcher_row::kName, choice_.at, launcher_ref(choice_.at)});
        notice_.clear();
        offer_launcher_row(choice_.at, row < 0 ? 0 : row, 0, mail, correlation);
    }

    /// WHAT A PANE MANAGER MENU CAME TO. The subject names the pane; it must still be in the
    /// list, or the choice is about a row the weaver can no longer see.
    void launcher_chose(const ws::PaneMenuAnswered& a, const DesktopState& state,
                        const Declared& pane_rows_decl, const Declared& app, loom::Mail& mail,
                        std::uint64_t& launches) {
        const std::vector<std::string> parts = split_subject(a.subject);
        if (parts.size() != 2) {
            return;
        }
        const ws::InventoryPane* p = nullptr;
        for (const ws::InventoryPane& each : known_) {
            if (each.office == parts[0] && each.pane == parts[1]) {
                p = &each;
                break;
            }
        }
        if (p == nullptr) {
            notice_ = parts[1] + " left the list -- nothing done";
            say(state, pane_rows_decl, app, mail);
            return;
        }
        notice_.clear();
        if (a.id == kMenuOpen || a.id == kMenuFocus) {
            launch(mail, p->office, p->pane, launches);
        } else if (a.id == kMenuClose) {
            close(mail, p->office, p->pane, launches);
        } else if (a.id == kMenuManage) {
            // THE HOST'S OWN PANE MENU ON THAT PANE -- covered, closed or consuming every right
            // press -- continuing this choice (its number is on the envelope).
            (void)pane_menu::manage(mail, kDesktopRole, kLauncherPane, p->office, p->pane);
        } else if (a.id == kMenuInspect) {
            // INSPECTION THROUGH ITS OWNER: Info takes the ask as its own inspect and answers what
            // came of it, and only the newest ask is waited on.
            const std::uint64_t asked = ++inspect_asks_;
            const loom::Ticket sent =
                mail.as_role(kDesktopRole)
                    .send_to_role(kInfoRole, ws::InspectPaneRequested{p->office, p->pane}, asked);
            inspecting_ = Inspecting{};
            if (sent.valid()) {
                inspecting_ = Inspecting{asked, sent, p->name};
                notice_ = "asking Info to inspect " + p->name;
            } else {
                notice_ = "inspect in Info not sent -- nothing was queued";
            }
        }
        say(state, pane_rows_decl, app, mail);
    }

    /// The launcher's rows, as its picture while it holds a canvas room -- numbered under the row
    /// map's own number, which fences a press -- and as prose to a host granting none.
    void publish_launcher(loom::Mail& mail, std::vector<surface::SurfaceTextRow> rows) {
        const std::int64_t meaning = map_.settle();
        launcher_canvas_.publish_rows(
            mail, meaning, std::move(rows),
            ws::row_parts(map_, launcher_canvas_.columns(),
                          [this](const LauncherMeaning& m) { return launcher_part(m); }));
    }

    // ---- State not in the shape ----------------------------------------------------------

    /// THE HOST'S LAST INVENTORY, held between publications and owned by nobody here.
    std::vector<ws::InventoryPane> known_;
    bool heard_ = false;

    PaneCanvas launcher_canvas_{kLauncherPane, kDesktopRole};
    /// WHERE THE LIST'S WINDOW BEGAN LAST TIME, so it scrolls by the least it can.
    std::size_t first_ = 0;
    std::string notice_;
    /// THE NAME THIS IMAGE LAST SAW ON THE CHOSEN ROW, for the sentence if it leaves the list.
    std::string held_name_;
    /// THE CHOICE, HELD BY IDENTITY -- derived from `DesktopState`'s keys whenever the list is
    /// said and written back whenever it moves, so a reload cannot forget it.
    component::HeldChoice<RefKey> choice_;
    /// THE PICTURE READ BACKWARDS, and its number: what a press is judged against.
    component::RowMap<LauncherMeaning> map_;
    /// THE ONE MENU THIS PANE ASKED FOR AND HAS NOT HEARD ANSWERED. Image-local, and never in
    /// `DesktopState`: a successor must not accept its predecessor's menu (`menu_answered`).
    pane_menu::Asked launcher_asked_;
    double wheel_ = 0.0;
    /// THE INSPECT THIS IMAGE ASKED OF INFO and waits on: its number, its queued attempt, which
    /// a refusal names, and the pane's name for the sentence. Not in `DesktopState`.
    struct Inspecting {
        std::uint64_t asked = 0;
        loom::Ticket sent{};
        std::string name;
    };
    Inspecting inspecting_;
    std::uint64_t inspect_asks_ = 0;
};

} // namespace zengine::desktop_pane

#endif // ZENGINE_DESKTOP_PANE_LAUNCHER_VIEW_HPP
