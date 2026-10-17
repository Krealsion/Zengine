// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_NEOVIM_EDITOR_TRANSFERS_HPP
#define ZENGINE_NEOVIM_EDITOR_TRANSFERS_HPP

// The Neovim-backed Editor's pointer and transfers: members of `NeovimEditorWeave`, which pane.cpp
// declares and includes this after. Neovim's own mouse on its screen; a press that becomes a carry
// of the selection or this file's location (WL-NVIM-10); material dropped in as data (WL-NVIM-11);
// a dropped location opened (WL-NVIM-12); and a change Neovim holds until it answers (WL-NVIM-13).
// Workshop law: agents/workshop/neovim-transfers.md

// The same Workshop carry the standard Editor answers, through Neovim's own owners: the copy is
// what Neovim's yank takes, the insertion is `nvim_buf_set_text` in one undo block, and every
// check of "still the same" is Neovim's buffer and changedtick, never the screen alone
// (`editor/neovim/lua.hpp`). Nothing here writes, builds, sends, or feeds a payload byte as a key.

#include "editor/neovim-editor/common.hpp"
#include "editor/neovim-editor/vocabulary.hpp"

#include "editor/neovim/lua.hpp"
#include "editor/office_seam.hpp"
#include "editor/source-transfer/cpp.hpp"
#include "editor/source-transfer/material.hpp"
#include "workshop/open_seam_vocabulary.hpp"
#include "workshop/pane_canvas_rows.hpp"
#include "workshop/pane_canvas_vocabulary.hpp"
#include "workshop/pane_carry.hpp"
#include "workshop/pane_menu.hpp"
#include "workshop/pane_operation.hpp"
#include "workshop/persist.hpp"

#include <zen/weave.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

// ---- The pointer: Neovim's own mouse, and the press that becomes a carry --------------------

/// THE POINTER ON THIS PANE'S CANVAS, each place read back to the row and column of the lattice
/// its rows stand on: Neovim's own mouse on its screen -- the left press, that press's drag and
/// its release, the right press and its release, the wheel -- and on the status row a focus
/// statement. A right drag and the middle button are not Neovim's here; a right press beside
/// the rows is handed back, so the host's pane menu opens there.
inline void NeovimEditorWeave::on(const ws::PaneCanvasPointer& event, loom::Mail& mail) {
    if (!mail.authored_from_role(kWorkshopRole) || event.pane != nve::kEditorPane || !on_canvas()) {
        return;
    }
    const ws::RowCell at = ws::row_cell_at(ws::canvas_rows(canvas_), event.x, event.y);
    const bool same_room = event.grant == canvas_.grant;
    const bool press = event.phase == ws::canvas_pointer::kPress;
    const bool ended = event.phase == ws::canvas_pointer::kRelease || event.phase == ws::canvas_pointer::kLost;
    if (press && same_room && event.button == 1) {
        pressed_ = at.shown ? event.gesture : 0;
        if (at.shown) {
            press_at(at.row, at.column, event.grant, event.picture, mail);
        }
    } else if (event.phase == ws::canvas_pointer::kMove && event.button == 1 && pressed_ != 0 &&
               event.gesture == pressed_) {
        dragged(at.row, at.column, mail);
    } else if (ended && event.button == 1 && event.gesture == pressed_ && pressed_ != 0) {
        pressed_ = 0;
        if (!grab_.started) {
            release_drag();
            flush(mail);
        }
    } else if (press && same_room && event.button == 3) {
        if (!at.shown) {
            (void)ws::pane_menu::pass_back(mail, nve::kEditorOffice, nve::kEditorPane);
            return;
        }
        right_press(at.row, at.column, event.grant, event.picture, mail);
    } else if (ended && event.button == 3) {
        right_release(at.row, at.column, mail);
    } else if (event.phase == ws::canvas_pointer::kWheel && same_room) {
        wheel(event.dy, mail);
    }
}

