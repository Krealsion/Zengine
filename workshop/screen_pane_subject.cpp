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

// WL-PED-05 -- agents/workshop/pane-manager.md; WL-GEO-12 -- agents/workshop/geometry.md
std::string pane_axis_text(const Session& s, const PaneRef& ref, std::size_t axis) {
    const SetupPane* row = pane_of(s.setup.active, ref);
    if (row == nullptr) {
        return "--";
    }
    bool projected = false;
    const auto amount = [&](std::int64_t px) {
        return geometry_amount_text(px, s.cell_px, projected) + " " +
               std::string(geometry_unit(s.cell_px));
    };
    std::string out;
    if (axis < 2) {
        // A PLACE IS ITS WORD AND, ON AN AXIS THE WORD READS, THE OFFSET FROM ITS EDGE.
        const std::int64_t mode = row->place.mode;
        if (mode == pane_unit::kDefault) {
            return "-";
        }
        const bool reads = axis == 0 ? ui::place_reads_x(mode) : ui::place_reads_y(mode);
        const std::string word =
            mode == pane_unit::kPixels ? std::string() : std::string(ui::place_word(mode));
        if (!reads) {
            return word;
        }
        out = (word.empty() ? "" : word + " ") + amount(axis == 0 ? row->place.x : row->place.y);
    } else {
        const PaneSize& size = axis == 2 ? row->width : row->height;
        switch (size.mode) {
        case pane_unit::kPixels: out = amount(size.amount); break;
        case ui::kSizeShare: out = share_text(size.amount); break;
        case ui::kSizeFit:
        case ui::kSizeFill: out = std::string(ui::size_word(size.mode)); break;
        default: return "-";
        }
    }
    if (projected) {
        out += kProjectedNote;
    }
    return out;
}

namespace {

std::string_view trimmed(std::string_view v) {
    while (!v.empty() && v.front() == ' ') {
        v.remove_prefix(1);
    }
    while (!v.empty() && v.back() == ' ') {
        v.remove_suffix(1);
    }
    return v;
}

/// A share as typed: `50%` or `33.33%`, hundredths of a percent at most, or `share 5000` in
/// ten-thousandths. None when the text is neither.
std::optional<std::int64_t> typed_share(std::string_view body) {
    if (body.size() > 6 && body.substr(0, 6) == "share ") {
        return TextForm<std::int64_t>::parse(trimmed(body.substr(6)));
    }
    if (body.size() < 2 || body.back() != '%') {
        return std::nullopt;
    }
    const std::string_view number = body.substr(0, body.size() - 1);
    std::string_view whole = number;
    std::string_view part;
    if (const std::size_t dot = number.find('.'); dot != std::string_view::npos) {
        whole = number.substr(0, dot);
        part = number.substr(dot + 1);
        if (part.empty() || part.size() > 2) {
            return std::nullopt;
        }
    }
    const std::optional<std::int64_t> w = TextForm<std::int64_t>::parse(whole);
    const std::optional<std::int64_t> f =
        part.empty() ? std::optional<std::int64_t>(0) : TextForm<std::int64_t>::parse(part);
    if (!w || !f || *w < 0 || *f < 0 || *w > 100) {
        return std::nullopt;
    }
    return *w * 100 + (part.size() == 1 ? *f * 10 : *f);
}

/// What else the row takes, said after a refusal of text that is no number at all; a number in
/// the other face's unit is refused in its own words alone.
std::string word_hint(const FaceAmount& face, const std::string& takes) {
    return face.refusal.rfind("not a whole number", 0) == 0 ? " -- or " + takes : std::string();
}

} // namespace

