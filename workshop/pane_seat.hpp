// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_SEAT_HPP
#define ZENGINE_WORKSHOP_PANE_SEAT_HPP

// What an office offering several panes keeps for each one, and the conversation it holds with
// Workshop and the graphics windows about all of them: the window that shows one pane's area and
// the rows drawn there (`PaneCanvasSeat`), and the office's activation, catalog answer, manual and
// window rooms (`MultiPaneFacet`, composed with `zengine::With`, activation/activation.hpp). An
// office of one pane composes workshop/pane_facets.hpp instead.

#include "activation/activation.hpp"
#include "graphics/canvas.hpp"
#include "graphics/vocabulary.hpp"
#include "pane_document.hpp"
#include "pane_drawing.hpp"
#include "pane_menu.hpp"
#include "pane_rows.hpp"
#include "pane_vocabulary.hpp"

#include <zen/weave.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// ONE PANE'S AREA, AS AN OFFICE OF SEVERAL PANES KEEPS IT: the graphics window that shows it, the
/// rows and columns the pane composes for there -- a row a line of the window's text and a
/// character an advance of it -- and the rows last drawn there with their parts and caret. Named
/// by its pane and the office it speaks as. Transient, as every room is.
class PaneCanvasSeat {
public:
    PaneCanvasSeat() = default;
    PaneCanvasSeat(std::string pane, std::string office)
        : pane_(std::move(pane)), office_(std::move(office)),
          window_(office_, area_of(office_, pane_)) {}

    /// THE AREA A GRAPHICS WINDOW SHOWS A PANE IN: its office and its key, as
    /// `zengine.desktop.launcher`.
    static std::string area_of(const std::string& office, const std::string& pane) {
        return office + "." + pane;
    }

    const std::string& pane() const noexcept { return pane_; }
    const std::string& office() const noexcept { return office_; }
    const std::string& area() const noexcept { return window_.area(); }

    /// A graphics window shows the pane's area, and gave it a room.
    bool granted() const noexcept { return in_window(); }
    bool in_window() const noexcept { return window_.shown(); }
    /// The office a paste is asked of: the window showing the area, none while no window does.
    std::string clipboard_office() const { return window_.clipboard_office(); }
    /// The rows and columns the pane composes for: the window room's.
    std::int64_t rows() const noexcept { return rows_; }
    std::int64_t columns() const noexcept { return columns_; }

    /// THE ROOM A GRAPHICS WINDOW GIVES THE PANE, said by the window that shows its area as that
    /// window's office: true when the window, its incarnation, its size or its measure changed or
    /// the area is empty, so the window holds none of the pane's rows.
    bool window_room(const graphics::AreaRoom& r, const loom::Mail& mail) {
        if (!window_.room(r, mail)) {
            return false;
        }
        const RowsRoom room = window_.rows_room();
        rows_ = room.rows();
        columns_ = room.columns();
        return true;
    }

    /// A DRAWING THE WINDOW REFUSED, said by the window that shows the area: the rows last drawn
    /// go again whole at once after a refused change, and after a refused whole drawing nothing
    /// is sent until the rows change (`WindowDrawing::refused`).
    bool window_refused(const graphics::DrawnRefused& refused, loom::Mail& mail) {
        return window_.refused(refused, mail);
    }

    /// A PRESS ON A PART IN THE WINDOW, asked by the window that shows the area as its office: the
    /// place of the part named `asked.action` in the rows last drawn there. None for any other
    /// asker or area, or an action those rows name no part by: no act, and nothing spent.
    std::optional<RowCell> window_part(const graphics::ActionRequested& asked,
                                       const loom::Mail& mail) const {
        if (!window_.from_window(asked.area, mail)) {
            return std::nullopt;
        }
        const auto at = window_places_.find(asked.action);
        return at == window_places_.end() ? std::nullopt : std::optional<RowCell>(at->second);
    }

    /// Whether `mail` is the word of the window that shows the area, about `about`.
    bool from_window(const std::string& about, const loom::Mail& mail) const {
        return window_.from_window(about, mail);
    }

