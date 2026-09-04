// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `screen.hpp`'s sections -- the Info panel's body, its action controls, and one
// windowed list's rows -- compiled once into `zengine-workshop-logic` and linked by the host and
// every suite; the declarations, the constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/info-body.md (+6 registers; agents/workshop.md routes)

#include "screen.hpp"

namespace zengine::workshop {

// ---- The Info panel's BODY, resolved ONCE ----

BodyShare share_body_rows(std::size_t budget, std::size_t want_objects,
                          std::size_t want_properties) {
    BodyShare s;
    if (budget == 0) {
        return s;
    }
    if (want_properties <= budget && want_objects <= budget - want_properties) {
        s.objects = want_objects; // both whole; the rest of the body stays spare
        s.properties = want_properties;
        return s;
    }
    const std::size_t half = budget / 2;
    if (want_objects <= half) {
        s.objects = want_objects; // it needs less than its share, so it takes what it needs
        s.properties = budget - s.objects;
    } else if (want_properties <= budget - half) {
        s.properties = want_properties;
        s.objects = budget - s.properties;
    } else {
        s.objects = half; // both want more than half: the contested room is shared
        s.properties = budget - half;
    }
    return s;
}

// ---- The Info panel's ACTION CONTROLS -----------------------------------------------------

// WL-CTRL-03 -- agents/workshop/info-controls.md; WL-PED-07 -- agents/workshop/pane-manager.md
bool draft_live(const Session& s) {
    for (const Row& row : s.rows) {
        if (row.editing()) {
            return true;
        }
    }
    return false;
}

// WL-CTRL-03 -- agents/workshop/info-controls.md
Availability action_availability(std::size_t which, const WorkshopDoc& d,
                                 const Session& s) {
    return action_availability(which, draft_live(s), doc::find(d, s.selected) != nullptr);
}

// WL-CTRL-04 -- agents/workshop/info-controls.md
std::string action_row_text(std::size_t which, bool pressable, std::int64_t columns) {
    const std::string open = pressable ? "[ " : "( ";
    const std::string close = pressable ? " ]" : " )";
    return detail::fit(open + action_label(which) + close, columns);
}

// WL-INFO-04 -- agents/workshop/info-body.md
std::size_t inspector_focus(const Session& s) {
    for (std::size_t i = 0; i < s.rows.size(); ++i) {
        if (s.rows[i].editing()) {
            return i;
        }
    }
    return s.cursor;
}

// WL-INFO-01, WL-INFO-08 -- agents/workshop/info-body.md
// WL-CHROME-05 -- agents/workshop/chrome.md
// WL-CTRL-01 -- agents/workshop/info-controls.md
InfoBodyPlace info_body_place(const FineRect& outer, const Screen& sc,
                              std::size_t total_objects, std::size_t selected_at,
                              std::size_t total_properties, std::size_t focus) {
    InfoBodyPlace p;
    const PaneInside inside = pane_inside(outer, sc);
    const FineRect panel = inside.rect;
    if (panel.w <= 0 || panel.h <= 0) {
        return p; // no panel at all, or none left inside its own chrome
    }
    // THE REGION IS THE WHOLE PANEL AND THE `OBJECTS` HEADING IS ITS FIRST PROSE ROW
    // -- `external_body_place`'s ordering exactly, and for its reason: the heading
    // is reserved out of the PROSE budget before either list is offered anything, so the
    // body's own rows still begin at zero and a press on the heading names nothing. In a
    // character medium one prose row is one cell row and the arithmetic is byte-for-byte
    // what `kInfoBodyY = 1` used to subtract; in a medium that sets type, the heading is a
    // row of that type like everything under it. The region fields and the fit are filled
    // for every non-empty panel -- present or not -- because the heading is sayable in a
    // panel whose body is not seatable, and the painter must not resolve the rectangle a
    // second time to say it.
    const surface::SurfaceRect wire = wire_rect_of(panel, surface::role::kFill);
    p.region_x = wire.x;
    p.region_y = wire.y;
    p.region_w = wire.w;
    p.region_h = wire.h;
    p.region_sub_x = wire.sub_x;
    p.region_sub_y = wire.sub_y;
    p.region_sub_w = wire.sub_w;
    p.region_sub_h = wire.sub_h;
    p.fit = inside.fit;
    p.columns = p.fit.columns;
    const std::int64_t used = kPropertyMarkCols + kPropertyLabelCols;
    if (surface::cell_of_subs(surface::add_cells(panel.x, panel.w)) -
            surface::cell_of_subs(panel.x) <=
        used) {
        return p; // no room for a value beside a name
    }
    p.value_columns = p.fit.columns - used - kPropertyCaretCols;
    if (p.value_columns < 0) {
        p.value_columns = 0;
    }
    p.capacity = p.fit.rows > kInfoHeadingRows
                     ? static_cast<std::size_t>(p.fit.rows - kInfoHeadingRows)
                     : 0;
    if (p.capacity < kInfoBodyMinRows + kActionRows) {
        return p; // not enough to seat a row of each list, the heading, and the controls
    }
    // ONE ROW OFF THE TOP FOR THE HEADING AND `kActionRows` OFF THE FOOT FOR THE CONTROLS,
    // before either list is offered anything. Both are chrome and both are bought at the same
    // price as a row of material, which is the same rule `list_window` follows for its own
    // markers: a bound that grows when it is exceeded is not a bound.
    //
    // THE WHOLE COMPOSITION POLICY IS THIS ONE SUBTRACTION AND THE ONE CALL UNDER IT.
    // The controls are a FIXED demand and the lists are VARIABLE ones, so they are not three
    // claimants on `share_body_rows`: sharing is what two parties do when they both want more
    // than there is, and a control wants exactly one row at every size this panel has. Giving
    // the footer a share would have made it grow into a tall panel's spare room for no reason
    // anybody could state. So the fixed demand comes off the top of the budget and the
    // variable ones share what is left -- and every property the suite pinned survives it, because
    // a budget reduced by a constant is still a budget: growing the panel still grows both
    // shares, a list that fits still gets exactly what it needs, and spare room is still spare.
    //
    // There is no `-2 for buttons` anywhere else in this file. This line is the reservation,
    // `action_row` below is where the reserved rows are, and the painter and the press both
    // ask for that number rather than recomputing it.
    const BodyShare share = share_body_rows(p.capacity - 1 - kActionRows,
                                            list_demand(total_objects),
                                            list_demand(total_properties));
    p.objects_rows = share.objects;
    p.properties_rows = share.properties;
    p.objects = list_window(total_objects, selected_at, p.objects_rows);
    p.properties = list_window(total_properties, focus, p.properties_rows);
    p.heading_row = static_cast<std::int64_t>(p.objects_rows);
    p.action_row = static_cast<std::int64_t>(p.capacity - kActionRows);
    p.present = true;
    return p;
}

InfoBodyPlace info_body_place(const FineRect& panel, const Screen& sc,
                              const WorkshopDoc& d, const Session& s) {
    return info_body_place(panel, sc, d.elements.size(), position_of(d, s.selected),
                           s.rows.size(), inspector_focus(s));
}

InfoBodyPlace info_body_place(const ui::Rect& panel, const Screen& sc,
                              std::size_t total_objects, std::size_t selected_at,
                              std::size_t total_properties, std::size_t focus) {
    return info_body_place(fine_of_cells(panel), sc, total_objects, selected_at,
                           total_properties, focus);
}

InfoBodyPlace info_body_place(const ui::Rect& panel, const Screen& sc,
                              const WorkshopDoc& d, const Session& s) {
    return info_body_place(fine_of_cells(panel), sc, d, s);
}

// WL-FOCUS-03 -- agents/workshop/focus.md
// WL-INFO-08 -- agents/workshop/info-body.md
// WL-PRESS-03 -- agents/workshop/press-chain.md
InfoBodyAt info_body_at(const WorkshopDoc& d, const Session& s, std::int64_t space,
                        std::int64_t x, std::int64_t y) {
    const Screen sc = screen_of(s);
    const PanelBounds info = bounds_of(s.panels, s.setup.active, panel::kInfo, sc);
    if (!info.open) {
        return InfoBodyAt{};
    }
    InfoBodyAt where;
    where.body = info_body_place(info.rect, sc, d, s);
    where.at = prose_at(space, x, y, where.body.region_x, where.body.region_y, where.body.fit);
    // THE HEADING ROW IS SUBTRACTED HERE BECAUSE IT WAS RESERVED THERE.
    // `info_body_place` keeps `kInfoHeadingRows` out of the body's budget before either
    // list is offered anything, so the row a handler means by 0 is the region's prose row
    // 1 -- doing that subtraction in one direction and forgetting it in the other is
    // precisely the off-by-one `external_press_at` already guards against, one panel over.
    // A press ON the heading names no body row and falls to the panel's occupancy answer.
    where.at.row -= kInfoHeadingRows;
    where.present = where.body.present && where.at.understood && where.at.row >= 0;
    return where;
}

// ---- One windowed list's rows, mapped both ways ------------------------------------------

// WL-INFO-04 -- agents/workshop/info-body.md
// WL-INFO-04 -- agents/workshop/info-body.md
std::int64_t prose_row_in_window(const ListWindow& w, std::int64_t first_row,
                                 std::size_t index) {
    if (index < w.first || index - w.first >= w.count) {
        return kNoProseRow;
    }
    return first_row + static_cast<std::int64_t>(index - w.first) + (w.before > 0 ? 1 : 0);
}

bool item_at_prose_row(const ListWindow& w, std::int64_t first_row, std::size_t rows,
                       std::int64_t row, std::size_t& out) {
    if (row < first_row || row >= first_row + static_cast<std::int64_t>(rows)) {
        return false;
    }
    const std::int64_t at = row - first_row - (w.before > 0 ? 1 : 0);
    if (at < 0 || at >= static_cast<std::int64_t>(w.count)) {
        return false; // an omission marker, or past the last item shown
    }
    out = w.first + static_cast<std::size_t>(at);
    return true;
}

std::int64_t prose_row_of_object(const InfoBodyPlace& p, std::size_t index) {
    return p.present ? prose_row_in_window(p.objects, 0, index) : kNoProseRow;
}

std::size_t object_at_prose_row(const InfoBodyPlace& p, std::int64_t row) {
    std::size_t at = 0;
    if (!p.present || !item_at_prose_row(p.objects, 0, p.objects_rows, row, at)) {
        return kNoObject;
    }
    return at;
}

std::int64_t prose_row_of_property(const InfoBodyPlace& p, std::size_t index) {
    if (!p.present) {
        return kNoProseRow;
    }
    return prose_row_in_window(p.properties, p.heading_row + 1, index);
}

std::size_t property_at_prose_row(const InfoBodyPlace& p, std::int64_t row) {
    std::size_t at = 0;
    if (!p.present ||
        !item_at_prose_row(p.properties, p.heading_row + 1, p.properties_rows, row, at)) {
        return kNoProperty;
    }
    return at;
}

// WL-CTRL-02 -- agents/workshop/info-controls.md
std::int64_t prose_row_of_action(const InfoBodyPlace& p, std::size_t which) {
    if (!p.present || which >= kActionCount) {
        return kNoProseRow;
    }
    return p.action_row + static_cast<std::int64_t>(which);
}

std::size_t action_at_prose_row(const InfoBodyPlace& p, std::int64_t row) {
    if (!p.present || row < p.action_row ||
        row >= p.action_row + static_cast<std::int64_t>(kActionRows)) {
        return kNoAction;
    }
    return static_cast<std::size_t>(row - p.action_row);
}

// WL-PRESS-03 -- agents/workshop/press-chain.md
std::size_t action_press_at(const InfoBodyPlace& p, std::int64_t column,
                            std::int64_t row) {
    if (!p.present || column < 0 || column > p.fit.columns) {
        return kNoAction;
    }
    return action_at_prose_row(p, row);
}

// WL-INFO-09 -- agents/workshop/info-body.md
std::string object_row_full(const ui::Element& e, bool chosen) {
    return std::string(chosen ? "> " : "  ") + "#" + std::to_string(e.id) + " " + e.label;
}

std::string object_row_text(const ui::Element& e, bool chosen, std::int64_t columns) {
    return detail::fit(object_row_full(e, chosen), columns);
}

// WL-INFO-05 -- agents/workshop/info-body.md
std::string property_row_prefix(const Row& row, bool here) {
    return std::string(here ? ">" : " ") +
           detail::pad(row.label(), static_cast<std::size_t>(kPropertyLabelCols));
}

std::string property_row_full(const Row& row, bool here) {
    return property_row_prefix(row, here) + row.value();
}

std::string property_row_text(const Row& row, bool here, std::int64_t value_columns) {
    std::string text = property_row_prefix(row, here);
    if (row.editing()) {
        return text + row.editor().visible(value_columns);
    }
    return text + detail::fit(row.value(), value_columns);
}

// WL-TEXT-13 -- agents/workshop/text-box.md
std::int64_t property_caret_column(const Row& row) {
    return kPropertyMarkCols + kPropertyLabelCols +
           static_cast<std::int64_t>(row.editor().caret_column());
}

// WL-TEXT-13 -- agents/workshop/text-box.md
TerminalSelectionSpan property_selection_columns(const Row& row,
                                                 std::int64_t value_columns) {
    const component::TextBox::VisibleSpan vis = row.editor().visible_selection(value_columns);
    if (!vis.present()) {
        return TerminalSelectionSpan{};
    }
    return TerminalSelectionSpan{kPropertyMarkCols + kPropertyLabelCols + vis.begin,
                                 kPropertyMarkCols + kPropertyLabelCols + vis.end, true};
}

// WL-PRESS-03 -- agents/workshop/press-chain.md
bool property_row_hit(const InfoBodyPlace& p, std::size_t index, std::int64_t column,
                      std::int64_t row) {
    return p.present && column >= 0 && column <= p.fit.columns &&
           property_at_prose_row(p, row) == index && index != kNoProperty;
}

// WL-INFO-09 -- agents/workshop/info-body.md; WL-PRESS-03 -- agents/workshop/press-chain.md
std::size_t object_press_at(const InfoBodyPlace& p, std::int64_t column,
                            std::int64_t row) {
    if (!p.present || column < 0 || column > p.fit.columns) {
        return kNoObject;
    }
    return object_at_prose_row(p, row);
}

// WL-INFO-01, WL-INFO-08 -- agents/workshop/info-body.md
void paint_info(surface::SurfaceLayer& layer, const WorkshopDoc& d, const Session& s,
                const FineRect& b, const Screen& sc,
                std::int64_t chrome) {
    // THE BACKDROP FIRST, so everything below is written over it and nothing authored
    // survives underneath it. One rect, the whole of `b`, and the same call the other two
    // presentations make -- and the part of it the body does not cover is this
    // panel's visible boundary.
    paint_panel_frame(layer, b, chrome);

    // THE BODY IS ONE BOUNDED REGION AND IT HOLDS BOTH LISTS.
    //
    // Everything under `OBJECTS` belongs to it: how many object names there are, where
    // `PROPERTIES` falls, how many properties there are, how wide a value may be, where the
    // caret is, and what neither list is showing. A region is the only shape on this canvas
    // that can be set in the active medium's own type and the only one that can carry an
    // insertion point, and both lists want the first.
    //
    // NOTHING BELOW MULTIPLIES A FONT METRIC. `info_body_place` asked `fit_region` once; the
    // loops spend `objects`/`properties`, `columns` and `value_columns` and know nothing about
    // pixels, faces, insets or line heights. That is what makes "the graphical body shows
    // eleven objects and the terminal body shows twenty" one publisher rather than two.
    //
    // AND NO ROW IS PAINTED THAT THE BODY CANNOT HOLD. Earlier the property loop ran over
    // every property and wrote a label per row, so a population taller than the panel ran off
    // its bottom edge; earlier the object loop was bounded, but by a CONSTANT rather than
    // by the room. Both bounds are windows now, both omissions are counted on the side they
    // happened, and both come from the OBJECTS list's own two functions.
    const InfoBodyPlace body = info_body_place(b, sc, d, s);
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
    // `OBJECTS` IS THE REGION'S FIRST PROSE ROW. It used to be an ordinary label on
    // the panel's cell row 0, kept OUT of the body's region because that row was shared
    // with the screen's own terminal hint; the shared top row is retired, the rectangle is
    // whole-panel and wholly this panel's, and the heading is set in whatever type the
    // active medium owns, like everything under it.
    region.rows.push_back(surface::SurfaceTextRow{detail::fit("OBJECTS", body.fit.columns),
                                                  surface::role::kAccent});
    // A PANEL WITH ROOM FOR THE HEADING AND NOTHING ELSE STILL SAYS WHAT IT IS -- the
    // external panes' own rule, one panel over: a rectangle showing a maker nothing at all
    // is the worse of the two answers.
    if (!body.present) {
        layer.texts.push_back(std::move(region));
        return; // no room under the heading: the heading, and no invented room
    }
    // A ROW MAY BE SET ON A GROUND, AND ALMOST NONE OF THEM IS. The ground is
    // defaulted rather than spelled at every call because `role::kNone` is not a value a row
    // could be wrong about -- it is the absence of one, and the picture it draws is the
    // picture every row of this body drew earlier. That is the opposite of
    // `first_visible`, where a default would have let a call site keep an old spelling and be
    // silently right until the first line long enough to scroll: here the two sites that pass
    // one are the whole of what this phase changed, and the default is what makes them read
    // as the exception they are.
    const auto say_row = [&region](std::string text, std::int64_t role,
                                   std::int64_t ground = surface::role::kNone) {
        region.rows.push_back(surface::SurfaceTextRow{std::move(text), role, ground});
    };
    // The markers are in the panel's own muted role because they are the tool's furniture and
    // not authored material: nothing here mints an identity, invents a name, or reorders a
    // document to make a screen fit.
    const auto say_omission = [&](std::size_t how_many, const char* which) {
        if (how_many > 0) {
            say_row(detail::fit(omitted_text(how_many, which), body.columns),
                    surface::role::kMuted);
        }
    };
    // ---- THE FOOTER, WRITTEN ONCE AND EMITTED ON EVERY PATH OUT OF THIS PAINTER -----------
    //
    // The body has two early exits -- an empty document's `(nothing selected)` and the
    // ordinary end -- and a maker in either of those states is exactly the maker who most
    // needs to see that `Create` exists. So the footer is a closure both of them call rather
    // than two copies, and this painter now finishes in one place.
    //
    // THE BLANK ROWS ARE THE SPARE ROOM, and they are written rather than left off because the
    // controls are anchored to `action_row` and a region's rows are positional. It is the same
    // padding the object list's share already gets when it has less to say than it was given.
    const auto say_footer = [&] {
        // `action_row` is a BODY row; the region's rows carry the heading above the body,
        // so every positional bound below is offset by the rows the heading keeps.
        while (region.rows.size() <
               static_cast<std::size_t>(kInfoHeadingRows + body.action_row)) {
            say_row(std::string(), surface::role::kFill);
        }
        for (std::size_t which = 0; which < kActionCount; ++which) {
            const bool pressable = available(action_availability(which, d, s));
            // THE ROLE IS THE SECOND SIGNAL AND NEVER THE ONLY ONE. `kMuted` is this panel's
            // existing word for "furniture, not the maker's material", which is what a control
            // a maker cannot use currently is; the brackets in the text carry the same fact to
            // a medium with no ink to spend. No role was added and none was widened.
            //
            // AND A CONTROL A MAKER CAN USE SITS ON SOMETHING, which is the THIRD
            // signal and still not the only one: `[ ... ]` is what a medium with no ground at
            // all reads, and it is unchanged. The ground is `kMuted` -- the same value the
            // Terminal's completion list has spent on its selected row -- and the
            // reason is a legibility fact each MEDIUM owns rather than a semantic one: it is
            // the one ground in either palette that every ink in `sgr_for_role` /
            // `ink_for_role` reads on, so a publisher may set a row on it without knowing
            // what ink the row's own role resolved to. (Pairing a role with its OWN ground is
            // the mistake this avoids -- `kFill` on `kFill` is white on white in a terminal.)
            //
            // AN UNAVAILABLE CONTROL IS GIVEN NO GROUND AT ALL, which is the whole of what
            // makes the ground say "actionable" rather than "a control is here". Handing it
            // the same slab and a quieter ink would make availability a matter of degree, and
            // a maker would be reading two shades of grey to learn a fact the brackets state
            // outright.
            say_row(action_row_text(which, pressable, body.columns),
                    pressable ? surface::role::kFill : surface::role::kMuted,
                    pressable ? surface::role::kMuted : surface::role::kNone);
        }
        layer.texts.push_back(std::move(region));
    };

    // ---- the objects, named by identity, pointing at the same selection the ring does ----
    //
    // An empty document SAYS it is empty. A maker can reach this state with their own hand by
    // deleting their work, and a panel that merely goes blank is indistinguishable from a tool
    // that has broken. It also says what to do next, because the answer is one key and the
    // alternative is a maker who thinks they have destroyed it. It is a ROW of the body rather
    // than a label beside it, so it is bounded and set in type like everything else here.
    if (d.elements.empty()) {
        say_row(detail::fit("(none) -- n makes one", body.columns), surface::role::kMuted);
    } else {
        say_omission(body.objects.before, "earlier");
        for (std::size_t n = 0; n < body.objects.count; ++n) {
            const std::size_t index = body.objects.first + n;
            const ui::Element& e = d.elements[index];
            const bool chosen = e.id == s.selected;
            // A NAME LONGER THAN THE COLUMN MAY BE READ PAST. The identity keeps the
            // row; what the pointer scrolls is the same string this row was already cutting,
            // and the document is as `const` here as it ever was.
            say_row(detail::reveal_shown(s.reveal, reveal_place::kInfoObject, index,
                                         object_row_full(e, chosen),
                                         object_row_text(e, chosen, body.columns),
                                         body.columns),
                    chosen ? surface::role::kAccent : surface::role::kFill);
        }
        say_omission(body.objects.after, "more");
    }
    // The object list's share is spent whether or not it had that much to say, because the
    // heading below it is at a row the composition chose and not at the row this loop happened
    // to reach. A list that says less than its share leaves blank rows under itself.
    while (region.rows.size() < static_cast<std::size_t>(kInfoHeadingRows) + body.objects_rows) {
        say_row(std::string(), surface::role::kFill);
    }

    // ---- `PROPERTIES`, a row of the body, at the row the composition put it ----
    //
    // AND IT IS SET ON A GROUND, because accent ink alone was not enough to say
    // "a section begins here". It was predicted and a live run confirmed it: the row
    // immediately above `PROPERTIES` is the SELECTED object, which is accent ink too, so the
    // heading and the thing it is not were the same colour on adjacent rows. A ground is the
    // one signal in this vocabulary that says "this row, all of it" -- which is what a
    // boundary is -- and it takes no room, so spent separator row stays spent.
    //
    // THE SAME `kMuted` THE CONTROLS BELOW USE, and that is agreement rather than sharing:
    // the two consumers arrive at one value because each medium offers exactly one ground
    // every ink reads on, not because a heading and a control mean the same thing. What
    // distinguishes them is what each already carried -- accent ink and a section's name
    // against fill ink and a bracketed verb -- so no role was added, none was widened, and
    // there is no `kSectionGround` constant pretending the two decisions are one.
    say_row("PROPERTIES", surface::role::kAccent, surface::role::kMuted);

    // ---- the properties ----
    if (s.rows.empty()) {
        say_row(detail::fit("(nothing selected)", body.columns), surface::role::kMuted);
        say_footer();
        return;
    }
    say_omission(body.properties.before, "earlier");
    for (std::size_t n = 0; n < body.properties.count; ++n) {
        const std::size_t i = body.properties.first + n;
        const Row& row = s.rows[i];
        const bool here = i == s.cursor;
        std::int64_t role = surface::role::kFill;
        if (row.editing()) {
            role = surface::role::kAlert; // a live draft is never quiet
        } else if (!row.editable()) {
            role = surface::role::kMuted; // not the maker's to author
        }
        // A RESTING VALUE LONGER THAN ITS COLUMN MAY BE READ PAST; a LIVE DRAFT
        // may not, and that exclusion is the feature rather than a gap -- a draft is
        // already windowed against its own caret, and a pointer scrolling it would be a
        // second window over one line, fighting the one `keep_caret_visible` reconciles.
        say_row(row.editing() ? property_row_text(row, here, body.value_columns)
                              : detail::reveal_shown(
                                    s.reveal, reveal_place::kInfoProperty, i,
                                    property_row_full(row, here),
                                    property_row_text(row, here, body.value_columns),
                                    body.columns),
                role);
        if (row.editing()) {
            // ONE MEASURER, TWICE OVER. The prose ROW is `prose_row_of_property` -- the same
            // function `property_at_prose_row` inverts for a press -- and the COLUMN is
            // `property_caret_column`, the same offset the row's own text was built with. So
            // a caret cannot land where the text is not, and a click cannot land where the
            // caret would not, on either axis.
            region.caret_row = kInfoHeadingRows + prose_row_of_property(body, i);
            region.caret_col = property_caret_column(row);
            // AND THE DRAFT'S SELECTION, THROUGH THE SAME TWO ANSWERS: the same
            // prose row, and the same value offset the caret's column was built with — so
            // the highlight cannot land where the caret would not, on either axis, in
            // either medium (a band under the glyphs where the body is real type, reverse
            // video over exactly the selected characters where it is cells).
            const TerminalSelectionSpan marked =
                property_selection_columns(row, body.value_columns);
            if (marked.present) {
                region.sel_begin_row = region.caret_row;
                region.sel_begin_col = marked.begin;
                region.sel_end_row = region.caret_row;
                region.sel_end_col = marked.end;
            }
        }
    }
    say_omission(body.properties.after, "more");
    say_footer();
}

} // namespace zengine::workshop
