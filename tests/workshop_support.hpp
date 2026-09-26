// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_WORKSHOP_SUPPORT_HPP
#define ZENGINE_TESTS_WORKSHOP_SUPPORT_HPP

// Shared support for the Workshop suites: what more than one of them needs, and nothing a single
// suite uses (VM-POP-13). Fixture placement, actor grants and delayed replies:
// docs/contributing/testing-workshop-panes.md. The helpers are `inline` at namespace scope: a
// namespace would change unqualified lookup inside each of them, and `inline` rather than
// `static` keeps one a suite does not use silent under -Wunused-function. It includes doctest.h
// because the fixtures assert.

#include "doctest.h"

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
#include "workshop/grant.hpp"
#include "workshop/opening.hpp" // the opening manager the host mounts
#include "workshop/vocabulary.hpp"
// The process id a temporary root is named by, and on Windows the attribute the sweep reads.
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h> // getpid
#endif

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
#include "workshop/provenance.hpp" // what stands behind an office's code, wired as the host wires it
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
#include <zen/kernel/admission.hpp>
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


/// A `component::TextBox` holding a text of this LENGTH, with its caret and its window where a
/// case wants them. The window is moved through the real door, never written:
/// `keep_caret_visible(caret - first)` lands exactly on `first` for every value a case can ask for
/// (its rule 1 caps the window at `size - room`, which is `first` here, and rule 3 pulls it up to
/// the same place). A window past the end of the text (`first > length`) collapses to the end.
inline component::TextBox box_of(std::size_t length, std::size_t caret, std::size_t first) {
    component::TextBox b;
    b.set(std::string(length, 'x'), caret);
    b.keep_caret_visible(caret > first ? static_cast<std::int64_t>(caret - first) : 0);
    return b;
}

/// A canvas is a list of planes. These concatenate one KIND across them, in the order a medium
/// walks them -- the right shape for "was this drawn" and "which of two same-kind primitives is
/// later" -- and none is a visibility oracle: what is SEEN at a cell is `cell_text_of` below
/// (text) and `rasterized` (the whole picture), which run the Skin's own two-level order.
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
/// overlays published, and nothing the DOCUMENT published. The workspace plane carries a text
/// region per placed object (the maker's authored name, `kGroundBeneath`), so a case counting the
/// chrome's regions or indexing a panel's projected rows asks about the planes after it. A plane
/// and not a predicate, because `paint` writes the workspace whole into `layers.front()` before
/// any pane exists; cases about the names themselves read `object_names`.
inline surface::SurfaceCanvas without_workspace(const surface::SurfaceCanvas& c) {
    surface::SurfaceCanvas out = c;
    if (!out.layers.empty()) {
        out.layers.erase(out.layers.begin());
    }
    return out;
}

/// THE AUTHORED OBJECTS' NAMES, as the workspace plane published them.
inline std::vector<surface::SurfaceTextRegion> object_names(const surface::SurfaceCanvas& c) {
    return c.layers.empty() ? std::vector<surface::SurfaceTextRegion>{} : c.layers.front().texts;
}

/// WHAT A TERMINAL WITH NO USEFUL COLOUR SHOWS -- the canvas's characters with every SGR
/// sequence removed. A role is ink, and ink is the half of a medium's answer a monochrome
/// terminal does not receive; `glyph_for_role` is the other half, so this string is exactly what
/// survives without the first. `\x1b[2K` goes with the rest -- it is an erase, not a character.
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

/// A CELL COUNT ON THE FINE LATTICE -- the one multiply a case that thinks in whole cells performs
/// to author or compare pane geometry, which is in sub-units. Short because it appears wherever a
/// case says "40 cells".
inline constexpr std::int64_t subs(std::int64_t cells) { return cells * surface::kCellSubs; }

/// THE PLANE A CASE THAT BUILDS ITS OWN CANVAS BY HAND WORKS ON, created on first use.
/// A case exercising ONE painter is asking about one presentation, which is one plane.
inline surface::SurfaceLayer& plane(surface::SurfaceCanvas& c) {
    if (c.layers.empty()) {
        c.layers.emplace_back();
    }
    return c.layers.back();
}

/// ONE REGION ON A CANVAS OF ITS OWN, at the same extent -- the plan for exactly this
/// presentation and nothing else on the screen. A fresh canvas, not the published one with a
/// plane overwritten: a Workshop canvas carries a plane per presentation, so replacing one
/// plane's regions would leave every other plane's on it.
inline surface::SurfaceCanvas canvas_of_region(const surface::SurfaceCanvas& like,
                                               const surface::SurfaceTextRegion& r) {
    surface::SurfaceCanvas out;
    out.width = like.width;
    out.height = like.height;
    out.layers.emplace_back();
    out.layers.back().texts.push_back(r);
    return out;
}

/// EVERY REGION THIS MEDIUM SETS IN REAL TYPE, and every region it draws as cells, gathered
/// across the planes in the order a medium walks them -- for the PARTITION questions ("is this
/// region in exactly one of the two lists", "how many rows did the pane get"), not for order:
/// `test_surface` states the ordering law over planes built for it, and `rasterized` says what is
/// actually on top.
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

/// EVERYTHING ON A CANVAS THAT A CELL MEDIUM WOULD SHOW AS TEXT, in painter's order: per plane,
/// its labels and then its text regions projected onto cells -- through the projection the
/// terminal Skins use (`surface::project_text_regions`), never a second reading invented here, so
/// these assertions describe the picture the golden-byte suite pins in test_surface.cpp. It is
/// `canvas_body`'s order, so the LAST entry at a cell is the topmost text there. Text only: a
/// later plane's rect over a label is a question for `rasterized`.
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

/// ONE PROSE ROW OF THE INSPECTOR'S PROPERTY BODY, as a maker reads it. The body is a bounded
/// region, which owns what is inside its bounds, so its cell projection pads every row to the
/// region's full width; that padding erases what was underneath and is noise in an assertion
/// about what a row SAYS, so this trims it. Through `label_at` and the real cell projection, so a
/// caret is still inserted at its own column and a row cut at the body's width is still cut.
inline std::string inspector_row(const surface::SurfaceCanvas& c, std::int64_t x, std::int64_t y) {
    std::string text = label_at(c, x, y);
    while (!text.empty() && text.back() == ' ') {
        text.pop_back();
    }
    return text;
}

