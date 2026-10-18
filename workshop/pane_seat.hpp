// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_SEAT_HPP
#define ZENGINE_WORKSHOP_PANE_SEAT_HPP

// What an office offering several panes keeps for each one, and the conversation it holds with
// Workshop and the graphics windows about all of them: one pane's rooms, the pictures numbered
// there and the window that shows its area (`PaneCanvasSeat`), and the office's activation,
// catalog answer, manual and window rooms (`MultiPaneFacet`, composed with `zengine::With`,
// activation/activation.hpp). The weave routes each pane's own Workshop deliveries to that pane's
// seat; an office of one pane composes workshop/pane_facets.hpp instead.

#include "activation/activation.hpp"
#include "graphics/canvas.hpp"
#include "graphics/vocabulary.hpp"
#include "pane_canvas_rows.hpp"
#include "pane_canvas_vocabulary.hpp"
#include "pane_document.hpp"
#include "pane_drawing.hpp"
#include "pane_menu.hpp"
#include "pane_vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <zen/weave.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop {

namespace detail {

/// THE PROSE A PANE'S ROWS GO AS, by the parts they name: parts that say no availability go as
/// `v4::PaneContent`, parts that say it as `v5::PaneContent`.
template <class Part>
struct ProseOf;
template <>
struct ProseOf<PaneRowPart> {
    using type = v4::PaneContent;
};
template <>
struct ProseOf<v2::PaneRowPart> {
    using type = v5::PaneContent;
};

} // namespace detail

/// ONE PANE'S ROOM, AS AN OFFICE OF SEVERAL PANES KEEPS IT: the canvas room the pane holds, the
/// pictures numbered there, the prose room a host granting no canvas gives it, and the graphics
/// window that shows the pane's area -- with the rows and columns it composes for, the canvas
/// lattice's while it holds one, else the prose room's, else, for a pane Workshop seats nowhere,
/// the window's. Named by its pane and the office it speaks as, it says the pane's rows itself, to
/// Workshop and to the window alike. Transient, as every room is.
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

    bool on_canvas() const noexcept {
        return canvas_.grant > 0 && canvas_.width > 0 && canvas_.height > 0;
    }
    /// A room was granted in this life: Workshop's, prose or canvas, or a graphics window's.
    bool granted() const noexcept { return seated_ || in_window(); }
    /// Workshop granted the pane a room in this life, prose or canvas.
    bool seated() const noexcept { return seated_; }
    /// A graphics window shows the pane's area.
    bool in_window() const noexcept { return window_.shown(); }
    /// The rows and columns the pane composes for: the canvas lattice's, else the prose room's,
    /// else the window's.
    std::int64_t rows() const noexcept { return rows_; }
    std::int64_t columns() const noexcept { return columns_; }
    std::int64_t prose_rows() const noexcept { return prose_rows_; }
    std::int64_t prose_columns() const noexcept { return prose_columns_; }
    const PaneCanvasRoom& room() const noexcept { return canvas_; }
    const PaneCanvasRoom& canvas_room() const noexcept { return canvas_; }
    /// The pictures numbered in this room, and which a press may still name.
    CanvasPictures& pictures() noexcept { return pictures_; }
    const CanvasPictures& pictures() const noexcept { return pictures_; }

    /// The room composed for, measured again.
    void fit() {
        const CanvasRows lattice = canvas_rows(canvas_);
        rows_ = on_canvas() ? lattice.rows : seated_ ? prose_rows_ : window_rows_;
        columns_ = on_canvas() ? lattice.columns : seated_ ? prose_columns_ : window_columns_;
    }

    /// THE PROSE ROOM, kept beside a canvas room: true when it is now the room composed for, false
    /// while the pane holds a canvas room, which, granted first, was this beat already.
    bool prose_room(const PaneRoom& room) {
        prose_rows_ = room.rows;
        prose_columns_ = room.columns;
        seated_ = true;
        if (on_canvas()) {
            return false;
        }
        fit();
        return true;
    }

    /// THE PANE'S OWN CANVAS: while it holds one, its rows stand on the room's lattice; a room of
    /// no extent leaves the pane its prose room.
    void canvas_room(const PaneCanvasRoom& room) {
        canvas_ = room;
        seated_ = true;
        fit();
    }

    /// THE ROOM A GRAPHICS WINDOW GIVES THE PANE, said by the window that shows its area as that
    /// window's office: true when the window, its incarnation or its size changed, so the window
    /// holds none of the pane's picture.
    bool window_room(const graphics::AreaRoom& r, const loom::Mail& mail) {
        if (!window_.room(r, mail)) {
            return false;
        }
        window_rows_ = r.h / graphics::kCellPx;
        window_columns_ = r.w / graphics::kCellPx;
        fit();
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
    /// asker or area, or an action that picture names no part by: no act, and nothing spent.
    std::optional<RowCell> window_part(const graphics::ActionRequested& asked,
                                       const loom::Mail& mail) const {
        if (!window_.from_window(asked.area, mail)) {
            return std::nullopt;
        }
        const auto at = window_places_.find(asked.action);
        return at == window_places_.end() ? std::nullopt : std::optional<RowCell>(at->second);
    }

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
        RowsDrawing drawn = rows_drawing(said_rows_, said_parts_, columns_, said_caret_);
        window_places_ = std::move(drawn.places);
        window_.draw(drawn.sketch, mail);
    }

    /// WHERE A HAND ON THE PANE'S PICTURE LANDED, as the row and column of its lattice, shown or
    /// not: none while the pane holds no canvas, or for a pointer naming a room since replaced.
    std::optional<RowCell> cell_of(const PaneCanvasPointer& event) const {
        if (!on_canvas() || event.grant != canvas_.grant) {
            return std::nullopt;
        }
        return row_cell_at(canvas_rows(canvas_), event.x, event.y);
    }

    /// IS `picture` OF `grant` ONE DRAWN UNDER THE MEANING THE PANE HOLDS NOW? A press or a drop on
    /// any other is fenced, never read against whatever row has since moved into its place.
    bool shows(std::int64_t grant, std::int64_t picture) const noexcept {
        return on_canvas() && pictures_.current(grant, picture);
    }
    bool shows(const PaneCanvasPointer& event) const noexcept {
        return shows(event.grant, event.picture);
    }

    /// THE PANE'S ROWS SAID, the caret standing in them, to the window showing its area (`redraw`)
    /// and, while Workshop seats it, to Workshop as its office: its picture while it holds a canvas
    /// room, numbered under `meaning`; else `prose` as it stands, its caret beside it while there
    /// is one -- or always with `always_caret`, "none" included, so a caret the rows lost is gone.
    /// The parts decide the shapes: `PaneRowPart`s go as `v4::PaneContent` or
    /// `v5::PaneCanvasContent`, `v2::PaneRowPart`s as `v5::PaneContent` or `v6::PaneCanvasContent`.
    template <class Content>
    void say(loom::Mail& mail, Content prose, std::int64_t meaning, const RowsCaret& caret = {},
             bool always_caret = false) {
        said_rows_ = prose.rows;
        said_parts_.clear();
        for (const auto& part : prose.parts) {
            said_parts_.push_back(PaneRowPart{part.name, part.row, part.column, part.columns});
        }
        said_caret_ = caret;
        redraw(mail);
        if (!seated_) {
            return;
        }
        if (on_canvas()) {
            (void)mail.as_role(office_).send_to_role(
                pane_menu::kWorkshopRole,
                rows_picture(canvas_, pictures_.next(canvas_, meaning), prose.rows, prose.parts,
                             caret));
            return;
        }
        (void)mail.as_role(office_).send_to_role(pane_menu::kWorkshopRole, prose);
        if (always_caret || caret.row != surface::kNoCaret) {
            (void)mail.as_role(office_).send_to_role(
                pane_menu::kWorkshopRole,
                PaneCaret{pane_, caret.row, caret.column, caret.sel_begin_row, caret.sel_begin_col,
                          caret.sel_end_row, caret.sel_end_col});
        }
    }

    /// ...ROWS COMPOSED HERE, numbered `meaning` in prose as on the canvas, with their parts.
    template <class Part = PaneRowPart>
    void publish_rows(loom::Mail& mail, std::int64_t meaning,
                      std::vector<surface::SurfaceTextRow> rows, std::vector<Part> parts = {},
                      const RowsCaret& caret = {}, bool always_caret = false) {
        typename detail::ProseOf<Part>::type prose;
        prose.pane = pane_;
        prose.rows = std::move(rows);
        prose.picture = meaning;
        prose.parts = std::move(parts);
        say(mail, std::move(prose), meaning, caret, always_caret);
    }

