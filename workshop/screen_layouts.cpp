// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `screen.hpp`'s sections -- the setup line, the setup slot, the layout tabs, and
// the layouts pane with the bottom band -- compiled once into `zengine-workshop-logic` and linked
// by the host and every suite; the declarations, the constants and the constexpr functions stay
// in the header.
// Workshop law: agents/workshop/tab-run.md (+5 registers; agents/workshop.md routes)

#include "screen.hpp"

namespace zengine::workshop {

// ---- THE SETUP LINE: which arrangement this is, and whether it is written down -----------

std::string setup_name_hint(const Keymap& k) {
    return "  " + hotkey_text(k, Act::kNamingCommit) + " renames  " +
           hotkey_text(k, Act::kNamingCancel) + " cancels";
}

// WL-KEY-02 -- agents/workshop/keyboard.md
std::string setup_hints(const Keymap& k) {
    return hotkey_text(k, Act::kSetupSave) + " save  " +
           hotkey_text(k, Act::kSetupRestore) + " restore";
}

// WL-TAB-01, WL-TAB-05 -- agents/workshop/tab-run.md
// WL-CHROME-05 -- agents/workshop/chrome.md
// WL-PRESS-05 -- agents/workshop/press-chain.md
ExternalBodyPlace layouts_body(const Session& s, const Screen& sc) {
    const PanelBounds where = bounds_of(s.panels, s.setup.active, panel::kLayouts, sc);
    if (!where.open) {
        return ExternalBodyPlace{};
    }
    return external_body_place(where.rect, sc, 0);
}

// WL-TEXT-03 -- agents/workshop/text-box.md
std::int64_t setup_name_columns(const Session& s, const Screen& sc) {
    const std::int64_t chrome =
        static_cast<std::int64_t>(std::char_traits<char>::length(kSetupNamePrompt)) +
        static_cast<std::int64_t>(setup_name_hint(s.keymap).size()) + 1;
    const std::int64_t room = layouts_body(s, sc).columns - chrome;
    return room > kSetupNameMinCols ? room : kSetupNameMinCols;
}

// ---- THE `setup:` SLOT: what the ACTIVE layout's association is --------------------------

// WL-TAB-02, WL-TAB-03 -- agents/workshop/tab-run.md
std::string setup_link_text(const SetupState& setup, std::int64_t path_columns) {
    const std::int64_t status = link_status(setup.active, setup.active_link);
    std::string line = kSetupSlot;
    if (status == setup_link::kNone) {
        line += kSetupLinkNone;
        return line;
    }
    line += detail::fit_path(setup.active_link.path, path_columns);
    line += kStatusJoin;
    line += status == setup_link::kCurrent ? kSetupLinkCurrent : kSetupLinkModified;
    return line;
}

// WL-MAKER-04 -- agents/workshop/maker-pane.md; WL-TAB-03 -- agents/workshop/tab-run.md
std::string setup_rest_text(const SetupState& setup, const Panels& panels,
                            const Keymap& keymap) {
    std::string line;
    // THE SESSION'S WHOLE RESOLUTION TABLE IS ASKED, AND THIS IS THE LINE THAT MADE IT A
    // REQUIRED ARGUMENT (the maker-made pane joined the table later). A pane a
    // maker can SEE must not be counted as unresolved on the row directly beneath it, and
    // the built-in-only resolver would have said exactly that about every admitted external
    // offer -- silently, and only in the configuration where somebody had actually loaded
    // a provider.
    const std::vector<PaneRef> waiting = unresolved_panes(setup.active, panels);
    if (!waiting.empty()) {
        // UNRESOLVED, NEVER UNAVAILABLE. Workshop knows that it cannot present these
        // references; it knows nothing whatever about whoever could, and a word implying
        // otherwise would be a claim made out of silence.
        line += kStatusJoin + std::to_string(waiting.size()) + " unresolved";
    }
    line += kStatusJoin;
    line += setup_hints(keymap);
    return line;
}

std::string workspace_text(const Session& s) {
    return "workspace " + std::to_string(s.workspace_w) + "x" +
           std::to_string(s.workspace_h) + " cells";
}

// ---- THE LAYOUT TABS: the left of the status row -----------------------------------------

// WL-TAB-08 -- agents/workshop/tab-run.md
std::string layouts_omitted_text(std::size_t how_many, bool ahead) {
    return ahead ? " " + std::to_string(how_many) + ">" : "<" + std::to_string(how_many);
}

std::string layout_tab_text(const SetupState& setup, std::size_t at) {
    const bool live = at == setup.active_at;
    std::string tab;
    tab += live ? kLayoutLiveOpen : kLayoutTabPad;
    tab += layout_at(setup, at).name;
    tab += live ? kLayoutLiveClose : kLayoutTabPad;
    return tab;
}

std::int64_t layout_tab_columns(std::int64_t row_columns) noexcept {
    const std::int64_t room = row_columns - kSetupStatusCols;
    return room > kLayoutTabMinCols ? room : kLayoutTabMinCols;
}

// WL-TAB-05, WL-TAB-08 -- agents/workshop/tab-run.md
LayoutTabRun layout_tab_run(const SetupState& setup, std::int64_t columns) {
    LayoutTabRun run;
    const std::size_t n = layout_count(setup);
    if (columns <= 0) {
        run.after = n; // no room at all: everything there is, is missing
        return run;
    }
    const std::size_t live = setup.active_at < n ? setup.active_at : 0;
    std::vector<std::string> text;
    text.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        text.push_back(layout_tab_text(setup, i));
    }
    // The cost of painting `[first, last]` -- the tabs plus whichever markers that window
    // would need. Asked afresh for every candidate window, because taking one more tab can
    // RETIRE the marker on that side and pay for itself.
    const auto cost = [&](std::size_t first, std::size_t last) {
        std::int64_t total = 0;
        for (std::size_t i = first; i <= last; ++i) {
            total += static_cast<std::int64_t>(text[i].size());
        }
        if (first > 0) {
            total += static_cast<std::int64_t>(layouts_omitted_text(first, false).size());
        }
        if (last + 1 < n) {
            total += static_cast<std::int64_t>(layouts_omitted_text(n - last - 1, true).size());
        }
        return total;
    };
    std::size_t first = live;
    std::size_t last = live;
    bool rightward = true;
    for (bool grew = true; grew;) {
        grew = false;
        // Two chances a round -- the preferred side, then the other -- so a window that has
        // run out of room on one side keeps growing on the other, and the preference
        // alternates so the live layout ends up inside the run rather than pinned to an end.
        for (int tries = 0; tries < 2 && !grew; ++tries) {
            if (rightward) {
                if (last + 1 < n && cost(first, last + 1) <= columns) {
                    ++last;
                    grew = true;
                }
            } else if (first > 0 && cost(first - 1, last) <= columns) {
                --first;
                grew = true;
            }
            rightward = !rightward;
        }
    }
    run.before = first;
    run.after = n - last - 1;
    // BOTH MARKERS ARE PAID FOR OUT OF THE SAME BUDGET AS THE TABS, and each is RESERVED
    // before a tab is written rather than appended after them. Rule 3 says an omission spends
    // columns of this budget, and a marker written once the budget was already gone would be
    // a bound that grows when it is exceeded -- the exact thing the rule refuses.
    //
    // AND RULE 2 OUTRANKS RULE 3 AT THE BOTTOM OF THE RANGE. Where the room will not hold a
    // marker AND something of the live layout, the marker is not written: which layout is
    // live is what a maker cannot do without, and a run that spent its last cells saying how
    // many it could not show would have stopped answering the question it exists for. The
    // COUNTS are still on the answer (`before`/`after`) whatever the text could carry.
    const std::string head =
        run.before > 0 ? layouts_omitted_text(run.before, false) : std::string();
    const std::string tail =
        run.after > 0 ? layouts_omitted_text(run.after, true) : std::string();
    const std::int64_t head_cost =
        static_cast<std::int64_t>(head.size()) < columns
            ? static_cast<std::int64_t>(head.size())
            : 0;
    const std::int64_t tail_cost =
        head_cost + static_cast<std::int64_t>(tail.size()) < columns
            ? static_cast<std::int64_t>(tail.size())
            : 0;
    if (head_cost > 0) {
        run.text += head;
    }
    for (std::size_t i = first; i <= last; ++i) {
        LayoutTab tab;
        tab.at = i;
        tab.active = i == live;
        tab.column = static_cast<std::int64_t>(run.text.size());
        // THE LIVE TAB IS CUT RATHER THAN DROPPED where even it alone cannot fit, because
        // "which layout am I in" is the one thing this run may not stop saying (rule 2).
        // `detail::fit` marks the cut, and the span recorded is what was actually written.
        std::string shown = text[i];
        if (tab.column + static_cast<std::int64_t>(shown.size()) > columns - tail_cost) {
            shown = detail::fit(std::move(shown), columns - tail_cost - tab.column);
        }
        tab.columns = static_cast<std::int64_t>(shown.size());
        run.text += shown;
        run.tabs.push_back(tab);
    }
    if (tail_cost > 0) {
        run.text += tail;
    }
    //...AND THE CREATE AFFORDANCE OUT OF WHAT IS GENUINELY LEFT. Last, and out of
    // the same budget: an affordance written once the budget was already gone would be the
    // bound-that-grows rule 3 refuses, and it would push the association's own reservation
    // off a narrow row to advertise a key that still works.
    const std::int64_t written = static_cast<std::int64_t>(run.text.size());
    if (written + kLayoutCreateCols <= columns) {
        run.create_column = written + 1; // the pad cell belongs to the gap, not to the mark
        run.create_columns = 1;
        run.text += kLayoutTabPad;
        run.text += kLayoutCreate;
    }
    return run;
}