/// THE SENTENCE THE TOOL IS SAYING, as a maker reads it. The notice is a bounded region two cells
/// tall -- the smallest room that holds one row of a real face -- read through the cell projection
/// with the region's padding trimmed, as the Inspector's rows are.
inline std::string notice_line(const surface::SurfaceCanvas& c, const Screen& sc) {
    return inspector_row(c, 0, sc.notice_y);
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

/// THE SETUP A CASE THAT BUILT ITS `Panels` BY HAND IS IMPLICITLY WORKING UNDER: one authored row
/// per open panel, in open order, with no override on any axis and the identity front ranks --
/// exactly what `reconcile` would have produced from that setup -- so such a case still asks the
/// ONE resolver the question the application asks, not a second one with a defaulted argument.
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

/// ONE DESK AS A WHOLE LAYOUT RUN -- what a case means when it says something about a session and
/// nothing about the plural. The session format carries the maker's run and the position that was
/// live, so a case that means "one desk" says so once, here, not `{desk}, 0` at sixty call sites.
/// Its association is `none`: the layout is durable and related to no standalone Setup file.
inline std::vector<Layout> one_layout(Setup desk) {
    std::vector<Layout> run;
    run.push_back(Layout{std::move(desk), SetupLink{}});
    return run;
}

/// A RUN OF PLAIN DESKS, EACH WITH NO ASSOCIATION -- the same convenience for the cases
/// that mean several layouts and nothing about their Setup relationships.
inline std::vector<Layout> plain_run(std::vector<Setup> desks) {
    std::vector<Layout> run;
    run.reserve(desks.size());
    for (Setup& desk : desks) {
        run.push_back(Layout{std::move(desk), SetupLink{}});
    }
    return run;
}

/// ONE LAYOUT ASSOCIATED WITH AN ARTIFACT WHOSE KNOWN VALUE IS `known`.
inline Layout linked_layout(Setup desk, std::string path, Setup known) {
    return Layout{std::move(desk), SetupLink{std::move(path), std::move(known)}};
}

/// THE DESKS OF A RUN, in order -- for the cases that compare arrangements and mean nothing
/// about associations.
inline std::vector<Setup> desks_of(const std::vector<Layout>& run) {
    std::vector<Setup> out;
    out.reserve(run.size());
    for (const Layout& layout : run) {
        out.push_back(layout.desk);
    }
    return out;
}

/// THE LAYOUT A LOADED SESSION WAS STANDING ON. `at()` rather than `[]` deliberately: a
/// case that reads this off a session that was NOT admitted has asked the wrong question,
/// and an exception says so where a default-constructed desk would quietly pass.
inline const Setup& live_layout(const session_persist::LoadedSession& loaded) {
    return loaded.layouts.at(loaded.active).desk;
}

/// ...AND THE SETUP ASSOCIATION IT WAS STANDING ON, by the same rule.
inline const SetupLink& live_link(const session_persist::LoadedSession& loaded) {
    return loaded.layouts.at(loaded.active).link;
}

/// THE LIVE LAYOUT'S SETUP-ASSOCIATION VERDICT -- `none`, `current` or `modified` -- asked of the
/// layout that owns it: a fresh desk is `none`, related to no artifact, and two layouts can give
/// two different answers.
inline std::int64_t live_status(const SetupState& s) {
    return link_status(s.active, s.active_link);
}

/// RELATE THE LIVE LAYOUT TO `path`, WITH ITS CURRENT DESK AS THE KNOWN VALUE -- the state a
/// successful `s` leaves behind, staged directly for the cases that are about something
/// else.
inline void link_live_setup(SetupState& s, std::string path) {
    s.active_link = SetupLink{std::move(path), s.active};
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

/// An ordinary Skin's ears: whatever Workshop published, kept as values. It hears the conditions
/// too, which is this rig's business and not a Skin's: the publication is `to_any` and a load
/// plan names who presents it, so a host-side case can only ask "did Workshop say anything, and
/// what" by having somebody on the bus listening. Every utterance is kept in order, because the
/// claim under test is as much about SILENCE as about content.
class Painter : public loom::WeaveBase<Painter, SeenState,
                                       loom::Accept<surface::SurfaceCanvas, surface::SurfaceText,
                                                    StandingConditions, TranscriptShown,
                                                    PaneSubjectShown>,
                                       loom::Emit<>> {
public:
    Painter(std::vector<surface::SurfaceCanvas>& canvases,
            std::vector<surface::SurfaceText>& notes,
            std::vector<StandingConditions>& conditions,
            std::vector<TranscriptShown>& transcripts,
            std::vector<PaneSubjectShown>& subjects)
        : canvases_(&canvases), notes_(&notes), conditions_(&conditions),
          transcripts_(&transcripts), subjects_(&subjects) {}
    void on(const surface::SurfaceCanvas& c, loom::Mail&) {
        ++state_.frames;
        canvases_->push_back(c);
    }
    void on(const surface::SurfaceText& t, loom::Mail&) { notes_->push_back(t); }
    void on(const StandingConditions& c, loom::Mail&) { conditions_->push_back(c); }
    void on(const TranscriptShown& t, loom::Mail&) { transcripts_->push_back(t); }
    void on(const PaneSubjectShown& s, loom::Mail&) { subjects_->push_back(s); }

private:
    std::vector<surface::SurfaceCanvas>* canvases_;
    std::vector<surface::SurfaceText>* notes_;
    std::vector<StandingConditions>* conditions_;
    std::vector<TranscriptShown>* transcripts_;
    std::vector<PaneSubjectShown>* subjects_;
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

    /// The platform's clipboard, as the real media hold it: a readable medium takes every copy the
    /// process says (the SDL skin's SDL_SetClipboardText), an unreadable one lets the offer pass
    /// (the terminal's OSC 52 claims nothing).
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

    /// A remembered placement offered back at restore: recorded the way the real media receive
    /// it, so a case can assert exactly what the desk's memory handed over.
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
    std::vector<surface::SurfacePlacementRemembered> offered; ///< placement offers at restore
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

/// THE MONOTONIC READING A CASE OWNS -- what a rig wires into `HostContext::interaction_now`, so
/// "how long ago was the last press" is a thing a case STATES rather than outruns. It advances on
/// every reading, by default past `kDoubleClickMs`, so every press a rig publishes is an ordinary
/// press: two presses in a case are two deliberate aims. A case that wants a DOUBLE-click says so
/// (`together`), and the pace is the only thing it has to say.
struct InteractionClock {
    std::int64_t now = 0;
    std::int64_t step = kDoubleClickMs + 1;
    std::int64_t read() {
        const std::int64_t at = now;
        now += step;
        return at;
    }
    /// PRESSES FROM HERE ON ARE CLOSE ENOUGH TO BE ONE GESTURE...
    void together() { step = 1; }
    /// ...and far enough apart to be two again.
    void apart() { step = kDoubleClickMs + 1; }
    /// ...or exactly this far apart, for the two cases that pin the boundary itself.
    void spaced(std::int64_t ms) { step = ms; }
};

// ---- The stack's stand-in: a runtime pane every `Live` admits first --------------------------
// An ordinary overlay-stack pane to seat, arrange, occlude, persist and remove: offered under a
// test office and admitted through the seam handler's own `admit_pane_offer`, with no provider
// behind it. `Live` admits it before anything else, so its handle is `kFirstRuntimeKind` by the
// mint's law; a case building its own `Panels` admits it first too (`admit_stock`). `PaneRig`
// does not: the seam suites mint their own handles and count on the order.
namespace stock {
// Nobody holds this office: the room Workshop grants the pane is refused on the tap, and its body
// shows the host's own waiting sentence.
inline constexpr const char* kOffice = "zengine.test.stack";
inline constexpr const char* kPane = "stack";
inline constexpr const char* kName = "Stack";
inline constexpr const char* kSummary = "a stack pane with nobody behind it";
inline constexpr std::int64_t kKind = kFirstRuntimeKind;
} // namespace stock

inline PaneRef stock_ref() { return PaneRef{stock::kOffice, stock::kPane}; }

/// THE SECOND STAND-IN, admitted right after the first under the same office: the other ordinary
/// stack pane a case seats, overlaps, orders and persists.
namespace second {
inline constexpr const char* kPane = "second";
inline constexpr const char* kName = "Second";
inline constexpr const char* kSummary = "a second stack pane with nobody behind it";
inline constexpr std::int64_t kKind = kFirstRuntimeKind + 1;
} // namespace second

inline PaneRef second_ref() { return PaneRef{stock::kOffice, second::kPane}; }

/// ADMIT THE STAND-IN INTO A CATALOG, first -- and prove it took the handle every case
/// spells. Idempotent: a refreshed offer keeps the handle it already had.
inline void admit_stock(Panels& panels) {
    const Admission took = admit_pane_offer(panels.runtime, stock::kOffice,
                                            PaneOffered{stock::kPane, stock::kName, stock::kSummary});
    REQUIRE_MESSAGE(took.written.accepted, took.written.refusal);
    const RuntimePane* row = panels.runtime.find(stock::kOffice, stock::kPane);
    REQUIRE(row != nullptr);
    REQUIRE_MESSAGE(row->kind == stock::kKind,
                    "the stand-in must be the first pane this catalog admits");
}

/// ...AND THE SECOND, right after it, proving it took the handle every case spells.
inline void admit_second(Panels& panels) {
    const Admission took = admit_pane_offer(
        panels.runtime, stock::kOffice, PaneOffered{second::kPane, second::kName, second::kSummary});
    REQUIRE_MESSAGE(took.written.accepted, took.written.refusal);
    const RuntimePane* row = panels.runtime.find(stock::kOffice, second::kPane);
    REQUIRE(row != nullptr);
    REQUIRE_MESSAGE(row->kind == second::kKind,
                    "the second stand-in must be the second pane this catalog admits");
}

class DoorHand;

/// A live Workshop: the real weave on a real bus, driven only by published input messages.
/// Nothing here reaches past the message boundary except to READ the result.
struct Live {
    loom::Switchboard bus;
    InteractionClock clock;
    HostContext host;
    std::vector<surface::SurfaceCanvas> canvases;
    std::vector<surface::SurfaceText> notes;
    /// EVERY `StandingConditions` THIS WORKSHOP HAS SAID, in order -- so a case can ask how
    /// MANY times it spoke and not only what it last said.
    std::vector<StandingConditions> said_conditions;
    /// ...and every `TranscriptShown` this host published, for the same reason.
    std::vector<TranscriptShown> said_transcripts;
    /// ...and every `PaneSubjectShown` -- an inspector's pane subject (WL-INFO-14) -- in order.
    std::vector<PaneSubjectShown> said_subjects;
    WorkshopWeave* w = nullptr;
    loom::WeaveId workshop_id{};
    loom::WeaveId terminal_id{};
    /// THE HAND THAT OPENS AND CLOSES PANES THROUGH THE HOST'S DOORS, mounted when a case first
    /// asks (`door_hand`), so a case that never opens a pane has no extra party on its bus.
    DoorHand* hand = nullptr;
    loom::WeaveId hand_id{};
    /// THE HOST'S WATCH OVER THE QUIT'S DELIVERIES, mounted beside Workshop as `workshop.cpp`
    /// mounts it (WL-SESSION-19). Declared after the bus and the `HostContext` whose book it
    /// writes, so it is removed first.
    std::unique_ptr<QuitDeliveryWatch> quit_watch;

    Live() {
        host.interaction_now = [this] { return clock.read(); };
        // THE HOST'S VERSION ANSWER, as workshop.cpp wires it (the grant is the Emit set's) --
        // with ONE fixture statement: the stack's stand-in (`stock`) is answered as present for
        // a ROOM, so the launch door (which asks exactly that) seats it as it seats any offered
        // pane. Nothing holds its office, so everything actually sent to it is still refused on
        // the tap; the statement is about the one question and nothing else.
        host.holder_accepts = [this](std::string_view role, const loom::Schema& shape) {
            if (role == stock::kOffice && shape.name() == PaneRoom::zen_name) {
                return true;
            }
            return holder_accepts_on(bus, role, shape);
        };
        host.role_holder = [this](std::string_view role) { return bus.role_holder(role); };
        host.destinations = [this] {
            return bus_destinations(bus, host.terminal != nullptr ? host.terminal->id()
                                                                  : loom::WeaveId{});
        };
        auto weave = std::make_unique<WorkshopWeave>(host);
        w = weave.get();
        loom::Grant grant = loom::emit_default_grant(*w);
        loom::allow_poke_answers(grant);
        // IN THE OFFICE, AS THE HOST MOUNTS IT. Workshop's weave holds `zengine.workshop`, because
        // the pane protocol is authored AS that office: mounted anonymously, its asks and room
        // grants would be refused at the bus. The three-argument `register_weave` is the Loom's
        // own way to bind one, which is what `mount_in_office` spells in the host.
        const loom::WeaveId id =
            bus.register_weave(std::move(weave), std::move(grant), std::string(kWorkshopProvider));
        w->zen_set_self(id);
        workshop_id = id;
        quit_watch = std::make_unique<QuitDeliveryWatch>(bus, id, host.undelivered_quits);
        (void)loom::mount<Painter>(bus, canvases, notes, said_conditions, said_transcripts,
                                   said_subjects);
        // THE STACK'S STAND-IN, FIRST (see `stock` above) -- through the same door the seam
        // spends, before any offer a case might make, so its handle is the constant.
        admit_stock(const_cast<Session&>(w->session()).panels);
        admit_second(const_cast<Session&>(w->session()).panels);
    }

    /// MOUNT THE PARTICIPANT THE WAY THE HOST DOES -- on this bus, beside Workshop's own weave,
    /// the pointer handed over through the HostContext `request_stop` travels through. Its one
    /// rule is the host's, SurfaceText to whoever holds `zengine.skin` at delivery, where
    /// Workshop's is `to_any`: the two differ by rule, not vocabulary. `widen` is the canary lever
    /// -- Workshop's wider rule -- without which "a pane's publication reaches nobody" measures
    /// nothing. `shapes` mounts extra declared shapes, so a completion list can outgrow its room.
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

    /// Type a whole line into whatever is taking text, then press Return.
    void type_line(const std::string& line) {
        for (const char c : line) {
            text(std::string(1, c));
        }
        key(input::scan::kReturn);
    }

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

    /// A PRESS AT AN EXACT POSITION IN THE MEDIUM'S OWN NUMBERS -- a window pixel or a terminal
    /// cell, untranslated. Every other helper here speaks the WORKSPACE cells a maker thinks in for
    /// the document; the Terminal's interior is finer than a cell, so its cases must say a pixel.
    void press_at(std::int64_t x, std::int64_t y, std::int64_t space,
                  std::int64_t mods = input::mod::kNone) {
        publish(loom::to_value(input::PointerButton{1, true, x, y, space, mods}));
    }
    /// A MOTION at a raw position -- the one gesture hover reveal is about.
    void motion_at(std::int64_t x, std::int64_t y, std::int64_t space) {
        publish(loom::to_value(input::PointerMoved{x, y, 0, 0, space, input::mod::kNone}));
    }

    void press(std::int64_t wx, std::int64_t wy, std::int64_t mods = input::mod::kNone) {
        publish(loom::to_value(input::PointerButton{1, true, term_x(wx), term_y(wy),
                                                    input::space::kCells, mods}));
    }
    void release(std::int64_t wx, std::int64_t wy) {
        publish(loom::to_value(input::PointerButton{1, false, term_x(wx), term_y(wy),
                                                    input::space::kCells, input::mod::kNone}));
    }
    /// A SECOND-BUTTON press at a workspace cell -- the same translation `press` uses, for the
    /// button that asks a question instead of taking hold.
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
    /// The contextual-action surface's state, as every contextual-menu case reads it.
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

    const Session& session() const { return w->session(); }
    std::string notice() const { return w->session().notice; }

    /// THE FRESHEST TEXT ON ONE SLOT -- asked BY SLOT, because a repaint publishes two (`status`
    /// and `score`) and `notes.back()` means "whichever this repaint said last", a fact about
    /// publication order rather than about the screen. The medium keeps them apart by name; a
    /// case must too.
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
};

/// A long value that cannot fit an Inspector row at any extent this composition has.
inline const std::string kLongValue = "the quick brown fox jumps over the lazy dog";

/// THIS PROCESS, AS A NAME: the one thing two processes of one suite running at once are
/// guaranteed not to share.
inline std::string this_process_id() {
#if defined(_WIN32)
    return std::to_string(::GetCurrentProcessId());
#else
    return std::to_string(static_cast<long long>(::getpid()));
#endif
}

/// THE TEMPORARY ROOT THIS SUITE OWNS IN THIS PROCESS, and nothing else does (VM-POP-14): the
/// suites run at once, each `TempDir` counter starts at zero, and `TempDir` removes what it finds
/// before it creates, so a shared name would be one case deleting another's files. The suite is
/// `ZENGINE_WORKSHOP_SUITE`, the entry name CMake gave this binary, and the process id is the
/// rest, so two runs of one suite cannot collide either. A process sweeps only the root it made:
/// a crashed run's root stays, since a global sweep would be the deleting this name prevents.
inline std::filesystem::path workshop_temp_root() {
    static const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("zengine-workshop-" ZENGINE_WORKSHOP_SUITE "-" + this_process_id());
    return root;
}

/// DOES THIS ENTRY LEAVE THE TREE? The sweep's own predicate: the host's reparse attribute on
/// Windows, `symlink_status` elsewhere. A failure marks the row, which is what makes a dangling
/// junction removable BY NAME.
inline bool sweep_leaves_the_tree(const std::filesystem::directory_entry& entry) {
#if defined(_WIN32)
    const DWORD attributes = ::GetFileAttributesW(entry.path().c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return true;
    }
    return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    std::error_code link_ec;
    const std::filesystem::file_status own = entry.symlink_status(link_ec);
    return link_ec ? true : !std::filesystem::is_directory(own);
#endif
}

/// REMOVE `p` AND EVERYTHING BENEATH IT, entering only what is really a directory (VM-POP-15):
/// libstdc++ on Windows walks a directory junction as one, so `std::filesystem::remove_all` would
/// empty a directory outside the tree through a junction. An entry that leaves the tree is
/// removed BY NAME and never entered -- `std::filesystem::remove` takes a junction, live or
/// dangling, and unlinks a symbolic link. Failures are swallowed: a destructor cannot report.
inline void remove_tree(const std::filesystem::path& p) {
    std::error_code ec;
    const std::filesystem::directory_entry entry(p, ec);
    bool enter = !sweep_leaves_the_tree(entry);
    if (enter) {
        std::error_code kind_ec;
        enter = entry.is_directory(kind_ec) && !kind_ec;
    }
    if (enter) {
        std::error_code walk_ec;
        const std::filesystem::directory_iterator done;
        for (std::filesystem::directory_iterator it(p, walk_ec); !walk_ec && it != done;
             it.increment(walk_ec)) {
            remove_tree(it->path());
        }
    }
    std::filesystem::remove(p, ec);
}

/// A directory of this run's own, removed when the case ends. Tests never write
/// into the source tree and never share a path with each other.
class TempDir {
public:
    explicit TempDir(const char* tag) {
        static int counter = 0;
        path_ = workshop_temp_root() / (std::string(tag) + "-" + std::to_string(++counter));
        // Whatever is already there is an earlier life of this process id's -- a run that
        // crashed, whose id the system handed out again -- and it goes the way of everything
        // this fixture removes: by the sweep that never enters a link.
        remove_tree(path_);
        std::filesystem::create_directories(path_);
    }
    ~TempDir() {
        remove_tree(path_);
        // ...and this process's root once the last case in it has gone. `remove` takes an
        // EMPTY directory only, so this is the whole cleanup: it succeeds for whoever
        // happens to be last and does nothing, quietly, for everyone before them.
        std::error_code ec;
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

/// A RETIRED OBJECT DOCUMENT, BYTE FOR BYTE -- what the object document's own writer
/// (`persist::save_file`) wrote for the boot document, two objects called `panel`, before the
/// prototype canvas retired. Produced by that commit's code and kept here as a maker's old
/// `workshop.json` is kept on their disk: every reader this host has must refuse it as what it
/// is not, and no door reads it as what it was (WL-DOC-22).
inline constexpr const char* kRetiredObjectDocument =
    R"({"zen":1,"schema":"WorkshopDocument","version":2,"content_id":"0xbfde7d02abe39177","fields":{"format":"zengine-workshop","format_version":"1","next_id":"3","objects":[{"id":"1","name":"panel","context":"0","x":"3","y":"2","width":{"mode":"percent","amount":"60"},"height":{"mode":"cells","amount":"6"}},{"id":"2","name":"panel","context":"0","x":"6","y":"10","width":{"mode":"cells","amount":"14"},"height":{"mode":"cells","amount":"4"}}]}})";

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
    /// TWO SHAPES ON ONE ASK, exactly as the real tool answers: what can be built here, and where
    /// the one it last built stands. The catalog first, because a panel that heard a status about
    /// a recipe it had never been told existed would be showing a choice nobody offered it.
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

/// EVERYTHING AT A PANEL'S BOUNDS, top to bottom -- one question about a rectangle, for every
/// kind. Read through `cell_text_of`, not `c.labels`, because a panel's rows can be a bounded
/// region's; and ONE ROW PER ROW, the one on top: a canvas carries a plane per presentation, and
/// a pane seated in the stack's first slot can have another authored over it, so concatenating
/// every text at those cells would describe a picture nobody paints. `cell_text_of` walks the
/// Skin's own order, so the LAST text at a row is what a maker reads there.
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

/// THE CELLS INSIDE A SURFACE'S OWN CHROME -- where every row a pane, panel or overlay draws
/// lands: `pane_interior` at the cell grain, so "what does this pane SAY" and the painter that
/// said it are one rectangle. A case about the pane's PLACE (occupancy, a press on its edge,
/// coverage) wants the outer rectangle `bounds_of` answers. It asks the character medium's
/// question, in canvas CELLS, subtracting the chrome a cell-projected interior leaves
/// (`kChromeSubs`); a case about a FACE's own boundary takes the overload below with its screen.
inline ui::Rect pane_body_cells(const FineRect& outer) {
    return cells_covered(pane_interior(outer, kChromeSubs));
}
inline ui::Rect pane_body_cells(const ui::Rect& outer) {
    return pane_body_cells(fine_of_cells(outer));
}
inline ui::Rect pane_body_cells(const FineRect& outer, const Screen& sc) {
    return cells_covered(pane_interior(outer, sc));
}

/// What the overlay stack's first slot is showing, whatever is in it. The stack is anchored to
/// the canvas's top-left and its ROWS are the same on every screen -- only its width follows the
/// room -- and `panel_text` reads one column and a run of rows, so the minimum screen's rectangle
/// names the right rows on any of them. It reads the slot's INTERIOR, where the rows are.
inline std::string stack_text(const surface::SurfaceCanvas& c) {
    return panel_text(c, pane_body_cells(placement_bounds(placement::kOverlayStack, 0,
                                                          kMinScreen)));
}

/// Where a kind sits in the combined population, so a case names a KIND rather than a row
/// number that a later catalog entry would silently invalidate. The population is the
/// combined one -- built-ins, then the maker's pane, then the admitted runtime panes --
/// because that is the one list every consumer walks (WL-CAT-05).
inline std::size_t catalog_at(const Panels& panels, std::int64_t kind) {
    const std::vector<CatalogRow> rows = combined_catalog(panels);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].kind == kind) {
            return i;
        }
    }
    return rows.size(); // walked off the end: the case that used it will fail loudly
}

/// OPEN A CLOSED KIND, OR CLOSE AN OPEN ONE -- through the host's launch and close doors, the two
/// the Pane Manager spends (defined below, once the hand that asks exists), for the cases that
/// only need a pane on or off the desk.
inline void pick(Live& t, std::int64_t kind);

/// Open the stack's stand-in pane the way a maker does (see `stock`).
inline void open_stock_pane(Live& t);

/// TAKE THE KEYS BACK FROM WHATEVER PANE HOLDS THEM, without a gesture. A runtime pane holds the
/// keyboard from the press that pointed at it until a press elsewhere (WL-FOCUS-01), so a case
/// that pressed into the stand-in and then wants COMMAND letters answered would otherwise be
/// typing into a pane nobody is behind.
inline void release_keys(Live& t) {
    const_cast<Session&>(t.session()).panels.keyboard = kNoPaneKind;
}

/// THE STAND-IN IS IN THE STACK'S FIRST SLOT, asked through the placement path and read off
/// the canvas at the answer -- so the case cannot agree with the screen by both of them
/// holding the same constant. What the slot shows is the host's own header for a runtime
/// pane, which names the pane.
inline bool first_slot_shows_stock(Live& t) {
    const Screen sc = screen_of(t.session());
    const PanelBounds at = bounds_of(t.session().panels, t.session().setup.active, stock::kKind, sc);
    const ui::Rect cells = pane_body_cells(at.rect);
    return at.open && at.rect == fine_of_cells(placement_bounds(placement::kOverlayStack, 0, sc)) &&
           label_at(t.canvases.back(), cells.x, cells.y).find(stock::kName) != std::string::npos;
}

/// EVERYTHING A PANEL IS SHOWING, top to bottom, ASKED BY KIND: the placement path says where the
/// panel IS, as it told the painter, so no case can agree with the screen by both of them holding
/// the same constant.
inline std::string panel_shown(const surface::SurfaceCanvas& c, const Session& s,
                               std::int64_t kind) {
    const PanelBounds at = bounds_of(s.panels, s.setup.active, kind, screen_of(s));
    if (!at.open) {
        return {}; // a closed panel says nothing, which is the answer a case wants
    }
    return panel_text(c, pane_body_cells(at.rect, screen_of(s)));
}

/// The reference a built-in kind is spelled with, as a case says it -- through
/// the catalog, never as a literal, so a case cannot agree with a typo.
inline PaneRef ref_of(std::int64_t kind) {
    if (kind == stock::kKind) {
        return stock_ref(); // the stand-in has no catalog row to derive one from
    }
    if (kind == second::kKind) {
        return second_ref(); // ...and neither has the second
    }
    return pane_ref_of(kind);
}

/// A reference to a pane no build of this Workshop has ever had. The
/// third-party entry every unresolved case is built on.
inline PaneRef stranger() { return PaneRef{"third.party.tools", "history"}; }

/// NOBODY HAS OFFERED ANYTHING -- said out loud, because the resolution door takes the whole
/// `Panels` rather than something a caller could forget. A default-constructed one has an empty
/// runtime catalog and a closed maker pane, so a case passing it asks what the BUILT-IN half
/// answers with no provider in the process.
inline const Panels& no_providers() {
    static const Panels empty;
    return empty;
}

/// The room the minimum composition actually has: one overlay slot, resolved
/// through `placement_bounds` rather than written down here (screen.hpp says
/// why it is one). Every case below reconciles at most one stacked panel, so
/// this is the capacity they were all written under.
inline StackCapacity min_room() { return stack_capacity(kMinScreen); }

/// A ROOM WITH TWO STACK SLOTS, for a case naming two overlay panes: the one built-in is the top
/// band's, so two overlays are two stand-ins, and the case has to say which screen it is on.
inline StackCapacity two_slot_room() { return stack_capacity(screen_of(120, 44)); }

/// A setup, spelled the way a case reads: a name and the kinds it means.
inline Setup setup_of(const std::string& name, const std::vector<std::int64_t>& kinds) {
    Setup s;
    s.name = name;
    for (const std::int64_t k : kinds) {
        // THROUGH THE DOOR, so every candidate a case builds carries the identity permutation
        // `add_pane` assigns. A `push_back` would build setups whose ranks are all zero, valid for
        // one row and refused by `check_setup` for two -- measuring the fixture, not the law.
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

/// The keys the shipped catalog binds to the four layout gestures, read from the keymap rather
/// than spelled here: a case that hard-coded `.` would keep passing after a remap, measuring its
/// own literal. Cases about the layout run's GEOMETRY belong to the screen suite and those about
/// its gestures to the panels -- two suites, one reading of the keymap.
struct LayoutKeys {
    Gesture next;
    Gesture previous;
    Gesture make;
    Gesture drop;
};

inline LayoutKeys layout_keys(const Live& t) {
    return LayoutKeys{t.session().keymap.gesture_of(Act::kLayoutNext),
                      t.session().keymap.gesture_of(Act::kLayoutPrevious),
                      t.session().keymap.gesture_of(Act::kLayoutNew),
                      t.session().keymap.gesture_of(Act::kLayoutRemove)};
}

inline void press_gesture(Live& t, const Gesture& g) { t.key(g.scancode, g.modifiers); }

/// WHERE A PAINTED TAB'S FIRST CELL IS, out of the SAME composition the painter wrote -- never a
/// column a case computed.
inline std::int64_t tab_column(Live& t, std::size_t at) {
    const BandStatus band = band_status(t.session(), screen_of(t.session()));
    for (const LayoutTab& tab : band.tabs) {
        if (tab.at == at) {
            return tab.column;
        }
    }
    return -1;
}

/// PRESS A PAINTED LAYOUT TAB. The band is the canvas's first row.
inline void press_tab(Live& t, std::size_t at) {
    const std::int64_t column = tab_column(t, at);
    REQUIRE(column >= 0);
    t.press_canvas(column, 0);
    t.release_canvas(column, 0);
}

/// ...AND ASK IT WHAT CAN BE DONE WITH IT.
inline void right_press_tab(Live& t, std::size_t at) {
    const std::int64_t column = tab_column(t, at);
    REQUIRE(column >= 0);
    t.right_press_canvas(column, 0);
}

/// CHOOSE A CONTEXTUAL ROW BY ITS ACTION ID, driving the open surface with its own keys --
/// so a case names the operation it means and never a cursor index. A row inside a
/// presentation group is reached by descending into that group, exactly as a maker reaches
/// it, and which group that is comes from the declaration table rather than from a guess.
inline bool choose_context_action(Live& t, const char* id) {
    std::string group;
    bool declared = false;
    for (const ContextRow& row : kContextCatalog) {
        if (std::string(row.action) == id &&
            (row.subjects & context_bit(t.menu().subject)) != 0) {
            group = row.group;
            declared = true;
            break;
        }
    }
    if (!declared) {
        return false;
    }
    const auto step_to = [&t](std::size_t at) {
        for (int guard = 0; guard < 32 && t.menu().cursor < at; ++guard) {
            t.key(input::scan::kDown);
        }
        for (int guard = 0; guard < 32 && t.menu().cursor > at; ++guard) {
            t.key(input::scan::kUp);
        }
        return t.menu().cursor == at;
    };
    if (!group.empty()) {
        const std::vector<ContextEntry> top = context_population(t.menu().subject, "");
        bool entered = false;
        for (std::size_t at = 0; at < top.size(); ++at) {
            if (top[at].is_group && group == top[at].group) {
                if (!step_to(at)) {
                    return false;
                }
                t.key(input::scan::kReturn);
                entered = t.menu().group == group;
                break;
            }
        }
        if (!entered) {
            return false;
        }
    }
    const std::vector<ContextEntry> rows =
        context_population(t.menu().subject, t.menu().group);
    for (std::size_t at = 0; at < rows.size(); ++at) {
        if (!rows[at].is_group && rows[at].row != nullptr &&
            std::string(rows[at].row->id) == id) {
            if (!step_to(at)) {
                return false;
            }
            t.key(input::scan::kReturn);
            return true;
        }
    }
    return false;
}

/// OPEN THE RENAME EDITOR ON A TAB THE WAY A MAKER DOES: two presses, close enough together to be
/// one gesture.
inline void open_rename_on_tab(Live& t, std::size_t at) {
    // THE PACE IS SET BEFORE THE FIRST PRESS, because `InteractionClock::read` hands out
    // the current instant and THEN advances by the step -- so a `together()` between the
    // two presses would set the pace for the gesture after this one.
    t.clock.together();
    press_tab(t, at);
    press_tab(t, at);
    t.clock.apart();
}

/// THE SAME GESTURE ON THE LIVE LAYOUT'S TAB, FOR ANY RIG that publishes raw input --
/// written against `publish` rather than against a workspace translation, because the pane
/// rig has no document room to speak in.
template <typename Rig>
inline void open_rename_on_live_tab(Rig& r) {
    const BandStatus band = band_status(r.session(), screen_of(r.session()));
    std::int64_t column = -1;
    for (const LayoutTab& tab : band.tabs) {
        if (tab.at == r.session().setup.active_at) {
            column = tab.column;
        }
    }
    REQUIRE(column >= 0);
    r.clock.together();
    for (int press = 0; press < 2; ++press) {
        r.publish(loom::to_value(input::PointerButton{1, true, column,
                                                      surface::kTuiCanvasTopRow,
                                                      input::space::kCells, input::mod::kNone}));
        r.publish(loom::to_value(input::PointerButton{1, false, column,
                                                      surface::kTuiCanvasTopRow,
                                                      input::space::kCells, input::mod::kNone}));
    }
    r.clock.apart();
}

/// DUPLICATE THE LIVE LAYOUT THROUGH THE CONTEXTUAL MENU ON ITS OWN TAB -- the shipped route, for
/// the rigs whose cases mean "two layouts holding the same desk".
template <typename Rig>
inline void duplicate_live_layout(Rig& r) {
    const BandStatus band = band_status(r.session(), screen_of(r.session()));
    std::int64_t column = -1;
    for (const LayoutTab& tab : band.tabs) {
        if (tab.at == r.session().setup.active_at) {
            column = tab.column;
        }
    }
    REQUIRE(column >= 0);
    r.publish(loom::to_value(input::PointerButton{3, true, column, surface::kTuiCanvasTopRow,
                                                  input::space::kCells, input::mod::kNone}));
    REQUIRE(r.session().context.open);
    REQUIRE(r.session().context.subject == context_subject::kLayout);
    for (int guard = 0; guard < 32 && r.session().context.open; ++guard) {
        const std::vector<ContextEntry> rows =
            context_population(r.session().context.subject, r.session().context.group);
        REQUIRE(r.session().context.cursor < rows.size());
        const ContextEntry& here = rows[r.session().context.cursor];
        if (!here.is_group && here.row != nullptr &&
            std::string(here.row->id) == "layout.duplicate") {
            r.key(input::scan::kReturn);
            return;
        }
        r.key(input::scan::kDown);
    }
    REQUIRE_FALSE(r.session().context.open);
}

/// TYPE A WHOLE NAME INTO THE OPEN EDITOR AND COMMIT IT.
inline void type_name(Live& t, const std::string& name) {
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

/// RENAME THE LIVE LAYOUT, THROUGH THE GESTURE A MAKER USES -- a double-click on its own tab, then
/// the name. NO FILE IS WRITTEN by any part of this.
inline void rename_live_layout(Live& t, const std::string& name) {
    open_rename_on_tab(t, t.session().setup.active_at);
    type_name(t, name);
}

/// SAVE THE LIVE LAYOUT'S DESK TO ITS SETUP ARTIFACT -- `s`, and only that.
inline void save_setup(Live& t) { t.key(input::scan::kS); }

/// NAME THE LIVE LAYOUT AND WRITE IT TO THE SETUP FILE -- two gestures, because renaming is a
/// layout operation and saving a file operation: a case that means "a named desk that matches its
/// file" says both, and one that means only one of them says only that one.
inline void name_setup(Live& t, const std::string& name) {
    rename_live_layout(t, name);
    save_setup(t);
}

/// The identity row as a maker reads it, off the canvas at the place the painter put it -- never
/// rebuilt here, so a case cannot pass while the screen says something else. That place is the
/// Layouts pane's first interior row, which at the developer default is Workshop's first row.
inline std::string setup_row(const surface::SurfaceCanvas& c, const Screen& sc) {
    (void)sc;
    return label_at(c, 0, 0);
}

/// The workspace-extent fact as a maker reads it -- the Layouts pane's SECOND row where the medium
/// fits one, folded into the identity row where it does not. It takes the SESSION, because how
/// many rows that surface has is a fact about a pane a maker can resize, not about the screen.
inline std::string workspace_row(const surface::SurfaceCanvas& c, const Session& s,
                                 const Screen& sc) {
    return layouts_body(s, sc).rows >= 2 ? inspector_row(c, 0, 1) : inspector_row(c, 0, 0);
}

/// THE LAYOUTS PANE'S PUBLISHED REGION on a canvas, found the way the painter published it --
/// `layouts_body`'s own answer, so a case cannot pass while reading a region the painter did not
/// write. Not the top band's whole rectangle: the pane's interior is inset by one device unit on
/// a face that can draw one.
inline const surface::SurfaceTextRegion* layouts_region_on(const surface::SurfaceCanvas& c,
                                                           const Session& s,
                                                           const Screen& sc) {
    const ExternalBodyPlace place = layouts_body(s, sc);
    for (const surface::SurfaceLayer& layer : c.layers) {
        for (const surface::SurfaceTextRegion& r : layer.texts) {
            if (r.x == place.region_x && r.y == place.region_y &&
                r.sub_x == place.region_sub_x && r.sub_y == place.region_sub_y &&
                r.h == place.region_h && r.sub_h == place.region_sub_h) {
                return &r;
            }
        }
    }
    return nullptr;
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

/// A TOOL THAT ASKS THIS HOST'S DOORS, in an office of its own: what the Files weave is, reduced
/// to the sentences a case needs. It asks `zengine.project` where this run began, asks
/// `zengine.recipes` to install or author a catalog, and asks `zengine.workshop` to open one source
/// in the Editor, recording every answer -- the party each door was built for. It authors AS AN
/// OFFICE, because all three doors refuse anonymous speech; a rig that asked personally would be
/// proving the refusal rather than the answer.
struct DoorAskerState {
    std::int64_t asks = 0;
    std::int64_t answers = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(DoorAskerState, 1, ZEN_FIELD(asks), ZEN_FIELD(answers));
};

class DoorAsker
    : public loom::WeaveBase<DoorAsker, DoorAskerState,
                             loom::Accept<ProjectRoot, ProjectFrontierSaid, PlanNames,
                                          RecipeOutcome, PlanRowWritten, SourceOpened,
                                          RecipeSourceSaid, SeatDo>,
                             loom::Emit<ProjectRootRequested, ProjectFrontierRequested,
                                        PlanNamesRequested, RecipeUseRequested,
                                        RecipeAuthorRequested, PlanRowRequested,
                                        OpenSourceRequested, RecipeSourceRequested,
                                        PaneRevealRequested, PaneQuitAnswered,
                                        surface::SurfaceExtent,
                                        // ...and the four
                                        // sentences a stranger might forge at the managed
                                        // opening's three parties, so the forgery cases prove
                                        // the parties' refusals and not the bus's.
                                        ManagedOpenSettled, SourcePrepared, PresentationAdmitted,
                                        loom::DispatchRefused>> {
public:
    explicit DoorAsker(std::string office) : office_(std::move(office)) {}

    /// What this asker should say next, run INSIDE its own delivery so `as_role` has a real
    /// authorship moment for Loom to verify.
    std::function<void(DoorAsker&, loom::Mail&)> next;

    /// The answers, as they arrived.
    std::vector<ProjectRoot> roots;
    std::vector<ProjectFrontierSaid> frontiers;
    std::vector<PlanNames> names;
    std::vector<RecipeOutcome> outcomes;
    std::vector<PlanRowWritten> rows;
    std::vector<SourceOpened> opens;
    std::vector<bool> opens_authentic; ///< beside `opens`: Loom's word that it answered our ask
    std::vector<RecipeSourceSaid> sources;
    /// Whether the last ask was authored as an office at all -- the canary lever for the
    /// rule every door keeps.
    bool personally = false;
    /// This asker's own id on the bus, so a case can nudge it the way `drive` nudges a seat.
    loom::WeaveId id{};

    void on(const SeatDo&, loom::Mail& mail) {
        if (next) {
            auto what = next;
            next = nullptr;
            what(*this, mail);
        }
    }
    /// ONE MORE THING TO SAY, ON THE BEAT THE PROJECT DOOR ANSWERS: a statement TWO deliveries
    /// behind an ask made in the same handler. The bus is FIFO, so a case that queues an ask and a
    /// nudge together cannot land the nudge inside the ask's own conversation, and this is the
    /// shortest honest way to reach that instant with real messages (VM-FIX-24, one hop further).
    std::function<void(DoorAsker&, loom::Mail&)> then_root;

    void on(const ProjectRoot& said, loom::Mail& mail) {
        ++state_.answers;
        roots.push_back(said);
        if (then_root) {
            auto what = then_root;
            then_root = nullptr;
            what(*this, mail);
        }
    }
    void on(const ProjectFrontierSaid& said, loom::Mail&) {
        ++state_.answers;
        frontiers.push_back(said);
    }
    void on(const PlanNames& said, loom::Mail&) {
        ++state_.answers;
        names.push_back(said);
    }
    void on(const RecipeOutcome& said, loom::Mail&) {
        ++state_.answers;
        outcomes.push_back(said);
    }
    void on(const PlanRowWritten& said, loom::Mail&) {
        ++state_.answers;
        rows.push_back(said);
    }
    void on(const SourceOpened& said, loom::Mail& mail) {
        ++state_.answers;
        opens.push_back(said);
        // WHETHER LOOM SAYS THIS ANSWERS AN ASK OF THIS WEAVE'S -- the provenance a relayed
        // answer must still carry for the ORIGINAL requester (WL-OPEN-07): the Editor spends the requester's own kept right, so this is true.
        opens_authentic.push_back(mail.answers_ask());
    }
    void on(const RecipeSourceSaid& said, loom::Mail&) {
        ++state_.answers;
        sources.push_back(said);
    }

    template <class Shape>
    void ask(loom::Mail& mail, const char* office, const Shape& shape) {
        ++state_.asks;
        if (personally) {
            (void)mail.send_to_role(office, shape);
            return;
        }
        (void)mail.as_role(office_).send_to_role(office, shape);
    }

private:
    std::string office_;
};

/// The office an asker holds unless a case says otherwise. It is a TEST office and nothing
/// in the host names it: what the doors require is that an asker holds SOME office, so the
/// spelling is the case's to choose and the door's answer must not depend on it.
inline constexpr const char* kDoorAskerOffice = "zengine.test.door-asker";

/// Seat an asker on this bus, granted exactly the eight asks and nothing else. It hears the
/// three answers the way any weave does -- through Loom's own answer route -- so nothing
/// here has to be granted for it to be told.
inline DoorAsker* mount_door_asker(Live& t, const char* office = kDoorAskerOffice) {
    auto asker = std::make_unique<DoorAsker>(std::string(office));
    DoorAsker* raw = asker.get();
    loom::Grant grant;
    grant.allow_to_any(ProjectRootRequested::zen_name, ProjectRootRequested::zen_version);
    grant.allow_to_any(RecipeUseRequested::zen_name, RecipeUseRequested::zen_version);
    grant.allow_to_any(RecipeAuthorRequested::zen_name, RecipeAuthorRequested::zen_version);
    grant.allow_to_any(OpenSourceRequested::zen_name, OpenSourceRequested::zen_version);
    grant.allow_to_any(ProjectFrontierRequested::zen_name,
                       ProjectFrontierRequested::zen_version);
    grant.allow_to_any(PlanNamesRequested::zen_name, PlanNamesRequested::zen_version);
    grant.allow_to_any(PlanRowRequested::zen_name, PlanRowRequested::zen_version);
    grant.allow_to_any(RecipeSourceRequested::zen_name, RecipeSourceRequested::zen_version);
    const loom::WeaveId id =
        t.bus.register_weave(std::move(asker), std::move(grant), std::string(office));
    raw->zen_set_self(id);
    raw->id = id;
    return raw;
}

/// Make an asker say one sentence INSIDE ITS OWN DELIVERY -- `drive`'s reason exactly, and
/// the only way `mail.as_role(...)` has an authorship moment Loom can verify.
inline void asker_do(Live& t, DoorAsker* asker,
                     std::function<void(DoorAsker&, loom::Mail&)> what) {
    asker->next = std::move(what);
    (void)t.bus.send(asker->id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                             loom::WeaveId{}, 0));
    t.bus.drain_until_idle();
}

/// ASK THE EDITOR'S DOOR TO OPEN ONE PATH, and hand back what it answered: the whole of what a
/// Return on a source row in the Files pane crosses as (WL-EDIT-05's asker half). The door is at
/// `zengine.editor`, so a rig that holds no Editor weave gets no answer -- the ask is refused on
/// the tap, and this helper fails loudly rather than inventing one; a case about the document
/// goes to `test_workshop_panes_editor.cpp`, over the real image.
inline SourceOpened open_through_door(Live& t, DoorAsker* asker, const std::string& path) {
    const std::size_t before = asker->opens.size();
    asker_do(t, asker, [path](DoorAsker& a, loom::Mail& mail) {
        a.ask(mail, kEditorRole, OpenSourceRequested{path});
    });
    REQUIRE(asker->opens.size() == before + 1);
    return asker->opens.back();
}

/// ASK THE PROJECT DOOR WHICH FILE A RECIPE NAMES, and hand back what it answered. This is
/// the first of the two doors the Builder pane's `builder.edit-source` walks: the pane holds a
/// recipe name and never a path, the host's read-only office resolves it against the catalog
/// IT owns (`RecipeSourceSaid`), and the pane then asks the Editor's own door to open it.
inline RecipeSourceSaid edit_source_through_door(Live& t, DoorAsker* asker,
                                                 const std::string& recipe) {
    const std::size_t before = asker->sources.size();
    asker_do(t, asker, [recipe](DoorAsker& a, loom::Mail& mail) {
        a.ask(mail, kProjectRole, RecipeSourceRequested{recipe});
    });
    REQUIRE(asker->sources.size() == before + 1);
    return asker->sources.back();
}

struct SeatState {
    std::int64_t said = 0;
    ZEN_SHAPE(SeatState, 1, ZEN_FIELD(said));
};

/// A PARTY THAT PUTS A PANE ON THE DESK, OR TAKES ONE OFF, THROUGH THE HOST'S TWO DOORS
/// (`PaneLaunchRequested`, `PaneCloseRequested`) -- what the desktop's Pane Manager does, for the
/// many cases that need a pane open or closed and are not about who asked or with which key. It
/// holds an office of its own, because the doors answer an office and only an office; the cases
/// about the Pane Manager itself drive the shipped desktop image.
class DoorHand
    : public loom::WeaveBase<DoorHand, SeatState,
                             loom::Accept<PaneLaunchAnswered, PaneCloseAnswered, PaneSubjectActed,
                                          MakerPaneAnswered, SeatDo>,
                             loom::Emit<PaneLaunchRequested, PaneCloseRequested,
                                        InspectPaneRequested, PaneCommitRequested,
                                        MakerPaneRequested>> {
public:
    void on(const PaneLaunchAnswered& a, loom::Mail&) { launched.push_back(a); }
    void on(const PaneCloseAnswered& a, loom::Mail&) { closed.push_back(a); }
    /// ...AND, ASKED AS AN INSPECTOR (Info's path), what naming a subject or writing a row came to.
    void on(const PaneSubjectActed& a, loom::Mail&) { acted.push_back(a); }
    /// ...AND, ASKED AS THE CREATOR'S PRESENTER (the desktop Pane Manager's path), what making,
    /// saving or discarding the maker's pane came to.
    void on(const MakerPaneAnswered& a, loom::Mail&) { made.push_back(a); }
    void on(const SeatDo&, loom::Mail& mail) {
        if (next) {
            std::function<void(DoorHand&, loom::Mail&)> once;
            once.swap(next);
            once(*this, mail);
        }
    }
    std::vector<PaneLaunchAnswered> launched;
    std::vector<PaneCloseAnswered> closed;
    std::vector<PaneSubjectActed> acted;
    std::vector<MakerPaneAnswered> made;
    std::function<void(DoorHand&, loom::Mail&)> next;
    static constexpr const char* kOffice = "zengine.test.hand";
};

/// THE RIG'S HAND, mounted on first use in its own office.
template <class Rig>
inline DoorHand& door_hand(Rig& t) {
    if (t.hand == nullptr) {
        auto seat = std::make_unique<DoorHand>();
        DoorHand* raw = seat.get();
        loom::Grant grant;
        grant.allow_to_role(PaneLaunchRequested::zen_name, PaneLaunchRequested::zen_version,
                            kWorkshopProvider);
        grant.allow_to_role(PaneCloseRequested::zen_name, PaneCloseRequested::zen_version,
                            kWorkshopProvider);
        grant.allow_to_role(InspectPaneRequested::zen_name, InspectPaneRequested::zen_version,
                            kWorkshopProvider);
        grant.allow_to_role(PaneCommitRequested::zen_name, PaneCommitRequested::zen_version,
                            kWorkshopProvider);
        grant.allow_to_role(MakerPaneRequested::zen_name, MakerPaneRequested::zen_version,
                            kWorkshopProvider);
        t.hand_id =
            t.bus.register_weave(std::move(seat), std::move(grant), std::string(DoorHand::kOffice));
        raw->zen_set_self(t.hand_id);
        t.hand = raw;
    }
    return *t.hand;
}

/// ASK THE HOST TO OPEN OR FOCUS THIS PANE, as an office, inside the hand's own delivery.
template <class Rig>
inline PaneLaunchAnswered hand_launch(Rig& t, const PaneRef& ref) {
    DoorHand& h = door_hand(t);
    const std::size_t before = h.launched.size();
    h.next = [ref](DoorHand&, loom::Mail& m) {
        (void)m.as_role(DoorHand::kOffice)
            .send_to_role(kWorkshopProvider, PaneLaunchRequested{ref.provider, ref.pane});
    };
    (void)t.bus.send(t.hand_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                              loom::WeaveId{}, 0));
    t.bus.drain_until_idle();
    REQUIRE_MESSAGE(h.launched.size() == before + 1, "the launch door did not answer");
    return h.launched.back();
}

/// ...AND TO TAKE IT OFF THE DESK, UNLOADING NOTHING.
template <class Rig>
inline PaneCloseAnswered hand_close(Rig& t, const PaneRef& ref) {
    DoorHand& h = door_hand(t);
    const std::size_t before = h.closed.size();
    h.next = [ref](DoorHand&, loom::Mail& m) {
        (void)m.as_role(DoorHand::kOffice)
            .send_to_role(kWorkshopProvider, PaneCloseRequested{ref.provider, ref.pane});
    };
    (void)t.bus.send(t.hand_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                              loom::WeaveId{}, 0));
    t.bus.drain_until_idle();
    REQUIRE_MESSAGE(h.closed.size() == before + 1, "the close door did not answer");
    return h.closed.back();
}

/// NAME A PANE AS THE INSPECTED SUBJECT THROUGH THE HOST'S DOOR, as an inspector office asks it
/// (Info's own path, WL-INFO-14), and answer what the door said.
template <class Rig>
inline PaneSubjectActed hand_inspect(Rig& t, const PaneRef& ref) {
    DoorHand& h = door_hand(t);
    const std::size_t before = h.acted.size();
    h.next = [ref](DoorHand&, loom::Mail& m) {
        (void)m.as_role(DoorHand::kOffice)
            .send_to_role(kWorkshopProvider, InspectPaneRequested{ref.provider, ref.pane});
    };
    (void)t.bus.send(t.hand_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                              loom::WeaveId{}, 0));
    t.bus.drain_until_idle();
    REQUIRE_MESSAGE(h.acted.size() == before + 1, "the inspection door did not answer");
    return h.acted.back();
}

/// WHERE THE INSPECTED PANE'S ROW WITH THIS LABEL IS -- the first such after the section named
/// `after` (the start when empty), so a maker pane's region `X` is told from its AUTHORED `X`.
inline std::size_t subject_row_index(const Session& s, const std::string& label,
                                     const std::string& after = std::string()) {
    const std::vector<Row>& rows = s.inspected.rows;
    std::size_t from = 0;
    if (!after.empty()) {
        from = rows.size();
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (rows[i].section() && rows[i].label() == after) {
                from = i + 1;
                break;
            }
        }
    }
    for (std::size_t i = from; i < rows.size(); ++i) {
        if (rows[i].label() == label) {
            return i;
        }
    }
    return rows.size();
}

/// ...ITS ROW, AND ITS VALUE READ NOW.
inline const Row* subject_row(const Session& s, const std::string& label,
                              const std::string& after = std::string()) {
    const std::size_t at = subject_row_index(s, label, after);
    return at < s.inspected.rows.size() ? &s.inspected.rows[at] : nullptr;
}
inline std::string subject_value(const Session& s, const std::string& label,
                                 const std::string& after = std::string()) {
    const Row* row = subject_row(s, label, after);
    REQUIRE_MESSAGE(row != nullptr, "the inspected pane has no row labelled ", label);
    return row->value();
}

/// WRITE THE INSPECTED PANE'S ROW WITH THIS LABEL THROUGH THE COMMIT DOOR -- the subject's name as
/// the host gave it, the row's index and the text, exactly as Info sends a finished draft -- and
/// answer what the door said.
template <class Rig>
inline PaneSubjectActed hand_commit(Rig& t, const std::string& label, const std::string& text,
                                    const std::string& after = std::string()) {
    const std::size_t at = subject_row_index(t.session(), label, after);
    REQUIRE_MESSAGE(at < t.session().inspected.rows.size(),
                    "the inspected pane has no row labelled ", label);
    const std::int64_t subject = t.session().inspected.name;
    DoorHand& h = door_hand(t);
    const std::size_t before = h.acted.size();
    h.next = [subject, at, text](DoorHand&, loom::Mail& m) {
        (void)m.as_role(DoorHand::kOffice)
            .send_to_role(kWorkshopProvider,
                          PaneCommitRequested{subject, static_cast<std::int64_t>(at), text});
    };
    (void)t.bus.send(t.hand_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                              loom::WeaveId{}, 0));
    t.bus.drain_until_idle();
    REQUIRE_MESSAGE(h.acted.size() == before + 1, "the commit door did not answer");
    return h.acted.back();
}

