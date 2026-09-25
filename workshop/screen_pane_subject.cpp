// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The screen's maker-made pane, presented, and a pane as a subject: the rows an inspector reads it
// by and the doors those rows write through.
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
    if (!m.open() || m.definition.regions.empty()) {
        return nullptr;
    }
    // The pane an inspector has named, and nothing else: the region is marked while the maker's
    // pane is the subject being read.
    if (!s.inspected.addressed() || !(s.inspected.ref == maker_pane_ref(m.definition.name))) {
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

// ---- A WORKSHOP PANE AS A SUBJECT, inspected and edited through its owners (Info's) ------

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
    // No refusal for the right column: the screen reserves nothing, so typed geometry reaches the
    // pane standing there like any other.
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

// WL-INFO-14 -- agents/workshop/info-body.md
std::int64_t inspected_region(const Session& s, const PaneRef& ref) {
    const std::optional<std::int64_t> kind = resolve_pane(ref, s.panels);
    if (!kind.has_value() || !is_maker_kind(*kind) || s.panels.maker.definition.regions.empty()) {
        return 0;
    }
    return s.panels.maker.definition.regions.front().id;
}

// WL-INFO-14 -- agents/workshop/info-body.md
PaneSubjectShown pane_subject_shown(const Session& s) {
    PaneSubjectShown shown;
    const InspectedPane& in = s.inspected;
    if (!in.addressed()) {
        return shown;
    }
    shown.office = in.ref.provider;
    shown.pane = in.ref.pane;
    shown.name = in.ref.pane;
    for (const CatalogRow& row : inventory_rows(s.setup.active, s.panels)) {
        if (row.ref == in.ref && row.kind != kNoPaneKind) {
            shown.name = row.name;
            break;
        }
    }
    shown.subject = in.name;
    // THE ROWS AS THEY STAND, VALUE INCLUDED: `Row::value()` is a fresh read through the
    // owner's property, so what crosses is what the desk says at this instant.
    for (const Row& row : in.rows) {
        shown.properties.push_back(
            ShownProperty{row.label(), row.value(), row.editable(), row.section()});
    }
    return shown;
}

bool same_pane_subject(const PaneSubjectShown& a, const PaneSubjectShown& b) {
    if (a.office != b.office || a.pane != b.pane || a.name != b.name || a.subject != b.subject ||
        a.properties.size() != b.properties.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.properties.size(); ++i) {
        const ShownProperty& x = a.properties[i];
        const ShownProperty& y = b.properties[i];
        if (x.label != y.label || x.value != y.value || x.editable != y.editable ||
            x.section != y.section) {
            return false;
        }
    }
    return true;
}

// WL-INFO-14 -- agents/workshop/info-body.md
std::vector<Row> pane_subject_rows(Session& s, const PaneRef& ref) {
    std::vector<Row> rows;
    if (ref.provider.empty()) {
        return rows;
    }
    Session* sp = &s;
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
    // THE FACTS ALONE, NAMING NO KEY: the reader of these rows holds its own keys, and a row
    // telling it to press one that means nothing where it is would be a second, wrong cheat
    // sheet. Ordering is the arrangement's, and opening and closing are the Pane Manager's.
    rows.push_back(Row::show("Front", [sp, ref] {
        const SetupPane* row = pane_of(sp->setup.active, ref);
        if (row == nullptr) {
            return std::string("--");
        }
        return "f" + std::to_string(row->front) + " of " +
               std::to_string(sp->setup.active.panes.size());
    }));
    rows.push_back(Row::show("Open", [sp, ref] {
        return has_pane(sp->setup.active, ref) ? std::string("yes") : std::string("no");
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
    // ---- INTERIOR: what is inside the subject, said honestly for each kind ---------------
    // A maker-made pane exposes its regions (the Pane Creator's rows: authored text and four
    // fine-lattice numbers, beside the resolved facts); any other pane gets a read-only capture
    // of its resolved body, never inferred controls. The arm is chosen at rebuild; rows read fresh.
    rows.push_back(Row::section("INTERIOR"));
    const std::optional<std::int64_t> resolved_now = resolve_pane(ref, s.panels);
    if (resolved_now.has_value() && is_maker_kind(*resolved_now) &&
        !s.panels.maker.definition.regions.empty()) {
        const std::int64_t region_id = s.panels.maker.definition.regions.front().id;
        rows.push_back(Row::show("Region", [sp, ref, region_id] {
            return maker_region(*sp, ref, region_id) == nullptr
                       ? std::string("--")
                       : "#" + std::to_string(region_id) + " text -- the Pane Creator made it";
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

} // namespace zengine::workshop