BandStatus band_status(const Session& s, const ExternalBodyPlace& place) {
    BandStatus out;
    if (!place.present) {
        return out;
    }
    const LayoutTabRun run = layout_tab_run(s.setup, layout_tab_columns(place.columns));
    out.before = run.before;
    out.after = run.after;
    const std::int64_t left = static_cast<std::int64_t>(run.text.size());
    std::string rest = setup_rest_text(s.setup, s.panels, s.keymap);
    // THE WORKSPACE FACT FOLDS IN WHERE THE TOP BAND HAS NO SECOND ROW FOR IT -- the band's
    // fold, unchanged in kind and re-measured against the band it is now on. A
    // character medium gives the fact its own row; the shipped face's single row carries
    // both. It folds into the CUTTABLE half, because a room's size is the one fact here a
    // maker can also read by looking at their window.
    if (place.rows < 2) {
        rest += kStatusJoin + workspace_text(s);
    }
    // WHAT IS LEFT FOR THE ARTIFACT'S NAME: what the tabs did not take, less the words of
    // the sentence, less everything that follows it.
    //
    // ⚠ THE PATH IS THE PART THAT SHRINKS, AND IT SHRINKS FOR THE WHOLE ROW. Taking the
    // remainder for the path alone reads as generous and starves the dynamic truth behind
    // it: at the 78-column minimum a real temporary path swallowed every cell after the
    // verdict, and a maker with an unresolved pane stopped being told so -- measured by the
    // suite, not reasoned about. So the path yields to the unresolved count and to the two
    // gestures as well, and only what THEN does not fit is cut from the right, which is the
    // ordering §9 asks for: the verdict is reserved, the tail degrades, the path absorbs.
    const std::int64_t path_columns = place.columns - left - kSetupStatusCols + kElidedCols -
                                      static_cast<std::int64_t>(rest.size());
    std::string standing = setup_link_text(
        s.setup, path_columns > kElidedCols ? path_columns : kElidedCols);
    // THE STATUS IS RIGHT-ADJUSTED WHERE THERE IS ROOM TO ADJUST IT. The run is the
    // row's left and the status is its right, so the gap between them is the row's own slack
    // -- which pins the association to the screen's edge instead of letting it drift with
    // however many tabs happen to exist. Combined with equal-width marker, that
    // makes the right-hand sentence perfectly still: neither switching layouts nor adding
    // one moves a cell of it while the row still fits.
    std::string line = run.text;
    const std::int64_t joined =
        left + kStatusJoinCols + static_cast<std::int64_t>(standing.size() + rest.size());
    if (joined < place.columns) {
        line.append(static_cast<std::size_t>(place.columns - joined + kStatusJoinCols), ' ');
    } else {
        line += kStatusJoin;
    }
    line += standing;
    line += rest;
    out.text = detail::fit(std::move(line), place.columns);
    // A SPAN THE ROW'S OWN CUT REMOVED IS NOT A TAB ANY MORE. The reservation above makes
    // this unreachable at every honest extent -- the tabs are composed against the row less
    // the association's own room -- and it is written anyway, because a span that outlived
    // the bytes it describes is exactly the stale geometry a press must never be answered
    // from. The create affordance is judged by the same rule and for the same reason.
    const std::int64_t painted = static_cast<std::int64_t>(out.text.size());
    for (const LayoutTab& tab : run.tabs) {
        if (tab.column + tab.columns <= painted) {
            out.tabs.push_back(tab);
        }
    }
    if (run.create_columns > 0 && run.create_column + run.create_columns <= painted) {
        out.create_column = run.create_column;
        out.create_columns = run.create_columns;
    }
    return out;
}

