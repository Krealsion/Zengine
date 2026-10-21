// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SURFACE_REGION_HPP
#define ZENGINE_SURFACE_REGION_HPP

// A bounded region of a canvas as a character medium draws it: its rows as labels on the cells
// its bounds cover, and the part of its viewport on the surface. How much prose it holds is
// `workshop/pane_text_fit.hpp`'s, the fit its publisher asked. Every product saturates, because
// a region's bounds are numbers a publisher chose. Reference: surface/docs/surface.md.

#include "graphics/pixels.hpp"
#include "graphics/role.hpp"
#include "vocabulary.hpp"
#include "workshop/pane_text_fit.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace zengine::surface {

/// How wide a caret bar is, in device pixels, in a medium that sets real type: part of where a
/// caret is, so a hit test, a plan and a renderer share it. A bar, never a block, where type is
/// set, because the caret is an insertion point between two characters; at an 8-pixel advance one
/// pixel reads as a stem and four as a block cursor. A medium whose character is a cell has no
/// place between two characters, and shows the cell the caret stands on inverted instead
/// (`ProjectedRow::caret`).
inline constexpr std::int64_t kCaretWidthPx = 2;

/// The same resolution from the shapes, so a region is fitted where it is painted.
inline constexpr workshop::RegionFit fit_region(const SurfaceTextRegion& r,
                                      const SurfaceExtent& metric) noexcept {
    return workshop::fit_region(r.x, r.y, r.w, r.h, metric.text_advance_px, metric.text_line_px);
}

/// The part of a viewport actually on the surface, in device pixels. Separate from the fit on
/// purpose: a window two pixels too small draws less, and does not change how much prose the
/// publisher was told fits.
inline constexpr workshop::RegionViewport clip_viewport(const workshop::RegionViewport& v, std::int64_t surface_w,
                                              std::int64_t surface_h) noexcept {
    workshop::RegionViewport out;
    out.x = v.x > 0 ? v.x : 0;
    out.y = v.y > 0 ? v.y : 0;
    // `add_cells` saturates pixels as well as cells: both numbers came off the wire.
    const std::int64_t right = graphics::add_cells(v.x, v.w);
    const std::int64_t bottom = graphics::add_cells(v.y, v.h);
    const std::int64_t clip_r = right < surface_w ? right : surface_w;
    const std::int64_t clip_b = bottom < surface_h ? bottom : surface_h;
    out.w = clip_r > out.x ? clip_r - out.x : 0;
    out.h = clip_b > out.y ? clip_b - out.y : 0;
    return out;
}

/// How much of a region the cell projection materializes, per axis: a bound, not a policy. `w`
/// and `h` are a publisher's numbers, and a region a hundred million cells wide would allocate
/// that much per row for a canvas that shows a few hundred; no display is sixteen thousand
/// cells across.
inline constexpr std::int64_t kMaxProjectedWidth = 16384;
inline constexpr std::int64_t kMaxProjectedRows = 16384;

/// One projected row: the label a cell medium draws, and the ground it draws it on. The ground
/// sits beside the label because `SurfaceLabel` is a wire shape with no background, and the
/// pairing belongs to this projection alone.
struct ProjectedRow {
    SurfaceLabel label;
    std::int64_t background = graphics::role::kNone; ///< role::kNone: whatever is underneath
    /// ...and the region's own answer: a medium resolves the row's ground if it named one, else
    /// the canvas ground if the region took its rectangle, else nothing.
    std::int64_t ground = kGroundOwn;
    /// The selected columns, in the label's own bytes (cut applied): a consumer highlights
    /// `text[i]` for `sel_begin <= i < sel_end`. Empty is the absence.
    std::int64_t sel_begin = 0;
    std::int64_t sel_end = 0;
    /// The byte of `label.text` the region's caret stands on, which a medium shows inverted;
    /// -1 is none. It inserts nothing, so no character moves for a caret.
    std::int64_t caret = -1;
};