private:
    std::string pane_, office_;
    PaneCanvasRoom canvas_;
    CanvasPictures pictures_;
    std::int64_t prose_rows_ = 0, prose_columns_ = 0;
    std::int64_t window_rows_ = 0, window_columns_ = 0;
    std::int64_t rows_ = 0, columns_ = 0;
    bool seated_ = false;
    WindowDrawing window_{std::string(), std::string()};
    /// The rows, parts and caret last said, and where a press in the window reaches each named
    /// part.
    std::vector<surface::SurfaceTextRow> said_rows_;
    std::vector<PaneRowPart> said_parts_;
    RowsCaret said_caret_;
    std::map<std::string, RowCell> window_places_;
};

/// THE OFFICE OF SEVERAL PANES AS WORKSHOP AND THE WINDOWS HEAR IT: its activation, its offers on
/// each one and on Workshop's catalog request (the weave's `announce`, or its `on_activated`), its
/// manual from `Page` about any pane it offers (`known_pane`), answered about `DefaultPane` for a
/// key it does not, which the answer refuses by name, and each pane's window room, refusals and
/// presses, found by area among the seats the weave's `each_seat` visits. Each pane's Workshop
/// room, rows and presses stay the weave's to route to that pane's seat.
template <const char* const& Office, const char* const& DefaultPane, const manual::Manual& Page>
class MultiPaneFacet {
public:
    using Accept = loom::Accept<loom::Activated, PaneCatalogRequested, PaneDocumentRequested,
                                graphics::AreaRoom, graphics::DrawnRefused,
                                graphics::ActionRequested>;
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

    /// THE ROOM A GRAPHICS WINDOW GIVES ONE OF THE PANES, from the window that shows its area. A
    /// pane Workshop seats keeps composing for Workshop's room, and the rows it last said go to the
    /// window whole; one it seats nowhere is composed again for the window's (`on_window_room`).
    template <class Self>
    void on(Self& self, const graphics::AreaRoom& r, loom::Mail& mail) {
        PaneCanvasSeat* seat = seat_of_area(self, r.area);
        if (seat == nullptr || !seat->window_room(r, mail)) {
            return;
        }
        if (seat->seated()) {
            seat->redraw(mail);
        } else {
            self.on_window_room(seat->pane(), mail);
        }
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
