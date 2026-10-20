// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_DESK_READ_HPP
#define ZENGINE_WORKSHOP_DESK_READ_HPP

// THE DESK, READ AS ARITHMETIC: where a medium draws a picture's glyphs, the words and parts said
// there and the point a press reaches each by, a menu's lines, what covers a pane, a reading's
// stamp and its earlier versions, and how much of a reading one reply holds. Each reads only what
// it is handed; `WorkshopWeave`'s answers (weave_desk_read.cpp) spend them.
// Workshop law: agents/workshop/desk-read.md (+3 registers; agents/workshop.md routes)

#include "decoded_cells.hpp"
#include "pane_canvas_vocabulary.hpp"
#include "pane_view.hpp"
#include "pane_vocabulary.hpp"
#include "panes.hpp"
#include "reply_bytes.hpp"
#include "screen.hpp"
#include "setup.hpp"

#include "input/vocabulary.hpp"
#include "surface/pointing.hpp"
#include "surface/region.hpp"
#include "surface/vocabulary.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace zengine::workshop::desk_read {

/// THE SPACE A POINT ON THIS SCREEN IS READ IN: a window's pixels where the medium sets type, a
/// terminal's cells where text is a cell -- the space `cell_center` answers in.
inline std::int64_t input_space_of(const Screen& sc) {
    return sc.text_advance_px > 0 && sc.text_line_px > 0 ? input::space::kPixels
                                                         : input::space::kCells;
}

inline DeskRect desk_rect(const PixelRect& r) {
    return r.w > 0 && r.h > 0 ? DeskRect{r.x, r.y, r.w, r.h} : DeskRect{};
}

/// WHERE A FIT DRAWS ITS GLYPHS: the first glyph's corner, and one glyph's advance and line, in
/// canvas pixels -- the face's where the fit sets type, one cell each where text is a cell -- and
/// the console rows the medium spends above the canvas, which a point in cells counts.
struct GlyphGrid {
    std::int64_t x = 0, y = 0;
    std::int64_t advance = surface::kCanvasCellPx, line = surface::kCanvasCellPx;
    std::int64_t rows_above = surface::kTuiCanvasTopRow;
};

inline GlyphGrid glyph_grid(const surface::RegionFit& fit, std::int64_t rows_above) {
    if (fit.graphical()) {
        return GlyphGrid{fit.view.x + fit.origin_x, fit.view.y + fit.origin_y, fit.advance_px,
                         fit.line_px, rows_above};
    }
    return GlyphGrid{fit.view.x, fit.view.y, surface::kCanvasCellPx, surface::kCanvasCellPx,
                     rows_above};
}

/// The centre of one glyph, in the input space: a window's pixel, or the terminal cell a medium
/// whose unit is the cell draws it on (floored, as it draws) on the console row it reads.
inline void glyph_point(const GlyphGrid& g, std::int64_t row, std::int64_t column, std::int64_t space,
                        std::int64_t& x, std::int64_t& y) {
    if (space == input::space::kPixels) {
        x = g.x + column * g.advance + g.advance / 2;
        y = g.y + row * g.line + g.line / 2;
        return;
    }
    x = surface::cell_of_pixel(g.x + column * g.advance);
    y = surface::cell_of_pixel(g.y + row * g.line) + g.rows_above;
}

/// ONE RUN OF GLYPHS AS A WORD: `text` drawn from `column` of `row` over its bytes' cells of the
/// grid, and its middle byte's centre as the place a press names it -- or, with no byte to name,
/// its first cell, where a caret on an empty line stands.
inline PaneWord word_on(const GlyphGrid& g, std::int64_t row, std::int64_t column, std::string text,
                        std::int64_t space) {
    PaneWord w;
    const std::int64_t bytes = static_cast<std::int64_t>(text.size());
    const std::int64_t n = (std::max<std::int64_t>)(1, bytes);
    w.place = DeskRect{g.x + column * g.advance, g.y + row * g.line, n * g.advance, g.line};
    glyph_point(g, row, column + (bytes > 0 ? (bytes - 1) / 2 : 0), space, w.x, w.y);
    w.space = space;
    w.text = std::move(text);
    return w;
}

