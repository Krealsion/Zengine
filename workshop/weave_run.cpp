// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s run: the frontier and the clock read alive, the repaint that publishes the
// slots and then the picture, and the one quit that asks the room.
// Workshop law: agents/workshop/attention.md (+5 registers; agents/workshop.md routes)

#include "weave.hpp"
#include "screen_canvas.hpp"

#include <algorithm>
#include <vector>

namespace zengine::workshop {

// ---- THE RUN: what is true now, the picture published, and the way out --------------

// WL-ATTN-04 -- agents/workshop/attention.md
ProjectFrontier WorkshopWeave::frontier_now() const {
    return host_->frontier ? host_->frontier() : ProjectFrontier{};
}

std::int64_t WorkshopWeave::interaction_now() const {
    return host_->interaction_now ? host_->interaction_now() : interaction_now_ms();
}

void WorkshopWeave::repaint(loom::Mail& mail) {
    // ⚠ WHAT THE HOST HAS LEARNED SINCE THE LAST PICTURE, FIRST. The boot's own discoveries --
    // an optional plan row that refused, and so a tool that is not in this Workshop -- settle
    // inside a delivery that can arrive after the first surface did (`HostContext`). One
    // integer compared; the list is re-taken only when it moved, and `establish` is keyed.
    if (conditions_taken_ != host_->conditions_generation) {
        take_host_conditions();
    }
    refresh_setup_name(); // a name editor's window is only true against the room it has now
    note_manuals();       // ...and which panes wear a `?`, against the holders there are now
    hand_settings(mail);  // ...and each pane's settings, before its area is placed
    // THE FRONTIER IS DERIVED HERE, PER PAINT, AND STORED NOWHERE. `paint` stays a
    // pure projection of what it is handed, and what it is handed is this repaint's
    // reading of the living realization owner — never a member, never a field of the
    // session, never yesterday's answer.
    const ProjectFrontier frontier = frontier_now();
    // The status slot, then the picture. What is true and worth a glance is the Attention pane's
    // to show, in its own room: no slot carries it.
    mail.publish(
        zengine::surface::SurfaceText{zengine::surface::kSlotStatus, status_line()});
    // WHAT IS TRUE, TO WHOEVER IS PRESENTING IT: one sentence per condition, in the host's own
    // order, said only when it changed. Derived at every repaint and held nowhere, so a resolved
    // condition is gone because it stopped being returned.
    say_conditions(frontier, mail);
    // ...AND THE ONE PANE INVENTORY, to whoever is listing it (WL-DESK-04). Same beat, same
    // rule, same silence when nothing changed: it is derived at every gesture, so the
    // comparison inside is what keeps a launcher from repainting for nothing.
    publish_inventory(mail);
    // ...AND THE EFFECTIVE KEYMAP, to whoever presents keys (WL-DESK-11), on the same terms.
    publish_keymap(mail);
    publish_pane_subject(mail);
    // ...and what the terminal participant's record holds, to whoever is presenting it.
    // Same beat, same rule, same silence when nothing changed.
    facet<TerminalDoorFacet>().say_transcript(mail);
    auto canvas = paint(session_);
    // THE PLANES ABOVE THE PICTURE, kept for the window's drawing of the desk too: the carried
    // value under the hand...
    window_overlays_.clear();
    if (carried_.drag && value_drag_.moved && !value_drag_.released && drag_pointer_.understood) {
        surface::SurfaceLayer overlay;
        const auto rect = wire_rect_of(PixelRect{drag_pointer_.px.x, drag_pointer_.px.y,
            12 * graphics::kCellPx, graphics::kCellPx}, graphics::role::kAccent);
        overlay.rects.push_back(rect);
        overlay.labels.push_back(
            surface::SurfaceLabel{rect.x, rect.y, "[value]", graphics::role::kFill});
        window_overlays_.push_back(std::move(overlay));
    }
    // ...and EVERY GUEST'S GUIDE MARK, each judged against what shows now.
    if (!guide_marks_.empty()) {
        surface::SurfaceLayer marks = guide_layer(mail);
        if (!marks.rects.empty() || !marks.labels.empty()) {
            window_overlays_.push_back(std::move(marks));
        }
    }
    canvas.layers.insert(canvas.layers.end(), window_overlays_.begin(), window_overlays_.end());
    mail.publish(std::move(canvas));
    stamps_owed_ = desk_owed_ = true; // the desk may have moved: looked at once, at the delivery's end
    // ...and the same desk in a graphics window, where a directory places its areas.
    show_in_graphics(mail);
}

// ---- THE DESK IN A GRAPHICS WINDOW: Workshop the arranger, and the drawer of its own area ----

bool WorkshopWeave::graphics_present() const {
    return host_->holder_accepts &&
           host_->holder_accepts(graphics::kGraphicsRole, *loom::schema_of<graphics::AreaPlaced>());
}

void WorkshopWeave::show_in_graphics(loom::Mail& mail) {
    arrange_graphics(mail);
    draw_window(mail);
}

namespace {

bool same_place(const graphics::AreaPlaced& a, const graphics::AreaPlaced& b) {
    return a.area == b.area && a.window == b.window && a.owner == b.owner && a.x == b.x &&
           a.y == b.y && a.w == b.w && a.h == b.h && a.order == b.order && a.keys == b.keys &&
           a.pointer == b.pointer && a.weaver_only == b.weaver_only && a.wheel == b.wheel &&
           a.right == b.right;
}

} // namespace

void WorkshopWeave::arrange_graphics(loom::Mail& mail) {
    if (!graphics_present()) {
        graphics_known_ = false; // a directory that comes is a new one, placed with afresh
        return;
    }
    const auto to_directory = [&mail](const auto& said) {
        (void)mail.as_role(kWorkshopProvider).send_to_role(graphics::kGraphicsRole, said);
    };
    const auto accepts = [this](const std::string& office, const loom::Schema& shape) {
        return host_->holder_accepts && host_->holder_accepts(office, shape);
    };
    // A DIRECTORY NOT PLACED WITH YET -- another holder, a reload, or one that said it started
    // afresh -- holds none of this desk's areas: the skins are offered and everything placed again.
    const loom::WeaveId holder =
        host_->role_holder ? host_->role_holder(graphics::kGraphicsRole) : loom::WeaveId{};
    const std::int64_t incarnation =
        host_->incarnation_of && holder.valid() ? host_->incarnation_of(holder) : 0;
    if (!graphics_known_ || holder.value != graphics_holder_ ||
        incarnation != graphics_incarnation_) {
        graphics_known_ = true;
        graphics_holder_ = holder.value;
        graphics_incarnation_ = incarnation;
        areas_placed_.clear();
        skins_chosen_.clear();
        graphics_asked_ = false;
        frames_.clear();
        frames_asked_.clear();
        if (!host_->dir.empty()) {
            to_directory(graphics::SkinOffered{kSkinClassic, host_->so("zengine-paint-classic")});
            to_directory(graphics::SkinOffered{kSkinBlock, host_->so("zengine-paint-block")});
            to_directory(graphics::SkinOffered{kSkinDay, host_->so("zengine-paint-day")});
            to_directory(graphics::SkinOffered{kSkinNight, host_->so("zengine-paint-night")});
        }
    }
    // EACH OPEN PANE AT ITS DEPTH in the desk's order, above Workshop's own area: its frame in an
    // area of Workshop's over its bounds, and, where its office draws in a window, the pane's own
    // area just in front under its title rows; else the frame says it draws in no window. The
    // window's keys are Workshop's alone, its keymap deciding first, and a pane hears them as
    // `PaneKey` and `PaneTextInput`; its pointer is the pane's where its office hears the window's.
    const Screen sc = screen_of(session_);
    const std::vector<std::int64_t> order =
        effective_pane_order(session_.setup.active, session_.panes);
    std::map<std::string, graphics::AreaPlaced> next;
    // EACH AREA'S SKIN: its pane's row's `workshop.skin`, else Workshop's own.
    std::map<std::string, std::string> skins;
    std::set<std::int64_t> kinds;
    std::map<std::string, std::int64_t> frames;
    for (std::size_t i = 0; i < order.size(); ++i) {
        const RuntimePane* row = session_.panes.runtime.of_kind(order[i]);
        const PaneBounds framed = bounds_of(session_.panes, session_.setup.active, order[i], sc);
        if (framed.open && !framed.rect.empty() &&
            (order[i] == pane_kind::kLayouts || (is_runtime_kind(order[i]) && row != nullptr))) {
            const std::string frame = pane_frame_area(order[i]);
            next[frame] = graphics::AreaPlaced{
                frame, window_name_, kWorkshopProvider, framed.rect.x, framed.rect.y,
                framed.rect.w, framed.rect.h, 2 * static_cast<std::int64_t>(i) + 1, false, true};
            frames[frame] = order[i];
        }
        ExternalPane* ext = session_.panes.external_pane(order[i]);
        if (!is_runtime_kind(order[i]) || row == nullptr || ext == nullptr ||
            !accepts(row->provider, *loom::schema_of<graphics::AreaRoom>())) {
            if (frames.count(pane_frame_area(order[i])) != 0) {
                skins[pane_frame_area(order[i])] = session_.skin;
            }
            continue;
        }
        const PaneBounds where = bounds_of(session_.panes, session_.setup.active, order[i], sc);
        const std::int64_t titles =
            external_title_rows(session_.panes, order[i], session_.pane_titles);
        // THE PLACE A HELD PRESS KEEPS (WL-FOCUS-11): the keys a press moved bring or take the
        // title their pane wears while it has them, and the area that title row alone would move
        // -- the pane where it was, under the title rows it was placed under -- stays where the
        // press was aimed until the window says the press ended. A pane moved keeps none.
        bool held = false;
        for (const WindowHold& hold : pane_holds_) held = held || (hold.active && hold.kind == order[i]);
        const PixelRect was{ext->placed_x, ext->placed_y, ext->placed_w, ext->placed_h};
        PixelRect place = canvas_body_place(where.rect, sc, titles);
        ext->title_waits = held && titles != ext->placed_titles && !was.empty() &&
                           canvas_body_place(where.rect, sc, ext->placed_titles) == was;
        if (ext->title_waits) {
            place = was;
        } else {
            ext->placed_x = place.x;
            ext->placed_y = place.y;
            ext->placed_w = place.w;
            ext->placed_h = place.h;
            ext->placed_titles = titles;
        }
        if (!where.open || place.empty()) {
            continue;
        }
        const std::string area = pane_window_area(session_.panes, *row);
        next[area] = graphics::AreaPlaced{
            area, window_name_, row->provider, place.x, place.y, place.w, place.h,
            2 * static_cast<std::int64_t>(i) + 2, false,
            accepts(row->provider, *loom::schema_of<graphics::AreaButtonChanged>()),
            weaver_only_pane(*row)};
        const SetupPane* seat =
            pane_of(session_.setup.active, PaneRef{row->provider, row->pane});
        const PaneSetting* own =
            seat != nullptr ? find_pane_setting(seat->settings, kWorkshopSkinKey) : nullptr;
        skins[area] = own != nullptr && own->text && is_workshop_skin_choice(*own->text)
                          ? *own->text
                          : session_.skin;
        if (frames.count(pane_frame_area(order[i])) != 0) {
            skins[pane_frame_area(order[i])] = skins[area]; // its frame wears its skin
        }
        kinds.insert(order[i]);
    }
    // WORKSHOP'S OWN AREA, the whole screen: behind every pane's, and in front while a weaver
    // arranges, a menu is open or something is carried -- the wheel let through but under a carry,
    // and the third button while a menu alone stands it there, so a right press re-asks the pane
    // under the hand. A pane's menu stands in its presenter's own area, for the weaver's hand alone;
    // the overlay stands in front of all, hearing nothing, so a guide mark or a carried value takes
    // no press from the pane under it.
    const auto panes_order = 2 * static_cast<std::int64_t>(order.size());
    const bool menu_alone = (session_.context.open || session_.presented.open) &&
                            !session_.arrange.open && carried_.data.empty();
    next[kWorkshopProvider] = graphics::AreaPlaced{
        kWorkshopProvider, window_name_, kWorkshopProvider, 0, 0, sc.w, sc.h,
        desk_in_front_of_panes() ? panes_order + 1 : 0, true, true, false,
        desk_in_front(session_) || carried_.data.empty(), !menu_alone};
    skins[kWorkshopProvider] = session_.skin;
    const PixelRect menu = presented_area_place(session_, sc);
    const bool menu_placed = session_.presented.open && !menu.empty();
    if (menu_placed) {
        const RuntimePane* about =
            session_.panes.runtime.find(session_.presented.office, session_.presented.pane);
        next[session_.presented.area] = graphics::AreaPlaced{
            session_.presented.area, window_name_, kPresenterRole, menu.x, menu.y, menu.w, menu.h,
            panes_order + 2, false, true, about != nullptr && weaver_only_pane(*about)};
        skins[session_.presented.area] = session_.skin;
    }
    next[kWorkshopOverlayArea] = graphics::AreaPlaced{
        kWorkshopOverlayArea, window_name_, kWorkshopProvider, 0, 0, sc.w, sc.h,
        panes_order + (menu_placed ? 3 : 2), false, false};
    skins[kWorkshopOverlayArea] = session_.skin;
    for (const auto& [area, was] : areas_placed_) {
        if (next.count(area) == 0) {
            to_directory(graphics::AreaPlaced{area, "", was.owner, 0, 0, 0, 0, 0, false, false});
            skins_chosen_.erase(area);
        }
    }
    // EACH PLACE THAT MOVED, SAID SO THAT NO AREA EVER STANDS IN FRONT OF ONE IT ENDS BEHIND: the
    // window ends a held press an area in front now covers after each place it hears, so the
    // areas going back are said first, the hindmost first, and then the rest, the foremost first.
    std::vector<const graphics::AreaPlaced*> back, front;
    for (const auto& [area, placed] : next) {
        const auto was = areas_placed_.find(area);
        if (was != areas_placed_.end() && same_place(was->second, placed)) continue;
        const bool lowered = was != areas_placed_.end() && placed.order < was->second.order;
        (lowered ? back : front).push_back(&placed);
    }
    std::sort(back.begin(), back.end(), [](const auto* a, const auto* b) { return a->order < b->order; });
    std::sort(front.begin(), front.end(), [](const auto* a, const auto* b) { return a->order > b->order; });
    for (const graphics::AreaPlaced* placed : back) to_directory(*placed);
    for (const graphics::AreaPlaced* placed : front) to_directory(*placed);
    for (const auto& [area, placed] : next) {
        // A SKIN IS SAID WHEN THE AREA'S CHOICE MOVED: placed afresh, or its setting changed.
        const std::string& skin = skins[area];
        if (!host_->dir.empty() && skins_chosen_[area] != skin) {
            skins_chosen_[area] = skin;
            to_directory(graphics::SkinChosen{area, skin});
        }
    }
    areas_placed_ = std::move(next);
    placed_kinds_ = std::move(kinds);
    // A FRAME WITHDRAWN TAKES WHAT WAS DRAWN THERE WITH IT; one placed afresh draws whole.
    for (auto it = frames_.begin(); it != frames_.end();) {
        if (frames.count(it->first) != 0) {
            ++it;
            continue;
        }
        frames_asked_.erase(it->first);
        it = frames_.erase(it);
    }
    for (const auto& [frame, kind] : frames) {
        frames_.try_emplace(frame, FrameDrawing{kind, WindowDrawing{kWorkshopProvider, frame}});
    }
    tell_keys(mail);
    if (!graphics_asked_ && (!window_.shown() || !overlay_.shown())) {
        graphics_asked_ = true;
        to_directory(graphics::RoomAsked{kWorkshopProvider});
        to_directory(graphics::RoomAsked{kWorkshopOverlayArea});
    }
    for (const auto& [frame, drawn] : frames_) {
        if (!drawn.drawing.shown() && frames_asked_.insert(frame).second) {
            to_directory(graphics::RoomAsked{frame});
        }
    }
}

bool WorkshopWeave::desk_in_front_of_panes() const {
    return desk_in_front(session_) || !carried_.data.empty();
}

// WL-FOCUS-04 -- agents/workshop/focus.md
void WorkshopWeave::tell_keys(loom::Mail& mail) {
    const std::int64_t typing = typing_pane(session_);
    std::map<std::int64_t, std::pair<std::uint64_t, bool>> told;
    for (const std::int64_t kind : placed_kinds_) {
        const RuntimePane* row = session_.panes.runtime.of_kind(kind);
        if (row == nullptr || !host_->holder_accepts ||
            !host_->holder_accepts(row->provider, *loom::schema_of<PaneKeys>())) {
            continue;
        }
        // A HOLDER IS KNOWN BY ITS WEAVE AND ITS INCARNATION: one started again knows nothing.
        const loom::WeaveId holder =
            host_->role_holder ? host_->role_holder(row->provider) : loom::WeaveId{};
        const std::uint64_t life =
            holder.value ^ (host_->incarnation_of && holder.valid()
                                ? static_cast<std::uint64_t>(host_->incarnation_of(holder)) << 32
                                : 0);
        const std::pair<std::uint64_t, bool> now{life, typing == kind};
        const auto was = keys_told_.find(kind);
        if (was == keys_told_.end() || was->second != now || keys_owed_.count(kind) != 0) {
            (void)mail.as_role(kWorkshopProvider)
                .send_to_role(row->provider, PaneKeys{row->pane, now.second});
        }
        told[kind] = now;
    }
    keys_told_ = std::move(told);
    keys_owed_.clear();
}

void WorkshopWeave::draw_window(loom::Mail& mail) {
    if (window_.shown()) {
        window_.draw(workshop_window_drawing(session_).sketch, mail);
    }
    for (auto& [frame, drawn] : frames_) {
        if (drawn.drawing.shown()) {
            drawn.drawing.draw(workshop_frame_drawing(session_, drawn.kind).sketch, mail);
        }
    }
    if (overlay_.shown()) {
        overlay_.draw(workshop_overlay_drawing(session_, window_overlays_), mail);
    }
}

WorkshopWeave::FrameDrawing* WorkshopWeave::frame_of(const std::string& area) {
    const auto it = frames_.find(area);
    return it == frames_.end() ? nullptr : &it->second;
}

bool WorkshopWeave::own_pointer_area(const std::string& area, const loom::Mail& mail) {
    if (window_.from_window(area, mail)) {
        return true;
    }
    const FrameDrawing* frame = frame_of(area);
    return frame != nullptr && frame->drawing.from_window(area, mail);
}

std::pair<std::int64_t, std::int64_t> WorkshopWeave::area_origin(const std::string& area) const {
    const auto at = areas_placed_.find(area);
    if (at == areas_placed_.end()) {
        return {0, 0};
    }
    return {at->second.x, at->second.y};
}

// WL-ATTN-12 -- agents/workshop/attention.md
void WorkshopWeave::say_conditions(const ProjectFrontier& frontier, loom::Mail& mail) {
    std::vector<StandingCondition> now = standing_conditions(session_, frontier);
    if (conditions_said_ && same_conditions(now, said_conditions_)) {
        // NO NEWS IS SILENCE, and silence is what makes this seam terminate. A pane that
        // hears a publication says its rows; content ending in a repaint would publish
        // again; the loop has no quiet state unless this arm exists.
        return;
    }
    said_conditions_ = now;
    conditions_said_ = true;
    // `to_any`, NOT ADDRESSED, AND SAID AS THE OFFICE. Which weave presents this is the load
    // plan's business, and a host that addressed one would be a host with a pane compiled
    // into it again -- but a reader still has to be able to tell this host's reading from
    // any weave's opinion, so the publication carries the office stamp and the pane refuses
    // an unstamped one. `as_role` adds provenance, never a capability (MSG-07).
    (void)mail.as_role(kWorkshopProvider).publish(StandingConditions{std::move(now)});
}

// WL-EDIT-03 -- agents/workshop/editor.md
// WL-SESSION-13 -- agents/workshop/session.md
// WL-GUEST-09 -- agents/workshop/guests.md
void WorkshopWeave::quit(loom::Mail& mail) {
    // A QUIT WRITES THE LAST SESSION, so it is a file write, judged for the hand that asked.
    if (const std::string refused = judge_classes(input_actor_, {scope::kWrite}, {}); !refused.empty()) {
        say("Workshop stays open -- " + refused + "; quitting writes the last session", true);
        return;
    }
    if (quitting_) {
        say("quitting -- waiting for " + std::to_string(quit_outstanding_) +
                " pane(s) to answer",
            true);
        return;
    }
    // The unsaved-loss floor at the one exit, asked of the room: every pane holding a weaver's
    // unsaved work accepts the question, and Loom's fan-out count is how many answers are owed
    // (zero: the exit proceeds now). Gestures are held until the last answer (`hold_input`). An
    // answer that can never come does not hold the hands: a refused delivery is written in the
    // host's book and refuses the quit (`quit_delivery.hpp`). The ask's number is minted by the
    // paste book, so an answer to it can never settle a paste.
    quit_ask_ = paste_asks_.mint_correlation();
    const loom::OfficePublication asked =
        mail.as_role(kWorkshopProvider).publish(PaneQuitRequested{}, quit_ask_);
    if (!asked.authored) {
        // THIS WEAVE DOES NOT HOLD ITS OWN OFFICE, so it cannot ask and cannot know. A host
        // composed that way is a defect worth reading, not a reason to lose a weaver's work.
        say("Workshop could not ask its panes whether they hold unsaved work -- it does not "
            "hold its own office; Workshop stays open",
            true);
        return;
    }
    if (asked.recipients == 0) {
        finish_quit();
        return;
    }
    quitting_ = true;
    quit_outstanding_ = asked.recipients;
    quit_refusals_.clear();
    held_input_.clear();
    held_dropped_ = 0;
}

// WL-SESSION-13 -- agents/workshop/session.md
void WorkshopWeave::finish_quit() {
    quitting_ = false;
    held_input_.clear();
    save_last_session();
    host_->quit = true;
    if (host_->request_stop) {
        host_->request_stop();
    }
}

// WL-SESSION-13 -- agents/workshop/session.md
// WL-GUEST-09 -- agents/workshop/guests.md
void WorkshopWeave::on(const WorkshopQuitRequested&, loom::Mail& mail) {
    // ...and asked by message, the asker is the hand: a guest's quit on a weaver's host is refused.
    const InputActor asker{true, false, mail.sender()};
    if (const std::string refused = judge_classes(asker, {scope::kWrite}, {}); !refused.empty()) {
        (void)mail.answer(loom::Refused{"Workshop stays open -- " + refused + "; quitting writes the last session"});
        return;
    }
    if (quit_asked_.valid()) {
        (void)mail.answer(loom::Refused{"a quit already asked is still waiting for its answer"});
        return;
    }
    quit_asked_ = mail.defer_answer();
    if (!quit_asked_.valid()) {
        return; // nothing can carry an answer back, and nothing is asked
    }
    quit(mail);
    if (quitting_) {
        return; // the room is being asked, and its last answer settles this ask
    }
    // Decided at once: the process ends, or the quit was refused in the words now on the notice.
    answer_quit_ask(host_->quit ? std::string() : session_.notice, mail);
}

void WorkshopWeave::answer_quit_ask(const std::string& why, loom::Mail& mail) {
    if (!quit_asked_.valid()) {
        return;
    }
    loom::DeferredAnswer due = std::move(quit_asked_);
    quit_asked_ = loom::DeferredAnswer{};
    if (why.empty()) {
        (void)loom::answer_deferred(due, mail, loom::Ack{});
    } else {
        (void)loom::answer_deferred(due, mail, loom::Refused{why});
    }
}

// WL-EDIT-03 -- agents/workshop/editor.md
void WorkshopWeave::on(const PaneQuitAnswered& said, loom::Mail& mail) {
    if (!mail.answers_ask() || !quitting_ || mail.correlation() != quit_ask_) {
        return; // not Loom's answer to the ask this host is waiting on
    }
    if (!said.permitted) {
        quit_refusals_.push_back(said.refusal.empty()
                                     ? said.pane + " refused the quit and gave no reason"
                                     : said.refusal);
    }
    if (quit_outstanding_ > 0) {
        --quit_outstanding_;
    }
    if (quit_outstanding_ > 0) {
        return; // more answers are owed, and the decision waits for the last of them
    }
    if (quit_refusals_.empty()) {
        quitting_ = false;
        answer_quit_ask({}, mail);
        finish_quit();
        return;
    }
    std::string why;
    for (const std::string& one : quit_refusals_) {
        if (!why.empty()) {
            why += "; ";
        }
        why += one;
    }
    refuse_quit(std::move(why), mail);
}

// WL-SESSION-19 -- agents/workshop/session.md
void WorkshopWeave::on(const QuitDeliveryRefusalNoted&, loom::Mail& mail) {
    // THE BOOK IS TAKEN WHOLE, EVERY TIME, so nothing in it outlives the wake-up that followed it:
    // an entry for an ask that already ended -- refused, or answered -- is discarded here, and
    // one written while no quit is in flight means nothing to this weave.
    const std::vector<UndeliveredQuit> noted = host_->undelivered_quits.take();
    if (!quitting_) {
        return;
    }
    std::string undelivered;
    for (const UndeliveredQuit& one : noted) {
        // THE ASK IS THIS WEAVE'S OWN NUMBER (`paste_asks_.mint_correlation`), and the watch took
        // the entry only from a refusal the bus stamped as this weave's -- so an equal number is
        // this quit's delivery and nothing else's. An older quit's late fact carries an older one.
        if (one.ask != quit_ask_) {
            continue;
        }
        undelivered += (undelivered.empty() ? "the quit could not ask " : "; nor ") +
                       undelivered_quit_words(one);
    }
    if (undelivered.empty()) {
        return;
    }
    // ONE KNOWN REFUSAL MAKES THIS QUIT IMPOSSIBLE, so it ends now: an answer still owed by
    // another pane cannot turn it back into an exit, and waiting for it would hold the weaver's
    // hands for nothing. What panes already refused is said too, in their own words.
    std::string why = undelivered + "; Workshop stays open, and a quit after the repair asks again";
    for (const std::string& one : quit_refusals_) {
        why += "; " + one;
    }
    refuse_quit(std::move(why), mail);
}

// WL-SESSION-19 -- agents/workshop/session.md
void WorkshopWeave::refuse_quit(std::string why, loom::Mail& mail) {
    quitting_ = false;
    quit_ask_ = 0;
    quit_outstanding_ = 0;
    quit_refusals_.clear();
    if (held_dropped_ > 0) {
        why += " (" + std::to_string(held_dropped_) +
               " gesture(s) that arrived while the quit was pending were dropped)";
    }
    answer_quit_ask(why, mail);
    say(std::move(why), true);
    // AND THE WEAVER'S HANDS GET BACK WHAT THEY DID MEANWHILE, in order, through the same
    // handlers -- a refused quit costs no keystroke.
    replay_held(mail);
    repaint(mail);
}

bool WorkshopWeave::hold_input(HeldInput held) {
    if (!quitting_) {
        return false;
    }
    if (held_input_.size() >= kMaxHeldInput) {
        ++held_dropped_;
        return true;
    }
    held.actor = input_actor_;
    held_input_.push_back(std::move(held));
    return true;
}

void WorkshopWeave::replay_held(loom::Mail& mail) {
    std::vector<HeldInput> replay;
    replay.swap(held_input_);
    held_dropped_ = 0;
    struct RestoreActor {
        InputActor& current;
        InputActor before;
        ~RestoreActor() { current = before; }
    } restore{input_actor_, input_actor_};
    for (const HeldInput& held : replay) {
        input_actor_ = held.actor;
        switch (held.kind) {
        case HeldInput::Kind::kKey: on(held.key, mail); break;
        case HeldInput::Kind::kText: on(held.text, mail); break;
        case HeldInput::Kind::kButton: on(held.button, mail); break;
        case HeldInput::Kind::kMoved: on(held.moved, mail); break;
        case HeldInput::Kind::kWheel: on(held.wheel, mail); break;
        case HeldInput::Kind::kAreaPress: area_pressed(held.area_press, held.press, mail); break;
        }
    }
}

} // namespace zengine::workshop
