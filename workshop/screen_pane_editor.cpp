// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `screen.hpp`'s sections -- a maker-made pane, presented, and the pane editor --
// compiled once into `zengine-workshop-logic` and linked by the host and every suite; the
// declarations, the constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/pane-manager.md (+2 registers; agents/workshop.md routes)

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

} // namespace zengine::workshop