inline std::string without_trailing_blanks(std::string text) {
    while (!text.empty() && text.back() == ' ') {
        text.pop_back();
    }
    return text;
}

/// What a column or a unit no part holds gives a press to.
inline constexpr std::size_t kNoPart = static_cast<std::size_t>(-1);

/// A PART NO PRESS REACHES ON ITS OWN HAS NO POINT: one in no space (`input::space::kUnknown`),
/// which no consumer reads, so a press made there blindly lands nowhere rather than on another.
inline void no_point(PanePart& out) {
    out.x = 0;
    out.y = 0;
    out.space = input::space::kUnknown;
}
inline void no_point(v2::PanePart& out) {
    out.x = 0;
    out.y = 0;
    out.space = input::space::kUnknown;
}

/// A PART SAYS ITS AVAILABILITY AS ITS PANE SAID IT; a part of a version that says none, and a part
/// Workshop names itself, is said as available.
inline void say_available(PanePart&, const PaneRowPart&) {}
inline void say_available(v2::PanePart& out, const PaneRowPart&) { out.available = true; }
inline void say_available(v2::PanePart& out, const v2::PaneRowPart& part) { out.available = part.available; }

/// Parts as a version before availability says them.
inline std::vector<PanePart> first_parts(const std::vector<v2::PanePart>& parts) {
    std::vector<PanePart> out;
    out.reserve(parts.size());
    for (const v2::PanePart& p : parts) {
        out.push_back(PanePart{p.name, p.text, p.place, p.x, p.y, p.space});
    }
    return out;
}

/// ...AND NEITHER HAS A WORD OF A PICTURE THAT TAKES NO PRESS.
inline void no_point(PaneWord& out) {
    out.x = 0;
    out.y = 0;
    out.space = input::space::kUnknown;
}

/// THE PART A PRESS ON EACH COLUMN OF EACH ROW REACHES: of `parts`, by its index, the last listed
/// that holds the column, or `kNoPart` -- on each row, every column up to the last any part holds.
template <class Part>
std::map<std::int64_t, std::vector<std::size_t>> row_owners(const std::vector<Part>& parts) {
    std::map<std::int64_t, std::vector<std::size_t>> owners;
    for (const Part& part : parts) {
        std::vector<std::size_t>& row = owners[part.row];
        const auto end = static_cast<std::size_t>((std::max<std::int64_t>)(0, part.column + part.columns));
        if (row.size() < end) row.resize(end, kNoPart);
    }
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const Part& part = parts[i];
        std::vector<std::size_t>& row = owners[part.row];
        for (std::int64_t c = (std::max<std::int64_t>)(0, part.column); c < part.column + part.columns;
             ++c) {
            row[static_cast<std::size_t>(c)] = i;
        }
    }
    return owners;
}