/// A PRESS ON NEOVIM'S SCREEN IS NEOVIM'S OWN PRESS; on the status row it is a focus statement
/// and moves nothing. On a picture still current it may mean one thing more once the hand moves:
/// ON the painted Visual highlight it remembers the selection Neovim held (its text taken now,
/// as its yank takes it), and on the status row this file's location (WL-NVIM-10).
// WL-NVIM-10 -- agents/workshop/neovim-transfers.md
inline void NeovimEditorWeave::press_at(std::int64_t row, std::int64_t column, std::int64_t grant, std::int64_t picture,
                                        loom::Mail& mail) {
    if (held_still()) {
        return;
    }
    release_drag();
    grab_ = Grab{};
    if (running()) {
        pump(mail); // what Neovim drew since the last picture, before the press is judged
    }
    const bool current = fresh(grant, picture);
    if (!running() || row < kChromeRows) {
        if (row == 0 && current && running() && host_->ready() && !doc_path().empty()) {
            grab_ = Grab{true, false, Take::Location, row, column, mail.correlation(), Snapshot{}};
        }
        return;
    }
    notice_.clear();
    const std::int64_t r = row - kChromeRows;
    const std::int64_t c = column < 0 ? 0 : column;
    if (current && on_highlight(r, c)) {
        Snapshot snap = snapshot_now();
        if (snap.ok) {
            grab_ = Grab{true, false, Take::Selection, row, column, mail.correlation(), std::move(snap)};
        }
    }
    drag_ = Drag{true, r, c};
    mouse("left", "press", drag_.row, drag_.col);
    flush(mail);
}

/// The hand moved with the left press held: Neovim's own drag, clamped to its screen -- or, for
/// a remembered press, a carry on its first motion to another cell.
inline void NeovimEditorWeave::dragged(std::int64_t row, std::int64_t column, loom::Mail& mail) {
    if (held_still()) {
        return;
    }
    // A REMEMBERED PRESS BECOMES A CARRY ON ITS FIRST MOTION TO ANOTHER CELL, never a sweep
    // (WL-NVIM-10): Neovim's own press ends as the click it was, the Visual selection is put
    // back (`gv`) when the buffer has not moved since, and the copy is the one taken at the press.
    if (grab_.armed) {
        if (!grab_.started && (row != grab_.row || column != grab_.column)) {
            grab_.started = true;
            if (grab_.what == Take::Location) {
                acquire_location(true, grab_.gesture, mail);
            } else {
                release_drag();
                std::string why;
                const std::optional<mp::Value> now = lua_now(nv::lua::kDocFacts, nv::rpc::params(), kAskMs, why);
                const bool same = now.has_value() && field(*now, "buf").as_int(-1) == grab_.snap.buf &&
                                  field(*now, "tick").as_int(-1) == grab_.snap.tick;
                if (!same) {
                    notice("nothing was carried -- the buffer changed after you pressed the highlight", true);
                } else {
                    (void)host_->input("gv");
                    carry_snapshot(grab_.snap, true, grab_.gesture, mail);
                }
            }
            flush(mail);
            resay_ = true;
        }
        return;
    }
    if (!drag_.down || !running()) {
        return;
    }
    drag_.row = std::clamp<std::int64_t>(row - kChromeRows, 0, ui_rows(rows_) - 1);
    drag_.col = std::clamp<std::int64_t>(column, 0, ui_cols(columns_) - 1);
    mouse("left", "drag", drag_.row, drag_.col);
    flush(mail);
}

/// THE SECOND BUTTON IS NEOVIM'S: a right press and its release cross as Neovim's own mouse
/// events (`nvim_input_mouse`), consumed whole -- no host menu opens over Neovim's screen. A
/// right drag does not cross, the middle button is left to Neovim's default of nothing, and a
/// lost release is delivered as a release so Neovim's own state agrees with the hand. This is
/// press and release, not Neovim's complete mouse.
inline void NeovimEditorWeave::right_press(std::int64_t row, std::int64_t column, std::int64_t grant,
                                           std::int64_t picture, loom::Mail& mail) {
    if (held_still() || !running()) {
        return;
    }
    grab_ = Grab{};
    if (row < kChromeRows) {
        // THE STATUS ROW NAMES THIS FILE: its menu carries the location (WL-NVIM-10).
        if (row == 0 && host_->ready() && !doc_path().empty()) {
            menu_take_ = Take::Location;
            menu_ = ws::pane_menu::Offer(nve::kEditorPane, "location")
                        .at(row, column)
                        .row(kLocationRow, "Carry this file's location")
                        .send(mail, nve::kEditorOffice);
        }
        return; // otherwise a focus statement that moves nothing
    }
    // ON THE PAINTED VISUAL HIGHLIGHT THE RIGHT PRESS IS NOT NEOVIM'S YET: a menu offers the
    // copy and Neovim's own menu, so neither a destructive click nor a replaced popup comes first.
    const std::int64_t r = row - kChromeRows;
    const std::int64_t c = column < 0 ? 0 : column;
    pump(mail);
    if (fresh(grant, picture) && on_highlight(r, c)) {
        Snapshot snap = snapshot_now();
        if (snap.ok) {
            release_drag();
            menu_take_ = Take::Selection;
            menu_snap_ = std::move(snap);
            menu_cell_ = Drag{true, r, c};
            menu_ = ws::pane_menu::Offer(nve::kEditorPane, "selection")
                        .at(row, column)
                        .row(kExtractRow, "Extract selection to Inventory")
                        .row(kNeovimMenuRow, "Neovim's own menu")
                        .send(mail, nve::kEditorOffice);
            return;
        }
    }
    release_drag();
    right_ = Drag{true, r, c};
    mouse("right", "press", right_.row, right_.col);
    flush(mail);
}