    /// A POINTER MOMENT THE WINDOW SHOWING THE AREA GAVE IT, as that window's office, read onto the
    /// rows last drawn there (`WindowDrawing::pointer`). None for another area or speaker.
    template <class Moment>
    std::optional<WindowPointer> window_pointer(const Moment& moment,
                                                const loom::Mail& mail) const {
        return window_.pointer(moment, mail, static_cast<std::int64_t>(said_rows_.size()),
                               columns_);
    }
    std::optional<WindowPointer> window_pointer(const graphics::AreaHoldEnded& ended,
                                                const loom::Mail& mail) const {
        return window_.pointer(ended, mail);
    }

    /// WHERE A POINT OF THE AREA FALLS ON THE ROWS LAST DRAWN THERE, shown or not: a drop's place.
    RowCell cell_at(std::int64_t x, std::int64_t y) const {
        return window_cell(x, y, static_cast<std::int64_t>(said_rows_.size()), columns_,
                           window_.text());
    }

    /// Whether the keys reach the pane now, as Workshop last said (`PaneKeys`).
    bool keys_here() const noexcept { return keys_here_; }
    void keys_here(bool here) noexcept { keys_here_ = here; }

    /// THE POINTER COME TO REST ON CELL `at` of the rows drawn there: true when it is another cell
    /// than the one it rested on, or it rested on none.
    bool rest(const RowCell& at) {
        if (rest_ && *rest_ == at) {
            return false;
        }
        rest_ = at;
        return true;
    }
    /// ...AND GONE FROM THE AREA: true when it rested there.
    bool leave() {
        const bool rested = rest_.has_value();
        rest_.reset();
        return rested;
    }
    /// The cell the pointer rests on, as the weave was last handed it, if it rests there.
    const std::optional<RowCell>& resting() const noexcept { return rest_; }

    /// THE ROOM ASKED OF THE GRAPHICS DIRECTORY, as the office, while no window has said it: a
    /// pane reloaded into a window draws there again.
    void ask_room(loom::Mail& mail) const {
        if (!in_window()) {
            (void)mail.as_role(office_).send_to_role(graphics::kGraphicsRole,
                                                     graphics::RoomAsked{area()});
        }
    }

    /// THE ROWS LAST SAID, drawn in the window that shows the area with the caret and selection
    /// said beside them: what changed there, or the whole of them after a new room or a refusal.
    void redraw(loom::Mail& mail) {
        if (!in_window() || rows_ <= 0 || columns_ <= 0) {
            return;
        }
        RowsDrawing drawn =
            rows_drawing(said_rows_, said_parts_, window_.rows_room(), columns_, said_caret_);
        drawn.sketch.number(said_number_);
        window_places_ = std::move(drawn.places);
        window_.draw(drawn.sketch, mail);
    }

    /// THE PANE'S ROWS SAID, with their parts, the caret standing in them, and the pane's number
    /// for what they mean (`component::RowMap::picture`, 0 for none), and drawn in the window that
    /// shows the area (`redraw`).
    void publish_rows(loom::Mail& mail, std::vector<TextRow> rows, std::vector<RowPart> parts = {},
                      const RowsCaret& caret = {}, std::int64_t number = 0) {
        said_rows_ = std::move(rows);
        said_parts_ = std::move(parts);
        said_caret_ = caret;
        said_number_ = number;
        redraw(mail);
    }

    /// Whether a drop at `x`, `y` of the area names another element than the rows last drawn there
    /// show, by the window's word of what it drew under the hand (`WindowDrawing::moved`).
    bool moved(const std::string& under, const std::string& part, std::int64_t drawing,
               std::int64_t x, std::int64_t y) const {
        return window_.moved(under, part, drawing, x, y);
    }

private:
    std::string pane_, office_;
    std::int64_t rows_ = 0, columns_ = 0;
    WindowDrawing window_{std::string(), std::string()};
    /// The rows, parts and caret last said, and where a press in the window reaches each named
    /// part.
    std::vector<TextRow> said_rows_;
    std::vector<RowPart> said_parts_;
    RowsCaret said_caret_;
    std::int64_t said_number_ = 0;
    std::map<std::string, RowCell> window_places_;
    bool keys_here_ = false;
    std::optional<RowCell> rest_;
};

/// THE OFFICE OF SEVERAL PANES AS WORKSHOP AND THE WINDOWS HEAR IT: its activation, its offers on
/// each one and on Workshop's catalog request (the weave's `announce`, or its `on_activated`), its
/// manual from `Page` about any pane it offers (`known_pane`), answered about `DefaultPane` for a
/// key it does not, which the answer refuses by name, and each pane's window room, refusals and
/// presses, found by area among the seats the weave's `each_seat` visits.
template <const char* const& Office, const char* const& DefaultPane, const manual::Manual& Page,
          bool HearsRest = false>