/// ONE NAMED RUN OF A ROW WHERE A FIT DRAWS IT: the cells of `cells` that show the columns of
/// `parts[self]` in `shown`, the characters there, and its point: the middle of its own
/// characters, the ones `owner` gives it, else the middle of its own blank cells the body shows,
/// else none. False where no cell shows any of its columns.
template <class Part, class Out>
bool row_part_on(const GlyphGrid& g, std::int64_t row, const std::string& shown,
                 std::int64_t cells, const std::vector<Part>& parts,
                 const std::vector<std::size_t>& owner, std::size_t self, std::int64_t space,
                 Out& out) {
    const Part& part = parts[self];
    const std::int64_t end = part.column + part.columns;
    const std::int64_t first = part.column;
    const std::int64_t last = (std::min)(end - 1, cells - 1);
    if (first > last) return false;
    const auto size = static_cast<std::int64_t>(shown.size());
    const auto from = static_cast<std::size_t>((std::min)(part.column, size));
    const auto to = static_cast<std::size_t>((std::min)(end, size));
    out.name = part.name;
    say_available(out, part);
    out.text = without_trailing_blanks(shown.substr(from, to - from));
    out.place = DeskRect{g.x + first * g.advance, g.y + row * g.line, (last - first + 1) * g.advance,
                         g.line};
    const std::int64_t characters_end = part.column + static_cast<std::int64_t>(out.text.size());
    std::vector<std::int64_t> characters, blanks;
    for (std::int64_t c = part.column; c <= last; ++c) {
        if (owner[static_cast<std::size_t>(c)] != self) continue;
        (c < characters_end ? characters : blanks).push_back(c);
    }
    const std::vector<std::int64_t>& own = !characters.empty() ? characters : blanks;
    if (own.empty()) {
        no_point(out);
        return true;
    }
    glyph_point(g, row, own[(own.size() - 1) / 2], space, out.x, out.y);
    out.space = space;
    return true;
}

/// The characters drawn wholly inside a place, word by word in their order, the run of each
/// word's joined to the next word's by a space: a word wholly inside gives all of it, and a word
/// the place's edge crosses gives the characters on the place's side, as a row part's text is the
/// characters of its columns.
inline std::string words_inside(const std::vector<PaneWord>& words, const DeskRect& place) {
    std::string out;
    for (const PaneWord& w : words) {
        const auto bytes = static_cast<std::int64_t>(w.text.size());
        if (bytes == 0 || w.place.y < place.y || w.place.y + w.place.h > place.y + place.h) {
            continue;
        }
        const std::int64_t advance = w.place.w / bytes;
        if (advance <= 0) continue;
        const std::int64_t first = (std::max<std::int64_t>)(
            0, -surface::floor_div_px(w.place.x - place.x, advance));
        const std::int64_t end = (std::min<std::int64_t>)(
            bytes, surface::floor_div_px(place.x + place.w - w.place.x, advance));
        if (end <= first) continue;
        const std::string piece =
            w.text.substr(static_cast<std::size_t>(first), static_cast<std::size_t>(end - first));
        out += out.empty() ? piece : " " + piece;
    }
    return out;
}

/// A canvas rectangle in a medium's own units -- pixels in a window, cells in a terminal -- as the
/// medium shows [begin, end) on them: [floor(begin / unit), floor(end / unit)) on each axis.
struct UnitBox {
    std::int64_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    bool empty() const { return x1 <= x0 || y1 <= y0; }
};

/// A part's local rectangle on the body, cut to it, in the medium's units.
inline UnitBox units_of(const PixelRect& body, const v2::PaneCanvasPart& part, std::int64_t unit) {
    const std::int64_t left = surface::add_cells(body.x, part.x);
    const std::int64_t top = surface::add_cells(body.y, part.y);
    return UnitBox{surface::floor_div_px((std::max)(body.x, left), unit),
                   surface::floor_div_px((std::max)(body.y, top), unit),
                   surface::floor_div_px((std::min)(body.x + body.w, surface::add_cells(left, part.w)), unit),
                   surface::floor_div_px((std::min)(body.y + body.h, surface::add_cells(top, part.h)), unit)};
}

/// A unit of a medium, by its column and row.
using UnitPoint = std::pair<std::int64_t, std::int64_t>;

