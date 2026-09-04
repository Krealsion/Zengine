// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `weave.hpp`'s sections -- pane management, and the pointer inside arrangement --
// compiled once into `zengine-workshop-logic` and linked by the host and every suite; the
// declarations, the constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/arrangement.md (+8 registers; agents/workshop.md routes)

#include "weave.hpp"

namespace zengine::workshop {

// ---- PANE MANAGEMENT: arrange the windows, and never lose one -------------

// WL-ARR-08 -- agents/workshop/arrangement.md
// WL-PANE-12 -- agents/workshop/panes-and-windows.md
std::vector<PaneRef> WorkshopWeave::arrangeable() const {
    std::vector<PaneRef> out;
    for (const CatalogRow& row : inventory_rows(session_.setup.active, session_.panels)) {
        if (has_pane(session_.setup.active, row.ref)) {
            out.push_back(row.ref);
        }
    }
    return out;
}

// WL-ARR-07 -- agents/workshop/arrangement.md
void WorkshopWeave::open_arrange_desk() {
    PaneArrange& a = session_.arrange;
    a.open = true;
    a.desk = true;
    a.resetting = false;
    a.pane = PaneRef{};
    if (arrangeable().empty()) {
        say("arrange desk -- this setup names no panes; " + hotkey(Act::kPicker) +
                " opens one, " + hotkey(Act::kManageClose) + " leaves",
            false);
        return;
    }
    say("arrange desk -- drag a pane's body or edges; " + hotkey(Act::kManageNext) +
            " steps, " + hotkey(Act::kManageClose) + " or right-click leaves",
        false);
}

// WL-ARR-07, WL-ARR-09 -- agents/workshop/arrangement.md
// WL-CTX-02 -- agents/workshop/contextual.md
// WL-FRONT-04 -- agents/workshop/planes.md
// WL-PRESS-06 -- agents/workshop/press-chain.md
void WorkshopWeave::enter_arrange_pane(const PaneRef& ref) {
    const Written ready = arrange_geometry_ready(ref);
    if (!ready.accepted) {
        say(ready.refusal, true);
        return;
    }
    // ARRANGING A PANE IS CHOOSING IT, AND IT SPENDS THE SELECTION THAT ALREADY
    // EXISTS. A maker who says "arrange this one" has identified the thing they are
    // working with as surely as a press into it does, so the same `Panels::selected`
    // truth an ordinary press writes is written here -- and everything the lift derives
    // from that truth follows with nothing added: the selected chrome, the temporary
    // foreground lift in `effective_pane_order`, the paint order, the hit order. There
    // is no arrangement-specific z-order, no second foreground fact and no `front` rank
    // touched; `manage.front` is still the only way to say "and I mean this
    // permanently", and a save straight after this writes the desk it always would.
    //
    // AFTER ADMISSION, NEVER BEFORE. A pane that cannot be arranged leaves the maker
    // exactly where they were (rule for this door), and that has to include the
    // selection: a refusal that had silently re-selected something would have moved the
    // desk while saying it changed nothing.
    //
    // THE KEYBOARD CANDIDATE IS NOT TOUCHED, and the two are deliberately not collapsed
    // merely because they happen together. Arrangement is a keyboard CONTEXT of its own
    // (`KeyContext::kArrangePane`), sitting above any pane's claim on the keys; where
    // those keys go when the maker LEAVES is a separate fact with a separate owner, and
    // an entrance that quietly re-pointed it would hand the keyboard somewhere new for
    // reasons the maker never stated.
    //
    // ...AND THE RESOLUTION IS FRESH, `bounds_of`'s discipline: admission proved this
    // reference names a pane a moment ago, and asking again is cheaper than carrying an
    // answer that could have been a different pane's.
    const std::optional<std::int64_t> kind = resolve_pane(ref, session_.panels);
    if (kind.has_value()) {
        session_.panels.selected = *kind;
    }
    PaneArrange& a = session_.arrange;
    a.open = true;
    a.desk = false;
    a.resetting = false;
    a.pane = ref;
    say("arranging " + ref_text(ref) + " -- drag its body to move, its edges to size; " +
            hotkey(Act::kManageClose) + " or right-click leaves",
        false);
}

// WL-ARR-03 -- agents/workshop/arrangement.md; WL-PED-03 -- agents/workshop/pane-manager.md
void WorkshopWeave::forget_removed_selection() {
    // ⚠ THE PANE EDITOR'S SUBJECT IS DELIBERATELY NOT REPAIRED HERE. The
    // arrangement's address is a claim about a pane ON THE DESK, so a removal ends it;
    // the editor's subject is an IDENTITY a maker asked to be described, and a pane
    // that just left the layout is exactly the pane that now reads `closed -- open it`
    // -- clearing it would make "remove, look, reopen" impossible from the one surface
    // built for it. Its one clearing rule is `repair_pane_editor_subject`.
    PaneArrange& a = session_.arrange;
    if (!a.addressed() || has_pane(session_.setup.active, a.pane)) {
        return;
    }
    a.pane = PaneRef{};
    session_.pane_drag = PaneGesture{};
    if (a.open && !a.desk) {
        a.open = false;
        a.resetting = false;
    }
}

void WorkshopWeave::close_arrange() {
    session_.arrange = PaneArrange{};
    session_.pane_drag = PaneGesture{};
    say("left arranging", false);
}

// WL-GEO-11, WL-GEO-12 -- agents/workshop/geometry.md
// WL-ARR-09 -- agents/workshop/arrangement.md
std::string WorkshopWeave::arrange_status() const {
    const PaneArrange& a = session_.arrange;
    if (!a.addressed()) {
        return "arrange -- no pane addressed";
    }
    const char* state = "";
    for (const CatalogRow& row : inventory_rows(session_.setup.active, session_.panels)) {
        if (row.ref == a.pane) {
            state = pane_state_word(pane_state_of(session_.panels, session_.setup.active,
                                                  screen_of(session_), row));
            break;
        }
    }
    const SetupPane* row = pane_of(session_.setup.active, a.pane);
    std::string text = "arrange " + ref_text(a.pane) + " (" + state + ") -- " +
                       pane_window_text(row, session_.cell_px);
    if (pane_window_partly_default(row)) {
        const FineRect now = managed_bounds().resolved;
        if (now.w > 0 && now.h > 0) {
            text += " -- now " + fine_rect_text(now, session_.cell_px);
        }
    }
    return text;
}

// WL-ARR-08 -- agents/workshop/arrangement.md
void WorkshopWeave::arrange_step(std::int64_t by) {
    const std::vector<PaneRef> rows = arrangeable();
    if (rows.empty()) {
        say("this setup names no panes -- " + hotkey(Act::kPicker) + " opens one", true);
        return;
    }
    PaneArrange& a = session_.arrange;
    if (!a.addressed()) {
        a.pane = by > 0 ? rows.front() : rows.back();
        say(arrange_status(), false);
        return;
    }
    std::size_t at = 0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i] == a.pane) {
            at = i;
            break;
        }
    }
    const std::size_t n = rows.size();
    const std::size_t next = by > 0 ? (at + 1) % n : (at + n - 1) % n;
    a.pane = rows[next];
    say(arrange_status(), false);
}