inline void NeovimEditorWeave::right_release(std::int64_t row, std::int64_t column, loom::Mail& mail) {
    if (held_still() || !running() || !right_.down) {
        return;
    }
    const std::int64_t r = std::clamp<std::int64_t>(row - kChromeRows, 0, ui_rows(rows_) - 1);
    const std::int64_t c = std::clamp<std::int64_t>(column, 0, ui_cols(columns_) - 1);
    right_ = Drag{};
    mouse("right", "release", r, c);
    flush(mail);
}

inline void NeovimEditorWeave::wheel(double dy, loom::Mail& mail) {
    if (held_still() || !running()) {
        return;
    }
    release_drag();
    wheel_ += dy;
    const std::int64_t notches = static_cast<std::int64_t>(wheel_);
    wheel_ -= static_cast<double>(notches);
    const std::int64_t row = ui_rows(rows_) / 2;
    for (std::int64_t i = 0; i < (notches < 0 ? -notches : notches); ++i) {
        mouse("wheel", notches > 0 ? "up" : "down", row, 0);
    }
    flush(mail);
}

// ---- Transfers: the selection carried out, material dropped in as data, a location opened ---

/// MATERIAL DROPPED ON NEOVIM'S SCREEN (WL-NVIM-11), its place read back to the lattice cell it
/// fell on.
inline void NeovimEditorWeave::on(const ws::PaneCanvasValueDrop& drop, loom::Mail& mail) {
    if (!mail.authored_from_role(kWorkshopRole) || drop.pane != nve::kEditorPane) {
        return;
    }
    if (held_still()) {
        return;
    }
    grab_ = Grab{};
    const ws::RowCell at = ws::row_cell_at(ws::canvas_rows(canvas_), drop.x, drop.y);
    receive(drop.data, at, drop.grant, drop.picture, mail); // each outcome says its own notice
    flush(mail);
    resay_ = true;
}

inline void NeovimEditorWeave::on(const ws::PaneMenuAnswered& answer, loom::Mail& mail) {
    if (drop_.pending && drop_.menu.pending() && answer.subject == kDropSubject) {
        const std::string chosen = drop_.menu.take(mail, answer);
        if (drop_.menu.pending()) {
            return;
        }
        Dropped d = std::move(drop_);
        drop_ = Dropped{};
        if (chosen.empty()) {
            notice("the dropped " + d.material.what + " was not inserted", false);
        } else if (chosen == kInsertLine) {
            insert_command(d.material, d.aim);
        } else if (chosen == kInsertCpp) {
            insert_cpp(d.material, d.aim);
        }
        flush(mail);
        resay_ = true;
        return;
    }
    const std::string chosen = menu_.take(mail, answer);
    if (chosen == kExtractRow) {
        const Snapshot now = snapshot_now();
        if (!now.ok || !(now == menu_snap_)) {
            notice(now.ok ? std::string("nothing was carried -- the selection changed after the menu opened")
                          : "nothing was carried -- " + now.refusal,
                   true);
        } else {
            carry_snapshot(now, false, mail.correlation(), mail);
        }
    } else if (chosen == kLocationRow) {
        acquire_location(false, mail.correlation(), mail);
    } else if (chosen == kNeovimMenuRow && running()) {
        // NEOVIM'S OWN POPUP, as the right press it would have been.
        mouse("right", "press", menu_cell_.row, menu_cell_.col);
        mouse("right", "release", menu_cell_.row, menu_cell_.col);
    }
    flush(mail);
    resay_ = true;
}