/// WHERE A PRESS REACHES EACH PART OF A PICTURE AS ITS OWN, in the medium's units: of `boxes`, in
/// the order the pane reads a press, a unit is the last one's that holds it. A part's point is its
/// centre unit where that is its own, else the middle of its widest stretch of its own on the row
/// nearest its centre that has one -- the upper of two as near, the leftmost of two as wide -- else
/// none. Each band the boxes' edges cut is painted once, from the last box down: the cost follows
/// the edges, no more than the body's units, never how the boxes nest or overlap.
inline std::vector<std::optional<UnitPoint>> own_points(const std::vector<UnitBox>& boxes) {
    std::vector<std::optional<UnitPoint>> out(boxes.size());
    std::vector<std::int64_t> xs, ys;
    for (const UnitBox& b : boxes) {
        if (b.empty()) continue;
        xs.insert(xs.end(), {b.x0, b.x1});
        ys.insert(ys.end(), {b.y0, b.y1});
    }
    for (std::vector<std::int64_t>* edges : {&xs, &ys}) {
        std::sort(edges->begin(), edges->end());
        edges->erase(std::unique(edges->begin(), edges->end()), edges->end());
    }
    if (xs.size() < 2) return out;
    const auto edge = [](const std::vector<std::int64_t>& edges, std::int64_t v) {
        return static_cast<std::size_t>(std::lower_bound(edges.begin(), edges.end(), v) - edges.begin());
    };
    // Each box's run of columns and of bands between the edges, its centre, and the best stretch
    // of its own found so far: its row's distance from the centre, the row, its width, its left end.
    struct Seen {
        std::size_t c0 = 0, c1 = 0, b0 = 0, b1 = 0;
        std::int64_t cx = 0, cy = 0;
        bool centre = false;
        std::int64_t far = -1, row = 0, width = 0, left = 0;
    };
    std::vector<Seen> seen(boxes.size());
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        const UnitBox& b = boxes[i];
        if (b.empty()) continue;
        Seen& s = seen[i];
        s.c0 = edge(xs, b.x0);
        s.c1 = edge(xs, b.x1);
        s.b0 = edge(ys, b.y0);
        s.b1 = edge(ys, b.y1);
        s.cx = b.x0 + (b.x1 - b.x0 - 1) / 2;
        s.cy = b.y0 + (b.y1 - b.y0 - 1) / 2;
    }
    const std::size_t columns = xs.size() - 1;
    std::vector<std::size_t> owner(columns), next(columns + 1);
    // The first column at or after `k` this band has not painted.
    const auto unpainted = [&next](std::size_t k) {
        std::size_t root = k;
        while (next[root] != root) root = next[root];
        while (next[k] != root) {
            const std::size_t up = next[k];
            next[k] = root;
            k = up;
        }
        return root;
    };
    for (std::size_t band = 0; band + 1 < ys.size(); ++band) {
        std::fill(owner.begin(), owner.end(), kNoPart);
        for (std::size_t k = 0; k <= columns; ++k) next[k] = k;
        for (std::size_t i = boxes.size(); i-- > 0;) {
            const Seen& s = seen[i];
            if (boxes[i].empty() || band < s.b0 || band >= s.b1) continue;
            for (std::size_t k = unpainted(s.c0); k < s.c1; k = unpainted(k + 1)) {
                owner[k] = i;
                next[k] = k + 1;
            }
        }
        const std::int64_t top = ys[band], bottom = ys[band + 1] - 1;
        for (std::size_t k = 0; k < columns;) {
            std::size_t end = k + 1;
            while (end < columns && owner[end] == owner[k]) ++end;
            if (owner[k] != kNoPart) {
                Seen& s = seen[owner[k]];
                const std::int64_t left = xs[k], width = xs[end] - xs[k];
                s.centre = s.centre ||
                           (s.cy >= top && s.cy <= bottom && s.cx >= left && s.cx < left + width);
                const std::int64_t row = (std::clamp)(s.cy, top, bottom);
                const std::int64_t far = row > s.cy ? row - s.cy : s.cy - row;
                if (s.far < 0 || std::make_tuple(far, row, -width, left) <
                                     std::make_tuple(s.far, s.row, -s.width, s.left)) {
                    s.far = far;
                    s.row = row;
                    s.width = width;
                    s.left = left;
                }
            }
            k = end;
        }
    }
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        const Seen& s = seen[i];
        if (s.centre) {
            out[i] = UnitPoint{s.cx, s.cy};
        } else if (s.far >= 0) {
            out[i] = UnitPoint{s.left + (s.width - 1) / 2, s.row};
        }
    }
    return out;
}

