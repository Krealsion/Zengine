// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_SEAT_HPP
#define ZENGINE_WORKSHOP_PANE_SEAT_HPP

// What an office offering several panes keeps for each one, and the conversation it holds with
// Workshop about all of them: one pane's rooms and the pictures numbered there (`PaneCanvasSeat`),
// and the office's activation, catalog answer and manual (`MultiPaneFacet`, composed with
// `zengine::With`, activation/activation.hpp). The weave still routes each pane's own deliveries
// to that pane's seat; an office of one pane composes workshop/pane_facets.hpp instead.

#include "activation/activation.hpp"
#include "pane_canvas_rows.hpp"
#include "pane_canvas_vocabulary.hpp"
#include "pane_document.hpp"
#include "pane_menu.hpp"
#include "pane_vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <zen/weave.hpp>

#include <cstdint>
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
/// pictures numbered there, and the prose room a host granting no canvas gives it -- with the rows
/// and columns it composes for, the canvas lattice's while it holds one, else the prose room's.
/// Named by its pane and the office it speaks as, it says the pane's rows itself. Transient, as
/// every room is.
class PaneCanvasSeat {
public:
    PaneCanvasSeat() = default;
    PaneCanvasSeat(std::string pane, std::string office)
        : pane_(std::move(pane)), office_(std::move(office)) {}

    const std::string& pane() const noexcept { return pane_; }
    const std::string& office() const noexcept { return office_; }

    bool on_canvas() const noexcept {
        return canvas_.grant > 0 && canvas_.width > 0 && canvas_.height > 0;
    }
    /// A room was granted in this life, prose or canvas.
    bool granted() const noexcept { return granted_; }
    /// The rows and columns the pane composes for: the canvas lattice's, else the prose room's.
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
        rows_ = on_canvas() ? lattice.rows : prose_rows_;
        columns_ = on_canvas() ? lattice.columns : prose_columns_;
    }

    /// THE PROSE ROOM, kept beside a canvas room: true when it is now the room composed for, false
    /// while the pane holds a canvas room, which, granted first, was this beat already.
    bool prose_room(const PaneRoom& room) {
        prose_rows_ = room.rows;
        prose_columns_ = room.columns;
        granted_ = true;
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
        granted_ = true;
        fit();
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

    /// THE PANE'S ROWS SAID, as its office to Workshop: its picture while it holds a canvas room,
    /// numbered under `meaning`, the caret standing in it; else `prose` as it stands, and its caret
    /// beside it while there is one -- or every time with `always_caret`, "none" included, so a
    /// caret the rows lost is gone. `prose`'s parts decide the shapes: `PaneRowPart`s go as
    /// `v4::PaneContent` or `v5::PaneCanvasContent`, `v2::PaneRowPart`s as `v5::PaneContent` or
    /// `v6::PaneCanvasContent`.
    template <class Content>
    void say(loom::Mail& mail, Content prose, std::int64_t meaning, const RowsCaret& caret = {},
             bool always_caret = false) {
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
    std::int64_t rows_ = 0, columns_ = 0;
    bool granted_ = false;
};

/// THE OFFICE OF SEVERAL PANES AS WORKSHOP HEARS IT: the activation it lives under, its offers on
/// each one and on Workshop's catalog request (the weave's `announce`, or its `on_activated`), and
/// its manual from `Page` about any pane it offers (the weave's `known_pane`), answered about
/// `DefaultPane` for a key it does not, which the answer refuses by name. Each pane's room, rows
/// and presses stay the weave's to route to that pane's seat.
template <const char* const& Office, const char* const& DefaultPane, const manual::Manual& Page>
class MultiPaneFacet {
public:
    using Accept = loom::Accept<loom::Activated, PaneCatalogRequested, PaneDocumentRequested>;
    using Emit = loom::Emit<PaneDocumentShown>;

    /// An accepted activation offers the panes, or is the weave's `on_activated`, which does.
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
    ActivationCursor activation_;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_SEAT_HPP