/// ASK THE MAKER DOOR -- make, save or discard the one open definition (WL-MAKER-11) -- as the
/// desktop's Pane Manager asks it, and answer what the door said.
template <class Rig>
inline MakerPaneAnswered hand_maker(Rig& t, std::int64_t act,
                                    const std::string& name = std::string()) {
    DoorHand& h = door_hand(t);
    const std::size_t before = h.made.size();
    h.next = [act, name](DoorHand&, loom::Mail& m) {
        (void)m.as_role(DoorHand::kOffice)
            .send_to_role(kWorkshopProvider, MakerPaneRequested{act, name});
    };
    (void)t.bus.send(t.hand_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                              loom::WeaveId{}, 0));
    t.bus.drain_until_idle();
    REQUIRE_MESSAGE(h.made.size() == before + 1, "the maker door did not answer");
    return h.made.back();
}

/// ...OR QUEUE THE CLOSE WITHOUT DELIVERING ANYTHING -- office-authored as the hand, behind
/// whatever the bus already holds -- for the cases about what a removal in the same poll does to
/// a flight already under way. The next pump delivers it in order.
template <class Rig>
inline void enqueue_close(Rig& t, const PaneRef& ref) {
    (void)door_hand(t);
    // ROLE-ADDRESSED, as the hand's grant is: a send to the weave's id would be refused at
    // delivery, silently to this case.
    const loom::Ticket queued = t.bus.office_send_to_role_as(
        t.hand_id, DoorHand::kOffice, kWorkshopProvider,
        loom::Message(loom::to_value(PaneCloseRequested{ref.provider, ref.pane}), t.hand_id,
                      t.hand_id, 0));
    REQUIRE(queued.valid());
}