/// ONE NAMED PART OF A PICTURE WHERE ITS BODY SHOWS IT: its rectangle cut to the body, the words
/// inside it, and its point in `box`'s units (`own_points`) -- a pixel in a window, the cell the
/// medium paints there in a terminal -- or none. False where the medium shows none of it.
inline bool canvas_part_on(const PixelRect& body, const v2::PaneCanvasPart& part, const UnitBox& box,
                           const std::optional<UnitPoint>& point, const std::vector<PaneWord>& words,
                           std::int64_t space, std::int64_t rows_above, v2::PanePart& out) {
    const UnitBox px = units_of(body, part, 1);
    if (box.empty() || px.empty()) return false;
    out.name = part.name;
    out.available = part.available;
    out.place = DeskRect{px.x0, px.y0, px.x1 - px.x0, px.y1 - px.y0};
    out.text = words_inside(words, out.place);
    if (!point) {
        no_point(out);
        return true;
    }
    out.x = point->first;
    out.y = space == input::space::kPixels ? point->second : point->second + rows_above;
    out.space = space;
    return true;
}

/// THE MENU ON THE SCREEN, read from the painter's own composition of it: Workshop's contextual
/// menu, or a pane's as its presenter showed it, and the lines each names. At most one is open.
inline v2::DeskMenu desk_menu(const Session& s, const Screen& sc) {
    v2::DeskMenu menu;
    surface::SurfaceLayer layer;
    PixelRect bounds;
    if (s.presented.open) {
        menu.office = s.presented.office;
        menu.pane = s.presented.pane;
        menu.picture = s.presented.picture;
        bounds = presented_bounds(s, sc);
        paint_presented(layer, s, sc);
    } else if (s.context.open) {
        menu.office = kWorkshopProvider;
        bounds = context_bounds(s, sc);
        paint_context(layer, s, sc);
    } else {
        return menu;
    }
    menu.open = true;
    menu.place = desk_rect(bounds);
    if (layer.texts.empty()) {
        return menu; // granted and not shown yet: no line is drawn, so none is said
    }
    const ProsePlace place = prose_place(bounds, sc);
    const GlyphGrid grid = glyph_grid(place.fit, sc.rows_above);
    const surface::SurfaceTextRegion& region = layer.texts.back();
    const std::int64_t space = input_space_of(sc);
    for (std::size_t i = 0; i < region.rows.size(); ++i) {
        std::string text = without_trailing_blanks(region.rows[i].text);
        const std::int64_t row = static_cast<std::int64_t>(i);
        PaneWord line = word_on(grid, row, 0, std::move(text), space);
        line.word = row;
        menu.lines.push_back(std::move(line));
    }
    // THE LINES EACH MENU NAMES: a pane's, as its presenter named them; Workshop's own, by the
    // action or group each line shows.
    std::vector<PaneRowPart> named = s.presented.open ? s.presented.parts : std::vector<PaneRowPart>{};
    if (!s.presented.open) {
        const std::vector<std::string> names = context_line_names(s, sc);
        for (std::size_t i = 0; i < names.size(); ++i) {
            if (!names[i].empty()) {
                named.push_back(PaneRowPart{names[i], static_cast<std::int64_t>(i), 0, place.columns});
            }
        }
    }
    const std::map<std::int64_t, std::vector<std::size_t>> owners = row_owners(named);
    for (std::size_t i = 0; i < named.size(); ++i) {
        const PaneRowPart& part = named[i];
        if (part.name.empty() || part.row < 0 ||
            part.row >= static_cast<std::int64_t>(region.rows.size())) {
            continue;
        }
        PanePart drawn;
        if (row_part_on(grid, part.row, region.rows[static_cast<std::size_t>(part.row)].text,
                        place.columns, named, owners.at(part.row), i, space, drawn)) {
            menu.parts.push_back(std::move(drawn));
        }
    }
    return menu;
}