/// A region's rows as canvas labels, the cell projection every cell medium shares: row `i` one
/// cell below row 0, cut at the covered cells' width, dropped past their height. Every cell row
/// gets a label padded to the full width, because a region is an overlay and an unwritten row shows
/// its emptiness. Under `kGroundBeneath` a row is cut but not padded, unless it named its own
/// ground, and an empty row is not produced unless a caret stands on it. A caret stands on the
/// cell of the character it sits before, and past a full row's end on the row's last cell (this
/// projection does not scroll); a caret at a beneath row's end gets one blank cell to stand on.
inline void project_one_text_region(const SurfaceTextRegion& r, std::vector<ProjectedRow>& out) {
    if (r.w <= 0 || r.h <= 0) {
        return; // a region with no bounds shows nothing, and says nothing about it
    }
    // The capacity is the covered cells: a right edge that crosses a cell boundary earns that
    // cell, so the cut, the padding and a pane's backdrop agree to the cell.
    const std::int64_t covered_w = graphics::cell_of_pixel(graphics::add_cells(r.x, r.w)) - graphics::cell_of_pixel(r.x);
    const std::int64_t covered_h = graphics::cell_of_pixel(graphics::add_cells(r.y, r.h)) - graphics::cell_of_pixel(r.y);
    const std::size_t width = static_cast<std::size_t>(
        covered_w < static_cast<std::int64_t>(kMaxProjectedWidth) ? covered_w
                                                                  : kMaxProjectedWidth);
    const std::int64_t lines =
        covered_h < static_cast<std::int64_t>(kMaxProjectedRows) ? covered_h
                                                                 : kMaxProjectedRows;
    for (std::int64_t i = 0; i < lines; ++i) {
        const std::size_t at = static_cast<std::size_t>(i);
        const bool said = at < r.rows.size();
        std::string text = said ? r.rows[at].text : std::string();
        const std::int64_t role = said ? r.rows[at].role : graphics::role::kFill;
        const std::int64_t back = said ? r.rows[at].background : graphics::role::kNone;
        workshop::RowSpan span = workshop::selection_span_of_row(r, i, static_cast<std::int64_t>(text.size()));
        // A caret further right than just past the last cell shown stands nowhere shown.
        const bool has_caret = r.caret_row == i && r.caret_col >= 0 &&
            r.caret_col <= static_cast<std::int64_t>(text.size()) &&
            r.caret_col <= static_cast<std::int64_t>(width) && width > 0;
        // `!= kGroundBeneath`, never `== kGroundOwn`: an unknown ground reads as OWNING its
        // room, so a number nobody chose cannot stop a region padding. See vocabulary.hpp.
        const bool takes_the_cells = r.ground != kGroundBeneath || back != graphics::role::kNone;
        if (has_caret && static_cast<std::size_t>(r.caret_col) == text.size() &&
            text.size() < width) {
            text += ' '; // a cell to stand on, past the last character
        }
        if (text.size() > width) {
            text.resize(width); // cut on a byte boundary: one cell per byte, as ever
        } else if (takes_the_cells) {
            text.append(width - text.size(), ' ');
        } else if (text.empty()) {
            continue; // nothing to write and no cells to claim: not a row at all
        }
        const std::int64_t last = static_cast<std::int64_t>(text.size()) - 1;
        const std::int64_t caret = has_caret ? (r.caret_col < last ? r.caret_col : last) : -1;
        if (span.begin > static_cast<std::int64_t>(text.size())) {
            span.begin = static_cast<std::int64_t>(text.size());
        }
        if (span.end > static_cast<std::int64_t>(text.size())) {
            span.end = static_cast<std::int64_t>(text.size()); // the cut cuts highlights too
        }
        // Row `i` is `i` cells below row 0: a character medium floors the anchor to its cell,
        // and the bitmap face draws it where it is.
        out.push_back(ProjectedRow{SurfaceLabel{r.x, graphics::add_cells(r.y, graphics::px_of_cells(i)),
                                                std::move(text), role},
                                   back, r.ground, span.begin, span.end, caret});
    }
}

/// Every region of one layer as cells: a character medium's whole answer for the layer. There
/// is deliberately no overload taking a `SurfaceCanvas`: a canvas-wide projection would flatten
/// every plane into one run, the global order the canvas stopped having.
inline std::vector<ProjectedRow> project_text_regions(const SurfaceLayer& l) {
    std::vector<ProjectedRow> out;
    for (const SurfaceTextRegion& r : l.texts) {
        project_one_text_region(r, out);
    }
    return out;
}

/// The regions of one layer this medium cannot set in its own type, as cells: those whose
/// bounds `fit_region` says hold no row of the face (`plan_layer_regions` draws the rest). With
/// a zero metric that is every region, byte for byte the overload above.
inline std::vector<ProjectedRow> project_text_regions(const SurfaceLayer& l,
                                                     const SurfaceExtent& metric) {
    std::vector<ProjectedRow> out;
    for (const SurfaceTextRegion& r : l.texts) {
        if (fit_region(r, metric).graphical()) {
            continue; // this medium sets this one in type: plan_layer_regions has it
        }
        project_one_text_region(r, out);
    }
    return out;
}

} // namespace zengine::surface

#endif // ZENGINE_SURFACE_REGION_HPP
