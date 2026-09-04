// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `screen.hpp`'s sections -- a maker-made pane, presented, and the pane editor --
// compiled once into `zengine-workshop-logic` and linked by the host and every suite; the
// declarations, the constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/pane-manager.md (+6 registers; agents/workshop.md routes)

#include "screen.hpp"

namespace zengine::workshop {

// ---- A MAKER-MADE PANE, PRESENTED: authored regions on an offered interior -----------------

RegionPresentation present_region(const TextRegion& r, const FineRect& interior,
                                  const Screen& sc) {
    RegionPresentation p;
    if (interior.empty()) {
        return p;
    }
    p.asked = FineRect{surface::add_cells(interior.x, r.x), surface::add_cells(interior.y, r.y),
                       r.w, r.h};
    p.shown = clip_to_fine(p.asked, interior);
    p.clipped = !(p.shown == p.asked);
    if (p.shown.empty()) {
        return p;
    }
    p.fit = surface::fit_region_subs(p.shown.x, p.shown.y, p.shown.w, p.shown.h,
                                     sc.text_advance_px, sc.text_line_px);
    p.present = true;
    return p;
}

surface::SurfaceTextRegion region_over(const FineRect& r) {
    surface::SurfaceTextRegion region;
    const surface::SurfaceRect wire = wire_rect_of(r, surface::role::kFill);
    region.x = wire.x;
    region.y = wire.y;
    region.w = wire.w;
    region.h = wire.h;
    region.sub_x = wire.sub_x;
    region.sub_y = wire.sub_y;
    region.sub_w = wire.sub_w;
    region.sub_h = wire.sub_h;
    return region;
}

const TextRegion* maker_region(const Session& s, const PaneRef& ref, std::int64_t id) {
    const MakerPane& m = s.panels.maker;
    if (!m.open() || !(maker_pane_ref(m.definition.name) == ref)) {
        return nullptr;
    }
    return region_of(m.definition, id);
}

FineRect maker_pane_interior(const Session& s, const Screen& sc) {
    const PanelBounds where = bounds_of(s.panels, s.setup.active, kMakerPaneKind, sc);
    if (!where.open || where.rect.empty()) {
        return FineRect{};
    }
    return pane_inside(where.rect, sc).rect;
}

std::string region_axis_text(const Session& s, const PaneRef& ref, std::int64_t id,
                             std::size_t axis) {
    const TextRegion* r = maker_region(s, ref, id);
    if (r == nullptr) {
        return "--";
    }
    const std::int64_t v = axis == 0 ? r->x : axis == 1 ? r->y : axis == 2 ? r->w : r->h;
    bool projected = false;
    std::string out = geometry_amount_text(v, s.cell_px, projected) + " " +
                      std::string(geometry_unit(s.cell_px));
    if (projected) {
        out += kProjectedNote;
    }
    return out;
}

Written write_region_axis(Session& s, const PaneRef& ref, std::int64_t id,
                          std::size_t axis, const std::string& text) {
    if (maker_region(s, ref, id) == nullptr) {
        return Written::no(ref_text(ref) + " is not the open pane definition -- nothing to author");
    }
    std::string_view body = text;
    while (!body.empty() && body.front() == ' ') {
        body.remove_prefix(1);
    }
    while (!body.empty() && body.back() == ' ') {
        body.remove_suffix(1);
    }
    if (body == "-") {
        return Written::no("a region has no default to reset to -- type a whole number of " +
                           std::string(geometry_unit(s.cell_px)));
    }
    const FaceAmount typed = parse_face_amount(body, s.cell_px);
    if (!typed.accepted) {
        return Written::no(typed.refusal);
    }
    return author_region_axis(s.panels.maker.definition, id, axis, typed.subs);
}

Written write_region_text(Session& s, const PaneRef& ref, std::int64_t id,
                          std::string text) {
    if (maker_region(s, ref, id) == nullptr) {
        return Written::no(ref_text(ref) + " is not the open pane definition -- nothing to author");
    }
    return set_region_text(s.panels.maker.definition, id, std::move(text));
}

std::string region_resolved_text(const Session& s, const PaneRef& ref, std::int64_t id) {
    const TextRegion* r = maker_region(s, ref, id);
    if (r == nullptr) {
        return "--";
    }
    const Screen sc = screen_of(s);
    const FineRect interior = maker_pane_interior(s, sc);
    const RegionPresentation p = present_region(*r, interior, sc);
    if (!p.present) {
        return "- (the pane is not presented, or the region lies outside it)";
    }
    const FineRect local{p.shown.x - interior.x, p.shown.y - interior.y, p.shown.w, p.shown.h};
    std::string out = fine_rect_text(local, s.cell_px);
    if (p.clipped) {
        out += " (clipped by the pane)";
    }
    return out;
}

std::string region_shown_text(const Session& s, const PaneRef& ref, std::int64_t id) {
    const TextRegion* r = maker_region(s, ref, id);
    if (r == nullptr) {
        return "--";
    }
    const Screen sc = screen_of(s);
    const RegionPresentation p = present_region(*r, maker_pane_interior(s, sc), sc);
    if (!p.present || p.fit.rows <= 0 || p.fit.columns <= 0) {
        return "no room -- nothing of it is drawn on this face";
    }
    return std::to_string(p.fit.rows) + (p.fit.rows == 1 ? " row x " : " rows x ") +
           std::to_string(p.fit.columns) + (p.fit.columns == 1 ? " column, " : " columns, ") +
           (p.fit.graphical() ? "presented in type" : "presented as cells");
}

std::string interior_capture_text(const Session& s, const PaneRef& ref) {
    const std::optional<std::int64_t> kind = resolve_pane(ref, s.panels);
    if (!kind.has_value()) {
        if (ref.provider == kMakerPaneProvider) {
            return "no open definition is named " + ref.pane + " -- nothing to show";
        }
        return "unresolved -- nothing to inspect";
    }
    const Screen sc = screen_of(s);
    const PanelBounds where = bounds_of(s.panels, s.setup.active, *kind, sc);
    const char* whose = is_runtime_kind(*kind) ? "a provider's own" : "code-backed";
    if (!where.open || where.rect.empty()) {
        return std::string(whose) + " -- not presented; no authored interior";
    }
    const PanelProsePlace place = panel_prose_place(where.rect, sc);
    if (!place.present) {
        return std::string(whose) + " -- body " + fine_rect_text(place.inside, s.cell_px) +
               ", no room for a row; no authored interior";
    }
    return std::string(whose) + " -- body " + fine_rect_text(place.inside, s.cell_px) + ", " +
           std::to_string(place.rows) + (place.rows == 1 ? " row x " : " rows x ") +
           std::to_string(place.columns) + (place.columns == 1 ? " column " : " columns ") +
           (place.fit.graphical() ? "in type" : "as cells") + "; no authored interior";
}

// WL-MAKER-05 -- agents/workshop/maker-pane.md
void paint_maker_pane(surface::SurfaceLayer& layer, const Session& s, const FineRect& b,
                      const Screen& sc, std::int64_t chrome) {
    paint_panel_frame(layer, b, chrome);
    const PaneInside inside = pane_inside(b, sc);
    if (inside.rect.empty()) {
        return;
    }
    layer.texts.push_back(region_over(inside.rect));
    const MakerPane& m = s.panels.maker;
    if (!m.open()) {
        return;
    }
    for (const TextRegion& r : m.definition.regions) {
        const RegionPresentation p = present_region(r, inside.rect, sc);
        if (!p.present || p.fit.rows <= 0 || p.fit.columns <= 0) {
            continue;
        }
        surface::SurfaceTextRegion region = region_over(p.shown);
        region.rows.push_back(
            surface::SurfaceTextRow{detail::fit(r.text, p.fit.columns), surface::role::kFill});
        layer.texts.push_back(std::move(region));
    }
}

const TextRegion* creator_subject_region(const Session& s) {
    const MakerPane& m = s.panels.maker;
    if (!m.open() || m.definition.regions.empty() || !s.panels.has(panel::kPaneEditor)) {
        return nullptr;
    }
    if (!s.pane_editor.addressed() ||
        !(s.pane_editor.subject == maker_pane_ref(m.definition.name))) {
        return nullptr;
    }
    return &m.definition.regions.front();
}

// WL-MAKER-06 -- agents/workshop/maker-pane.md
void paint_creator_region_mark(surface::SurfaceLayer& layer, const Session& s,
                               const Screen& sc) {
    const TextRegion* r = creator_subject_region(s);
    if (r == nullptr) {
        return;
    }
    const PanelBounds where = bounds_of(s.panels, s.setup.active, kMakerPaneKind, sc);
    if (!where.open || where.rect.empty() ||
        pane_is_covered(s.panels, s.setup.active, sc, kMakerPaneKind, where.rect)) {
        return;
    }
    const RegionPresentation p = present_region(*r, pane_inside(where.rect, sc).rect, sc);
    if (!p.present) {
        return;
    }
    layer.rects.push_back(wire_rect_of(p.shown, kRegionMark));
    if (p.fit.rows > 0 && p.fit.columns > 0) {
        surface::SurfaceTextRegion over = region_over(p.shown);
        over.ground = surface::kGroundBeneath;
        over.rows.push_back(
            surface::SurfaceTextRow{detail::fit(r->text, p.fit.columns), surface::role::kFill});
        layer.texts.push_back(std::move(over));
    }
}

std::int64_t pane_name_columns(std::int64_t heading_columns) {
    const std::int64_t taken =
        static_cast<std::int64_t>(std::char_traits<char>::length(kPaneNamePrompt)) + 1;
    return heading_columns > taken ? heading_columns - taken : 0;
}

// ---- THE PANE EDITOR: a Workshop pane as a SUBJECT, inspected and edited -----------------

std::optional<CatalogRow> pane_editor_subject_row(const Session& s) {
    if (!s.pane_editor.addressed()) {
        return std::nullopt;
    }
    for (const CatalogRow& row : inventory_rows(s.setup.active, s.panels)) {
        if (row.ref == s.pane_editor.subject) {
            return row;
        }
    }
    return std::nullopt;
}

// WL-PED-05 -- agents/workshop/pane-manager.md
FineRect pane_window_base(const Session& s, const PaneRef& ref) {
    FineRect out;
    const std::optional<std::int64_t> kind = resolve_pane(ref, s.panels);
    if (kind.has_value()) {
        out = bounds_of(s.panels, s.setup.active, *kind, screen_of(s)).resolved;
    }
    const SetupPane* row = pane_of(s.setup.active, ref);
    if (row != nullptr && row->place.mode == pane_unit::kSubcells) {
        out.x = row->place.x;
        out.y = row->place.y;
    }
    if (row != nullptr && row->width.mode == pane_unit::kSubcells) {
        out.w = row->width.amount;
    }
    if (row != nullptr && row->height.mode == pane_unit::kSubcells) {
        out.h = row->height.amount;
    }
    return out;
}

// WL-PED-06 -- agents/workshop/pane-manager.md; WL-PANE-08 -- agents/workshop/panes-and-windows.md
Written pane_geometry_typeable(const Session& s, const PaneRef& ref) {
    if (!has_pane(s.setup.active, ref)) {
        return Written::no(ref_text(ref) + " is not in this layout -- open it first");
    }
    const std::optional<std::int64_t> kind = resolve_pane(ref, s.panels);
    if (!kind.has_value()) {
        return Written::no(ref_text(ref) +
                           " is unresolved -- its window cannot be measured; `-` resets an "
                           "axis and the order keys still work");
    }
    if (placement_of(*kind) == placement::kSideRegion) {
        return Written::no(kind_name(s.panels, *kind) +
                           " is in the reserved side column -- the screen owns its place");
    }
    const PanelBounds where = bounds_of(s.panels, s.setup.active, *kind, screen_of(s));
    if (!where.open) {
        return Written::no(kind_name(s.panels, *kind) +
                           " has no room on this screen yet -- `-` resets an axis");
    }
    return Written::ok();
}

std::string pane_axis_text(const Session& s, const PaneRef& ref, std::size_t axis) {
    const SetupPane* row = pane_of(s.setup.active, ref);
    if (row == nullptr) {
        return "--";
    }
    bool projected = false;
    std::string out;
    if (axis < 2) {
        if (row->place.mode != pane_unit::kSubcells) {
            return "-";
        }
        out = geometry_amount_text(axis == 0 ? row->place.x : row->place.y, s.cell_px,
                                   projected);
    } else {
        const PaneSize& size = axis == 2 ? row->width : row->height;
        if (size.mode == pane_unit::kPixels) {
            return std::to_string(size.amount) + "px";
        }
        if (size.mode != pane_unit::kSubcells) {
            return "-";
        }
        out = geometry_amount_text(size.amount, s.cell_px, projected);
    }
    out += " " + std::string(geometry_unit(s.cell_px));
    if (projected) {
        out += kProjectedNote;
    }
    return out;
}

// WL-PED-05 -- agents/workshop/pane-manager.md
Written write_pane_axis(Session& s, const PaneRef& ref, std::size_t axis,
                        const std::string& text) {
    std::string_view body = text;
    while (!body.empty() && body.front() == ' ') {
        body.remove_prefix(1);
    }
    while (!body.empty() && body.back() == ' ') {
        body.remove_suffix(1);
    }
    if (body == "-") {
        if (!has_pane(s.setup.active, ref)) {
            return Written::no(ref_text(ref) + " is not in this layout -- open it first");
        }
        bool moved = false;
        const char* what = "";
        if (axis < 2) {
            moved = reset_pane_place(s.setup.active, ref);
            what = "place";
        } else if (axis == 2) {
            moved = reset_pane_width(s.setup.active, ref);
            what = "width";
        } else {
            moved = reset_pane_height(s.setup.active, ref);
            what = "height";
        }
        if (!moved) {
            return Written::no(ref_text(ref) + " already takes the developer's " + what);
        }
        return Written::ok();
    }
    const Written ready = pane_geometry_typeable(s, ref);
    if (!ready.accepted) {
        return ready;
    }
    const FaceAmount typed = parse_face_amount(body, s.cell_px);
    if (!typed.accepted) {
        return Written::no(typed.refusal);
    }
    const FineRect from = pane_window_base(s, ref);
    PaneAxisProposal horizontal;
    PaneAxisProposal vertical;
    horizontal.base = from.x;
    vertical.base = from.y;
    switch (axis) {
    case 0: horizontal.position = typed.subs; break;
    case 1: vertical.position = typed.subs; break;
    case 2: horizontal.extent = PaneSize{pane_unit::kSubcells, typed.subs}; break;
    default: vertical.extent = PaneSize{pane_unit::kSubcells, typed.subs}; break;
    }
    return author_pane_window(s.setup.active, ref, horizontal, vertical).written;
}

// WL-PED-04 -- agents/workshop/pane-manager.md
std::vector<Row> pane_editor_rows(Session& s) {
    std::vector<Row> rows;
    if (!s.pane_editor.addressed()) {
        return rows;
    }
    Session* sp = &s;
    const PaneRef ref = s.pane_editor.subject;
    const auto found = [sp, ref]() -> std::optional<CatalogRow> {
        for (const CatalogRow& row : inventory_rows(sp->setup.active, sp->panels)) {
            if (row.ref == ref) {
                return row;
            }
        }
        return std::nullopt;
    };
    rows.push_back(Row::show("Name", [found, ref] {
        const std::optional<CatalogRow> row = found();
        return row && row->kind != kNoPaneKind ? row->name : ref.pane;
    }));
    rows.push_back(Row::show("Identity", [ref] { return ref_text(ref); }));
    rows.push_back(Row::show("Provider", [found, ref] {
        const std::optional<CatalogRow> row = found();
        if (ref.provider == kMakerPaneProvider) {
            // A MAKER-MADE PANE'S NAMESPACE, said as what it is: Workshop's own, with no
            // office behind it to be loaded or missing. An unresolved one names the one
            // thing that would resolve it -- a definition file with this name.
            if (!row || row->kind == kNoPaneKind) {
                return ref.provider + " (a pane a maker made -- no open definition is named " +
                       ref.pane + "; --pane <file> opens one)";
            }
            return ref.provider + " (made here -- Pane Creator)";
        }
        if (!row || row->kind == kNoPaneKind) {
            return ref.provider + " (unresolved -- no office here offers it)";
        }
        if (is_runtime_kind(row->kind)) {
            return ref.provider + " (offered this session)";
        }
        return ref.provider + " (built in)";
    }));
    rows.push_back(Row::show("Summary", [found, ref] {
        const std::optional<CatalogRow> row = found();
        return row && row->kind != kNoPaneKind ? row->summary : std::string("--");
    }));
    rows.push_back(Row::section("AUTHORED"));
    static const char* const kAxisLabels[] = {"X", "Y", "Width", "Height"};
    for (std::size_t axis = 0; axis < 4; ++axis) {
        rows.push_back(Row::edit(
            kAxisLabels[axis],
            Property<std::string>([sp, ref, axis] { return pane_axis_text(*sp, ref, axis); },
                                  [sp, ref, axis](std::string text) {
                                      return write_pane_axis(*sp, ref, axis, text);
                                  })));
    }
    rows.push_back(Row::show("Front", [sp, ref] {
        const SetupPane* row = pane_of(sp->setup.active, ref);
        if (row == nullptr) {
            return std::string("--");
        }
        return "f" + std::to_string(row->front) + " of " +
               std::to_string(sp->setup.active.panes.size()) + " -- f/b/r/l order it";
    }));
    rows.push_back(Row::show("Open", [sp, ref] {
        return has_pane(sp->setup.active, ref) ? std::string("yes -- o removes it")
                                               : std::string("no -- o opens it");
    }));
    rows.push_back(Row::section("RESOLVED"));
    rows.push_back(Row::show("Window", [sp, ref] {
        const std::optional<std::int64_t> kind = resolve_pane(ref, sp->panels);
        if (!kind.has_value()) {
            return std::string("-");
        }
        const PanelBounds where =
            bounds_of(sp->panels, sp->setup.active, *kind, screen_of(*sp));
        if (!where.open) {
            return std::string("-");
        }
        if (!where.projected) {
            return std::string("refused -- a pixel axis projects on no medium here");
        }
        return fine_rect_text(where.resolved, sp->cell_px);
    }));
    rows.push_back(Row::show("State", [sp, found] {
        const std::optional<CatalogRow> row = found();
        if (!row) {
            return std::string("-- not in this build's vocabulary nor this layout");
        }
        const std::int64_t state =
            pane_state_of(sp->panels, sp->setup.active, screen_of(*sp), *row);
        std::string out = pane_state_word(state);
        const char* remedy = pane_state_remedy(state);
        if (remedy[0] != '\0') {
            out += std::string(" -- ") + remedy;
        }
        return out;
    }));
    // ---- INTERIOR: what is INSIDE the subject, said honestly for each kind ---------------
    //
    // A MAKER-MADE PANE EXPOSES ITS REGIONS, because regions are what it is made of: the
    // Pane Creator's rows over its one text region -- the text and four fine-lattice
    // numbers, AUTHORED through the definition's own doors -- and the freshly RESOLVED
    // facts beside them. EVERY OTHER PANE IS CODE-BACKED OR SOMEBODY ELSE'S, and the only
    // honest interior this surface can show for one is a read-only CAPTURE of the resolved
    // body: where it is, how many rows of type it holds, in which presentation. No
    // decompilation, no inferred controls, no pretence that a painter is a definition.
    //
    // Which arm a subject gets is decided at REBUILD (when the subject is chosen, or when a
    // definition opens, closes or is replaced) and the rows then read fresh: a maker pane
    // whose definition has since closed reads `--` in every row rather than a stale value.
    rows.push_back(Row::section("INTERIOR"));
    const std::optional<std::int64_t> resolved_now = resolve_pane(ref, s.panels);
    if (resolved_now.has_value() && is_maker_kind(*resolved_now) &&
        !s.panels.maker.definition.regions.empty()) {
        const std::int64_t region_id = s.panels.maker.definition.regions.front().id;
        rows.push_back(Row::show("Region", [sp, ref, region_id] {
            return maker_region(*sp, ref, region_id) == nullptr
                       ? std::string("--")
                       : "#" + std::to_string(region_id) + " text -- the Pane Creator's subject";
        }));
        rows.push_back(Row::edit(
            "Text",
            Property<std::string>(
                [sp, ref, region_id] {
                    const TextRegion* r = maker_region(*sp, ref, region_id);
                    return r == nullptr ? std::string("--") : r->text;
                },
                [sp, ref, region_id](std::string text) {
                    return write_region_text(*sp, ref, region_id, std::move(text));
                })));
        for (std::size_t axis = 0; axis < 4; ++axis) {
            rows.push_back(Row::edit(
                kAxisLabels[axis],
                Property<std::string>(
                    [sp, ref, region_id, axis] {
                        return region_axis_text(*sp, ref, region_id, axis);
                    },
                    [sp, ref, region_id, axis](std::string text) {
                        return write_region_axis(*sp, ref, region_id, axis, text);
                    })));
        }
        rows.push_back(Row::show("Resolved", [sp, ref, region_id] {
            return region_resolved_text(*sp, ref, region_id);
        }));
        rows.push_back(Row::show("Shown", [sp, ref, region_id] {
            return region_shown_text(*sp, ref, region_id);
        }));
    } else {
        rows.push_back(Row::show("Interior", [sp, ref] { return interior_capture_text(*sp, ref); }));
    }
    return rows;
}

std::size_t pane_editor_focus(const Session& s) {
    for (std::size_t i = 0; i < s.pane_editor.rows.size(); ++i) {
        if (s.pane_editor.rows[i].editing()) {
            return i;
        }
    }
    return s.pane_editor.row_cursor;
}

PaneEditorBodyPlace pane_editor_body_place(const FineRect& outer, const Screen& sc,
                                           std::size_t total_panes,
                                           std::size_t pane_cursor,
                                           std::size_t total_fields,
                                           std::size_t field_focus) {
    PaneEditorBodyPlace p;
    const PaneInside inside = pane_inside(outer, sc);
    const FineRect panel = inside.rect;
    if (panel.w <= 0 || panel.h <= 0) {
        return p;
    }
    const surface::SurfaceRect wire = wire_rect_of(panel, surface::role::kFill);
    p.region_x = wire.x;
    p.region_y = wire.y;
    p.region_w = wire.w;
    p.region_h = wire.h;
    p.region_sub_x = wire.sub_x;
    p.region_sub_y = wire.sub_y;
    p.region_sub_w = wire.sub_w;
    p.region_sub_h = wire.sub_h;
    p.fit = inside.fit;
    p.columns = p.fit.columns;
    const std::int64_t used = kPropertyMarkCols + kPropertyLabelCols;
    if (surface::cell_of_subs(surface::add_cells(panel.x, panel.w)) -
            surface::cell_of_subs(panel.x) <=
        used) {
        return p;
    }
    p.value_columns = p.fit.columns - used - kPropertyCaretCols;
    if (p.value_columns < 0) {
        p.value_columns = 0;
    }
    p.capacity = p.fit.rows > kPaneEditorHeadingRows
                     ? static_cast<std::size_t>(p.fit.rows - kPaneEditorHeadingRows)
                     : 0;
    if (p.capacity < 2) {
        return p; // one row of each list is the smallest body that says anything
    }
    const BodyShare share =
        share_body_rows(p.capacity, list_demand(total_panes), list_demand(total_fields));
    p.panes_rows = share.objects;
    p.field_rows = share.properties;
    p.panes = list_window(total_panes, pane_cursor, p.panes_rows);
    p.fields = list_window(total_fields, field_focus, p.field_rows);
    p.present = true;
    return p;
}

PaneEditorBodyPlace pane_editor_body(const Session& s, const Screen& sc,
                                     const FineRect& outer) {
    return pane_editor_body_place(outer, sc,
                                  inventory_rows(s.setup.active, s.panels).size(),
                                  s.pane_editor.cursor, s.pane_editor.rows.size(),
                                  pane_editor_focus(s));
}

std::int64_t prose_row_of_editor_pane(const PaneEditorBodyPlace& p, std::size_t index) {
    return p.present ? prose_row_in_window(p.panes, 0, index) : kNoProseRow;
}

std::size_t editor_pane_at_prose_row(const PaneEditorBodyPlace& p, std::int64_t row) {
    std::size_t at = 0;
    if (!p.present || !item_at_prose_row(p.panes, 0, p.panes_rows, row, at)) {
        return kNoObject;
    }
    return at;
}

std::int64_t prose_row_of_field(const PaneEditorBodyPlace& p, std::size_t index) {
    if (!p.present) {
        return kNoProseRow;
    }
    return prose_row_in_window(p.fields, static_cast<std::int64_t>(p.panes_rows), index);
}

std::size_t field_at_prose_row(const PaneEditorBodyPlace& p, std::int64_t row) {
    std::size_t at = 0;
    if (!p.present || !item_at_prose_row(p.fields, static_cast<std::int64_t>(p.panes_rows),
                                         p.field_rows, row, at)) {
        return kNoProperty;
    }
    return at;
}

PaneEditorAt pane_editor_at(const Session& s, std::int64_t space, std::int64_t x,
                            std::int64_t y) {
    const Screen sc = screen_of(s);
    const PanelBounds where = bounds_of(s.panels, s.setup.active, panel::kPaneEditor, sc);
    if (!where.open) {
        return PaneEditorAt{};
    }
    PaneEditorAt out;
    out.body = pane_editor_body(s, sc, where.rect);
    out.at = prose_at(space, x, y, out.body.region_x, out.body.region_y, out.body.fit);
    out.at.row -= kPaneEditorHeadingRows;
    out.present = out.body.present && out.at.understood && out.at.row >= 0;
    return out;
}

void paint_pane_editor(surface::SurfaceLayer& layer, const Session& s,
                       const FineRect& b, const Screen& sc,
                       std::int64_t chrome) {
    paint_panel_frame(layer, b, chrome);
    const std::vector<CatalogRow> panes = inventory_rows(s.setup.active, s.panels);
    const PaneEditor& ed = s.pane_editor;
    const PaneEditorBodyPlace body = pane_editor_body(s, sc, b);
    if (body.fit.rows <= 0 || body.fit.columns <= 0) {
        return;
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
    // THE HEADING SAYS WHAT THIS IS AND WHETHER THE KEYS ARE HERE -- the Files header's
    // `*`, for reason: arrows that stopped meaning command mode's arrows are
    // arrows a maker is entitled to read the reason for.
    std::string heading = "PANE MANAGER";
    if (pane_editor_has_keyboard(s)) {
        heading += " *";
    }
    // THE PANE CREATOR'S NAME PROMPT TAKES THE HEADING ROW WHILE IT IS OPEN: the
    // layout-name editor's own composition -- a prompt, the line's visible window, and the
    // caret and selection as the REGION's own so each face answers in its voice.
    if (s.pane_naming.open) {
        const std::int64_t cols = pane_name_columns(body.fit.columns);
        const std::string shown = s.pane_naming.line.visible(cols);
        const component::TextBox::VisibleSpan vis = s.pane_naming.line.visible_selection(cols);
        const std::int64_t prompt =
            static_cast<std::int64_t>(std::char_traits<char>::length(kPaneNamePrompt));
        const std::int64_t at = static_cast<std::int64_t>(s.pane_naming.line.caret_column());
        region.caret_row = 0;
        region.caret_col = prompt + (at < static_cast<std::int64_t>(shown.size())
                                         ? at
                                         : static_cast<std::int64_t>(shown.size()));
        if (vis.present()) {
            region.sel_begin_row = 0;
            region.sel_begin_col = prompt + vis.begin;
            region.sel_end_row = 0;
            region.sel_end_col = prompt + vis.end;
        }
        heading = std::string(kPaneNamePrompt) + shown;
    }
    region.rows.push_back(
        surface::SurfaceTextRow{detail::fit(heading, body.fit.columns), surface::role::kAccent});
    if (!body.present) {
        layer.texts.push_back(std::move(region));
        return;
    }
    const auto say_row = [&region](std::string text, std::int64_t role,
                                   std::int64_t ground = surface::role::kNone) {
        region.rows.push_back(surface::SurfaceTextRow{std::move(text), role, ground});
    };
    const auto say_omission = [&](std::size_t how_many, const char* which) {
        if (how_many > 0) {
            say_row(detail::fit(omitted_text(how_many, which), body.columns),
                    surface::role::kMuted);
        }
    };
    // ---- the PANES list: the picker's population, the picker's row, plus a subject mark --
    say_omission(body.panes.before, "earlier");
    for (std::size_t n = 0; n < body.panes.count; ++n) {
        const std::size_t i = body.panes.first + n;
        const CatalogRow& row = panes[i];
        const bool here = !ed.on_rows && i == ed.cursor;
        const bool subject = ed.addressed() && row.ref == ed.subject;
        say_row(std::string(here ? ">" : " ") + (subject ? "*" : " ") +
                    detail::fit(picker_entry_text(
                                    row.name,
                                    pane_state_word(pane_state_of(s.panels, s.setup.active,
                                                                  sc, row)),
                                    row.summary),
                                body.columns - 2),
                here || subject ? surface::role::kAccent : surface::role::kFill);
    }
    say_omission(body.panes.after, "more");
    while (region.rows.size() <
           static_cast<std::size_t>(kPaneEditorHeadingRows) + body.panes_rows) {
        say_row(std::string(), surface::role::kFill);
    }
    // ---- the subject's rows ------------------------------------------------------------
    if (ed.rows.empty()) {
        say_row(detail::fit(ed.addressed() ? "(the subject is gone -- choose a pane above)"
                                           : "(no subject -- choose a pane above)",
                            body.columns),
                surface::role::kMuted);
        layer.texts.push_back(std::move(region));
        return;
    }
    say_omission(body.fields.before, "earlier");
    for (std::size_t n = 0; n < body.fields.count; ++n) {
        const std::size_t i = body.fields.first + n;
        const Row& row = ed.rows[i];
        if (row.section()) {
            // A SECTION IS A BOUNDARY, said the way `PROPERTIES` is said: accent ink on
            // the one ground every ink reads on.
            say_row(detail::fit(row.label(), body.columns), surface::role::kAccent,
                    surface::role::kMuted);
            continue;
        }
        const bool here = ed.on_rows && i == ed.row_cursor;
        std::int64_t role = surface::role::kFill;
        if (row.editing()) {
            role = surface::role::kAlert;
        } else if (!row.editable()) {
            role = surface::role::kMuted;
        }
        say_row(property_row_text(row, here, body.value_columns), role);
        if (row.editing()) {
            region.caret_row = kPaneEditorHeadingRows + prose_row_of_field(body, i);
            region.caret_col = property_caret_column(row);
            const TerminalSelectionSpan marked =
                property_selection_columns(row, body.value_columns);
            if (marked.present) {
                region.sel_begin_row = region.caret_row;
                region.sel_begin_col = marked.begin;
                region.sel_end_row = region.caret_row;
                region.sel_end_col = marked.end;
            }
        }
    }
    say_omission(body.fields.after, "more");
    layer.texts.push_back(std::move(region));
}

// WL-FRONT-01, WL-FRONT-05, WL-FRONT-07 -- agents/workshop/planes.md
// WL-MAKER-05 -- agents/workshop/maker-pane.md
void paint_panels(surface::SurfaceCanvas& c, const WorkshopDoc& d, const Session& s,
                  const Screen& sc, const ProjectFrontier& frontier) {
    const Panels& panels = s.panels;
    const std::int64_t lifted = selected_pane(panels);
    for (const std::int64_t kind : effective_pane_order(s.setup.active, panels)) {
        const Panel p{kind};
        const FineRect b = bounds_of(panels, s.setup.active, p.kind, sc).rect;
        if (b.w <= 0 || b.h <= 0) {
            continue;
        }
        const std::int64_t chrome = p.kind == lifted ? kPaneChromeSelected : kPaneChrome;
        detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
            if (p.kind == panel::kBuilder) {
                paint_builder(layer, panels.builder, b, sc, frontier, s.recipes_moved_to,
                              chrome);
            } else if (p.kind == panel::kInfo) {
                paint_info(layer, d, s, b, sc, chrome);
            } else if (p.kind == panel::kEditor) {
                paint_editor(layer, s, b, sc, chrome);
            } else if (p.kind == panel::kProjectFiles) {
                paint_files(layer, s, b, sc, s.keymap, chrome);
            } else if (p.kind == panel::kLayouts) {
                // THE LAYOUT RUN, THE SETUP ASSOCIATION AND THE WORKSPACE FACT --
                // one more arm, in the one walk, and that is the whole of what the
                // conversion cost this function. What it BUYS is the two lines above it:
                // the rectangle is `bounds_of`'s, the order is `effective_pane_order`'s,
                // and a pane a maker put in front of this one is drawn over it.
                paint_layouts(layer, s, b, sc, chrome);
            } else if (p.kind == panel::kPaneEditor) {
                paint_pane_editor(layer, s, b, sc, chrome);
            } else if (is_maker_kind(p.kind)) {
                // THE MAKER'S OWN PANE -- one more arm in the one walk, and that
                // is the whole of what a pane made of DATA costs this function. Its
                // rectangle is `bounds_of`'s, its order is `effective_pane_order`'s, its
                // chrome is the same chrome, and the only thing this arm decides is which
                // painter: the one that reads an authored interior instead of composing one.
                paint_maker_pane(layer, s, b, sc, chrome);
            } else if (is_runtime_kind(p.kind)) {
                // ONE GENERIC ARM FOR EVERY EXTERNAL PANE, and there is no second one to
                // add. The branch above chooses a PAINTER, which placement named as the one
                // thing about a panel kind that genuinely cannot be shared -- and this arm
                // is the case where it can be, because every external pane is presented
                // identically: a header Workshop writes and a region the provider fills. A
                // second provider costs this function nothing at all.
                paint_external(layer, panels, p.kind, b, sc, s.pane_titles, chrome);
            }
        });
    }
    // THE PANE CREATOR'S REGION MARK: over the panes, in the affordances' own
    // position and for their reason -- it says which rectangle of the maker's pane the
    // rows they are editing describe, derived from the same resolution that painted it, and
    // it is drawn on a plane of its own so the pane's own interior cannot cover it.
    detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
        paint_creator_region_mark(layer, s, sc);
    });
    detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
        paint_pane_affordances(layer, s, sc);
    });
    detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
        paint_picker(layer, panels, s.setup.active, sc, s.keymap);
    });
    // THE CURRENT-CONDITION VIEW, IN THE PICKER'S OWN PLANE: over the panes it
    // covers, under the screen's own chrome. The band keeps speaking while it is open --
    // what a maker is READING is what is currently true, and what the band SAYS is what
    // just happened, and those are two different sentences that must not cover each other.
    detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
        paint_attention(layer, s, sc, frontier);
    });
    // THE CONTEXTUAL-ACTION SURFACE, LAST IN THE BAND: over the picker and the
    // attention view, because it is the band's later, more deliberate gesture -- and it
    // takes the band's keys first for the same reason (`keyboard_context`), so what is
    // frontmost and what answers agree.
    detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
        paint_context(layer, s, sc);
    });
}

