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
void paint_panels(surface::SurfaceCanvas& c, const WorkshopDoc& d, const Session& s,
                  const Screen& sc, const ProjectFrontier& frontier) {
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
            if (p.kind == panel::kBuilder) {
                paint_builder(layer, panels.builder, b, sc, frontier, s.recipes_moved_to,
                              chrome);
            } else if (p.kind == panel::kInfo) {
                paint_info(layer, d, s, b, sc, chrome);
            } else if (p.kind == panel::kEditor) {
                paint_editor(layer, s, b, sc, chrome);
            } else if (p.kind == panel::kProjectFiles) {
                paint_files(layer, s, b, sc, s.keymap, chrome);
            } else if (p.kind == panel::kLayouts) {
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
    // THE CURRENT-CONDITION VIEW, IN THE PICKER'S OWN PLANE: over the panes it
    // covers, under the screen's own chrome. The band keeps speaking while it is open --
    // what a maker is READING is what is currently true, and what the band SAYS is what
    // just happened, and those are two different sentences that must not cover each other.
    detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
        paint_attention(layer, s, sc, frontier);
    });
    // THE CONTEXTUAL-ACTION SURFACE, LAST IN THE BAND: over the picker and the
    // attention view, because it is the band's later, more deliberate gesture -- and it
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
        const RuntimePane* typed_into =
            typing == kNoPaneKind ? nullptr : s.panels.runtime.of_kind(typing);
        // THE SOURCE EDITOR IS THE SECOND KEYBOARD-TAKING PANE, and it gets the same
        // sentence for the same measured reason: keystrokes landing somewhere
        // the screen does not name is the lie this row exists to refuse.
        std::string said;
        if (typed_into != nullptr && s.keymap.resolved_legend() == legend_mode::kFull) {
            said = "typing goes to " + typed_into->name + " @" + typed_into->provider +
                   " -- press elsewhere for Workshop's keys";
        } else if (ctx == KeyContext::kEditor &&
                   s.keymap.resolved_legend() == legend_mode::kFull) {
            said = "typing goes to the source editor -- press elsewhere for Workshop's keys";
        } else if (ctx == KeyContext::kFiles &&
                   s.keymap.resolved_legend() == legend_mode::kFull) {
            // THE BROWSER TAKES KEYS WITHOUT TAKING TEXT, so the sentence says KEYS. The
            // row exists for the same measured reason the two above it do: a maker whose
            // arrows have stopped meaning what they mean in command mode is entitled to
            // read why on the screen rather than infer it from a gesture that did nothing.
            said = "keys go to Project Files -- press elsewhere for Workshop's keys";
        } else if (ctx == KeyContext::kPaneEditor &&
                   s.keymap.resolved_legend() == legend_mode::kFull) {
            said = "keys go to the Pane Manager -- press elsewhere for Workshop's keys";
        }
        if (!said.empty()) {
            if (legend_rows == 1) {
                const std::int64_t rest =
                    columns - static_cast<std::int64_t>(said.size()) - 3;
                const std::vector<std::string> pairs = help_rows(s.keymap, ctx, rest, 1);
                legend.push_back(detail::fit(
                    pairs.empty() ? said : said + " | " + pairs.front(), columns));
            } else {
                legend.push_back(detail::fit(said, columns));
                const std::vector<std::string> pairs =
                    help_rows(s.keymap, ctx, columns, legend_rows - 1);
                for (const std::string& row : pairs) {
                    legend.push_back(row);
                }
            }
        } else {
            legend = help_rows(s.keymap, ctx, columns, legend_rows);
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
            help_rows(s.keymap, keyboard_context(s), columns, 1);
        if (!pairs.empty()) {
            push(pairs.front(), surface::role::kMuted);
        }
    }
    return band;
}

