// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `screen.hpp`'s sections -- the source editor's pane -- compiled once into
// `zengine-workshop-logic` and linked by the host and every suite; the declarations, the
// constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/editor.md

#include "screen.hpp"

namespace zengine::workshop {

// ---- THE SOURCE EDITOR'S PANE: one document, projected through a viewport ---------------

ExternalBodyPlace editor_body(const Session& s, const Screen& sc) {
    const PanelBounds where = bounds_of(s.panels, s.setup.active, panel::kEditor, sc);
    if (!where.open) {
        return ExternalBodyPlace{};
    }
    return external_body_place(where.rect, sc, kEditorHeaderRows);
}

// WL-EDIT-12 -- agents/workshop/editor.md
std::string editor_header(const EditorState& e, bool typing) {
    std::string head = std::string(typing ? kTypingHere : kTypingElsewhere);
    if (!e.open_document()) {
        return head + "Editor -- no source open";
    }
    head += std::string("Editor ") + (e.dirty() ? "UNSAVED" : "saved");
    head += " L" + std::to_string(e.buffer.caret_row() + 1) + ":C" +
            std::to_string(visual_col_of(e.buffer.line(e.buffer.caret_row()),
                                         e.buffer.caret_byte()) +
                           1);
    head += "/" + std::to_string(e.buffer.line_count());
    head += " -- " + e.path;
    return head;
}

// WL-EDIT-09 -- agents/workshop/editor.md
void reconcile_editor_view(Session& s) {
    EditorState& e = s.editor;
    if (!e.open_document()) {
        return;
    }
    const ExternalBodyPlace body = editor_body(s, screen_of(s));
    if (!body.present) {
        return; // no presented body: the viewport keeps its answer for the room to come
    }
    const bool resized = body.rows != e.last_rows || body.columns != e.last_cols;
    e.last_rows = body.rows;
    e.last_cols = body.columns;
    const std::size_t rows = static_cast<std::size_t>(body.rows);
    const std::size_t total = e.buffer.line_count();
    const std::size_t furthest_row = total > rows ? total - rows : 0;
    if (e.first_row > furthest_row) {
        e.first_row = furthest_row; // rule 1's vertical half: no blank rows below while
    }                               // lines are hidden above
    if (e.first_col < 0) {
        e.first_col = 0;
    }
    if (!e.follow_caret && !resized) {
        return;
    }
    e.follow_caret = false;
    const std::size_t cr = e.buffer.caret_row();
    if (cr < e.first_row) {
        e.first_row = cr;
    }
    if (cr >= e.first_row + rows) {
        e.first_row = cr + 1 - rows;
    }
    const std::int64_t text_cols = editor_text_columns(body);
    if (text_cols <= 0) {
        return;
    }
    const std::string& line = e.buffer.line(cr);
    const std::int64_t vis = visual_col_of(line, e.buffer.caret_byte());
    // Rule 1's horizontal half, measured on the caret's own line: no blank room at the
    // right while its text is hidden at the left, so erasing a long line back down
    // recovers the room it freed.
    const std::int64_t need = visual_len(line) + kEditorCaretCols;
    const std::int64_t furthest_col = need > text_cols ? need - text_cols : 0;
    if (e.first_col > furthest_col) {
        e.first_col = furthest_col;
    }
    if (vis < e.first_col) {
        e.first_col = vis;
    }
    if (vis - e.first_col > text_cols) {
        e.first_col = vis - text_cols;
    }
}

EditorPressAt editor_press_at(const Session& s, const Screen& sc, std::int64_t space,
                              std::int64_t x, std::int64_t y) {
    const ExternalBodyPlace body = editor_body(s, sc);
    if (!body.present) {
        return EditorPressAt{};
    }
    const ProseAt at = prose_at(space, x, y, body.region_x, body.region_y, body.fit);
    if (!at.understood) {
        return EditorPressAt{};
    }
    const std::int64_t row = at.row - body.header_rows;
    if (row < 0 || row >= body.rows || at.column < 0 || at.column > body.columns) {
        return EditorPressAt{};
    }
    return EditorPressAt{true, row, at.column};
}

bool over_editor_body(const Session& s, const Screen& sc, std::int64_t space,
                      std::int64_t x, std::int64_t y) {
    const ExternalBodyPlace body = editor_body(s, sc);
    if (!body.present) {
        return false;
    }
    const ProseAt at = prose_at(space, x, y, body.region_x, body.region_y, body.fit);
    return at.understood && at.row >= body.header_rows && at.row < body.fit.rows;
}

// WL-EDIT-12 -- agents/workshop/editor.md
void paint_editor(surface::SurfaceLayer& layer, const Session& s, const FineRect& b,
                  const Screen& sc, std::int64_t chrome) {
    paint_panel_frame(layer, b, chrome);
    const ExternalBodyPlace body = external_body_place(b, sc, kEditorHeaderRows);
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
    const EditorState& e = s.editor;
    if (body.header_rows > 0) {
        region.rows.push_back(surface::SurfaceTextRow{
            detail::fit(editor_header(e, editor_has_keyboard(s)), body.columns),
            surface::role::kAccent});
    }
    if (!body.present) {
        if (!region.rows.empty()) {
            layer.texts.push_back(std::move(region));
        }
        return; // room for the heading and nothing else: the heading, honestly
    }
    if (!e.open_document()) {
        // THE ABSENCE IS THE HEADER'S SENTENCE AND IS NOT SAID TWICE. This row
        // used to read `no source open -- e opens the Builder's chosen recipe`: half of it
        // repeated `editor_header`'s own `Editor -- no source open`, and the other half
        // taught a key `document.open`'s catalog row already owns. The header still states
        // the absence, the hotkey view still teaches the gesture, and the pane gives the
        // row back to the document it is waiting for.
        layer.texts.push_back(std::move(region));
        return;
    }
    const std::int64_t text_cols = editor_text_columns(body);
    const std::size_t total = e.buffer.line_count();
    const std::size_t rows = static_cast<std::size_t>(body.rows);
    const std::size_t last = e.first_row + rows < total ? e.first_row + rows : total;
    for (std::size_t r = e.first_row; r < last; ++r) {
        region.rows.push_back(surface::SurfaceTextRow{
            expanded_slice(e.buffer.line(r), e.first_col, text_cols), surface::role::kFill});
    }
    // THE CARET, WHEN ITS ROW IS IN THE WINDOW -- region prose coordinates, the header
    // counted, the column in the viewport's own displayed lattice.
    const std::size_t cr = e.buffer.caret_row();
    if (cr >= e.first_row && cr < last) {
        const std::int64_t vis = visual_col_of(e.buffer.line(cr), e.buffer.caret_byte());
        std::int64_t col = vis - e.first_col;
        if (col >= 0 && col <= text_cols) {
            region.caret_row = body.header_rows + static_cast<std::int64_t>(cr - e.first_row);
            region.caret_col = col;
        }
    }
    // THE SELECTION, CLAMPED INTO THE WINDOW. The range travels in reading order on the
    // region (begin inclusive, end exclusive) and `selection_span_of_row` does the
    // per-row arithmetic in both media; what this clamps is only the part outside the
    // viewport, which no medium could show.
    if (e.buffer.has_selection()) {
        const EditorPos from = e.buffer.selection_begin();
        const EditorPos to = e.buffer.selection_end();
        if (from.row < last && to.row >= e.first_row) {
            std::int64_t brow;
            std::int64_t bcol;
            if (from.row < e.first_row) {
                brow = body.header_rows;
                bcol = 0;
            } else {
                brow = body.header_rows + static_cast<std::int64_t>(from.row - e.first_row);
                const std::int64_t v =
                    visual_col_of(e.buffer.line(from.row), from.byte) - e.first_col;
                bcol = v < 0 ? 0 : (v > text_cols ? text_cols : v);
            }
            std::int64_t erow;
            std::int64_t ecol;
            if (to.row >= last) {
                erow = body.header_rows + static_cast<std::int64_t>(last - e.first_row);
                ecol = 0;
            } else {
                erow = body.header_rows + static_cast<std::int64_t>(to.row - e.first_row);
                const std::int64_t v =
                    visual_col_of(e.buffer.line(to.row), to.byte) - e.first_col;
                ecol = v < 0 ? 0 : (v > text_cols ? text_cols : v);
            }
            if (erow > brow || ecol > bcol) {
                region.sel_begin_row = brow;
                region.sel_begin_col = bcol;
                region.sel_end_row = erow;
                region.sel_end_col = ecol;
            }
        }
    }
    layer.texts.push_back(std::move(region));
}

} // namespace zengine::workshop
