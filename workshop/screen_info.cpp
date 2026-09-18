// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `screen.hpp`'s sections -- the Info panel's body, its action controls, and one
// windowed list's rows -- compiled once into `zengine-workshop-logic` and linked by the host and
// every suite; the declarations, the constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/info-body.md (+6 registers; agents/workshop.md routes)

#include "screen.hpp"

namespace zengine::workshop {

// ---- The Info panel's BODY, resolved ONCE ----

BodyShare share_body_rows(std::size_t budget, std::size_t want_objects,
                          std::size_t want_properties) {
    BodyShare s;
    if (budget == 0) {
        return s;
    }
    if (want_properties <= budget && want_objects <= budget - want_properties) {
        s.objects = want_objects; // both whole; the rest of the body stays spare
        s.properties = want_properties;
        return s;
    }
    const std::size_t half = budget / 2;
    if (want_objects <= half) {
        s.objects = want_objects; // it needs less than its share, so it takes what it needs
        s.properties = budget - s.objects;
    } else if (want_properties <= budget - half) {
        s.properties = want_properties;
        s.objects = budget - s.properties;
    } else {
        s.objects = half; // both want more than half: the contested room is shared
        s.properties = budget - half;
    }
    return s;
}

// ⭐ `draft_live`, `document_shown` AND `same_document` WERE HERE -- a draft on the object
// inspector's rows, and the object document's picture across the pane seam -- and retired with
// the prototype canvas. A pane's picture is `pane_subject_shown` (`screen_pane_editor.cpp`).

// ---- One windowed list's rows, mapped both ways ------------------------------------------

// WL-PED-04 -- agents/workshop/pane-manager.md
std::int64_t prose_row_in_window(const ListWindow& w, std::int64_t first_row,
                                 std::size_t index) {
    if (index < w.first || index - w.first >= w.count) {
        return kNoProseRow;
    }
    return first_row + static_cast<std::int64_t>(index - w.first) + (w.before > 0 ? 1 : 0);
}

bool item_at_prose_row(const ListWindow& w, std::int64_t first_row, std::size_t rows,
                       std::int64_t row, std::size_t& out) {
    if (row < first_row || row >= first_row + static_cast<std::int64_t>(rows)) {
        return false;
    }
    const std::int64_t at = row - first_row - (w.before > 0 ? 1 : 0);
    if (at < 0 || at >= static_cast<std::int64_t>(w.count)) {
        return false; // an omission marker, or past the last item shown
    }
    out = w.first + static_cast<std::size_t>(at);
    return true;
}

std::string property_row_prefix(const Row& row, bool here) {
    return std::string(here ? ">" : " ") +
           detail::pad(row.label(), static_cast<std::size_t>(kPropertyLabelCols));
}

std::string property_row_full(const Row& row, bool here) {
    return property_row_prefix(row, here) + row.value();
}

std::string property_row_text(const Row& row, bool here, std::int64_t value_columns) {
    std::string text = property_row_prefix(row, here);
    if (row.editing()) {
        return text + row.editor().visible(value_columns);
    }
    return text + detail::fit(row.value(), value_columns);
}

// WL-TEXT-13 -- agents/workshop/text-box.md
std::int64_t property_caret_column(const Row& row) {
    return kPropertyMarkCols + kPropertyLabelCols +
           static_cast<std::int64_t>(row.editor().caret_column());
}

// WL-TEXT-13 -- agents/workshop/text-box.md
TextSelectionSpan property_selection_columns(const Row& row,
                                                 std::int64_t value_columns) {
    const component::TextBox::VisibleSpan vis = row.editor().visible_selection(value_columns);
    if (!vis.present()) {
        return TextSelectionSpan{};
    }
    return TextSelectionSpan{kPropertyMarkCols + kPropertyLabelCols + vis.begin,
                                 kPropertyMarkCols + kPropertyLabelCols + vis.end, true};
}


// ⭐ THE INFO PANEL'S PRESENTATION LEFT THIS FILE WITH THE PANEL. What stood here was the
// panel's own composition -- where its two lists sat inside its rectangle, which prose row a
// press landed on, what each row read, and the painter that wrote all of it onto a canvas --
// and every line of it is `Zengine/info-pane/pane.cpp`'s now, said as rows into a room the
// pane is granted. `InfoBodyPlace`, `InfoBodyAt`, `info_body_place`, `info_body_at`,
// `inspector_focus`, the six prose-row mappers, the three press helpers, `action_row_text`,
// the object row texts and `paint_info` went together, because they were one composition and
// half of it would have been a geometry with nobody to draw.
//
// WHAT STAYED, AND WHY EACH ONE IS NOT THE PANEL'S. `share_body_rows` is the max-min share and
// the Pane Manager spends it too; `prose_row_in_window` and `item_at_prose_row` are a WINDOW's
// arithmetic with the same two callers; the three property-row functions below are the PANE
// MANAGER's row composition, which happens to have been written here first. (`draft_live`,
// `document_shown` and `same_document` stayed here too, and retired with the object document.)
//
// ⚠ AND `action_availability` LEFT AFTER THE OTHERS. It was kept one stage longer on the
// reading that a control's availability was the host's to derive; nothing called it once
// `paint_info` was gone, and the rule is made in `Zengine/info-pane/pane.cpp` against facts
// only that image holds -- its own draft, and the picture it was shown.

} // namespace zengine::workshop
