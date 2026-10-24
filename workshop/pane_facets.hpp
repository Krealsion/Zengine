// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_FACETS_HPP
#define ZENGINE_WORKSHOP_PANE_FACETS_HPP

// The conversations every pane office holds with Workshop and the graphics window that shows its
// area, as facets composed with `zengine::With` (activation/activation.hpp): its offer and manual
// (`PaneFacet`), its rows (`PaneRowsFacet`) or its one drawing (`PaneDrawingFacet`), drawn in its
// area, and the presses on them, its asks (`AskFacet`), declared actions (`PaneActionFacet`) and
// menu (`PaneMenuFacet`). The shapes they carry leave the weave's own lists; what each calls on the
// weave, and when, is workshop/docs/workshop-panes.md, "The pane facets".

#include "activation/activation.hpp"
#include "component/control_strip.hpp"
#include "component/list_window.hpp"
#include "component/row_map.hpp"
#include "graphics/canvas.hpp"
#include "graphics/vocabulary.hpp"
#include "pane_document.hpp"
#include "pane_drawing.hpp"
#include "pane_menu.hpp"
#include "pane_operation.hpp"
#include "pane_parts.hpp"
#include "pane_rows.hpp"
#include "pane_rows_block.hpp"
#include "pane_text.hpp"
#include "pane_vocabulary.hpp"

#include <zen/weave.hpp>
#include <zen/weave/ask_book.hpp>
#include <zen/weave/dispatch_refusal.hpp>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// THE PANE ITS OFFICE OFFERS, ITS ROOM AND ITS MANUAL: the activation the office lives under,
/// the offer (`Offer`, from the weave's `offering()`) on each one and on Workshop's catalog
/// request, the manual from `Page`, and the room the graphics window that shows its area gives it,
/// with the rows and columns the pane composes for. The weave draws in `on_room`.
template <const char* const& Pane, const char* const& Office, const manual::Manual& Page,
          class Offer = v2::PaneOffered>
class PaneFacet {
public:
    using Accept = loom::Accept<loom::Activated, PaneCatalogRequested, PaneDocumentRequested>;
    using Emit = loom::Emit<Offer, PaneDocumentShown>;

    /// An accepted activation offers the pane, or is the weave's `on_activated`, which announces.
    template <class Self>
    void on(Self& self, const loom::Activated& activated, loom::Mail& mail) {
        if (!activation_.accept(mail, activated)) {
            return;
        }
        if constexpr (requires { self.on_activated(mail); }) {
            self.on_activated(mail);
        } else {
            announce(self, mail);
        }
    }

    template <class Self>
    void on(Self& self, const PaneCatalogRequested&, loom::Mail& mail) {
        if (mail.authored_from_role(pane_menu::kWorkshopRole)) {
            announce(self, mail);
        }
    }

    template <class Self>
    void on(Self&, const PaneDocumentRequested& ask, loom::Mail& mail) {
        (void)mail.answer(pane_document(ask, Pane, Page));
    }

    /// THE OFFER, from the office to Workshop, then the weave's own `offered` (its rows, its
    /// settings), when it has one.
    template <class Self>
    void announce(Self& self, loom::Mail& mail) {
        (void)mail.as_role(Office).send_to_role(pane_menu::kWorkshopRole, self.offering());
        if constexpr (requires { self.offered(mail); }) {
            self.offered(mail);
        }
    }

    /// Whether this delivery is Workshop's own word about this office's pane.
    static bool from_workshop(const loom::Mail& mail, const std::string& pane) {
        return pane == Pane && mail.authored_from_role(pane_menu::kWorkshopRole);
    }

    const ActivationCursor& activation() const noexcept { return activation_; }
    /// A graphics window gave the pane's area a room in this life.
    bool granted() const noexcept { return granted_; }
    /// The rows and columns the pane composes for: the window room's, a row a line of the window's
    /// text and a character an advance of it.
    std::int64_t rows() const noexcept { return rows_; }
    std::int64_t columns() const noexcept { return columns_; }

protected:
    /// A graphics window's room, in its rows and columns.
    void window_room(std::int64_t rows, std::int64_t columns) {
        rows_ = rows;
        columns_ = columns;
        granted_ = true;
    }

private:
    ActivationCursor activation_;
    bool granted_ = false;
    std::int64_t rows_ = 0, columns_ = 0;
};

