// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_WORKSHOP_SUPPORT_HPP
#define ZENGINE_TESTS_WORKSHOP_SUPPORT_HPP

// Shared support for the Workshop suites.
//
// WHAT MAY LIVE HERE, and what may not. Product behaviour stays in `workshop/`; a case
// stays in the suite that proves its behaviour; this file holds only the fixtures, rigs
// and canvas readers that MORE THAN ONE Workshop suite needs. A helper one suite uses
// stays in that suite's own file, and moving one here is a decision, not a tidy-up —
// every suite pays this header's parse on every build.
//
// The suites it serves:
//
//   test_workshop_document.cpp      the authored material and the maker's hands on it
//   test_workshop_screen.cpp        composition and geometry
//   test_workshop_panels.cpp        the panels Workshop ships, and attention
//   test_workshop_panes.cpp         the external pane seam
//   test_workshop_persistence.cpp   what survives a process
//   test_workshop_editor.cpp        the built-in source editor and its document
//
// `test_workshop_load.cpp` — which artifacts are in the room at all — carries its own
// rigs and does not include this file: its cases own a Switchboard and a Kernel each and
// drain in bounded turns, which is a different harness from the one below.
//
// THE HELPERS KEEP THE FILE SCOPE THEY WERE WRITTEN IN. They were an anonymous namespace
// in one translation unit and they are `inline` at namespace scope in six; wrapping two
// thousand lines of support in a namespace would change unqualified lookup inside every
// one of them for no gain. `inline` rather than `static` is what makes an unused helper
// silent under `-Wunused-function`: no suite uses all of these.
//
// It includes `doctest.h` because the fixtures assert — `rich_document()` REQUIREs the
// removal it depends on, and `open_pane()` fails with a sentence rather than spinning.

#include "doctest.h"

#include "workshop/document.hpp"
#include "workshop/persist.hpp"
#include "workshop/property.hpp"
#include "workshop/screen.hpp"
#include "workshop/setup.hpp"
#include "workshop/setup_persist.hpp"
#include "workshop/keymap.hpp"
#include "workshop/keymap_persist.hpp"
#include "workshop/prefs_persist.hpp"
#include "workshop/user_paths.hpp"
#include "workshop/weave.hpp"
#include "workshop/vocabulary.hpp"

#include "composer/vocabulary.hpp"
#include "introspection/loaded.hpp"
#include "introspection/powers.hpp"
#include "introspection/resolved.hpp"
#include "introspection/vocabulary.hpp"
#include "operator/catalog.hpp"
#include "operator/host_surface.hpp"
#include "operator/provider_host.hpp"
#include "workshop/arrangement.hpp"
#include "workshop/arrangement_vocabulary.hpp"
#include "workshop/host_sources.hpp"
#include "workshop/load_execute.hpp"
#include "workshop/load_plan.hpp"
#include "workshop/recipes.hpp"
#include "workshop/sample_door.hpp"
#include "workshop/sample_presentation.hpp"
#include "workshop/sample_vocabulary.hpp"
#include "surface/skin_sdl_plan.hpp"
#include "timer/vocabulary.hpp"
#include "surface/skin_tui.hpp"
#include "surface/vocabulary.hpp"
#include "ui/layout.hpp"
#include "ui/vocabulary.hpp"

#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>

#include <zen/history/logger.hpp>   // the durable record attention must not reach
#include <zen/history/recorder.hpp> // ...and the working memory it must not reach either

#include <zen/host/terminal_wiring.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/terminal/input_lex.hpp>
#include <zen/terminal/session.hpp>
#include <zen/terminal/transcript.hpp>
#include <zen/terminal/vocabulary.hpp>
#include <zen/weave.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <memory>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

using namespace zengine::workshop;
using loom::schema_of;
namespace component = zengine::component;
namespace input = zengine::input;
namespace op = zengine::op;
namespace surface = zengine::surface;
namespace ui = zengine::ui;
namespace load = zengine::workshop::load;


/// A `component::TextBox` holding a text of this LENGTH, with its caret and its window where
/// a case wants them — the fixture the pane's geometry helpers take since HD-5 moved those
/// two indices inside the component they belong to.
///
/// THE WINDOW IS REACHED THROUGH THE REAL DOOR, never written. `keep_caret_visible(caret -
/// first)` is the only way to move it, and it lands exactly on `first` for every value a case
/// can ask for: rule 1 caps the window at `size - room`, which is `first` itself here, and
/// rule 3 pulls it up to the same place. A window past the end of the text (`first > length`)
/// collapses to the end, which is the answer the pre-HD-5 helpers reached by clamping.
inline component::TextBox box_of(std::size_t length, std::size_t caret, std::size_t first) {
    component::TextBox b;
    b.set(std::string(length, 'x'), caret);
    b.keep_caret_visible(caret > first ? static_cast<std::int64_t>(caret - first) : 0);
    return b;
}

/// Two rectangles that SHARE A NAME. The fixture is the point: if a name were
/// identity, this document could not exist.
inline WorkshopDoc two_panels() {
    WorkshopDoc d;
    doc::add(d, "panel", 3, 2, ui::Extent{ui::kExtentPercent, 60}, ui::Extent{ui::kExtentCells, 6});
    doc::add(d, "panel", 6, 10, ui::Extent{ui::kExtentCells, 14}, ui::Extent{ui::kExtentCells, 4});
    return d;
}

/// A document of `n` objects in creation order, identities 1..n.
///
/// The fixture that was missing for a long time, and the reason a whole class of
/// panel defect survived: every live run and every screen case used two objects,
/// and the 300- and 500-link documents were exercised headlessly, where nothing
/// paints. More objects than the OBJECTS panel is tall is the shape those cases
/// could not express.
inline WorkshopDoc many(std::int64_t n) {
    WorkshopDoc d;
    for (std::int64_t i = 0; i < n; ++i) {
        doc::add(d, "panel", 0, 0, ui::Extent{ui::kExtentCells, 2},
                 ui::Extent{ui::kExtentCells, 1});
    }
    return d;
}

/// Type the whole of `text` into a row, one character at a time -- the way a
/// maker's keystrokes actually arrive.
inline void type_all(Row& row, const std::string& text) {
    for (const char c : text) {
        row.type(c);
    }
}

/// EVERYTHING ON A CANVAS THAT A CELL MEDIUM WOULD SHOW AS TEXT, in painter's
/// order -- the labels, and then the text regions projected onto cells.
///
/// Since HD-1 a canvas may carry a bounded region whose interior a graphical
/// medium sets in real type. Every assertion in this file that asks "what would a
/// A CANVAS IS A LIST OF PLANES SINCE WIND-2a, and these four are how this suite reads
/// one. Three of them concatenate a KIND across the planes, in the order a medium walks
/// them, which is the right shape for "was this drawn" and for "which of these two
/// same-kind primitives is later". None of them is a visibility oracle -- what is SEEN at
/// a cell is `cell_text_of` below (text) and `rasterized` (the whole picture), and both
/// execute the Skin's own two-level order rather than flattening it.
inline std::vector<surface::SurfaceRect> all_rects(const surface::SurfaceCanvas& c) {
    std::vector<surface::SurfaceRect> out;
    for (const surface::SurfaceLayer& l : c.layers) {
        out.insert(out.end(), l.rects.begin(), l.rects.end());
    }
    return out;
}

inline std::vector<surface::SurfaceLabel> all_labels(const surface::SurfaceCanvas& c) {
    std::vector<surface::SurfaceLabel> out;
    for (const surface::SurfaceLayer& l : c.layers) {
        out.insert(out.end(), l.labels.begin(), l.labels.end());
    }
    return out;
}

inline std::vector<surface::SurfaceTextRegion> all_texts(const surface::SurfaceCanvas& c) {
    std::vector<surface::SurfaceTextRegion> out;
    for (const surface::SurfaceLayer& l : c.layers) {
        out.insert(out.end(), l.texts.begin(), l.texts.end());
    }
    return out;
}

/// THE SAME CANVAS WITHOUT THE WORKSPACE'S OWN PLANE -- what the screen's panels, panes and
/// overlays published, and nothing the DOCUMENT published (TYPE-1).
///
/// It exists because the workspace plane carries text regions now: one per placed object,
/// `kGroundBeneath`, holding the maker's authored name over the object's own material. A case
/// asking "how many bounded regions does this screen's chrome publish", or indexing the
/// projected rows of a panel, is asking about the planes AFTER the workspace, and before
/// TYPE-1 that distinction cost nothing because the workspace published none.
///
/// A PLANE AND NOT A PREDICATE ON THE REGIONS, because that is the actual fact: `paint`
/// writes the workspace whole into `layers.front()` before any pane exists (WIND-2a), so
/// dropping the first plane is exactly "everything a pane or the chrome drew". Cases that ask
/// about the names themselves read `object_names` below.
inline surface::SurfaceCanvas without_workspace(const surface::SurfaceCanvas& c) {
    surface::SurfaceCanvas out = c;
    if (!out.layers.empty()) {
        out.layers.erase(out.layers.begin());
    }
    return out;
}

/// THE AUTHORED OBJECTS' NAMES, as the workspace plane published them (TYPE-1).
inline std::vector<surface::SurfaceTextRegion> object_names(const surface::SurfaceCanvas& c) {
    return c.layers.empty() ? std::vector<surface::SurfaceTextRegion>{} : c.layers.front().texts;
}

/// WHAT A TERMINAL WITH NO USEFUL COLOUR SHOWS -- the canvas's characters with every SGR
/// sequence removed (TYPE-1's §14 witness).
///
/// It is the whole of the monochrome question and it is worth being able to ask directly: a
/// role is ink, and ink is the half of a medium's answer that a monochrome terminal does not
/// receive. `glyph_for_role` is the other half, and a case reading this string is reading
/// exactly what survives when the first half is thrown away. `\x1b[2K` goes with the rest --
/// it is an erase, not a character.
inline std::string plain_cells(const surface::SurfaceCanvas& c) {
    const std::string body = surface::canvas_body(c);
    std::string out;
    for (std::size_t i = 0; i < body.size(); ++i) {
        if (body[i] == '\x1b') {
            while (i < body.size() && body[i] != 'm' && body[i] != 'K') {
                ++i;
            }
            continue; // the terminator itself is consumed by the loop's own ++i
        }
        out += body[i];
    }
    return out;
}

/// A CELL COUNT ON THE FINE LATTICE (WUX-2) — the one multiply a case that thinks in
/// whole cells performs to author or compare pane geometry, which since WUX-2 is
/// sub-units. Spelled short because it appears wherever a case says "40 cells".
inline constexpr std::int64_t subs(std::int64_t cells) { return cells * surface::kCellSubs; }

/// THE PLANE A CASE THAT BUILDS ITS OWN CANVAS BY HAND WORKS ON, created on first use.
/// A case exercising ONE painter is asking about one presentation, which is one plane.
inline surface::SurfaceLayer& plane(surface::SurfaceCanvas& c) {
    if (c.layers.empty()) {
        c.layers.emplace_back();
    }
    return c.layers.back();
}

/// ONE REGION ON A CANVAS OF ITS OWN, at the same extent — what a case asks for when it
/// wants the plan for exactly this presentation and not for everything else on the screen.
///
/// A FRESH CANVAS, NOT THE PUBLISHED ONE WITH A PLANE OVERWRITTEN (WIND-2a): a Workshop
/// canvas carries a plane per presentation now, so replacing one plane's regions would
/// leave every other plane's still on it.
inline surface::SurfaceCanvas canvas_of_region(const surface::SurfaceCanvas& like,
                                               const surface::SurfaceTextRegion& r) {
    surface::SurfaceCanvas out;
    out.width = like.width;
    out.height = like.height;
    out.layers.emplace_back();
    out.layers.back().texts.push_back(r);
    return out;
}

/// EVERY REGION THIS MEDIUM SETS IN REAL TYPE, and every region it draws as cells —
/// gathered across the planes in the order a medium walks them.
///
/// They are for the PARTITION questions ("is this region in exactly one of the two lists",
/// "how many rows did the pane get"), not for order: the ordering law is `test_surface`'s,
/// stated over planes built for it, and `rasterized` is what this file asks when it wants
/// to know what is actually on top.
inline std::vector<surface::PlanTextRegion> plan_regions_of(const surface::SurfaceCanvas& c,
                                                            const surface::SurfaceExtent& metric,
                                                            const surface::PlanSize& size) {
    std::vector<surface::PlanTextRegion> out;
    for (const surface::SurfaceLayer& l : c.layers) {
        for (surface::PlanTextRegion& r : surface::plan_layer_regions(l, metric, size)) {
            out.push_back(std::move(r));
        }
    }
    return out;
}

inline std::vector<surface::ProjectedRow> projected_of(const surface::SurfaceCanvas& c) {
    std::vector<surface::ProjectedRow> out;
    for (const surface::SurfaceLayer& l : c.layers) {
        for (surface::ProjectedRow& r : surface::project_text_regions(l)) {
            out.push_back(std::move(r));
        }
    }
    return out;
}

inline std::vector<surface::ProjectedRow> projected_of(const surface::SurfaceCanvas& c,
                                                       const surface::SurfaceExtent& metric) {
    std::vector<surface::ProjectedRow> out;
    for (const surface::SurfaceLayer& l : c.layers) {
        for (surface::ProjectedRow& r : surface::project_text_regions(l, metric)) {
            out.push_back(std::move(r));
        }
    }
    return out;
}

/// maker see at cell (x, y)" is asking the CELL question, so it goes through the
/// same projection the terminal Skins use (`surface::project_text_regions`) rather
/// than through a second reading of the region invented here. That is deliberate
/// rather than convenient: if the projection ever stopped agreeing with what a
/// character medium draws, these assertions would be describing a picture nothing
/// paints -- and it is the picture the golden-byte suite pins in test_surface.cpp.
///
/// THE SKIN'S OWN TWO-LEVEL ORDER, AND NOT A FLATTENING OF IT (WIND-2a): one plane at a
/// time, and inside a plane its labels and then its regions -- which is exactly what
/// `canvas_body` does with the same two lists. So the LAST entry at a cell is the topmost
/// text there, across planes and across kinds, and a case can no longer be right about a
/// picture the medium does not draw. (Text only: a later plane's RECT covering an earlier
/// plane's label is a question for `rasterized`, which runs the whole rasterizer.)
inline std::vector<surface::SurfaceLabel> cell_text_of(const surface::SurfaceCanvas& c) {
    std::vector<surface::SurfaceLabel> out;
    for (const surface::SurfaceLayer& l : c.layers) {
        out.insert(out.end(), l.labels.begin(), l.labels.end());
        for (surface::ProjectedRow& p : surface::project_text_regions(l)) {
            out.push_back(std::move(p.label));
        }
    }
    return out;
}

/// Find a label's text at a canvas cell, or "" -- how the screen tier asks what
/// a maker would see at a place.
inline std::string label_at(const surface::SurfaceCanvas& c, std::int64_t x, std::int64_t y) {
    for (const surface::SurfaceLabel& l : cell_text_of(c)) {
        if (l.x == x && l.y == y) {
            return l.text;
        }
    }
    return {};
}