BandStatus band_status(const Session& s, const Screen& sc) {
    return band_status(s, layouts_body(s, sc));
}

std::int64_t band_tab_row(const Session& s, const Screen& sc) {
    if (!layouts_body(s, sc).present || s.setup.naming.open) {
        return kNoBandRow;
    }
    // THE IDENTITY IS THE PANE'S FIRST ROW WHENEVER THE PANE HAS ONE (re-homed when the band
    // became a pane). It used to share a band with the notice, so at a one-row budget the tool's
    // voice outranked it and there was no tab row at all; the notice lives at the foot now,
    // and nothing in this pane can displace the selector but the name editor taking its row.
    return 0;
}

LayoutTabPress band_tab_at(const Session& s, const Screen& sc, std::int64_t space,
                           std::int64_t x, std::int64_t y) {
    const std::int64_t row = band_tab_row(s, sc);
    if (row == kNoBandRow) {
        return {};
    }
    // ⚠ THE PRESS IS RESOLVED AGAINST THE RECTANGLE THE TABS ARE PAINTED IN, which since
    // the conversion is the Layouts pane's INTERIOR -- the maker's authored place and size, less
    // its chrome. A stale origin here would answer a press at the rectangle the band used
    // to own and ignore the row a maker can actually see, which is the same one-row lie
    // the split made unsayable at the other end and the reason the origin is taken from the
    // same `layouts_body` the painter publishes at.
    //
    // AND THIS IS A PANE-LOCAL INVERSE NOW, NOT A GLOBAL QUESTION. Nothing calls it until
    // ordinary occupancy has already answered `Layouts` for the point, so a pane authored
    // in front of this one takes the press before this arithmetic is ever spent.
    const ExternalBodyPlace place = layouts_body(s, sc);
    const ProseAt at = prose_at(space, x, y, place.region_x, place.region_y, place.fit);
    if (!at.understood || at.row != row) {
        return {};
    }
    const BandStatus band = band_status(s, place);
    for (const LayoutTab& tab : band.tabs) {
        if (at.column >= tab.column && at.column < tab.column + tab.columns) {
            return LayoutTabPress{true, tab.at, false};
        }
    }
    if (band.create_columns > 0 && at.column >= band.create_column &&
        at.column < band.create_column + band.create_columns) {
        return LayoutTabPress{true, 0, true};
    }
    return {};
}