class MultiPaneFacet {
public:
    using Accept = std::conditional_t<
        HearsRest,
        loom::Accept<loom::Activated, PaneCatalogRequested, PaneDocumentRequested,
                     graphics::AreaRoom, graphics::DrawnRefused, graphics::ActionRequested,
                     graphics::AreaButtonChanged, graphics::AreaPointerMoved,
                     graphics::AreaPointerLeft, graphics::AreaWheelTurned, graphics::AreaHoldEnded,
                     PaneKeys>,
        loom::Accept<loom::Activated, PaneCatalogRequested, PaneDocumentRequested,
                     graphics::AreaRoom, graphics::DrawnRefused, graphics::ActionRequested,
                     graphics::AreaButtonChanged, graphics::AreaPointerMoved,
                     graphics::AreaWheelTurned, graphics::AreaHoldEnded, PaneKeys>>;
    using Emit = loom::Emit<PaneDocumentShown, graphics::Drawn, graphics::RoomAsked>;

    /// An accepted activation offers the panes, or is the weave's `on_activated`, which does; then
    /// each pane's window room is asked, once a life, so a reloaded office draws there again.
    template <class Self>
    void on(Self& self, const loom::Activated& activated, loom::Mail& mail) {
        if (!activation_.accept(mail, activated)) {
            return;
        }
        if constexpr (requires { self.on_activated(mail); }) {
            self.on_activated(mail);
        } else {
            self.announce(mail);
        }
        self.each_seat([&mail](PaneCanvasSeat& seat) { seat.ask_room(mail); });
    }

    /// THE ROOM A GRAPHICS WINDOW GIVES ONE OF THE PANES, from the window that shows its area: the
    /// pane composed again for it (`on_window_room`).
    template <class Self>
    void on(Self& self, const graphics::AreaRoom& r, loom::Mail& mail) {
        PaneCanvasSeat* seat = seat_of_area(self, r.area);
        if (seat == nullptr || !seat->window_room(r, mail)) {
            return;
        }
        self.on_window_room(seat->pane(), mail);
    }

    /// A drawing the window refused: the pane's rows go whole, once (`window_refused`), so a
    /// picture the window will always refuse is not sent again and again.
    template <class Self>
    void on(Self& self, const graphics::DrawnRefused& refused, loom::Mail& mail) {
        if (PaneCanvasSeat* seat = seat_of_area(self, refused.area)) {
            (void)seat->window_refused(refused, mail);
        }
    }

    /// A PRESS ON A PART IN A WINDOW: the weave's `on_window_press` at the part's place in the rows
    /// that window shows, for an action naming a part they hold (`PaneCanvasSeat::window_part`).
    template <class Self>
    void on(Self& self, const graphics::ActionRequested& asked, loom::Mail& mail) {
        const PaneCanvasSeat* seat = seat_of_area(self, asked.area);
        if (seat == nullptr) {
            return;
        }
        if (const std::optional<RowCell> at = seat->window_part(asked, mail)) {
            self.on_window_press(seat->pane(), at->row, at->column, mail);
        }
    }

