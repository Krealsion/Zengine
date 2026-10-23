// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The screen's external pane body.
// Workshop law: agents/workshop/focus.md (+5 registers; agents/workshop.md routes)

#include "screen.hpp"

namespace zengine::workshop {

// ---- AN EXTERNAL PANE'S BODY: one header row of Workshop's, and a region ---------------

// WL-FOCUS-11 -- agents/workshop/focus.md
std::int64_t external_title_rows(const Panes& panes, std::int64_t kind,
                                 bool titles_shown) noexcept {
    return (titles_shown || keyboard_pane(panes) == kind) ? kExternalHeaderRows : 0;
}

// WL-CHROME-05 -- agents/workshop/chrome.md
// WL-EDIT-12 -- agents/workshop/editor.md
// WL-PANE-06 -- agents/workshop/panes-and-windows.md
ExternalBodyPlace external_body_place(const PixelRect& pane_rect, const Screen& sc,
                                      std::int64_t header_rows) {
    ExternalBodyPlace p;
    const PaneInside inside = pane_inside(pane_rect, sc);
    const PixelRect inner = inside.rect;
    if (inner.w <= 0 || inner.h <= 0) {
        return p;
    }
    p.region_x = inner.x;
    p.region_y = inner.y;
    p.region_w = inner.w;
    p.region_h = inner.h;
    p.fit = inside.fit;
    p.header_rows = header_rows;
    p.rows = p.fit.rows > header_rows ? p.fit.rows - header_rows : 0;
    p.columns = p.fit.columns;
    p.present = p.rows > 0 && p.columns > 0;
    return p;
}

// WL-PRESS-04 -- agents/workshop/press-chain.md
ExternalPressAt external_press_at(const Panes& panes, const Setup& setup,
                                  const Screen& sc, std::int64_t kind, bool titles,
                                  std::int64_t space, std::int64_t x, std::int64_t y) {
    const PaneBounds where = bounds_of(panes, setup, kind, sc);
    if (!where.open) {
        return ExternalPressAt{};
    }
    const ExternalBodyPlace body =
        external_body_place(where.rect, sc, external_title_rows(panes, kind, titles));
    if (!body.present) {
        return ExternalPressAt{};
    }
    const ProseAt at = prose_at(space, x, y, body.fit, sc.rows_above);
    if (!at.understood) {
        return ExternalPressAt{};
    }
    const std::int64_t row = at.row - body.header_rows;
    if (row < 0 || row >= body.rows || at.column < 0 || at.column >= body.columns) {
        return ExternalPressAt{};
    }
    // The cell names the character it shows: a caret moves no character from its cell.
    return ExternalPressAt{true, row, at.column};
}

// WL-EDIT-12 -- agents/workshop/editor.md; WL-FOCUS-10 -- agents/workshop/focus.md
std::string external_header(const RuntimePane& row, bool typing) {
    return std::string(typing ? kTypingHere : kTypingElsewhere) + row.name + " @" +
           row.provider;
}

namespace {

/// Does a header row this wide wear the marks?
bool marks_fit(bool marks, std::int64_t columns) noexcept {
    return marks && columns >= kChromeMarksMinColumns;
}

} // namespace

// WL-CHROME-09 -- agents/workshop/chrome.md
std::string external_header_row(const RuntimePane& row, bool typing, bool marks,
                                std::int64_t columns, const std::string& prefix) {
    const std::string title = prefix + external_header(row, typing);
    if (!marks_fit(marks, columns)) {
        return detail::fit(title, columns);
    }
    const std::int64_t room =
        columns - static_cast<std::int64_t>(std::char_traits<char>::length(kChromeMarks)) - 1;
    return detail::pad(detail::fit(title, room), static_cast<std::size_t>(room)) + " " +
           kChromeMarks;
}

// WL-CHROME-09 -- agents/workshop/chrome.md
std::int64_t chrome_mark_at(const Panes& panes, const Setup& setup, const Screen& sc,
                            std::int64_t kind, bool titles, std::int64_t space, std::int64_t x,
                            std::int64_t y) {
    const ExternalPane* pane = panes.external_pane(kind);
    const PaneBounds where = bounds_of(panes, setup, kind, sc);
    if (pane == nullptr || !where.open) {
        return chrome_mark::kNone;
    }
    const ExternalBodyPlace body =
        external_body_place(where.rect, sc, external_title_rows(panes, kind, titles));
    if (body.header_rows <= 0 || !marks_fit(pane->manual, body.columns)) {
        return chrome_mark::kNone;
    }
    const ProseAt at = prose_at(space, x, y, body.fit, sc.rows_above);
    if (!at.understood || at.row != 0) {
        return chrome_mark::kNone;
    }
    // The cells `external_header_row` put them in: the `?` last, the gear two before it.
    if (at.column == body.columns - 1) {
        return chrome_mark::kManual;
    }
    if (at.column == body.columns - 3) {
        return chrome_mark::kSettings;
    }
    return chrome_mark::kNone;
}

// WL-PANE-16 -- agents/workshop/panes-and-windows.md
std::string external_body_words(const ExternalPane& pane, const std::string& name) {
    return pane.held && !pane.in_window ? name + kExternalNoWindow : kExternalWaiting + name;
}

// WL-FOCUS-10 -- agents/workshop/focus.md; WL-PANE-06 -- agents/workshop/panes-and-windows.md
void paint_external(surface::SurfaceLayer& layer, const Panes& panes, std::int64_t kind,
                    const PixelRect& b, const Screen& sc, bool titles, std::int64_t chrome) {
    paint_pane_frame(layer, b, chrome);
    const RuntimePane* row = panes.runtime.of_kind(kind);
    if (row == nullptr) {
        return; // an open kind with no catalog row cannot happen; drawing a lie could
    }
    const ExternalBodyPlace body =
        external_body_place(b, sc, external_title_rows(panes, kind, titles));
    if (body.fit.rows <= 0 || body.fit.columns <= 0) {
        return; // no room for one row of this medium's type: say nothing at all
    }
    surface::SurfaceTextRegion region;
    region.x = body.region_x;
    region.y = body.region_y;
    region.w = body.region_w;
    region.h = body.region_h;
    // Workshop's header is the region's first row, fitted to its columns, and exists exactly when
    // the resolution reserved one: hidden titles return it to the pane, but the keyboard pane keeps
    // its title and its `> ` mark (`external_title_rows`). While a held press keeps the pane's area
    // where it was, the title is not drawn over the row the area still holds.
    const ExternalPane* pane = panes.external_pane(kind);
    const bool typing = keyboard_pane(panes) == kind;
    const bool marks = pane != nullptr && pane->manual;
    if (body.header_rows > 0 && (pane == nullptr || !pane->title_waits)) {
        region.rows.push_back(surface::SurfaceTextRow{
            external_header_row(*row, typing, marks, body.columns), graphics::role::kAccent});
    }
    // ...AND IN THE BODY, WHAT STANDS THERE BEHIND THE PANE'S OWN AREA: that Workshop waits for the
    // pane to draw, covered once its area's ground is drawn, or that its office draws in no window.
    if (body.present && pane != nullptr) {
        region.rows.push_back(surface::SurfaceTextRow{
            detail::fit(external_body_words(*pane, row->name), body.columns),
            graphics::role::kMuted});
    }
    if (!region.rows.empty()) {
        layer.texts.push_back(std::move(region));
    }
}

} // namespace zengine::workshop