/// WORKSHOP'S WORD ON WHETHER THIS GESTURE'S ACTOR MAY DO WHAT THIS PANE ASKED: carry a copy, or
/// open a dropped location (`office::answer_operation`), settled against whether it names the
/// buffer Neovim holds and that buffer's changedtick now.
inline void NeovimEditorWeave::on(const ws::PaneOperationAnswered& answer, loom::Mail& mail) {
    const auto stamp = [this](Locate& l) {
        l.same_path = doc_path() == l.loc.path;
        l.snapshot = doc_.tick;
    };
    if (office::answer_operation(answer, mail, nve::kEditorOffice, nve::kEditorPane, asked_, pickup_, locate_, stamp,
                                 noticing())) {
        resay_ = true;
    }
}

/// WORKSHOP TOOK THE COPY, OR SAID WHY NOT. Neovim's buffer was never touched either way.
inline void NeovimEditorWeave::on(const ws::PaneCarryAnswered& answer, loom::Mail& mail) {
    if (office::settle_carry(answer, mail, pickup_, "Neovim's buffer is unchanged", noticing())) {
        resay_ = true;
    }
}

// ---- A change Neovim holds until it answers (WL-NVIM-13) ------------------------------------

/// ONE CHANGE ASKED OF THE MODULE, OWNED UNTIL NEOVIM ANSWERS IT (WL-NVIM-13). `settle` runs
/// exactly once with what Neovim said: now, when it answered within the bound or the change
/// never reached it; or later, from `pump`, when Neovim holds the request -- which the Editor
/// then says (`waiting`, and why) and keeps as `held_`. Nothing is sent while another change is
/// held: that one is answered first, and this one is refused.
// WL-NVIM-13 -- agents/workshop/neovim-transfers.md
inline void NeovimEditorWeave::change(const char* chunk, mp::Value args, std::string what,
                                      const std::string& waiting, Settle settle) {
    if (!settle_held()) {
        settle(Heard{std::nullopt, held_->what + " is still waiting for Neovim; try again once it has settled", false});
        return;
    }
    if (!running()) {
        settle(Heard{std::nullopt, "Neovim is not running" + (failure_.empty() ? std::string() : ": " + failure_), false});
        return;
    }
    auto answer = std::make_shared<std::optional<nv::rpc::Response>>();
    std::optional<nv::rpc::Response> got;
    std::string why;
    switch (host_->ask("nvim_exec_lua", nv::rpc::params(mp::Value::str(chunk), std::move(args)), kAskMs, got, why,
                       [answer](const nv::rpc::Response& r) { *answer = r; })) {
    case nv::Host::Asked::Answered:
        settle(heard_of(*got));
        return;
    case nv::Host::Asked::Unsent:
        settle(Heard{std::nullopt, "Neovim could not be asked (" + why + ")", false});
        return;
    case nv::Host::Asked::Outstanding:
        held_ = Held{std::move(what), std::move(answer), std::move(settle)};
        notice(waiting + " -- " + why, false);
        return;
    }
}

/// WHAT AN ANSWER TO A CHANGE SAYS: the module's result or its error -- and when Neovim has
/// ended since, only that it ended, because whatever the change did was in a buffer now gone.
inline Heard NeovimEditorWeave::heard_of(const nv::rpc::Response& r) const {
    if (!running()) {
        return Heard{std::nullopt, host_ != nullptr && !host_->failure().empty() ? host_->failure() : failure_, true};
    }
    if (!r.error.is_nil()) {
        return Heard{std::nullopt, "Neovim answered with an error (" + nv::rpc::error_text(r.error) + ")", false};
    }
    return Heard{r.result, std::string(), false};
}

/// A HELD CHANGE NEOVIM HAS ANSWERED SINCE IS SAID NOW, once (WL-NVIM-13). True when no change
/// is held any longer.
inline bool NeovimEditorWeave::settle_held() {
    if (!held_.has_value()) {
        return true;
    }
    if (!held_->answer->has_value()) {
        return false;
    }
    Held done = std::move(*held_);
    held_.reset();
    done.settle(heard_of(**done.answer));
    resay_ = true;
    return true;
}

// ---- Transfer helpers ---------------------------------------------------------------------