// WL-FRONT-01, WL-FRONT-07 -- agents/workshop/planes.md
// WL-ATTN-04 -- agents/workshop/attention.md
// WL-DOC-18 -- agents/workshop/document.md
// WL-RGN-05 -- agents/workshop/regions.md
surface::SurfaceCanvas paint(const WorkshopDoc& d, const Session& s,
                             const ProjectFrontier& frontier) {
    const Screen sc = screen_of(s);
    surface::SurfaceCanvas c;
    c.width = sc.w;
    c.height = sc.h;

    // THE WORKSPACE PLANE: what a maker authored, as this workspace places it.
    // It is written whole before any pane is, because a pane is a presentation IN FRONT of
    // the document -- which is what `occupied_at` has answered and what the
    // picture now agrees with instead of merely being told.
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

    // The workspace, as a thing with edges a maker can see. Its extent is a
    // session fact, so resizing it visibly changes what a share resolves to
    // while changing no authored value at all.
    rect(kWorkspaceX, kWorkspaceY, s.workspace_w, s.workspace_h, surface::role::kMuted);

    // The scene: the authored elements, as this workspace places them. Painting
    // walks the SCENE, not the document -- so a rectangle on screen is by
    // construction a rectangle the hit test can find.
    const ui::Scene scene = workspace_scene(d, s);
    for (const ui::Placed& p : scene.items) {
        const std::int64_t x = kWorkspaceX + p.rect.x;
        const std::int64_t y = kWorkspaceY + p.rect.y;
        if (p.id == s.selected) {
            rect(x - 1, y - 1, p.rect.w + 2, p.rect.h + 2, surface::role::kAccent);
        }
        rect(x, y, p.rect.w, p.rect.h, surface::role::kFill);
        // The label, written on the object, clipped to the workspace rather than
        // allowed to run into the panel beside it. The label is authored, so it
        // is read from the element and not from the observation of it.
        //
        // AND IT IS SEMANTIC TYPE ON MATERIAL SOMEBODY ELSE OWNS, which is the
        // one place in this tool where that sentence has to be argued rather than assumed.
        // The name is semantic -- it is the maker's word for this object and its exact cell
        // occupancy is no part of what they authored -- so it belongs in a bounded region.
        // Its rectangle, though, is already full: the object's body is authored MATERIAL,
        // drawn one line up as a `SurfaceRect`. An ordinary region over it erases that
        // material in both media, and rows carrying the object's role as a GROUND leave a
        // `12h - 4 - 18*rows` pixel band the strips cannot reach (10 px across the foot of a
        // default 12x4 object; `12h - 4 == 18k` has no integer solutions, so SOME remainder
        // exists at every height). Both were built and run live, twice -- once at first and
        // once again to re-measure them. `surface::kGroundBeneath` is the third
        // answer: the region keeps its bounds, so the name is fitted and cut against them,
        // and gives up the ground, so nothing under it is painted over.
        //
        // THE BOUND IS THE OBJECT'S OWN RESOLVED WIDTH, clipped by the workspace's
        // right edge -- and earlier it was only the second of those. The name used to be
        // given `workspace_w - x` cells, so a name longer than the object it names ran out of
        // it and across the backdrop; the re-measure preserved that deliberately and then MEASURED
        // what it costs, which is the paragraph below. The room is the material's, because
        // this is type ON material and material the object does not have is not this name's
        // room to spend. The workspace clip stays because it answers a different question --
        // an object may be authored wider than the room to the edge, and its name is still
        // not the panel's to write into.
        //
        // ...OR ONE COLUMN, WHICHEVER IS MORE, for the row floor's reason said about the
        // other axis (below): a zero-WIDTH object is reachable from a poke or a hand-built
        // document exactly as a zero-height one is, and one cell of room leaves `detail::fit`
        // a mark to put there rather than leaving the object with no trace at all.
        //
        // WHAT A MEDIUM STILL GETS TO SAY IS HOW MANY CHARACTERS THOSE CELLS HOLD, and that
        // half is and unchanged: `fit_region` answers 12 columns in cells and 17
        // columns of a 13pt face for a 12-cell object, so a name is marked when it genuinely
        // did not fit rather than when it would not have fitted as bitmap cells.
        //
        // AND ITS HEIGHT IS THE OBJECT'S, which is what makes a one-cell object honest for
        // free. `fit_region` sends a region with no room for a row of the medium's face back
        // to the cell projection, so an object a maker sized to one cell shows its
        // name in cells -- the same picture a terminal shows -- rather than 18 pixels of type
        // hanging out of a 12-pixel object. No `if (h < N)` was written here; the rule is the
        // one both media already resolve with.
        //
        // ...OR ONE ROW, WHICHEVER IS MORE, and that floor is not a fudge: a name is written
        // ON a row, so the room it needs is a row, and an object whose resolved height is
        // zero still has the row its origin is on. `check_extent` refuses an authored height
        // below one cell, so this is reachable only from a poke or a hand-built document --
        // but it WAS reachable earlier and such an object's name was the only trace of
        // it on the workspace, and a region with no bounds shows nothing and says nothing
        // about it. Measured: without the floor, three zero-height objects lost their names
        // outright. The floor restores byte-for-byte the run of cells the label drew, in
        // every medium, because one cell of room is a cell region either way.
        //
        // THE CUT IS MARKED, and earlier it was not. `resize` here was a silent
        // truncation of a string a MAKER chose (up to `doc::kMaxNameLen`), which is the exact
        // defect found in the picker's name column and repaired the same way: a shorter
        // name that looks finished is a lie about the document. `detail::fit` marks it.
        //
        // AND WHY THE ROOM IS THE MATERIAL'S, WRITTEN HERE BECAUSE IT IS THIS CALL SITE'S.
        // The re-measure found the cost of the old bound in a medium that paints roles as ink: the
        // name is `kMuted` so it reads quietly on the object's `kFill` body, and the workspace
        // backdrop a few statements up is ALSO `kMuted` -- so every character past the
        // object's own edge was the backdrop's exact colour and could not be read at all. Six
        // cells of material and a thirty-two byte name meant 9 characters legible and 23
        // invisible, measured on the pristine tree. Earlier the overhang was legible
        // only for a reason nobody chose: every label cell was cleared to the canvas
        // background first, which is the same hole in the workspace that it was in the object.
        //
        // NO ROLE FIXES THAT, WHICH IS WHY THE ANSWER IS THE BOUND. This medium's inks are
        // kFill 176, kAccent 112/232/240, kMuted 96 and kAlert red: nothing reads on BOTH a
        // `kFill` body and a `kMuted` backdrop, `kAccent` means "the one thing being pointed
        // at" and would make every object shout, and a fifth role is exactly what
        // `surface/vocabulary.hpp` refuses. Contrast is a palette question and the palette is
        // the medium's -- which is the whole reason a publisher ships roles. So the repair is
        // not a colour and not a ground: it is that a name never leaves the material it names,
        // and where it does not fit that material it says so with `detail::fit`'s mark. The
        // authored name is untouched by any of it, and widening the object reveals more of the
        // same authored bytes -- which is the property the whole arrangement is for.
        const ui::Element* authored = doc::find(d, p.id);
        const std::int64_t columns = p.rect.w > 1 ? p.rect.w : 1;
        const std::int64_t to_edge = s.workspace_w - p.rect.x;
        const std::int64_t room = columns < to_edge ? columns : to_edge;
        if (authored != nullptr && room > 0) {
            const std::int64_t rows = p.rect.h > 1 ? p.rect.h : 1;
            const surface::RegionFit fit =
                surface::fit_region(x, y, room, rows, sc.text_advance_px, sc.text_line_px);
            surface::SurfaceTextRegion named;
            named.x = x;
            named.y = y;
            named.w = room;
            named.h = rows;
            named.ground = surface::kGroundBeneath;
            named.rows.push_back(surface::SurfaceTextRow{
                detail::fit(authored->label, fit.columns), surface::role::kMuted});
            on->texts.push_back(std::move(named));
        }
    }

    // The size handle, over everything in the workspace, as a GLYPH rather than
    // as another rectangle. That is not decoration: the ring already paints this
    // exact cell in the accent role, so a rect here would be invisible, and the
    // affordance has to be distinguishable from the ring, from the object's body
    // and from the workspace at a glance. `SurfaceLabel` carries arbitrary text
    // over the rects, so the generic canvas vocabulary already had what this
    // needed -- no role was added, and nothing in surface/ or ui/ changed.
    // (Honest cost: a Skin with no text stack draws no handle. Both shipped
    // media have one -- a terminal's own font, and the SDL medium's bitmap face
    // in surface/skin_sdl_glyphs.hpp -- so nothing declines it today.)
    const Handle handle = size_handle(d, s);
    if (handle.shown) {
        label(kWorkspaceX + handle.x, kWorkspaceY + handle.y, kHandleGlyph,
              surface::role::kAccent);
    }

    // THE DYNAMIC PANELS -- every one of them, INCLUDING the OBJECTS and PROPERTIES columns
    // a maker has always read on the right. Each takes a PLANE of its own, in canonical
    // front order, so a later-ranked pane covers an earlier one kind for kind.
    //
    // THIS ONE CALL IS THE WHOLE OF A REMOVABLE INFO AT THIS LEVEL. What used to be forty lines of
    // furniture painted unconditionally here is now a panel like any other: present because a
    // fresh session opens it, absent the moment a maker removes it, and painted by whoever
    // owns that kind rather than by `paint`.
    paint_panels(c, d, s, sc, frontier);

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
    // ⚠ THE BOTTOM BAND BELONGS TO THE OVERLAY WHILE THAT IS OPEN, AND THE LAYOUTS PANE
    // DOES NOT. The Terminal is anchored to the bottom-right corner and covers most of the
    // screen's width at every extent, so bottom-band rows painted underneath it would
    // survive only in the cells to its left -- a sentence beheaded mid-word with nothing to
    // say so. The Layouts pane's default rows are ones the overlay cannot reach:
    // `terminal_y` is `h - terminal_h`, which is 9 at the minimum screen and grows with the
    // surface, so it is never less than `kTopRows`. A maker in the Terminal therefore keeps
    // reading which layout they are in, which is the honest answer rather than a courtesy --
    // those rows are not covered, so hiding them would be a lie about occlusion. A maker who
    // MOVED the pane under the overlay is covered by it and correctly so, which is a thing
    // this screen could not say at all until the conversion.
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
    if (!s.terminal.open) {
        detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
            layer.texts.push_back(band_region(s, sc));
        });
    }

    if (s.terminal.open) {
        // THE FINAL MODAL PLANE, and that is the whole of what "overlay" means here. A pane
        // in the last layer covers whatever it lands on -- and the screen underneath is
        // composed exactly as it was before this phase, with no row budget taken from it and
        // no constant moved. A closed pane appends no layer at all.
        detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
            paint_terminal(layer, s.terminal, sc, s.keymap);
        });
    }

    // THE HOTKEY VIEW, LATER STILL: a maker can open it OVER the Terminal to read
    // the Terminal line's own keys, so it must be readable above the pane whose context it
    // is describing. It is the one plane after the Terminal's, and it is a projection --
    // the screen beneath it, the Terminal included, is composed exactly as if it were
    // closed, which is also why the context it reports is the context beneath it.
    if (s.hotkeys.open) {
        detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
            paint_hotkeys(layer, s, sc);
        });
    }

    return c;
}

} // namespace zengine::workshop