// WL-ARR-07 -- agents/workshop/arrangement.md
// WL-CTX-02 -- agents/workshop/contextual.md
// WL-PANE-08 -- agents/workshop/panes-and-windows.md
// WL-SETUP-06 -- agents/workshop/setup-file.md
Written WorkshopWeave::arrange_geometry_ready(const PaneRef& ref) const {
    if (ref.provider.empty()) {
        return Written::no("no pane is addressed -- " + hotkey(Act::kManageNext) +
                           " steps to one");
    }
    // A CAPTURED SUBJECT HAS NO `forget_removed_selection` KEEPING IT FRESH, so absence
    // is answered here, first, in its own words -- falling through would report a
    // removed pane as "has no room on this screen yet": true of the screen, wrong about
    // the cause. For the mode's own selection this branch is unreachable today (the
    // clearing runs inside `apply_setup`), and it is written anyway: belt, not door.
    if (!has_pane(session_.setup.active, ref)) {
        return Written::no(ref_text(ref) + " is no longer in this setup -- " +
                           hotkey(Act::kPicker) + " can bring it back");
    }
    const std::optional<std::int64_t> kind = resolve_pane(ref, session_.panels);
    if (!kind.has_value()) {
        return Written::no(ref_text(ref) +
                           " is unresolved -- its place and size cannot be measured; "
                           "0 resets it and f/b/r/l still order it");
    }
    // A UNIT OUTRANKS A RESERVATION, the same precedence `pane_state_of` spends between
    // a unit and a want of room. Both sentences are true of a fixed pane
    // sized in pixels, and only one of them tells a maker what to press.
    if (!pane_unit_projectable(pane_of(session_.setup.active, ref))) {
        return Written::no(kind_name(session_.panels, *kind) +
                           " is sized in pixels, which no medium here can project -- "
                           "0 then w or h resets that axis");
    }
    if (placement_of(*kind) == placement::kSideRegion) {
        return Written::no(kind_name(session_.panels, *kind) +
                           " is in the reserved side column -- the screen owns its place");
    }
    const PanelBounds where =
        bounds_of(session_.panels, session_.setup.active, *kind, screen_of(session_));
    if (!where.open) {
        return Written::no(kind_name(session_.panels, *kind) +
                           " has no room on this screen yet -- 0 resets it");
    }
    if (!where.projected) {
        return Written::no(kind_name(session_.panels, *kind) +
                           " is sized in pixels, which no medium here can project -- "
                           "0 then w or h resets that axis");
    }
    if (where.rect.w <= 0 || where.rect.h <= 0) {
        return Written::no(kind_name(session_.panels, *kind) +
                           " is off this screen -- 0 then p resets its place");
    }
    return Written::ok();
}