// ---- THE LAYOUTS PANE AND THE BOTTOM BAND, EACH COMPOSED AGAINST ITS BUDGET ---------------

// WL-TAB-01 -- agents/workshop/tab-run.md
// WL-TAB-01, WL-TAB-05 -- agents/workshop/tab-run.md
void paint_layouts(surface::SurfaceLayer& layer, const Session& s, const FineRect& b,
                   const Screen& sc, std::int64_t chrome) {
    paint_panel_frame(layer, b, chrome);
    const ExternalBodyPlace place = external_body_place(b, sc, 0);
    surface::SurfaceTextRegion band;
    band.x = place.region_x;
    band.y = place.region_y;
    band.w = place.region_w;
    band.h = place.region_h;
    band.sub_x = place.region_sub_x;
    band.sub_y = place.region_sub_y;
    band.sub_w = place.region_sub_w;
    band.sub_h = place.region_sub_h;
    const std::int64_t budget = place.rows;
    const std::int64_t columns = place.columns;
    if (!place.present) {
        return; // no room for one row of this medium's type: say nothing at all
    }

    const bool naming = s.setup.naming.open;
    std::string identity;
    std::int64_t caret_col = surface::kNoCaret;
    std::int64_t sel_begin = 0;
    std::int64_t sel_end = 0;
    if (naming) {
        const std::int64_t cols = setup_name_columns(s, sc);
        const std::string shown = s.setup.naming.line.visible(cols);
        const component::TextBox::VisibleSpan vis =
            s.setup.naming.line.visible_selection(cols);
        const std::int64_t prompt =
            static_cast<std::int64_t>(std::char_traits<char>::length(kSetupNamePrompt));
        const std::int64_t at =
            static_cast<std::int64_t>(s.setup.naming.line.caret_column());
        caret_col = prompt + (at < static_cast<std::int64_t>(shown.size())
                                  ? at
                                  : static_cast<std::int64_t>(shown.size()));
        if (vis.present()) {
            sel_begin = prompt + vis.begin;
            sel_end = prompt + vis.end;
        }
        identity = detail::fit(std::string(kSetupNamePrompt) + shown +
                                   setup_name_hint(s.keymap),
                               columns);
    } else {
        // THE LAYOUT TABS AND THE STATUS ARE ONE COMPOSITION, and the painter takes
        // it whole -- the workspace fold and the row's own cut included, so the spans
        // `band_tab_at` answers a press from are the spans that were written here. It is
        // composed against THE PLACE THIS PAINTER RESOLVED rather than against a
        // second reading of the session's own geometry: one rectangle in, one row out.
        identity = band_status(s, place).text;
    }

    band.rows.push_back(surface::SurfaceTextRow{std::move(identity), surface::role::kMuted});
    if (caret_col != surface::kNoCaret) {
        band.caret_row = 0;
        band.caret_col = caret_col;
    }
    if (sel_end > sel_begin) {
        band.sel_begin_row = 0;
        band.sel_begin_col = sel_begin;
        band.sel_end_row = 0;
        band.sel_end_col = sel_end;
    }
    // THE WORKSPACE FACT GETS ITS OWN ROW WHERE THERE IS ONE, and folds into the identity
    // row where there is not (`band_status`). It yields to a name being typed for the reason
    // it has always yielded: a maker mid-name is reading their own words.
    if (budget >= 2 && !naming) {
        band.rows.push_back(
            surface::SurfaceTextRow{detail::fit(workspace_text(s), columns),
                                    surface::role::kMuted});
    }
    layer.texts.push_back(std::move(band));
}

} // namespace zengine::workshop
