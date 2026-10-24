// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_DESK_READ_HPP
#define ZENGINE_WORKSHOP_DESK_READ_HPP

// THE DESK, READ AS ARITHMETIC: where a medium draws Workshop's own glyphs, the words and parts
// said there and the point a press reaches each by, a menu's lines, what covers a pane, a reading's
// stamp and its earlier versions, and how much of a reading one reply holds. Each reads only what
// it is handed; `WorkshopWeave`'s answers (weave_desk_read.cpp) spend them.
// Workshop law: agents/workshop/desk-read.md (+3 registers; agents/workshop.md routes)

#include "decoded_cells.hpp"
#include "pane_rows.hpp"
#include "pane_view.hpp"
#include "pane_vocabulary.hpp"
#include "panes.hpp"
#include "reply_bytes.hpp"
#include "screen.hpp"
#include "setup.hpp"

#include "graphics/pixels.hpp"
#include "graphics/vocabulary.hpp"
#include "input/vocabulary.hpp"
#include "workshop/screen_planes.hpp"
#include "workshop/pane_text_fit.hpp"
#include "pane_text_fit.hpp"

#include <zen/weave/standard_shapes.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace zengine::workshop::desk_read {

/// What a reading's handler says: the shape, or a refusal in words.
template <class T>
using Said = std::variant<T, loom::Refused>;

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
/// canvas pixels -- the face's where the fit sets type, one cell each where text is a cell.
struct GlyphGrid {
    std::int64_t x = 0, y = 0;
    std::int64_t advance = graphics::kCellPx, line = graphics::kCellPx;
    constexpr GlyphGrid(std::int64_t at_x, std::int64_t at_y, std::int64_t glyph_advance,
                        std::int64_t glyph_line) noexcept
        : x(at_x), y(at_y), advance(glyph_advance), line(glyph_line) {}
};

inline GlyphGrid glyph_grid(const RegionFit& fit) {
    if (fit.graphical()) {
        return GlyphGrid{fit.view.x + fit.origin_x, fit.view.y + fit.origin_y, fit.advance_px,
                         fit.line_px};
    }
    return GlyphGrid{fit.view.x, fit.view.y, graphics::kCellPx, graphics::kCellPx};
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
    x = graphics::cell_of_pixel(g.x + column * g.advance);
    y = graphics::cell_of_pixel(g.y + row * g.line);
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

/// A PART SAYS ITS AVAILABILITY AS IT WAS NAMED.
inline void say_available(PanePart& out, const RowPart& part) { out.available = part.available; }

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
        // ...ON ITS ROWS: a word whose line's middle the part holds, the part cut where its clip is.
        const std::int64_t middle = w.place.y + w.place.h / 2;
        if (bytes == 0 || middle < place.y || middle >= place.y + place.h) {
            continue;
        }
        const std::int64_t advance = w.place.w / bytes;
        if (advance <= 0) continue;
        const std::int64_t first = (std::max<std::int64_t>)(
            0, -graphics::floor_div_px(w.place.x - place.x, advance));
        const std::int64_t end = (std::min<std::int64_t>)(
            bytes, graphics::floor_div_px(place.x + place.w - w.place.x, advance));
        if (end <= first) continue;
        const std::string piece =
            w.text.substr(static_cast<std::size_t>(first), static_cast<std::size_t>(end - first));
        out += out.empty() ? piece : " " + piece;
    }
    return out;
}

/// THE MENU ON THE SCREEN: Workshop's contextual menu, read from the painter's own composition of
/// it with the lines it names; or a pane's, whose and where -- its lines its presenter's, read
/// from the window that shows them (`WorkshopWeave::presented_menu_reading`). At most one is open.
inline DeskMenu desk_menu(const Session& s, const Screen& sc) {
    DeskMenu menu;
    if (s.presented.open) {
        menu.open = true;
        menu.office = s.presented.office;
        menu.pane = s.presented.pane;
        menu.picture = s.presented.picture;
        menu.place = desk_rect(presented_bounds(s, sc));
        return menu;
    }
    if (!s.context.open) {
        return menu;
    }
    ScreenPlane layer;
    menu.office = kWorkshopProvider;
    const PixelRect bounds = context_bounds(s, sc);
    paint_context(layer, s, sc);
    menu.open = true;
    menu.place = desk_rect(bounds);
    if (layer.texts.empty()) {
        return menu;
    }
    const ProsePlace place = prose_place(bounds, sc);
    const GlyphGrid grid = glyph_grid(place.fit);
    const ScreenRegion& region = layer.texts.back();
    const std::int64_t space = input_space_of(sc);
    for (std::size_t i = 0; i < region.rows.size(); ++i) {
        std::string text = without_trailing_blanks(region.rows[i].text);
        const std::int64_t row = static_cast<std::int64_t>(i);
        PaneWord line = word_on(grid, row, 0, std::move(text), space);
        line.word = row;
        menu.lines.push_back(std::move(line));
    }
    // THE LINES IT NAMES, by the action or group each line shows.
    std::vector<RowPart> named;
    const std::vector<std::string> names = context_line_names(s, sc);
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (!names[i].empty()) {
            named.push_back(RowPart{names[i], static_cast<std::int64_t>(i), 0, place.columns});
        }
    }
    const std::map<std::int64_t, std::vector<std::size_t>> owners = row_owners(named);
    for (std::size_t i = 0; i < named.size(); ++i) {
        const RowPart& part = named[i];
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
                   std::vector<PaneWord>& words, std::vector<PanePart>& parts, PaneCover& out) {
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
                               [&](const PanePart& p) { return covered(p.place); }),
                parts.end());
    out.words = before - static_cast<std::int64_t>(words.size() + parts.size());
}

/// WHILE A MENU IS OPEN, A PRIMARY PRESS OUTSIDE IT REACHES NOTHING BESIDE IT -- it closes the
/// menu, or is refused: what a pane still says there keeps its words and place, and no point.
inline void without_points(std::vector<PaneWord>& words, std::vector<PanePart>& parts) {
    for (PaneWord& w : words) no_point(w);
    for (PanePart& p : parts) no_point(p);
}

inline bool same_stamp(const PaneStamp& a, const PaneStamp& b) {
    return a.provider == b.provider && a.pane == b.pane && a.holder == b.holder &&
           a.incarnation == b.incarnation && a.epoch == b.epoch && a.fingerprint == b.fingerprint;
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

inline bool same_stamps(const std::vector<PaneStamp>& a, const std::vector<PaneStamp>& b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                      [](const PaneStamp& x, const PaneStamp& y) { return same_stamp(x, y); });
}

} // namespace zengine::workshop::desk_read

#endif // ZENGINE_WORKSHOP_DESK_READ_HPP