// WL-ARR-04 -- agents/workshop/arrangement.md; WL-GEO-12 -- agents/workshop/geometry.md
PanelBounds WorkshopWeave::managed_bounds() const {
    const std::optional<std::int64_t> kind =
        resolve_pane(session_.arrange.pane, session_.panels);
    if (!kind.has_value()) {
        return PanelBounds{};
    }
    return bounds_of(session_.panels, session_.setup.active, *kind, screen_of(session_));
}

// WL-ARR-04 -- agents/workshop/arrangement.md; WL-PED-05 -- agents/workshop/pane-manager.md
FineRect WorkshopWeave::managed_window_base() {
    // ONE READING FOR THE HAND AND FOR THE TYPED VALUE: `pane_window_base`
    // (screen.hpp) is this function's old body, quarried out so the Pane Editor's
    // per-axis writes measure the axis they did not type from the same window the
    // arrangement's gestures measure from.
    return pane_window_base(session_, session_.arrange.pane);
}

// WL-ARR-06 -- agents/workshop/arrangement.md
void WorkshopWeave::arrange_place(std::int64_t x, std::int64_t y, loom::Mail& mail) {
    const Written ready = arrange_geometry_ready(session_.arrange.pane);
    if (!ready.accepted) {
        say(ready.refusal, true);
        return;
    }
    const FineRect from = managed_window_base();
    PaneAxisProposal horizontal;
    horizontal.base = from.x;
    if (x != from.x) {
        horizontal.position = x;
    }
    PaneAxisProposal vertical;
    vertical.base = from.y;
    if (y != from.y) {
        vertical.position = y;
    }
    const WindowWritten done = author_pane_window(session_.setup.active,
                                                  session_.arrange.pane, horizontal,
                                                  vertical);
    if (!done.written.accepted) {
        say(done.written.refusal, true);
        return;
    }
    // AND THE SEATING IS RECONCILED, because authoring a place takes the pane OUT of the
    // reactive stack -- it stops spending a tile, and whatever was waiting for one may
    // now have it. Resetting the place puts it back. This is the one door that opens or
    // closes a panel, so a geometry edit cannot produce a screen the setup disagrees with.
    if (done.place_written) {
        apply_setup(mail);
    }
    say(arrange_status(), false);
}

// WL-ARR-08 -- agents/workshop/arrangement.md
void WorkshopWeave::arrange_nudge(std::int64_t dx, std::int64_t dy, loom::Mail& mail) {
    const Written ready = arrange_geometry_ready(session_.arrange.pane);
    if (!ready.accepted) {
        say(ready.refusal, true);
        return;
    }
    // THE RESOLVED CORNER, NEVER THE CLIPPED ONE -- see `managed_bounds`.
    const FineRect from = managed_window_base();
    arrange_place(detail::step(from.x, dx * surface::kCellSubs),
                  detail::step(from.y, dy * surface::kCellSubs), mail);
}