/// ONE PROSE ROW OF THE INSPECTOR'S PROPERTY BODY, as a maker reads it (HD-6).
///
/// The body is a bounded REGION since HD-6, and a region owns what is inside its bounds --
/// so its cell projection pads every row to the region's full width, exactly as the terminal
/// pane's has always done. That padding is a real fact about the picture (it is what erases
/// what was underneath) and it is noise in an assertion about what a row SAYS, so this trims
/// it and the cases below read the way they read before the body had bounds.
///
/// It goes through `label_at`, which goes through the real cell projection, so a caret is
/// still inserted at its own column and a row cut at the body's width is still cut.
inline std::string inspector_row(const surface::SurfaceCanvas& c, std::int64_t x, std::int64_t y) {
    std::string text = label_at(c, x, y);
    while (!text.empty() && text.back() == ' ') {
        text.pop_back();
    }
    return text;
}

/// THE SENTENCE THE TOOL IS SAYING, as a maker reads it (TYPE-0).
///
/// The notice is a bounded region since TYPE-0 -- two cells, which is the smallest room
/// that holds one row of a real face -- so it is read exactly as the Inspector's rows are:
/// through the cell projection, with the region's padding trimmed off the assertion.
inline std::string notice_line(const surface::SurfaceCanvas& c, const Screen& sc) {
    return inspector_row(c, 0, sc.notice_y);
}

/// THE TERMINAL PANE'S REGION on a canvas, found by the place it was drawn at.
///
/// BY PLACE, NOT BY POSITION (HD-6). `texts[0]` was the pane for as long as the pane was the
/// only region this application published; the Inspector's property body is published on
/// every paint since HD-6 and is painted BEFORE the overlay, so an index now names the
/// Inspector. That is the same defect a second copy of any geometry is, arriving in a test
/// helper: it would have gone on passing while asserting things about the wrong rectangle.
///
/// IT RETURNS A POINTER and the call sites dereference it, which is not style: GCC 13's
/// `-Wdangling-reference` fires on a function returning a REFERENCE when any argument is a
/// temporary, and `screen_of(t.session())` is one. The reference would have been perfectly
/// alive -- it points into the canvas, not into the Screen -- but a heuristic that cannot know
/// that is `-Werror` on the MinGW lane, and a pointer says the same thing without arguing.
inline const surface::SurfaceTextRegion* pane_of(const surface::SurfaceCanvas& c, const Screen& sc) {
    for (const surface::SurfaceLayer& layer : c.layers) {
        for (const surface::SurfaceTextRegion& r : layer.texts) {
            if (r.x == sc.terminal_x && r.y == sc.terminal_y) {
                return &r;
            }
        }
    }
    FAIL("no terminal pane region on this canvas");
    return nullptr;
}

/// The completion list on a canvas, or nullptr — the region that is neither the pane nor
/// the Inspector's property body.
///
/// BY PLACE, NOT BY POSITION (HD-6). It used to be "the second region", which was true for
/// exactly as long as the pane was the only other one; since HD-6 the Inspector publishes
/// its property body on every paint, so an index would name the wrong region on every canvas
/// this file paints. The pane's own x is `Screen::terminal_x` and the list sits in the same
/// column above it, so "not the pane's top row" identifies it without a second arithmetic.
inline const surface::SurfaceTextRegion* list_of(const surface::SurfaceCanvas& c, const Screen& sc) {
    for (const surface::SurfaceLayer& layer : c.layers) {
        for (const surface::SurfaceTextRegion& r : layer.texts) {
            if (r.x == sc.terminal_x && r.y != sc.terminal_y) {
                return &r;
            }
        }
    }
    return nullptr;
}

/// What is actually SEEN at a cell where several labels landed: the LAST one
/// written, because painter's order is list order and every Skin draws it that
/// way. `label_at` above answers with the first, which is the bottom of the
/// stack -- fine everywhere nothing overlaps, and exactly wrong for asking
/// whether an overlay covered what is under it.
inline std::string topmost_at(const surface::SurfaceCanvas& c, std::int64_t x, std::int64_t y) {
    std::string seen;
    for (const surface::SurfaceLabel& l : cell_text_of(c)) {
        if (l.x == x && l.y == y) {
            seen = l.text;
        }
    }
    return seen;
}

/// THE SETUP A CASE THAT BUILT ITS `Panels` BY HAND IS IMPLICITLY WORKING UNDER (WIND-2).
///
/// One authored row per open panel, in open order, with no override on any axis and the
/// identity front ranks -- which is exactly what `reconcile` would have produced from that
/// setup. So a case that assembles a `Panels` directly still asks the ONE resolver the same
/// question the application asks, rather than a second one with a defaulted argument.
inline Setup setup_for(const Panels& panels) {
    Setup s;
    s.name = "case";
    for (const Panel& p : panels.open) {
        if (is_runtime_kind(p.kind)) {
            if (const RuntimePane* row = panels.runtime.of_kind(p.kind)) {
                (void)add_pane(s, PaneRef{row->provider, row->pane});
            }
            continue;
        }
        (void)add_pane(s, pane_ref_of(p.kind));
    }
    return s;
}

/// THE INFO PANEL'S BODY, resolved the way the painter resolves it — through `bounds_of` and
/// `info_body_place`, never through a second arithmetic (HD-6, widened by HD-7). A case that
/// computed the rectangle or the row of a heading for itself would pass while the picture and
/// the hit test disagreed.
inline InfoBodyPlace body_of(const WorkshopDoc& d, const Session& s) {
    const Screen sc = screen_of(s);
    return info_body_place(bounds_of(s.panels, s.setup.active, panel::kInfo, sc).rect, sc, d, s);
}

/// The body region a canvas actually published, or nullptr.
inline const surface::SurfaceTextRegion* body_on(const surface::SurfaceCanvas& c,
                                                 const InfoBodyPlace& p) {
    for (const surface::SurfaceLayer& layer : c.layers) {
        for (const surface::SurfaceTextRegion& r : layer.texts) {
            if (r.x == p.region_x && r.y == p.region_y) {
                return &r;
            }
        }
    }
    return nullptr;
}

/// ONE OBJECT ROW OF THE BODY, as a maker reads it in a CELL medium (HD-7).
///
/// A prose row of the body is a cell row when the medium has no type, so this is the same
/// arithmetic `project_text_regions` performs and no second copy of it.
inline std::string object_row(const surface::SurfaceCanvas& c, const WorkshopDoc& d, const Session& s,
                              std::int64_t n) {
    const InfoBodyPlace p = body_of(d, s);
    return inspector_row(c, p.region_x, p.region_y + kInfoHeadingRows + n);
}

/// ONE PROPERTY ROW OF THE BODY, and the row `PROPERTIES` itself sits on (HD-7).
///
/// RESOLVED, NEVER ADDED TO A CONSTANT. `kRowsY = 8` used to be the property body's first row
/// and these cases used to say `kRowsY + n`; the heading now moves with the object list above
/// it, so a case that kept a constant would be asserting about a row nobody drew.
inline std::string property_row(const surface::SurfaceCanvas& c, const WorkshopDoc& d, const Session& s,
                                std::int64_t n) {
    const InfoBodyPlace p = body_of(d, s);
    return inspector_row(c, p.region_x,
                         p.region_y + kInfoHeadingRows + p.heading_row + 1 + n);
}

inline std::string properties_heading(const surface::SurfaceCanvas& c, const WorkshopDoc& d,
                                      const Session& s) {
    const InfoBodyPlace p = body_of(d, s);
    return inspector_row(c, p.region_x, p.region_y + kInfoHeadingRows + p.heading_row);
}

/// The OBJECTS list exactly as a maker reads it: every row of its share of the body, in
/// order, including the ones that are empty.
inline std::vector<std::string> object_lines(const surface::SurfaceCanvas& c, const WorkshopDoc& d,
                                             const Session& s) {
    const InfoBodyPlace p = body_of(d, s);
    std::vector<std::string> lines;
    for (std::size_t i = 0; i < p.objects_rows; ++i) {
        lines.push_back(inspector_row(c, p.region_x, p.region_y + kInfoHeadingRows +
                                                         static_cast<std::int64_t>(i)));
    }
    return lines;
}

inline bool has_rect(const surface::SurfaceCanvas& c, std::int64_t x, std::int64_t y, std::int64_t w,
                     std::int64_t h, std::int64_t role) {
    for (const surface::SurfaceRect& r : all_rects(c)) {
        if (r.x == x && r.y == y && r.w == w && r.h == h && r.role == role) {
            return true;
        }
    }
    return false;
}

/// One canvas as the terminal medium's own pure function draws it, one string per screen row,
/// with the SGR and erase escapes taken out so what is left is the picture.
inline std::vector<std::string> rasterized(const surface::SurfaceCanvas& canvas) {
    const std::string body = surface::canvas_body(canvas);
    std::vector<std::string> rows;
    std::size_t at = 0;
    while (at < body.size()) {
        const std::size_t end = body.find("\r\n", at);
        if (end == std::string::npos) {
            break;
        }
        std::string row;
        for (std::size_t i = at; i < end; ++i) {
            if (body[i] == '\x1b') { // skip the SGR/erase escapes; keep the picture
                while (i < end && body[i] != 'm' && body[i] != 'K') {
                    ++i;
                }
                continue;
            }
            row += body[i];
        }
        rows.push_back(row);
        at = end + 2;
    }
    return rows;
}

struct SeenState {
    std::int64_t frames = 0;
    ZEN_SHAPE(SeenState, 1, ZEN_FIELD(frames));
};

/// An ordinary Skin's ears: whatever Workshop published, kept as values.
class Painter : public loom::WeaveBase<Painter, SeenState,
                                       loom::Accept<surface::SurfaceCanvas, surface::SurfaceText>,
                                       loom::Emit<>> {
public:
    Painter(std::vector<surface::SurfaceCanvas>& canvases,
            std::vector<surface::SurfaceText>& notes)
        : canvases_(&canvases), notes_(&notes) {}
    void on(const surface::SurfaceCanvas& c, loom::Mail&) {
        ++state_.frames;
        canvases_->push_back(c);
    }
    void on(const surface::SurfaceText& t, loom::Mail&) { notes_->push_back(t); }

private:
    std::vector<surface::SurfaceCanvas>* canvases_;
    std::vector<surface::SurfaceText>* notes_;
};



/// WHOEVER HOLDS `zengine.skin` -- an ordinary weave that records not only WHAT it
/// was told but WHO told it. `mail.sender()` is the BUS STAMP: it cannot be
/// written by a payload and cannot be chosen by whoever composed the message,
/// which is what makes it the one right instrument for "which identity spoke".
class SkinSeat : public loom::WeaveBase<SkinSeat, SeenState,
                                        loom::Accept<surface::SurfaceText, surface::ClipboardCopy,
                                                     surface::ClipboardTextRequested,
                                                     surface::SurfacePlacementRemembered>,
                                        loom::Emit<loom::Ack, surface::ClipboardText>> {
public:
    void on(const surface::SurfaceText& t, loom::Mail& mail) {
        ++state_.frames;
        heard.push_back(t);
        from.push_back(mail.sender());
        // Answers ONE slot, so Workshop's own status publications -- which are not asks and
        // which arrive here constantly -- are never answered by accident.
        if (t.slot == "ask") {
            (void)mail.answer(loom::Ack{});
        }
    }

    /// The platform's clipboard, as the real media hold it (QR-11): a readable medium
    /// takes every copy the process says (the SDL skin's SDL_SetClipboardText), an
    /// unreadable one lets the offer pass (the terminal's OSC 52 claims nothing).
    void on(const surface::ClipboardCopy& c, loom::Mail&) {
        if (readable_medium) {
            platform = c.text;
        }
    }

    /// A paste ask: answered the way SkinT answers it — the medium's current value, or
    /// the honest cannot-say — and counted, because "read exactly once, exactly on
    /// request" is a custody claim the cases pin.
    void on(const surface::ClipboardTextRequested&, loom::Mail& mail) {
        ++clipboard_reads;
        (void)mail.answer(surface::ClipboardText{readable_medium, readable_medium ? platform
                                                                                  : std::string()});
    }

    /// A remembered placement offered back at restore (WUX-3): recorded the way the real
    /// media receive it, so a case can assert exactly what the desk's memory handed over.
    void on(const surface::SurfacePlacementRemembered& p, loom::Mail&) {
        offered.push_back(p);
    }

    /// Who said this exact text, or the invalid id if nobody did.
    loom::WeaveId who_said(const std::string& text) const {
        for (std::size_t i = 0; i < heard.size(); ++i) {
            if (heard[i].text == text) {
                return from[i];
            }
        }
        return loom::WeaveId{};
    }

    std::vector<surface::SurfaceText> heard;
    std::vector<loom::WeaveId> from;
    std::vector<surface::SurfacePlacementRemembered> offered; ///< placement offers (WUX-3)
    bool readable_medium = true; ///< false = the terminal's standing truth
    std::string platform;        ///< what "the platform clipboard" holds, when readable
    int clipboard_reads = 0;
};

/// One participant's own record, filtered -- the transcript is a MODEL, so a test asks it
/// structured questions rather than grepping rendered prose.
inline std::vector<loom::TranscriptEntry> of_kind(const loom::TerminalSession& me,
                                                  loom::TranscriptKind kind) {
    std::vector<loom::TranscriptEntry> out;
    for (const loom::TranscriptEntry& e : me.transcript().entries()) {
        if (e.kind == kind) {
            out.push_back(e);
        }
    }
    return out;
}

/// Everything the overlay is showing, as one string -- the pane's own column, top to bottom.
inline std::string pane_text(const surface::SurfaceCanvas& c) {
    std::string out;
    for (const surface::SurfaceLabel& l : cell_text_of(c)) {
        if (l.x == kMinScreen.terminal_x && l.y >= kMinScreen.terminal_y) {
            out += l.text;
            out += '\n';
        }
    }
    return out;
}

/// ONE CONDITION OUT OF A PROJECTION, BY KEY -- or nothing, which is the answer a resolved
/// condition gives. It takes the vector rather than a fixture so the two rigs share
/// one spelling, and it deliberately reads the PROJECTION rather than `Session::conditions`:
/// a derived condition is never in the held set, and a case that looked there would be
/// asserting about the wrong half of the model.
inline const Condition* condition_by_key(const std::vector<Condition>& all, const std::string& key) {
    for (const Condition& c : all) {
        if (c.key == key) {
            return &c;
        }
    }
    return nullptr;
}

/// A live Workshop: the real weave on a real bus, driven only by published
/// input messages. Nothing here reaches past the message boundary except to
/// READ the result.
struct Live {
    loom::Switchboard bus;
    HostContext host;
    std::vector<surface::SurfaceCanvas> canvases;
    std::vector<surface::SurfaceText> notes;
    WorkshopWeave* w = nullptr;
    loom::WeaveId workshop_id{};
    loom::WeaveId terminal_id{};

    Live() {
        auto weave = std::make_unique<WorkshopWeave>(host);
        w = weave.get();
        loom::Grant grant = loom::emit_default_grant(*w);
        loom::allow_poke_answers(grant);
        // IN THE OFFICE, AS THE HOST MOUNTS IT (WP-0). Workshop's own weave now holds
        // `zengine.workshop`, because the pane protocol is authored AS that office and a
        // rig that mounted it anonymously would silently produce a Workshop whose ask and
        // whose room grants are refused at the bus -- a difference no pre-WP-0 case could
        // see and every WP-0 case depends on. The three-argument `register_weave` is the
        // Loom's own way to bind one, which is what `mount_in_office` spells in the host.
        const loom::WeaveId id =
            bus.register_weave(std::move(weave), std::move(grant), std::string(kWorkshopProvider));
        w->zen_set_self(id);
        workshop_id = id;
        (void)loom::mount<Painter>(bus, canvases, notes);
    }

