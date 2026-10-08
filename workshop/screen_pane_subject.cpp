// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// A pane as a subject: the rows an inspector reads it by, the doors those rows write through, and
// the read-only capture of its interior.
// Workshop law: agents/workshop/pane-manager.md (+2 registers; agents/workshop.md routes)

#include "screen.hpp"

namespace zengine::workshop {

// WL-MAKER-12 -- agents/workshop/maker-pane.md
std::string interior_capture_text(const Session& s, const PaneRef& ref) {
    const std::optional<std::int64_t> kind = resolve_pane(ref, s.panes);
    if (!kind.has_value()) {
        return "unresolved -- nothing to inspect";
    }
    const Screen sc = screen_of(s);
    const PaneBounds where = bounds_of(s.panes, s.setup.active, *kind, sc);
    const char* whose = is_runtime_kind(*kind) ? "a provider's own" : "code-backed";
    if (!where.open || where.rect.empty()) {
        return std::string(whose) + " -- not presented; no authored interior";
    }
    const ProsePlace place = prose_place(where.rect, sc);
    if (!place.present) {
        return std::string(whose) + " -- body " + pixel_rect_text(room_of_canvas(place.inside, sc), s.cell_px) +
               ", no room for a row; no authored interior";
    }
    return std::string(whose) + " -- body " + pixel_rect_text(room_of_canvas(place.inside, sc), s.cell_px) + ", " +
           std::to_string(place.rows) + (place.rows == 1 ? " row x " : " rows x ") +
           std::to_string(place.columns) + (place.columns == 1 ? " column " : " columns ") +
           (place.fit.graphical() ? "in type" : "as cells") + "; no authored interior";
}

// ---- A WORKSHOP PANE AS A SUBJECT, inspected and edited through its owners (Info's) ------

// WL-PED-05 -- agents/workshop/pane-manager.md
PixelRect pane_window_base(const Session& s, const PaneRef& ref) {
    PixelRect out;
    const std::optional<std::int64_t> kind = resolve_pane(ref, s.panes);
    if (kind.has_value()) {
        const Screen sc = screen_of(s);
        out = room_of_canvas(bounds_of(s.panes, s.setup.active, *kind, sc).resolved, sc);
    }
    const SetupPane* row = pane_of(s.setup.active, ref);
    if (row != nullptr && row->place.mode == pane_unit::kPixels) {
        out.x = row->place.x;
        out.y = row->place.y;
    }
    if (row != nullptr && row->width.mode == pane_unit::kPixels) {
        out.w = row->width.amount;
    }
    if (row != nullptr && row->height.mode == pane_unit::kPixels) {
        out.h = row->height.amount;
    }
    return out;
}

// WL-PED-06 -- agents/workshop/pane-manager.md; WL-PANE-08 -- agents/workshop/panes-and-windows.md
Written pane_geometry_typeable(const Session& s, const PaneRef& ref) {
    if (!has_pane(s.setup.active, ref)) {
        return Written::no(ref_text(ref) + " is not in this layout -- open it first");
    }
    const std::optional<std::int64_t> kind = resolve_pane(ref, s.panes);
    if (!kind.has_value()) {
        return Written::no(ref_text(ref) +
                           " is unresolved -- its window cannot be measured; `-` resets an "
                           "axis and the order keys still work");
    }
    // No refusal for the right column: the screen reserves nothing, so typed geometry reaches the
    // pane standing there like any other, and none for a pane off the screen, which typed values
    // bring back.
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
        if (row->place.mode != pane_unit::kPixels) {
            return "-";
        }
        out = geometry_amount_text(axis == 0 ? row->place.x : row->place.y, s.cell_px,
                                   projected);
    } else {
        const PaneSize& size = axis == 2 ? row->width : row->height;
        if (size.mode != pane_unit::kPixels) {
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
    const PixelRect from = pane_window_base(s, ref);
    PaneAxisProposal horizontal;
    PaneAxisProposal vertical;
    horizontal.base = from.x;
    vertical.base = from.y;
    switch (axis) {
    case 0: horizontal.position = typed.px; break;
    case 1: vertical.position = typed.px; break;
    case 2: horizontal.extent = PaneSize{pane_unit::kPixels, typed.px}; break;
    default: vertical.extent = PaneSize{pane_unit::kPixels, typed.px}; break;
    }
    return author_pane_window(s.setup.active, ref, horizontal, vertical).written;
}

// ---- A PANE'S SETTINGS AS ROWS OF ITS SUBJECT ---------------------------------------------

// WL-SETTING-05 -- agents/workshop/settings.md
SettingsSubject settings_subject(const Session& s, const PaneRef& ref,
                                 const std::vector<PaneSettingRow>* declared) {
    SettingsSubject out;
    if (declared != nullptr) {
        out.declared = *declared;
    }
    if (const SetupPane* seat = pane_of(s.setup.active, ref)) {
        for (const PaneSetting& stored : seat->settings) {
            bool takes = false;
            for (const PaneSettingRow& row : out.declared) {
                takes = takes || row.key == stored.key;
            }
            if (!takes) {
                out.kept.push_back(stored.key);
            }
        }
    }
    return out;
}

namespace {

std::string_view without_edge_spaces(std::string_view text) {
    while (!text.empty() && text.front() == ' ') {
        text.remove_prefix(1);
    }
    while (!text.empty() && text.back() == ' ') {
        text.remove_suffix(1);
    }
    return text;
}

} // namespace

std::string pane_setting_text(const Session& s, const PaneRef& ref, const PaneSettingRow& row) {
    const SetupPane* seat = pane_of(s.setup.active, ref);
    if (seat == nullptr) {
        return "--";
    }
    const PaneSetting fallback = pane_setting_default(row);
    const PaneSetting* stored = find_pane_setting(seat->settings, row.key);
    if (stored == nullptr) {
        return setting_value_text(fallback);
    }
    if (pane_setting_refused_by(row, *stored).empty()) {
        return setting_value_text(*stored);
    }
    // A STORED VALUE THE SETTING DOES NOT TAKE IS SHOWN AS STORED, never repaired, beside what it
    // takes and what stands in for it.
    return setting_value_text(*stored) + " -- takes " + pane_setting_takes(row) + "; " +
           setting_value_text(fallback) + " in effect";
}

// WL-SETTING-05 -- agents/workshop/settings.md
Written write_pane_setting(Session& s, const PaneRef& ref, const PaneSettingRow& row,
                           const std::string& text) {
    const std::string_view body = without_edge_spaces(text);
    const PaneSetting fallback = pane_setting_default(row);
    if (body == "-") {
        return author_pane_setting(s.setup.active, ref, row.key, std::nullopt, fallback);
    }
    if (!has_pane(s.setup.active, ref)) {
        return Written::no(ref_text(ref) + " is not in this layout -- open it first");
    }
    const std::string refused = "takes " + pane_setting_takes(row) + ", not `" +
                                std::string(body) + "`";
    PaneSetting typed;
    typed.key = row.key;
    switch (setting_kind(row)) {
    case SettingKind::kFlag:
        if (body == kSettingOn || body == kSettingOff) {
            typed.flag = body == kSettingOn;
        }
        break;
    case SettingKind::kNumber:
        // A NUMBER'S ONE CONVERSION IS THE INSPECTOR'S OWN (WL-DOC-02).
        typed.number = TextForm<std::int64_t>::parse(body);
        break;
    case SettingKind::kText: typed.text = std::string(body); break;
    default: break;
    }
    if (setting_kind(typed) == SettingKind::kNone || !pane_setting_refused_by(row, typed).empty()) {
        return Written::no(refused);
    }
    return author_pane_setting(s.setup.active, ref, row.key, typed, fallback);
}

std::string kept_setting_text(const Session& s, const PaneRef& ref, const std::string& key) {
    const SetupPane* seat = pane_of(s.setup.active, ref);
    const PaneSetting* stored = seat == nullptr ? nullptr : find_pane_setting(seat->settings, key);
    if (stored == nullptr) {
        return "--";
    }
    return setting_value_text(*stored) + " -- kept; no setting here takes it, and `-` clears it";
}

// WL-SETTING-05 -- agents/workshop/settings.md
Written write_kept_setting(Session& s, const PaneRef& ref, const std::string& key,
                           const std::string& text) {
    if (without_edge_spaces(text) != "-") {
        return Written::no("kept for a pane that declares it -- it takes only `-`, which clears it");
    }
    return author_pane_setting(s.setup.active, ref, key, std::nullopt, std::nullopt);
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
    for (const CatalogRow& row : inventory_rows(s.setup.active, s.panes)) {
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
std::vector<Row> pane_subject_rows(Session& s, const PaneRef& ref,
                                   const SettingsSubject& settings) {
    std::vector<Row> rows;
    if (ref.provider.empty()) {
        return rows;
    }
    Session* sp = &s;
    const auto found = [sp, ref]() -> std::optional<CatalogRow> {
        for (const CatalogRow& row : inventory_rows(sp->setup.active, sp->panes)) {
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
        const std::optional<std::int64_t> kind = resolve_pane(ref, sp->panes);
        if (!kind.has_value()) {
            return std::string("-");
        }
        const Screen sc = screen_of(*sp);
        const PaneBounds where = bounds_of(sp->panes, sp->setup.active, *kind, sc);
        if (!where.open) {
            return std::string("-");
        }
        // Said in the room, where the pane's X and Y are measured from.
        return pixel_rect_text(room_of_canvas(where.resolved, sc), sp->cell_px);
    }));
    rows.push_back(Row::show("State", [sp, found] {
        const std::optional<CatalogRow> row = found();
        if (!row) {
            return std::string("-- not in this build's vocabulary nor this layout");
        }
        const std::int64_t state =
            pane_state_of(sp->panes, sp->setup.active, screen_of(*sp), *row);
        std::string out = pane_state_word(state);
        const char* remedy = pane_state_remedy(state);
        if (remedy[0] != '\0') {
            out += std::string(" -- ") + remedy;
        }
        return out;
    }));
    // ---- SETTINGS: a row per setting the declaration gives, named by its key, and a kept row per
    // key the row stores that none of them takes. A pane with neither has no such section.
    if (settings.any()) {
        rows.push_back(Row::section("SETTINGS"));
        for (const PaneSettingRow& declared : settings.declared) {
            rows.push_back(Row::edit(
                declared.key,
                Property<std::string>(
                    [sp, ref, declared] { return pane_setting_text(*sp, ref, declared); },
                    [sp, ref, declared](std::string text) {
                        return write_pane_setting(*sp, ref, declared, text);
                    })));
        }
        for (const std::string& key : settings.kept) {
            rows.push_back(Row::edit(
                key, Property<std::string>(
                         [sp, ref, key] { return kept_setting_text(*sp, ref, key); },
                         [sp, ref, key](std::string text) {
                             return write_kept_setting(*sp, ref, key, text);
                         })));
        }
    }
    // ---- INTERIOR: a read-only capture of the resolved body, never inferred controls --------
    rows.push_back(Row::section("INTERIOR"));
    rows.push_back(Row::show("Interior", [sp, ref] { return interior_capture_text(*sp, ref); }));
    return rows;
}

} // namespace zengine::workshop