// WL-ARR-05, WL-ARR-06 -- agents/workshop/arrangement.md
void WorkshopWeave::arrange_resize(std::int64_t edge, std::int64_t base_x, std::int64_t base_y,
                                   std::int64_t base_w, std::int64_t base_h, std::int64_t dx,
                                   std::int64_t dy, loom::Mail& mail) {
    const Written ready = arrange_geometry_ready(session_.arrange.pane);
    if (!ready.accepted) {
        say(ready.refusal, true);
        return;
    }
    const PaneWindowProposal want =
        pane_window_proposal(edge, base_x, base_y, base_w, base_h, dx, dy);
    PaneAxisProposal horizontal;
    horizontal.base = base_x;
    if (want.place_moved_x && want.x != base_x) {
        horizontal.position = want.x;
    }
    if (want.w != base_w) {
        horizontal.extent = PaneSize{pane_unit::kSubcells, want.w};
    }
    PaneAxisProposal vertical;
    vertical.base = base_y;
    if (want.place_moved_y && want.y != base_y) {
        vertical.position = want.y;
    }
    if (want.h != base_h) {
        vertical.extent = PaneSize{pane_unit::kSubcells, want.h};
    }
    const WindowWritten done = author_pane_window(session_.setup.active,
                                                  session_.arrange.pane, horizontal,
                                                  vertical);
    if (!done.written.accepted) {
        say(done.written.refusal, true);
        return;
    }
    if (done.place_written) {
        // AUTHORING A PLACE TAKES THE PANE OUT OF THE REACTIVE STACK — `arrange_place`'s
        // own reconciliation, owed here the moment an anchored resize writes one.
        apply_setup(mail);
    } else {
        // A SIZE-ONLY CHANGE CANNOT MOVE A PANE BETWEEN SEATED AND WAITING -- only a
        // PLACE does that -- but the room an external pane was granted may have moved,
        // and `repaint` owns that (`refresh_external_rooms`). Nothing is reconciled here.
        (void)mail;
    }
    say(arrange_status(), false);
}

// WL-ARR-08, WL-ARR-10 -- agents/workshop/arrangement.md
void WorkshopWeave::arrange_grow(std::int64_t dx, std::int64_t dy, loom::Mail& mail) {
    const Written ready = arrange_geometry_ready(session_.arrange.pane);
    if (!ready.accepted) {
        say(ready.refusal, true);
        return;
    }
    // THE RESOLVED WINDOW, NEVER THE VISIBLE ONE -- see `managed_bounds`.
    const FineRect base = managed_window_base();
    arrange_resize(pane_edge::kBottomRight, base.x, base.y, base.w, base.h,
                   dx * surface::kCellSubs, dy * surface::kCellSubs, mail);
}

// WL-CTX-07 -- agents/workshop/contextual.md; WL-PED-05 -- agents/workshop/pane-manager.md
void WorkshopWeave::spend_pane_action(Act a, const PaneRef& ref, loom::Mail& mail) {
    if (ref.provider.empty()) {
        say("no pane is addressed -- " + hotkey(Act::kManageNext) + " steps to one",
            true);
        return;
    }
    Setup& s = session_.setup.active;
    if (!has_pane(s, ref)) {
        say(ref_text(ref) + " is no longer in this setup -- " + hotkey(Act::kPicker) +
                " can bring it back",
            true);
        return;
    }
    switch (a) {
    // THE FOUR ORDERING OPERATIONS. Available for EVERY row, including an unresolved
    // one, because the rank is over all authored rows and reordering one writes
    // nothing that seating or placement reads.
    case Act::kManageFront:
    case Act::kManageBack:
    case Act::kManageRaise:
    case Act::kManageLower: {
        bool moved = false;
        const char* what = "";
        switch (a) {
        case Act::kManageFront: moved = send_to_front(s, ref); what = "front-most"; break;
        case Act::kManageBack: moved = send_to_back(s, ref); what = "back-most"; break;
        case Act::kManageRaise: moved = raise_one(s, ref); what = "raised"; break;
        default: moved = lower_one(s, ref); what = "lowered"; break;
        }
        if (!moved) {
            say(ref_text(ref) + " is already where that would put it", true);
            return;
        }
        say(ref_text(ref) + " " + what + " -- " +
                pane_window_text(pane_of(s, ref), session_.cell_px),
            false);
        return;
    }
    // THE PER-PANE RESETS, one per authored dimension. (`order` is the whole setup's
    // and zero-target -- `reset_front_order` below -- because the rank is a
    // permutation over all of it.)
    case Act::kManageResetPlace:
    case Act::kManageResetWidth:
    case Act::kManageResetHeight: {
        bool moved = false;
        const char* what = "";
        if (a == Act::kManageResetPlace) {
            moved = reset_pane_place(s, ref);
            what = "place";
        } else if (a == Act::kManageResetWidth) {
            moved = reset_pane_width(s, ref);
            what = "width";
        } else {
            moved = reset_pane_height(s, ref);
            what = "height";
        }
        if (!moved) {
            say(ref_text(ref) + " already takes the developer's " + what, true);
            return;
        }
        // A PLACE RESET PUTS THE PANE BACK IN THE REACTIVE STACK, so the seating has
        // to be reconciled for the same reason authoring one does.
        apply_setup(mail);
        say(ref_text(ref) + " " + what + " reset -- " +
                pane_window_text(pane_of(s, ref), session_.cell_px),
            false);
        return;
    }
    // REMOVE THIS PANE. The picker's own semantics through the picker's own
    // door: the intent leaves the setup, `apply_setup` is what closes the
    // presentation, and what the pane was presenting is untouched -- a panel is a
    // presentation, and removing one removes a presentation. A removal works on a
    // waiting or unresolved row exactly as on an open one (rule).
    case Act::kManageRemove: {
        const std::string name = ref_text(ref);
        if (!remove_pane(s, ref)) {
            say(name + " is no longer in this setup -- " + hotkey(Act::kPicker) +
                    " can bring it back",
                true);
            return;
        }
        apply_setup(mail);
        say("removed " + name + " -- " + hotkey(Act::kPicker) +
                " brings it back; nothing behind it was touched",
            false);
        return;
    }
    default: return;
    }
}