    /// MOUNT THE PARTICIPANT THE WAY THE HOST DOES -- on THIS bus, the one that already
    /// carries Workshop's own weave, and hand the weave the non-owning pointer through the
    /// same HostContext `request_stop` travels through.
    ///
    /// Its baseline is the host's own: one rule, SurfaceText to whoever holds `zengine.skin`
    /// AT DELIVERY. Workshop's own grant, minted above from its Emit set, is `to_any` for the
    /// same shape -- so the two identities differ by their RULE rather than by their
    /// vocabulary, which is the sharpest form the difference can take.
    ///
    /// `widen` is the CANARY LEVER: it gives the participant Workshop's wider rule. The case
    /// that asserts a publication from the pane reaches nobody is only a measurement if this
    /// makes it fail.
    /// `shapes` mounts EXTRA declared shapes beyond the default three. Zero is the default
    /// and every pre-existing case keeps exactly the vocabulary it had; a case that needs the
    /// completion list to be longer than the room it has (HD-3, the scrolled hit test) asks
    /// for more, because a window that never slides proves nothing about the sliding.
    loom::TerminalSession* mount_terminal(bool widen = false, int shapes = 0) {
        loom::TerminalVocabulary vocab;
        vocab.knows(loom::schema_of<surface::SurfaceText>())
            .accepts(loom::schema_of<loom::Ack>())
            .accepts(loom::schema_of<loom::Refused>());
        for (int i = 0; i < shapes; ++i) {
            vocab.knows(loom::SchemaBuilder("Extra" + std::to_string(i), 1)
                            .field("seq", loom::Kind::Int)
                            .build());
        }
        loom::Grant grant;
        grant.allow_to_role(surface::SurfaceText::zen_name, surface::SurfaceText::zen_version,
                            surface::kSkinRole);
        if (widen) {
            grant.allow_to_any(surface::SurfaceText::zen_name,
                               surface::SurfaceText::zen_version);
        }
        const loom::MountedTerminal mounted = loom::host_mount_terminal(
            bus, std::make_unique<loom::TerminalSession>("workshop", std::move(vocab)),
            std::move(grant));
        host.terminal = mounted.session;
        terminal_id = mounted.id;
        return mounted.session;
    }

    /// The office the participant is allowed to reach, held by a weave that remembers who
    /// spoke to it.
    SkinSeat* mount_skin_seat() {
        auto seat = std::make_unique<SkinSeat>();
        SkinSeat* raw = seat.get();
        loom::Grant grant;
        grant.allow_to_any(loom::Ack::zen_name, loom::Ack::zen_version);
        grant.allow_to_any(surface::ClipboardText::zen_name,
                           surface::ClipboardText::zen_version);
        const loom::WeaveId id =
            bus.register_weave(std::move(seat), std::move(grant), surface::kSkinRole);
        raw->zen_set_self(id);
        return raw;
    }

    /// Ctrl+T, AS THE BACKENDS ACTUALLY REPORT IT (KEY-0): the key transition and no text,
    /// because a ctrl chord produces no character on any supported backend. The old
    /// shift+space default is gone -- it could not arrive from the POSIX backend at all --
    /// and a fixture that still sent it would be driving a binding that no longer exists.
    void toggle_terminal() {
        key(input::scan::kT, input::mod::kCtrl);
    }

    /// Type a whole line into whatever is taking text, then press Return.
    void type_line(const std::string& line) {
        for (const char c : line) {
            text(std::string(1, c));
        }
        key(input::scan::kReturn);
    }

    const TerminalPane& pane() const { return w->session().terminal; }

    void publish(const loom::Value& v) {
        (void)bus.publish(loom::Message(v, loom::WeaveId{}, loom::WeaveId{}, 0));
        bus.drain_until_idle();
    }

    void key(std::int64_t sc, std::int64_t mods = input::mod::kNone) {
        publish(loom::to_value(input::KeyPressed{sc, "", mods}));
    }
    void text(const std::string& s) { publish(loom::to_value(input::TextEntered{s})); }

    /// A pointer event AT A WORKSPACE CELL. The translation from workspace cell
    /// to the terminal position a backend reports is the inverse of the weave's
    /// own, done here so every case below reads in the coordinates a maker
    /// thinks in.
    static std::int64_t term_x(std::int64_t wx) { return wx + kWorkspaceX; }
    static std::int64_t term_y(std::int64_t wy) {
        return wy + kWorkspaceY + surface::kTuiCanvasTopRow;
    }

    /// The same workspace cell, as the WINDOW would report it: the pixel at the
    /// cell's top-left corner. Deliberately the inverse of the graphical Skin's
    /// own layout and not of the terminal's -- the two media report different
    /// numbers for one place (docs/reference/pointer-spaces.md).
    static std::int64_t px_x(std::int64_t wx) {
        return (wx + kWorkspaceX) * surface::kCanvasCellPx;
    }
    static std::int64_t px_y(std::int64_t wy) {
        return (wy + kWorkspaceY) * surface::kCanvasCellPx;
    }

    /// A PRESS AT AN EXACT POSITION IN THE MEDIUM'S OWN NUMBERS -- a window pixel or a
    /// terminal cell, untranslated. Every other helper here speaks WORKSPACE cells because
    /// that is what a maker thinks in for the document; the Terminal's interior is finer
    /// than a cell (HD-1), so its cases have to be able to say a pixel.
    void press_at(std::int64_t x, std::int64_t y, std::int64_t space) {
        publish(loom::to_value(input::PointerButton{1, true, x, y, space, input::mod::kNone}));
    }

    void press(std::int64_t wx, std::int64_t wy, std::int64_t mods = input::mod::kNone) {
        publish(loom::to_value(input::PointerButton{1, true, term_x(wx), term_y(wy),
                                                    input::space::kCells, mods}));
    }
    void release(std::int64_t wx, std::int64_t wy) {
        publish(loom::to_value(input::PointerButton{1, false, term_x(wx), term_y(wy),
                                                    input::space::kCells, input::mod::kNone}));
    }
    /// A SECOND-BUTTON press at a workspace cell (CTX-0) -- the same translation `press`
    /// uses, for the button that asks a question instead of taking hold.
    void right_press(std::int64_t wx, std::int64_t wy) {
        publish(loom::to_value(input::PointerButton{3, true, term_x(wx), term_y(wy),
                                                    input::space::kCells, input::mod::kNone}));
    }
    /// The same two gestures at a CANVAS cell -- for presses on Workshop's own furniture
    /// (a pane's slot, the contextual surface), which is placed on the canvas and never
    /// in the document's room.
    void press_canvas(std::int64_t cx, std::int64_t cy) {
        publish(loom::to_value(input::PointerButton{1, true, cx,
                                                    cy + surface::kTuiCanvasTopRow,
                                                    input::space::kCells, input::mod::kNone}));
    }
    void release_canvas(std::int64_t cx, std::int64_t cy) {
        publish(loom::to_value(input::PointerButton{1, false, cx,
                                                    cy + surface::kTuiCanvasTopRow,
                                                    input::space::kCells, input::mod::kNone}));
    }
    void motion_canvas(std::int64_t cx, std::int64_t cy) {
        publish(loom::to_value(input::PointerMoved{cx, cy + surface::kTuiCanvasTopRow, 0, 0,
                                                   input::space::kCells, input::mod::kNone}));
    }
    void right_press_canvas(std::int64_t cx, std::int64_t cy) {
        publish(loom::to_value(input::PointerButton{3, true, cx,
                                                    cy + surface::kTuiCanvasTopRow,
                                                    input::space::kCells, input::mod::kNone}));
    }
    /// The contextual-action surface's state, as every CTX-0 case reads it.
    const ContextMenu& menu() const { return w->session().context; }
    void motion(std::int64_t wx, std::int64_t wy) {
        publish(loom::to_value(input::PointerMoved{term_x(wx), term_y(wy), 0, 0,
                                                   input::space::kCells, input::mod::kNone}));
    }
    /// THE WHEEL, AT A CANVAS CELL -- `press_canvas`'s translation for the one event the
    /// editor's viewport consumes. `dy` is notches, +1 away from the maker (the wire's
    /// own convention), fractional exactly as a high-resolution wheel reports.
    void wheel_canvas(double dy, std::int64_t cx, std::int64_t cy) {
        publish(loom::to_value(input::PointerWheel{0.0, dy, cx,
                                                   cy + surface::kTuiCanvasTopRow,
                                                   input::space::kCells, input::mod::kNone}));
    }

    /// The same three gestures, arriving from the graphical Skin's window. The
    /// `+ cell/2` puts the event in the MIDDLE of the cell rather than on its
    /// corner, which is where a maker's pointer actually is and is what makes a
    /// truncating (rather than flooring) projection visible.
    static std::int64_t mid(std::int64_t p) { return p + surface::kCanvasCellPx / 2; }
    void press_px(std::int64_t wx, std::int64_t wy) {
        publish(loom::to_value(input::PointerButton{1, true, mid(px_x(wx)), mid(px_y(wy)),
                                                    input::space::kPixels, input::mod::kNone}));
    }
    void release_px(std::int64_t wx, std::int64_t wy) {
        publish(loom::to_value(input::PointerButton{1, false, mid(px_x(wx)), mid(px_y(wy)),
                                                    input::space::kPixels, input::mod::kNone}));
    }
    void motion_px(std::int64_t wx, std::int64_t wy) {
        publish(loom::to_value(input::PointerMoved{mid(px_x(wx)), mid(px_y(wy)), 0, 0,
                                                   input::space::kPixels, input::mod::kNone}));
    }

    /// The window manager asked the surface to close.
    void close_requested() { publish(loom::to_value(surface::SurfaceCloseRequested{})); }

    const WorkshopDoc& doc() const { return w->document(); }
    const Session& session() const { return w->session(); }
    std::string notice() const { return w->session().notice; }

    /// THE FRESHEST TEXT ON ONE SLOT -- asked BY SLOT, because a repaint now
    /// publishes two (`status` and `score`) and `notes.back()` means "whichever this
    /// repaint said last", which is a fact about publication order rather than about the
    /// screen. The medium already keeps them apart by name; a case must too.
    std::string note_on(const char* slot) const {
        for (std::size_t i = notes.size(); i > 0; --i) {
            if (notes[i - 1].slot == slot) {
                return notes[i - 1].text;
            }
        }
        return std::string();
    }
    std::string status_note() const { return note_on(surface::kSlotStatus); }
    /// The compact attention line, as the medium was handed it. Empty is the honest answer
    /// and the retraction both: nothing currently deserves a glance.
    std::string attention_note() const { return note_on(surface::kSlotScore); }
    /// WHAT IS CURRENTLY TRUE OF THIS WORKSHOP, through the one projection the
    /// screen, the compact indicator and the view all spend.
    std::vector<Condition> conditions() const {
        return attention_conditions(w->session(),
                                    host.frontier ? host.frontier() : ProjectFrontier{});
    }
    const ui::Element* first() const { return &w->document().elements.front(); }
    const ui::Element* second() const { return &w->document().elements[1]; }

    /// The inspector row with this label, as a maker would read it.
    const Row* row(const std::string& label) const {
        for (const Row& r : w->session().rows) {
            if (r.label() == label) {
                return &r;
            }
        }
        return nullptr;
    }

    /// Put the cursor on a named row and open it for editing, by keys only.
    void begin_editing(const std::string& label) {
        for (int guard = 0; guard < 32; ++guard) {
            const Session& s = session();
            REQUIRE(s.cursor < s.rows.size());
            if (s.rows[s.cursor].label() == label) {
                key(input::scan::kReturn);
                return;
            }
            key(input::scan::kDown);
        }
        FAIL("no inspector row labelled ", label);
    }
};

/// The index of the inspector row a session is editing, or `rows.size()` when none is.
inline std::size_t editing_index(const Live& t) {
    const Session& s = t.session();
    for (std::size_t i = 0; i < s.rows.size(); ++i) {
        if (s.rows[i].editing()) {
            return i;
        }
    }
    return s.rows.size();
}

/// THE INSPECTOR'S PROPERTY BODY, resolved the way the painter resolves it — through
/// `bounds_of` and `info_body_place`, never through a second arithmetic. A case that
/// computed the rectangle for itself would pass while the picture and the hit test disagreed.
inline InfoBodyPlace body_place(const Live& t) {
    const Screen sc = screen_of(t.session());
    return info_body_place(bounds_of(t.session().panels, t.session().setup.active, panel::kInfo, sc).rect, sc, t.doc(),
                           t.session());
}

/// What the last canvas actually published for that body, or nullptr if it published none.
inline const surface::SurfaceTextRegion* body_region(const surface::SurfaceCanvas& c,
                                                     const InfoBodyPlace& p) {
    // OVER THE PLANES THEMSELVES, never over `all_texts` -- that returns a VALUE and a
    // pointer into it would dangle at the end of this expression (WIND-2a).
    for (const surface::SurfaceLayer& layer : c.layers) {
        for (const surface::SurfaceTextRegion& r : layer.texts) {
            if (r.x == p.region_x && r.y == p.region_y) {
                return &r;
            }
        }
    }
    return nullptr;
}

/// The prose row of the body the editing property is drawn on. `kNoProseRow` when none is.
inline std::int64_t editing_prose_row(const Live& t, const InfoBodyPlace& p) {
    return prose_row_of_property(p, editing_index(t));
}

/// The window pixel a graphical medium, and the terminal cell a character medium, would
/// report for a VALUE column of a body row. The inverse of what `prose_at` does with them,
/// and it goes through the same `RegionFit` — a helper that assumed cells would pass on the
/// TUI lane and lie on the SDL one.
inline std::int64_t body_pixel_x(const InfoBodyPlace& p, std::int64_t column) {
    if (!p.fit.graphical()) {
        return (p.region_x + column) * surface::kCanvasCellPx + surface::kCanvasCellPx / 2;
    }
    return p.region_x * surface::kCanvasCellPx + p.fit.origin_x + column * p.fit.advance_px +
           p.fit.advance_px / 2;
}
inline std::int64_t body_pixel_y(const InfoBodyPlace& p, std::int64_t prose_row) {
    // A BODY row sits under the `OBJECTS` heading inside the panel's one region (WUX-1),
    // so the region row a pixel resolves to is the body row plus the heading's reservation.
    const std::int64_t region_row = kInfoHeadingRows + prose_row;
    if (!p.fit.graphical()) {
        return (p.region_y + region_row) * surface::kCanvasCellPx + surface::kCanvasCellPx / 2;
    }
    return p.region_y * surface::kCanvasCellPx + p.fit.origin_y + region_row * p.fit.line_px +
           p.fit.line_px / 2;
}
inline std::int64_t value_pixel_x(const InfoBodyPlace& p, std::int64_t value_column) {
    return body_pixel_x(p, kPropertyMarkCols + kPropertyLabelCols + value_column);
}
inline std::int64_t value_pixel_y(const InfoBodyPlace& p, std::int64_t prose_row) {
    return body_pixel_y(p, prose_row);
}

/// A long value that cannot fit an Inspector row at any extent this composition has.
inline const std::string kLongValue = "the quick brown fox jumps over the lazy dog";

/// The selected object's RESOLVED width, read the way the canvas reads it.
inline std::int64_t resolved_w(const Live& t) {
    const ui::Scene scene = workspace_scene(t.doc(), t.session());
    const ui::Placed* p = ui::placed_for(scene, t.session().selected);
    REQUIRE(p != nullptr);
    return p->rect.w;
}