/// IS THIS CELL OF NEOVIM'S SCREEN ON THE PAINTED VISUAL HIGHLIGHT? The cells Neovim marks
/// Visual, and the cursor's own cell while a selection stands (Neovim does not mark it).
inline bool NeovimEditorWeave::on_highlight(std::int64_t row, std::int64_t col) const {
    if (!running() || !host_->ready() || !visual_mode(host_->mode())) {
        return false;
    }
    const nv::Grid& g = host_->grid();
    if (row < 0 || row >= g.rows() || col < 0 || col >= g.columns()) {
        return false;
    }
    return (g.groups_at(row, col) & nv::ui_group::kVisual) != 0 ||
           (row == g.cursor_row() && col == g.cursor_column());
}

/// ONE SCREEN ROW, CELL BY CELL, as this pane's grid holds it -- what a drop tells Neovim it
/// was aimed at, so Neovim can refuse one whose row moved.
inline std::string NeovimEditorWeave::grid_row(std::int64_t row) const {
    std::string out;
    const nv::Grid& g = host_->grid();
    for (std::int64_t c = 0; row >= 0 && row < g.rows() && c < g.columns(); ++c) {
        out += g.at(row, c).text;
    }
    return out;
}

/// THE SELECTION NEOVIM HOLDS NOW, as its own yank would take it -- or why there is none.
inline Snapshot NeovimEditorWeave::snapshot_now() {
    Snapshot s;
    std::string why;
    const std::optional<mp::Value> v = lua_now(nv::lua::kSelection, nv::rpc::params(), kAskMs, why);
    if (!v.has_value()) {
        s.refusal = "Neovim could not be asked for its selection (" + why + ")";
        return s;
    }
    if (const mp::Value* w = v->get("why"); w != nullptr) {
        s.refusal = w->as_str();
        return s;
    }
    const mp::Value::Array& lines = field(*v, "lines").as_array();
    for (std::size_t i = 0; i < lines.size(); ++i) {
        s.text += (i > 0 ? "\n" : "") + lines[i].as_str();
    }
    if (field(*v, "newline").as_bool(false)) {
        s.text += '\n';
    }
    const std::string kind = field(*v, "kind").as_str();
    s.kind = kind == "V" ? st::kLines : kind == "\x16" ? st::kBlock : st::kCharacters;
    s.mode = field(*v, "mode").as_str();
    s.buf = field(*v, "buf").as_int();
    s.tick = field(*v, "tick").as_int();
    s.first_line = field(*v, "first").at(0).as_int();
    s.first_col = field(*v, "first").at(1).as_int();
    s.last_line = field(*v, "after").at(0).as_int();
    s.last_col = field(*v, "after").at(1).as_int();
    s.name = field(*v, "name").as_str();
    s.buftype = field(*v, "buftype").as_str();
    s.fileformat = field(*v, "fileformat").as_str();
    s.modified = field(*v, "modified").as_bool(false);
    s.ok = true;
    return s;
}

inline std::string NeovimEditorWeave::project_root() const {
    return project_known_ ? ws::persist::resolved_against(std::string(), project_dir_) : std::string();
}

/// ASK WORKSHOP TO APPROVE A CARRY OF THIS PAIR FOR THE GESTURE THAT CAUSED IT; the carry
/// follows the approval (`office::begin_pickup`).
inline void NeovimEditorWeave::begin_pickup(const st::Pair& pair, bool drag, std::uint64_t gesture,
                                            std::string label, std::string what, loom::Mail& mail) {
    office::begin_pickup(pair, drag, gesture, label, std::move(what), mail, nve::kEditorOffice, nve::kEditorPane, asked_,
                         pickup_, noticing());
}

/// A COPY OF THE SELECTION NEOVIM HELD (WL-NVIM-10): its text, with where it came from beside it.
// WL-NVIM-10 -- agents/workshop/neovim-transfers.md
inline void NeovimEditorWeave::carry_snapshot(const Snapshot& snap, bool drag, std::uint64_t gesture,
                                              loom::Mail& mail) {
    st::SourceSelection s;
    s.editor = started_words();
    s.path = snap.buftype.empty() && !snap.name.empty() ? spelled(snap.name) : std::string();
    s.project_root = project_root();
    s.kind = snap.kind;
    s.first_line = snap.first_line;
    s.first_column = snap.first_col;
    s.end_line = snap.last_line;
    s.end_column = snap.last_col;
    s.line_ending = snap.fileformat == "dos" ? "CRLF" : "LF";
    s.unsaved = snap.modified;
    s.captured_at_epoch_s = st::clock_now();
    const st::Lines lines = st::neovim_lines(snap.text);
    const std::string name = s.path.empty() ? std::string("[No Name]") : file_name(s.path);
    begin_pickup(st::text_pair(snap.text, s), drag, gesture,
                 name + " " + std::to_string(snap.first_line) + ":" + std::to_string(snap.first_col) + " (" + snap.kind + ")",
                 (lines.ok ? st::amount_words(lines.lines) : std::string("the selection")) + " of " + name, mail);
}

