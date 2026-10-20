// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s startup files, the host's conditions, and the surface and key handlers.
// Workshop law: agents/workshop/attention.md (+7 registers; agents/workshop.md routes)

#include "weave.hpp"

namespace zengine::workshop {

// The terminal door reads the same host this weave does.
WorkshopWeave::WorkshopWeave(HostContext& host) : Weave(host), host_(&host) {}

// WL-KEY-07, WL-KEY-08 -- agents/workshop/keyboard.md
void WorkshopWeave::load_keymap(loom::Mail& mail) {
    if (keymap_loaded_) {
        return;
    }
    keymap_loaded_ = true;
    if (host_->keymap_path.empty()) {
        keymap_standing_ = "no keymap file -- the shipped bindings stand";
        return;
    }
    if (!std::filesystem::exists(host_->keymap_path)) {
        keymap_standing_ = "not written yet -- the shipped bindings stand; a row written there "
                           "applies at the next launch";
        return;
    }
    // READ ONCE, AND THE BYTES KEPT: they are the baseline an edit's write compares the file
    // against, so a change by another hand since this read is seen rather than overwritten.
    const persist::FileText read = persist::read_file(
        host_->keymap_path, keymap_persist::kMaxKeymapBytes, "a Workshop keymap");
    const keymap_persist::LoadedKeymap loaded =
        read.outcome.accepted ? keymap_persist::from_text(read.text)
                              : keymap_persist::LoadedKeymap{read.outcome, {}};
    if (read.outcome.accepted) {
        keymap_bytes_ = read.text;
        keymap_file_present_ = true;
    }
    if (!loaded.outcome.accepted) {
        // A standing wall, said as one: every later launch meets it, so it is a condition under
        // its own key, retracted by nobody, since nothing in a run makes the file readable.
        keymap_bad_ = true;
        keymap_standing_ = "refused, so the shipped bindings stand: " + loaded.outcome.refusal;
        session_.conditions.establish(
            Condition{kKeymapWallKey, "keymap refused -- default bindings stand",
                      loaded.outcome.refusal, surface::role::kAlert, std::string()});
        return;
    }
    session_.keymap = loaded.keymap;
    // And every pane that declared rows before the file loaded is joined again under it, so both
    // load orders end the same way: the file in force, and a colliding pane's rows refused in
    // words (WL-KEY-15).
    // A ROW FOR AN ID WHOSE OWNER CHANGED IS READ AS ITS SUCCESSOR, and the load says so, naming
    // the rename that would make the file say what it means (`kRenamedActions`).
    std::string renamed;
    for (const AuthoredOverride& o : session_.keymap.authored) {
        if (const char* now = renamed_to(o.action)) {
            renamed += (renamed.empty() ? "" : "; ") + ("`" + o.action + "` is read as `" +
                                                        std::string(now) + "` -- rename it there");
        }
    }
    // ...AND A ROW FOR AN ID THAT RETIRED IS SAID AS ONE (`kRetiredActions`): kept byte for byte,
    // answered by nothing, and named with what took it, so a weaver is not left wondering whether
    // a pane might yet declare it.
    std::string retired;
    for (const AuthoredOverride& o : session_.keymap.authored) {
        if (const char* with = retired_with(o.action)) {
            const char* instead = retired_instead(o.action);
            retired += (retired.empty() ? "" : "; ") +
                       ("`" + o.action + "` retired with " + std::string(with) +
                        " -- kept, and nothing answers it" +
                        (instead != nullptr && *instead != 0
                             ? " (" + std::string(instead) + ")"
                             : std::string()));
        }
    }
    keymap_standing_ = "applied -- " + std::to_string(session_.keymap.authored.size()) +
                       " authored row" + (session_.keymap.authored.size() == 1 ? "" : "s") +
                       (renamed.empty() ? std::string() : "; " + renamed) +
                       (retired.empty() ? std::string() : "; " + retired);
    if (!renamed.empty()) {
        session_.keymap.note += (session_.keymap.note.empty() ? "" : "; ") + renamed;
    }
    if (!retired.empty()) {
        session_.keymap.note += (session_.keymap.note.empty() ? "" : "; ") + retired;
    }
    std::string refused;
    // THE APPLICATION'S ROWS FIRST, because a pane is judged against them (WL-DESK-07): the file
    // replaced the whole map, and the declaration the desktop has in force is owed the weaver's
    // overrides now rather than whenever the desktop next happens to speak.
    rejoin_app_rows(refused, mail);
    std::string gaps;
    rejoin_pane_rows(refused, gaps, mail);
    if (!session_.keymap.authored.empty() || session_.keymap.legend != legend_mode::kDefault) {
        keymap_word_ = "keymap " + host_->keymap_path + " applied -- " +
                       std::to_string(session_.keymap.authored.size()) + " override" +
                       (session_.keymap.authored.size() == 1 ? "" : "s");
        if (!session_.keymap.note.empty()) {
            keymap_word_ += "; " + session_.keymap.note;
        }
    }
    for (const std::string* part : {&refused, &gaps}) {
        if (part->empty()) {
            continue;
        }
        if (!keymap_word_.empty()) {
            keymap_word_ += "; ";
        }
        keymap_word_ += *part;
    }
}

// WL-FOCUS-11 -- agents/workshop/focus.md
void WorkshopWeave::load_prefs() {
    if (prefs_loaded_) {
        return;
    }
    prefs_loaded_ = true;
    if (host_->prefs_path.empty()) {
        return;
    }
    if (!std::filesystem::exists(host_->prefs_path)) {
        return;
    }
    // READ ONCE, AND THE BYTES KEPT: the baseline a write compares the file against, as the
    // keymap's are, so a change by another hand since this read is seen rather than overwritten.
    const persist::FileText read = persist::read_file(
        host_->prefs_path, prefs_persist::kMaxPrefsBytes, "a Workshop prefs file");
    if (read.outcome.accepted) {
        prefs_bytes_ = read.text;
        prefs_file_present_ = true;
    }
    const prefs_persist::LoadedPrefs loaded =
        read.outcome.accepted ? prefs_persist::from_text(read.text)
                              : prefs_persist::LoadedPrefs{read.outcome, {}, {}};
    if (!loaded.outcome.accepted) {
        // The keymap wall's twin: `prefs_bad_` blocks every later write. The condition is the
        // standing half; a toggle's refusal stays an event about the press.
        prefs_bad_ = true;
        session_.conditions.establish(
            Condition{kPrefsWallKey, "preferences refused -- defaults stand",
                      loaded.outcome.refusal, surface::role::kAlert, std::string()});
        return;
    }
    prefs_ = loaded.kept;
    apply_workshop_settings();
}

// WL-ATTN-02 -- agents/workshop/attention.md
void WorkshopWeave::speak_startup_notes(loom::Mail& mail) {
    if (startup_spoken_) {
        return;
    }
    startup_spoken_ = true;
    std::string word;
    // AN OBJECT DOCUMENT THIS RUN WAS POINTED AT, SAID ONCE AND LEFT ALONE (WL-DOC-22): the file is
    // a weaver's, the canvas that read it retired, and nothing here opens, rewrites or deletes it.
    const std::string retired =
        host_->retired_document.empty()
            ? std::string()
            : "object document " + host_->retired_document +
                  " left as it is -- the object canvas retired, and nothing here reads or "
                  "writes it";
    const std::string* parts[] = {&keymap_word_, &host_->transition_note, &retired};
    for (const std::string* part : parts) {
        if (part->empty()) {
            continue;
        }
        if (!word.empty()) {
            word += "; ";
        }
        word += *part;
    }
    if (word.empty()) {
        return;
    }
    say(word, false);
    repaint(mail);
}

// WL-ATTN-01, WL-ATTN-02 -- agents/workshop/attention.md
void WorkshopWeave::take_host_conditions() {
    conditions_taken_ = host_->conditions_generation;
    for (const Condition& c : host_->standing_conditions) {
        session_.conditions.establish(c);
    }
}

void WorkshopWeave::on(const zengine::surface::SurfaceReady&, loom::Mail& mail) {
    start_surface(mail);
}

void WorkshopWeave::start_surface(loom::Mail& mail) {
    surface_started_ = true;
    (void)mail.as_role(kWorkshopProvider).publish(PaneCatalogRequested{});
    // The first picture of a run is Workshop's floor: the smallest room it is honest in comes
    // first and the room it wants back second, so a weaver can always shrink the window.
    load_keymap(mail);
    // The prefs beside it, BEFORE the first paint: the first band and the
    // first pane headers a weaver reads are already wearing their own preference.
    load_prefs();
    // ...and whatever the host already knew was standing, so the first picture
    // of the run already carries every condition this launch is going to have.
    take_host_conditions();
    repaint(mail);
    restore_last_session(mail);
    speak_startup_notes(mail);
}

// WL-GEO-08 -- agents/workshop/geometry.md
void WorkshopWeave::on(const zengine::surface::SurfaceExtent& e, loom::Mail& mail) {
    // The medium names its unit here, so the restore's sentence is spelled in it.
    const bool respelled = respell_restore_notice(e.cell_px);
    if (!adopt_screen(session_, e.width, e.height, e.text_advance_px, e.text_line_px,
                      e.cell_px)) {
        if (respelled) {
            repaint(mail);
        }
        return;
    }
    // The normal window's room follows the screen, except while this run's medium says the window
    // is maximized (placement arrives before extent on its beat). A maximized flag restored from
    // a file does not gate: this run's resizes are this run's to remember.
    if (!(medium_placed_ && session_.place_maximized)) {
        session_.normal_w = session_.screen_w;
        session_.normal_h = session_.screen_h;
    }
    // A new room opens and closes nothing: the stack is not rationed, so the repaint lays the
    // same panes out in it, the column beginning again at its top where it no longer reaches.
    repaint(mail);
}

void WorkshopWeave::on(const graphics::GraphicsReady&, loom::Mail& mail) {
    if (!mail.authored_from_role(graphics::kGraphicsRole)) {
        return;
    }
    graphics_known_ = false;
    show_in_graphics(mail);
}

static_assert(graphics::kCellPx == surface::kCanvasCellPx,
              "the window draws the desk one canvas pixel to one of its own, so its cell is the "
              "canvas cell and its room in cells is the screen in cells");

void WorkshopWeave::on(const graphics::WindowOffered& w, loom::Mail& mail) {
    if ((w.window != graphics::kTerminalWindow && w.window != graphics::kSdlWindow) ||
        !mail.authored_from_role(std::string(graphics::kWindowOfficePrefix) + w.window)) {
        return;
    }
    // THE WINDOW THE DESK IS PLACED ON: the one that offered itself, the terminal's or the OS's.
    // Another one offering itself takes every area afresh.
    window_offered_ = true;
    if (window_name_ != w.window) {
        window_name_ = w.window;
        areas_placed_.clear();
        skins_chosen_.clear();
        graphics_asked_ = false;
    }
    // A HOST WITH NO SKIN COMES UP IN THE WINDOW, as a skin's hello brings it up: the session
    // restored first, and the window's room taken after it, as a skin's extent follows its hello.
    if (!surface_started_) {
        start_surface(mail);
    }
    // THE WINDOW'S ROOM, WHILE NO MEDIUM HAS NAMED ITS UNIT: its cells, as a terminal skin reports
    // its own, text a cell and the device unit the cell. A skin's `SurfaceExtent` owns the room
    // from then on; until one comes, a window that offers another size is laid out in again.
    if (medium_unit_known_ || w.cols <= 0 || w.rows <= 0) {
        return;
    }
    if (!adopt_screen(session_, surface::px_of_cells(w.cols), surface::px_of_cells(w.rows))) {
        return;
    }
    if (!(medium_placed_ && session_.place_maximized)) {
        session_.normal_w = session_.screen_w;
        session_.normal_h = session_.screen_h;
    }
    repaint(mail);
}

void WorkshopWeave::on(const graphics::AreaRoom& r, loom::Mail& mail) {
    // A room said by anyone but the window it names, about another area, or the same again, is
    // nothing new (`WindowDrawing::room`).
    const bool desk = window_.room(r, mail);
    const bool overlay = !desk && overlay_.room(r, mail);
    if (desk || overlay) {
        draw_window(mail);
    }
}

void WorkshopWeave::on(const graphics::DrawnRefused& refused, loom::Mail& mail) {
    if (!window_.refused(refused, mail)) {
        (void)overlay_.refused(refused, mail);
    }
}

namespace {

/// WHILE A MOMENT A GRAPHICS WINDOW GAVE IS ANSWERED: `on` holds true, and the moment is the
/// weaver's own local hand's, as the platform's input is.
template <class Actor>
struct WindowMoment {
    bool& on;
    Actor& actor;
    bool was_on;
    Actor was;
    WindowMoment(bool& flag, Actor& who) : on(flag), actor(who), was_on(flag), was(who) {
        on = true;
        actor = Actor{true, true, loom::WeaveId{}};
    }
    ~WindowMoment() {
        on = was_on;
        actor = was;
    }
    WindowMoment(const WindowMoment&) = delete;
    WindowMoment& operator=(const WindowMoment&) = delete;
};

} // namespace

// WL-PRESS-04 -- agents/workshop/press-chain.md
void WorkshopWeave::on(const graphics::AreaButtonChanged& b, loom::Mail& mail) {
    if (!window_.from_window(b.area, mail)) {
        return;
    }
    const auto own = areas_placed_.find(kWorkshopProvider);
    const std::int64_t x = b.x + (own != areas_placed_.end() ? own->second.x : 0);
    const std::int64_t y = b.y + (own != areas_placed_.end() ? own->second.y : 0);
    // ITS OWN HOLD, AS THE WINDOW HOLDS IT: where the press was made, until its release.
    if (b.button >= 1 && b.button <= 3) {
        own_holds_[static_cast<std::size_t>(b.button - 1)] =
            b.pressed ? WindowHold{true, kNoPaneKind, b.area, mail.correlation(), x, y}
                      : WindowHold{};
    }
    const WindowMoment moment{window_moment_, input_actor_};
    pointer_button(input::PointerButton{b.button, b.pressed, x, y, input::space::kPixels,
                                        b.modifiers},
                   mail);
}

void WorkshopWeave::on(const graphics::AreaPointerMoved& m, loom::Mail& mail) {
    if (!window_.from_window(m.area, mail)) {
        return;
    }
    const auto own = areas_placed_.find(kWorkshopProvider);
    const std::int64_t x = m.x + (own != areas_placed_.end() ? own->second.x : 0);
    const std::int64_t y = m.y + (own != areas_placed_.end() ? own->second.y : 0);
    const WindowMoment moment{window_moment_, input_actor_};
    pointer_moved(input::PointerMoved{x, y, m.dx, m.dy, input::space::kPixels, m.modifiers}, mail);
}

// WL-PTR-10 -- agents/workshop/pointer.md
void WorkshopWeave::on(const graphics::AreaWheelTurned& w, loom::Mail& mail) {
    if (!window_.from_window(w.area, mail)) {
        return;
    }
    const auto own = areas_placed_.find(kWorkshopProvider);
    const std::int64_t x = w.x + (own != areas_placed_.end() ? own->second.x : 0);
    const std::int64_t y = w.y + (own != areas_placed_.end() ? own->second.y : 0);
    const WindowMoment moment{window_moment_, input_actor_};
    pointer_wheel(input::PointerWheel{w.dx, w.dy, x, y, input::space::kPixels, w.modifiers}, mail);
}

bool WorkshopWeave::placed_in_window(std::int64_t kind) const {
    return window_moment_ && placed_kinds_.count(kind) != 0;
}

// WL-FOCUS-03 -- agents/workshop/focus.md; WL-CANVAS-05 -- agents/workshop/canvas.md
// WL-CTX-08 -- agents/workshop/contextual.md; WL-GUIDE-06 -- agents/workshop/guide.md
void WorkshopWeave::on(const graphics::AreaPressed& p, loom::Mail& mail) {
    if (p.window != window_name_ ||
        !mail.authored_from_role(std::string(graphics::kWindowOfficePrefix) + p.window) ||
        p.button < 1 || p.button > 3 || quitting_) {
        return;
    }
    // THE PANE WHOSE AREA IT IS, among those this desk placed in that window.
    std::int64_t kind = kNoPaneKind;
    for (const std::int64_t placed : placed_kinds_) {
        const RuntimePane* row = session_.panes.runtime.of_kind(placed);
        if (row != nullptr && pane_window_area(session_.panes, *row) == p.area) {
            kind = placed;
        }
    }
    if (kind == kNoPaneKind) {
        return;
    }
    const WindowMoment moment{window_moment_, input_actor_};
    const std::uint64_t press = mail.correlation();
    const auto slot = static_cast<std::size_t>(p.button - 1);
    const PointedAt at = canvas_point_of(input::space::kPixels, p.x, p.y);
    if (!p.pressed) {
        // THE RELEASE ENDS THE HOLD AND RESTORES NO ELIGIBILITY: the release is no gesture.
        WindowHold& held = pane_holds_[slot];
        if (held.active && held.press == press) {
            held = WindowHold{};
        }
        if (slot > 0) {
            each_hand([&](Hand& h) {
                SecondaryContinuation& c = h.secondary_cont[slot - 1];
                if (c.live && !c.released && c.correlation == press) c.released = true;
            });
        }
        if (slot == 0) {
            (void)release_value_drag(
                input::PointerButton{1, false, p.x, p.y, input::space::kPixels, input::mod::kNone},
                mail);
        }
        repaint(mail);
        return;
    }
    count_gesture();
    leave_canvas_hover(mail);
    // THE WEAVER'S OWN PRESS ENDS A GUIDE MARK IT LANDS IN; the press itself is the pane's.
    if (!guide_marks_.empty()) {
        guide_pressed(mail, at);
    }
    pane_holds_[slot] = WindowHold{true, kind, p.area, press, p.x, p.y};
    if (slot == 0) {
        // A primary press chooses the pane and moves the keys, as on Workshop's canvas, and is
        // the pane's to continue: under the window's number it may ask to carry a value out.
        session_.panes.selected = kind;
        session_.panes.keyboard = kind_takes_keyboard(kind) ? kind : kNoPaneKind;
        keep_act(hand(), hand().press_sent = GestureSent{kind, hand().latest, press});
        begin_value_drag(
            input::PointerButton{1, true, p.x, p.y, input::space::kPixels, input::mod::kNone});
    } else {
        // A secondary press is the pane's, which may hand it back or ask for its menu under the
        // window's number, and a gesture for the other button's continuation.
        for (SecondaryContinuation& other : hand().secondary_cont) {
            if (other.live && other.button != p.button) other.interrupted = true;
        }
        SecondaryContinuation& c = hand().secondary_cont[slot - 1];
        c = SecondaryContinuation{};
        c.live = true;
        c.kind = kind;
        c.button = p.button;
        c.correlation = press;
        c.gesture_at_press = hand().latest;
        c.cell = at;
    }
    note_routed(kind);
    repaint(mail);
}

// WL-SESSION-08, WL-SESSION-09 -- agents/workshop/session-restore.md
void WorkshopWeave::on(const zengine::surface::SurfacePlacement& p, loom::Mail&) {
    medium_placed_ = true;
    session_.placement_known = true;
    session_.place_x = p.x;
    session_.place_y = p.y;
    session_.place_maximized = p.maximized;
}

// WL-SESSION-13 -- agents/workshop/session.md
void WorkshopWeave::on(const zengine::surface::SurfaceCloseRequested&, loom::Mail& mail) {
    quit(mail);
}

// WL-KEY-03, WL-KEY-05, WL-KEY-12 -- agents/workshop/keyboard.md
// WL-FOCUS-13 -- agents/workshop/focus.md
void WorkshopWeave::on(const zengine::input::KeyPressed& k, loom::Mail& mail) {
    if (duplicate_input(mail)) return;
    // WHILE THE ROOM IS BEING ASKED WHETHER THIS WORKSHOP MAY END, NOTHING IS ROUTED. Held,
    // and replayed if the answer is no (`quit`).
    if (quitting_) {
        HeldInput held;
        held.kind = HeldInput::Kind::kKey;
        held.key = k;
        (void)hold_input(std::move(held));
        return;
    }
    count_gesture();
    // ESCAPE PUTS DOWN WHAT ITS OWN HAND CARRIES: the weaver's hand is its attributed local input
    // and its unattributed input alike, a graphics window's keys among them.
    const Hand* carrier = carried_.data.empty() ? nullptr : find_hand(carried_.actor);
    if (carrier != nullptr && k.scancode == input::scan::kEscape &&
        find_hand(input_actor_) == carrier) {
        carried_ = {};
        say("Reference put down", false);
        repaint(mail);
        return;
    }
    // THE CONTEXT IS RESOLVED ONCE, AT ENTRY, and every decision this turn -- the
    // above-mode arm, the swallow, the chain -- spends the same answer, so a mode a
    // dispatch arm opens cannot change what THIS keystroke meant.
    const KeyContext ctx = keyboard_context(session_);
    // The swallow belongs to one moment: cleared on every key, armed only when this keystroke is
    // consumed as a binding whose key also enters text (`expected_text_of`); an unmatched
    // expectation eats nothing.
    hand().swallow_text.clear();
    if (ctx == KeyContext::kContext && session_.presented.open) {
        // Every key a presented menu takes is its presenter's, so the character it produced is
        // part of that act, not a second one.
        hand().swallow_text = expected_text_of(k.scancode, k.modifiers);
    } else if (session_.keymap.action_for(ctx, k.scancode, k.modifiers, keyboard_pane()) !=
        Act::kNone) {
        hand().swallow_text = expected_text_of(k.scancode, k.modifiers);
    } else if (session_.keymap.app_action_for(app_precedence::kAboveModes, ctx, k.scancode,
                                             k.modifiers, keyboard_pane()) != nullptr) {
        // An application row is a binding too (WL-DESK-07): a desktop's launch on a bare letter
        // must not also type that letter.
        hand().swallow_text = expected_text_of(k.scancode, k.modifiers);
    } else if (ctx == KeyContext::kPane &&
               session_.keymap.pane_action_for(keyboard_pane(), k.scancode, k.modifiers) !=
                   nullptr) {
        // A PANE'S OWN ROW IS A BINDING TOO (WL-KEY-15): the keystroke crosses as the
        // resolved id, and the character it produced belongs to the trigger, not to the
        // pane's field.
        hand().swallow_text = expected_text_of(k.scancode, k.modifiers);
    }
    // Only the rows declared above the modes are answered here; `workshop.quit`'s ordinary `q`
    // row still travels the chain, behind every mode.
    switch (session_.keymap.above_mode_action(ctx, k.scancode, k.modifiers, keyboard_pane())) {
    case Act::kQuit:
        // A quit REFUSED says so on the notice line, which has to be painted to be read;
        // a quit that proceeded publishes one last unchanged frame on its way out, which
        // costs nothing anybody sees; a quit that is ASKING paints the desk as it is.
        quit(mail);
        repaint(mail);
        return;
    case Act::kFocusLeave: {
        // THE WAY OUT THAT NO PANE KEEPS: whatever has the keys -- a menu, the arrangement, the
        // name editor, a pane in a mode of its own that spends Escape -- gives them back to
        // Workshop, and the pane left stays selected for the keys to find it again.
        bool left = false;
        if (session_.presented.open) {
            withdraw_menu("the weaver left it", mail);
            left = true;
        }
        if (session_.context.open) {
            close_context();
            left = true;
        }
        if (session_.arrange.open) {
            close_arrange();
            left = true;
        }
        if (session_.setup.naming.open) {
            close_naming();
            left = true;
        }
        const std::int64_t pane = session_.panes.keyboard;
        if (pane != kNoPaneKind) {
            if (session_.panes.has(pane)) {
                session_.panes.selected = pane;
            }
            session_.panes.keyboard = kNoPaneKind;
            left = true;
        }
        const std::string name = pane == kNoPaneKind ? std::string()
                                                     : kind_name(session_.panes, pane);
        say(!left          ? std::string("Workshop already has the keys")
            : name.empty() ? std::string("Workshop has the keys")
                           : "Workshop has the keys -- left " + name,
            false);
        repaint(mail);
        return;
    }
    default: break;
    }
    // The chain is `keyboard_context`'s answer; this switch names the owner it resolved. A copy or
    // paste anywhere below is noticed once, here, by comparing the clipboard's counters around the
    // chain: a copy publishes `ClipboardCopy`, a paste asks the Skin for the current value
    // (`begin_clipboard_paste`). The application's own rows come first, above every mode and
    // below this host's own above-mode rows; a pane keeps a gesture only by declaring it owns
    // it (the laws WL-DESK-07 and WL-ARR-15), never by silence.
    if (const AppRow* row = session_.keymap.app_action_for(app_precedence::kAboveModes, ctx,
                                                           k.scancode, k.modifiers,
                                                           keyboard_pane())) {
        request_app_action(row->id, mail);
        repaint(mail);
        return;
    }
    const std::uint64_t copied_before = session_.clipboard.writes;
    const std::uint64_t pastes_before = session_.clipboard.paste_requests;
    bool crossed = true; // whether a key sent to a pane crossed at all (`external_key`)
    switch (ctx) {
    case KeyContext::kArrangePane:
    case KeyContext::kArrangeDesk:
    case KeyContext::kArrangeReset: arrange_key(k, mail); break;
    case KeyContext::kNaming: naming_key(k, mail); break;
    case KeyContext::kContext: context_key(k, mail); break;
    case KeyContext::kPane: crossed = external_key(keyboard_pane(), k, mail); break;
    default: command(k, mail); break;
    }
    // Escape's final meaning is asked last, after the resolved context had the key. A bare Escape
    // no binding claimed, where the keys are held by a list, by nothing, or by a pane that took no
    // key, requests the desktop's default-class row (the law WL-DESK-02); a pane that took the key
    // answers with `PaneEscapeUnspent`, and a place a weaver types into keeps Escape. Not a keymap
    // action: a recovery gesture must not be authorable into a lockout.
    if ((default_row_context(ctx) || (ctx == KeyContext::kPane && !crossed)) &&
        session_.keymap.action_for(ctx, k.scancode, k.modifiers, keyboard_pane()) ==
            Act::kNone) {
        if (const AppRow* row = session_.keymap.app_action_for(
                app_precedence::kDefault, ctx, k.scancode, k.modifiers, keyboard_pane())) {
            request_app_action(row->id, mail);
        }
    }
    if (session_.clipboard.writes != copied_before) {
        mail.publish(zengine::surface::ClipboardCopy{session_.clipboard.text});
    }
    if (session_.clipboard.paste_requests != pastes_before) {
        begin_clipboard_paste(mail);
    }
    repaint(mail);
}

} // namespace zengine::workshop
