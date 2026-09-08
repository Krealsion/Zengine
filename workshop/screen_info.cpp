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

// ---- Is a draft live on the rows this host holds? ----------------------------------------

// WL-PED-07 -- agents/workshop/pane-manager.md
bool draft_live(const Session& s) {
    for (const Row& row : s.rows) {
        if (row.editing()) {
            return true;
        }
    }
    return false;
}

// WL-DOC-20 -- agents/workshop/document.md
DocumentShown document_shown(const WorkshopDoc& d, const Session& s) {
    DocumentShown shown;
    shown.selected = s.selected;
    for (const ui::Element& e : d.elements) {
        shown.objects.push_back(ShownObject{e.id, e.label});
    }
    // THE INSPECTOR ROWS AS THEY STAND, VALUE INCLUDED. `Row::value()` is a fresh read
    // through the property, so what crosses is what the document says at this instant --
    // never a cached copy, on either side of the seam.
    for (const Row& row : s.rows) {
        shown.properties.push_back(
            ShownProperty{row.label(), row.value(), row.editable(), row.section()});
    }
    return shown;
}

// WL-DOC-20 -- agents/workshop/document.md
bool same_document(const DocumentShown& a, const DocumentShown& b) {
    if (a.selected != b.selected || a.objects.size() != b.objects.size() ||
        a.properties.size() != b.properties.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.objects.size(); ++i) {
        if (a.objects[i].identity != b.objects[i].identity ||
            a.objects[i].name != b.objects[i].name) {
            return false;
        }
    }
    for (std::size_t i = 0; i < a.properties.size(); ++i) {
        if (a.properties[i].label != b.properties[i].label ||
            a.properties[i].value != b.properties[i].value ||
            a.properties[i].editable != b.properties[i].editable ||
            a.properties[i].section != b.properties[i].section) {
            return false;
        }
    }
    return true;
}

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
TerminalSelectionSpan property_selection_columns(const Row& row,
                                                 std::int64_t value_columns) {
    const component::TextBox::VisibleSpan vis = row.editor().visible_selection(value_columns);
    if (!vis.present()) {
        return TerminalSelectionSpan{};
    }
    return TerminalSelectionSpan{kPropertyMarkCols + kPropertyLabelCols + vis.begin,
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
// MANAGER's row composition, which happens to have been written here first; `draft_live` is
// what a contextual delete asks about `Session::rows`, which are the Pane Manager's; and
// `document_shown` and `same_document` are the document SEAM -- the host's reading of the
// host's document, published because the pane cannot make it.
//
// ⚠ AND `action_availability` LEFT AFTER THE OTHERS. It was kept one stage longer on the
// reading that a control's availability was the host's to derive; nothing called it once
// `paint_info` was gone, and the rule is made in `Zengine/info-pane/pane.cpp` against facts
// only that image holds -- its own draft, and the picture it was shown.

} // namespace zengine::workshop