/// THIS FILE'S LOCATION, AT NEOVIM'S CURSOR (WL-NVIM-12).
inline void NeovimEditorWeave::acquire_location(bool drag, std::uint64_t gesture, loom::Mail& mail) {
    std::string why;
    const std::optional<mp::Value> v = lua_now(nv::lua::kLocation, nv::rpc::params(), kAskMs, why);
    if (!v.has_value()) {
        notice("nothing was carried -- Neovim could not say where its cursor is (" + why + ")", true);
        return;
    }
    const std::string name = field(*v, "name").as_str();
    if (!field(*v, "buftype").as_str().empty() || name.empty()) {
        notice("nothing was carried -- Neovim's current buffer names no file", true);
        return;
    }
    st::SourceLocation loc{spelled(name), field(*v, "line").as_int(), field(*v, "col").as_int()};
    st::SourceLocationContext c;
    c.editor = started_words();
    c.project_root = project_root();
    c.relative = st::relative_to(loc.path, c.project_root);
    c.line_text = st::observe_line(field(*v, "text").as_str());
    c.unsaved = field(*v, "modified").as_bool(false);
    c.captured_at_epoch_s = st::clock_now();
    begin_pickup(st::location_pair(loc, c), drag, gesture,
                 file_name(loc.path) + ":" + std::to_string(loc.line) + " (location)",
                 "the location of " + file_name(loc.path) + " at line " + std::to_string(loc.line), mail);
}

inline void NeovimEditorWeave::receive(const loom::Bytes& data, const ws::RowCell& drop, std::int64_t grant,
                                       std::int64_t picture, loom::Mail& mail) {
    const std::string bytes(data.begin(), data.end());
    st::Material m = st::read_material(bytes);
    if (m.kind == st::MaterialKind::Location) {
        open_location(m, mail);
        return;
    }
    if (m.kind == st::MaterialKind::Unsupported) {
        notice("nothing was inserted -- " + m.refusal, true);
        return;
    }
    if (!running() || !host_->ready()) {
        notice("nothing was inserted -- Neovim is not running; open a source to start it", true);
        return;
    }
    if (candidate_.live || !pastes_.empty() || drop_.pending || !settle_held()) {
        notice(std::string("nothing was inserted -- ") +
                   (candidate_.live ? "Neovim is opening " + candidate_.path
                    : drop_.pending ? std::string("a dropped command is still waiting for your choice")
                    : held_.has_value() ? held_->what + " is still waiting for Neovim"
                                        : std::string("a paste is still arriving")) +
                   "; drop again once it has settled",
               true);
        return;
    }
    pump(mail);
    if (!fresh(grant, picture)) {
        notice("nothing was inserted -- Neovim's screen moved under the drop; drop it again", true);
        return;
    }
    if (!drop.shown) {
        notice("nothing was inserted -- drop onto Neovim's text", true);
        return;
    }
    if (drop.row < kChromeRows) {
        notice("nothing was inserted -- drop onto Neovim's text, not the status row", true);
        return;
    }
    std::string why;
    const std::optional<mp::Value> lang =
        lua_now(nv::lua::kLanguage, nv::rpc::params(mp::Value::integer(400)), kAskMs, why);
    if (!lang.has_value()) {
        notice("nothing was inserted -- Neovim could not be asked about its buffer (" + why + ")", true);
        return;
    }
    Aim aim;
    aim.row = drop.row - kChromeRows;
    aim.column = drop.column < 0 ? 0 : drop.column;
    aim.buf = field(*lang, "buf").as_int();
    aim.tick = field(*lang, "tick").as_int();
    aim.row_text = grid_row(aim.row);
    if (m.kind == st::MaterialKind::Text) {
        insert_lines(m.text, aim, Insertion{});
        return;
    }
    // A COMMAND: its Terminal line, or -- in a C++ buffer, by a separate choice -- C++. Neovim's
    // own filetype is the language owner; an extension decides only where Neovim named none.
    const std::string ft = field(*lang, "filetype").as_str();
    const st::CppDocument cpp = ft == "cpp" ? st::CppDocument::Yes
                                : !ft.empty() ? st::CppDocument::No
                                              : st::cpp_document(field(*lang, "name").as_str());
    if (cpp == st::CppDocument::No) {
        insert_command(m, aim);
        return;
    }
    drop_.pending = true;
    drop_.material = std::move(m);
    drop_.aim = aim;
    drop_.menu = ws::pane_menu::Offer(nve::kEditorPane, kDropSubject)
                     .at(drop.row, drop.column)
                     .row(kInsertLine, "Insert its Terminal line")
                     .row(kInsertCpp, cpp == st::CppDocument::Yes ? "Generate C++ that builds it"
                                                                  : "Generate C++ (this .h is C++)")
                     .send(mail, nve::kEditorOffice);
    if (!drop_.menu.pending()) {
        drop_ = Dropped{};
        notice("nothing was inserted -- the choice for the dropped command could not be offered", true);
        return;
    }
    notice("choose how the dropped " + drop_.material.what + " goes in -- nothing is inserted until you do", false);
}