/// ONE FIRST-BUTTON PRESS ON A SHOWN ROW OF THE ROWS LAST DRAWN, as the row and column it landed
/// on, and whether the keys were already reaching the pane when it was made, as the last
/// `PaneKeys` Workshop sent says. A press a window asked as a part's action (`window`), in an area
/// placed deaf to the pointer, never says so, since Workshop saw no press there. `moved` says the
/// window drew another element under the hand than the rows last drawn show there
/// (`WindowDrawing::moved`): the pane acts on nothing and says so in its own words. `told` says the
/// window named the drawing the press landed on, by the pane's number for it, `drawing`.
struct RowPress {
    std::int64_t row = 0, column = 0;
    bool keys_went_here = false;
    bool window = false;
    bool moved = false;
    bool told = false;
    std::int64_t drawing = 0;
};

/// A PANE THAT SAYS ROWS: `PaneFacet`, the rows being composed, what each row and run of columns
/// means (`component::RowMap<Meaning>`), the caret, and the presses on them. Its rows go to the
/// graphics window that shows the area named `Office` as a drawing, the caret in it
/// (`rows_drawing`). A press is read against the rows last drawn there; a third-button press that
/// means nothing to the pane is passed back to Workshop.
template <const char* const& Pane, const char* const& Office, const manual::Manual& Page,
          class Meaning, class Offer = v2::PaneOffered>
class PaneRowsFacet : public PaneFacet<Pane, Office, Page, Offer> {
    using Room = PaneFacet<Pane, Office, Page, Offer>;

public:
    using Accept = loom::Accept<loom::Activated, PaneCatalogRequested, PaneDocumentRequested,
                                graphics::AreaRoom, graphics::DrawnRefused,
                                graphics::ActionRequested, graphics::AreaButtonChanged,
                                graphics::AreaPointerMoved, graphics::AreaWheelTurned, PaneKeys>;
    using Emit = loom::Emit<Offer, PaneDocumentShown, PanePassRequested, graphics::Drawn,
                            graphics::RoomAsked>;
    using Room::on;

    /// The room, from the window that shows the area, as that window's office. When the window,
    /// its incarnation, its size or its measure changed or the area is empty, the window holds none
    /// of the pane's rows, so the weave's `on_room` says them, and the window is sent them whole:
    /// a row a line of the window's text, a character an advance of it.
    template <class Self>
    void on(Self& self, const graphics::AreaRoom& r, loom::Mail& mail) {
        if (window_.room(r, mail)) {
            const RowsRoom room = window_.rows_room();
            this->window_room(room.rows(), room.columns());
            self.on_room(mail);
        }
    }

    /// Rows the window refused leave what it shows as it was: after a refused change the rows last
    /// sent go again whole at once, and after a refused whole drawing nothing goes until the rows
    /// change, and then whole (`WindowDrawing::refused`).
    template <class Self>
    void on(Self&, const graphics::DrawnRefused& refused, loom::Mail& mail) {
        (void)window_.refused(refused, mail);
    }

    /// A PRESS ON A PART IN THE WINDOW, asked by the window that shows the area as its office: for a
    /// part of the rows last sent there, the weave's `press` at the part's place -- a cell only it
    /// holds, else the first a press reaches it at -- else
    /// the weave's `on_action` for an id its `answers(id)` admits. Any other is no act and spends
    /// nothing.
    template <class Self>
    void on(Self& self, const graphics::ActionRequested& asked, loom::Mail& mail) {
        if (!window_.from_window(asked.area, mail)) {
            return;
        }
        const auto place = window_places_.find(asked.action);
        if (place != window_places_.end()) {
            const RowPress press{place->second.row, place->second.column, false, true};
            if constexpr (requires { self.press(press, mail); }) {
                self.press(press, mail);
            }
            return;
        }
        if constexpr (requires {
                          { self.answers(asked.action) } -> std::convertible_to<bool>;
                          self.on_action(asked.action, mail);
                      }) {
            if (self.answers(asked.action)) {
                self.on_action(asked.action, mail);
            }
        }
    }