/// PUT A PANE ON THE DESK AS SCAFFOLDING: the launch door, then the selection and the keys put
/// back where they were, with a fresh frame. A launch selects and focuses what it opens
/// (WL-DESK-03); the many cases that open a pane only to arrange, persist or paint it do not mean
/// that, and the cases that do drive the desktop's Pane Manager or the door directly. The frame
/// is redrawn by a key nothing answers (`kUnknown`), so a case reads the desk as it now stands.
template <class Rig>
inline PaneLaunchAnswered seat_pane(Rig& t, const PaneRef& ref) {
    Session& s = const_cast<Session&>(t.session());
    const std::int64_t selected = s.panels.selected;
    const std::int64_t keyboard = s.panels.keyboard;
    const PaneLaunchAnswered said = hand_launch(t, ref);
    s.panels.selected = selected;
    s.panels.keyboard = keyboard;
    t.key(input::scan::kUnknown);
    return said;
}

/// ONE TOGGLE OVER THE TWO DOORS: close the pane if the live desk names it, seat it if not.
template <class Rig>
inline void toggle_pane(Rig& t, const PaneRef& ref) {
    if (has_pane(t.session().setup.active, ref)) {
        (void)hand_close(t, ref);
    } else {
        (void)seat_pane(t, ref);
    }
}