/// THE TEMPORARY ROOT THIS SUITE OWNS, and nothing else does.
///
/// The Workshop cases used to be one binary, where a counter that starts at zero and a tag
/// nobody repeated were enough to keep two directories apart. Six binaries run them now,
/// and CTest runs the six AT ONCE: every one of them starts its counter at zero, so two
/// suites that ever came to share a tag would name one directory -- and `TempDir` REMOVES
/// what it finds there before it creates it, so the collision would not be a failure, it
/// would be one case deleting another's files underneath it.
///
/// So the owner is named rather than hoped for. `ZENGINE_WORKSHOP_SUITE` is the entry name
/// CMake gave this binary, two suites cannot resolve to one path whatever they call their
/// tags, and the property is a case rather than a convention (see
/// `test_workshop_persistence.cpp`). What it does NOT cover, deliberately: two runs of the
/// SAME suite at once, which is what a build tree already assumes it is not doing.
inline std::filesystem::path workshop_temp_root() {
    return std::filesystem::temp_directory_path() /
           ("zengine-workshop-" ZENGINE_WORKSHOP_SUITE);
}

/// A directory of this run's own, removed when the case ends. Tests never write
/// into the source tree and never share a path with each other.
class TempDir {
public:
    explicit TempDir(const char* tag) {
        static int counter = 0;
        path_ = workshop_temp_root() / (std::string(tag) + "-" + std::to_string(++counter));
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
        std::filesystem::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
        // ...and the suite's root once the last case in it has gone. `remove` takes an
        // EMPTY directory only, so this is the whole cleanup: it succeeds for whoever
        // happens to be last and does nothing, quietly, for everyone before them.
        std::filesystem::remove(workshop_temp_root(), ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    std::string file(const char* name) const { return (path_ / name).string(); }
    std::string document() const { return file("document.json"); }
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

/// Read a whole file as bytes, for a case that wants to look at what was
/// actually written rather than at what the writer said it wrote.
inline std::string slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

/// Put bytes at a path -- how a case forges a file that Workshop then meets as
/// an ordinary maker would.
inline void spillout(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
}

/// A document with everything persistence has to preserve in it: two objects sharing a
/// name, one authored in cells and one as a share, and a mint that has already
/// been past a deleted identity.
inline WorkshopDoc rich_document() {
    WorkshopDoc d;
    doc::add(d, "panel", 3, 2, ui::Extent{ui::kExtentPercent, 60},
             ui::Extent{ui::kExtentCells, 6});
    doc::add(d, "panel", 6, 10, ui::Extent{ui::kExtentCells, 14},
             ui::Extent{ui::kExtentCells, 4});
    const std::int64_t doomed = doc::add(d, "temporary", 0, 0, ui::Extent{ui::kExtentCells, 2},
                                         ui::Extent{ui::kExtentCells, 2});
    REQUIRE(doc::remove(d, doomed).accepted);
    return d;
}

/// The text of a saved document, with one substring replaced — how the refusal
/// cases forge a file that the honest writer could never produce.
inline std::string forged(const WorkshopDoc& d, const std::string& from, const std::string& to) {
    std::string text = persist::to_text(d);
    const std::size_t at = text.find(from);
    REQUIRE(at != std::string::npos);
    text.replace(at, from.size(), to);
    return text;
}

/// A stand-in for the Builder tool: it holds the office, records what it was
/// asked, and answers with whatever status the case has set up.
///
/// It is not a mock of the real tool's LOGIC (that has its own suite); it is the
/// other end of the conversation, so that "Workshop asked" and "Workshop showed
/// what it was told" are two facts a case can separate.
class ToolSeat
    : public loom::WeaveBase<ToolSeat, SeenState,
                             loom::Accept<zengine::builder::StatusRequested,
                                          zengine::builder::BuildRequested>,
                             loom::Emit<zengine::builder::BuildStatus,
                                        zengine::builder::RecipeCatalog>> {
public:
    /// TWO SHAPES ON ONE ASK (BLD-1), exactly as the real tool answers: what can be
    /// built here, and where the one it last built stands. The catalog first, because
    /// a panel that heard a status about a recipe it had never been told existed would
    /// be a panel showing a choice nobody offered it.
    void on(const zengine::builder::StatusRequested&, loom::Mail& mail) {
        ++described;
        (void)mail.publish(catalog);
        (void)mail.publish(next);
    }
    void on(const zengine::builder::BuildRequested& ask, loom::Mail& mail) {
        asked.push_back(ask.recipe);
        realize_asked.push_back(ask.realize);
        if (answers_builds) {
            (void)mail.publish(next);
        }
    }

    /// What this tool will say next time it is asked anything.
    zengine::builder::BuildStatus next{};
    /// ...and what it says this project can build at all.
    zengine::builder::RecipeCatalog catalog{};
    /// WHETHER EACH ASK WAS A BUILD OR A BUILD-AND-REALIZE. Recorded rather than
    /// asserted from the panel, because "the maker's second intention crossed the
    /// office boundary" is a fact about what was SAID and not about what was shown.
    std::vector<bool> realize_asked;
    /// ...and whether it answers a build at all. A real build takes seconds and
    /// answers when the process exits; a stand-in that always answers instantly
    /// would make the panel's `waiting` state unreachable from any case.
    bool answers_builds = true;
    std::int64_t described = 0;
    std::vector<std::string> asked;
};

/// Mount the stand-in into the Builder office on this Workshop's bus.
inline ToolSeat* mount_tool(Live& t, const std::string& recipe) {
    auto seat = std::make_unique<ToolSeat>();
    ToolSeat* raw = seat.get();
    loom::Grant grant;
    grant.allow_to_any(zengine::builder::BuildStatus::zen_name,
                       zengine::builder::BuildStatus::zen_version);
    grant.allow_to_any(zengine::builder::RecipeCatalog::zen_name,
                       zengine::builder::RecipeCatalog::zen_version);
    const loom::WeaveId id = t.bus.register_weave(std::move(seat), std::move(grant),
                                                  std::string(zengine::builder::kBuilderRole));
    raw->zen_set_self(id);
    raw->next.recipe = recipe;
    raw->next.artifact = recipe;
    raw->catalog.recipes.push_back(zengine::builder::RecipeSummary{recipe, recipe});
    return raw;
}

/// EVERYTHING AT A PANEL'S BOUNDS, top to bottom.
///
/// One helper for both kinds, and that is PNL-1 arriving in the suite: "what is
/// this panel showing" used to be two questions with two different answers --
/// one walked the stack's hard-coded column and rows, the other walked every
/// label at `Screen::panel_x` -- and it is now one question about a rectangle.
/// THROUGH `cell_text_of`, NOT THROUGH `c.labels` (HD-7). The Info panel's object names used
/// to be ordinary canvas labels and are rows of a bounded region now, so a helper reading only
/// the label list would have gone on passing while asserting about half a panel.
/// ONE ROW PER ROW, AND IT IS THE ONE ON TOP (WIND-2a). A canvas carries a plane per
/// presentation now, and two presentations genuinely share this rectangle -- an external
/// pane is seated in the stack's first slot and the picker opens over it. Concatenating
/// every text at those cells would read both at once and call the result the panel,
/// which is a sentence about a picture nobody paints. `cell_text_of` walks the Skin's
/// own order, so the LAST text at a row is what a maker reads there.
inline std::string panel_text(const surface::SurfaceCanvas& c, const ui::Rect& b) {
    const std::size_t rows_n = static_cast<std::size_t>(b.h > 0 ? b.h : 0);
    std::vector<std::string> rows(rows_n);
    std::vector<bool> said(rows_n, false);
    for (const surface::SurfaceLabel& l : cell_text_of(c)) {
        if (l.x == b.x && l.y >= b.y && l.y < b.y + b.h) {
            const std::size_t at = static_cast<std::size_t>(l.y - b.y);
            rows[at] = l.text;
            said[at] = true;
        }
    }
    std::string out;
    for (std::size_t i = 0; i < rows_n; ++i) {
        if (said[i]) {
            out += rows[i];
            out += '\n';
        }
    }
    return out;
}

/// THE CELLS INSIDE A SURFACE'S OWN CHROME (WUX-5) -- where every row a pane, panel or
/// overlay draws actually lands. It is `pane_interior` read at the cell grain, so a case
/// asking "what does this pane SAY" and the painter that said it are one rectangle.
///
/// A case asking about the pane's PLACE -- occupancy, a press on its edge, coverage --
/// still wants the OUTER rectangle `bounds_of` answers, which is unchanged: the boundary
/// is inside the pane and the pane did not move.
inline ui::Rect pane_body_cells(const FineRect& outer) {
    return cells_covered(pane_interior(outer));
}
inline ui::Rect pane_body_cells(const ui::Rect& outer) {
    return pane_body_cells(fine_of_cells(outer));
}

/// What the overlay stack's first slot is showing, whatever is in it. The stack is
/// anchored to the canvas's top-left and its ROWS are the same on every screen -- only
/// its width follows the room (WIND-1) -- and `panel_text` reads one column and a run of
/// rows, so the minimum screen's rectangle still names the right rows on any of them.
/// Since WUX-5 it reads the slot's INTERIOR, because that is where the rows are.
inline std::string stack_text(const surface::SurfaceCanvas& c) {
    return panel_text(c, pane_body_cells(placement_bounds(placement::kOverlayStack, 0,
                                                          kMinScreen)));
}

/// Where a kind sits in the catalog, so a case names a KIND rather than a row
/// number that a later catalog entry would silently invalidate.
inline std::size_t catalog_at(std::int64_t kind) {
    for (std::size_t i = 0; i < kPanelKinds; ++i) {
        if (kPanelCatalog[i].kind == kind) {
            return i;
        }
    }
    return kPanelKinds; // walked off the end: the case that used it will fail loudly
}

/// SELECT A KIND IN THE PICKER, the way a maker does: `p`, down to it, Return.
///
/// One helper for both directions, because since PNL-0 there is one gesture for
/// both directions: what this does to a closed kind is open it, and what it does
/// to an open kind is remove it.
inline void pick(Live& t, std::int64_t kind) {
    const std::size_t at = catalog_at(kind);
    REQUIRE(at < kPanelKinds);
    t.key(input::scan::kP);
    for (std::size_t i = 0; i < at; ++i) {
        t.key(input::scan::kDown);
    }
    t.key(input::scan::kReturn);
}

/// Open the Builder panel the way a maker does.
inline void open_builder(Live& t) { pick(t, panel::kBuilder); }

/// THE BUILDER IS IN THE STACK'S FIRST SLOT, asked through the placement path
/// and read off the canvas at the answer -- so the case cannot agree with the
/// screen by both of them holding the same constant.
inline bool first_slot_shows_builder(Live& t) {
    const Screen sc = screen_of(t.session());
    const PanelBounds at = bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder, sc);
    const ui::Rect cells = pane_body_cells(at.rect);
    return at.open && at.rect == fine_of_cells(placement_bounds(placement::kOverlayStack, 0, sc)) &&
           label_at(t.canvases.back(), cells.x, cells.y).find("BUILDER") == 0;
}

/// Everything the Info panel is showing, top to bottom -- through the same path,
/// asked about the other place.
inline std::string info_text(const surface::SurfaceCanvas& c, const Screen& sc) {
    return panel_text(c, pane_body_cells(placement_bounds(placement::kSideRegion, 0, sc)));
}

/// WHAT THE TERMINAL OVERLAY IS SAYING, at its own rectangle. It is a MODE and not a pane,
/// so it wears no pane chrome and its rows begin at its own corner (WUX-5 changed nothing
/// about it) -- and at the minimum composition that corner is column 0, which is also the
/// overlay stack's, so a case reading it through `stack_text` would be reading the stack's
/// INTERIOR and missing the terminal's first column.
inline std::string terminal_text(const surface::SurfaceCanvas& c, const Screen& sc) {
    return panel_text(c, ui::Rect{sc.terminal_x, sc.terminal_y, sc.terminal_w, sc.terminal_h});
}

/// The cell a terminal medium would report for a prose position of the pane's own region.
/// The inverse of `terminal_input_place`'s resolution, exactly as `pane_pixel_x` is.
inline std::int64_t pane_cell_x(const TerminalInputPlace& p, std::int64_t column) {
    return p.region_x + column;
}
inline std::int64_t pane_cell_y(const TerminalInputPlace& p, std::int64_t row) {
    return p.region_y + row + surface::kTuiCanvasTopRow;
}

/// A document of `n` identical objects with the selection on the `at`th, and a session
/// resolved for the given extent. The names are all `panel`, which is what `n` actually
/// produces -- see the duplicate-name case below.
struct Sample {
    WorkshopDoc d;
    Session s;
};
inline Sample panel_of(std::size_t n, std::size_t at, std::int64_t w, std::int64_t h,
                      std::int64_t advance = 0, std::int64_t line = 0) {
    Sample p;
    for (std::size_t i = 0; i < n; ++i) {
        REQUIRE(doc::add_default(p.d) != 0);
    }
    adopt_screen(p.s, w, h, advance, line);
    if (!p.d.elements.empty()) {
        p.s.selected = p.d.elements[at].id;
    }
    refocus(p.d, p.s);
    return p;
}

/// The reference a built-in kind is spelled with, as a case says it -- through
/// the catalog, never as a literal, so a case cannot agree with a typo.
inline PaneRef ref_of(std::int64_t kind) { return pane_ref_of(kind); }

/// A reference to a pane no build of this Workshop has ever had. The
/// third-party entry every unresolved case is built on.
inline PaneRef stranger() { return PaneRef{"third.party.tools", "history"}; }

/// NOBODY HAS OFFERED ANYTHING -- the state every case in this tier was written
/// in, said out loud since WP-0 made the runtime catalog a required argument to
/// the resolution door rather than something a caller could forget. Passing an
/// empty one is what keeps these cases' claims exactly what they were: this is
/// what the BUILT-IN half answers, with no provider in the process.
inline const RuntimeCatalog& no_providers() {
    static const RuntimeCatalog empty;
    return empty;
}

/// The room the minimum composition actually has: one overlay slot, resolved
/// through `placement_bounds` rather than written down here (screen.hpp says
/// why it is one). Every case below reconciles at most one stacked panel, so
/// this is the capacity they were all written under.
inline StackCapacity min_room() { return stack_capacity(kMinScreen); }

/// A setup, spelled the way a case reads: a name and the kinds it means.
inline Setup setup_of(const std::string& name, const std::vector<std::int64_t>& kinds) {
    Setup s;
    s.name = name;
    for (const std::int64_t k : kinds) {
        // THROUGH THE DOOR, so every candidate a case builds carries the identity
        // permutation `add_pane` assigns (WIND-2). A `push_back` here would build
        // setups whose ranks are all zero, which is valid for exactly one row and
        // which `check_setup` refuses for two -- and every case below would then be
        // measuring the fixture rather than the law.
        REQUIRE(add_pane(s, ref_of(k)));
    }
    return s;
}

/// The kinds a session currently has open, in open order -- what the authored
/// order is supposed to have produced.
inline std::vector<std::int64_t> open_kinds(const Panels& panels) {
    std::vector<std::int64_t> out;
    for (const Panel& p : panels.open) {
        out.push_back(p.kind);
    }
    return out;
}

/// A setup file's text with one substring replaced -- how the refusal cases
/// forge a file the honest writer could never produce. The document tier's own
/// `forged`, asked about the other artifact.
inline std::string forged_setup(const Setup& s, const std::string& from, const std::string& to) {
    std::string text = setup_persist::to_text(s);
    const std::size_t at = text.find(from);
    INFO("looking for `", from, "` in: ", text);
    REQUIRE(at != std::string::npos);
    text.replace(at, from.size(), to);
    return text;
}

/// Name the setup and save it, the way a maker does: `s`, the character that
/// key produced, clear what is there, type a name, Return.
///
/// IT SENDS THE TRIGGER'S OWN TEXT EVERY TIME, deliberately. The backends report
/// `s` as `KeyPressed{S}` AND `TextEntered{"s"}`, and a fixture that sent only
/// the first would make the swallow untestable from every case that uses it.
inline void name_setup(Live& t, const std::string& name) {
    t.key(input::scan::kS);
    t.text("s");
    REQUIRE(t.session().setup.naming.open);
    for (int guard = 0; guard < 64 && !t.session().setup.naming.line.empty(); ++guard) {
        t.key(input::scan::kBackspace);
    }
    REQUIRE(t.session().setup.naming.line.empty());
    for (const char c : name) {
        t.text(std::string(1, c));
    }
    t.key(input::scan::kReturn);
}

/// The setup line as a maker reads it, off the canvas at the place the painter
/// put it -- never rebuilt here, so a case cannot pass while the screen says
/// something else.
inline std::string setup_row(const surface::SurfaceCanvas& c, const Screen& sc) {
    return label_at(c, 0, sc.notice_y - 1);
}

/// Make this Workshop paint once, the way a Skin claiming the surface makes it:
/// a weave runs only on message, so a session nobody has spoken to has published
/// no canvas at all.
inline const surface::SurfaceCanvas& first_frame(Live& t) {
    t.publish(loom::to_value(surface::SurfaceReady{}));
    REQUIRE_FALSE(t.canvases.empty());
    return t.canvases.back();
}

/// The Hello fixture's identity, spelled as a STRANGER would have to spell it --
/// by string. Nothing here includes the fixture's source, because a provider is a
/// stranger to Workshop and the whole claim is that two strings are enough.
inline constexpr const char* kHelloOffice = "zengine.test.workshop-hello";
inline constexpr const char* kHelloPane = "hello";

inline PaneRef hello_ref() { return PaneRef{kHelloOffice, kHelloPane}; }

/// A SECOND OFFICE, for the cases about two providers and one pane key.
inline constexpr const char* kOtherOffice = "zengine.test.other-provider";

/// The nudge that makes a native seat speak at an instant a case chooses. It is
/// the seat's own private shape and no part of the pane protocol -- a case must
/// be able to make a real weave author a real sentence without that mechanism
/// being reachable by anything else.
struct SeatDo {
    ZEN_SHAPE(SeatDo, 1);
};

struct SeatState {
    std::int64_t said = 0;
    ZEN_SHAPE(SeatState, 1, ZEN_FIELD(said));
};

/// A NATIVE PROVIDER SEAT: a weave that holds an office and can be made to say
/// anything at all, deliberately or personally.
///
/// IT IS EVIDENCE OF A DIFFERENT KIND FROM THE DYNAMIC FIXTURE, and the two are
/// kept apart on purpose. The `.so` proves the real ABI, the real load path and a
/// real attested activation; this proves the AUTHORITY cases, because a case must
/// be able to send the exact wrong sentence at the exact wrong moment -- personal
/// speech from the actual role holder, one office speaking about another's pane, a
/// content message one column too wide -- and a shipped fixture that could be
/// talked into those would not be a fixture worth shipping.
class ProviderSeat
    : public loom::WeaveBase<ProviderSeat, SeatState,
                             loom::Accept<PaneCatalogRequested, PaneRoom, PanePressed, PaneKey,
                                          PaneTextInput, SeatDo>,
                             loom::Emit<PaneOffered, PaneContent, PanePressed>> {
public:
    explicit ProviderSeat(std::string office) : office_(std::move(office)) {}

    void on(const PaneCatalogRequested&, loom::Mail& mail) {
        ++state_.said;
        ++said;
        asks.push_back(std::string(mail.authored_role()));
    }
    void on(const PaneRoom& r, loom::Mail& mail) {
        ++state_.said;
        ++said;
        rooms.push_back(r);
        room_authors.push_back(std::string(mail.authored_role()));
    }
    /// A PRESS THIS SEAT WAS TOLD ABOUT (SEL-0), and its author beside it. The seat
    /// interprets nothing -- it is a recorder, so a case can assert exactly what
    /// Workshop said and nothing about what a real provider would make of it.
    void on(const PanePressed& p, loom::Mail& mail) {
        ++state_.said;
        ++said;
        presses.push_back(p);
        press_authors.push_back(std::string(mail.authored_role()));
    }
    /// A KEY AND THE TEXT IT PRODUCED (MSG-0). Recorded and never interpreted, for
    /// the press's reason exactly: a case asserts what WORKSHOP said, and a seat that
    /// made something of one would be asserting a provider's opinion instead.
    void on(const PaneKey& k, loom::Mail& mail) {
        ++state_.said;
        ++said;
        keys.push_back(k);
        key_authors.push_back(std::string(mail.authored_role()));
    }
    void on(const PaneTextInput& t, loom::Mail& mail) {
        ++state_.said;
        ++said;
        typed.push_back(t);
        text_authors.push_back(std::string(mail.authored_role()));
    }
    void on(const SeatDo&, loom::Mail& mail) {
        if (next) {
            std::function<void(ProviderSeat&, loom::Mail&)> once;
            once.swap(next);
            once(*this, mail);
        }
    }

    /// Offer, deliberately AS this office -- the ordinary, correct spelling.
    void offer(loom::Mail& mail, const PaneOffered& o) {
        (void)mail.as_role(office_).send_to_role(kWorkshopProvider, o);
    }
    /// Offer PERSONALLY, from the very weave that holds the office. Holding is not
    /// speaking-for (MSG-07), and this is the sentence that says so.
    void offer_personally(loom::Mail& mail, const PaneOffered& o) {
        (void)mail.send_to_role(kWorkshopProvider, o);
    }
    void say(loom::Mail& mail, const PaneContent& c) {
        (void)mail.as_role(office_).send_to_role(kWorkshopProvider, c);
    }
    void say_personally(loom::Mail& mail, const PaneContent& c) {
        (void)mail.send_to_role(kWorkshopProvider, c);
    }
    /// FORGE A PRESS AT SOMEBODY ELSE'S PANE (SEL-0) -- deliberately authored, and
    /// deliberately by an office that is not `zengine.workshop`. This is the sentence
    /// a provider must refuse: a stranger telling it a maker clicked one of its rows.
    void press_at(loom::Mail& mail, const char* office, const PanePressed& p) {
        (void)mail.as_role(office_).send_to_role(office, p);
    }

    /// EVERYTHING THIS SEAT WAS EVER TOLD, counted. It is what lets a case say
    /// "closing a presentation reached the provider not at all" as a number rather
    /// than as an absence somebody has to trust.
    std::int64_t said = 0;
    std::vector<PaneRoom> rooms;
    std::vector<std::string> room_authors;
    std::vector<std::string> asks;
    std::vector<PanePressed> presses;
    std::vector<std::string> press_authors;
    std::vector<PaneKey> keys;
    std::vector<std::string> key_authors;
    std::vector<PaneTextInput> typed;
    std::vector<std::string> text_authors;
    std::function<void(ProviderSeat&, loom::Mail&)> next;

private:
    std::string office_;
};


/// A WEAVE THAT HOLDS `zengine.workshop` AND IS NOT WORKSHOP -- the instrument for
/// measuring a PROVIDER's own authorship checks.
///
/// It exists because the fixture's refusals are invisible from Workshop's side: a
/// room the provider declined to believe and a room it answered into a pane nobody
/// has open both look like silence there. Holding the office lets this author a real
/// `PaneRoom` deliberately, and holding it lets it also send one PERSONALLY -- which
/// is exactly the sentence a weave that merely held the office would produce by
/// reaching for `send_to_role`. Two spellings, one holder, opposite outcomes.
class PaneWatcher
    : public loom::WeaveBase<PaneWatcher, SeatState,
                             loom::Accept<PaneOffered, PaneContent, SeatDo>,
                             loom::Emit<PaneCatalogRequested, PaneRoom, PanePressed>> {
public:
    void on(const PaneOffered& o, loom::Mail& mail) {
        offers.push_back(o);
        offer_authors.push_back(std::string(mail.authored_role()));
    }
    void on(const PaneContent& c, loom::Mail& mail) {
        content.push_back(c);
        content_authors.push_back(std::string(mail.authored_role()));
    }
    void on(const SeatDo&, loom::Mail& mail) {
        if (next) {
            std::function<void(PaneWatcher&, loom::Mail&)> once;
            once.swap(next);
            once(*this, mail);
        }
    }

    void grant(loom::Mail& mail, const char* office, const PaneRoom& room) {
        (void)mail.as_role(kWorkshopProvider).send_to_role(office, room);
    }
    void grant_personally(loom::Mail& mail, const char* office, const PaneRoom& room) {
        (void)mail.send_to_role(office, room);
    }
    void ask(loom::Mail& mail) {
        (void)mail.as_role(kWorkshopProvider).publish(PaneCatalogRequested{});
    }
    void ask_personally(loom::Mail& mail) { (void)mail.publish(PaneCatalogRequested{}); }
    /// A PRESS FROM THE OFFICE THIS WATCHER HOLDS (SEL-0) -- the correctly authored
    /// spelling and the personal one, exactly as `grant`/`grant_personally` are, so a
    /// provider's own authorship check can be measured from the only side it shows on.
    void press(loom::Mail& mail, const char* office, const PanePressed& p) {
        (void)mail.as_role(kWorkshopProvider).send_to_role(office, p);
    }
    void press_personally(loom::Mail& mail, const char* office, const PanePressed& p) {
        (void)mail.send_to_role(office, p);
    }

    std::vector<PaneOffered> offers;
    std::vector<std::string> offer_authors;
    std::vector<PaneContent> content;
    std::vector<std::string> content_authors;
    std::function<void(PaneWatcher&, loom::Mail&)> next;
};

struct BootState {
    std::int64_t n = 0;
    ZEN_SHAPE(BootState, 1, ZEN_FIELD(n));
};

/// The weave that commands the Weave Manager and HEARS ITS ANSWERS -- the host's
/// own boot shape, because a load whose refusal is addressed to nobody looks
/// exactly like a load that worked.
///
/// IT HEARS `zen.Ack` TOO SINCE INTR-0, because the lifecycle case unloads a library
/// and the control door answers an unload with an Ack rather than a Result. A door's
/// answer arriving at a weave that does not accept the shape is not a failure worth
/// asserting on -- it is simply an answer nobody read, which is the exact silence the
/// original comment above exists to complain about.
class Booter : public loom::WeaveBase<Booter, BootState,
                                      loom::Accept<loom::Result, loom::Ack, loom::Refused>,
                                      loom::Emit<loom::LoadWeave, loom::UnloadLibrary>> {
public:
    Booter(std::vector<std::string>& ok, std::vector<std::string>& no) : ok_(&ok), no_(&no) {}
    void on(const loom::Result& r, loom::Mail&) { ok_->push_back(r.value); }
    void on(const loom::Ack&, loom::Mail&) { ++acks; }
    void on(const loom::Refused& r, loom::Mail&) { no_->push_back(r.reason); }

    std::int64_t acks = 0;

private:
    std::vector<std::string>* ok_;
    std::vector<std::string>* no_;
};

/// A live Workshop that can be handed providers -- native ones always, and the real
/// dynamic Hello when a case asks for it.
///
/// THE MOUNT ORDER IS THE CASE'S TO CHOOSE, which is why this is its own rig and not
/// a flag on `Live`: the whole discovery claim is that neither load order loses an
/// offer, and a rig that always mounted Workshop first could only ever prove one of
/// the two.
struct PaneRig {
    /// ---- INTR-1's additions, and their order is the host's ---------------------
    ///
    /// `catalog` IS DECLARED BEFORE THE KERNEL, which is the host's own lifetime claim
    /// -- destruction runs in reverse, so the Kernel and every artifact it holds go
    /// down before the store their contributions live in.
    ///
    /// IT EXISTS IN EVERY RIG AND DOES NOTHING UNTIL A CASE ASKS. An empty catalog is
    /// what a host has before it starts; the realization owner and the door are
    /// `run_plan`/`mount_arrangement`'s, so no case that predates INTR-1 gained a
    /// weave, a mount or a message.
    ///
    /// ⚠ THE AUTHORED PLAN MOVED INTO THE OWNER (BOOT-0). It used to be a rig field
    /// declared before the bus, because the door held it by reference; the owner is
    /// persistent now and holds the plan it is realizing, so there is one copy and the
    /// door reads it from there.
    /// ---- SOURCE-1: the owners this host's OWN Sources read ---------------------
    ///
    /// DECLARED BEFORE THE CATALOG, which is the host's own lifetime claim once more:
    /// each Source's native body closes over a REFERENCE to one of these and reads it
    /// at the moment of the sample, so reverse-order destruction must drop the catalog
    /// holding those closures first. `workshop.cpp` declares them far above its own
    /// `op::Catalog` for exactly this reason, and this rig copies the order rather
    /// than the outcome.
    ///
    /// THEY ARE ORDINARY MUTABLE OWNERS, and a case moving one between two samples is
    /// how "a sample is an evaluation and not a cached answer" is proved with the
    /// answers themselves rather than with a counter alone.
    std::string project_anchor = "/zen/pane-rig";
    CurrentRecipes host_recipes;

    loom::Switchboard bus;
    op::Catalog catalog;
    op::OperatorHostSurface operator_host{catalog};
    loom::Kernel kernel{bus};
    loom::WeaveId control = loom::mount_control(kernel, bus);
    loom::WeaveId manager = loom::mount_manager(control, bus);
    HostContext host;
    std::vector<surface::SurfaceCanvas> canvases;
    std::vector<surface::SurfaceText> notes;
    WorkshopWeave* w = nullptr;
    loom::WeaveId workshop_id{};
    std::vector<std::string> loaded;
    std::vector<std::string> load_refusals;

    PaneRig() { (void)loom::mount<Painter>(bus, canvases, notes); }

    /// MOUNT WORKSHOP THE WAY THE HOST DOES: in the `zengine.workshop` office, with
    /// the exact production grant.
    ///
    /// THE RULES ARE SPELLED OUT RATHER THAN MINTED FROM THE EMIT SET, and that is
    /// half of why this rig exists beside `Live`. `emit_default_grant` gives a weave
    /// `to_any` for everything it declares, which is wider than what workshop.cpp
    /// writes -- and the two Builder sentences being ROLE-SCOPED is a claim WP-0 must
    /// not quietly relax. These six lines are the host's, copied deliberately so a
    /// case can assert what Workshop may and may not say.
    WorkshopWeave* mount_workshop() {
        auto weave = std::make_unique<WorkshopWeave>(host);
        w = weave.get();
        loom::Grant speak;
        speak.allow_to_any(surface::SurfaceCanvas::zen_name, surface::SurfaceCanvas::zen_version);
        speak.allow_to_any(surface::SurfaceText::zen_name, surface::SurfaceText::zen_version);
        // TEXT-0's copy sentence and QR-11's role-scoped clipboard question, exactly as
        // workshop.cpp grants them (the QR-11 case that races a pane's paste needs the
        // production truth here, not a narrower rig-only one).
        speak.allow_to_any(surface::ClipboardCopy::zen_name,
                           surface::ClipboardCopy::zen_version);
        speak.allow_to_role(surface::ClipboardTextRequested::zen_name,
                            surface::ClipboardTextRequested::zen_version, surface::kSkinRole);
        // WUX-3's placement offer, role-scoped to the skin exactly as workshop.cpp
        // grants it.
        speak.allow_to_role(surface::SurfacePlacementRemembered::zen_name,
                            surface::SurfacePlacementRemembered::zen_version,
                            surface::kSkinRole);
        speak.allow_to_role(zengine::builder::StatusRequested::zen_name,
                            zengine::builder::StatusRequested::zen_version,
                            zengine::builder::kBuilderRole);
        speak.allow_to_role(zengine::builder::BuildRequested::zen_name,
                            zengine::builder::BuildRequested::zen_version,
                            zengine::builder::kBuilderRole);
        speak.allow_to_any(PaneCatalogRequested::zen_name, PaneCatalogRequested::zen_version);
        speak.allow_to_any(PaneRoom::zen_name, PaneRoom::zen_version);
        speak.allow_to_any(PanePressed::zen_name, PanePressed::zen_version);
        speak.allow_to_any(PaneKey::zen_name, PaneKey::zen_version);
        speak.allow_to_any(PaneTextInput::zen_name, PaneTextInput::zen_version);
        workshop_id =
            bus.register_weave(std::move(weave), std::move(speak), std::string(kWorkshopProvider));
        w->zen_set_self(workshop_id);
        return w;
    }

    /// A native provider in an office of its own, granted exactly the two sentences
    /// the pane protocol has and nothing else.
    ///
    /// THE OFFICE IS A VIEW (WP-0a) so a case can seat a weave in an office longer
    /// than Workshop's own key bound. The `const char*` spelling every existing case
    /// uses converts and is unchanged; what this buys is one case that could not be
    /// written at all before, and it is not a second rig.
    ProviderSeat* mount_provider(std::string_view office) {
        auto seat = std::make_unique<ProviderSeat>(std::string(office));
        ProviderSeat* raw = seat.get();
        loom::Grant grant;
        grant.allow_to_any(PaneOffered::zen_name, PaneOffered::zen_version);
        grant.allow_to_any(PaneContent::zen_name, PaneContent::zen_version);
        // A SEAT MAY FORGE A PRESS (SEL-0). Granted here deliberately, because the
        // claim under test is that a PROVIDER refuses a press it did not get from
        // Workshop -- a refusal the bus made unreachable would prove nothing.
        grant.allow_to_any(PanePressed::zen_name, PanePressed::zen_version);
        const loom::WeaveId id =
            bus.register_weave(std::move(seat), std::move(grant), std::string(office));
        raw->zen_set_self(id);
        seat_ids.push_back(id);
        seats_.push_back(raw);
        return raw;
    }

    /// Make a seat perform one sentence INSIDE ITS OWN DELIVERY, which is what gives
    /// `mail.as_role(...)` a real authorship moment for Loom to verify.
    void drive(ProviderSeat* seat, std::function<void(ProviderSeat&, loom::Mail&)> what) {
        seat->next = std::move(what);
        loom::WeaveId id{};
        for (std::size_t i = 0; i < seats_.size(); ++i) {
            if (seats_[i] == seat) {
                id = seat_ids[i];
            }
        }
        (void)bus.send(id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                         loom::WeaveId{}, 0));
        bus.drain_until_idle();
    }

    /// LOAD A REAL SHARED LIBRARY THROUGH THE REAL KERNEL AND MANAGER, exactly as a
    /// host does: an ordinary `LoadWeave` command sent as a weave that can hear the
    /// answer. There is no direct `Kernel::load` here and no test-only door -- the
    /// artifact goes through the same path `zengine-workshop` sends its Skin through,
    /// and the activation the loaded weave receives is Loom's own, attested.
    loom::WeaveId load(const char* name, const char* path, const char* role) {
        loom::Grant reach;
        reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
        const std::size_t before = loaded.size();
        const loom::WeaveId booter =
            loom::mount_granted<Booter>(bus, std::move(reach), loaded, load_refusals);
        bus.send_as(booter, manager,
                    loom::Message(loom::to_value(loom::LoadWeave{name, path, role}), booter,
                                  booter, 0));
        bus.drain_until_idle();
        if (loaded.size() <= before) {
            return loom::WeaveId{};
        }
        return loom::WeaveId{static_cast<std::uint64_t>(std::stoll(loaded.back()))};
    }

    /// UNLOAD A REAL LIBRARY THROUGH THE REAL CONTROL DOOR (INTR-0).
    ///
    /// The Weave Manager has no unload op -- its four are load, swap, reload and list --
    /// so this addresses `zen.UnloadLibrary` to the door itself, which is what the whole
    /// `load_capability` grant exists to permit. The grant is Loom's own function rather
    /// than a hand-written subset, because a case that quietly narrowed the dangerous
    /// grant would be testing its own idea of it.
    bool unload(const char* name) {
        const loom::WeaveId booter = loom::mount_granted<Booter>(
            bus, loom::load_capability(control), loaded, load_refusals);
        const std::size_t before = load_refusals.size();
        bus.send_as(booter, control,
                    loom::Message(loom::to_value(loom::UnloadLibrary{name}), booter, booter, 0));
        bus.drain_until_idle();
        return load_refusals.size() == before;
    }


    /// A watcher in the `zengine.workshop` office, INSTEAD of Workshop. Only one weave
    /// may hold a role, so a case uses this OR `mount_workshop`, never both.
    PaneWatcher* mount_watcher() {
        auto seat = std::make_unique<PaneWatcher>();
        PaneWatcher* raw = seat.get();
        loom::Grant grant;
        grant.allow_to_any(PaneCatalogRequested::zen_name, PaneCatalogRequested::zen_version);
        grant.allow_to_any(PaneRoom::zen_name, PaneRoom::zen_version);
        grant.allow_to_any(PanePressed::zen_name, PanePressed::zen_version);
        const loom::WeaveId id =
            bus.register_weave(std::move(seat), std::move(grant), std::string(kWorkshopProvider));
        raw->zen_set_self(id);
        watcher_id = id;
        return raw;
    }

    void drive_watcher(PaneWatcher* watch, std::function<void(PaneWatcher&, loom::Mail&)> what) {
        watch->next = std::move(what);
        (void)bus.send(watcher_id,
                       loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{}, loom::WeaveId{}, 0));
        bus.drain_until_idle();
    }

