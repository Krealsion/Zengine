// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `screen.hpp`'s section -- the composition: every presented pane back to front,
// one layer each, the bottom band as one published region, and the whole screen as one canvas of
// planes -- compiled once into `zengine-workshop-logic` and linked by the host and every suite;
// the declarations, the constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/planes.md (+4 registers; agents/workshop.md routes)

#include "screen.hpp"

namespace zengine::workshop {

// ---- THE COMPOSITION: every pane back to front, the bottom band, and the screen as planes ----

// WL-FRONT-01, WL-FRONT-05, WL-FRONT-07 -- agents/workshop/planes.md
// WL-MAKER-05 -- agents/workshop/maker-pane.md
// ⭐ THE DOCUMENT IS NO LONGER A PARAMETER. Info was the one pane whose painter needed it --
// its two lists ARE the document -- and Info is a weave; every remaining painter draws from the
// session alone, which is what a host that composes no document view looks like.
void paint_panels(surface::SurfaceCanvas& c, const Session& s, const Screen& sc) {
    const Panels& panels = s.panels;
    const std::int64_t lifted = selected_pane(panels);
    for (const std::int64_t kind : effective_pane_order(s.setup.active, panels)) {
        const Panel p{kind};
        const FineRect b = bounds_of(panels, s.setup.active, p.kind, sc).rect;
        if (b.w <= 0 || b.h <= 0) {
            continue;
        }
        const std::int64_t chrome = p.kind == lifted ? kPaneChromeSelected : kPaneChrome;
        detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
            // ⭐ THE EDITOR'S ARM WAS FIRST HERE AND IS GONE: it is an external pane, painted
            // by the generic arm at the end of this chain like every other.
            if (p.kind == panel::kLayouts) {
                // THE LAYOUT RUN, THE SETUP ASSOCIATION AND THE WORKSPACE FACT --
                // one more arm, in the one walk, and that is the whole of what the
                // conversion cost this function. What it BUYS is the two lines above it:
                // the rectangle is `bounds_of`'s, the order is `effective_pane_order`'s,
                // and a pane a maker put in front of this one is drawn over it.
                paint_layouts(layer, s, b, sc, chrome);
            } else if (p.kind == panel::kPaneEditor) {
                paint_pane_editor(layer, s, b, sc, chrome);
            } else if (is_maker_kind(p.kind)) {
                // THE MAKER'S OWN PANE -- one more arm in the one walk, and that
                // is the whole of what a pane made of DATA costs this function. Its
                // rectangle is `bounds_of`'s, its order is `effective_pane_order`'s, its
                // chrome is the same chrome, and the only thing this arm decides is which
                // painter: the one that reads an authored interior instead of composing one.
                paint_maker_pane(layer, s, b, sc, chrome);
            } else if (is_runtime_kind(p.kind)) {
                // ONE GENERIC ARM FOR EVERY EXTERNAL PANE, and there is no second one to
                // add. The branch above chooses a PAINTER, which placement named as the one
                // thing about a panel kind that genuinely cannot be shared -- and this arm
                // is the case where it can be, because every external pane is presented
                // identically: a header Workshop writes and a region the provider fills. A
                // second provider costs this function nothing at all.
                paint_external(layer, panels, p.kind, b, sc, s.pane_titles, chrome);
            }
        });
    }
    // THE PANE CREATOR'S REGION MARK: over the panes, in the affordances' own
    // position and for their reason -- it says which rectangle of the maker's pane the
    // rows they are editing describe, derived from the same resolution that painted it, and
    // it is drawn on a plane of its own so the pane's own interior cannot cover it.
    detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
        paint_creator_region_mark(layer, s, sc);
    });
    detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
        paint_pane_affordances(layer, s, sc);
    });
    detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
        paint_picker(layer, panels, s.setup.active, sc, s.keymap);
    });
    // ⚠ THE CURRENT-CONDITION VIEW USED TO BE A PLANE HERE, in the picker's own place, over
    // the panes it covered. It is a PANE now and is drawn where its setup row puts it, by
    // the same walk that draws every other pane -- so what covers what is a maker's own
    // arrangement rather than a decision this function makes for them.
    // THE CONTEXTUAL-ACTION SURFACE, LAST IN THE BAND: over the picker, because it is the
    // band's later, more deliberate gesture -- and it
    // takes the band's keys first for the same reason (`keyboard_context`), so what is
    // frontmost and what answers agree.
    detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
        paint_context(layer, s, sc);
    });
}