    /// THE POINTER IN THE WINDOW, given by the window that shows the area as its office: a press
    /// of the first button on a shown cell of the rows last sent there is the weave's `press`, read
    /// against whether the keys were already the pane's (`PaneKeys`); one of the third is its
    /// `right_press` on what the cell means, else passed back under the window's number for the
    /// press, so Workshop's own pane menu opens where it was made. A first- or third-button press
    /// the window landed on another element than those rows show there is the weave's `press`,
    /// `moved`.
    template <class Self>
    void on([[maybe_unused]] Self& self, const graphics::AreaButtonChanged& b, loom::Mail& mail) {
        const std::optional<WindowPointer> p =
            window_.pointer(b, mail, window_rows_, this->columns());
        if (!p || p->phase != canvas_pointer::kPress) {
            return;
        }
        const RowPress press{p->at.row, p->at.column, keys_here_, false, p->moved, p->told,
                             p->drawing};
        if (p->moved && (p->button == 1 || p->button == 3)) {
            keys_here_ = keys_here_ || p->button == 1;
            if constexpr (requires { self.press(press, mail); }) {
                self.press(press, mail);
            }
            return;
        }
        if (p->button == 1) {
            // A PRIMARY PRESS OF ITS OWN MOVES THE KEYS HERE (WL-FOCUS-03), so the next press is
            // made with them here, whether or not Workshop's word on this one has come yet.
            keys_here_ = true;
            if constexpr (requires { self.press(press, mail); }) {
                if (p->at.shown) {
                    self.press(press, mail);
                }
            }
            return;
        }
        if (p->button != 3) {
            return;
        }
        const Meaning* meant = p->at.shown ? block_.map().at(p->at.row, p->at.column) : nullptr;
        if constexpr (requires { self.right_press(*meant, press, mail); }) {
            if (meant != nullptr && self.right_press(*meant, press, mail)) {
                return;
            }
        }
        pass_back(mail);
    }

    /// A move and a release in the window mean nothing to rows; the wheel is the weave's `wheel`.
    template <class Self>
    void on(Self&, const graphics::AreaPointerMoved&, loom::Mail&) {}
    template <class Self>
    void on([[maybe_unused]] Self& self, const graphics::AreaWheelTurned& w,
            [[maybe_unused]] loom::Mail& mail) {
        if constexpr (requires { self.wheel(w.dy, mail); }) {
            if (window_.pointer(w, mail, window_rows_, this->columns())) {
                self.wheel(w.dy, mail);
            }
        }
    }

    /// Whether the keys reach the pane now, as Workshop last said.
    template <class Self>
    void on(Self&, const PaneKeys& keys, loom::Mail& mail) {
        if (Room::from_workshop(mail, keys.pane)) {
            keys_here_ = keys.here;
        }
    }

    /// The room asked once in each life while no window has said it, after the first message the
    /// life delivers: a pane reloaded into a window says its rows there again.
    template <class Self>
    void after_delivery(Self&, loom::Mail& mail) {
        if (!asked_ && !window_.shown()) {
            asked_ = true;
            (void)mail.as_role(Office).send_to_role(graphics::kGraphicsRole,
                                                    graphics::RoomAsked{window_.area()});
        }
    }

    /// THAT PRESS WAS NOT THE PANE'S: Workshop's own pane menu opens where it was made.
    void pass_back(loom::Mail& mail) { (void)pane_menu::pass_back(mail, Office, Pane); }

    /// BEGIN A PICTURE: no rows, no meanings, no caret, in the room the window gave. What the last
    /// picture meant is kept aside until `publish` compares the two. The row tools are a
    /// `RowsBlock`'s, the area's whole room its room (workshop/pane_rows_block.hpp).
    void begin() { block_.begin(this->rows(), this->columns()); }
    bool push(TextRow row, std::optional<Meaning> meaning = std::nullopt) {
        return block_.push(std::move(row), std::move(meaning));
    }
    std::int64_t next_row() const noexcept { return block_.next_row(); }
    std::vector<TextRow>& composing() noexcept { return block_.composing(); }
    component::RowMap<Meaning>& map() noexcept { return block_.map(); }
    const component::RowMap<Meaning>& map() const noexcept { return block_.map(); }
    RowsCaret& caret() noexcept { return block_.caret(); }
    template <class Row>
    std::int64_t strip_rows(const std::vector<Row>& controls,
                            std::int64_t max_rows = component::kMaxStripRows) const {
        return block_.strip_rows(controls, max_rows);
    }
    template <class Row, class FitRow, class MeaningOf>
    void push_controls(const std::vector<Row>& controls, FitRow&& fit_row, MeaningOf&& meaning_of,
                       std::int64_t max_rows = component::kMaxStripRows) {
        block_.push_controls(controls, std::forward<FitRow>(fit_row),
                             std::forward<MeaningOf>(meaning_of), max_rows);
    }
    template <class FitRow, class EmitEntry>
    std::int64_t push_window(const component::ListWindow& win, FitRow&& fit_row,
                             EmitEntry&& emit_entry) {
        return block_.push_window(win, std::forward<FitRow>(fit_row),
                                  std::forward<EmitEntry>(emit_entry));
    }
    void hold() { block_.hold(); }