    loom::WeaveId watcher_id{};

    void publish(const loom::Value& v) {
        (void)bus.publish(loom::Message(v, loom::WeaveId{}, loom::WeaveId{}, 0));
        bus.drain_until_idle();
    }

    /// The Skin says hello -- Workshop's startup hook, and where it asks the room who
    /// has panes.
    void ready() { publish(loom::to_value(surface::SurfaceReady{})); }

    void extent(std::int64_t width, std::int64_t height, std::int64_t adv = 0,
                std::int64_t line = 0) {
        publish(loom::to_value(surface::SurfaceExtent{width, height, adv, line}));
    }

    void key(std::int64_t sc, std::int64_t mods = input::mod::kNone) {
        publish(loom::to_value(input::KeyPressed{sc, "", mods}));
    }

    /// THE CHARACTER A PRINTABLE TRIGGER ALSO PRODUCES -- `Live`'s own door, here because a
    /// case that enters pane management has to pay the `swallow_text_` rule the way a real
    /// backend makes it pay: the key transition AND the text, in the order they arrive.
    void text(const std::string& s) { publish(loom::to_value(input::TextEntered{s})); }

    /// A PRIMARY PRESS AT A CANVAS CELL, as the TERMINAL medium reports it (SEL-0).
    ///
    /// The cases below speak canvas cells rather than workspace cells, because a pane
    /// is placed on the canvas and never in the document's room -- `Live::term_x/y`
    /// exists for the other conversation and using it here would be the wrong inverse.
    void press_cell(std::int64_t cx, std::int64_t cy) {
        publish(loom::to_value(input::PointerButton{1, true, cx, cy + surface::kTuiCanvasTopRow,
                                                    input::space::kCells, input::mod::kNone}));
    }
    /// The same gesture as the WINDOW reports it: an exact device pixel, untranslated.
    /// A graphical press is finer than a cell, so its cases have to be able to say one.
    void press_pixel(std::int64_t px, std::int64_t py) {
        publish(loom::to_value(input::PointerButton{1, true, px, py, input::space::kPixels,
                                                    input::mod::kNone}));
    }
    /// A SECOND-BUTTON press at a canvas cell (CTX-0), `press_cell`'s own translation.
    void right_press_cell(std::int64_t cx, std::int64_t cy) {
        publish(loom::to_value(input::PointerButton{3, true, cx, cy + surface::kTuiCanvasTopRow,
                                                    input::space::kCells, input::mod::kNone}));
    }
    void release_cell(std::int64_t cx, std::int64_t cy) {
        publish(loom::to_value(input::PointerButton{1, false, cx, cy + surface::kTuiCanvasTopRow,
                                                    input::space::kCells, input::mod::kNone}));
    }