surface::SurfaceTextRegion band_region(const Session& s, const Screen& sc) {
    const ui::Rect b = band_bounds(sc);
    const surface::RegionFit fit = band_fit(sc);
    surface::SurfaceTextRegion band;
    band.x = b.x;
    band.y = b.y;
    band.w = b.w;
    band.h = b.h;
    const std::int64_t budget = fit.rows;
    const std::int64_t columns = fit.columns;
    if (budget <= 0 || columns <= 0) {
        return band;
    }

    const std::string notice = s.notice.empty() ? std::string() : detail::fit(s.notice, columns);
    const std::int64_t notice_role =
        s.notice_is_bad ? surface::role::kAlert : surface::role::kFill;

    // THE LEGEND TAKES WHAT THE NOTICE LEAVES, which is this band's whole composition policy
    // now that the identity has its own band. A character medium's four rows are the
    // notice and three of legend where the context has that many pairs; the shipped face's
    // two are the notice and one, which is exactly the pair it read before the split. No
    // reserved row is spare: the band spent the old blank row on the workspace fact and this
    // keeps that discipline rather than handing one back.
    //
    // While an external pane holds the keyboard and the legend is FULL, the first legend row
    // still says so -- that sentence is keyboard-ownership truth, not a binding list,
    // and where the legend has one row the sentence takes it and the chorded survivors follow
    // in whatever room is left.
    const std::size_t legend_rows =
        budget >= 2 ? static_cast<std::size_t>(budget - 1) : 0;
    std::vector<std::string> legend;
    if (legend_rows > 0) {
        const KeyContext ctx = keyboard_context(s);
        const std::int64_t typing = keyboard_pane(s.panels);
        // THE SENTENCE NAMES WHERE AN ORDINARY KEY GOES, WHICH IS NOT ALWAYS THE PANE THE KEYS
        // ARE POINTED AT. Under the picker, a naming line or the hotkey view that pane is still
        // the candidate -- its title keeps the mark, and the keys return to it when the mode
        // closes -- while every ordinary key is the mode's. `typing_pane` is the answer a
        // press's `keys_went_here` is read from, so the band and the seam say one thing.
        const std::int64_t typed = typing_pane(s);
        const RuntimePane* typed_into =
            typed == kNoPaneKind ? nullptr : s.panels.runtime.of_kind(typed);
        // THE PANE MANAGER IS THE ONE BUILT-IN LEFT THAT TAKES THE KEYS, and it gets the
        // same sentence for the same measured reason: keystrokes landing somewhere the
        // screen does not name is the lie this row exists to refuse. (The source editor
        // had a sentence of its own here; it is a pane, and the first arm names it.)
        std::string said;
        if (typed_into != nullptr && s.keymap.resolved_legend() == legend_mode::kFull) {
            said = "typing goes to " + typed_into->name + " @" + typed_into->provider +
                   " -- press elsewhere for Workshop's keys";
        } else if (ctx == KeyContext::kPaneEditor &&
                   s.keymap.resolved_legend() == legend_mode::kFull) {
            said = "keys go to the Pane Manager -- press elsewhere for Workshop's keys";
        }
        if (!said.empty()) {
            if (legend_rows == 1) {
                const std::int64_t rest =
                    columns - static_cast<std::int64_t>(said.size()) - 3;
                const std::vector<std::string> pairs =
                    help_rows(s.keymap, ctx, rest, 1, typing);
                legend.push_back(detail::fit(
                    pairs.empty() ? said : said + " | " + pairs.front(), columns));
            } else {
                legend.push_back(detail::fit(said, columns));
                // THE PANE'S OWN DECLARED ROWS COME FIRST IN WHAT FOLLOWS (WL-KEY-15),
                // then the chorded survivors: `help_pairs` orders them so.
                const std::vector<std::string> pairs =
                    help_rows(s.keymap, ctx, columns, legend_rows - 1, typing);
                for (const std::string& row : pairs) {
                    legend.push_back(row);
                }
            }
        } else {
            legend = help_rows(s.keymap, ctx, columns, legend_rows, typing);
        }
    }

    const auto push = [&band](std::string text, std::int64_t role) {
        band.rows.push_back(surface::SurfaceTextRow{std::move(text), role});
    };
    if (budget >= 2) {
        push(notice, notice_role);
        for (std::string& row : legend) {
            push(std::move(row), surface::role::kMuted);
        }
    } else if (!notice.empty()) {
        // One row: the tool's own voice while it has something to say. The identity line is
        // not a candidate here any more -- it has a band of its own that this budget cannot
        // take away.
        push(notice, notice_role);
    } else {
        const std::vector<std::string> pairs =
            help_rows(s.keymap, keyboard_context(s), columns, 1, keyboard_pane(s.panels));
        if (!pairs.empty()) {
            push(pairs.front(), surface::role::kMuted);
        }
    }
    return band;
}