void WorkshopWeave::reset_front_order() {
    reset_front(session_.setup.active);
    say("front order reset to the setup's own order", false);
}

// WL-ARR-08 -- agents/workshop/arrangement.md
void WorkshopWeave::arrange_key(const zengine::input::KeyPressed& k, loom::Mail& mail) {
    PaneArrange& a = session_.arrange;
    const KeyContext ctx = keyboard_context(session_);
    switch (session_.keymap.action_for(ctx, k.scancode, k.modifiers)) {
    // -- the desk's stepping, and its door into the one-pane scope ------------------
    case Act::kManageNext: arrange_step(+1); break;
    case Act::kManagePrevious: arrange_step(-1); break;
    case Act::kArrange: enter_arrange_pane(a.pane); break;
    // -- moving: arrows place the addressed pane ------------------------------------
    case Act::kManagePlaceLeft: arrange_nudge(-1, 0, mail); break;
    case Act::kManagePlaceRight: arrange_nudge(+1, 0, mail); break;
    case Act::kManagePlaceUp: arrange_nudge(0, -1, mail); break;
    case Act::kManagePlaceDown: arrange_nudge(0, +1, mail); break;
    // -- sizing: shifted arrows pull the extent, anchored at the place --------------
    case Act::kManagePullLeft: arrange_grow(-1, 0, mail); break;
    case Act::kManagePullRight: arrange_grow(+1, 0, mail); break;
    case Act::kManagePullUp: arrange_grow(0, -1, mail); break;
    case Act::kManagePullDown: arrange_grow(0, +1, mail); break;
    // -- and the coarse step, on both axes at once -----------------------------------
    //
    // THE SAME FUNCTION, A BIGGER DELTA. There is no second geometry owner here and
    // deliberately no second proposal: `arrange_grow` is the door a shifted arrow
    // already goes through, anchored bottom-right, so a coarse step cannot move the
    // pane, cannot move any other pane, and meets the identical per-axis settlement --
    // a shrink that would take the width below one cell keeps the width and still
    // shortens the height, refuse-never-clamp, per axis.
    case Act::kManageGrow:
        arrange_grow(+kCoarseStepCells, +kCoarseStepCells, mail);
        break;
    case Act::kManageShrink:
        arrange_grow(-kCoarseStepCells, -kCoarseStepCells, mail);
        break;
    // -- ordering and removal, on the addressed pane --------------------------------
    case Act::kManageFront: spend_pane_action(Act::kManageFront, a.pane, mail); break;
    case Act::kManageBack: spend_pane_action(Act::kManageBack, a.pane, mail); break;
    case Act::kManageRaise: spend_pane_action(Act::kManageRaise, a.pane, mail); break;
    case Act::kManageLower: spend_pane_action(Act::kManageLower, a.pane, mail); break;
    case Act::kManageRemove: spend_pane_action(Act::kManageRemove, a.pane, mail); break;
    // -- the reset prompt -----------------------------------------------------------
    case Act::kManageReset:
        a.resetting = true;
        say("reset -- " + hotkey_text(session_.keymap, Act::kManageResetPlace) +
                " place, " + hotkey_text(session_.keymap, Act::kManageResetWidth) +
                " width, " + hotkey_text(session_.keymap, Act::kManageResetHeight) +
                " height, " + hotkey_text(session_.keymap, Act::kManageResetOrder) +
                " order, " + hotkey_text(session_.keymap, Act::kManageDone) + " back",
            false);
        break;
    // The prompt closes exactly when the reset REACHED its operation (the earlier
    // behaviour, preserved: a refusal for want of an addressed pane leaves the maker
    // in the prompt they were in).
    case Act::kManageResetPlace:
        spend_pane_action(Act::kManageResetPlace, a.pane, mail);
        if (a.addressed()) {
            a.resetting = false;
        }
        break;
    case Act::kManageResetWidth:
        spend_pane_action(Act::kManageResetWidth, a.pane, mail);
        if (a.addressed()) {
            a.resetting = false;
        }
        break;
    case Act::kManageResetHeight:
        spend_pane_action(Act::kManageResetHeight, a.pane, mail);
        if (a.addressed()) {
            a.resetting = false;
        }
        break;
    case Act::kManageResetOrder:
        reset_front_order();
        a.resetting = false;
        break;
    case Act::kManageDone:
        a.resetting = false;
        say(arrange_status(), false);
        break;
    // -- leaving --------------------------------------------------------------------
    case Act::kManageClose: close_arrange(); break;
    default: break;
    }
}