    /// THE ROWS COMPOSED, each part named `name_of(meaning)` and available as
    /// `available_of(meaning)` says, the caret in them when `with_caret`.
    template <class NameOf, class AvailableOf>
    void publish(loom::Mail& mail, NameOf&& name_of, AvailableOf&& available_of,
                 bool with_caret = true) {
        ComposedRows rows = block_.take(name_of, available_of);
        send(mail, std::move(rows.rows), std::move(rows.parts), with_caret);
    }

    /// ...or rows composed elsewhere, with their parts: to the window that shows the area as what
    /// changed there, the caret in them when `with_caret`.
    void send(loom::Mail& mail, std::vector<TextRow> rows, std::vector<RowPart> parts,
              bool with_caret = true) {
        ++published_;
        if (window_.shown()) {
            RowsDrawing drawn = rows_drawing(rows, parts, window_.rows_room(), this->columns(),
                                             with_caret ? block_.caret() : RowsCaret{});
            // NUMBERED BY WHAT ITS ROWS MEAN (`component::RowMap`), so a press on a row no part
            // names is read against the meaning it was drawn under.
            drawn.sketch.number(block_.map().picture());
            window_places_ = std::move(drawn.places);
            window_rows_ = static_cast<std::int64_t>(rows.size());
            window_.draw(drawn.sketch, mail);
        }
    }

    /// How many pictures the pane has published, so a handler can tell whether the act it ran
    /// already said its rows.
    std::uint64_t published() const noexcept { return published_; }

    /// THE WEAVER HAS ACTED, SO THE LAST ACT'S ANSWER IS SPENT: `notice` is cleared before `act()`
    /// runs, and when one stood and the act published no picture of its own -- its answer still on
    /// its way, or it meant nothing -- `say()` says the rows once without it.
    template <class Act, class Say>
    void spend_notice(std::string& notice, Act&& act, Say&& say) {
        const bool spent = !notice.empty();
        const std::uint64_t before = published_;
        notice.clear();
        std::forward<Act>(act)();
        if (spent && published_ == before) {
            std::forward<Say>(say)();
        }
    }

    /// The office a paste is asked of: the window showing the area, none while no window does.
    std::string clipboard_office() const { return window_.clipboard_office(); }
    /// The area the pane draws its rows in, and where a point of it falls on the rows last drawn
    /// there: a drop's place.
    const std::string& area() const noexcept { return window_.area(); }
    RowCell cell_at(std::int64_t x, std::int64_t y) const {
        return window_cell(x, y, window_rows_, this->columns(), window_.text());
    }
    /// Whether a drop at `x`, `y` of the area names another element than the rows last drawn there
    /// show, by the window's word of what it drew under the hand (`WindowDrawing::moved`).
    bool moved(const std::string& under, const std::string& part, std::int64_t drawing,
               std::int64_t x, std::int64_t y) const {
        return window_.moved(under, part, drawing, x, y);
    }

private:
    RowsBlock<Meaning> block_;
    std::uint64_t published_ = 0;
    WindowDrawing window_{Office, pane_area(Office, Pane)};
    std::map<std::string, RowCell> window_places_; ///< where each part of the rows last sent lands
    std::int64_t window_rows_ = 0;                 ///< how many rows were last sent there
    bool keys_here_ = false;                       ///< the keys reach the pane, as Workshop last said
    bool asked_ = false;                           ///< the room asked in this life
};

/// A PANE THAT DRAWS ONE PICTURE: `PaneFacet`, and the weave's `picture(graphics::Sketch&)` shown
/// in the graphics window that shows the area named by its office, as what changed there. Each
/// element record is a part named by its action, and a press anywhere on the element's face asks
/// that action of the weave's `on_action(id, mail)`: the owner never reads a place.
template <const char* const& Pane, const char* const& Office, const manual::Manual& Page,
          class Offer = v2::PaneOffered>