    /// Walk the picker to the row naming this reference and press Return.
    void pick(const PaneRef& ref) {
        key(input::scan::kP);
        const std::vector<CatalogRow> rows = combined_catalog(session().panels);
        std::size_t want = 0;
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (rows[i].ref == ref) {
                want = i;
            }
        }
        while (session().panels.picker.cursor < want) {
            key(input::scan::kDown);
        }
        key(input::scan::kReturn);
    }

    // ---- INTR-1: a real authored arrangement, and the door that answers for it ----
    //
    // ⚠ NO TIMER IN THIS RIG, EVER. `PaneRig` pumps to EMPTY and a live Timer service
    // re-arms its own beat inside its own handler, so a plan naming `zengine-timer`
    // here would not return. The arrangement facts that need a provider+weave artifact
    // are proved in `test_workshop_load.cpp`, whose rigs drain in bounded turns; what
    // is proved HERE is the pane seam, which needs neither.
    //
    // THE ARTIFACTS ARE RESOLVED FROM THE SUITE'S OWN `_SO` PATHS rather than from a
    // staging directory, because this tier is not asking where a host finds a file --
    // that is the load suite's question, and it has a real directory for it.
    static std::string artifact_path(const std::string& stem) {
        if (stem == "zengine-operators-basic") {
            return PROVIDER_BASIC_SO;
        }
        if (stem == "zengine-provider-min") {
            return PROVIDER_MIN_SO;
        }
        if (stem == "zengine-provider-a") {
            return PROVIDER_A_SO;
        }
        if (stem == "zengine-plain-weave") {
            return WORKSHOP_SO_HELLO;
        }
        if (stem == zengine::introspection::kIntrospectionStem) {
            return WORKSHOP_SO_INTROSPECTION;
        }
        return stem; // a stem this rig cannot spell refuses at the loader, by name
    }

    /// REALIZE AN AUTHORED PLAN ON THIS RIG'S BUS, the way the host does: the host
    /// writes the booter's grant, the owner realizes the rows, and the plan is
    /// RETAINED by the owner because the projection pairs authored intent with
    /// resolved state.
    ///
    /// THE TURNS ARE HERE, IN THE CALLER (BOOT-0). `begin` issues what it can and
    /// returns with a row in flight; a rig with no host loop of its own turns the crank
    /// itself. `PlanExecutor` never does -- see `test_workshop_load.cpp`, which owns
    /// that claim and its falsifiers.
    load::Executed run_plan(load::LoadPlan plan) {
        loom::Grant operate;
        operate.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
        auto speaker = std::make_unique<load::PlanBooter>(plan_answers);
        load::PlanBooter& voice = *speaker;
        const loom::WeaveId plan_booter = bus.register_weave(std::move(speaker),
                                                             std::move(operate));
        voice.zen_set_self(plan_booter);
        plan_ = std::make_unique<load::PlanExecutor>(bus, catalog, operator_host, voice, manager,
                                                     plan_answers, &artifact_path);
        plan_->begin(std::move(plan));
        for (int turn = 0; turn < 32; ++turn) {
            bus.pump_pending();
        }
        return plan_->outcome();
    }

    /// MOUNT THE HOST'S OBSERVATION DOOR, with the production grant spelled out --
    /// `mount_workshop`'s discipline, for its reason: a rig that minted the grant from
    /// the Emit set could not notice the host quietly widening it.
    loom::WeaveId mount_arrangement(std::string plan_path = std::string()) {
        REQUIRE(plan_ != nullptr); // a door with no owner would describe nothing
        auto door = std::make_unique<ArrangementDoor>(*plan_, catalog, std::move(plan_path));
        ArrangementDoor* raw = door.get();
        loom::Grant say;
        say.allow_to_any(ResolvedArrangement::zen_name, ResolvedArrangement::zen_version);
        say.allow_to_any(ResolvedPowers::zen_name, ResolvedPowers::zen_version);
        const loom::WeaveId id = bus.register_weave(std::move(door), std::move(say),
                                                    std::string(kArrangementRole));
        raw->zen_set_self(id);
        bus.drain_until_idle();
        return id;
    }

    /// EXPOSE THE HOST'S OWN TWO SOURCES, through the one door the host uses.
    ///
    /// ⚠ BEFORE THE PLAN RUNS, deliberately, exactly as `workshop.cpp` does it: the
    /// ordinary collision law then answers a provider that would supply one of these
    /// identities, in words, rather than letting load order decide.
    void expose_host_sources() {
        const op::MountReport done =
            mount_host_sources(catalog, host_sources(project_anchor, host_recipes));
        REQUIRE_MESSAGE(done.ok, done.reason);
    }

    /// WHOEVER HOLDS `zengine.skin` -- the medium that owns the platform clipboard in
    /// both directions. `Live` has the same seat for the same reason; a pane that can
    /// PASTE needs somebody to answer its ask (QR-11), and the Painter above holds no
    /// role and cannot.
    SkinSeat* mount_skin_seat() {
        auto seat = std::make_unique<SkinSeat>();
        SkinSeat* raw = seat.get();
        loom::Grant grant;
        grant.allow_to_any(loom::Ack::zen_name, loom::Ack::zen_version);
        grant.allow_to_any(surface::ClipboardText::zen_name, surface::ClipboardText::zen_version);
        const loom::WeaveId id =
            bus.register_weave(std::move(seat), std::move(grant), surface::kSkinRole);
        raw->zen_set_self(id);
        return raw;
    }

    /// MOUNT THE HOST'S SAMPLE DOOR (SOURCE-1), with the production grant spelled out
    /// -- `mount_arrangement`'s discipline for its reason: a rig that minted the grant
    /// from the Emit set could not notice the host quietly widening it.
    loom::WeaveId mount_sampler() {
        auto door = std::make_unique<SampleDoor>(catalog);
        SampleDoor* raw = door.get();
        loom::Grant say;
        say.allow_to_any(SourceSampled::zen_name, SourceSampled::zen_version);
        const loom::WeaveId id =
            bus.register_weave(std::move(door), std::move(say), std::string(kSampleRole));
        raw->zen_set_self(id);
        bus.drain_until_idle();
        return id;
    }

    load::BootAnswers plan_answers;

    Session& session() { return const_cast<Session&>(w->session()); }
    const surface::SurfaceCanvas& last_canvas() const { return canvases.back(); }
    /// THE NOTICE LINE, READ WHERE IT LIVES. `Session::notice` is painted onto the
    /// canvas, not published as a `SurfaceText` -- the published texts are the status
    /// slot, which is the document's line and says nothing about a pane, and (since
    /// the attention slot, which says what is CURRENTLY true.
    const std::string& last_notice() const { return w->session().notice; }

    /// WHAT IS CURRENTLY TRUE OF THIS WORKSHOP, through the same projection the
    /// screen and the compact indicator both spend -- never a second walk of the owners.
    std::vector<Condition> conditions() const {
        return attention_conditions(w->session(),
                                    host.frontier ? host.frontier() : ProjectFrontier{});
    }
    /// The compact attention line, as the medium was handed it.
    std::string attention_note() const {
        for (std::size_t i = notes.size(); i > 0; --i) {
            if (notes[i - 1].slot == surface::kSlotScore) {
                return notes[i - 1].text;
            }
        }
        return std::string();
    }

    std::vector<loom::WeaveId> seat_ids;
    std::vector<ProviderSeat*> seats_;
    /// HELD BY POINTER SO IT IS NOT CONSTRUCTED UNTIL A CASE ASKS, and declared LAST
    /// so it is destroyed FIRST: it retains the provider identity of every mount it
    /// made, and must not outlive the artifacts holding them.
    std::unique_ptr<load::PlanExecutor> plan_;
};