/// TEXT INTO NEOVIM AS DATA (WL-NVIM-11): one undo block where the hand aimed, or Neovim's own
/// refusal with nothing changed -- now, or once Neovim answers a drop it held (WL-NVIM-13).
// WL-NVIM-11, WL-NVIM-13 -- agents/workshop/neovim-transfers.md
inline void NeovimEditorWeave::insert_lines(const std::string& text, const Aim& aim, Insertion said, bool whole) {
    const st::Lines lines = st::neovim_lines(text);
    if (!lines.ok) {
        notice("nothing was inserted -- " + lines.refusal, true);
        return;
    }
    if (said.amount.empty()) {
        said.amount = st::amount_words(lines.lines);
    }
    mp::Value::Array arr;
    for (const std::string& l : lines.lines) {
        arr.push_back(mp::Value::str(l));
    }
    change(nv::lua::kDrop,
           nv::rpc::params(mp::Value::integer(aim.buf), mp::Value::integer(aim.tick), mp::Value::integer(aim.row),
                           mp::Value::integer(aim.column), mp::Value::array(std::move(arr)), mp::Value::str(aim.row_text),
                           mp::Value::boolean(whole)),
           "a drop", "the drop waits for Neovim to take it where you aimed, unless that spot changes first",
           [this, said = std::move(said)](const Heard& h) { settle_insertion(said, h); });
}

/// WHAT NEOVIM DID WITH A DROP, said once, whenever it answered.
inline void NeovimEditorWeave::settle_insertion(const Insertion& said, const Heard& h) {
    if (h.ended) {
        notice(ended_words("a drop", h.why), true);
        return;
    }
    if (!h.result.has_value()) {
        notice("nothing was inserted -- " + h.why, true);
        return;
    }
    if (const mp::Value* refused = h.result->get("why"); refused != nullptr) {
        notice("nothing was inserted -- " + refused->as_str(), true);
        return;
    }
    if (!said.instead.empty()) {
        notice(said.instead, said.alert);
        return;
    }
    const bool replaced = field(*h.result, "replaced").as_bool(false);
    notice(std::string(replaced ? "replaced the Visual selection with " : "inserted ") + said.amount + " at line " +
               std::to_string(field(*h.result, "line").as_int()) + ", byte " +
               std::to_string(field(*h.result, "col").as_int()) + " -- u takes it back; nothing was written" +
               said.after,
           said.alert);
}

inline void NeovimEditorWeave::insert_command(const st::Material& m, const Aim& aim) {
    const st::TerminalLine t = st::terminal_line(*m.command, m.address);
    if (!t.ok) {
        notice("nothing was inserted -- " + m.what + ": " + t.refusal, true);
        return;
    }
    Insertion said;
    said.amount = "the Terminal line for " + m.what;
    said.after = office::terminal_line_after(t, m);
    said.alert = !t.missing.empty();
    insert_lines(t.line, aim, std::move(said));
}