class PaneDrawingFacet : public PaneFacet<Pane, Office, Page, Offer> {
    using Room = PaneFacet<Pane, Office, Page, Offer>;

public:
    using Accept = loom::Accept<loom::Activated, PaneCatalogRequested, PaneDocumentRequested,
                                graphics::AreaRoom, graphics::DrawnRefused,
                                graphics::ActionRequested, graphics::AreaButtonChanged,
                                graphics::AreaPointerMoved, graphics::AreaWheelTurned>;
    using Emit = loom::Emit<Offer, PaneDocumentShown, PanePassRequested, graphics::Drawn,
                            graphics::RoomAsked>;
    using Room::on;

    /// The room, from the window that shows the area, as that window's office. When the window,
    /// its incarnation, its size or its measure changed or the area is empty, the window holds none
    /// of the pane's drawing, so the weave's `on_room` draws, and the window is sent the picture
    /// whole.
    template <class Self>
    void on(Self& self, const graphics::AreaRoom& r, loom::Mail& mail) {
        if (window_.room(r, mail)) {
            const RowsRoom room = window_.rows_room();
            this->window_room(room.rows(), room.columns());
            self.on_room(mail);
        }
    }

    /// A drawing the window refused leaves what it shows as it was: after a refused change the
    /// picture last sent goes again whole at once, and after a refused whole picture nothing goes
    /// until the picture changes, and then whole (`WindowDrawing::refused`).
    template <class Self>
    void on(Self&, const graphics::DrawnRefused& refused, loom::Mail& mail) {
        (void)window_.refused(refused, mail);
    }

    /// A PRESS ON A PART IN THE WINDOW, asked by the window that shows the area as its office: the
    /// weave's `on_action` for an action the picture last shown there names, or one the weave's
    /// `answers(id)` admits where it has one. Any other is no act and spends nothing.
    template <class Self>
    void on(Self& self, const graphics::ActionRequested& asked, loom::Mail& mail) {
        if (!window_.from_window(asked.area, mail)) {
            return;
        }
        bool admitted = window_actions_.count(asked.action) != 0;
        if constexpr (requires {
                          { self.answers(asked.action) } -> std::convertible_to<bool>;
                      }) {
            admitted = admitted || self.answers(asked.action);
        }
        if (admitted) {
            act(self, asked.action, mail);
        }
    }

    /// THE POINTER IN THE WINDOW, given by the window that shows the area as its office: the first
    /// button pressed anywhere on a part's face in the picture last sent there asks the part's
    /// action (`graphics::part_at`); one the window drew on another element than that picture
    /// shows there acts on nothing, and is the weave's `fenced` where it has one, to say so; the
    /// third is passed back under the window's number for the press, so Workshop's own pane menu
    /// opens where it was made; the wheel is the weave's `wheel`. A move and a release mean
    /// nothing.
    template <class Self>
    void on(Self& self, const graphics::AreaButtonChanged& b, loom::Mail& mail) {
        const std::optional<WindowPointer> p = window_.pointer(b, mail, 0, 0);
        if (!p || p->phase != canvas_pointer::kPress) {
            return;
        }
        if (p->button == 1 && p->moved) {
            if constexpr (requires { self.fenced(mail); }) {
                self.fenced(mail);
            }
            return;
        }
        if (p->button == 3) {
            pass_back(mail);
            return;
        }
        const std::string action = p->button == 1 ? window_.part_at(p->x, p->y) : std::string();
        if (!action.empty()) {
            act(self, action, mail);
        }
    }
    template <class Self>
    void on(Self&, const graphics::AreaPointerMoved&, loom::Mail&) {}
    template <class Self>
    void on([[maybe_unused]] Self& self, const graphics::AreaWheelTurned& w,
            [[maybe_unused]] loom::Mail& mail) {
        if constexpr (requires { self.wheel(w.dy, mail); }) {
            if (window_.pointer(w, mail, 0, 0)) {
                self.wheel(w.dy, mail);
            }
        }
    }

    /// The room asked once in each life while no window has said it, after the first message the
    /// life delivers: a pane reloaded into a window draws there again.
    template <class Self>
    void after_delivery(Self&, loom::Mail& mail) {
        if (!asked_ && !window_.shown()) {
            asked_ = true;
            (void)mail.as_role(Office).send_to_role(graphics::kGraphicsRole,
                                                    graphics::RoomAsked{window_.area()});
        }
    }

    /// THAT PRESS WAS NOT THE PANE'S: Workshop's own pane menu opens where it was made.
    void pass_back(loom::Mail& mail) { (void)pane_menu::pass_back(mail, Office, Pane); }