surface::SurfaceTextRegion band_region(const Session& s, const Screen& sc) {
    const ui::Rect b = band_bounds(sc);
    const surface::RegionFit fit = band_fit(sc);
    surface::SurfaceTextRegion band;
    band.x = b.x;
    band.y = b.y;
    band.w = b.w;
    band.h = b.h;
    const std::int64_t budget = fit.rows;
    const std::int64_t columns = fit.columns;
    if (budget <= 0 || columns <= 0) {
        return band;
    }

    const std::string notice = s.notice.empty() ? std::string() : detail::fit(s.notice, columns);
    const std::int64_t notice_role =
        s.notice_is_bad ? surface::role::kAlert : surface::role::kFill;

    // THE LEGEND TAKES WHAT THE NOTICE LEAVES, which is this band's whole composition policy
    // now that the identity has its own band. A character medium's four rows are the
    // notice and three of legend where the context has that many pairs; the shipped face's
    // two are the notice and one, which is exactly the pair it read before the split. No
    // reserved row is spare: the band spent the old blank row on the workspace fact and this
    // keeps that discipline rather than handing one back.
    //
    // While an external pane holds the keyboard and the legend is FULL, the first legend row
    // still says so -- that sentence is keyboard-ownership truth, not a binding list,
    // and where the legend has one row the sentence takes it and the chorded survivors follow
    // in whatever room is left.
    const std::size_t legend_rows =
        budget >= 2 ? static_cast<std::size_t>(budget - 1) : 0;
    std::vector<std::string> legend;
    if (legend_rows > 0) {
        const KeyContext ctx = keyboard_context(s);
        const std::int64_t typing = keyboard_pane(s.panels);
        const RuntimePane* typed_into =
            typing == kNoPaneKind ? nullptr : s.panels.runtime.of_kind(typing);
        // THE SOURCE EDITOR IS THE SECOND KEYBOARD-TAKING PANE, and it gets the same
        // sentence for the same measured reason: keystrokes landing somewhere
        // the screen does not name is the lie this row exists to refuse.
        std::string said;
        if (typed_into != nullptr && s.keymap.resolved_legend() == legend_mode::kFull) {
            said = "typing goes to " + typed_into->name + " @" + typed_into->provider +
                   " -- press elsewhere for Workshop's keys";
        } else if (ctx == KeyContext::kEditor &&
                   s.keymap.resolved_legend() == legend_mode::kFull) {
            said = "typing goes to the source editor -- press elsewhere for Workshop's keys";
        } else if (ctx == KeyContext::kFiles &&
                   s.keymap.resolved_legend() == legend_mode::kFull) {
            // THE BROWSER TAKES KEYS WITHOUT TAKING TEXT, so the sentence says KEYS. The
            // row exists for the same measured reason the two above it do: a maker whose
            // arrows have stopped meaning what they mean in command mode is entitled to
            // read why on the screen rather than infer it from a gesture that did nothing.
            said = "keys go to Project Files -- press elsewhere for Workshop's keys";
        } else if (ctx == KeyContext::kPaneEditor &&
                   s.keymap.resolved_legend() == legend_mode::kFull) {
            said = "keys go to the Pane Manager -- press elsewhere for Workshop's keys";
        }
        if (!said.empty()) {
            if (legend_rows == 1) {
                const std::int64_t rest =
                    columns - static_cast<std::int64_t>(said.size()) - 3;
                const std::vector<std::string> pairs = help_rows(s.keymap, ctx, rest, 1);
                legend.push_back(detail::fit(
                    pairs.empty() ? said : said + " | " + pairs.front(), columns));
            } else {
                legend.push_back(detail::fit(said, columns));
                const std::vector<std::string> pairs =
                    help_rows(s.keymap, ctx, columns, legend_rows - 1);
                for (const std::string& row : pairs) {
                    legend.push_back(row);
                }
            }
        } else {
            legend = help_rows(s.keymap, ctx, columns, legend_rows);
        }
    }

    const auto push = [&band](std::string text, std::int64_t role) {
        band.rows.push_back(surface::SurfaceTextRow{std::move(text), role});
    };
    if (budget >= 2) {
        push(notice, notice_role);
        for (std::string& row : legend) {
            push(std::move(row), surface::role::kMuted);
        }
    } else if (!notice.empty()) {
        // One row: the tool's own voice while it has something to say. The identity line is
        // not a candidate here any more -- it has a band of its own that this budget cannot
        // take away.
        push(notice, notice_role);
    } else {
        const std::vector<std::string> pairs =
            help_rows(s.keymap, keyboard_context(s), columns, 1);
        if (!pairs.empty()) {
            push(pairs.front(), surface::role::kMuted);
        }
    }
    return band;
}