/// ONE CANVAS LABEL AS THE PAINTER DRAWS IT (`paint_pane_canvas`): the bytes left of the body
/// dropped whole, the row cut at its right edge, a label outside its rows not drawn at all.
inline bool drawn_label(const PaneCanvasLabel& label, const PixelRect& body, std::string& text,
                        std::int64_t& x) {
    if (label.y < 0 || label.y > body.h - kPaneCanvasUnit) return false;
    std::size_t first = 0;
    x = label.x;
    while (first < label.text.size() && x < 0) {
        x = surface::add_cells(x, kPaneCanvasUnit);
        ++first;
    }
    if (x > body.w - kPaneCanvasUnit || first == label.text.size()) return false;
    const auto room = static_cast<std::size_t>((body.w - x) / kPaneCanvasUnit);
    const auto count = (std::min)(label.text.size() - first, room);
    if (count == 0) return false;
    text = label.text.substr(first, count);
    return true;
}

inline bool meets(const DeskRect& a, const PixelRect& b) {
    return a.w > 0 && a.h > 0 && b.w > 0 && b.h > 0 && a.x < b.x + b.w && a.x + a.w > b.x &&
           a.y < b.y + b.h && a.y + a.h > b.y;
}

inline PixelRect overlap(const PixelRect& a, const PixelRect& b) {
    const std::int64_t x0 = (std::max)(a.x, b.x), y0 = (std::max)(a.y, b.y);
    const std::int64_t x1 = (std::min)(a.x + a.w, b.x + b.w), y1 = (std::min)(a.y + a.h, b.y + b.h);
    return x1 > x0 && y1 > y0 ? PixelRect{x0, y0, x1 - x0, y1 - y0} : PixelRect{};
}

/// A COVERED WORD IS NOT SAID: of `words` and `parts`, each whose place meets a cover is dropped,
/// counted, and the covers over `rect` named once each, with the bounds of what they cover of it.
/// A cover says what covers (`by`) and where (`rect`), as `WorkshopWeave::covers_of` names each.
template <class Cover>
void cover_reading(const std::vector<Cover>& covers, const PixelRect& rect,
                   std::vector<PaneWord>& words, std::vector<v2::PanePart>& parts, PaneCover& out) {
    out = PaneCover{};
    std::int64_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    bool any = false;
    for (const Cover& c : covers) {
        const PixelRect over = overlap(c.rect, rect);
        if (over.w <= 0 || over.h <= 0) continue;
        if (std::find(out.by.begin(), out.by.end(), c.by) == out.by.end()) out.by.push_back(c.by);
        x0 = any ? (std::min)(x0, over.x) : over.x;
        y0 = any ? (std::min)(y0, over.y) : over.y;
        x1 = any ? (std::max)(x1, over.x + over.w) : over.x + over.w;
        y1 = any ? (std::max)(y1, over.y + over.h) : over.y + over.h;
        any = true;
    }
    if (any) out.rect = DeskRect{x0, y0, x1 - x0, y1 - y0};
    const auto covered = [&](const DeskRect& place) {
        for (const Cover& c : covers) {
            if (meets(place, c.rect)) return true;
        }
        return false;
    };
    const auto before = static_cast<std::int64_t>(words.size() + parts.size());
    words.erase(std::remove_if(words.begin(), words.end(),
                               [&](const PaneWord& w) { return covered(w.place); }),
                words.end());
    parts.erase(std::remove_if(parts.begin(), parts.end(),
                               [&](const v2::PanePart& p) { return covered(p.place); }),
                parts.end());
    out.words = before - static_cast<std::int64_t>(words.size() + parts.size());
}