// ---- The pointer, inside arrangement -------------------------------------

// WL-ARR-01 -- agents/workshop/arrangement.md
// WL-ARR-01, WL-ARR-07 -- agents/workshop/arrangement.md
// WL-PANE-01 -- agents/workshop/panes-and-windows.md
bool WorkshopWeave::take_pane_hold(const PaneRef& ref, const PointedAt& at, const Screen& sc) {
    const std::optional<std::int64_t> kind = resolve_pane(ref, session_.panels);
    // A HAND MAY TAKE HOLD OF ANY PANE WHOSE PLACE IS THE MAKER'S TO AUTHOR.
    // This named the overlay stack while the stack was the only such place; saying it
    // as the exclusion (`place_is_authorable` -- the side column is the screen's) is
    // what keeps the hand and the KEYS agreeing, since the arrangement admission has
    // always refused by that same sentence.
    if (!kind.has_value() || !place_is_authorable(placement_of(*kind))) {
        return false;
    }
    const PanelBounds mine = bounds_of(session_.panels, session_.setup.active, *kind, sc);
    if (!mine.open || mine.rect.w <= 0 || mine.rect.h <= 0) {
        return false;
    }
    const std::int64_t edge = pane_edge_at(mine.rect, at.sub.x, at.sub.y, at.grain);
    if (edge != kNoPaneEdge) {
        session_.pane_drag = PaneGesture{};
        session_.pane_drag.active = true;
        session_.pane_drag.pane = ref;
        session_.pane_drag.sizing = true;
        session_.pane_drag.edge = edge;
        session_.pane_drag.from_x = at.sub.x;
        session_.pane_drag.from_y = at.sub.y;
        const SetupPane* row = pane_of(session_.setup.active, ref);
        // THE AFFORDANCE IS ON THE VISIBLE BOUNDARY -- that is where the eye and the
        // hand are -- AND THE BASE IS THE RESOLVED WINDOW: place beside
        // size because an anchored top or left pull authors both from
        // this one captured rectangle. A hand and a key author from the same numbers,
        // which is the pairing this file has kept since both gestures existed.
        session_.pane_drag.base_x = row != nullptr && row->place.mode == pane_unit::kSubcells
                                        ? row->place.x
                                        : mine.resolved.x;
        session_.pane_drag.base_y = row != nullptr && row->place.mode == pane_unit::kSubcells
                                        ? row->place.y
                                        : mine.resolved.y;
        session_.pane_drag.base_w = row != nullptr && row->width.mode == pane_unit::kSubcells
                                        ? row->width.amount
                                        : mine.resolved.w;
        session_.pane_drag.base_h =
            row != nullptr && row->height.mode == pane_unit::kSubcells
                ? row->height.amount
                : mine.resolved.h;
        say(std::string("sizing ") + ref_text(ref) + " by its " + pane_edge_name(edge),
            false);
        return true;
    }
    if (mine.rect.contains_at(at.sub.x, at.sub.y, at.grain)) {
        session_.pane_drag = PaneGesture{};
        session_.pane_drag.active = true;
        session_.pane_drag.pane = ref;
        session_.pane_drag.grab_dx = detail::minus(at.sub.x, mine.rect.x);
        session_.pane_drag.grab_dy = detail::minus(at.sub.y, mine.rect.y);
        say("moving " + ref_text(ref) + " -- drag to place it", false);
        return true;
    }
    return false;
}