/// Every prose row of the region an external pane occupies -- Workshop's header row
/// included, and it is row 0 since TYPE-0 folded the header into the same region.
///
/// THE FIRST REGION AT THOSE BOUNDS, AND THAT IS A STATEMENT ABOUT ORDER (TYPE-0). The
/// picker and the pane-management surface open over the overlay stack's FIRST SLOT -- the
/// same rectangle an external pane in that slot occupies -- and since TYPE-0 both of them
/// are regions too. `all_texts` walks the planes back to front and `paint_panels` paints
/// every pane before either of those overlays, so the first match is the PANE's and any
/// later one is whatever is covering it. That is exactly the fact the Z0a control below
/// asks about: the provider is still publishing, and something is on top of it.
inline std::vector<std::string> external_region_rows(const surface::SurfaceCanvas& c,
                                                     const ui::Rect& body) {
    std::vector<std::string> out;
    for (const surface::SurfaceTextRegion& r : all_texts(c)) {
        if (r.x == body.x && r.y == body.y) {
            for (const surface::SurfaceTextRow& row : r.rows) {
                out.push_back(row.text);
            }
            return out;
        }
    }
    return out;
}

/// The rows an external pane's PROVIDER is currently showing, read off the published
/// canvas at the region the pane's body actually occupies -- never off the session
/// directly, so what a case reads is what a maker would see.
///
/// Workshop's own header is dropped, because it is Workshop's sentence and not the
/// provider's: TYPE-0 made it prose row 0 of the same region rather than a cell label
/// above it, and every case below is asking what the PROVIDER said.
inline std::vector<std::string> external_rows(const surface::SurfaceCanvas& c, const ui::Rect& body) {
    std::vector<std::string> out = external_region_rows(c, body);
    const std::size_t header = static_cast<std::size_t>(kExternalHeaderRows);
    out.erase(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(
                                             out.size() < header ? out.size() : header));
    return out;
}

inline PaneOffered good_offer() { return PaneOffered{"hello", "Hello", "a bounded external greeting"}; }

inline std::string bytes(std::size_t n, char c) { return std::string(n, c); }

/// The body bounds an open external pane resolves to -- read through the very
/// functions the painter and the pointer use, so a case cannot measure a rectangle
/// nothing draws in.
inline ui::Rect external_body_rect(const Session& s, std::int64_t kind) {
    const Screen sc = screen_of(s);
    const ExternalBodyPlace body =
        external_body_place(bounds_of(s.panels, s.setup.active, kind, sc).rect, sc,
                            external_title_rows(s.panels, kind, s.pane_titles));
    return ui::Rect{body.region_x, body.region_y, body.region_w, body.region_h};
}

/// The setup a WIND-2 case starts from: two overlay panes and the side region, at a screen
/// with room for two stack slots. Built through the doors, so the ranks are the identity
/// permutation `add_pane` assigns.
inline Setup two_overlays() {
    Setup s;
    s.name = "Arranged";
    REQUIRE(add_pane(s, ref_of(panel::kBuilder)));
    REQUIRE(add_pane(s, ref_of(panel::kInfo)));
    return s;
}

/// A `Live`'s session, mutably -- the same door `PaneRig::session` already opens, so a case
/// that has to arrange a setup DIRECTLY (rather than through the keys it is measuring) can.
inline Session& live(Live& t) { return const_cast<Session&>(t.session()); }

/// The kinds a setup AUTHORS, in the order the file holds them. Since WUX-5 this is the
/// BASE and not what a maker sees: `painted_order` below is the effective one.
inline std::vector<std::int64_t> authored_order(const Session& s) {
    return presentation_order(s.setup.active, s.panels);
}

/// The kinds a setup resolves to, in the order they would be PAINTED -- the authored order
/// with the selected pane lifted (WUX-5). The same call paint, hit testing and coverage
/// spend, so a case reading it is reading the picture.
inline std::vector<std::int64_t> painted_order(const Session& s) {
    return effective_pane_order(s.setup.active, s.panels);
}

/// The front ranks of a setup, in list order -- what every ordering case reads.
inline std::vector<std::int64_t> ranks_of(const Setup& s) {
    std::vector<std::int64_t> out;
    for (const SetupPane& row : s.panes) {
        out.push_back(row.front);
    }
    return out;
}

/// Is this set of ranks exactly `{0 .. n-1}`? The invariant, spelled once so every ordering
/// case asks it the same way and none of them can weaken it by accident.
inline bool is_permutation(const Setup& s) {
    const std::size_t n = s.panes.size();
    std::vector<bool> seen(n, false);
    for (const SetupPane& row : s.panes) {
        if (row.front < 0 || row.front >= static_cast<std::int64_t>(n)) {
            return false;
        }
        if (seen[static_cast<std::size_t>(row.front)]) {
            return false;
        }
        seen[static_cast<std::size_t>(row.front)] = true;
    }
    return true;
}

/// THE CONTEXTUAL SURFACE'S GEOMETRY IN CANVAS CELLS on a CELL medium (CTX-0; local
/// bounds since ARR-0; inside its own chrome since WUX-5) -- through the same
/// `context_bounds` the painter and the press resolver spend, never a second arithmetic.
/// `context_entry_cell_y` is population row `index`'s canvas row WHILE the window shows the
/// population from its top with no `earlier` marker, which every case using it arranges
/// (the pane and object populations always fit the room). The surface reserves NO heading
/// rows now, so row `index` is the `index`'th row of the interior.
inline std::int64_t context_cell_x(const Session& s) {
    return surface::cell_of_subs(context_bounds(s, screen_of(s)).x) + kChromeCells + 1;
}
inline std::int64_t context_entry_cell_y(const Session& s, std::size_t index) {
    return surface::cell_of_subs(context_bounds(s, screen_of(s)).y) + kChromeCells +
           static_cast<std::int64_t>(index);
}

/// The surface's published region, read off a canvas at exactly its bounds -- searched
/// BACK TO FRONT because the picker, a slot-seated pane and the attention view can share
/// the popup's origin, and the contextual surface paints over all of them.
inline std::vector<std::string> context_rows_on(const surface::SurfaceCanvas& c,
                                                const Session& s) {
    const surface::SurfaceTextRegion want =
        panel_prose_region(context_bounds(s, screen_of(s)));
    for (std::size_t li = c.layers.size(); li > 0; --li) {
        const surface::SurfaceLayer& layer = c.layers[li - 1];
        for (std::size_t ri = layer.texts.size(); ri > 0; --ri) {
            const surface::SurfaceTextRegion& r = layer.texts[ri - 1];
            if (r.x != want.x || r.y != want.y || r.w != want.w || r.h != want.h) {
                continue;
            }
            std::vector<std::string> out;
            for (const surface::SurfaceTextRow& row : r.rows) {
                std::string text = row.text;
                while (!text.empty() && text.back() == ' ') {
                    text.pop_back();
                }
                out.push_back(std::move(text));
            }
            return out;
        }
    }
    return {};
}

/// OPEN A PANE THROUGH THE PICKER, the way a maker does -- and BOUNDED, so a case that
/// cannot reach the row it wants fails with a sentence instead of spinning. An unbounded
/// `while (cursor != want)` is right until the first case that reaches it with the picker
/// closed, at which point the suite stops rather than reddens.
inline void open_pane(Live& t, const PaneRef& ref) {
    REQUIRE_FALSE(t.session().arrange.open); // `p` belongs to command mode
    t.key(input::scan::kP);
    REQUIRE(t.session().panels.picker.open);
    const std::vector<CatalogRow> rows =
        inventory_rows(t.session().setup.active, t.session().panels);
    std::size_t want = rows.size();
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].ref == ref) {
            want = i;
        }
    }
    REQUIRE(want < rows.size());
    for (std::size_t guard = 0; guard <= rows.size(); ++guard) {
        if (t.session().panels.picker.cursor == want) {
            break;
        }
        t.key(input::scan::kDown);
    }
    REQUIRE(t.session().panels.picker.cursor == want);
    t.key(input::scan::kReturn);
    REQUIRE(has_pane(t.session().setup.active, ref));
}

/// Put the desk's keyboard address on a setup-named pane, by the key a maker presses.
/// Bounded for `open_pane`'s reason.
inline void select_pane(Live& t, const PaneRef& ref) {
    REQUIRE(t.session().arrange.open);
    for (std::size_t guard = 0; guard <= t.session().setup.active.panes.size() + 1; ++guard) {
        if (t.session().arrange.pane == ref) {
            return;
        }
        t.key(input::scan::kTab);
    }
    FAIL("no arrangeable row for ", ref_text(ref));
}

/// A `Live` driven into the desk arrangement scope (ARR-0). Every keyboard case begins
/// here, and it goes through the REAL input path -- the `w` transition and the character
/// the platform's layout made of it, both, in the order the backends report them.
inline void enter_arrange_desk(Live& t) {
    t.key(input::scan::kW);
    t.text("w");
    REQUIRE(t.session().arrange.open);
    REQUIRE(t.session().arrange.desk);
}

namespace intro = zengine::introspection;

inline constexpr const char* kIntroOffice = intro::kIntrospectionRole;
inline constexpr const char* kIntroPane = intro::kLoadedPane;

inline PaneRef intro_ref() { return PaneRef{kIntroOffice, kIntroPane}; }

/// HOW MANY PANES THIS ONE OFFICE OFFERS (INTR-1). It was one for two phases; the
/// cases below say the number rather than assuming it, because "one provider is not
/// one pane" is exactly what `PaneOffered` was shaped for and a case that indexed
/// `entries[0]` was quietly asserting the opposite.
inline constexpr std::size_t kIntroPaneCount = 3;

/// THE RUNTIME HANDLE WORKSHOP MINTED FOR ONE OF THIS OFFICE'S PANES -- BY `PaneRef`
/// AND NEVER BY INDEX. Catalog order is first-accepted-offer order, which is a fact
/// about a boot sequence and not an identity; a case that meant `loaded` says so.
inline const RuntimePane* intro_row(PaneRig& r, const char* pane) {
    return r.session().panels.runtime.find(kIntroOffice, pane);
}