inline void NeovimEditorWeave::insert_cpp(const st::Material& m, const Aim& aim) {
    std::string why;
    const std::optional<mp::Value> lang =
        lua_now(nv::lua::kLanguage, nv::rpc::params(mp::Value::integer(400)), kAskMs, why);
    std::vector<std::string> document;
    if (lang.has_value()) {
        for (const mp::Value& l : field(*lang, "lines").as_array()) {
            document.push_back(l.as_str());
        }
    }
    const st::GeneratedCpp g = st::cpp_value_function(*m.command, document);
    if (!g.ok) {
        notice("no C++ was generated -- " + g.refusal, true);
        return;
    }
    Insertion said;
    said.amount = "C++ for " + m.what;
    said.instead = office::generated_cpp_words(g, m.what, "") + " -- u removes it; nothing was sent, written or built";
    said.alert = !g.holes.empty();
    // GENERATED CODE IS WHOLE LINES, before the line the drop landed on, joining no text.
    insert_lines(st::join_lf(g.lines), aim, std::move(said), true);
}

/// A DROPPED LOCATION (WL-NVIM-12): Workshop approves the gesture, then the managed open.
// WL-NVIM-12 -- agents/workshop/neovim-transfers.md
inline void NeovimEditorWeave::open_location(const st::Material& m, loom::Mail& mail) {
    const std::string refused = office::location_refusal(m.location, locate_);
    if (!refused.empty()) {
        notice(refused, true);
        return;
    }
    if (!settle_held()) {
        notice("nothing was opened -- " + held_->what + " is still waiting for Neovim; drop the location again once it "
               "has settled",
               true);
        return;
    }
    // A LOCATION REOPENS A FILE; IT NEVER CREATES ONE. Neovim edits a path that is not there
    // as a new buffer, so a location whose file is gone is refused here, before anything is
    // asked -- with where it was saved, when that was another root.
    std::error_code ec;
    const std::string path = spelled(m.location.path);
    if (!std::filesystem::is_regular_file(std::filesystem::path(path), ec)) {
        const office::Roots roots = office::roots_of(m.location_context, project_root());
        std::string why =
            "nothing was opened -- " + path + " is not there, and a location reopens a file rather than creating one";
        if (roots.elsewhere) {
            why += " (it was saved under " + roots.saved + ", and this run's project is " + roots.here +
                   "; edit its path in Info to rebind it)";
        }
        notice(why, true);
        return;
    }
    office::ask_location(m, mail, nve::kEditorOffice, nve::kEditorPane, asked_, locate_, noticing());
}

inline void NeovimEditorWeave::settle_location(const SourceOpened& said) {
    Locate l = std::move(locate_);
    locate_ = Locate{};
    const office::Roots roots = office::roots_of(l.ctx, project_root());
    if (!said.accepted) {
        notice(office::open_refused_words(l.loc.path, said.refusal, roots), true);
        return;
    }
    if (doc_path() != l.loc.path) {
        notice("the location's file was shown, but Neovim holds another buffer now -- the cursor was not moved", false);
        return;
    }
    const std::string opened = office::opened_words(shown_path(l.loc.path), roots);
    if (l.loc.line <= 0) {
        notice(opened, false);
        return;
    }
    // THE CURSOR IS A CHANGE LIKE A DROP (WL-NVIM-13): asked with its buffer, its changedtick and
    // the saved line -- whole or cut, as the observation says (`whole_line`) -- which Neovim
    // checks again when it runs it, so a placement it holds lands only on that line unchanged.
    const std::string line = std::to_string(l.loc.line);
    const bool text = l.ctx.has_value() && !l.ctx->line_text.empty();
    change(nv::lua::kLocate,
           nv::rpc::params(mp::Value::integer(doc_.buf), mp::Value::integer(l.same_path ? l.snapshot : shown_tick_),
                           mp::Value::integer(l.loc.line), mp::Value::integer(l.loc.column),
                           text ? mp::Value::str(l.ctx->line_text) : mp::Value::nil(),
                           mp::Value::boolean(text && st::whole_line(l.ctx->line_text))),
           "a location's cursor", opened + " -- the cursor waits for Neovim to put it on line " + line +
                                      ", unless the file changes first",
           [this, opened, line](const Heard& h) {
               if (h.ended) {
                   notice(ended_words("a location's cursor", h.why), true);
               } else if (!h.result.has_value()) {
                   notice(opened + " -- the cursor was not moved: " + h.why, false);
               } else if (!field(*h.result, "placed").as_bool(false)) {
                   const mp::Value& why = field(*h.result, "why");
                   notice(opened + " -- " +
                              (why.is_nil() ? std::string("the cursor was not moved")
                                            : why.as_str() + ", so the cursor was not moved"),
                          false);
               } else {
                   notice(opened + " at line " + line, false);
               }
           });
}

} // namespace

#endif // ZENGINE_NEOVIM_EDITOR_TRANSFERS_HPP