// WL-FRONT-01, WL-FRONT-07 -- agents/workshop/planes.md
// WL-ATTN-04 -- agents/workshop/attention.md
surface::SurfaceCanvas paint(const Session& s) {
    const Screen sc = screen_of(s);
    surface::SurfaceCanvas c;
    c.width = sc.w;
    c.height = sc.h;

    // THE ROOM'S PLANE: the room's own edges and the desktop's floor. It is written whole before
    // any pane is, because a pane is a presentation IN FRONT of the room -- which is what
    // `occupied_at` has answered and what the picture agrees with instead of merely being told.
    // (It was the prototype object canvas's plane, the authored objects as the workspace placed
    // them, until that canvas retired.)
    //
    // THE SCREEN'S OWN CHROME IS NOT HERE. It is a plane of its own, added after the panes,
    // for a reason worth stating where both are decided: the bottom band is where the tool
    // SPEAKS, and a panel's backdrop painted over it would take the notice that just told a
    // maker what happened and erase it under the furniture it describes. (The shared top
    // row that used to be this note's other half is retired -- see the band below.)
    //
    // A REFERENCE INTO `c.layers` IS SPENT BEFORE ANY OTHER LAYER IS ADDED. That is not a
    // coincidence to be preserved by care: `paint_panels` is the first thing that grows the
    // vector, and the two lambdas below are the only writers before it.
    c.layers.emplace_back();
    surface::SurfaceLayer* on = &c.layers.back();

    const auto rect = [&on](std::int64_t x, std::int64_t y, std::int64_t w, std::int64_t h,
                            std::int64_t role) {
        on->rects.push_back(surface::SurfaceRect{x, y, w, h, role});
    };
    const auto label = [&on](std::int64_t x, std::int64_t y, std::string text,
                             std::int64_t role) {
        on->labels.push_back(surface::SurfaceLabel{x, y, std::move(text), role});
    };

    // The room, as a thing with edges a maker can see.
    rect(kWorkspaceX, kWorkspaceY, sc.room_w, sc.room_h, surface::role::kMuted);

    // ⭐ THE ROOM'S FLOOR -- what the participating desktop said stands in the empty workspace
    // (WL-DESK-05).
    //
    // THE CANVAS STOOD ON IT UNTIL THE CANVAS RETIRED: its rectangles were written after the
    // floor and covered it, the transition visible rather than hidden. The scene loop that drew
    // them is gone, and the floor is what the room shows.
    //
    // ⚠ WORKSHOP COMPOSES NOTHING HERE. The rows are the desktop's own words, painted at the
    // place and in the roles it said, clipped to the room the host owns. A host that edited
    // them would be a host with a desktop compiled into it again -- which is the whole of what
    // this seam removed. An empty `backdrop` is the honest picture of a Workshop whose desktop
    // has not spoken, or never loaded, and it paints nothing at all.
    for (std::size_t i = 0; i < s.backdrop.size(); ++i) {
        const std::int64_t y = kWorkspaceY + 1 + static_cast<std::int64_t>(i);
        if (y >= kWorkspaceY + sc.room_h) {
            break; // the room ran out; the rest is not drawn and nothing is invented
        }
        const surface::SurfaceTextRow& row = s.backdrop[i];
        label(kWorkspaceX + 2, y, detail::fit(row.text, sc.room_w - 4), row.role);
    }

    // THE DYNAMIC PANELS -- every one of them, INCLUDING the Info column a maker has always
    // read on the right. Each takes a PLANE of its own, in canonical
    // front order, so a later-ranked pane covers an earlier one kind for kind.
    //
    // THIS ONE CALL IS THE WHOLE OF A REMOVABLE INFO AT THIS LEVEL. What used to be forty lines of
    // furniture painted unconditionally here is now a panel like any other: present because a
    // fresh session opens it, absent the moment a maker removes it, and painted by whoever
    // owns that kind rather than by `paint`.
    paint_panels(c, s, sc);

    // AND THE SCREEN'S OWN CHROME OVER THEM, on its own plane -- which is a
    // budget-composed region rather than one label per cell row, and is ONE of
    // them: the bottom band, where the tool speaks and where the keys are explained. See the
    // note at the top of this function for why it is in front rather than behind: a band is
    // where the tool SPEAKS, and a panel backdrop drawn over one would erase the notice that
    // just told a maker what happened.
    //
    // ⚠ THE TOP BAND IS NOT HERE ANY MORE. The layout selector, the setup
    // association and the workspace fact were the other half of this plane and are an
    // ordinary pane now -- painted by `paint_panels` above, in canonical front order, over
    // and under whatever a maker arranged around them. The ROWS they defaulted to are still
    // reserved (`kTopRows`, and `room_h` is byte-identical either way); what changed is that
    // something authorable stands on them instead of something this function drew.
    //
    // THE OLD SHARED TOP ROW IS STILL RETIRED, AND ITS CELL IS SPENT NOW.
    // Canvas row 0 carried four one-cell voices -- the workspace's extent, the picker and
    // window hints, the terminal hint -- each structurally unable to hold a row of a real
    // face. The band conversion moved those facts into the band and left the row EMPTY, because the
    // workspace's extent is what a share resolves against and a chrome retirement must not
    // resize a maker's document. The split spends that cell, and one more from the bottom band,
    // on a top band two cells tall -- which is what a face needs for one row of type. The
    // reserved total is what it was, so the workspace still did not move.
    //
    // A REGION TAKES ITS RECTANGLE, and that is a deliberate widening over the labels it
    // replaced: the old rows cleared only the cells their characters landed on, and a band
    // clears all of its rows across the canvas. A pane a maker authors over the bottom band
    // is covered BY it, because the panes are in front of the DOCUMENT and not in front of
    // the tool's own voice, and the band occupies no pointer space at all.
    //
    // ⚠ THAT LAST EXEMPTION USED TO HAVE AN EXCEPTION AND NO LONGER DOES. The top
    // band painted in front of every pane and answered presses on the layout tabs alone, so
    // a pane dragged under it was visually erased and still met the hand -- see-here,
    // press-there, at exactly the boundary one geometry exists to forbid. Both halves are gone: the
    // tabs are a pane's interior, and `occupied_at` answers that pane for those cells like
    // any other.
    // ⭐ AND THE BAND IS PAINTED UNCONDITIONALLY NOW (VD-24). It used to be suppressed while
    // the terminal overlay was open, because that overlay was anchored to the bottom-right
    // corner and covered most of the screen's width at every extent -- so band rows painted
    // under it survived only in the cells to its left, a sentence beheaded mid-word with
    // nothing to say so. A pane that a maker placed has no such claim: whatever a pane
    // covers, it covers with a boundary a maker can see and can move.
    detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
        layer.texts.push_back(band_region(s, sc));
    });

    // ⭐ THE TERMINAL'S FINAL MODAL PLANE WAS HERE AND IS GONE (VD-24). It was the whole of
    // what "overlay" meant: a layer after every pane, so the terminal covered whatever it
    // landed on and no arrangement a maker authored could put anything in front of it. The
    // Terminal is in the pane planes now, in the order a maker chose.

    // ⭐ THE HOTKEY VIEW WAS PAINTED HERE, LAST, as a host overlay. It is the desktop's Hotkeys
    // pane now, arranged like any pane, over the keymap this host publishes (`KeymapShown`).

    return c;
}

} // namespace zengine::workshop