inline void pick(Live& t, std::int64_t kind) {
    const std::vector<CatalogRow> rows = combined_catalog(t.session().panels);
    const std::size_t at = catalog_at(t.session().panels, kind);
    REQUIRE(at < rows.size());
    toggle_pane(t, rows[at].ref);
}

inline void open_stock_pane(Live& t) { pick(t, stock::kKind); }

/// A NATIVE PROVIDER SEAT: a weave that holds an office and can be made to say anything,
/// deliberately or personally. Evidence of a different kind from the dynamic fixture, kept apart
/// on purpose: the `.so` proves the real ABI, load path and attested activation; this proves the
/// AUTHORITY cases, which need the exact wrong sentence at the exact wrong moment -- personal
/// speech from the role holder, one office speaking about another's pane, a content message one
/// column too wide -- and a shipped fixture that could be talked into those would not be one.
class ProviderSeat
    : public loom::WeaveBase<ProviderSeat, SeatState,
                             loom::Accept<PaneCatalogRequested, PaneRoom, PanePressed, PaneKey,
                                          PaneTextInput, PaneWheel, PaneActionRequested, SeatDo>,
                             loom::Emit<PaneOffered, PaneContent, PanePressed, PaneActions,
                                        v2::PaneActions, PaneCaret, PaneEscapeUnspent>> {
public:
    explicit ProviderSeat(std::string office) : office_(std::move(office)) {}

    /// A RESOLVED ACTION THIS SEAT WAS ASKED FOR (WL-KEY-15), recorded and never
    /// interpreted, for the key's reason exactly: a case asserts what WORKSHOP resolved.
    void on(const PaneActionRequested& a, loom::Mail& mail) {
        ++state_.said;
        ++said;
        actions.push_back(a);
        action_authors.push_back(std::string(mail.authored_role()));
    }

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
    /// A PRESS THIS SEAT WAS TOLD ABOUT, and its author beside it. The seat interprets nothing --
    /// it is a recorder, so a case can assert exactly what Workshop said and nothing about what a
    /// real provider would make of it.
    void on(const PanePressed& p, loom::Mail& mail) {
        ++state_.said;
        ++said;
        presses.push_back(p);
        press_authors.push_back(std::string(mail.authored_role()));
    }
    /// A KEY AND THE TEXT IT PRODUCED, recorded and never interpreted, for the press's reason
    /// exactly: a case asserts what WORKSHOP said, and a seat that made something of one would be
    /// asserting a provider's opinion instead.
    void on(const PaneKey& k, loom::Mail& mail) {
        ++state_.said;
        ++said;
        keys.push_back(k);
        key_authors.push_back(std::string(mail.authored_role()));
        key_asks.push_back(mail.correlation());
    }
    void on(const PaneTextInput& t, loom::Mail& mail) {
        ++state_.said;
        ++said;
        typed.push_back(t);
        text_authors.push_back(std::string(mail.authored_role()));
    }
    /// A WHEEL THIS SEAT WAS TOLD ABOUT, recorded and never interpreted, for the press's reason
    /// exactly.
    void on(const PaneWheel& w, loom::Mail& mail) {
        ++state_.said;
        ++said;
        wheels.push_back(w);
        wheel_authors.push_back(std::string(mail.authored_role()));
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
    /// THE ESCAPE THIS SEAT WAS SENT WAS UNSPENT HERE -- said as the office, and personally for
    /// the case about authorship rather than about the gesture. Echoed under the number the
    /// LATEST Escape arrived on, which is what a pane answering the key it was just handed
    /// does; `unspent_answering` is for a case that answers an older one on purpose, and
    /// `unspent_anonymously` for one that echoes nothing at all.
    void unspent(loom::Mail& mail, const std::string& pane) {
        unspent_answering(mail, pane, latest_escape_ask());
    }
    void unspent_answering(loom::Mail& mail, const std::string& pane, std::uint64_t answering) {
        (void)mail.as_role(office_).send_to_role(kWorkshopProvider, PaneEscapeUnspent{pane},
                                                 answering);
    }
    void unspent_anonymously(loom::Mail& mail, const std::string& pane) {
        (void)mail.as_role(office_).send_to_role(kWorkshopProvider, PaneEscapeUnspent{pane}, 0);
    }
    void unspent_personally(loom::Mail& mail, const std::string& pane) {
        (void)mail.send_to_role(kWorkshopProvider, PaneEscapeUnspent{pane}, latest_escape_ask());
    }
    /// THE NUMBER THE LAST ESCAPE THIS SEAT WAS SENT CAME UNDER, or zero if it was sent none.
    std::uint64_t latest_escape_ask() const {
        for (std::size_t i = keys.size(); i-- > 0;) {
            if (keys[i].scancode == zengine::input::scan::kEscape && i < key_asks.size()) {
                return key_asks[i];
            }
        }
        return 0;
    }
    void say_personally(loom::Mail& mail, const PaneContent& c) {
        (void)mail.send_to_role(kWorkshopProvider, c);
    }
    /// WHERE THIS SEAT'S CARET IS -- a pane-to-host sentence, said as the office, and personally
    /// for the refusal that is about authorship rather than lattice.
    void caret(loom::Mail& mail, const PaneCaret& c) {
        (void)mail.as_role(office_).send_to_role(kWorkshopProvider, c);
    }
    void caret_personally(loom::Mail& mail, const PaneCaret& c) {
        (void)mail.send_to_role(kWorkshopProvider, c);
    }
    /// Declare a pane's actions, deliberately AS this office -- and personally, from the
    /// very weave that holds it, which Workshop must drop for the offer's reason.
    void declare(loom::Mail& mail, const PaneActions& a) {
        (void)mail.as_role(office_).send_to_role(kWorkshopProvider, a);
    }
    void declare_personally(loom::Mail& mail, const PaneActions& a) {
        (void)mail.send_to_role(kWorkshopProvider, a);
    }
    /// The SECOND published version of the declaration -- the one that can name an action this
    /// pane owns. A provider built before it existed sends the shape above.
    void declare_v2(loom::Mail& mail, const v2::PaneActions& a) {
        (void)mail.as_role(office_).send_to_role(kWorkshopProvider, a);
    }
    /// FORGE A PRESS AT SOMEBODY ELSE'S PANE -- deliberately authored, and deliberately by an
    /// office that is not `zengine.workshop`: the sentence a provider must refuse, a stranger
    /// telling it a maker clicked one of its rows.
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
    /// THE CORRELATION EACH KEY ARRIVED UNDER, beside `keys` index for index. Workshop stamps
    /// one on an Escape and nothing else, and an answer that does not echo it is about no
    /// Escape at all.
    std::vector<std::uint64_t> key_asks;
    std::vector<PaneTextInput> typed;
    std::vector<std::string> text_authors;
    std::vector<PaneWheel> wheels;
    std::vector<std::string> wheel_authors;
    std::vector<PaneActionRequested> actions;
    std::vector<std::string> action_authors;
    std::function<void(ProviderSeat&, loom::Mail&)> next;

private:
    std::string office_;
};


/// A WEAVE THAT HOLDS `zengine.workshop` AND IS NOT WORKSHOP -- the instrument for a PROVIDER's
/// own authorship checks, whose refusals are invisible from Workshop's side: a room the provider
/// declined to believe and one it answered into a pane nobody has open both look like silence
/// there. Holding the office lets this author a real `PaneRoom`, and also send one PERSONALLY --
/// the sentence a weave that merely held the office would produce with `send_to_role`. Two
/// spellings, one holder, opposite outcomes.
class PaneWatcher
    : public loom::WeaveBase<PaneWatcher, SeatState,
                             loom::Accept<PaneOffered, v2::PaneOffered, PaneContent, v3::PaneContent, SeatDo>,
                             loom::Emit<PaneCatalogRequested, PaneRoom, PaneWheel, PanePressed,
                                        v2::PanePressed, v3::PanePressed>> {
public:
    void on(const v2::PaneOffered& o, loom::Mail& mail) {
        on(PaneOffered{o.pane, o.name, o.summary}, mail);
    }
    void on(const PaneOffered& o, loom::Mail& mail) {
        offers.push_back(o);
        offer_authors.push_back(std::string(mail.authored_role()));
    }
    void on(const PaneContent& c, loom::Mail& mail) {
        content.push_back(c);
        pictures.push_back(0);
        content_authors.push_back(std::string(mail.authored_role()));
    }
    /// A PANE THAT NUMBERS ITS PICTURE SAYS THE SAME ROWS AND ONE MORE FACT. The rows go where
    /// v1's do, so every case written against `content` reads a numbered pane unchanged, and
    /// the number is kept beside them for a case that presses with it.
    void on(const v3::PaneContent& c, loom::Mail& mail) {
        content.push_back(PaneContent{c.pane, c.rows});
        pictures.push_back(c.picture);
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
    /// A PRESS FROM THE OFFICE THIS WATCHER HOLDS -- the correctly authored spelling and the
    /// personal one, as `grant`/`grant_personally` are, so a provider's own authorship check can
    /// be measured from the only side it shows on.
    void press(loom::Mail& mail, const char* office, const PanePressed& p) {
        (void)mail.as_role(kWorkshopProvider).send_to_role(office, p);
    }
    void press_personally(loom::Mail& mail, const char* office, const PanePressed& p) {
        (void)mail.send_to_role(office, p);
    }
    /// ...and the press's second version, in both spellings, for a provider's checks on it.
    void press(loom::Mail& mail, const char* office, const v2::PanePressed& p) {
        (void)mail.as_role(kWorkshopProvider).send_to_role(office, p);
    }
    void press_personally(loom::Mail& mail, const char* office, const v2::PanePressed& p) {
        (void)mail.send_to_role(office, p);
    }
    /// ...and the third, which also names the picture the press was aimed at.
    void press(loom::Mail& mail, const char* office, const v3::PanePressed& p) {
        (void)mail.as_role(kWorkshopProvider).send_to_role(office, p);
    }
    void press_personally(loom::Mail& mail, const char* office, const v3::PanePressed& p) {
        (void)mail.send_to_role(office, p);
    }

    std::vector<PaneOffered> offers;
    std::vector<std::string> offer_authors;
    std::vector<PaneContent> content;
    /// THE PICTURE NUMBER OF EACH CONTENT, parallel to `content`: 0 for a pane that numbers none.
    std::vector<std::int64_t> pictures;
    std::vector<std::string> content_authors;
    std::function<void(PaneWatcher&, loom::Mail&)> next;
};

struct BootState {
    std::int64_t n = 0;
    ZEN_SHAPE(BootState, 1, ZEN_FIELD(n));
};

/// The weave that commands the Weave Manager and HEARS ITS ANSWERS -- the host's own boot shape,
/// because a load whose refusal is addressed to nobody looks exactly like a load that worked. It
/// hears `zen.Ack` too: the control door answers an unload with an Ack rather than a Result, and
/// an answer nobody accepts is that same silence.
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

/// A SEAT AT THE CONTROL DOOR FOR A RELOAD OR AN
/// UNLOAD, with a state shape of its own (see `PaneRig::enqueue_reload`).
struct ControlSeatState {
    std::int64_t n = 0;
    ZEN_SHAPE(ControlSeatState, 1, ZEN_FIELD(n));
};

class ControlSeat
    : public loom::WeaveBase<ControlSeat, ControlSeatState,
                             loom::Accept<loom::Result, loom::Ack, loom::Refused>,
                             loom::Emit<loom::ReloadLibrary, loom::UnloadLibrary>> {
public:
    ControlSeat(std::vector<std::string>& ok, std::vector<std::string>& no)
        : ok_(&ok), no_(&no) {}
    void on(const loom::Result& r, loom::Mail&) { ok_->push_back(r.value); }
    void on(const loom::Ack&, loom::Mail&) {}
    void on(const loom::Refused& r, loom::Mail&) { no_->push_back(r.reason); }

private:
    std::vector<std::string>* ok_;
    std::vector<std::string>* no_;
};

/// A live Workshop that can be handed providers -- native ones always, and the real dynamic Hello
/// when a case asks for it. The mount order is the case's to choose, which is why this is its own
/// rig and not a flag on `Live`: the discovery claim is that neither load order loses an offer,
/// and a rig that always mounted Workshop first could prove only one of the two.
struct PaneRig {
    /// The declaration order is the host's lifetime claim, destruction running in reverse: the
    /// owners the host's own Sources read come first, because each Source's native body closes
    /// over a reference to one and reads it at the sample (`workshop.cpp` declares them above its
    /// own `op::Catalog`); the catalog precedes the Kernel, so every artifact goes down before the
    /// store its contributions live in. The owners are ordinary and mutable: a case moving one
    /// between two samples proves a sample is an evaluation, not a cached answer.
    std::string project_anchor = "/zen/pane-rig";
    CurrentRecipes host_recipes;

    loom::Switchboard bus;
    op::Catalog catalog;
    op::OperatorHostSurface operator_host{catalog};
    loom::Kernel kernel{bus, loom::trust_every_artifact(
                                 "Zengine's test harness: it loads only this build tree's output")};
    loom::WeaveId control = loom::mount_control(kernel, bus);
    loom::WeaveId manager = loom::mount_manager(control, bus);
    InteractionClock clock;
    HostContext host;
    std::vector<surface::SurfaceCanvas> canvases;
    std::vector<surface::SurfaceText> notes;
    /// EVERY `StandingConditions` THIS WORKSHOP HAS SAID, in order -- so a case can ask how
    /// MANY times it spoke and not only what it last said.
    std::vector<StandingConditions> said_conditions;
    /// ...and every `TranscriptShown` this host published, for the same reason.
    std::vector<TranscriptShown> said_transcripts;
    /// ...and every `PaneSubjectShown` -- an inspector's pane subject (WL-INFO-14) -- in order.
    std::vector<PaneSubjectShown> said_subjects;
    /// ...and the hand that opens and closes panes through the host's doors (`door_hand`).
    DoorHand* hand = nullptr;
    loom::WeaveId hand_id{};
    WorkshopWeave* w = nullptr;
    loom::WeaveId workshop_id{};
    /// THE HOST'S WATCH OVER THE QUIT'S DELIVERIES, mounted with Workshop exactly as
    /// `workshop.cpp` mounts it (WL-SESSION-19) -- a rig without it would be a Workshop whose
    /// quit waits on questions Loom refused to deliver, which no shipped host is. Declared after
    /// the bus and the `HostContext` whose book it writes, so it is removed first.
    std::unique_ptr<QuitDeliveryWatch> quit_watch;
    std::vector<std::string> loaded;
    std::vector<std::string> load_refusals;

    PaneRig() {
        host.interaction_now = [this] { return clock.read(); };
        (void)loom::mount<Painter>(bus, canvases, notes, said_conditions, said_transcripts,
                                   said_subjects);
    }


    /// MOUNT WORKSHOP THE WAY THE HOST DOES: in the `zengine.workshop` office, with the host's own
    /// compiled grant, not one minted from the Emit set -- `emit_default_grant` gives `to_any` for
    /// everything declared, wider than the host writes, and the two Builder sentences are
    /// ROLE-SCOPED. Independent denial witnesses check the policy acquires no unintended authority.
    WorkshopWeave* mount_workshop() {
        auto weave = std::make_unique<WorkshopWeave>(host);
        w = weave.get();
        // The press's second version is chosen by the host's answer about the office's holder,
        // wired as workshop.cpp wires it; a case that wants an OLDER host -- one that answers
        // nothing, so every press crosses as v1 -- empties `host.holder_accepts`. With `Live`'s
        // one fixture statement: the stack's stand-in, if a case admits it, is answered present
        // for a ROOM, so the launch door seats it; a case that never admits it gets the host's.
        host.holder_accepts = [this](std::string_view role, const loom::Schema& shape) {
            if (role == stock::kOffice && shape.name() == PaneRoom::zen_name) {
                return true;
            }
            return holder_accepts_on(bus, role, shape);
        };
        host.role_holder = [this](std::string_view role) { return bus.role_holder(role); };
        // ...and where a terminal line can go, read off the same bus at the ask, as workshop.cpp
        // wires it. A case that wants a host listing nothing empties `host.destinations`.
        host.destinations = [this] {
            return bus_destinations(bus, host.terminal != nullptr ? host.terminal->id()
                                                                  : loom::WeaveId{});
        };
        register_workshop(std::move(weave));
        return w;
    }

    /// Use the native host's policy; policy denials are also exercised independently.
    static loom::Grant workshop_grant() { return zengine::workshop::workshop_grant(); }

    /// REGISTER WORKSHOP'S WEAVE IN ITS OFFICE, with the production grant and the quit's watch.
    void register_workshop(std::unique_ptr<loom::Weave> weave) {
        workshop_id =
            bus.register_weave(std::move(weave), workshop_grant(), std::string(kWorkshopProvider));
        w->zen_set_self(workshop_id);
        quit_watch = std::make_unique<QuitDeliveryWatch>(bus, workshop_id, host.undelivered_quits);
    }

    /// TAKE WORKSHOP'S WEAVE OFF THIS BUS FOR AN INTERVAL, AND PUT IT BACK. The weave itself --
    /// its document, its session, the panes it seated -- is the same object throughout; what leaves
    /// with its record is its office and the accept-set that declared its doors, so for the interval
    /// a shape only Workshop accepts is one this bus has never heard of. Put back, it holds the
    /// office again under a new id (`workshop_id`), with the grant it was mounted with. Host root
    /// authority, for an interval no gesture can make.
    std::unique_ptr<loom::Weave> take_workshop_off() {
        quit_watch.reset();
        std::unique_ptr<loom::Weave> off = bus.unregister_weave(workshop_id);
        REQUIRE(off != nullptr);
        workshop_id = loom::WeaveId{};
        return off;
    }
    void put_workshop_back(std::unique_ptr<loom::Weave> weave) {
        REQUIRE(weave != nullptr);
        register_workshop(std::move(weave));
    }

    /// THE OPENING MANAGER, MOUNTED THE WAY THE HOST MOUNTS IT -- its production grant spelled by
    /// hand, since one minted from the Emit set could not notice the host widening it, and the
    /// authority to commit a joint publication minted by this rig's bus for the manager's own id,
    /// over exactly the two offices the host names. A case that wants the managed door mounts
    /// this beside Workshop; one that wants the direct door alone does not.
    OpeningManager* opening = nullptr;
    loom::WeaveId opening_id{};

    OpeningManager* mount_opening() {
        auto opener = std::make_unique<OpeningManager>(
            std::string(kEditorRole), std::string(kWorkshopProvider), host.managed_pane);
        opening = opener.get();
        loom::Grant arrange;
        arrange.allow_to_role(PresentationTrialRequested::zen_name,
                              PresentationTrialRequested::zen_version, kWorkshopProvider);
        arrange.allow_to_role(PresentationAdmitRequested::zen_name,
                              PresentationAdmitRequested::zen_version, kWorkshopProvider);
        arrange.allow_to_role(ManagedOpenProgress::zen_name, ManagedOpenProgress::zen_version,
                              kWorkshopProvider);
        arrange.allow_to_role(ManagedOpenSettled::zen_name, ManagedOpenSettled::zen_version,
                              kWorkshopProvider);
        arrange.allow_to_role(ManagedOpenSettled::zen_name, ManagedOpenSettled::zen_version,
                              kEditorRole);
        arrange.allow_to_role(ManagedOpenProgress::zen_name, ManagedOpenProgress::zen_version,
                              kEditorRole); // the `apply` word, as workshop.cpp grants it
        arrange.allow_to_role(PrepareSourceRequested::zen_name,
                              PrepareSourceRequested::zen_version, kEditorRole);
        arrange.allow_to_any(SourceOpened::zen_name, SourceOpened::zen_version);
        loom::allow_poke_answers(arrange); // inspectable, as the host mounts it
        opening_id = bus.register_weave(std::move(opener), std::move(arrange),
                                        std::string(kOpeningRole));
        opening->zen_set_self(opening_id);
        opening->set_authority(bus.mint_joint_authority(
            opening_id, {std::string(kEditorRole), std::string(kWorkshopProvider)}));
        return opening;
    }

    /// THE TERMINAL PARTICIPANT, MOUNTED THE WAY THE HOST MOUNTS IT -- one narrow grant, owned by
    /// the bus, and handed to Workshop as a non-owning pointer. It is mounted here and not in the
    /// pane, which is the seam's whole shape: the pane under test cannot construct one, reach this
    /// one or speak as it, so every case drives the pane and asks THIS object what it heard.
    /// `widen` lets it say `SurfaceText` to any target, so a case can address a weave by id and
    /// measure what the BUS says about the target rather than what the grant does.
    loom::TerminalSession* mount_terminal(int shapes = 0, bool widen = false) {
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
    loom::WeaveId terminal_id{};

    /// A native provider in an office of its own, granted the pane sentences a provider says --
    /// offer, content, both declarations, caret, unspent Escape -- and a press to forge. The office
    /// is a view, so a case can seat a weave in an office longer than Workshop's own key bound.
    ProviderSeat* mount_provider(std::string_view office) {
        auto seat = std::make_unique<ProviderSeat>(std::string(office));
        ProviderSeat* raw = seat.get();
        loom::Grant grant;
        grant.allow_to_any(PaneOffered::zen_name, PaneOffered::zen_version);
        grant.allow_to_any(PaneContent::zen_name, PaneContent::zen_version);
        grant.allow_to_any(PaneActions::zen_name, PaneActions::zen_version);
        grant.allow_to_any(v2::PaneActions::zen_name, v2::PaneActions::zen_version);
        grant.allow_to_any(PaneCaret::zen_name, PaneCaret::zen_version);
        // ...and that an Escape it was sent was unspent, which is how a pane asks to be put down.
        grant.allow_to_any(PaneEscapeUnspent::zen_name, PaneEscapeUnspent::zen_version);
        // A SEAT MAY FORGE A PRESS, deliberately: the claim under test is that a PROVIDER refuses
        // a press it did not get from Workshop, and a refusal the bus made unreachable proves
        // nothing.
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
#ifdef WORKSHOP_SO_MENU_PRESENTER
    /// LOAD A PRESENTER INTO `zengine.presenter` THROUGH THE REAL MANAGER -- the shipped one by
    /// default, the replacement example when a case asks -- under the plan's own stem, so a later
    /// reload through the control door names the library the plan names.
    loom::WeaveId load_presenter(const char* path = WORKSHOP_SO_MENU_PRESENTER) {
        const loom::WeaveId id = load("zengine-menu-presenter", path, kPresenterRole);
        REQUIRE(id.valid());
        REQUIRE(load_refusals.empty());
        return id;
    }
#endif

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

    /// UNLOAD A REAL LIBRARY THROUGH THE REAL CONTROL DOOR. The Weave Manager has no unload op --
    /// its four are load, swap, reload and list -- so this addresses `zen.UnloadLibrary` to the
    /// door itself, under Loom's own `load_capability` grant rather than a hand-written subset: a
    /// case that quietly narrowed the dangerous grant would be testing its own idea of it.
    bool unload(const char* name) {
        const loom::WeaveId booter = loom::mount_granted<Booter>(
            bus, loom::load_capability(control), loaded, load_refusals);
        const std::size_t before = load_refusals.size();
        bus.send_as(booter, control,
                    loom::Message(loom::to_value(loom::UnloadLibrary{name}), booter, booter, 0));
        bus.drain_until_idle();
        return load_refusals.size() == before;
    }

    /// RELOAD A REAL LIBRARY IN PLACE THROUGH THE REAL CONTROL DOOR -- `zen.ReloadLibrary`, the op
    /// the Weave Manager spends when a maker's rebuilt product is offered: the Kernel snapshots the
    /// live weave, opens the new image and revives it at the same id. QUEUED, NOT DRAINED, so a
    /// reload lands at an exact interval of an operation in flight; the case pumps, and
    /// `load_refusals` says whether it was refused. The seat is `ControlSeat`, not `Booter`: a
    /// realized plan already published the booter's `BootState`, and a second is refused.
    void enqueue_reload(const char* name, const std::string& path) {
        const loom::WeaveId seat = loom::mount_granted<ControlSeat>(
            bus, loom::load_capability(control), loaded, load_refusals);
        bus.send_as(seat, control,
                    loom::Message(loom::to_value(loom::ReloadLibrary{name, path}), seat, seat,
                                  0));
    }
    /// ...and the same, queued: the unload lands at its place in the burst.
    void enqueue_unload(const char* name) {
        const loom::WeaveId seat = loom::mount_granted<ControlSeat>(
            bus, loom::load_capability(control), loaded, load_refusals);
        bus.send_as(seat, control,
                    loom::Message(loom::to_value(loom::UnloadLibrary{name}), seat, seat, 0));
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
        grant.allow_to_any(v2::PanePressed::zen_name, v2::PanePressed::zen_version);
        grant.allow_to_any(v3::PanePressed::zen_name, v3::PanePressed::zen_version);
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

    /// WHAT BECAME OF ONE ID SAID AS WORKSHOP'S OFFICE (`workshop_action`), read off the tap.
    struct OfficeAction {
        bool authored = false;  ///< Loom took the authorship: Workshop held the office named
        bool delivered = false; ///< ...and the office's holder ran its handler on the delivery
        std::string author;     ///< the office that delivery carried
    };

    /// A RESOLVED ACTION ID SAID AS WORKSHOP'S OWN OFFICE, WITH NO KEY BEHIND IT -- through the
    /// host's verified office door (`office_send_to_role_as`): Workshop's weave is the stamped
    /// sender, `zengine.workshop` is verified at authorship, and Workshop's grant gates it. It
    /// hands a pane an id the keymap would never resolve for it, under provenance the pane
    /// accepts; a case pairs it with a declared id through the same door to show provenance was
    /// never the reason. Drained.
    OfficeAction workshop_action(std::string_view office, const std::string& pane,
                                 const std::string& id) {
        OfficeAction said;
        const loom::WeaveId holder = bus.role_holder(office);
        const loom::ObserverId tap = bus.add_observer([&said, holder](const loom::BusEvent& ev) {
            if (ev.kind == loom::EventKind::Delivered && ev.target == holder &&
                ev.schema_name == PaneActionRequested::zen_name) {
                said.delivered = true;
                said.author = ev.authored_role;
            }
        });
        const loom::Ticket sent = bus.office_send_to_role_as(
            workshop_id, kWorkshopProvider, office,
            loom::Message(loom::to_value(PaneActionRequested{pane, id}), workshop_id, workshop_id,
                          0));
        said.authored = sent.valid();
        bus.drain_until_idle();
        bus.remove_observer(tap);
        return said;
    }

    void publish(const loom::Value& v) {
        (void)bus.publish(loom::Message(v, loom::WeaveId{}, loom::WeaveId{}, 0));
        bus.drain_until_idle();
    }

    /// The Skin says hello -- Workshop's startup hook, and where it asks the room who
    /// has panes.
    void ready() { publish(loom::to_value(surface::SurfaceReady{})); }

    /// A MEDIUM REPORTS ITS ROOM, ITS FACE AND ITS CANVAS'S OWN DEVICE UNIT. `cell` defaults to
    /// zero, which the vocabulary spells "my device unit IS the cell": a case that says nothing
    /// about it describes a character medium.
    void extent(std::int64_t width, std::int64_t height, std::int64_t adv = 0,
                std::int64_t line = 0, std::int64_t cell = 0) {
        publish(loom::to_value(surface::SurfaceExtent{width, height, adv, line, cell}));
    }

    /// THE SHIPPED GRAPHICAL FACE'S OWN REPORT, at whatever room a case wants -- the
    /// window's real face metric and the canvas cell it really lays out at. Named, so a
    /// case reads as "the maker is on the window" rather than as four numbers.
    void extent_on_window(std::int64_t width, std::int64_t height) {
        extent(width, height, 8, 18, surface::kCanvasCellPx);
    }

    void key(std::int64_t sc, std::int64_t mods = input::mod::kNone) {
        publish(loom::to_value(input::KeyPressed{sc, "", mods}));
    }

    /// THE CHARACTER A PRINTABLE TRIGGER ALSO PRODUCES -- `Live`'s own door, here because a
    /// case that enters pane management has to pay the `swallow_text_` rule the way a real
    /// backend makes it pay: the key transition AND the text, in the order they arrive.
    void text(const std::string& s) { publish(loom::to_value(input::TextEntered{s})); }

    /// A PRIMARY PRESS AT A CANVAS CELL, as the TERMINAL medium reports it. These cases speak
    /// canvas cells, not workspace cells, because a pane is placed on the canvas and never in the
    /// document's room -- `Live::term_x/y` is the other conversation's inverse.
    void press_cell(std::int64_t cx, std::int64_t cy) {
        publish(loom::to_value(input::PointerButton{1, true, cx, cy + surface::kTuiCanvasTopRow,
                                                    input::space::kCells, input::mod::kNone}));
    }
    /// THE WHEEL AT A CANVAS CELL, as the terminal medium reports it -- `press_cell`'s translation
    /// for the one other pointer gesture that crosses the seam. `dy` is notches, +1 away from the
    /// maker, fractional as a precise wheel reports.
    void wheel_cell(double dy, std::int64_t cx, std::int64_t cy) {
        publish(loom::to_value(input::PointerWheel{0.0, dy, cx, cy + surface::kTuiCanvasTopRow,
                                                   input::space::kCells, input::mod::kNone}));
    }
    /// The same gesture as the WINDOW reports it: an exact device pixel, untranslated.
    /// A graphical press is finer than a cell, so its cases have to be able to say one.
    void press_pixel(std::int64_t px, std::int64_t py) {
        publish(loom::to_value(input::PointerButton{1, true, px, py, input::space::kPixels,
                                                    input::mod::kNone}));
    }
    /// A SECOND-BUTTON press at a canvas cell, `press_cell`'s own translation.
    void right_press_cell(std::int64_t cx, std::int64_t cy) {
        publish(loom::to_value(input::PointerButton{3, true, cx, cy + surface::kTuiCanvasTopRow,
                                                    input::space::kCells, input::mod::kNone}));
    }
    void release_cell(std::int64_t cx, std::int64_t cy) {
        publish(loom::to_value(input::PointerButton{1, false, cx, cy + surface::kTuiCanvasTopRow,
                                                    input::space::kCells, input::mod::kNone}));
    }
    /// THE HAND MOVING AT A CANVAS CELL, as the terminal medium reports it -- what a sweep
    /// is made of between its press and its release (`PaneDragged`'s own instrument).
    void motion_cell(std::int64_t cx, std::int64_t cy) {
        publish(loom::to_value(input::PointerMoved{cx, cy + surface::kTuiCanvasTopRow, 0, 0,
                                                   input::space::kCells, input::mod::kNone}));
    }

    /// OPEN THIS PANE, OR CLOSE IT IF THE DESK NAMES IT -- through the host's two doors, the ones
    /// the Pane Manager spends (`toggle_pane`).
    void pick(const PaneRef& ref) { toggle_pane(*this, ref); }

    // ---- A real authored arrangement, and the door that answers for it ----------------------
    // NO TIMER IN THIS RIG, EVER: `PaneRig` pumps to EMPTY and a live Timer service re-arms its
    // own beat inside its own handler, so a plan naming `zengine-timer` would not return; the
    // arrangement facts that need one are proved in `test_workshop_load.cpp`, whose rigs drain in
    // bounded turns. The artifacts resolve from the suite's own `_SO` paths, not a staging
    // directory: where a host finds a file is the load suite's question.
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
        if (stem == "zengine-files") {
            return WORKSHOP_SO_FILES;
        }
        if (stem == "zengine-builder-pane") {
            return WORKSHOP_SO_BUILDER_PANE;
        }
        if (stem == "zengine-attention-pane") {
            return WORKSHOP_SO_ATTENTION_PANE;
        }
#ifdef WORKSHOP_SO_COMPOSER
        if (stem == "zengine-composer") return WORKSHOP_SO_COMPOSER;
#endif
#ifdef WORKSHOP_SO_INVENTORY_PANE
        if (stem == "zengine-inventory-pane") return WORKSHOP_SO_INVENTORY_PANE;
        if (stem == "zengine-inventory") return WORKSHOP_SO_INVENTORY;
#endif
        if (stem == "zengine-info-pane") {
            return WORKSHOP_SO_INFO_PANE;
        }
        if (stem == "zengine-desktop-pane") {
            return WORKSHOP_SO_DESKTOP_PANE;
        }
#ifdef WORKSHOP_SO_MENU_PRESENTER
        if (stem == "zengine-menu-presenter") {
            return WORKSHOP_SO_MENU_PRESENTER;
        }
#endif
#ifdef WORKSHOP_SO_CONNECTIONS_PANE
        if (stem == "zengine-connections-pane") {
            return WORKSHOP_SO_CONNECTIONS_PANE;
        }
#endif
#ifdef WORKSHOP_SO_TERMINAL_PANE
        if (stem == "zengine-terminal-pane") {
            return WORKSHOP_SO_TERMINAL_PANE;
        }
#endif
#ifdef WORKSHOP_SO_EDITOR_PANE
        if (stem == "zengine-editor-pane") {
            return WORKSHOP_SO_EDITOR_PANE;
        }
#endif
#ifdef WORKSHOP_SO_LEGACY_PANE
        if (stem == "zengine-legacy-pane") {
            return WORKSHOP_SO_LEGACY_PANE;
        }
#endif
#ifdef WORKSHOP_SO_TALLY
        if (stem == "zengine-example-tally") {
            return WORKSHOP_SO_TALLY;
        }
#endif
#ifdef WORKSHOP_SO_FAILING_EDITOR
        if (stem == "zengine-failing-editor") {
            return WORKSHOP_SO_FAILING_EDITOR;
        }
#endif
#ifdef WORKSHOP_SO_EDITOR_THROWING
        if (stem == "zengine-editor-throwing") {
            return WORKSHOP_SO_EDITOR_THROWING;
        }
#endif
#ifdef WORKSHOP_SO_EDITOR_SWITCH_B
        // THE EDITOR SWITCH SUITE'S OTHER CHOICES: the standard Editor built under other stems.
        if (stem == "zengine-editor-pane-b") {
            return WORKSHOP_SO_EDITOR_SWITCH_B;
        }
        if (stem == "zengine-editor-pane-silent") {
            return WORKSHOP_SO_EDITOR_SWITCH_SILENT;
        }
        if (stem == "zengine-editor-pane-refusing") {
            return WORKSHOP_SO_EDITOR_SWITCH_REFUSING;
        }
        if (stem == "zengine-editor-pane-not-live") {
            return WORKSHOP_SO_EDITOR_SWITCH_NOT_LIVE;
        }
        if (stem == "zengine-editor-pane-losing") {
            return WORKSHOP_SO_EDITOR_SWITCH_LOSING;
        }
#endif
#ifdef WORKSHOP_SO_NEOVIM_EDITOR
        // THE NEOVIM-BACKED EDITOR, for suite `workshop_neovim`.
        if (stem == "zengine-neovim-editor") {
            return WORKSHOP_SO_NEOVIM_EDITOR;
        }
#endif
        return stem; // a stem this rig cannot spell refuses at the loader, by name
    }

    /// REALIZE AN AUTHORED PLAN ON THIS RIG'S BUS, the way the host does: the host writes the
    /// booter's grant, the owner realizes the rows, and the owner RETAINS the plan because the
    /// projection pairs authored intent with resolved state. The turns are the caller's: `begin`
    /// issues what it can and returns with a row in flight, and a rig with no host loop turns the
    /// crank itself -- `PlanExecutor` never does (`test_workshop_load.cpp` owns that claim).
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

    /// WIRE WHAT STANDS BEHIND AN OFFICE'S CODE the way `workshop.cpp` wires it: the bus's holder,
    /// this rig's realization owner's rows (none before `run_plan`), and the catalog it holds --
    /// each read at the moment a pane's Edit Code asks.
    void wire_code_source() {
        host.code_source = [this](const std::string& office) {
            if (plan_ != nullptr) {
                return provenance::code_source_of(office, bus, *plan_, host_recipes);
            }
            return provenance::code_source_of(office, bus.role_holder(office), {},
                                              host_recipes.all());
        };
    }

    /// MOUNT THE HOST'S OBSERVATION DOOR, with the production grant spelled out: a rig that minted
    /// the grant from the Emit set could not notice the host quietly widening it.
    loom::WeaveId mount_arrangement(std::string plan_path = std::string()) {
        REQUIRE(plan_ != nullptr); // a door with no owner would describe nothing
        auto door = std::make_unique<ArrangementDoor>(
            *plan_, catalog, std::move(plan_path),
            [this](std::string_view role, const loom::Schema& shape) {
                return holder_accepts_on(bus, role, shape);
            });
        ArrangementDoor* raw = door.get();
        loom::Grant say;
        say.allow_to_any(ResolvedArrangement::zen_name, ResolvedArrangement::zen_version);
        say.allow_to_any(v2::ResolvedArrangement::zen_name, v2::ResolvedArrangement::zen_version);
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

    /// WHOEVER HOLDS `zengine.skin` -- the medium that owns the platform clipboard in both
    /// directions. `Live` has the same seat for the same reason: a pane that can PASTE needs
    /// somebody to answer its ask, and the Painter above holds no role and cannot.
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

    /// MOUNT THE HOST'S SAMPLE DOOR, with the production grant spelled out, for
    /// `mount_arrangement`'s reason.
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
    /// THE NOTICE LINE, READ WHERE IT LIVES. `Session::notice` is painted onto the canvas, not
    /// published as a `SurfaceText`: the published texts are the status slot, the document's line,
    /// and the attention slot, which says what is CURRENTLY true.
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

/// Every prose row of the region an external pane occupies, Workshop's header row included as
/// row 0. The FIRST region at those bounds, which is a statement about order: `all_texts` walks
/// the planes back to front and `paint_panels` paints every pane before any overlay over the
/// stack's first slot, so the first match is the PANE's and a later one is whatever covers it --
/// the fact a case asks when the provider is still publishing and something is on top of it.
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

/// The rows an external pane's PROVIDER is currently showing, read off the published canvas at
/// the region the pane's body occupies -- never off the session, so a case reads what a maker
/// sees. Workshop's own header, prose row 0 of the same region, is dropped: it is Workshop's
/// sentence, and every case here asks what the PROVIDER said.
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

/// THE DURABLE REFERENCE THE INFO PANE IS OFFERED UNDER, as a case spells it: a weave's name
/// (`info-pane/vocabulary.hpp`), not a built-in kind's. A case in the panes suite checks this
/// literal against the weave's own constant, which is where a divergence would be caught.
inline PaneRef info_ref() { return PaneRef{"zengine.info", "info"}; }

/// The setup a two-pane ordering case starts from: two overlay panes, at a screen with room for
/// two stack slots, built through the doors so the ranks are the identity permutation `add_pane`
/// assigns. The two are the stack's stand-ins (`stock` and `second`, both admitted by every
/// `Live`), so no case built on it is about an unresolved row.
inline Setup two_overlays() {
    Setup s;
    s.name = "Arranged";
    REQUIRE(add_pane(s, stock_ref()));
    REQUIRE(add_pane(s, second_ref()));
    return s;
}

/// A `Live`'s session, mutably -- the same door `PaneRig::session` already opens, so a case
/// that has to arrange a setup DIRECTLY (rather than through the keys it is measuring) can.
inline Session& live(Live& t) { return const_cast<Session&>(t.session()); }

/// The kinds a setup AUTHORS, in the order the file holds them: the BASE, not what a maker sees
/// -- `painted_order` below is the effective one.
inline std::vector<std::int64_t> authored_order(const Session& s) {
    return presentation_order(s.setup.active, s.panels);
}

/// WHERE A KIND SITS IN AN ORDER, or -1 -- so a case can state a RELATION between two panes
/// ("this one is ahead of that one") rather than a position ("this one is last"). A position
/// is a claim about every other pane on the desk as well, which is how a case about two
/// panes comes to go red when a third arrives that has nothing to do with it.
inline std::int64_t index_of(const std::vector<std::int64_t>& order, std::int64_t kind) {
    for (std::size_t i = 0; i < order.size(); ++i) {
        if (order[i] == kind) {
            return static_cast<std::int64_t>(i);
        }
    }
    return -1;
}

/// The kinds a setup resolves to, in the order they would be PAINTED -- the authored order with
/// the selected pane lifted. The same call paint, hit testing and coverage spend, so a case
/// reading it is reading the picture.
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

/// THE POPUP UNDER THE HAND, in canvas cells on a CELL medium: a pane's menu while its presenter
/// shows one, the host's own otherwise (at most one is open) -- through the same `context_bounds`
/// the painter and the press resolver spend. `context_entry_cell_y` is population row `index`'s
/// canvas row while the window shows the population from its top with no `earlier` marker, which
/// every case using it arranges; the surface reserves no heading rows, so row `index` is the
/// `index`'th row of the interior.
inline FineRect menu_bounds(const Session& s) {
    return s.presented.open ? presented_bounds(s, screen_of(s)) : context_bounds(s, screen_of(s));
}
inline std::int64_t context_cell_x(const Session& s) {
    return surface::cell_of_subs(menu_bounds(s).x) + kChromeCells + 1;
}
inline std::int64_t context_entry_cell_y(const Session& s, std::size_t index) {
    return surface::cell_of_subs(menu_bounds(s).y) + kChromeCells +
           static_cast<std::int64_t>(index);
}

/// THE LINES THE PRESENTER SHOWED FOR THE OPEN MENU, as the host holds them (empty when none).
inline std::vector<std::string> presented_texts(const Session& s) {
    std::vector<std::string> out;
    for (const surface::SurfaceTextRow& line : s.presented.lines) {
        out.push_back(line.text);
    }
    return out;
}

/// THE LINE OF THE PRESENTED MENU THAT READS `label` (after whatever marks the presenter puts in
/// front of a row), or -1.
inline std::int64_t presented_line_of(const Session& s, const std::string& label) {
    const std::vector<std::string> lines = presented_texts(s);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::string& t = lines[i];
        if (t.size() >= label.size() && t.compare(t.size() - label.size(), label.size(), label) == 0) {
            return static_cast<std::int64_t>(i);
        }
    }
    return -1;
}

/// IS A PANE'S MENU OPEN AND SHOWN -- granted to the presenter, and its lines drawn?
inline bool menu_shown(const Session& s) {
    return s.presented.open && !s.presented.lines.empty();
}

/// The surface's published region, read off a canvas at exactly its bounds -- searched
/// BACK TO FRONT because a slot-seated pane and another authored over it can share
/// the popup's origin, and the contextual surface paints over all of them.
inline std::vector<std::string> context_rows_on(const surface::SurfaceCanvas& c,
                                                const Session& s) {
    const Screen sc = screen_of(s);
    const FineRect popup = menu_bounds(s);
    const surface::SurfaceTextRegion want = panel_prose_region(panel_prose_place(popup, sc));
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

/// OPEN A PANE THROUGH THE LAUNCH DOOR, AS SCAFFOLDING (`seat_pane`), and require that it opened.
inline void open_pane(Live& t, const PaneRef& ref) {
    const PaneLaunchAnswered said = seat_pane(t, ref);
    REQUIRE_MESSAGE(said.refusal.empty(), said.refusal);
    REQUIRE(has_pane(t.session().setup.active, ref));
}

/// OPEN A PANE AND PUT IT AT THE RIGHT COLUMN -- the desk row `default_setup` ships for the Info
/// weave, spelled by hand so a case can have a pane in that place without an office behind it.
/// It is written, not gestured: `pane_unit::kRightColumn` is a PLACE MODE, not a coordinate, and
/// no arrow produces "the right edge, the room's full height" on a screen the desk does not know;
/// a setup file spelling `right-column` produces exactly this row, and so does `default_setup`.
inline void open_at_right_column(Live& t, std::int64_t kind) {
    pick(t, kind);
    REQUIRE(t.session().panels.has(kind));
    for (SetupPane& row : live(t).setup.active.panes) {
        if (row.ref == ref_of(kind)) {
            row.place.mode = pane_unit::kRightColumn;
        }
    }
    REQUIRE(bounds_of(t.session().panels, t.session().setup.active, kind,
                      screen_of(t.session()))
                .placed_in == placement::kSideRegion);
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

/// A `Live` driven into the desk arrangement scope. Every keyboard case begins here, through the
/// REAL input path -- the `w` transition and the character the platform's layout made of it,
/// both, in the order the backends report them.
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

/// HOW MANY PANES THIS ONE OFFICE OFFERS. Cases say the number rather than assume it: "one
/// provider is not one pane" is what `PaneOffered` was shaped for, and a case that indexed
/// `entries[0]` would quietly assert the opposite.
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

/// A session with a screen extent and a text metric.
inline Session screen_session(std::int64_t w, std::int64_t h, std::int64_t advance,
                              std::int64_t line, std::int64_t cell_px = 0) {
    Session s;
    s.screen_w = w;
    s.screen_h = h;
    s.text_advance_px = advance;
    s.text_line_px = line;
    s.cell_px = cell_px;
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

/// AN INDEPENDENT LISTENER, and the point is that it is a STRANGER: it compiles
/// `introspection/vocabulary.hpp` and nothing else of the tool, registers no callback, is handed
/// no pointer, and is not Workshop, a provider or in any office. What it hears, it hears because
/// a weave published an ordinary Loom message -- the whole claim `LoadedSelected` makes good on.
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

// ---- A stand-in desktop: the participating owner of the application's defaults ------------
// Escape-to-deselect is a DECLARED application row (WL-DESK-02), so a Workshop with no desktop
// has no such row and Escape does nothing -- "a maker can disable an application default" -- and
// a case asserting the deselect must supply the party that owns it. A stand-in, not the shipped
// weave: it declares the shipped ids on the shipped gestures and answers the deselect row the
// shipped way, for the many cases that need the behaviour without loading `desktop-pane/`.
class DesktopSeat
    : public loom::WeaveBase<DesktopSeat, SeatState,
                             loom::Accept<AppActionRequested, ActionsJudged, ActionsWithdrawn,
                                          PaneLaunchAnswered, PaneCloseAnswered, SeatDo>,
                             loom::Emit<AppActions, DeselectRequested, PaneLaunchRequested,
                                        PaneCloseRequested>> {
public:
    void on(const AppActionRequested& asked, loom::Mail& mail) {
        asked_.push_back(asked.id);
        last_ask_ = mail.correlation();
        if (asked.id == kDeselectId && autoanswer) {
            // ECHOED UNDER THE NUMBER IT ARRIVED ON, which is what makes this answer about
            // THIS keystroke and no other.
            (void)mail.as_role(kDesktopRole)
                .send_to_role(kWorkshopProvider, DeselectRequested{}, mail.correlation());
        }
    }
    /// WORKSHOP'S VERDICT ON ONE OF THIS PARTY'S DECLARATIONS, with the number Loom says it
    /// answers and whether Loom says it IS an answer. Kept, never acted on: a declarer's recovery
    /// is its own, and this one's is to remember.
    void on(const ActionsJudged& said, loom::Mail& mail) {
        verdicts_.push_back(Verdict{mail.correlation(), mail.answers_ask(), said});
        if (!said.accepted) {
            refusals_.push_back(said.refusal);
        }
    }
    /// ...AND A DECLARATION IT HAD IN FORCE LEAVING THE KEYMAP LATER.
    void on(const ActionsWithdrawn& said, loom::Mail&) {
        withdrawals_.push_back(said);
        refusals_.push_back(said.refusal);
    }
    /// WHAT ITS LAUNCHES AND CLOSES CAME TO, kept in arrival order.
    void on(const PaneLaunchAnswered& said, loom::Mail&) { launched_.push_back(said); }
    void on(const PaneCloseAnswered& said, loom::Mail&) { closed_.push_back(said); }

    void on(const SeatDo&, loom::Mail& mail) {
        if (next) {
            std::function<void(DesktopSeat&, loom::Mail&)> once;
            once.swap(next);
            once(*this, mail);
        }
    }

    void declare(loom::Mail& mail, const AppActions& a, std::uint64_t attempt = 0) {
        (void)mail.as_role(kDesktopRole).send_to_role(kWorkshopProvider, a, attempt);
    }
    /// Declared PERSONALLY, from the very weave holding the office. Holding is not
    /// speaking-for (MSG-07), and Workshop drops it for the offer's reason.
    void declare_personally(loom::Mail& mail, const AppActions& a) {
        (void)mail.send_to_role(kWorkshopProvider, a);
    }
    void launch(loom::Mail& mail, const std::string& office, const std::string& pane) {
        (void)mail.as_role(kDesktopRole)
            .send_to_role(kWorkshopProvider, PaneLaunchRequested{office, pane});
    }
    void close(loom::Mail& mail, const std::string& office, const std::string& pane) {
        (void)mail.as_role(kDesktopRole)
            .send_to_role(kWorkshopProvider, PaneCloseRequested{office, pane});
    }
    /// A DESELECT ANSWER THAT ECHOES A NUMBER OF ITS OWN CHOOSING -- for the cases about an
    /// answer to an ask that is over, and about one that echoes nothing at all.
    void deselect_answering(loom::Mail& mail, std::uint64_t answering) {
        (void)mail.as_role(kDesktopRole)
            .send_to_role(kWorkshopProvider, DeselectRequested{}, answering);
    }

    const std::vector<std::string>& asked() const { return asked_; }
    const std::vector<std::string>& refusals() const { return refusals_; }
    /// ONE VERDICT AS IT ARRIVED: the correlation it echoes, Loom's word that it answers an ask,
    /// and what it said.
    struct Verdict {
        std::uint64_t correlation = 0;
        bool answer = false;
        ActionsJudged said;
    };
    const std::vector<Verdict>& verdicts() const { return verdicts_; }
    const std::vector<ActionsWithdrawn>& withdrawals() const { return withdrawals_; }
    const std::vector<PaneLaunchAnswered>& launched() const { return launched_; }
    const std::vector<PaneCloseAnswered>& closed() const { return closed_; }
    /// THE NUMBER THE LAST ASK ARRIVED UNDER -- what an honest answer echoes, and what a case
    /// about a DISHONEST one has to be able to miss on purpose.
    std::uint64_t last_ask() const { return last_ask_; }

    /// WHETHER THIS STAND-IN ANSWERS ITS DESELECT ROW BY ITSELF. Off for the case that has to
    /// leave an ask outstanding at a known gesture and then answer it wrongly: with the
    /// automatic answer in the way, the host has already reset its record and the wrong answer
    /// is refused by the gesture test rather than by the one under examination.
    bool autoanswer = true;

    std::function<void(DesktopSeat&, loom::Mail&)> next;

    static constexpr const char* kDeselectId = "desktop.deselect";
    static constexpr const char* kTerminalId = "desktop.terminal";
    static constexpr const char* kPanesId = "desktop.panes";
    static constexpr const char* kHotkeysId = "desktop.hotkeys";

private:
    std::vector<std::string> asked_;
    std::vector<std::string> refusals_;
    std::vector<Verdict> verdicts_;
    std::vector<ActionsWithdrawn> withdrawals_;
    std::vector<PaneLaunchAnswered> launched_;
    std::vector<PaneCloseAnswered> closed_;
    std::uint64_t last_ask_ = 0;
};

/// THE FOUR ROWS THE SHIPPED DESKTOP DECLARES, spelled once so a case and the product cannot
/// come to disagree about what the defaults are.
inline AppActions shipped_app_actions() {
    AppActions a;
    a.rows.push_back(AppActionRow{DesktopSeat::kTerminalId, "terminal", zengine::input::scan::kT,
                                  zengine::input::mod::kCtrl, app_precedence::kAboveModes});
    a.rows.push_back(AppActionRow{DesktopSeat::kPanesId, "panes", zengine::input::scan::kP,
                                  zengine::input::mod::kCtrl, app_precedence::kAboveModes});
    a.rows.push_back(AppActionRow{DesktopSeat::kHotkeysId, "hotkeys", zengine::input::scan::kK,
                                  zengine::input::mod::kCtrl, app_precedence::kAboveModes});
    a.rows.push_back(AppActionRow{DesktopSeat::kDeselectId, "put down",
                                  zengine::input::scan::kEscape, zengine::input::mod::kNone,
                                  app_precedence::kDefault});
    return a;
}

/// THE EFFECTIVE KEYMAP AS TEXT, one binding per line -- `group | gesture | label | id`, with
/// ` *` where the maker's file moved it -- the value a presenter of keys is given
/// (`keymap_shown`), read by a case the way the Hotkeys pane reads it.
inline std::string keymap_text(const Session& s) {
    std::string out;
    for (const ShownBinding& b : keymap_shown(s, std::string(), std::string()).rows) {
        out += b.group + " | " + b.gesture + " | " + b.label + " | " + b.id +
               (b.authored ? " *" : "") + (b.remappable ? "" : " (not remappable)") + "\n";
    }
    return out;
}

/// SEAT A STAND-IN DESKTOP AND LET IT DECLARE. Returns the seat, so a case can read what it
/// was asked for and what Workshop refused it.
template <class Rig>
inline DesktopSeat* mount_desktop(Rig& t, AppActions rows = shipped_app_actions()) {
    auto seat = std::make_unique<DesktopSeat>();
    DesktopSeat* raw = seat.get();
    loom::Grant grant;
    grant.allow_to_role(AppActions::zen_name, AppActions::zen_version, kWorkshopProvider);
    grant.allow_to_role(DeselectRequested::zen_name, DeselectRequested::zen_version,
                        kWorkshopProvider);
    grant.allow_to_role(PaneLaunchRequested::zen_name, PaneLaunchRequested::zen_version,
                        kWorkshopProvider);
    grant.allow_to_role(PaneCloseRequested::zen_name, PaneCloseRequested::zen_version,
                        kWorkshopProvider);
    const loom::WeaveId id =
        t.bus.register_weave(std::move(seat), std::move(grant), std::string(kDesktopRole));
    raw->zen_set_self(id);
    raw->next = [rows](DesktopSeat& d, loom::Mail& m) { d.declare(m, rows); };
    (void)t.bus.send(id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                       loom::WeaveId{}, 0));
    t.bus.drain_until_idle();
    return raw;
}

/// RUN ONE SENTENCE FROM THE STAND-IN DESKTOP, inside its own delivery, and drain.
template <class Rig>
inline void desktop_does(Rig& t, DesktopSeat* seat,
                         std::function<void(DesktopSeat&, loom::Mail&)> what) {
    REQUIRE(seat != nullptr);
    seat->next = std::move(what);
    t.publish(loom::to_value(SeatDo{}));
}


/// SEAT A RECORDING PROVIDER ON A `Live` RIG, and make it offer one pane -- answering with the
/// kind Workshop minted, or `kNoPaneKind`. `PaneRig` has its own for the seam's cases; a `Live`
/// case uses this when it needs another entry in a pane population, which comes from where every
/// pane comes from: an office that offered one.
inline std::int64_t live_offer_pane(Live& t, const char* office, const char* pane,
                                    const char* name) {
    auto seat = std::make_unique<ProviderSeat>(std::string(office));
    ProviderSeat* raw = seat.get();
    loom::Grant grant;
    grant.allow_to_any(PaneOffered::zen_name, PaneOffered::zen_version);
    grant.allow_to_any(PaneContent::zen_name, PaneContent::zen_version);
    grant.allow_to_any(PaneActions::zen_name, PaneActions::zen_version);
    grant.allow_to_any(v2::PaneActions::zen_name, v2::PaneActions::zen_version);
    const loom::WeaveId id =
        t.bus.register_weave(std::move(seat), std::move(grant), std::string(office));
    raw->zen_set_self(id);
    raw->next = [pane, name](ProviderSeat& p, loom::Mail& m) {
        p.offer(m, PaneOffered{pane, name, "a seated provider"});
    };
    (void)t.bus.send(id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                       loom::WeaveId{}, 0));
    t.bus.drain_until_idle();
    for (const RuntimePane& row : t.w->session().panels.runtime.entries) {
        if (row.provider == std::string(office) && row.pane == std::string(pane)) {
            return row.kind;
        }
    }
    return kNoPaneKind;
}

/// Open an external pane belonging to a seat in `office`, and answer with its kind: the seat
/// offers, Workshop admits, the launch door opens it and the room is granted -- four beats a case
/// would otherwise spell every time.
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

/// Press somewhere this pane is NOT -- the gesture that hands the keyboard back to Workshop,
/// which a case must perform before it can use a command key. The cell is derived from the
/// pane's own rectangle: an overlay slot starts at the canvas's top-left, so a literal `(1, 1)`
/// is inside the pane it was meant to be outside of, a mistake that reads as a routing defect.
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

/// ---- Contributions that carry the exterior contract ------------------------------------
/// `supplied_by` above leaves `source` and `output` at their defaults, an OPERATOR with no
/// reported output schema, which is what the arrangement cases want. These two say the other
/// half, in the shape `describe_powers` reads off a real definition: `source` is `op::is_source`
/// and the output identity is the definition's own output schema. The provider, the construction
/// and the contract are three arguments because the claim under test is that they are three facts.
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

/// THE SAME LIVE WORKSHOP, WITH THE POWERS PANE OPEN AND BOTH HOST DOORS MOUNTED -- and with the
/// host's own two Sources really in the catalog, so the Sources view has the population a maker
/// meets. The order is the host's: sources exposed before the plan runs, the observation door
/// and the sample door mounted before realization can grant a pane room.
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

/// The band's legend rows, padding trimmed, read in one place.
inline std::vector<std::string> band_lines(PaneRig& r) {
    std::vector<std::string> out;
    const Screen sc = screen_of(r.session());
    for (const std::int64_t y : {sc.help_y, sc.help_y + 1}) {
        const std::string row = inspector_row(r.last_canvas(), 0, y);
        if (!row.empty()) {
            out.push_back(row);
        }
    }
    return out;
}

/// PRESS A PLACE IN AN EXTERNAL PANE'S OWN ROOM -- the provider's row and column,
/// which is exactly the pair `PanePressed` carries. Cases speak the provider's lattice
/// so the arithmetic that turns it into a canvas cell lives in one place.
inline void press_pane(PaneRig& r, std::int64_t kind, std::int64_t row, std::int64_t column) {
    const ui::Rect body = external_body_rect(r.session(), kind);
    r.press_cell(body.x + column, body.y + kExternalHeaderRows + row);
}

/// POINT THE KEYBOARD AT A PANE WITHOUT ALSO AUTHORING A GESTURE. The keyboard goes to the pane a
/// maker last pressed into (WL-FOCUS-01) and there is no shape for asking, so this spends a press
/// on a row whose provider-side meaning is NOTHING -- the bound sentence where there is one, the
/// last row otherwise -- possible only because every target in this pane means exactly one thing.
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

/// MAKE ONE PANE TALLER, the way a maker can: an authored height in canvas cells, written into
/// the setup the screen resolves its bounds from. Not a test door: `kStackRows` (nine -- eight
/// prose rows under one header) is the developer's default, not a law, and a projection whose
/// entries are several rows tall does not fit six artifacts in eight rows; it counts what it
/// could not show and gives the maker a pane that grows.
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

/// THE PRESENTATION, AS A POPULATION OF SOURCE FILES, for the tripwires that read Workshop as
/// text: every `.hpp` and `.cpp` under `WORKSHOP_SOURCE_DIR` but the host's side of the seam,
/// whose whole business is what the presentation must not reach -- a tripwire that read those
/// would forbid a door from naming what it opens. WALKED, NEVER ENUMERATED: a list fails open when
/// a body moves into a new file, and a walk holds the file the moment it exists. A walk that finds
/// fewer than the floor is a red, never a smaller claim; it goes down only with the files moved.
inline constexpr std::size_t kPresentationSourceFloor = 62;

inline std::vector<std::string> presentation_sources() {
    // The host's side of the seam, by name -- the host, its load plan and executor, the
    // observation door, the rules and doors it wires (`staging`, `provenance`, `pane_doors` and
    // their shapes) -- and `pane_migration.hpp`, for another reason: it is the one file that may
    // spell the reference saved files wrote for a retired browser, and the tripwire forbidding
    // every other file that spelling would otherwise forbid it too.
    static constexpr const char* kHostSide[] = {"workshop.cpp", "load_execute.hpp", "load_plan.hpp",
                                                "arrangement.hpp", "arrangement_vocabulary.hpp",
                                                "staging.hpp", "authoring.hpp", "pane_doors.hpp",
                                                "pane_seam_vocabulary.hpp",
                                                "pane_migration.hpp", "provenance.hpp"};
    std::vector<std::string> out;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(WORKSHOP_SOURCE_DIR)) {
        const std::string name = entry.path().filename().string();
        const std::string ext = entry.path().extension().string();
        if (ext != ".hpp" && ext != ".cpp") {
            continue;
        }
        bool host_side = false;
        for (const char* h : kHostSide) {
            host_side = host_side || name == h;
        }
        if (!host_side) {
            out.push_back(entry.path().generic_string());
        }
    }
    std::sort(out.begin(), out.end());
    REQUIRE_MESSAGE(out.size() >= kPresentationSourceFloor, "the walk of ", WORKSHOP_SOURCE_DIR,
                    " found ", out.size(), " presentation sources, below the floor of ",
                    kPresentationSourceFloor);
    return out;
}

/// A keymap file's bytes, composed through the format's own serializer -- which is also what
/// makes the round-trip cases byte-exact rather than approximately so.
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

// Author an explicit TUI body budget for a behavior fixture. Preferences remain the
// default-size witnesses' subject; a larger window alone need not change a preferred pane.
template<class Rig>
void author_test_pane_room(Rig& r, std::int64_t kind, std::int64_t rows, std::int64_t columns) {
    bool found = false;
    for (auto& row : r.session().setup.active.panes) {
        if (resolve_pane(row.ref, r.session().panels) != kind) continue;
        row.width = {pane_unit::kSubcells, subs(columns + 2)};
        row.height = {pane_unit::kSubcells, subs(rows + 3)};
        found = true;
    }
    REQUIRE(found);
}

#endif // ZENGINE_TESTS_WORKSHOP_SUPPORT_HPP