    /// The office a paste is asked of: the window showing the area, none while no window does.
    std::string clipboard_office() const { return window_.clipboard_office(); }

    /// How the picture being drawn sets its text: the window's measure for the window that shows
    /// the area. The weave's `picture` stands its rows a line apart.
    const graphics::TextMeasure& text() const noexcept { return text_; }

    /// SHOW THE PICTURE, once the pane holds a room: the weave's `picture` drawn in the window's
    /// measure over its room's ground, sent to the window that shows the area as what changed there
    /// (nothing when nothing did).
    template <class Self>
    void show(Self& self, loom::Mail& mail) {
        if (!this->granted() || this->rows() <= 0 || this->columns() <= 0 || !window_.shown()) {
            return;
        }
        text_ = window_.text();
        graphics::Sketch sketch;
        // THE ROOM'S GROUND FIRST, all of it, so nothing behind the area shows through.
        sketch.fill_rect("room", 0, 0, window_.width(), window_.height(),
                         graphics::by_role(graphics::role::kGround));
        self.picture(sketch);
        window_actions_.clear();
        for (const graphics::ElementRecord& record : sketch.records()) {
            if (!record.action.empty()) {
                window_actions_.insert(record.action);
            }
        }
        window_.draw(sketch, mail);
    }

private:
    template <class Self>
    static void act(Self& self, const std::string& id, loom::Mail& mail) {
        if constexpr (requires { self.on_action(id, mail); }) {
            self.on_action(id, mail);
        }
    }

    WindowDrawing window_{Office, pane_area(Office, Pane)};
    std::set<std::string> window_actions_; ///< the actions the picture last sent the window names
    graphics::TextMeasure text_;           ///< how the picture being drawn sets its text
    bool asked_ = false;                   ///< the room asked in this life
};

/// ONE OF THE PANE'S DECLARED ACTIONS, ASKED FOR BY ID: Workshop resolved the keystroke against
/// the effective keymap, so what arrives is the id. Only Workshop's word about this office's pane
/// counts, and only an id the weave's public `answers(id)` admits, where it has one: an id the
/// pane does not answer to is no act and spends nothing. The weave's `on_action(id, mail)` acts.
template <const char* const& Pane, const char* const& Office>
class PaneActionFacet {
public:
    using Accept = loom::Accept<PaneActionRequested>;
    using Emit = loom::Emit<v3::PaneActions>;

    template <class Self>
    void on(Self& self, const PaneActionRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(pane_menu::kWorkshopRole) || asked.pane != Pane) {
            return;
        }
        if constexpr (requires {
                          { self.answers(asked.id) } -> std::convertible_to<bool>;
                      }) {
            if (!self.answers(asked.id)) {
                return;
            }
        }
        self.on_action(asked.id, mail);
    }

    /// THE ROWS THE PANE ANSWERS TO, replacing the last it declared: those of `mode` where the pane
    /// has modes, beside what each other mode's rows may cause.
    void declare(loom::Mail& mail, std::vector<v3::PaneActionRow> rows, std::string mode = {},
                 std::vector<ModeCauses> elsewhere = {}) const {
        (void)mail.as_role(Office).send_to_role(
            pane_menu::kWorkshopRole,
            v3::PaneActions{Pane, std::move(mode), std::move(rows), std::move(elsewhere)});
    }
};

/// THE PANE'S OWN MENU: the one this image asked the presenter for and has not heard answered. An
/// answer settles it only as that ask's (`pane_menu::Asked::take`), and a chosen row goes to the
/// weave's `on_menu(id, subject, mail)`. Image-local: a successor asked nothing, so its
/// predecessor's menus act on nothing.
template <const char* const& Pane, const char* const& Office>
class PaneMenuFacet {
public:
    using Accept = loom::Accept<PaneMenuAnswered>;
    using Emit = loom::Emit<PaneMenuRequested>;

    template <class Self>
    void on(Self& self, const PaneMenuAnswered& answer, loom::Mail& mail) {
        const std::string id = asked_.take(mail, answer);
        if (!id.empty()) {
            self.on_menu(id, answer.subject, mail);
        }
    }

    bool pending() const noexcept { return asked_.pending(); }
    void clear() noexcept { asked_.clear(); }
    pane_menu::Asked& asked() noexcept { return asked_; }
    const pane_menu::Asked& asked() const noexcept { return asked_; }

