// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `screen.hpp`'s sections -- the project browser, presented, and which revealable
// row the pointer is on -- compiled once into `zengine-workshop-logic` and linked by the host and
// every suite; the declarations, the constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/arrangement.md (+6 registers; agents/workshop.md routes)

#include "screen.hpp"

namespace zengine::workshop {

// ---- THE PROJECT BROWSER, PRESENTED -----------------------------------------------------

// WL-EDIT-10 -- agents/workshop/editor.md
std::int64_t spend_wheel(double& accum, double dy, std::int64_t rows_per_notch) {
    accum += dy * static_cast<double>(rows_per_notch);
    const std::int64_t rows = static_cast<std::int64_t>(accum);
    accum -= static_cast<double>(rows);
    return rows;
}

ExternalBodyPlace files_body(const Session& s, const Screen& sc) {
    const PanelBounds where = bounds_of(s.panels, s.setup.active, panel::kProjectFiles, sc);
    if (!where.open) {
        return ExternalBodyPlace{};
    }
    return external_body_place(where.rect, sc, kFilesHeaderRows);
}

// WL-PROJ-10 -- agents/workshop/project.md
std::string files_header_prefix(const FilesPane& pane, const std::string& why,
                                bool typing) {
    const std::size_t total = pane.listing.rows.size();
    std::string out = "Files";
    if (typing) {
        out += " *"; // the keys are here -- the Editor header's own mark, same voice
    }
    if (!pane.listing.known) {
        out += " --";
    } else if (total == 0) {
        out += " empty";
    } else {
        const std::size_t at = pane.cursor < total ? pane.cursor + 1 : total;
        out += " " + std::to_string(at) + "/" + std::to_string(total);
        // A BOUND CLAIMS WHAT IT READ. The walk stopped at the ceiling, so what
        // stands beside the count is the fact that counting STOPPED -- never a total this
        // browser never reached, and never a fraction of one.
        if (pane.listing.bounded) {
            out += "+ (stopped counting)";
        }
    }
    if (!why.empty()) {
        out += "  " + why;
    }
    out += "  ";
    return out;
}

std::string files_location(const FilesPane& pane) {
    return pane.current_dir.empty() ? std::string("nowhere") : pane.current_dir;
}

std::string files_header_full(const FilesPane& pane, const std::string& why,
                              bool typing) {
    return files_header_prefix(pane, why, typing) + files_location(pane);
}

std::string files_header(const FilesPane& pane, const std::string& why, bool typing,
                         std::int64_t columns) {
    const std::string out = files_header_prefix(pane, why, typing);
    return out +
           detail::fit_path(files_location(pane), columns - static_cast<std::int64_t>(out.size()));
}

// WL-FILES-04, WL-FILES-10 -- agents/workshop/files.md
std::string files_row_text(const FileRow& row) {
    std::string out = shown_name(row.name);
    if (row.directory) {
        out += "/";
        if (row.linked) {
            out += "  (link)";
        }
    }
    if (!row.openable) {
        out += "  (name this Workshop cannot open)";
    }
    return out;
}

std::string files_row_full(const FileRow& row, bool here) {
    return std::string(here ? "> " : "  ") + files_row_text(row);
}

FilesPressAt files_press_at(const Session& s, const Screen& sc, std::int64_t space,
                            std::int64_t x, std::int64_t y) {
    const ExternalBodyPlace body = files_body(s, sc);
    if (!body.present) {
        return FilesPressAt{};
    }
    const ProseAt at = prose_at(space, x, y, body.region_x, body.region_y, body.fit);
    if (!at.understood) {
        return FilesPressAt{};
    }
    const std::int64_t row = at.row - body.header_rows;
    if (row < 0 || row >= body.rows) {
        return FilesPressAt{};
    }
    return FilesPressAt{true, row};
}

bool over_files_body(const Session& s, const Screen& sc, std::int64_t space,
                     std::int64_t x, std::int64_t y) {
    const ExternalBodyPlace body = files_body(s, sc);
    if (!body.present) {
        return false;
    }
    const ProseAt at = prose_at(space, x, y, body.region_x, body.region_y, body.fit);
    return at.understood && at.row >= body.header_rows && at.row < body.fit.rows;
}

// WL-PRESS-03 -- agents/workshop/press-chain.md
bool files_row_of_body_row(const FilesPane& pane, std::int64_t body_rows,
                           std::int64_t body_row, std::size_t& out) {
    if (body_rows <= 0 || body_row < 0) {
        return false;
    }
    const std::size_t total = pane.listing.rows.size();
    const ListWindow win = list_window(total, pane.cursor, static_cast<std::size_t>(body_rows));
    const std::int64_t first_entry_row = win.before > 0 ? 1 : 0;
    const std::int64_t offset = body_row - first_entry_row;
    if (offset < 0 || offset >= static_cast<std::int64_t>(win.count)) {
        return false;
    }
    out = win.first + static_cast<std::size_t>(offset);
    return out < total;
}

// ---- Which revealable row the pointer is on ----------------------------------------------

RevealAt files_reveal_at(const Session& s, const Screen& sc, std::int64_t space,
                         std::int64_t x, std::int64_t y) {
    const ExternalBodyPlace body = files_body(s, sc);
    if (!body.present) {
        return RevealAt{};
    }
    const ProseAt at = prose_at(space, x, y, body.region_x, body.region_y, body.fit);
    if (!at.understood || at.column < 0 || at.column >= body.columns) {
        return RevealAt{};
    }
    const FilesPane& pane = s.panels.files;
    if (body.header_rows > 0 && at.row >= 0 && at.row < body.header_rows) {
        const std::string why = provenance_words(s.marks.provenance(pane.current_dir));
        const bool typing = files_has_keyboard(s);
        return RevealAt{true,
                        reveal_place::kFilesLocation,
                        0,
                        files_header_full(pane, why, typing),
                        detail::fit(files_header(pane, why, typing, body.columns), body.columns),
                        body.columns,
                        at.column};
    }
    if (!pane.listing.known || pane.listing.rows.empty()) {
        return RevealAt{}; // a refusal or an emptiness is the browser's own sentence
    }
    std::size_t index = 0;
    if (!files_row_of_body_row(pane, body.rows, at.row - body.header_rows, index)) {
        return RevealAt{}; // a marker row, the padding, or no row at all
    }
    const std::string full = files_row_full(pane.listing.rows[index], index == pane.cursor);
    return RevealAt{true,          reveal_place::kFilesRow, index, full,
                    detail::fit(full, body.columns), body.columns, at.column};
}

RevealAt info_reveal_at(const WorkshopDoc& d, const Session& s, std::int64_t space,
                        std::int64_t x, std::int64_t y) {
    const InfoBodyAt where = info_body_at(d, s, space, x, y);
    if (!where.present || where.at.column < 0 || where.at.column >= where.body.columns) {
        return RevealAt{};
    }
    const std::size_t object = object_at_prose_row(where.body, where.at.row);
    if (object != kNoObject && object < d.elements.size()) {
        const ui::Element& e = d.elements[object];
        const std::string full = object_row_full(e, e.id == s.selected);
        return RevealAt{true,
                        reveal_place::kInfoObject,
                        object,
                        full,
                        detail::fit(full, where.body.columns),
                        where.body.columns,
                        where.at.column};
    }
    const std::size_t property = property_at_prose_row(where.body, where.at.row);
    if (property == kNoProperty || property >= s.rows.size()) {
        return RevealAt{};
    }
    const Row& row = s.rows[property];
    if (row.editing()) {
        return RevealAt{}; // a draft owns its own window; see `paint_info`
    }
    const bool here = property == s.cursor;
    return RevealAt{true,
                    reveal_place::kInfoProperty,
                    property,
                    property_row_full(row, here),
                    property_row_text(row, here, where.body.value_columns),
                    where.body.columns,
                    where.at.column};
}

RevealAt reveal_at(const WorkshopDoc& d, const Session& s, std::int64_t space,
                   std::int64_t x, std::int64_t y) {
    const Screen sc = screen_of(s);
    const PointedAt at = canvas_point_of(space, x, y);
    if (!at.understood) {
        return RevealAt{};
    }
    const Occupancy here = occupied_at(s.panels, s.setup.active, sc, at);
    if (!here.occupied) {
        return RevealAt{};
    }
    if (here.kind == panel::kProjectFiles) {
        return files_reveal_at(s, sc, space, x, y);
    }
    if (here.kind == panel::kInfo) {
        return info_reveal_at(d, s, space, x, y);
    }
    return RevealAt{};
}

Revealed reveal_for(const WorkshopDoc& d, const Session& s, std::int64_t space,
                    std::int64_t x, std::int64_t y) {
    const RevealAt at = reveal_at(d, s, space, x, y);
    if (!at.clipped()) {
        return Revealed{};
    }
    return Revealed{at.place, at.item, at.text,
                    detail::reveal_offset_at_column(at.text, at.columns, at.column)};
}

void paint_files(surface::SurfaceLayer& layer, const Session& s, const FineRect& b,
                 const Screen& sc, const Keymap& k,
                 std::int64_t chrome) {
    paint_panel_frame(layer, b, chrome);
    const ExternalBodyPlace body = external_body_place(b, sc, kFilesHeaderRows);
    if (body.fit.rows <= 0 || body.fit.columns <= 0) {
        return; // no room for one row of this medium's type: say nothing at all
    }
    surface::SurfaceTextRegion region;
    region.x = body.region_x;
    region.y = body.region_y;
    region.w = body.region_w;
    region.h = body.region_h;
    region.sub_x = body.region_sub_x;
    region.sub_y = body.region_sub_y;
    region.sub_w = body.region_sub_w;
    region.sub_h = body.region_sub_h;
    const FilesPane& pane = s.panels.files;
    const auto say = [&region, &body](const std::string& text, std::int64_t role) {
        region.rows.push_back(surface::SurfaceTextRow{detail::fit(text, body.columns), role});
    };
    if (body.header_rows > 0) {
        // THE LOCATION IS THE ROW A POINTER MAY READ PAST. The full spelling is what
        // this painter is already holding; the fitted one is the same answer it has always
        // given; `reveal_shown` chooses between them and the choice is the pointer's.
        const std::string why = provenance_words(s.marks.provenance(pane.current_dir));
        const bool typing = files_has_keyboard(s);
        say(detail::reveal_shown(s.reveal, reveal_place::kFilesLocation, 0,
                                 files_header_full(pane, why, typing),
                                 files_header(pane, why, typing, body.columns), body.columns),
            surface::role::kAccent);
    }
    if (!body.present) {
        if (!region.rows.empty()) {
            layer.texts.push_back(std::move(region));
        }
        return; // room for the heading and nothing else: the heading, honestly
    }
    if (!pane.listing.known) {
        say(pane.listing.refusal.empty() ? std::string("nothing has been listed yet")
                                         : pane.listing.refusal,
            surface::role::kMuted);
        layer.texts.push_back(std::move(region));
        return;
    }
    if (pane.listing.rows.empty()) {
        say("this directory is empty", surface::role::kMuted);
        layer.texts.push_back(std::move(region));
        return;
    }
    const std::size_t rows = static_cast<std::size_t>(body.rows);
    const ListWindow win = list_window(pane.listing.rows.size(), pane.cursor, rows);
    if (win.before > 0) {
        say("  " + omitted_text(win.before, "earlier"), surface::role::kMuted);
    }
    for (std::size_t i = win.first; i < win.first + win.count; ++i) {
        const bool here = i == pane.cursor;
        const FileRow& row = pane.listing.rows[i];
        const std::string full = files_row_full(row, here);
        say(detail::reveal_shown(s.reveal, reveal_place::kFilesRow, i, full, full,
                                 body.columns),
            here ? surface::role::kAccent
                 : (row.openable ? surface::role::kFill : surface::role::kMuted));
    }
    if (win.after > 0) {
        say("  " + omitted_text(win.after, "more"), surface::role::kMuted);
    }
    // THE GESTURES ARE NOT PAINTED HERE. What Return and Backspace do in this pane is the
    // band's to say, from the one action truth (`help_rows` over `KeyContext::kFiles`), so
    // a maker who remapped them reads their own bindings rather than this file's guess --
    // and the pane spends every row it has on the project instead of on instructions.
    (void)k;
    layer.texts.push_back(std::move(region));
}

// WL-ARR-09 -- agents/workshop/arrangement.md
// WL-PANE-01 -- agents/workshop/panes-and-windows.md
// WL-FRONT-01 -- agents/workshop/planes.md
void paint_pane_affordances(surface::SurfaceLayer& layer, const Session& s,
                            const Screen& sc) {
    if (!s.arrange.open) {
        return;
    }
    const auto ring = [&](const PaneRef& ref, bool emphasized) {
        const std::optional<std::int64_t> kind = resolve_pane(ref, s.panels);
        // EVERY PANE WHOSE PLACE IS THE MAKER'S TO AUTHOR WEARS HANDLES. This
        // named the overlay stack while the stack was the only such place, which made the
        // ring a list rather than the rule it is; `place_is_authorable` is the same
        // exclusion the arrangement admission already spoke -- the side column is the
        // screen's, and a pane whose geometry no gesture can change must not advertise
        // eight grips that all refuse.
        if (!kind.has_value() || !place_is_authorable(placement_of(*kind))) {
            return;
        }
        const PanelBounds where = bounds_of(s.panels, s.setup.active, *kind, sc);
        if (!where.open || where.rect.w <= 0 || where.rect.h <= 0) {
            return;
        }
        const bool held = s.pane_drag.active && s.pane_drag.sizing && s.pane_drag.pane == ref;
        for (std::int64_t edge = 0; edge < pane_edge::kCount; ++edge) {
            const FineRect at = pane_edge_cell(where.rect, edge);
            const bool chosen = held ? s.pane_drag.edge == edge : emphasized;
            // THE WIRE SPELLING, cells plus remainders (`wire_rect_of`'s decomposition):
            // a label's x/y ARE canvas cells, and the fine-lattice construction that handed
            // them raw sub-units put every mark off the canvas -- rings that hit
            // correctly and painted nowhere, the exact see/grab split one geometry forbids.
            const std::int64_t cx = surface::cell_of_subs(at.x);
            const std::int64_t cy = surface::cell_of_subs(at.y);
            layer.labels.push_back(surface::SurfaceLabel{
                cx, cy, std::string(pane_edge_glyph(edge)),
                chosen ? surface::role::kAccent : surface::role::kMuted,
                at.x - surface::subs_of_cells(cx), at.y - surface::subs_of_cells(cy)});
        }
    };
    if (!s.arrange.desk) {
        if (s.arrange.addressed()) {
            ring(s.arrange.pane, true);
        }
        return;
    }
    for (const SetupPane& row : s.setup.active.panes) {
        ring(row.ref, s.arrange.addressed() && row.ref == s.arrange.pane);
    }
}

} // namespace zengine::workshop