    /// THE POINTER IN A WINDOW, given by the window that shows one of the panes' areas as its
    /// office, a hold it ended included: the moment read onto the rows drawn there, to the weave's
    /// `on_window_pointer(pane, pointer, keys_went_here, mail)` where it has one -- else a
    /// first-button press on a shown cell to its `on_window_press`. Whether the keys were already
    /// the pane's is the last `PaneKeys` Workshop said about it.
    template <class Self>
    void on(Self& self, const graphics::AreaButtonChanged& b, loom::Mail& mail) {
        window_moment(self, b.area, b, mail);
    }
    /// ...A MOVE, and where the medium reports one, for an office whose panes hear it
    /// (`kHearsRest`), the pointer resting on the rows: a move with no button held is also the
    /// weave's `on_window_rest(pane, cell, mail)`, once each time the cell under it changes.
    template <class Self>
    void on(Self& self, const graphics::AreaPointerMoved& m, loom::Mail& mail) {
        window_moment(self, m.area, m, mail);
        if constexpr (HearsRest) {
            PaneCanvasSeat* seat = seat_of_area(self, m.area);
            const std::optional<WindowPointer> p =
                seat == nullptr ? std::nullopt : seat->window_pointer(m, mail);
            if (p && p->press == 0 && seat->rest(p->at)) {
                self.on_window_rest(seat->pane(), p->at, mail);
            }
        }
    }
    /// ...AND ITS LEAVE, which the window that shows the area says: the weave's
    /// `on_window_left(pane, mail)`, once for each rest it was handed.
    template <class Self>
    void on([[maybe_unused]] Self& self, [[maybe_unused]] const graphics::AreaPointerLeft& left,
            [[maybe_unused]] loom::Mail& mail) {
        if constexpr (HearsRest) {
            PaneCanvasSeat* seat = seat_of_area(self, left.area);
            if (seat == nullptr || !seat->from_window(left.area, mail) || !seat->leave()) {
                return;
            }
            self.on_window_left(seat->pane(), mail);
        }
    }
    template <class Self>
    void on(Self& self, const graphics::AreaWheelTurned& w, loom::Mail& mail) {
        window_moment(self, w.area, w, mail);
    }
    template <class Self>
    void on(Self& self, const graphics::AreaHoldEnded& e, loom::Mail& mail) {
        window_moment(self, e.area, e, mail);
    }

    /// Whether the keys reach one of the panes now, as Workshop says.
    template <class Self>
    void on(Self& self, const PaneKeys& keys, loom::Mail& mail) {
        if (!from_workshop(mail)) {
            return;
        }
        self.each_seat([&keys](PaneCanvasSeat& seat) {
            if (seat.pane() == keys.pane) {
                seat.keys_here(keys.here);
            }
        });
    }

    /// Workshop asking who has panes. A weave that judges Workshop's word itself -- counting what
    /// it refused -- does so in `workshop_said`.
    template <class Self>
    void on(Self& self, const PaneCatalogRequested&, loom::Mail& mail) {
        bool said = false;
        if constexpr (requires { self.workshop_said(mail); }) {
            said = self.workshop_said(mail);
        } else {
            said = from_workshop(mail);
        }
        if (said) {
            self.announce(mail);
        }
    }

    template <class Self>
    void on(Self& self, const PaneDocumentRequested& ask, loom::Mail& mail) {
        const std::string_view pane =
            self.known_pane(ask.pane) ? std::string_view(ask.pane) : std::string_view(DefaultPane);
        (void)mail.answer(pane_document(ask, pane, Page));
    }

    /// Whether this delivery is Workshop's own word, as its office.
    static bool from_workshop(const loom::Mail& mail) {
        return mail.authored_from_role(pane_menu::kWorkshopRole);
    }

    /// The office whose panes these are.
    static const char* office() noexcept { return Office; }
    const ActivationCursor& activation() const noexcept { return activation_; }

private:
    template <class Self, class Moment>
    static void window_moment(Self& self, const std::string& area, const Moment& moment,
                              loom::Mail& mail) {
        PaneCanvasSeat* seat = seat_of_area(self, area);
        const std::optional<WindowPointer> p =
            seat == nullptr ? std::nullopt : seat->window_pointer(moment, mail);
        if (!p) {
            return;
        }
        const bool keys_went_here = seat->keys_here();
        // A PRIMARY PRESS OF ITS OWN MOVES THE KEYS TO THE PANE (WL-FOCUS-03), so the next press is
        // made with them there, whether or not Workshop's word on this one has come yet.
        if (p->phase == canvas_pointer::kPress && p->button == 1) {
            seat->keys_here(true);
        }
        if constexpr (requires { self.on_window_pointer(seat->pane(), *p, true, mail); }) {
            self.on_window_pointer(seat->pane(), *p, keys_went_here, mail);
        } else if (p->phase == canvas_pointer::kPress && p->button == 1 && p->at.shown) {
            self.on_window_press(seat->pane(), p->at.row, p->at.column, mail);
        }
    }

    /// The seat whose pane a window shows in `area`, or nullptr.
    template <class Self>
    static PaneCanvasSeat* seat_of_area(Self& self, const std::string& area) {
        PaneCanvasSeat* found = nullptr;
        self.each_seat([&found, &area](PaneCanvasSeat& seat) {
            if (found == nullptr && seat.area() == area) {
                found = &seat;
            }
        });
        return found;
    }

    ActivationCursor activation_;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_SEAT_HPP