    /// A menu about `subject`, to fill and send.
    pane_menu::Offer offer(std::string subject) const {
        return pane_menu::Offer(Pane, std::move(subject));
    }
    /// SEND `menu` AS THE PANE'S OFFICE, and wait on it in place of any earlier one...
    void send(loom::Mail& mail, const pane_menu::Offer& menu) { asked_ = menu.send(mail, Office); }
    /// ...or continuing the gesture numbered `correlation`.
    void send_continuing(loom::Mail& mail, const pane_menu::Offer& menu,
                         std::uint64_t correlation) {
        asked_ = menu.continuing(mail, Office, correlation);
    }

private:
    pane_menu::Asked asked_;
};

/// THE ASKS A PANE STILL WAITS ON, in one bounded table numbered from its one correlation
/// sequence (`loom::AskBook`), each holding the pane's record of the act, `Act`. An approval asks
/// Workshop whether a gesture's actor may do an act, at most `Approvals` at once; an ask is any
/// other request the pane expects one answer to, or a send whose refusal it must hear. Workshop's
/// answer goes to the weave's `on_approval`, Loom's refusal of exactly that send to its
/// `on_refused`. Image-local: a successor asked nothing, so no earlier answer settles anything.
template <const char* const& Pane, const char* const& Office, class Act,
          std::size_t Approvals = 8, std::size_t Capacity = 32>
class AskFacet {
    static_assert(Approvals > 0 && Approvals <= Capacity, "AskFacet: the approvals fit the table");

public:
    using Accept = loom::Accept<PaneOperationAnswered, loom::DispatchRefused>;
    using Emit = loom::Emit<v2::PaneOperationRequested>;

    /// What became of an ask: held until it is answered, refused because the table (or its share
    /// for approvals) is full, or refused because the send queued nothing. Only a held one waits.
    enum class Asked { kHeld, kFull, kUnqueued };

    static constexpr std::size_t kApprovals = Approvals;

    /// ASK WORKSHOP WHETHER THE ACTOR OF `gesture` MAY DO AN ACT OF `classes` -- `subject` is what
    /// a class `open` opens -- and hold `act` until it answers. One approval stands for the act's
    /// every later beat under that gesture.
    Asked approve(loom::Mail& mail, Act act, std::uint64_t gesture,
                  std::vector<std::string> classes, std::string subject = {}) {
        if (approvals() >= Approvals) {
            return Asked::kFull;
        }
        v2::PaneOperationRequested asked;
        asked.pane = Pane;
        asked.gesture = static_cast<std::int64_t>(gesture);
        asked.classes = std::move(classes);
        asked.subject = std::move(subject);
        return ask(mail, pane_menu::kWorkshopRole, asked, std::move(act), /*approval=*/true);
    }

    /// SEND `request` TO THE OFFICE `role`, as the pane's office, under a fresh correlation, and
    /// hold `act` until its answer or its refusal.
    template <class Request>
    Asked ask(loom::Mail& mail, const char* role, const Request& request, Act act,
              bool approval = false) {
        const loom::AskOpened opened =
            book_.open_to_role(role, Request::zen_name, Request::zen_version);
        if (!opened) {
            return Asked::kFull;
        }
        const loom::Ticket attempt =
            mail.as_role(Office).send_to_role(role, request, opened.correlation);
        return keep(opened, attempt, std::move(act), approval);
    }

    /// HOLD AN ASK THE PANE SENDS ITSELF, of `shape` at `version` to `role`: `send(correlation)`
    /// sends it and returns its ticket.
    template <class Send>
    Asked ask_with(const char* role, std::string shape, std::uint32_t version, Act act,
                   Send&& send) {
        const loom::AskOpened opened = book_.open_to_role(role, std::move(shape), version);
        if (!opened) {
            return Asked::kFull;
        }
        return keep(opened, send(opened.correlation), std::move(act), false);
    }

    /// A number from the pane's one sequence, for a send that is not an ask.
    std::uint64_t mint() { return book_.mint_correlation(); }

    /// THE ASK THIS ANSWER SETTLES, taken from the table: Loom's answer, under the correlation of
    /// an ask held here, whose act `which` accepts. None for a stale, duplicate or forged answer,
    /// or one about another ask, which then stays held.
    template <class Which>
    std::optional<Act> answered(const loom::Mail& mail, Which&& which) {
        if (!mail.answers_ask()) {
            return std::nullopt;
        }
        const loom::PendingAsk* held = book_.match(mail.correlation(), mail.sender());
        const auto at = held == nullptr ? acts_.end() : find_id(held->id);
        if (at == acts_.end() || !which(at->act)) {
            return std::nullopt;
        }
        return take(at);
    }