// WL-FRONT-01, WL-FRONT-07 -- agents/workshop/planes.md
// WL-ATTN-04 -- agents/workshop/attention.md
// WL-DOC-18 -- agents/workshop/document.md
// WL-RGN-05 -- agents/workshop/regions.md
surface::SurfaceCanvas paint(const WorkshopDoc& d, const Session& s,
                             const ProjectFrontier& frontier) {
    const Screen sc = screen_of(s);
    surface::SurfaceCanvas c;
    c.width = sc.w;
    c.height = sc.h;

    // THE WORKSPACE PLANE: what a maker authored, as this workspace places it.
    // It is written whole before any pane is, because a pane is a presentation IN FRONT of
    // the document -- which is what `occupied_at` has answered and what the
    // picture now agrees with instead of merely being told.
    //
    // THE SCREEN'S OWN CHROME IS NOT HERE. It is a plane of its own, added after the panes,
    // for a reason worth stating where both are decided: the bottom band is where the tool
    // SPEAKS, and a panel's backdrop painted over it would take the notice that just told a
    // maker what happened and erase it under the furniture it describes. (The shared top
    // row that used to be this note's other half is retired -- see the band below.)
    //
    // A REFERENCE INTO `c.layers` IS SPENT BEFORE ANY OTHER LAYER IS ADDED. That is not a
    // coincidence to be preserved by care: `paint_panels` is the first thing that grows the
    // vector, and the two lambdas below are the only writers before it.
    c.layers.emplace_back();
    surface::SurfaceLayer* on = &c.layers.back();

    const auto rect = [&on](std::int64_t x, std::int64_t y, std::int64_t w, std::int64_t h,
                            std::int64_t role) {
        on->rects.push_back(surface::SurfaceRect{x, y, w, h, role});
    };
    const auto label = [&on](std::int64_t x, std::int64_t y, std::string text,
                             std::int64_t role) {
        on->labels.push_back(surface::SurfaceLabel{x, y, std::move(text), role});
    };

    // The workspace, as a thing with edges a maker can see. Its extent is a
    // session fact, so resizing it visibly changes what a share resolves to
    // while changing no authored value at all.
    rect(kWorkspaceX, kWorkspaceY, s.workspace_w, s.workspace_h, surface::role::kMuted);

    // The scene: the authored elements, as this workspace places them. Painting
    // walks the SCENE, not the document -- so a rectangle on screen is by
    // construction a rectangle the hit test can find.
    const ui::Scene scene = workspace_scene(d, s);
    for (const ui::Placed& p : scene.items) {
        const std::int64_t x = kWorkspaceX + p.rect.x;
        const std::int64_t y = kWorkspaceY + p.rect.y;
        if (p.id == s.selected) {
            rect(x - 1, y - 1, p.rect.w + 2, p.rect.h + 2, surface::role::kAccent);
        }
        rect(x, y, p.rect.w, p.rect.h, surface::role::kFill);
        // The label, written on the object, clipped to the workspace rather than
        // allowed to run into the panel beside it. The label is authored, so it
        // is read from the element and not from the observation of it.
        //
        // AND IT IS SEMANTIC TYPE ON MATERIAL SOMEBODY ELSE OWNS, which is the
        // one place in this tool where that sentence has to be argued rather than assumed.
        // The name is semantic -- it is the maker's word for this object and its exact cell
        // occupancy is no part of what they authored -- so it belongs in a bounded region.
        // Its rectangle, though, is already full: the object's body is authored MATERIAL,
        // drawn one line up as a `SurfaceRect`. An ordinary region over it erases that
        // material in both media, and rows carrying the object's role as a GROUND leave a
        // `12h - 4 - 18*rows` pixel band the strips cannot reach (10 px across the foot of a
        // default 12x4 object; `12h - 4 == 18k` has no integer solutions, so SOME remainder
        // exists at every height). Both were built and run live, twice -- once at first and
        // once again to re-measure them. `surface::kGroundBeneath` is the third
        // answer: the region keeps its bounds, so the name is fitted and cut against them,
        // and gives up the ground, so nothing under it is painted over.
        //
        // THE BOUND IS THE OBJECT'S OWN RESOLVED WIDTH, clipped by the workspace's
        // right edge -- and earlier it was only the second of those. The name used to be
        // given `workspace_w - x` cells, so a name longer than the object it names ran out of
        // it and across the backdrop; the re-measure preserved that deliberately and then MEASURED
        // what it costs, which is the paragraph below. The room is the material's, because
        // this is type ON material and material the object does not have is not this name's
        // room to spend. The workspace clip stays because it answers a different question --
        // an object may be authored wider than the room to the edge, and its name is still
        // not the panel's to write into.
        //
        // ...OR ONE COLUMN, WHICHEVER IS MORE, for the row floor's reason said about the
        // other axis (below): a zero-WIDTH object is reachable from a poke or a hand-built
        // document exactly as a zero-height one is, and one cell of room leaves `detail::fit`
        // a mark to put there rather than leaving the object with no trace at all.
        //
        // WHAT A MEDIUM STILL GETS TO SAY IS HOW MANY CHARACTERS THOSE CELLS HOLD, and that
        // half is and unchanged: `fit_region` answers 12 columns in cells and 17
        // columns of a 13pt face for a 12-cell object, so a name is marked when it genuinely
        // did not fit rather than when it would not have fitted as bitmap cells.
        //
        // AND ITS HEIGHT IS THE OBJECT'S, which is what makes a one-cell object honest for
        // free. `fit_region` sends a region with no room for a row of the medium's face back
        // to the cell projection, so an object a maker sized to one cell shows its
        // name in cells -- the same picture a terminal shows -- rather than 18 pixels of type
        // hanging out of a 12-pixel object. No `if (h < N)` was written here; the rule is the
        // one both media already resolve with.
        //
        // ...OR ONE ROW, WHICHEVER IS MORE, and that floor is not a fudge: a name is written
        // ON a row, so the room it needs is a row, and an object whose resolved height is
        // zero still has the row its origin is on. `check_extent` refuses an authored height
        // below one cell, so this is reachable only from a poke or a hand-built document --
        // but it WAS reachable earlier and such an object's name was the only trace of
        // it on the workspace, and a region with no bounds shows nothing and says nothing
        // about it. Measured: without the floor, three zero-height objects lost their names
        // outright. The floor restores byte-for-byte the run of cells the label drew, in
        // every medium, because one cell of room is a cell region either way.
        //
        // THE CUT IS MARKED, and earlier it was not. `resize` here was a silent
        // truncation of a string a MAKER chose (up to `doc::kMaxNameLen`), which is the exact
        // defect found in the picker's name column and repaired the same way: a shorter
        // name that looks finished is a lie about the document. `detail::fit` marks it.
        //
        // AND WHY THE ROOM IS THE MATERIAL'S, WRITTEN HERE BECAUSE IT IS THIS CALL SITE'S.
        // The re-measure found the cost of the old bound in a medium that paints roles as ink: the
        // name is `kMuted` so it reads quietly on the object's `kFill` body, and the workspace
        // backdrop a few statements up is ALSO `kMuted` -- so every character past the
        // object's own edge was the backdrop's exact colour and could not be read at all. Six
        // cells of material and a thirty-two byte name meant 9 characters legible and 23
        // invisible, measured on the pristine tree. Earlier the overhang was legible
        // only for a reason nobody chose: every label cell was cleared to the canvas
        // background first, which is the same hole in the workspace that it was in the object.
        //
        // NO ROLE FIXES THAT, WHICH IS WHY THE ANSWER IS THE BOUND. This medium's inks are
        // kFill 176, kAccent 112/232/240, kMuted 96 and kAlert red: nothing reads on BOTH a
        // `kFill` body and a `kMuted` backdrop, `kAccent` means "the one thing being pointed
        // at" and would make every object shout, and a fifth role is exactly what
        // `surface/vocabulary.hpp` refuses. Contrast is a palette question and the palette is
        // the medium's -- which is the whole reason a publisher ships roles. So the repair is
        // not a colour and not a ground: it is that a name never leaves the material it names,
        // and where it does not fit that material it says so with `detail::fit`'s mark. The
        // authored name is untouched by any of it, and widening the object reveals more of the
        // same authored bytes -- which is the property the whole arrangement is for.
        const ui::Element* authored = doc::find(d, p.id);
        const std::int64_t columns = p.rect.w > 1 ? p.rect.w : 1;
        const std::int64_t to_edge = s.workspace_w - p.rect.x;
        const std::int64_t room = columns < to_edge ? columns : to_edge;
        if (authored != nullptr && room > 0) {
            const std::int64_t rows = p.rect.h > 1 ? p.rect.h : 1;
            const surface::RegionFit fit =
                surface::fit_region(x, y, room, rows, sc.text_advance_px, sc.text_line_px);
            surface::SurfaceTextRegion named;
            named.x = x;
            named.y = y;
            named.w = room;
            named.h = rows;
            named.ground = surface::kGroundBeneath;
            named.rows.push_back(surface::SurfaceTextRow{
                detail::fit(authored->label, fit.columns), surface::role::kMuted});
            on->texts.push_back(std::move(named));
        }
    }

    // The size handle, over everything in the workspace, as a GLYPH rather than
    // as another rectangle. That is not decoration: the ring already paints this
    // exact cell in the accent role, so a rect here would be invisible, and the
    // affordance has to be distinguishable from the ring, from the object's body
    // and from the workspace at a glance. `SurfaceLabel` carries arbitrary text
    // over the rects, so the generic canvas vocabulary already had what this
    // needed -- no role was added, and nothing in surface/ or ui/ changed.
    // (Honest cost: a Skin with no text stack draws no handle. Both shipped
    // media have one -- a terminal's own font, and the SDL medium's bitmap face
    // in surface/skin_sdl_glyphs.hpp -- so nothing declines it today.)
    const Handle handle = size_handle(d, s);
    if (handle.shown) {
        label(kWorkspaceX + handle.x, kWorkspaceY + handle.y, kHandleGlyph,
              surface::role::kAccent);
    }

    // THE DYNAMIC PANELS -- every one of them, INCLUDING the OBJECTS and PROPERTIES columns
    // a maker has always read on the right. Each takes a PLANE of its own, in canonical
    // front order, so a later-ranked pane covers an earlier one kind for kind.
    //
    // THIS ONE CALL IS THE WHOLE OF A REMOVABLE INFO AT THIS LEVEL. What used to be forty lines of
    // furniture painted unconditionally here is now a panel like any other: present because a
    // fresh session opens it, absent the moment a maker removes it, and painted by whoever
    // owns that kind rather than by `paint`.
    paint_panels(c, d, s, sc, frontier);

    // AND THE SCREEN'S OWN CHROME OVER THEM, on its own plane -- which is a
    // budget-composed region rather than one label per cell row, and is ONE of
    // them: the bottom band, where the tool speaks and where the keys are explained. See the
    // note at the top of this function for why it is in front rather than behind: a band is
    // where the tool SPEAKS, and a panel backdrop drawn over one would erase the notice that
    // just told a maker what happened.
    //
    // ⚠ THE TOP BAND IS NOT HERE ANY MORE. The layout selector, the setup
    // association and the workspace fact were the other half of this plane and are an
    // ordinary pane now -- painted by `paint_panels` above, in canonical front order, over
    // and under whatever a maker arranged around them. The ROWS they defaulted to are still
    // reserved (`kTopRows`, and `room_h` is byte-identical either way); what changed is that
    // something authorable stands on them instead of something this function drew.
    //
    // THE OLD SHARED TOP ROW IS STILL RETIRED, AND ITS CELL IS SPENT NOW.
    // Canvas row 0 carried four one-cell voices -- the workspace's extent, the picker and
    // window hints, the terminal hint -- each structurally unable to hold a row of a real
    // face. The band conversion moved those facts into the band and left the row EMPTY, because the
    // workspace's extent is what a share resolves against and a chrome retirement must not
    // resize a maker's document. The split spends that cell, and one more from the bottom band,
    // on a top band two cells tall -- which is what a face needs for one row of type. The
    // reserved total is what it was, so the workspace still did not move.
    //
    // ⚠ THE BOTTOM BAND BELONGS TO THE OVERLAY WHILE THAT IS OPEN, AND THE LAYOUTS PANE
    // DOES NOT. The Terminal is anchored to the bottom-right corner and covers most of the
    // screen's width at every extent, so bottom-band rows painted underneath it would
    // survive only in the cells to its left -- a sentence beheaded mid-word with nothing to
    // say so. The Layouts pane's default rows are ones the overlay cannot reach:
    // `terminal_y` is `h - terminal_h`, which is 9 at the minimum screen and grows with the
    // surface, so it is never less than `kTopRows`. A maker in the Terminal therefore keeps
    // reading which layout they are in, which is the honest answer rather than a courtesy --
    // those rows are not covered, so hiding them would be a lie about occlusion. A maker who
    // MOVED the pane under the overlay is covered by it and correctly so, which is a thing
    // this screen could not say at all until the conversion.
    //
    // A REGION TAKES ITS RECTANGLE, and that is a deliberate widening over the labels it
    // replaced: the old rows cleared only the cells their characters landed on, and a band
    // clears all of its rows across the canvas. A pane a maker authors over the bottom band
    // is covered BY it, because the panes are in front of the DOCUMENT and not in front of
    // the tool's own voice, and the band occupies no pointer space at all.
    //
    // ⚠ THAT LAST EXEMPTION USED TO HAVE AN EXCEPTION AND NO LONGER DOES. The top
    // band painted in front of every pane and answered presses on the layout tabs alone, so
    // a pane dragged under it was visually erased and still met the hand -- see-here,
    // press-there, at exactly the boundary one geometry exists to forbid. Both halves are gone: the
    // tabs are a pane's interior, and `occupied_at` answers that pane for those cells like
    // any other.
    if (!s.terminal.open) {
        detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
            layer.texts.push_back(band_region(s, sc));
        });
    }

    if (s.terminal.open) {
        // THE FINAL MODAL PLANE, and that is the whole of what "overlay" means here. A pane
        // in the last layer covers whatever it lands on -- and the screen underneath is
        // composed exactly as it was before this phase, with no row budget taken from it and
        // no constant moved. A closed pane appends no layer at all.
        detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
            paint_terminal(layer, s.terminal, sc, s.keymap);
        });
    }

    // THE HOTKEY VIEW, LATER STILL: a maker can open it OVER the Terminal to read
    // the Terminal line's own keys, so it must be readable above the pane whose context it
    // is describing. It is the one plane after the Terminal's, and it is a projection --
    // the screen beneath it, the Terminal included, is composed exactly as if it were
    // closed, which is also why the context it reports is the context beneath it.
    if (s.hotkeys.open) {
        detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
            paint_hotkeys(layer, s, sc);
        });
    }

    return c;
}

} // namespace zengine::workshop