/// WHILE A MENU IS OPEN, A PRIMARY PRESS OUTSIDE IT REACHES NOTHING BESIDE IT -- it closes the
/// menu, or is refused: what a pane still says there keeps its words and place, and no point.
inline void without_points(std::vector<PaneWord>& words, std::vector<v2::PanePart>& parts) {
    for (PaneWord& w : words) no_point(w);
    for (v2::PanePart& p : parts) no_point(p);
}

inline bool same_stamp(const PaneStamp& a, const v6::PaneView& b) {
    return a.provider == b.provider && a.pane == b.pane && a.holder == b.holder &&
           a.incarnation == b.incarnation && a.grant == b.grant && a.picture == b.picture;
}

inline bool same_stamp(const v2::PaneStamp& a, const v2::PaneStamp& b) {
    return a.provider == b.provider && a.pane == b.pane && a.holder == b.holder &&
           a.incarnation == b.incarnation && a.grant == b.grant && a.fingerprint == b.fingerprint;
}

inline PaneStamp stamp_from(const v6::PaneView& v) {
    return PaneStamp{v.provider, v.pane, v.holder, v.incarnation, v.grant, v.picture};
}

inline v2::PaneStamp fingerprint_stamp_from(const v6::PaneView& v) {
    return v2::PaneStamp{v.provider, v.pane, v.holder, v.incarnation, v.grant, v.fingerprint};
}

/// THE FOURTH VERSION OF A READING: the sixth's, its parts saying no availability and its stamp the
/// picture's number -- a reading judged by numbers (`pane_reading`), so in flight while a number
/// handed out is not yet aimed at.
inline v4::PaneView fourth_version(const v6::PaneView& v) {
    return v4::PaneView{v.provider, v.pane,        v.picture,     v.canvas, v.words,
                        first_parts(v.parts),      v.holder,      v.incarnation,
                        v.grant,    v.covered,     v.in_flight,   v.from,   v.total};
}

/// THE FIFTH VERSION OF A READING: the sixth's, its parts saying no availability.
inline v5::PaneView fifth_version(const v6::PaneView& v) {
    return v5::PaneView{v.provider,  v.pane,    v.picture,     v.canvas, v.words,
                        first_parts(v.parts),   v.holder,      v.incarnation,
                        v.grant,     v.fingerprint, v.covered, v.in_flight, v.from, v.total};
}

/// THE FINGERPRINT A STAMP NAMES: what the picture aimed at shows, and none for a canvas picture
/// no press is stamped with -- a room not drawn yet, a preview, a managed opening's picture -- so
/// the picture that first takes a press moves the stamp. A room whose own picture was refused
/// shows that refusal until another is admitted, so its stamp names what it shows.
inline std::int64_t stamp_fingerprint(const ExternalPane& pane) {
    if (pane.canvas.grant == 0 || pane.stamp.aimed != 0) return pane.stamp.aimed_fingerprint;
    return pane.refused_grant == pane.canvas.grant ? picture_fingerprint(pane) : 0;
}

/// A LAYOUT'S PART NAME: `layout:` and its name, a byte outside printable ASCII (and `%`, `#` and
/// `+`) spelled `%XX`, so no layout's part is the create tab's `layout:+`; a name two layouts share
/// is followed by `#` and each one's place in the weaver's order, which no spelled name can be.
inline std::vector<std::string> layout_part_names(const SetupState& setup) {
    std::vector<std::string> names;
    for (std::size_t at = 0; at < layout_count(setup); ++at) {
        std::string name = "layout:";
        for (const char c : layout_at(setup, at).name) {
            const auto byte = static_cast<unsigned char>(c);
            if (byte < 0x20u || byte >= 0x7Fu || c == '%' || c == '#' || c == '+') {
                static constexpr char kHex[] = "0123456789ABCDEF";
                name += '%';
                name += kHex[byte >> 4];
                name += kHex[byte & 0xFu];
            } else {
                name += c;
            }
        }
        names.push_back(std::move(name));
    }
    const std::vector<std::string> spelled = names;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (std::count(spelled.begin(), spelled.end(), spelled[i]) > 1) {
            names[i] += "#" + std::to_string(i + 1);
        }
    }
    return names;
}