/// Does any row of this projection contain `needle`?
inline bool any_row(const std::vector<surface::SurfaceTextRow>& rows, const std::string& needle) {
    for (const surface::SurfaceTextRow& r : rows) {
        if (r.text.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

inline bool any_row(const std::vector<std::string>& rows, const std::string& needle) {
    for (const std::string& r : rows) {
        if (r.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

/// A population of `n` distinct weaves, each with a role -- the input the density
/// sweep varies, so that what changes between budgets is only the budget.
inline std::vector<intro::LoadedWeave> loaded_population(std::size_t n) {
    std::vector<intro::LoadedWeave> out;
    for (std::size_t i = 0; i < n; ++i) {
        out.push_back(intro::LoadedWeave{"weave-" + std::to_string(i), "role." + std::to_string(i)});
    }
    return out;
}

/// Every region on a canvas whose upper-left corner is this cell.
inline std::vector<surface::SurfaceTextRegion> regions_at(const surface::SurfaceCanvas& c,
                                                          std::int64_t x, std::int64_t y) {
    std::vector<surface::SurfaceTextRegion> out;
    for (const surface::SurfaceTextRegion& r : all_texts(c)) {
        if (r.x == x && r.y == y) {
            out.push_back(r);
        }
    }
    return out;
}

/// A session with a screen extent and a text metric, and a workspace that fills the room.
inline Session screen_session(std::int64_t w, std::int64_t h, std::int64_t advance,
                              std::int64_t line) {
    Session s;
    s.screen_w = w;
    s.screen_h = h;
    s.text_advance_px = advance;
    s.text_line_px = line;
    const Screen sc = screen_of(s);
    s.workspace_w = sc.room_w;
    s.workspace_h = sc.room_h;
    return s;
}

/// A PRESS AT A CANVAS CELL AS THE GRAPHICAL MEDIUM REPORTS IT -- the pixel at the
/// middle of that cell, which is where a maker's pointer actually is.
///
/// It is the inverse of `plan_canvas`'s layout and NOT of the terminal's, because the
/// two media report different numbers for one place (docs/reference/pointer-spaces.md).
inline std::int64_t cell_mid_px(std::int64_t cell) {
    return cell * surface::kCanvasCellPx + surface::kCanvasCellPx / 2;
}

/// THE PANEL RECTANGLE AN EXTERNAL PANE OCCUPIES -- the painter's own, through the one
/// `bounds_of` path, so a case never spells a placement of its own.
inline FineRect external_panel_rect(const Session& s, std::int64_t kind) {
    return bounds_of(s.panels, s.setup.active, kind, screen_of(s)).rect;
}

/// The room Workshop resolved for that pane's body -- the `PaneRoom` a provider was
/// granted, read from the same function that granted it.
inline ExternalBodyPlace external_body_of(const Session& s, std::int64_t kind) {
    return external_body_place(external_panel_rect(s, kind), screen_of(s),
                               external_title_rows(s.panels, kind, s.pane_titles));
}

/// WHAT AN INDEPENDENT LISTENER HEARD -- kept outside the weave so a case can outlive
/// the bus that delivered it, which is the same shape `Painter` uses for canvases.
struct Ears {
    std::vector<intro::LoadedSelected> heard;
    std::vector<std::string> authors;
};

/// AN INDEPENDENT LISTENER, and the point is that it is a STRANGER.
///
/// It compiles `introspection/vocabulary.hpp` and nothing else of the tool: no callback
/// is registered anywhere, no pointer is handed to anybody, and it is not Workshop, not
/// a provider and not in any office. What it hears, it hears because a weave published
/// an ordinary Loom message and the bus delivered it -- which is the whole claim
/// `LoadedSelected` exists to make good on.
class SelectionListener
    : public loom::WeaveBase<SelectionListener, SeenState, loom::Accept<intro::LoadedSelected>,
                             loom::Emit<>> {
public:
    explicit SelectionListener(Ears& ears) : ears_(&ears) {}
    void on(const intro::LoadedSelected& s, loom::Mail& mail) {
        ++state_.frames;
        ears_->heard.push_back(s);
        ears_->authors.push_back(std::string(mail.authored_role()));
    }

private:
    Ears* ears_;
};

/// The prose rows the Loaded pane is currently showing, with Workshop's header dropped
/// -- said once here so a case below reads as a gesture and an assertion.
inline std::vector<std::string> loaded_rows(PaneRig& r, std::int64_t kind) {
    return external_rows(r.last_canvas(), external_body_rect(r.session(), kind));
}

/// The library name an entry row shows, read off the canvas: `  name @role` without
/// its two-character mark. A case must never assume WHICH weave a row holds -- the
/// kernel's map decides that -- so it reads the row a maker would have aimed at.
inline std::string named_by(const std::string& row) {
    const std::size_t at = row.find(" @");
    return at == std::string::npos ? std::string() : row.substr(2, at - 2);
}

inline bool is_entry_row(const std::string& row) {
    return row.size() > 2 && row.find(" @") != std::string::npos &&
           (row.rfind(intro::kUnselectedMark, 0) == 0 || row.rfind(intro::kSelectedMark, 0) == 0) &&
           row.find(intro::kElided) == std::string::npos;
}

inline constexpr const char* kComposerOffice = zengine::composer::kComposerRole;
inline constexpr const char* kComposePane = zengine::composer::kComposePane;
inline constexpr const char* kTimerOffice = zengine::timer::kTimerRole;

inline PaneRef composer_ref() { return PaneRef{kComposerOffice, kComposePane}; }

/// Open an external pane belonging to a seat in `office`, and answer with its kind.
/// The seat offers, Workshop admits, the picker opens it, and the room is granted --
/// four beats a case would otherwise spell every time.
inline std::int64_t seat_pane_open(PaneRig& r, ProviderSeat* seat, const char* office,
                                   const char* pane) {
    r.drive(seat, [pane](ProviderSeat& s, loom::Mail& m) {
        s.offer(m, PaneOffered{pane, "Seat", "a recording provider"});
    });
    r.pick(PaneRef{office, pane});
    for (const RuntimePane& row : r.session().panels.runtime.entries) {
        if (row.provider == std::string(office) && row.pane == std::string(pane)) {
            return row.kind;
        }
    }
    return kNoPaneKind;
}

/// Press the first prose row of an external pane's body, in cells.
inline void press_body(PaneRig& r, std::int64_t kind) {
    const ui::Rect body = external_body_rect(r.session(), kind);
    r.press_cell(body.x + 1, body.y + kExternalHeaderRows);
}

/// Press somewhere this pane is NOT -- the gesture that hands the keyboard back to
/// Workshop, which is what a case has to perform before it can use a command key.
///
/// THE CELL IS DERIVED FROM THE PANE'S OWN RECTANGLE rather than spelled. An overlay
/// slot starts at the canvas's top-left corner, so a literal `(1, 1)` is inside the
/// pane it was meant to be outside of -- which is a mistake that reads as a defect in
/// the routing rather than as a defect in the case.
inline void press_outside(PaneRig& r, std::int64_t kind) {
    const ui::Rect panel = cells_covered(external_panel_rect(r.session(), kind));
    r.press_cell(panel.x + 1, panel.y + panel.h + 1);
}

namespace ws = zengine::workshop;

/// AN ARRANGEMENT BUILT AS A VALUE, so a projection can be asked what it means without
/// a bus, a Kernel or an artifact anywhere near it.
inline ws::ArtifactParticipation participation(const char* stem, const char* mode, const char* role) {
    ws::ArtifactParticipation a;
    a.artifact = stem;
    a.authored_provider = mode;
    a.authored_role = role;
    a.state = ws::kResolvedToken;
    return a;
}

inline ws::ArtifactParticipation resolved_provider(ws::ArtifactParticipation a, const char* identity,
                                                   std::int64_t powers) {
    a.provider = identity;
    a.powers = powers;
    return a;
}

inline ws::ArtifactParticipation resolved_weave(ws::ArtifactParticipation a, std::int64_t id,
                                                const char* offer) {
    a.weave = id;
    a.offer = offer;
    return a;
}

inline ws::PowerStack power_of(const char* identity, std::vector<ws::PowerContribution> stack) {
    ws::PowerStack p;
    p.power = identity;
    p.contributions = std::move(stack);
    return p;
}

inline ws::PowerContribution supplied_by(const char* provider, bool composite = false) {
    ws::PowerContribution c;
    c.provider = provider;
    c.composite = composite;
    return c;
}

/// The production-shaped arrangement, as a value: a provider-only artifact, two
/// weave-only artifacts, and one artifact that is BOTH.
inline ws::ResolvedArrangement shaped_arrangement() {
    ws::ResolvedArrangement said;
    said.plan = "default-load-plan.json";
    said.artifacts.push_back(resolved_provider(
        participation("zengine-operators-basic", "normal", ""), "zengine.operators.basic", 2));
    said.artifacts.push_back(resolved_weave(
        participation("zengine-skin-tui-classic", "", "zengine.skin"), 4, "not-a-consumer"));
    said.artifacts.push_back(resolved_weave(
        resolved_provider(participation("zengine-timer", "normal", "zengine.timer"),
                          "zengine.timer", 1),
        7, "offered"));
    said.artifacts.push_back(resolved_weave(
        participation("zengine-composer", "", "zengine.composer"), 9, "not-a-consumer"));
    return said;
}

/// The production-shaped powers, as a value: two natives from one provider and one
/// composite from another.
inline ws::ResolvedPowers shaped_powers() {
    ws::ResolvedPowers said;
    said.providers = {"zengine.operators.basic", "zengine.timer"};
    said.powers.push_back(power_of("logic.select_int", {supplied_by("zengine.operators.basic")}));
    said.powers.push_back(power_of("math.max", {supplied_by("zengine.operators.basic")}));
    said.powers.push_back(
        power_of("timer.normalize_delay", {supplied_by("zengine.timer", /*composite=*/true)}));
    return said;
}

/// ---- SOURCE-1: contributions that carry the exterior contract ------------------
///
/// `supplied_by` above predates the `source`/`output` fields and leaves both at their
/// defaults, which is an OPERATOR with no reported output schema -- still exactly what
/// the arrangement cases want. These two say the other half, in the shape
/// `describe_powers` would have read off a real definition: `source` is
/// `op::is_source` and the output identity is the definition's own output schema.
///
/// THE PROVIDER, THE CONSTRUCTION AND THE CONTRACT ARE THREE SEPARATE ARGUMENTS,
/// because the whole claim under test is that they are three independent facts.
inline ws::PowerContribution source_from(const char* provider, bool composite = false,
                                         const char* yields = "zengine.Fixture") {
    ws::PowerContribution c;
    c.provider = provider;
    c.composite = composite;
    c.source = true;
    c.output.name = yields;
    c.output.version = 1;
    return c;
}

inline ws::PowerContribution operator_from(const char* provider, bool composite = false,
                                           const char* yields = "zengine.Fixture") {
    ws::PowerContribution c = source_from(provider, composite, yields);
    c.source = false;
    return c;
}

/// ALL FOUR `Source x Composite` CELLS, WITH DELIBERATELY MISLEADING NAMES.
///
/// `source.looks.like.one` takes arguments and `math.max` does not, which is the
/// falsifier for any implementation tempted to read a view membership off an
/// identity's spelling: a pane that classified by name would put both in the wrong
/// list and every other case here would stay green.
inline ws::ResolvedPowers four_cells() {
    ws::ResolvedPowers said;
    said.providers = {"zengine.operators.basic", "zengine.timer", "zengine.workshop.host"};
    said.powers.push_back(power_of("logic.select_int", {operator_from("zengine.operators.basic")}));
    said.powers.push_back(power_of("math.max", {source_from("zengine.workshop.host", false,
                                                            "zengine.MaxSoFar")}));
    said.powers.push_back(
        power_of("source.looks.like.one", {operator_from("zengine.timer", /*composite=*/true)}));
    said.powers.push_back(power_of("zengine.recipes.catalog",
                                   {source_from("zengine.workshop.host", /*composite=*/true,
                                                "zengine.RecipeCatalog")}));
    return said;
}

/// A population big enough that a pane has to window it, in catalog (name) order.
inline ws::ResolvedPowers many_sources(std::size_t n) {
    ws::ResolvedPowers said;
    said.providers = {"zengine.fixture"};
    for (std::size_t i = 0; i < n; ++i) {
        std::string id = "src." + std::string(i < 10 ? "0" : "") + std::to_string(i);
        said.powers.push_back(power_of(id.c_str(), {source_from("zengine.fixture")}));
    }
    return said;
}

/// A Powers pane state holding one reading, with nothing else authored.
inline intro::PowersUi showing(ws::ResolvedPowers said) {
    intro::PowersUi ui;
    ui.reading = std::move(said);
    ui.read = true;
    return ui;
}

inline std::vector<std::string> texts_of(const std::vector<surface::SurfaceTextRow>& rows) {
    std::vector<std::string> out;
    for (const surface::SurfaceTextRow& r : rows) {
        out.push_back(r.text);
    }
    return out;
}

/// Which row of a projection carries `needle`, or -1.
inline std::int64_t row_with(const std::vector<surface::SurfaceTextRow>& rows, const std::string& needle) {
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].text.find(needle) != std::string::npos) {
            return static_cast<std::int64_t>(i);
        }
    }
    return -1;
}

/// The same question of rows already read off a published canvas.
inline std::int64_t row_with_text(const std::vector<std::string>& rows, const std::string& needle) {
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].find(needle) != std::string::npos) {
            return static_cast<std::int64_t>(i);
        }
    }
    return -1;
}

/// The arrangement this tier performs, and it is deliberately Timer-free (see the
/// section header). It still carries a PROVIDER-ONLY artifact and two weave-only ones,
/// which is what the pane claims must be distinguishable.
inline load::LoadPlan pane_plan() {
    load::LoadPlan plan;
    load::ArtifactIntent basic;
    basic.stem = "zengine-operators-basic";
    basic.provider = load::ProviderIntent{op::MountMode::Ordinary};
    plan.artifacts.push_back(basic);
    load::ArtifactIntent hello;
    hello.stem = "zengine-plain-weave";
    hello.weave = load::WeaveIntent{"test.plain"};
    plan.artifacts.push_back(hello);
    load::ArtifactIntent tool;
    tool.stem = intro::kIntrospectionStem;
    tool.weave = load::WeaveIntent{kIntroOffice};
    plan.artifacts.push_back(tool);
    return plan;
}

/// Stand a live Workshop up over that arrangement, with the host's observation door
/// mounted, and open one of the tool's panes.
inline std::int64_t open_intro_pane(PaneRig& r, const char* pane) {
    r.mount_workshop();
    const load::Executed done = r.run_plan(pane_plan());
    REQUIRE_MESSAGE(done.ok, done.refusal);
    r.mount_arrangement("default-load-plan.json");
    r.ready();
    r.extent(160, 48);
    REQUIRE(intro_row(r, pane) != nullptr);
    r.pick(PaneRef{kIntroOffice, pane});
    return intro_row(r, pane)->kind;
}

/// THE SAME LIVE WORKSHOP, WITH THE POWERS PANE OPEN AND BOTH HOST DOORS MOUNTED
/// (SOURCE-1) -- and with the host's own two Sources really in the catalog, so the
/// Sources view has the population a maker actually meets.
///
/// THE ORDER IS THE HOST'S: sources exposed before the plan runs, the observation
/// door and the sample door mounted before realization can grant a pane room.
inline std::int64_t open_powers(PaneRig& r) {
    r.expose_host_sources();
    r.mount_workshop();
    const load::Executed done = r.run_plan(pane_plan());
    REQUIRE_MESSAGE(done.ok, done.refusal);
    r.mount_arrangement("default-load-plan.json");
    (void)r.mount_sampler();
    r.ready();
    r.extent(160, 48);
    REQUIRE(intro_row(r, intro::kPowersPane) != nullptr);
    r.pick(PaneRef{kIntroOffice, intro::kPowersPane});
    return intro_row(r, intro::kPowersPane)->kind;
}

inline std::vector<std::string> pane_rows(PaneRig& r, std::int64_t kind) {
    return external_rows(r.last_canvas(), external_body_rect(r.session(), kind));
}

/// PRESS A PLACE IN AN EXTERNAL PANE'S OWN ROOM -- the provider's row and column,
/// which is exactly the pair `PanePressed` carries. Cases speak the provider's lattice
/// so the arithmetic that turns it into a canvas cell lives in one place.
inline void press_pane(PaneRig& r, std::int64_t kind, std::int64_t row, std::int64_t column) {
    const ui::Rect body = external_body_rect(r.session(), kind);
    r.press_cell(body.x + column, body.y + kExternalHeaderRows + row);
}

/// POINT THE KEYBOARD AT A PANE WITHOUT ALSO AUTHORING A GESTURE.
///
/// Workshop's focus rule is "the pane a maker last pressed into" (MSG-0) and there is
/// no shape for asking, so a case that wants to type has to press first. This spends
/// that press on a row whose provider-side meaning is NOTHING -- the bound sentence
/// where there is one, and the last row otherwise -- which is only possible because
/// every target in this pane means exactly one thing.
inline void focus_pane(PaneRig& r, std::int64_t kind) {
    const std::vector<std::string> shown = pane_rows(r, kind);
    REQUIRE_FALSE(shown.empty());
    std::int64_t row = static_cast<std::int64_t>(shown.size()) - 1;
    const std::int64_t bound = row_with_text(shown, intro::kHostResolution);
    if (bound >= 0) {
        row = bound;
    }
    press_pane(r, kind, row, 0);
}

/// Which column of the Powers chrome row carries a given word, or -1 -- how a case
/// aims at the `Operators` control without knowing the composition's arithmetic.
inline std::int64_t chrome_column(PaneRig& r, std::int64_t kind, const std::string& word) {
    const std::vector<std::string> shown = pane_rows(r, kind);
    if (shown.empty()) {
        return -1;
    }
    const std::size_t at = shown[0].find(word);
    return at == std::string::npos ? -1 : static_cast<std::int64_t>(at);
}

/// MAKE ONE PANE TALLER, the way WIND-2 lets a maker: an authored height in canvas
/// cells, written into the setup the screen actually resolves its bounds from.
///
/// THIS IS NOT A TEST DOOR. `kStackRows` is NINE -- eight prose rows under one header --
/// and that is the DEVELOPER'S DEFAULT rather than a law, which is exactly what WIND-2's
/// authored window is for. A projection whose entries are several rows tall does not fit
/// six artifacts in eight rows and never could; what it does instead is count what it
/// could not show, and give the maker a pane that grows.
inline void make_taller(PaneRig& r, const char* pane, std::int64_t cells) {
    const Written wrote =
        author_pane_size(r.session().setup.active, PaneRef{kIntroOffice, pane}, PaneSize{},
                         PaneSize{pane_unit::kSubcells, subs(cells)});
    REQUIRE_MESSAGE(wrote.accepted, wrote.refusal);
    // A REPAINT, WHICH IS A ROOM GRANT, WHICH IS THIS TOOL'S ONE BEAT.
    r.extent(200, 60);
}

inline std::string file_source(const char* path) {
    std::ifstream in(path);
    REQUIRE_MESSAGE(in.good(), "cannot read ", path);
    std::ostringstream all;
    all << in.rdbuf();
    return all.str();
}

/// A keymap file's bytes, composed through the format's own serializer -- which is
/// also what makes the round-trip cases byte-exact rather than approximately so.
inline std::string keymap_file_text(const std::string& legend,
                                    const std::vector<std::pair<std::string, std::string>>& rows) {
    keymap_persist::WorkshopKeymap f;
    f.format = keymap_persist::kFormat;
    f.format_version = keymap_persist::kFormatVersion;
    f.legend = legend;
    for (const std::pair<std::string, std::string>& r : rows) {
        f.overrides.push_back(keymap_persist::WorkshopKeymapRow{r.first, r.second});
    }
    return loom::compat::serialize(loom::to_value(f));
}

inline void write_keymap_file(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
}

/// A Live Workshop that read the given keymap file on its first surface.
struct Keyed : Live {
    explicit Keyed(const std::string& path) {
        host.keymap_path = path;
        publish(loom::to_value(surface::SurfaceReady{}));
    }
};

#endif // ZENGINE_TESTS_WORKSHOP_SUPPORT_HPP