    /// Is an ask whose act `which` accepts still held?
    template <class Which>
    bool holds(Which&& which) const {
        return find(which) != nullptr;
    }
    /// The first held act `which` accepts, or nullptr.
    template <class Which>
    const Act* find(Which&& which) const {
        for (const Entry& e : acts_) {
            if (which(e.act)) {
                return &e.act;
            }
        }
        return nullptr;
    }
    /// How many held acts `which` accepts.
    template <class Which>
    std::size_t count(Which&& which) const {
        return static_cast<std::size_t>(std::count_if(
            acts_.begin(), acts_.end(), [&](const Entry& e) { return which(e.act); }));
    }
    /// STOP WAITING ON EVERY ASK WHOSE ACT `which` ACCEPTS, locally: the far end is told nothing,
    /// and an answer that still arrives settles nothing. Returns how many were forgotten.
    template <class Which>
    std::size_t forget(Which&& which) {
        std::size_t gone = 0;
        for (auto at = acts_.begin(); at != acts_.end();) {
            if (which(at->act)) {
                (void)book_.forget(at->id);
                at = acts_.erase(at);
                ++gone;
            } else {
                ++at;
            }
        }
        return gone;
    }
    /// How many approvals are held.
    std::size_t approvals() const {
        return static_cast<std::size_t>(
            std::count_if(acts_.begin(), acts_.end(), [](const Entry& e) { return e.approval; }));
    }

    /// WORKSHOP'S WORD ON AN ACT IT WAS ASKED ABOUT: Loom's answer, under the correlation of an
    /// ask of `PaneOperationRequested` held here, settles exactly that ask.
    template <class Self>
    void on(Self& self, const PaneOperationAnswered& answer, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return;
        }
        const loom::PendingAsk* held = book_.match(mail.correlation(), mail.sender());
        const auto at = held == nullptr ? acts_.end() : find_id(held->id);
        if (at == acts_.end() || held->shape != PaneOperationRequested::zen_name) {
            return;
        }
        self.on_approval(take(at), answer, mail);
    }

    /// LOOM'S WORD THAT ONE OF THE PANE'S SENDS WAS REFUSED BEFORE ANY HANDLER RAN. Provenance
    /// first, then the exact attempt, office and shape against the asks held, and only the matched
    /// one is taken. A forged, stale, duplicate or unmatched notice settles nothing; delivered
    /// silence is not a refusal, and the ask stays held.
    template <class Self>
    void on(Self& self, const loom::DispatchRefused& refused, loom::Mail& mail) {
        if (!mail.dispatch_refused()) {
            return;
        }
        const loom::Ticket attempt = refused.refused_attempt();
        const loom::PendingAsk* held =
            attempt.valid() ? book_.match_attempt(attempt.seq) : nullptr;
        if (held == nullptr || held->role != refused.role || !refused.target.empty() ||
            held->shape != refused.shape ||
            static_cast<std::int64_t>(held->version) != refused.version) {
            return;
        }
        const auto at = find_id(held->id);
        if (at != acts_.end()) {
            self.on_refused(take(at), refused, mail);
        }
    }

private:
    struct Entry {
        std::uint64_t id = 0;
        bool approval = false;
        Act act;
    };

    Asked keep(const loom::AskOpened& opened, loom::Ticket attempt, Act act, bool approval) {
        if (!attempt.valid() || !book_.bind_attempt(opened.id, attempt.seq)) {
            (void)book_.forget(opened.id);
            return Asked::kUnqueued;
        }
        acts_.push_back(Entry{opened.id, approval, std::move(act)});
        return Asked::kHeld;
    }

    typename std::vector<Entry>::iterator find_id(std::uint64_t id) {
        return std::find_if(acts_.begin(), acts_.end(),
                            [id](const Entry& e) { return e.id == id; });
    }

    Act take(typename std::vector<Entry>::iterator at) {
        (void)book_.forget(at->id);
        Act act = std::move(at->act);
        acts_.erase(at);
        return act;
    }

    loom::AskBook book_{Capacity};
    std::vector<Entry> acts_;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_FACETS_HPP