// WL-ARR-07 -- agents/workshop/arrangement.md; WL-FRONT-05 -- agents/workshop/planes.md
void WorkshopWeave::arrange_press(const PointedAt& at) {
    PaneArrange& a = session_.arrange;
    const Screen sc = screen_of(session_);
    if (!a.desk) {
        if (a.addressed() && take_pane_hold(a.pane, at, sc)) {
            return;
        }
        say("arranging " + ref_text(a.pane) + " -- " + hotkey(Act::kManageClose) +
                " or right-click leaves",
            false);
        return;
    }
    const std::vector<std::int64_t> order =
        effective_pane_order(session_.setup.active, session_.panels);
    for (std::size_t i = order.size(); i > 0; --i) {
        const std::int64_t kind = order[i - 1];
        if (!bounds_of(session_.panels, session_.setup.active, kind, sc)
                 .rect.contains_at(at.sub.x, at.sub.y, at.grain)) {
            continue;
        }
        for (const SetupPane& row : session_.setup.active.panes) {
            const std::optional<std::int64_t> named =
                resolve_pane(row.ref, session_.panels);
            if (named.has_value() && *named == kind) {
                a.pane = row.ref;
                if (!take_pane_hold(row.ref, at, sc)) {
                    // Addressed and not draggable -- the reserved side column. The
                    // admission owns the sentence.
                    const Written why = arrange_geometry_ready(row.ref);
                    say(why.accepted ? arrange_status() : why.refusal, !why.accepted);
                }
                return;
            }
        }
    }
    say("nothing to arrange there -- " + hotkey(Act::kManageClose) + " leaves", false);
}

// WL-ARR-01 -- agents/workshop/arrangement.md
void WorkshopWeave::arrange_motion(std::int64_t sub_x, std::int64_t sub_y, loom::Mail& mail) {
    PaneGesture& g = session_.pane_drag;
    if (!g.active) {
        return;
    }
    // THE TARGET MAY HAVE LEFT THE SETUP UNDER THE HAND -- a picker cannot be open while
    // this mode is, but a restore or a provider going away can -- so the gesture ends
    // safely rather than writing to a row that is no longer there.
    if (!has_pane(session_.setup.active, g.pane)) {
        g = PaneGesture{};
        forget_removed_selection();
        return;
    }
    const PaneRef held = g.pane;
    const PaneRef was_addressed = session_.arrange.pane;
    session_.arrange.pane = held;
    if (g.sizing) {
        arrange_resize(g.edge, g.base_x, g.base_y, g.base_w, g.base_h,
                       detail::minus(sub_x, g.from_x), detail::minus(sub_y, g.from_y),
                       mail);
    } else {
        arrange_place(detail::minus(sub_x, g.grab_dx), detail::minus(sub_y, g.grab_dy),
                      mail);
    }
    if (!has_pane(session_.setup.active, held)) {
        session_.arrange.pane = was_addressed;
    }
}

