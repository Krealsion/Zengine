// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `screen.hpp`'s sections -- the dynamic panels painted, what state one pane is in,
// a pane's geometry in the face's own language, and a surface sized by what it says -- compiled
// once into `zengine-workshop-logic` and linked by the host and every suite; the declarations,
// the constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/panes-and-windows.md (+7 registers; agents/workshop.md routes)

#include "screen.hpp"

namespace zengine::workshop {

// ---- The dynamic panels, painted -------------------------------------------------------

// WL-CHROME-04 -- agents/workshop/chrome.md; WL-PANE-05 -- agents/workshop/panes-and-windows.md
// WL-CHROME-04 -- agents/workshop/chrome.md; WL-PANE-05 -- agents/workshop/panes-and-windows.md
void paint_panel_frame(surface::SurfaceLayer& layer, const FineRect& b,
                       std::int64_t role) {
    layer.rects.push_back(wire_rect_of(b, role));
}

// WL-CHROME-05 -- agents/workshop/chrome.md; WL-RGN-01 -- agents/workshop/regions.md
PanelProsePlace panel_prose_place(const FineRect& b, const Screen& sc) {
    PanelProsePlace p;
    const PaneInside inside = pane_inside(b, sc);
    p.inside = inside.rect;
    p.chrome_subs = inside.chrome_subs;
    if (inside.rect.w <= 0 || inside.rect.h <= 0) {
        return p;
    }
    p.fit = inside.fit;
    p.rows = p.fit.rows;
    p.columns = p.fit.columns;
    p.present = p.rows > 0 && p.columns > 0;
    return p;
}

// WL-RGN-01 -- agents/workshop/regions.md
surface::SurfaceTextRegion panel_prose_region(const PanelProsePlace& place) {
    surface::SurfaceTextRegion region;
    const surface::SurfaceRect wire = wire_rect_of(place.inside, surface::role::kFill);
    region.x = wire.x;
    region.y = wire.y;
    region.w = wire.w;
    region.h = wire.h;
    region.sub_x = wire.sub_x;
    region.sub_y = wire.sub_y;
    region.sub_w = wire.sub_w;
    region.sub_h = wire.sub_h;
    return region;
}

std::string panel_field(const char* label, const std::string& value) {
    return detail::pad(label, 9) + value;
}

// WL-RGN-02 -- agents/workshop/regions.md
std::vector<std::string> panel_block(const char* label, const std::string& value,
                                     std::size_t rows, std::int64_t width) {
    std::vector<std::string> lines = detail::wrap(panel_field(label, value), width);
    if (lines.size() > rows) {
        lines.resize(rows);
        lines.back() = detail::fit(lines.back() + " " + detail::kElided, width);
    }
    while (lines.size() < rows) {
        lines.push_back(std::string());
    }
    return lines;
}

// WL-RGN-02 -- agents/workshop/regions.md
void paint_builder(surface::SurfaceLayer& layer, const BuilderPane& pane,
                   const FineRect& b, const Screen& sc,
                   const ProjectFrontier& frontier,
                   const std::string& catalog_moved_to,
                   std::int64_t chrome) {
    paint_panel_frame(layer, b, chrome);
    // THE PANEL IS ONE REGION AND ITS ROWS ARE COMPOSED AGAINST THE BUDGET. The
    // Builder was the last consumer of the cell-lattice row spelling, and the recorded
    // reason was never typography: nine facts do not fit five rows, and until the panel
    // had a COMPOSITION PRIORITY there was no honest way to choose which five. The
    // priority is written below, on each fact, and the rule is:
    //
    //     what survives longest is what a maker is ACTING on -- the office's identity,
    //     the live build's activity/result, the frontier the project is waiting on, what
    //     `b` will build next, the realization outcome, the compiler's own words --
    //     and what yields first is static metadata (the exit code's row, the command
    //     echo) and the tail of the output block.
    //
    // The DISPLAY order never changes with the budget: a shorter face shows the same
    // rows in the same order minus the ones that did not fit, so growing the window
    // reveals more truth rather than switching to a different panel. A character medium's
    // nine-row budget selects every fact, byte-for-byte the composition this panel has
    // painted.
    const PanelProsePlace place = panel_prose_place(b, sc);
    if (!place.present) {
        return; // no room for one row of this medium's type: say nothing at all
    }
    const std::int64_t columns = place.columns;
    struct Fact {
        std::string text;
        std::int64_t role = surface::role::kFill;
        std::int64_t priority = 0; ///< smaller survives longer; all distinct
    };
    std::vector<Fact> facts; // display order, priorities deciding survival

    // THE HEADER NAMES THE OFFICE IT IS PRESENTING, AND NOTHING ELSE. The same
    // discipline the terminal pane's header follows: a presentation that shows somebody
    // else's facts without saying whose is a presentation that will eventually be read as
    // its own.
    //
    // ITS FOUR SHORTCUTS ARE GONE. `b`/`B`, `c`, `f` and the picker's removal key are
    // ordinary `kActionCatalog` rows in command mode, so the band's legend and the full
    // hotkey view already say every one of them -- in the maker's own bindings -- and this
    // pane was spending a third of its widest row restating them. The keymap made the claims
    // truthful; this makes them singular. Project Files reached the same answer first
    // ("THE GESTURES ARE NOT PAINTED HERE") and the argument is the same one: the pane
    // spends its rows on the project rather than on instructions.
    facts.push_back(Fact{std::string("BUILDER @") + builder::kBuilderRole,
                         surface::role::kAccent, 0});

    const auto publish = [&](std::vector<Fact> chosen, const std::string& said_detail) {
        // KEEP WHAT THE BUDGET SEATS, IN DISPLAY ORDER. The priorities are distinct, so
        // "the `place.rows` smallest" is one threshold; the said block is wrapped LAST,
        // into exactly the rows that survived, so its elision mark tells the truth about
        // this budget rather than about the nine-row one. A dropped fact is dropped WHOLE
        // -- nothing substitutes for it, and the rows that remain neither move nor reword.
        std::vector<std::int64_t> priorities;
        priorities.reserve(chosen.size());
        for (const Fact& f : chosen) {
            priorities.push_back(f.priority);
        }
        std::sort(priorities.begin(), priorities.end());
        const std::size_t seats = static_cast<std::size_t>(place.rows);
        const std::int64_t cut =
            priorities.size() > seats ? priorities[seats] : priorities.back() + 1;
        std::size_t said_kept = 0;
        for (const Fact& f : chosen) {
            if (f.priority < cut && f.text.empty()) {
                ++said_kept; // a said placeholder: counted now, written below
            }
        }
        std::vector<std::string> said;
        if (said_kept > 0) {
            said = panel_block("said", said_detail.empty() ? std::string("--") : said_detail,
                               said_kept, columns);
        }
        surface::SurfaceTextRegion region = panel_prose_region(place);
        std::size_t said_at = 0;
        for (Fact& f : chosen) {
            if (f.priority >= cut) {
                continue;
            }
            std::string text = f.text.empty() ? said[said_at++] : std::move(f.text);
            region.rows.push_back(surface::SurfaceTextRow{
                detail::fit(std::move(text), columns), f.role});
        }
        layer.texts.push_back(std::move(region));
    };

    if (!pane.heard) {
        // NOT THE SAME AS "NEVER BUILT", and the panel must not show it as though it were.
        // This is a fact about this panel -- it has asked and is waiting -- and the recipe's
        // own history is not knowable from here until the tool says it.
        facts.push_back(Fact{panel_field("recipe", "(the Builder has not answered yet)"),
                             surface::role::kMuted, 1});
        publish(std::move(facts), std::string());
        return;
    }

    const builder::BuildStatus& s = pane.shown;
    // WHAT THE MAKER HAS PICKED OUT, AND HOW MANY THERE ARE TO PICK FROM.
    //
    // IT IS THE CHOICE AND NOT THE LAST BUILD, and when they differ the choice is the
    // truer row: it is what `b` will do next, which is the question a maker looking at a
    // Builder panel is actually asking. What the last build was about is on the rows
    // below it, where an outcome belongs.
    //
    // AN EMPTY CATALOG IS SAID PLAINLY. A project may hold no recipes -- there is no
    // recipes file, or it names none -- and a panel that showed a blank name would look
    // like one that had not heard yet, which is the distinction `heard` exists to keep.
    const std::size_t held = pane.known.recipes.size();
    if (held == 0) {
        facts.push_back(Fact{panel_field("recipe", "(this project has no build recipes)"),
                             surface::role::kMuted, 3});
    } else {
        const std::size_t at = pane.chosen < held ? pane.chosen : std::size_t{0};
        facts.push_back(
            Fact{panel_field("recipe", pane.known.recipes[at].recipe + " -> " +
                                           pane.known.recipes[at].artifact + "  (" +
                                           std::to_string(at + 1) + "/" +
                                           std::to_string(held) + ")"),
                 surface::role::kFill, 3});
    }
    // ---- WHICH AUTHORED CATALOG THIS SESSION MOVED TO, WHILE IT HAS -------------------
    //
    // THE ROW EXISTS EXACTLY WHILE THE FACT HAS MOVED, which is the `project` row's rule
    // and is taken for the same reason: this panel is exactly full. Nine facts and nine
    // rows of a character medium, measured -- so a tenth UNCONDITIONAL row would spend the
    // third `said` row of every session, including every session that never changes a
    // catalog, to restate what the host's own banner already said correctly at launch.
    // What a replacement changes is that the banner STOPS being true, and that is the
    // moment this row appears. It costs one `said` row while it holds (`shift`, below),
    // and its priority puts it last, so a face whose budget seats five keeps the same five
    // rows it seated before this phase.
    //
    // THE PATH IS ABSOLUTE, AND IT IS CUT BY THE MEASURER THAT KEEPS ITS TAIL. An earlier phase put
    // a project-relative spelling here because the browser could not reach outside the
    // project and an ordinary fit removes a path's filename -- the half that says which
    // catalog this is. Free navigation removed the first half of that premise, so a based spelling
    // with no stated base became a wrong-looking name for the right file, and the answer is
    // the absolute path plus `detail::fit_path`: root cue, a mark where the middle was
    // removed, and the tail intact. Nothing here reformats a path, shortens it to a
    // basename, or widens the panel to hold one.
    const bool moved_catalog = !catalog_moved_to.empty();
    if (moved_catalog) {
        facts.push_back(Fact{panel_field("catalog",
                                         detail::fit_path(catalog_moved_to, columns - 9)),
                             surface::role::kMuted, 10});
    }
    // ---- WHAT THE PROJECT IS WAITING ON, WHILE IT IS ----------------------------------
    //
    // THE ROW EXISTS EXACTLY WHILE THE FRONTIER DOES, and it costs the third `said` row,
    // which is the row this panel can best afford exactly here: a maker whose project is
    // WAITING has no build output yet, and one whose frontier build FAILED still reads two
    // rows of the compiler's ending plus the whole stream on the bus. When nothing is
    // waiting the composition is byte-for-byte the earlier one, because absence of a pending
    // frontier is the whole answer and this panel will not invent a "nothing blocked" to
    // fill a row. Under a constrained budget the row OUTLIVES everything but the header
    // and the live activity row -- it is the actionable pressure this panel exists to
    // surface, and a face that hid it while showing the command echo would be showing the
    // less useful truth.
    //
    // THREE FACTS, ONE ROW, TWO OWNERS. The artifact and the blocked count are the
    // realization owner's, read alive through the host at this paint; which recipes can
    // produce the artifact is the tool's own published catalog, joined here BY STEM --
    // the one edge the catalog allows. One producing recipe is named; several are counted
    // (`f` names them, and `c` shows each beside the artifact it makes); none is said
    // plainly, because a frontier this project cannot produce is a different problem.
    const std::size_t shift = (frontier.waiting ? 1u : 0u) + (moved_catalog ? 1u : 0u);
    if (frontier.waiting) {
        std::size_t makers = 0;
        const builder::RecipeSummary* maker = nullptr;
        for (const builder::RecipeSummary& known : pane.known.recipes) {
            if (known.artifact == frontier.artifact) {
                ++makers;
                maker = &known;
            }
        }
        std::string said = "waiting " + frontier.artifact + " (";
        if (makers == 0) {
            said += "no recipe";
        } else if (makers == 1) {
            said += maker->recipe;
        } else {
            said += std::to_string(makers) + " recipes";
        }
        said += ", blocks " + std::to_string(frontier.blocked) + ")";
        facts.push_back(Fact{panel_field("project", said), surface::role::kAccent, 2});
    }
    // WHAT THIS PANEL IS WATCHING beats what it was last told. `awaiting` is the panel's own
    // fact and it is the truer one while it holds: the tool's last OUTCOME is still the
    // previous build's, and showing that while a new one is running would answer "what
    // happened on the last build" with a sentence about the wrong build.
    //
    // THE OPERATION AND THE OUTPUT COUNT SHARE THIS ROW, and they are on the panel
    // for one reason: they are what make a running build VISIBLE rather than asserted. A
    // maker who presses `b`, moves a rectangle, opens Info and comes back to a Builder that
    // says `running -- op #1, 37 out` has watched Workshop stay alive while a real child
    // process ran, and has watched the count climb while doing it. A build that had frozen
    // the pump could not have produced either number, because nothing would have been
    // delivered to change them. They stay on the row after it ends, so the evidence does not
    // vanish at the moment it becomes a result. It is the LIVE row, so under a constrained
    // budget it outlives everything but the header.
    const bool named_op = s.op != 0;
    const std::string carried =
        named_op ? " -- op #" + std::to_string(s.op) + ", " + std::to_string(s.chunks) + " out"
                 : std::string();
    const bool unanswered = pane.awaiting && s.outcome != builder::outcome::kRunning;
    facts.push_back(
        Fact{unanswered ? panel_field("last", "asked -- waiting for it to start")
                        : panel_field("last",
                                      std::string(builder::name_of_outcome(s.outcome)) +
                                          carried),
             unanswered || s.outcome == builder::outcome::kRunning
                 ? surface::role::kAccent
                 : (s.outcome == builder::outcome::kFailed ||
                            s.outcome == builder::outcome::kNotStarted ||
                            s.outcome == builder::outcome::kNoArtifact ||
                            s.outcome == builder::outcome::kUnknownRecipe
                        ? surface::role::kAlert
                        : surface::role::kFill),
             1});
    // THE EXIT STATUS IS ONLY SHOWN WHEN THERE WAS ONE. A `0` printed after a build that
    // never started reads as success, which is the exact wrong answer at the exact moment a
    // maker most needs the right one.
    //
    // THE TOOL'S OWN COUNTER SHARES THE ROW, and it is on the panel at all because it is the
    // number that proves the tool outlives its presentation: close this panel, reopen it,
    // build again, and it reads 2 -- which a panel that owned the state could not say. It
    // shares rather than taking its own because the rows below are worth more to a maker
    // whose build just failed, and this one has a column to spare. Static metadata: under a
    // constrained budget it yields to every outcome row and to the first `said` row.
    facts.push_back(
        Fact{panel_field("exit", detail::pad(s.outcome == builder::outcome::kSucceeded ||
                                                     s.outcome == builder::outcome::kFailed
                                                 ? std::to_string(s.status)
                                                 : std::string("--"),
                                             11) +
                                     "asks " + std::to_string(s.builds) + " ever"),
             surface::role::kMuted, 6});
    // WHAT WAS ACTUALLY RUN, as the runner reported it. Empty until something has been run,
    // because the tool holds no command and this panel will not invent one to fill a row.
    // The first fact a constrained budget gives up: it is an echo of the maker's own act.
    facts.push_back(Fact{panel_field("ran", s.command.empty()
                                                ? std::string("(nothing has run yet)")
                                                : s.command),
                         surface::role::kMuted, 7});
    // ---- THE SECOND OUTCOME, ON ITS OWN ROW ---------------------------------------------
    //
    // A BUILD OUTCOME AND A REALIZATION OUTCOME ARE TWO ANSWERS AND THIS PANEL SHOWS TWO.
    // The alternative -- one "status" row that says whichever of them is more recent -- is
    // exactly the conflation the Builder's own two fields exist to prevent, and it is worst
    // in the case a maker most needs: a build that WORKED whose realization was REFUSED.
    // The row is present even when nothing was asked, because an absent row reads as an
    // absent question rather than as an unasked one.
    facts.push_back(
        Fact{panel_field("realize",
                         s.realization == builder::realization::kNotAsked
                             ? std::string("-- (B builds and realizes)")
                             : std::string(builder::name_of_realization(s.realization)) +
                                   (s.realized_detail.empty() ? std::string()
                                                              : " -- " + s.realized_detail)),
             s.realization == builder::realization::kRefused
                 ? surface::role::kAlert
                 : (s.realization == builder::realization::kRealized ? surface::role::kFill
                                                                     : surface::role::kMuted),
             4});
    // THREE ROWS FOR WHAT THE BUILD SAID, because this is the row budget a maker spends when
    // something has gone wrong, and one row of a compiler's answer is a row of nothing.
    //
    // THEY TOOK THE FOOTER'S ROW and the footer is gone rather than shortened: it
    // said `[ Build ] press b`, which the header now says beside the two keys the catalog added,
    // and a panel that spends a row of a compiler's answer on repeating its own header is
    // spending the wrong row.
    //
    // ...AND WHILE THE PROJECT IS WAITING, THE `project` ROW HOLDS THE THIRD OF THEM
    //. The trade is argued where the row is painted, above. The rows are
    // PLACEHOLDERS here (empty text, `publish` wraps the detail into exactly the rows that
    // survive the budget, so the elision mark tells the truth about THIS face): the first
    // of them outlives the exit and command rows -- the compiler's own words are what a
    // maker acts on when something went wrong -- and the rest go first.
    const std::size_t said_max = 3 - shift;
    const std::int64_t said_priorities[3] = {5, 8, 9};
    for (std::size_t i = 0; i < said_max; ++i) {
        facts.push_back(Fact{std::string(), surface::role::kMuted, said_priorities[i]});
    }
    publish(std::move(facts), s.detail);
}

// ---- WHAT STATE ONE PANE IS IN -- the recovery invariant, as one word -----------------

const char* pane_state_word(std::int64_t state) {
    switch (state) {
    case pane_state::kUnresolved: return "unresolved";
    case pane_state::kRefused: return "refused";
    case pane_state::kWaiting: return "waiting";
    case pane_state::kOffRoom: return "off-room";
    case pane_state::kCovered: return "covered";
    case pane_state::kOpen: return "open";
    default: return "closed";
    }
}

// WL-PANE-10 -- agents/workshop/panes-and-windows.md
const char* pane_state_remedy(std::int64_t state) {
    switch (state) {
    case pane_state::kClosed: return "open it from the picker";
    case pane_state::kUnresolved: return "check the spelling, or the provider is not loaded";
    case pane_state::kRefused: return "reset its size, or open it on the other medium";
    case pane_state::kWaiting: return "make the window taller, or place it yourself";
    case pane_state::kOffRoom: return "reset its place";
    case pane_state::kCovered: return "raise it";
    default: return "";
    }
}

// WL-PANE-10 -- agents/workshop/panes-and-windows.md; WL-FRONT-05 -- agents/workshop/planes.md
bool pane_is_covered(const Panels& panels, const Setup& setup, const Screen& sc,
                     std::int64_t kind, const FineRect& mine) {
    if (mine.w <= 0 || mine.h <= 0) {
        return false; // nothing visible is OFF-ROOM, which is a different word
    }
    const std::vector<std::int64_t> order = effective_pane_order(setup, panels);
    std::size_t me = order.size();
    for (std::size_t i = 0; i < order.size(); ++i) {
        if (order[i] == kind) {
            me = i;
            break;
        }
    }
    if (me == order.size()) {
        return false;
    }
    std::vector<FineRect> ahead;
    for (std::size_t i = me + 1; i < order.size(); ++i) {
        const FineRect r = bounds_of(panels, setup, order[i], sc).rect;
        if (r.w > 0 && r.h > 0) {
            ahead.push_back(r);
        }
    }
    if (ahead.empty()) {
        return false;
    }
    // EXACT ON THE FINE LATTICE, BY EDGE COMPRESSION. The union of a handful of
    // rectangles is constant between their edges, so the question "is every sub-unit of
    // mine behind the union" needs one representative point per edge-bounded stripe —
    // never a walk of the lattice, which at this resolution would be forty-eight squared
    // points per cell of what used to be one. A pane peeking out by a single sub-unit
    // produces a stripe whose representative is visible, so a maker's sliver still means
    // `open` — one thing a maker can see is enough, exactly as it always was.
    std::vector<std::int64_t> xs{mine.x, surface::add_cells(mine.x, mine.w)};
    std::vector<std::int64_t> ys{mine.y, surface::add_cells(mine.y, mine.h)};
    for (const FineRect& r : ahead) {
        xs.push_back(r.x);
        xs.push_back(surface::add_cells(r.x, r.w));
        ys.push_back(r.y);
        ys.push_back(surface::add_cells(r.y, r.h));
    }
    std::sort(xs.begin(), xs.end());
    std::sort(ys.begin(), ys.end());
    const std::int64_t right = surface::add_cells(mine.x, mine.w);
    const std::int64_t bottom = surface::add_cells(mine.y, mine.h);
    for (std::size_t yi = 0; yi + 1 < ys.size(); ++yi) {
        const std::int64_t y = ys[yi];
        if (y < mine.y || y >= bottom || ys[yi + 1] == y) {
            continue;
        }
        for (std::size_t xi = 0; xi + 1 < xs.size(); ++xi) {
            const std::int64_t x = xs[xi];
            if (x < mine.x || x >= right || xs[xi + 1] == x) {
                continue;
            }
            bool hidden = false;
            for (const FineRect& r : ahead) {
                if (x >= r.x && x < surface::add_cells(r.x, r.w) && y >= r.y &&
                    y < surface::add_cells(r.y, r.h)) {
                    hidden = true;
                    break;
                }
            }
            if (!hidden) {
                return false; // one place a maker can see is enough
            }
        }
    }
    return true;
}

std::int64_t pane_state_of(const Panels& panels, const Setup& setup, const Screen& sc,
                           const CatalogRow& row) {
    if (!has_pane(setup, row.ref)) {
        return pane_state::kClosed;
    }
    if (row.kind == kNoPaneKind || !resolvable(row.ref, panels)) {
        return pane_state::kUnresolved;
    }
    // A UNIT OUTRANKS A WANT OF ROOM, and this is where that precedence is spent. A pane
    // with a pixel axis AND no tile left is refused rather than waiting: a taller window
    // would give it the tile and it still would not be presented, so telling the maker to
    // make the window taller would be a true sentence about the wrong problem.
    if (!pane_unit_projectable(pane_of(setup, row.ref))) {
        return pane_state::kRefused;
    }
    const PanelBounds where = bounds_of(panels, setup, row.kind, sc);
    if (!where.open) {
        // Named, resolved, projectable and not presented -- which is what `waiting` has
        // always meant here. `seat_panes` is the only thing that produces it and it is
        // medium-independent, which is why this branch does not consult one.
        return pane_state::kWaiting;
    }
    if (!where.projected) {
        return pane_state::kRefused;
    }
    if (where.rect.w <= 0 || where.rect.h <= 0) {
        return pane_state::kOffRoom;
    }
    if (pane_is_covered(panels, setup, sc, row.kind, where.rect)) {
        return pane_state::kCovered;
    }
    return pane_state::kOpen;
}

// WL-PED-01 -- agents/workshop/pane-manager.md
std::string picker_entry_text(const std::string& name, const char* state,
                              const std::string& tail) {
    return detail::pad(detail::fit(name, static_cast<std::int64_t>(kPickerNameCols)),
                       kPickerNameCols) +
           detail::pad(state, kPaneStateCols) + tail;
}

void paint_picker(surface::SurfaceLayer& layer, const Panels& panels, const Setup& setup,
                  const Screen& sc, const Keymap& keymap) {
    const PanelPicker& picker = panels.picker;
    if (!picker.open) {
        return;
    }
    const FineRect b = picker_bounds(sc);
    paint_panel_frame(layer, b, kTransientChrome);
    // THE PICKER IS ONE BOUNDED REGION OF PROSE, and the budget it spends is the
    // ACTIVE medium's row count rather than the slot's cell count. The two are the same
    // number in a character medium and they are not in one that sets real type -- nine cells
    // of slot is nine rows of a terminal and five rows of an 18-pixel face -- which is the
    // same pair of honest projections the Info panel's body has had.
    const PanelProsePlace place = panel_prose_place(b, sc);
    if (!place.present) {
        return; // a slot with no room for a row says nothing rather than lying about the room
    }
    surface::SurfaceTextRegion region = panel_prose_region(place);
    const auto say = [&region, &place](const std::string& text, std::int64_t role) {
        region.rows.push_back(
            surface::SurfaceTextRow{detail::fit(text, place.columns), role});
    };
    say("+ PANEL -- " + hotkey_text(keymap, Act::kPickerUp) + "/" +
            hotkey_text(keymap, Act::kPickerDown) + ", " +
            hotkey_text(keymap, Act::kPickerChoose) + " opens or removes",
        surface::role::kAccent);
    // THE POPULATION IS THE COMBINED CATALOG AND THE BUDGET IS THE SLOT'S.
    // Before this the list was `kPanelKinds` long and the picker's height was a
    // constant derived from it, which is a catalog census standing in for a
    // capacity -- it was right for exactly as long as no catalog could outgrow
    // the box, and a runtime offer is precisely a catalog that can. So the rows
    // under the heading are `list_window`'s to spend: the OBJECTS list's own
    // function, its own three rules and its own wording (`omitted_text`), which
    // is the second consumer the rule was established with and the fourth
    // overall. There is no second scrolling algorithm here and the picker did not
    // get taller.
    //
    // AND THE POPULATION IS THE SHARED INVENTORY -- the catalog UNION every
    // reference the setup names -- so a pane a maker authored and this build cannot resolve
    // has a row here too, and can be removed with the gesture that removes any other.
    const std::vector<CatalogRow> rows = inventory_rows(setup, panels);
    const std::size_t budget =
        place.rows > 1 ? static_cast<std::size_t>(place.rows - 1) : 0;
    const ListWindow win = list_window(rows.size(), picker.cursor, budget);
    if (win.before > 0) {
        say("  " + omitted_text(win.before, "earlier"), surface::role::kMuted);
    }
    for (std::size_t i = win.first; i < win.first + win.count; ++i) {
        const bool here = i == picker.cursor;
        say(std::string(here ? "> " : "  ") +
                picker_entry_text(rows[i].name,
                                  pane_state_word(pane_state_of(panels, setup, sc, rows[i])),
                                  rows[i].summary),
            here ? surface::role::kAccent : surface::role::kFill);
    }
    if (win.after > 0) {
        say("  " + omitted_text(win.after, "more"), surface::role::kMuted);
    }
    // THE REST OF THE SLOT IS THE REGION'S OWN EMPTINESS, and nobody writes it.
    // A region owns what is inside its bounds, so its cell projection already pads every row
    // it was not given -- the spaces that erase the panel underneath in a character medium are
    // `project_one_text_region`'s, and the graphical medium clears the same rectangle once
    // rather than a row at a time. What used to be a loop padding out to `b.h` is now the
    // primitive's contract, which is why this painter no longer has one. See kPickerRows for
    // why the whole slot is covered at all.
    layer.texts.push_back(std::move(region));
}

// ---- SAYING A PANE'S GEOMETRY IN THE FACE'S OWN LANGUAGE ------------------------------

const char* geometry_unit(std::int64_t cell_px) {
    return cell_px > 0 ? "px" : "cells";
}

// WL-GEO-09, WL-GEO-10 -- agents/workshop/geometry.md
GeometrySpelling geometry_spelling(std::int64_t subs, std::int64_t cell_px) {
    return GeometrySpelling{std::to_string(surface::device_of_subs(subs, cell_px)),
                            surface::subs_exact_in_device(subs, cell_px)};
}

std::string geometry_amount_text(std::int64_t subs, std::int64_t cell_px,
                                 bool& any_projected) {
    const GeometrySpelling spelled = geometry_spelling(subs, cell_px);
    if (spelled.exact) {
        return spelled.amount;
    }
    any_projected = true;
    return std::string(kProjectedMark) + spelled.amount;
}

FaceAmount parse_face_amount(std::string_view text, std::int64_t cell_px) {
    FaceAmount out;
    const auto trim = [](std::string_view v) {
        while (!v.empty() && v.front() == ' ') {
            v.remove_prefix(1);
        }
        while (!v.empty() && v.back() == ' ') {
            v.remove_suffix(1);
        }
        return v;
    };
    std::string_view body = trim(text);
    const std::string_view unit = geometry_unit(cell_px);
    const std::string_view other = cell_px > 0 ? "cells" : "px";
    if (body.size() > other.size() &&
        body.substr(body.size() - other.size()) == other) {
        out.refusal = "this face reads " + std::string(unit) + ", not " + std::string(other);
        return out;
    }
    if (body.size() > unit.size() && body.substr(body.size() - unit.size()) == unit) {
        body = trim(body.substr(0, body.size() - unit.size()));
    }
    const std::optional<std::int64_t> amount = TextForm<std::int64_t>::parse(body);
    if (!amount) {
        out.refusal = "not a whole number of " + std::string(unit) + " (`-` resets it)";
        return out;
    }
    out.accepted = true;
    out.subs = subs_of_device_amount(*amount, cell_px);
    return out;
}

std::string fine_rect_text(const FineRect& r, std::int64_t cell_px) {
    bool projected = false;
    std::string text = "@" + geometry_amount_text(r.x, cell_px, projected) + "," +
                       geometry_amount_text(r.y, cell_px, projected) + " " +
                       geometry_amount_text(r.w, cell_px, projected) + "x" +
                       geometry_amount_text(r.h, cell_px, projected) + " " +
                       geometry_unit(cell_px);
    if (projected) {
        text += kProjectedNote;
    }
    return text;
}

// WL-GEO-09 -- agents/workshop/geometry.md
std::string pane_window_text(const SetupPane* row, std::int64_t cell_px) {
    if (row == nullptr) {
        return "--";
    }
    bool projected = false;
    const auto axis = [cell_px, &projected](const PaneSize& s) -> std::string {
        if (s.mode == pane_unit::kSubcells) {
            return geometry_amount_text(s.amount, cell_px, projected);
        }
        if (s.mode == pane_unit::kPixels) {
            return std::to_string(s.amount) + "px";
        }
        return std::string("-");
    };
    std::string text;
    if (row->place.mode == pane_unit::kSubcells) {
        text += "@" + geometry_amount_text(row->place.x, cell_px, projected) + "," +
                geometry_amount_text(row->place.y, cell_px, projected) + " ";
    }
    text += axis(row->width) + "x" + axis(row->height);
    // THE UNIT IS SAID ONCE, AND ONLY WHERE A NUMBER IN IT WAS PRINTED. A row default
    // on every axis has said nothing measurable, and appending `cells` to `-x-` would
    // be naming the unit of a number that is not there.
    if (row->place.mode == pane_unit::kSubcells || row->width.mode == pane_unit::kSubcells ||
        row->height.mode == pane_unit::kSubcells) {
        text += " " + std::string(geometry_unit(cell_px));
    }
    text += " f" + std::to_string(row->front);
    if (projected) {
        text += kProjectedNote;
    }
    return text;
}

// WL-GEO-12 -- agents/workshop/geometry.md
bool pane_window_partly_default(const SetupPane* row) {
    if (row == nullptr) {
        return false;
    }
    return row->place.mode == pane_unit::kDefault || row->width.mode == pane_unit::kDefault ||
           row->height.mode == pane_unit::kDefault;
}

// ---- A SURFACE SIZED BY WHAT IT SAYS, PLACED ---------------------------------------------

// WL-CTX-03 -- agents/workshop/contextual.md; WL-KEY-10 -- agents/workshop/keyboard.md
FineRect popup_bounds_at(std::int64_t want_cols, std::int64_t want_rows,
                         std::int64_t x, std::int64_t y, const Screen& sc) {
    const surface::RegionCells cells =
        surface::region_cells_for(want_cols, want_rows, sc.text_advance_px, sc.text_line_px);
    const ui::Rect outer = chrome_outer_of(0, 0, cells.w, cells.h);
    const std::int64_t floor_y = kWorkspaceY + sc.room_h;
    const std::int64_t w = outer.w > sc.w ? sc.w : outer.w;
    const std::int64_t room_rows = floor_y - kStackY;
    const std::int64_t h = outer.h > room_rows ? room_rows : outer.h;
    if (x + w > sc.w) {
        x = sc.w - w;
    }
    if (x < 0) {
        x = 0;
    }
    if (y + h > floor_y) {
        y = floor_y - h;
    }
    if (y < kStackY) {
        y = kStackY;
    }
    return fine_of_cells(ui::Rect{x, y, w, h});
}

} // namespace zengine::workshop