/// ONE PAGE OF A READING, and at most what it spends: its decoded cells and its reply bytes.
template <class View>
struct Page {
    View view;
    std::int64_t cells = 0, bytes = 0;
};

/// THE ITEMS OF ONE READING FROM `from`, as many as `cells` decoded cells and `bytes` reply bytes
/// hold beside the rest. An item adds at most its own reply bytes and a comma (`reply_bytes`), so
/// the page's bytes are at most what is counted.
template <class View>
Page<View> page_of(const View& whole, std::int64_t from, std::int64_t cells, std::int64_t bytes) {
    Page<View> page;
    page.view = whole;
    page.view.words.clear();
    page.view.parts.clear();
    page.view.from = from;
    const loom::Value shell = loom::to_value(page.view);
    page.cells = decoded_cells(shell);
    page.bytes = reply_bytes(shell);
    const auto nwords = static_cast<std::int64_t>(whole.words.size());
    for (std::int64_t i = from; i < whole.total; ++i) {
        const loom::Value item =
            i < nwords ? loom::to_value(whole.words[static_cast<std::size_t>(i)])
                       : loom::to_value(whole.parts[static_cast<std::size_t>(i - nwords)]);
        const std::int64_t cost = 1 + decoded_cells(item);
        const std::int64_t size = 1 + reply_bytes(item);
        if (page.cells + cost > cells || page.bytes + size > bytes) break;
        page.cells += cost;
        page.bytes += size;
        if (i < nwords) page.view.words.push_back(whole.words[static_cast<std::size_t>(i)]);
        else page.view.parts.push_back(whole.parts[static_cast<std::size_t>(i - nwords)]);
    }
    return page;
}

template <class View>
std::int64_t items_of(const View& v) {
    return static_cast<std::int64_t>(v.words.size() + v.parts.size());
}

/// Why a page from `from` that holds nothing is refused though the reading goes on past it: its
/// item alone is past one reply or one decoded value. Nothing, for a page holding any.
template <class View>
std::string page_alone_refused(const Page<View>& page, std::int64_t from, std::int64_t total) {
    if (from >= total || items_of(page.view) > 0) return {};
    return "pane view unavailable: item " + std::to_string(from) + " alone is past one reply's " +
           std::to_string(kReplyByteBudget) + " bytes or one decoded value";
}

/// Why a page from `from` is refused, or nothing: below the first item, or past the total of a
/// reading not in flight.
template <class View>
std::string page_refused(std::int64_t from, const View& whole) {
    if (from < 0) return "pane view unavailable: `from` is below the reading's first item";
    if (!whole.in_flight && from > whole.total) {
        return "pane view unavailable: `from` is past the reading's " + std::to_string(whole.total) +
               " words and parts -- what covers the pane may have moved; read it again from the start";
    }
    return {};
}

/// A picture in flight, said at any page: no word.
template <class View>
View in_flight_page(View whole, std::int64_t from) {
    whole.from = from;
    return whole;
}

inline bool same_stamps(const std::vector<v2::PaneStamp>& a, const std::vector<v2::PaneStamp>& b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                      [](const v2::PaneStamp& x, const v2::PaneStamp& y) {
                          return same_stamp(x, y);
                      });
}

inline bool same_panes(const std::vector<v2::StampedPane>& a, const std::vector<v2::StampedPane>& b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                      [](const v2::StampedPane& x, const v2::StampedPane& y) {
                          return same_stamp(x.stamp, y.stamp) && x.aimed == y.aimed;
                      });
}

} // namespace zengine::workshop::desk_read

#endif // ZENGINE_WORKSHOP_DESK_READ_HPP