// WL-PROJ-13 -- agents/workshop/project.md
void WorkshopWeave::build_now(loom::Mail& mail, bool realize) {
    if (!session_.panels.has(panel::kBuilder)) {
        return; // `b` is an unbound key with no Builder panel open, exactly as before
    }
    const BuilderPane& pane = session_.panels.builder;
    if (!pane.heard) {
        say("the Builder has not said what it builds yet -- nothing was asked for", true);
        return;
    }
    if (pane.known.recipes.empty()) {
        say("this project has no build recipes -- nothing was asked for", true);
        return;
    }
    const std::size_t at =
        pane.chosen < pane.known.recipes.size() ? pane.chosen : std::size_t{0};
    const std::string chosen = pane.known.recipes[at].recipe;
    (void)mail.send_to_role(zengine::builder::kBuilderRole,
                            zengine::builder::BuildRequested{chosen, realize});
    // I ASKED. Workshop's own fact, recorded before anything is dispatched,
    // and the thing that decides whether the answer will be news to this
    // panel.
    //
    // THE SENTENCE CHANGED WITH THE ASYNC BUILD AND THE CHANGE IS THE POINT. It used
    // to say `the screen waits until it is done`, which was true and was the
    // measured cost of a runner that built inside its own handler. It is now
    // false: the runner starts a child, keeps it, and comes back to it on an
    // ordinary beat, so every other delivery in this program goes on
    // happening. Leaving the old words in place would have been the one kind
    // of stale comment this repository treats as a defect -- a sentence a
    // maker reads on the screen.
    session_.panels.builder.awaiting = true;
    session_.panels.builder.awaiting_realization = realize;
    say("asked the Builder for `" + chosen + "`" +
            (realize ? " and to realize it" : std::string()) +
            " -- Workshop stays live while it builds",
        false);
}

void WorkshopWeave::choose_recipe(int by, loom::Mail& mail) {
    if (!session_.panels.has(panel::kBuilder)) {
        return; // an unbound key with no Builder panel open, exactly as `b` is
    }
    BuilderPane& pane = session_.panels.builder;
    const std::size_t held = pane.known.recipes.size();
    if (held == 0) {
        say("this project has no build recipes to choose between", true);
        return;
    }
    const std::size_t at = pane.chosen < held ? pane.chosen : std::size_t{0};
    pane.chosen = by < 0 ? (at == 0 ? held - 1 : at - 1) : (at + 1 >= held ? 0 : at + 1);
    // THE ONE WRITER OF `picked`: this gesture is what makes a selection the
    // MAKER's rather than the catalog's order wearing an index. The frontier action
    // reads it when several recipes produce one artifact.
    pane.picked = true;
    say("build recipe: " + pane.known.recipes[pane.chosen].recipe + " -> " +
            pane.known.recipes[pane.chosen].artifact,
        false);
    repaint(mail);
}

// WL-PROJ-14 -- agents/workshop/project.md
void WorkshopWeave::build_frontier(loom::Mail& mail) {
    if (!session_.panels.has(panel::kBuilder)) {
        return; // an unbound key with no Builder panel open, exactly as `b` is
    }
    BuilderPane& pane = session_.panels.builder;
    if (!pane.heard) {
        say("the Builder has not said what it builds yet -- nothing was asked for", true);
        return;
    }
    const ProjectFrontier now = frontier_now();
    if (!now.waiting) {
        // THE ABSENCE IS THE OWNER'S OWN ANSWER, read a moment ago — not a status
        // this panel manufactured. A project that is complete, still loading, or
        // was never begun is equally "not waiting", and all three are states in
        // which there is no frontier for this gesture to spend.
        say("this project is not waiting on any artifact -- nothing was asked for", true);
        return;
    }
    std::size_t makers = 0;
    std::size_t match = 0;
    std::string named;
    for (std::size_t i = 0; i < pane.known.recipes.size(); ++i) {
        if (pane.known.recipes[i].artifact != now.artifact) {
            continue;
        }
        ++makers;
        match = i;
        if (!named.empty()) {
            named += ", ";
        }
        named += "`" + pane.known.recipes[i].recipe + "`";
    }
    if (makers == 0) {
        say("no authored recipe produces `" + now.artifact + "` -- nothing was asked for",
            true);
        return;
    }
    if (makers > 1) {
        const bool standing_pick = pane.picked && pane.chosen < pane.known.recipes.size() &&
                                   pane.known.recipes[pane.chosen].artifact == now.artifact;
        if (!standing_pick) {
            say(std::to_string(makers) + " recipes produce `" + now.artifact + "` (" +
                    named + ") -- pick one with c, then f builds and realizes it",
                true);
            return;
        }
        match = pane.chosen;
    }
    // THE SELECTION MOVES WITH THE GESTURE, VISIBLY: row 1 of the panel now names
    // the recipe this ask is about, and `build_now`'s own notice says it again. A
    // gesture that sent one recipe while the panel showed another would be the
    // cross-referencing this phase exists to end, reintroduced one row up.
    pane.chosen = match;
    build_now(mail, /*realize=*/true);
}

} // namespace zengine::workshop