// WL-PED-05 -- agents/workshop/pane-manager.md
Written write_pane_axis(Session& s, const PaneRef& ref, std::size_t axis,
                        const std::string& text) {
    const std::string_view body = trimmed(text);
    // THE EDIT IS JUDGED ON A COPY OF THE LIVE DESK, and the row it makes is what the commit asks
    // the store to write.
    Setup desk = s.setup.active;
    if (body == "-") {
        if (!has_pane(desk, ref)) {
            return Written::no(ref_text(ref) + " is not in this layout -- open it first");
        }
        bool moved = false;
        const char* what = "";
        if (axis < 2) {
            moved = reset_pane_place(desk, ref);
            what = "place";
        } else if (axis == 2) {
            moved = reset_pane_width(desk, ref);
            what = "width";
        } else {
            moved = reset_pane_height(desk, ref);
            what = "height";
        }
        if (!moved) {
            return Written::no(ref_text(ref) + " already takes the developer's " + what);
        }
        s.pending_edit = window_edit(*pane_of(desk, ref));
        return Written::ok();
    }
    const Written ready = pane_geometry_typeable(s, ref);
    if (!ready.accepted) {
        return ready;
    }
    SetupPane* row = pane_of(desk, ref);
    const auto stored = [&s, &desk, &ref](const Written& legal) {
        if (legal.accepted) {
            s.pending_edit = window_edit(*pane_of(desk, ref));
        }
        return legal;
    };
    // A TYPED SIZE IS WRITTEN AS TYPED, judged by the owner's own rules for its word: a number
    // in the face's unit is pixels, `fit` and `fill` are themselves, and a share is `50%`,
    // `33.33%` or `share 5000`.
    if (axis >= 2) {
        PaneSize& size = axis == 2 ? row->width : row->height;
        const char* which = axis == 2 ? "width" : "height";
        PaneSize typed;
        const std::optional<std::int64_t> mode = ui::size_named(body);
        if (mode && (*mode == ui::kSizeFit || *mode == ui::kSizeFill)) {
            typed = PaneSize{*mode, 0};
        } else if (const std::optional<std::int64_t> share = typed_share(body)) {
            typed = PaneSize{ui::kSizeShare, *share};
        } else {
            std::string_view amount = body;
            if (amount.size() > 7 && amount.substr(0, 7) == "pixels ") {
                amount = trimmed(amount.substr(7));
            }
            const FaceAmount face = parse_face_amount(amount, s.cell_px);
            if (!face.accepted) {
                return Written::no(face.refusal + word_hint(face, "a word: fit, fill, `50%` or `share 5000`"));
            }
            typed = PaneSize{pane_unit::kPixels, face.px};
        }
        const Written legal = check_pane_size(typed, which);
        if (legal.accepted) {
            size = typed;
        }
        return stored(legal);
    }
    // A PLACE WORD, with this axis's offset after it where the word reads one; the other axis
    // keeps its offset where both words read it.
    const bool across = axis == 0;
    const std::size_t space = body.find(' ');
    const std::string_view head = body.substr(0, space);
    if (const std::optional<std::int64_t> mode = ui::place_named(head);
        mode && *mode != pane_unit::kDefault) {
        std::int64_t offset = 0;
        if (space != std::string_view::npos) {
            const FaceAmount face = parse_face_amount(trimmed(body.substr(space)), s.cell_px);
            if (!face.accepted) {
                return Written::no(face.refusal);
            }
            offset = face.px;
        }
        const PanePlace was = row->place;
        const auto kept = [](bool now_reads, bool was_reads, std::int64_t v) {
            return now_reads && was_reads ? v : std::int64_t{0};
        };
        PanePlace typed{*mode, kept(ui::place_reads_x(*mode), ui::place_reads_x(was.mode), was.x),
                        kept(ui::place_reads_y(*mode), ui::place_reads_y(was.mode), was.y)};
        if (across ? ui::place_reads_x(*mode) : ui::place_reads_y(*mode)) {
            (across ? typed.x : typed.y) = offset;
        } else if (offset != 0) {
            return Written::no("`" + std::string(head) + "` is centred " +
                               (across ? "across" : "down") + " -- it takes no offset there");
        }
        const Written legal = check_pane_place(typed);
        if (legal.accepted) {
            row->place = typed;
        }
        return stored(legal);
    }
    const FaceAmount face = parse_face_amount(body, s.cell_px);
    if (!face.accepted) {
        return Written::no(face.refusal + word_hint(face, "a place word: " + pane_place_words()));
    }
    // A NUMBER ON AN AXIS THE PLACE'S WORD READS is the offset from that word's edge, as the row
    // shows it; on a centred or `default` axis, and for `pixels`, it is pixels from the room's
    // top-left, through the gesture door.
    const std::int64_t mode = row->place.mode;
    if (mode != pane_unit::kPixels &&
        (across ? ui::place_reads_x(mode) : ui::place_reads_y(mode))) {
        PanePlace moved = row->place;
        (across ? moved.x : moved.y) = face.px;
        const Written legal = check_pane_place(moved);
        if (legal.accepted) {
            row->place = moved;
        }
        return stored(legal);
    }
    PaneWindowAxes axes = pane_window_at(pane_window_base(s, ref), screen_of(s));
    (across ? axes.horizontal : axes.vertical).position = face.px;
    return stored(author_pane_window(desk, ref, axes.horizontal, axes.vertical).written);
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
std::vector<Row> pane_subject_rows(Session& s, const PaneRef& ref) {
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
    // ---- INTERIOR: a read-only capture of the resolved body, never inferred controls --------
    rows.push_back(Row::section("INTERIOR"));
    rows.push_back(Row::show("Interior", [sp, ref] { return interior_capture_text(*sp, ref); }));
    return rows;
}

} // namespace zengine::workshop
