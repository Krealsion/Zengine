// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The bodies of tests/workshop_support.hpp, compiled once into zengine-workshop-support,
// which every Workshop suite links (tests/CMakeLists.txt).

#include "workshop_support.hpp"

namespace zengine::testing {
} // namespace zengine::testing

DrawnMirror::DrawnMirror(loom::Switchboard& bus) : bus_(bus) {
        tap_ = bus_.add_observer([this](const loom::BusEvent& ev) { take(ev); });
    }

DrawnMirror::~DrawnMirror() { bus_.remove_observer(tap_); }

auto DrawnMirror::area(const std::string& name) const -> const zengine::graphics::window::WinArea* {
        const auto at = areas_.find(name);
        return at == areas_.end() ? nullptr : &at->second;
    }

auto DrawnMirror::measure(const std::string& name) const -> gfx::TextMeasure {
        const auto at = measures_.find(name);
        return at == measures_.end() ? gfx::TextMeasure{} : at->second;
    }

auto DrawnMirror::take(const loom::BusEvent& ev) -> void {
        if (ev.kind != loom::EventKind::Delivered || ev.payload == nullptr) return;
        const std::string_view prefix = gfx::kWindowRolePrefix;
        if (ev.schema_name == gfx::Drawn::zen_name) {
            const std::vector<std::string> held = bus_.roles_of(ev.target);
            if (std::none_of(held.begin(), held.end(), [&prefix](const std::string& r) {
                    return r.rfind(prefix, 0) == 0;
                })) {
                return;
            }
            const gfx::Drawn d = loom::from_value<gfx::Drawn>(*ev.payload);
            zengine::graphics::window::WinArea& a = areas_[d.area];
            a.area = d.area;
            a.owner = ev.authored_role;
            (void)zengine::graphics::window::apply(a, d);
        } else if (ev.schema_name == gfx::AreaRoom::zen_name) {
            if (ev.authored_role.rfind(prefix, 0) != 0) return;
            const gfx::AreaRoom r = loom::from_value<gfx::AreaRoom>(*ev.payload);
            zengine::graphics::window::WinArea& a = areas_[r.area];
            a.area = r.area;
            a.w = r.w;
            a.h = r.h;
            if (r.empty) {
                a.nodes.clear();
                a.records.clear();
            }
            measures_[r.area] = r.measure();
        }
    }

auto held_desks() -> std::vector<HeldDesk>& {
    static std::vector<HeldDesk> desks;
    return desks;
}

HeldDeskEntry::HeldDeskEntry(const DrawnMirror& mirror, std::function<const Session&()> session) {
        held_desks().push_back(HeldDesk{&mirror, std::move(session)});
        mirror_ = &mirror;
    }

HeldDeskEntry::~HeldDeskEntry() {
        auto& desks = held_desks();
        for (std::size_t i = desks.size(); i-- > 0;) {
            if (desks[i].mirror == mirror_) {
                desks.erase(desks.begin() + static_cast<std::ptrdiff_t>(i));
                break;
            }
        }
    }

auto box_of(std::size_t length, std::size_t caret, std::size_t first) -> component::TextBox {
    component::TextBox b;
    b.set(std::string(length, 'x'), caret);
    b.keep_caret_visible(caret > first ? static_cast<std::int64_t>(caret - first) : 0);
    return b;
}

auto all_rects(const ScreenCanvas& c) -> std::vector<ScreenRect> {
    std::vector<ScreenRect> out;
    for (const ScreenPlane& l : c.layers) {
        out.insert(out.end(), l.rects.begin(), l.rects.end());
    }
    return out;
}

auto all_labels(const ScreenCanvas& c) -> std::vector<ScreenLabel> {
    std::vector<ScreenLabel> out;
    for (const ScreenPlane& l : c.layers) {
        out.insert(out.end(), l.labels.begin(), l.labels.end());
    }
    return out;
}

auto all_texts(const ScreenCanvas& c) -> std::vector<ScreenRegion> {
    std::vector<ScreenRegion> out;
    for (const ScreenPlane& l : c.layers) {
        out.insert(out.end(), l.texts.begin(), l.texts.end());
    }
    return out;
}

auto without_workspace(const ScreenCanvas& c) -> ScreenCanvas {
    ScreenCanvas out = c;
    if (!out.layers.empty()) {
        out.layers.erase(out.layers.begin());
    }
    return out;
}

auto object_names(const ScreenCanvas& c) -> std::vector<ScreenRegion> {
    return c.layers.empty() ? std::vector<ScreenRegion>{} : c.layers.front().texts;
}

auto plain_cells(const ScreenCanvas& c) -> std::string {
    std::string out;
    for (const std::string& row : zengine::testing::screen_cells::rows_of(c)) {
        out += row;
        out += "\r\n";
    }
    return out;
}

auto cells_of(const Screen& sc) -> ScreenCells {
    const auto c = [](std::int64_t px) { return gfx::cell_of_pixel(px); };
    return ScreenCells{c(sc.w),      c(sc.h),
                       c(sc.room_w - cells_px(kStripCols)), c(sc.room_y),
                       c(sc.room_w), c(sc.room_y + sc.room_h) - c(sc.room_y), c(sc.band_y)};
}

auto help_row(const Screen& sc) -> std::int64_t { return gfx::cell_of_pixel(sc.band_y); }

auto plane(ScreenCanvas& c) -> ScreenPlane& {
    if (c.layers.empty()) {
        c.layers.emplace_back();
    }
    return c.layers.back();
}

auto canvas_of_region(const ScreenCanvas& like,
                                     const ScreenRegion& r) -> ScreenCanvas {
    ScreenCanvas out;
    out.width = like.width;
    out.height = like.height;
    out.layers.emplace_back();
    out.layers.back().texts.push_back(r);
    return out;
}

auto projected_of(const ScreenCanvas& c) -> std::vector<zengine::testing::screen_cells::CellRow> {
    std::vector<zengine::testing::screen_cells::CellRow> out;
    for (const ScreenPlane& l : c.layers) {
        for (auto& r : zengine::testing::screen_cells::region_rows(l)) {
            out.push_back(std::move(r));
        }
    }
    return out;
}

auto cell_text_of(const ScreenCanvas& c) -> std::vector<ScreenLabel> {
    std::vector<ScreenLabel> out;
    for (const ScreenPlane& l : c.layers) {
        out.insert(out.end(), l.labels.begin(), l.labels.end());
        for (auto& p : zengine::testing::screen_cells::region_rows(l)) {
            out.push_back(std::move(p.label));
        }
    }
    return out;
}

auto label_at(const ScreenCanvas& c, std::int64_t x, std::int64_t y) -> std::string {
    for (const ScreenLabel& l : cell_text_of(c)) {
        if (gfx::cell_of_pixel(l.x) == x && gfx::cell_of_pixel(l.y) == y) {
            return l.text;
        }
    }
    return {};
}

auto caret_at(const ScreenCanvas& c, std::int64_t x, std::int64_t y) -> std::int64_t {
    for (const ScreenPlane& l : c.layers) {
        for (const auto& p : zengine::testing::screen_cells::region_rows(l)) {
            if (gfx::cell_of_pixel(p.label.x) == x && gfx::cell_of_pixel(p.label.y) == y) {
                return p.caret;
            }
        }
    }
    return -1;
}

auto inspector_row(const ScreenCanvas& c, std::int64_t x, std::int64_t y) -> std::string {
    std::string text = label_at(c, x, y);
    while (!text.empty() && text.back() == ' ') {
        text.pop_back();
    }
    return text;
}

auto topmost_at(const ScreenCanvas& c, std::int64_t x, std::int64_t y) -> std::string {
    std::string seen;
    for (const ScreenLabel& l : cell_text_of(c)) {
        if (gfx::cell_of_pixel(l.x) == x && gfx::cell_of_pixel(l.y) == y) {
            seen = l.text;
        }
    }
    return seen;
}

auto label_at(const DeskShown& s, std::int64_t x, std::int64_t y) -> std::string {
    return s.text_at_px(gfx::px_of_cells(x), gfx::px_of_cells(y));
}

auto inspector_row(const DeskShown& s, std::int64_t x, std::int64_t y) -> std::string {
    return label_at(s, x, y);
}

auto topmost_at(const DeskShown& s, std::int64_t x, std::int64_t y) -> std::string {
    return label_at(s, x, y);
}

auto rasterized(const DeskShown& s) -> std::vector<std::string> { return s.rows; }

auto plain_cells(const DeskShown& s) -> std::string {
    std::string out;
    for (const std::string& row : s.rows) {
        out += row;
        out += "\r\n";
    }
    return out;
}

auto setup_for(const Panes& panes) -> Setup {
    Setup s;
    s.name = "case";
    for (const OpenPane& p : panes.open) {
        if (is_runtime_kind(p.kind)) {
            if (const RuntimePane* row = panes.runtime.of_kind(p.kind)) {
                (void)add_pane(s, PaneRef{row->provider, row->pane});
            }
            continue;
        }
        (void)add_pane(s, pane_ref_of(p.kind));
    }
    return s;
}

auto one_layout(Setup desk) -> std::vector<Layout> {
    std::vector<Layout> run;
    run.push_back(Layout{std::move(desk), SetupLink{}, 1});
    return run;
}

auto plain_run(std::vector<Setup> desks) -> std::vector<Layout> {
    std::vector<Layout> run;
    run.reserve(desks.size());
    for (Setup& desk : desks) {
        run.push_back(Layout{std::move(desk), SetupLink{}, static_cast<std::int64_t>(run.size()) + 1});
    }
    return run;
}

auto linked_layout(Setup desk, std::string path, Setup known) -> Layout {
    return Layout{std::move(desk), SetupLink{std::move(path), std::move(known)}};
}

auto desks_of(const std::vector<Layout>& run) -> std::vector<Setup> {
    std::vector<Setup> out;
    out.reserve(run.size());
    for (const Layout& layout : run) {
        out.push_back(layout.desk);
    }
    return out;
}

auto numbered(std::vector<Layout> run) -> std::vector<Layout> {
    for (std::size_t i = 0; i < run.size(); ++i) {
        if (run[i].id <= 0) {
            run[i].id = static_cast<std::int64_t>(i) + 1;
        }
    }
    return run;
}

auto next_id_of(const std::vector<Layout>& run) -> std::int64_t {
    std::int64_t top = 0;
    for (const Layout& l : numbered(run)) {
        top = std::max(top, l.id);
    }
    return top + 1;
}

auto session_value(const std::vector<Layout>& run, std::size_t active,
              const std::vector<session_persist::WorkshopChoice>& choices) -> session_persist::WorkshopSession {
    return session_persist::to_session(numbered(run), active, next_id_of(run), choices);
}

auto session_text(const std::vector<Layout>& run, std::size_t active,
                                const std::vector<session_persist::WorkshopChoice>& choices) -> std::string {
    return session_persist::to_text(numbered(run), active, next_id_of(run), choices);
}

auto save_last_run(const std::string& path, const std::vector<Layout>& run,
                             std::size_t active, std::int64_t w, std::int64_t h,
                             const window_persist::Placement& place,
                             const std::vector<session_persist::WorkshopChoice>& choices) -> Written {
    const Written session =
        session_persist::save_file(path, numbered(run), active, next_id_of(run), choices);
    if (!session.accepted) {
        return session;
    }
    return window_persist::save_file(window_persist::path_beside(path), w, h, place);
}

auto live_layout(const session_persist::LoadedSession& loaded) -> const Setup& {
    return loaded.layouts.at(loaded.active).desk;
}

auto live_link(const session_persist::LoadedSession& loaded) -> const SetupLink& {
    return loaded.layouts.at(loaded.active).link;
}

auto live_status(const SetupState& s) -> std::int64_t {
    return link_status(s.active, s.active_link);
}

auto live_status(const LiveDesk& d) -> std::int64_t { return d.link; }

auto live_copy_of(const SetupState& run) -> LiveDesk {
    LiveDesk d;
    // EACH ROW WITH WORKSHOP'S OWN KEYS ALONE, as the owner's picture carries it.
    d.active = run.active;
    for (SetupPane& row : d.active.panes) {
        std::erase_if(row.settings,
                      [](const PaneSetting& s) { return !is_desk_setting_key(s.key); });
    }
    d.owned = d.active;
    d.id = run.active_id;
    d.active_at = run.active_at;
    d.tabs.clear();
    for (const Layout& l : layout_run(run)) {
        d.tabs.push_back(LiveTab{l.id, l.desk.name, link_status(l.desk, l.link)});
    }
    d.link_path = run.active_link.path;
    d.link = link_status(run.active, run.active_link);
    d.put_live = run.put_live;
    return d;
}

auto layout_tab_run(const SetupState& run, std::int64_t columns) -> LayoutTabRun {
    return layout_tab_run(live_copy_of(run), columns);
}

auto layout_tab_text(const SetupState& run, std::size_t at) -> std::string {
    return layout_tab_text(live_copy_of(run), at);
}

auto StageHand::on(const StageNudge&, loom::Mail& mail) -> void {
        (void)mail.as_role(kStageRole).send_to_role(kLayoutsRole, LayoutSettingsRequested{});
    }

auto StageHand::on(const LayoutSettingsAnswered&, loom::Mail&) -> void {}

auto link_live_setup(SetupState& s, std::string path) -> void {
    s.active_link = SetupLink{std::move(path), s.active};
}

auto covered_cells(const ScreenRect& r) -> CellRect {
    return cells_covered(PixelRect{r.x, r.y, r.w, r.h});
}

auto covered_cells(const ScreenRegion& r) -> CellRect {
    return cells_covered(PixelRect{r.x, r.y, r.w, r.h});
}

auto has_rect(const ScreenCanvas& c, std::int64_t x, std::int64_t y, std::int64_t w,
                     std::int64_t h, gfx::Color ink) -> bool {
    for (const ScreenRect& r : all_rects(c)) {
        if (covered_cells(r) == CellRect{x, y, w, h} && r.ink == ink) {
            return true;
        }
    }
    return false;
}

auto rasterized(const ScreenCanvas& canvas) -> std::vector<std::string> {
    return zengine::testing::screen_cells::rows_of(canvas);
}

Painter::Painter(std::vector<StandingConditions>& conditions,
            std::vector<TranscriptShown>& transcripts,
            std::vector<PaneSubjectShown>& subjects) : conditions_(&conditions), transcripts_(&transcripts), subjects_(&subjects) {}

auto Painter::on(const StandingConditions& c, loom::Mail&) -> void { conditions_->push_back(c); }

auto Painter::on(const TranscriptShown& t, loom::Mail&) -> void { transcripts_->push_back(t); }

auto Painter::on(const PaneSubjectShown& s, loom::Mail&) -> void { subjects_->push_back(s); }

RoomAsks::RoomAsks(loom::Switchboard& bus) : bus_(bus) {
        tap_ = bus_.add_observer([this](const loom::BusEvent& ev) {
            if ((ev.kind == loom::EventKind::Delivered || ev.kind == loom::EventKind::Refused) &&
                ev.payload != nullptr &&
                ev.schema_name == gfx::RoomAsked::zen_name) {
                asked_.emplace_back(loom::from_value<gfx::RoomAsked>(*ev.payload).area,
                                    std::string(ev.authored_role));
            }
        });
    }

RoomAsks::~RoomAsks() { bus_.remove_observer(tap_); }

auto RoomAsks::times(const std::string& area, const std::string& role) const -> std::size_t {
        std::size_t n = 0;
        for (const auto& [a, o] : asked_) {
            n += a == area && o == role ? std::size_t{1} : std::size_t{0};
        }
        return n;
    }

auto count_paste_asks(loom::Switchboard& bus, std::string window,
                                         std::int64_t& asks) -> loom::ObserverId {
    return bus.add_observer([&bus, window = std::move(window), &asks](const loom::BusEvent& ev) {
        if (ev.kind == loom::EventKind::Delivered &&
            ev.schema_name == gfx::ClipboardTextRequested::zen_name &&
            ev.target == bus.role_holder(window)) {
            ++asks;
        }
    });
}

auto count_desk_drawings(loom::Switchboard& bus, const loom::WeaveId& workshop,
                                            std::size_t& drawings) -> loom::ObserverId {
    return bus.add_observer([&bus, &workshop, &drawings](const loom::BusEvent& ev) {
        (void)bus;
        if (ev.kind != loom::EventKind::Delivered || ev.sender != workshop || ev.payload == nullptr ||
            ev.schema_name != gfx::Drawn::zen_name) {
            return;
        }
        if (loom::from_value<gfx::Drawn>(*ev.payload).area == kWorkshopProvider) {
            ++drawings;
        }
    });
}

auto of_kind(const loom::TerminalSession& me,
                                                  loom::TranscriptKind kind) -> std::vector<loom::TranscriptEntry> {
    std::vector<loom::TranscriptEntry> out;
    for (const loom::TranscriptEntry& e : me.transcript().entries()) {
        if (e.kind == kind) {
            out.push_back(e);
        }
    }
    return out;
}

auto condition_by_key(const std::vector<Condition>& all, const std::string& key) -> const Condition* {
    for (const Condition& c : all) {
        if (c.key == key) {
            return &c;
        }
    }
    return nullptr;
}

auto InteractionClock::read() -> std::int64_t {
        const std::int64_t at = now;
        now += step;
        return at;
    }

auto InteractionClock::together() -> void { step = 1; }

auto InteractionClock::apart() -> void { step = input::kMostDoubleClickMs + 1; }

auto InteractionClock::spaced(std::int64_t ms) -> void { step = ms; }

auto host_wake() -> loom::Message {
    loom::host::Waited w;
    w.held = true;
    w.ready = true;
    w.due = true;
    w.now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
                   .count();
    return loom::Message(loom::to_value(w), loom::WeaveId{}, loom::WeaveId{}, 0);
}

auto HubTime::OnReading::now_ms() const -> std::int64_t { return clock->now; }

auto HubTime::OnReading::moves_on_its_own() const noexcept -> bool { return false; }

auto HubTime::OnReading::arrive(std::int64_t ms) -> void {
            if (ms > clock->now) clock->now = ms;
        }

HubTime::HubTime(loom::Switchboard& b, InteractionClock& clock) : bus(b), reading(on(clock)), wait(b, reading) {
        wait.trust(kWorkshopProvider);
        settle();
    }

auto HubTime::on(InteractionClock& clock) -> OnReading {
        OnReading r;
        r.clock = &clock;
        return r;
    }

auto HubTime::settle() -> void {
        for (int turn = 0; turn < 64; ++turn) {
            bus.drain_until_idle();
            reading.clock->now += reading.clock->step;
            (void)wait.after_turn(0);
            if (bus.pending() == 0) return;
        }
    }

namespace stock {
} // namespace stock

auto stock_ref() -> PaneRef { return PaneRef{stock::kRole, stock::kPane}; }

namespace second {
} // namespace second

auto second_ref() -> PaneRef { return PaneRef{stock::kRole, second::kPane}; }

auto admit_stock(Panes& panes) -> void {
    const Admission took = admit_pane_offer(panes.runtime, stock::kRole,
                                            PaneOffered{stock::kPane, stock::kName, stock::kSummary});
    REQUIRE_MESSAGE(took.written.accepted, took.written.refusal);
    const RuntimePane* row = panes.runtime.find(stock::kRole, stock::kPane);
    REQUIRE(row != nullptr);
    REQUIRE_MESSAGE(row->kind == stock::kKind,
                    "the stand-in must be the first pane this catalog admits");
}

auto admit_second(Panes& panes) -> void {
    const Admission took = admit_pane_offer(
        panes.runtime, stock::kRole, PaneOffered{second::kPane, second::kName, second::kSummary});
    REQUIRE_MESSAGE(took.written.accepted, took.written.refusal);
    const RuntimePane* row = panes.runtime.find(stock::kRole, second::kPane);
    REQUIRE(row != nullptr);
    REQUIRE_MESSAGE(row->kind == second::kKind,
                    "the second stand-in must be the second pane this catalog admits");
}

auto Live::mount_owner() -> void {
        auto weave = std::make_unique<LayoutsOwner>(layouts_owner_host(host));
        owner = weave.get();
        owner_id = bus.register_weave(std::move(weave), layouts_grant(), std::string(kLayoutsRole));
        owner->zen_set_self(owner_id);
    }

auto Live::nudge_owner() -> void {
        if (!stage_id.valid()) {
            auto stage = std::make_unique<StageHand>();
            StageHand* raw = stage.get();
            loom::Grant grant;
            grant.allow_to_role(LayoutSettingsRequested::zen_name,
                                LayoutSettingsRequested::zen_version, kLayoutsRole);
            stage_id = bus.register_weave(std::move(stage), std::move(grant), std::string(kStageRole));
            raw->zen_set_self(stage_id);
        }
        (void)bus.send(stage_id,
                       loom::Message(loom::to_value(StageNudge{}), loom::WeaveId{}, loom::WeaveId{}, 0));
    }

Live::Live() {
        // The windows' clock, which they count the weaver's clicks by.
        windows.clock([this] { return clock.read(); });
        // THE PAINTERS' DIRECTORY, where Workshop offers the window's skins from, as the host's own.
        host.dir = WORKSHOP_SKINS_DIR;
#ifdef WORKSHOP_THEMES_DIR
        host.shipped_themes_path = WORKSHOP_THEMES_DIR; // the shipped themes, as the build has them
#endif
        // THE HOST'S VERSION ANSWER, as workshop.cpp wires it (the grant is the Emit set's) --
        // with ONE fixture statement: the stack's stand-in (`stock`) is answered as present for
        // a ROOM, so the launch door (which asks exactly that) seats it as it seats any offered
        // pane. Nothing holds its role, so everything actually sent to it is still refused on
        // the tap; the statement is about the one question and nothing else.
        host.holder_accepts = [this](std::string_view role, const loom::Schema& shape) {
            if (role == stock::kRole && shape.name() == gfx::AreaRoom::zen_name) {
                return true;
            }
            return holder_accepts_on(bus, role, shape);
        };
        host.holder_emits = [this](std::string_view role, const loom::Schema& shape) {
            return holder_emits_on(bus, role, shape);
        };
        host.role_holder = [this](std::string_view role) { return bus.role_holder(role); };
        host.incarnation_of = [this](loom::WeaveId id) {
            return static_cast<std::int64_t>(bus.participant(id).incarnation);
        };
        host.destinations = [this] {
            return bus_destinations(bus, host.terminal != nullptr ? host.terminal->id()
                                                                  : loom::WeaveId{});
        };
        mount_owner();
        auto weave = std::make_unique<WorkshopWeave>(host);
        w = weave.get();
        loom::Grant grant = loom::emit_default_grant(*w);
        loom::allow_poke_answers(grant);
        // ...AND WHERE THE PLAN IS, the realization owner's claim, as the host grants it.
        grant.allow_observe(zengine::realization::PlanStatus::zen_name,
                            zengine::realization::PlanStatus::zen_version);
        // IN THE ROLE, AS THE HOST MOUNTS IT. Workshop's weave holds `zengine.workshop`, because
        // the pane protocol is authored AS that role: mounted anonymously, its asks and room
        // grants would be refused at the bus. The three-argument `register_weave` is the Loom's
        // own way to bind one, which is what `mount_in_role` spells in the host.
        const loom::WeaveId id =
            bus.register_weave(std::move(weave), std::move(grant), std::string(kWorkshopProvider));
        w->zen_set_self(id);
        workshop_id = id;
        quit_watch = std::make_unique<QuitDeliveryWatch>(bus, id, host.undelivered_quits);
        (void)loom::mount<Painter>(bus, said_conditions, said_transcripts, said_subjects);
        (void)count_desk_drawings(bus, workshop_id, pictures);
        // THE STACK'S STAND-IN, FIRST (see `stock` above) -- through the same door the seam
        // spends, before any offer a case might make, so its handle is the constant.
        admit_stock(const_cast<Session&>(w->session()).panes);
        admit_second(const_cast<Session&>(w->session()).panes);
    }

auto Live::mount_terminal(bool widen, int shapes) -> loom::TerminalSession* {
        loom::TerminalVocabulary vocab;
        vocab.knows(loom::schema_of<RigNote>())
            .accepts(loom::schema_of<loom::Ack>())
            .accepts(loom::schema_of<loom::Refused>());
        for (int i = 0; i < shapes; ++i) {
            vocab.knows(loom::SchemaBuilder("Extra" + std::to_string(i), 1)
                            .field("seq", loom::Kind::Int)
                            .build());
        }
        loom::Grant grant;
        grant.allow_to_role(RigNote::zen_name, RigNote::zen_version, kRigBoard);
        if (widen) {
            grant.allow_to_any(RigNote::zen_name, RigNote::zen_version);
        }
        const loom::MountedTerminal mounted = loom::host_mount_terminal(
            bus, std::make_unique<loom::TerminalSession>("workshop", std::move(vocab)),
            std::move(grant));
        host.terminal = mounted.session;
        terminal_id = mounted.id;
        return mounted.session;
    }

auto Live::ready() -> void {
        if (own_medium()) {
            return; // the case's own window starts the desk by its offer
        }
        come_up();
    }

auto Live::extent(std::int64_t width, std::int64_t height, std::int64_t adv,
                std::int64_t line, std::int64_t cell) -> void {
        extent_px(cells_px(width), cells_px(height), adv, line, cell);
    }

auto Live::extent_px(std::int64_t width, std::int64_t height, std::int64_t adv,
                   std::int64_t line, std::int64_t cell) -> void {
        REQUIRE_MESSAGE(!own_medium(), "a case that seated a window of its own sizes it there");
        (void)cell;
        const auto [a, l] = face_for(adv, line);
        windows.room(width, height, a, l);
    }

auto Live::extent_on_window(std::int64_t width, std::int64_t height) -> void {
        extent(width, height, 8, 18, gfx::kCellPx);
    }

auto Live::shown() -> DeskShown {
        come_up();
        REQUIRE_MESSAGE(windows.up(), "no window shows this desk");
        return windows.shown();
    }

auto Live::window_button(std::int64_t cx, std::int64_t cy, bool pressed, std::int64_t button) -> void {
        come_up();
        REQUIRE_MESSAGE(windows.up(), "no window shows this desk");
        windows.hand({input::PointerButton{button, pressed, cx, cy, input::space::kCells,
                                           input::mod::kNone}});
        bus.drain_until_idle();
    }

auto Live::window_click(std::int64_t cx, std::int64_t cy, std::int64_t button) -> void {
        window_button(cx, cy, true, button);
        window_button(cx, cy, false, button);
    }

auto Live::hand_button(std::int64_t x, std::int64_t y, std::int64_t space, bool pressed,
                     std::int64_t button) -> void {
        come_up();
        REQUIRE_MESSAGE(windows.up(), "no window shows this desk");
        windows.hand({input::PointerButton{button, pressed, x, y, space, input::mod::kNone}});
        bus.drain_until_idle();
    }

auto Live::window_queue_button(std::int64_t cx, std::int64_t cy, bool pressed,
                             std::int64_t button) -> void {
        come_up();
        windows.queue({input::PointerButton{button, pressed, cx, cy, input::space::kCells,
                                            input::mod::kNone}});
    }

auto Live::window_queue_key(std::int64_t scan, std::int64_t mods) -> void {
        come_up();
        windows.queue({input::KeyPressed{scan, "", mods}});
    }

auto Live::window_beat() -> void {
        windows.pump();
        bus.drain_until_idle();
    }

auto Live::on_gui_window() -> void {
        gui_window = true;
        if (windows.up()) {
            const Session& s = w->session();
            windows.room(s.screen_w, s.screen_h, 8, 18);
        }
    }

auto Live::face_for(std::int64_t adv, std::int64_t line) const -> std::pair<std::int64_t, std::int64_t> {
        if (gui_window && (adv <= 0 || line <= 0)) {
            return {8, 18};
        }
        return {adv, line};
    }

auto Live::own_medium() -> bool {
        if (in_window && !windows.up() &&
            (bus.role_holder(gfx::kGraphicsRole).valid() ||
             bus.role_holder(std::string(gfx::kWindowRolePrefix) + gfx::kTuiWindow).valid() ||
             bus.role_holder(std::string(gfx::kWindowRolePrefix) + gfx::kGuiWindow).valid())) {
            in_window = false;
        }
        return !in_window;
    }

auto Live::come_up() -> void {
        if (own_medium() || windows.up() || w == nullptr) {
            return;
        }
        const Session& s = w->session();
        const auto [a, l] = face_for(s.text_advance_px, s.text_line_px);
        windows.room(s.screen_w, s.screen_h, a, l);
    }

auto Live::type_line(const std::string& line) -> void {
        for (const char c : line) {
            text(std::string(1, c));
        }
        key(input::scan::kReturn);
    }

auto Live::publish(const loom::Value& v) -> void {
        come_up();
        (void)bus.publish(loom::Message(v, loom::WeaveId{}, loom::WeaveId{}, 0));
        bus.drain_until_idle();
    }

auto Live::key(std::int64_t sc, std::int64_t mods) -> void {
        publish(loom::to_value(input::KeyPressed{sc, "", mods}));
    }

auto Live::text(const std::string& s) -> void { publish(loom::to_value(input::TextEntered{s})); }

auto Live::term_x(std::int64_t wx) const -> std::int64_t { return wx; }

auto Live::term_y(std::int64_t wy) -> std::int64_t {
        return wy + gfx::cell_of_pixel(screen_of(w->session()).room_y);
    }

auto Live::px_x(std::int64_t wx) const -> std::int64_t { return wx * gfx::kCellPx; }

auto Live::px_y(std::int64_t wy) const -> std::int64_t {
        return screen_of(w->session()).room_y + wy * gfx::kCellPx;
    }

auto Live::press_at(std::int64_t x, std::int64_t y, std::int64_t space,
                  std::int64_t mods) -> void {
        hand_moment(input::PointerButton{1, true, x, y, space, mods});
    }

auto Live::motion_at(std::int64_t x, std::int64_t y, std::int64_t space) -> void {
        hand_moment(input::PointerMoved{x, y, 0, 0, space, input::mod::kNone});
    }

auto Live::press(std::int64_t wx, std::int64_t wy, std::int64_t mods) -> void {
        hand_moment(input::PointerButton{1, true, term_x(wx), term_y(wy),
                                                    input::space::kCells, mods});
    }

auto Live::release(std::int64_t wx, std::int64_t wy) -> void {
        hand_moment(input::PointerButton{1, false, term_x(wx), term_y(wy),
                                                    input::space::kCells, input::mod::kNone});
    }

auto Live::right_press(std::int64_t wx, std::int64_t wy) -> void {
        hand_moment(input::PointerButton{3, true, term_x(wx), term_y(wy),
                                                    input::space::kCells, input::mod::kNone});
    }

auto Live::press_canvas(std::int64_t cx, std::int64_t cy) -> void {
        hand_moment(input::PointerButton{1, true, cx, cy,
                                                    input::space::kCells, input::mod::kNone});
    }

auto Live::release_canvas(std::int64_t cx, std::int64_t cy) -> void {
        hand_moment(input::PointerButton{1, false, cx, cy,
                                                    input::space::kCells, input::mod::kNone});
    }

auto Live::motion_canvas(std::int64_t cx, std::int64_t cy) -> void {
        hand_moment(input::PointerMoved{cx, cy, 0, 0,
                                                   input::space::kCells, input::mod::kNone});
    }

auto Live::right_press_canvas(std::int64_t cx, std::int64_t cy) -> void {
        hand_moment(input::PointerButton{3, true, cx, cy,
                                                    input::space::kCells, input::mod::kNone});
    }

auto Live::menu() const -> const ContextMenu& { return w->session().context; }

auto Live::motion(std::int64_t wx, std::int64_t wy) -> void {
        hand_moment(input::PointerMoved{term_x(wx), term_y(wy), 0, 0,
                                                   input::space::kCells, input::mod::kNone});
    }

auto Live::wheel_canvas(double dy, std::int64_t cx, std::int64_t cy) -> void {
        hand_moment(input::PointerWheel{0.0, dy, cx, cy,
                                                   input::space::kCells, input::mod::kNone});
    }

auto Live::mid(std::int64_t p) -> std::int64_t { return p + gfx::kCellPx / 2; }

auto Live::press_px(std::int64_t wx, std::int64_t wy) -> void {
        hand_moment(input::PointerButton{1, true, mid(px_x(wx)), mid(px_y(wy)),
                                                    input::space::kPixels, input::mod::kNone});
    }

auto Live::release_px(std::int64_t wx, std::int64_t wy) -> void {
        hand_moment(input::PointerButton{1, false, mid(px_x(wx)), mid(px_y(wy)),
                                                    input::space::kPixels, input::mod::kNone});
    }

auto Live::motion_px(std::int64_t wx, std::int64_t wy) -> void {
        hand_moment(input::PointerMoved{mid(px_x(wx)), mid(px_y(wy)), 0, 0,
                                                   input::space::kPixels, input::mod::kNone});
    }

auto Live::window_settles() -> void {
        for (int beat = 0; beat < 4; ++beat) {
            const std::int64_t was_w = session().screen_w, was_h = session().screen_h;
            windows.pump();
            windows.pump();
            if (session().screen_w == was_w && session().screen_h == was_h) {
                return;
            }
        }
    }

auto Live::close_requested() -> void {
        come_up();
        REQUIRE_MESSAGE(windows.up(), "no window shows this desk");
        (void)bus.role_publish_as(windows.id(), windows.role(),
                                    loom::Message(loom::to_value(gfx::SurfaceCloseRequested{})));
        bus.drain_until_idle();
    }

auto Live::session() const -> const Session& { return w->session(); }

auto Live::run() const -> const SetupState& { return owner->layouts().run(); }

auto Live::notice() const -> std::string { return w->session().notice; }

auto Live::glance() const -> std::string {
        return said_conditions.empty() ? std::string()
                                       : attention_glance(said_conditions.back().rows);
    }

auto Live::conditions() const -> std::vector<Condition> {
        return attention_conditions(w->session(), frontier_claimed(bus));
    }

auto this_process_id() -> std::string {
#if defined(_WIN32)
    return std::to_string(::GetCurrentProcessId());
#else
    return std::to_string(static_cast<long long>(::getpid()));
#endif
}

auto workshop_temp_root() -> std::filesystem::path {
    static const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("zengine-workshop-" + std::string(zengine::testing::workshop_suite()) + "-" + this_process_id());
    return root;
}

auto sweep_leaves_the_tree(const std::filesystem::directory_entry& entry) -> bool {
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

auto remove_tree(const std::filesystem::path& p) -> void {
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

auto link_arm_name(LinkArm arm) -> const char* {
    return arm == LinkArm::junction        ? "junction"
           : arm == LinkArm::symbolic_link ? "symbolic link"
                                           : "nothing";
}

auto make_linked_directory(const std::filesystem::path& link,
                                     const std::filesystem::path& target) -> LinkArm {
#if defined(_WIN32)
    const std::string command = "cmd /c mklink /J \"" + link.string() + "\" \"" +
                                target.string() + "\" >nul 2>&1";
    std::error_code exists_ec;
    if (std::system(command.c_str()) == 0 && std::filesystem::exists(link, exists_ec) &&
        !exists_ec) {
        return LinkArm::junction;
    }
    return LinkArm::none;
#else
    std::error_code ec;
    std::filesystem::create_directory_symlink(target, link, ec);
    return ec ? LinkArm::none : LinkArm::symbolic_link;
#endif
}

TempDir::TempDir(const char* tag) {
        static int counter = 0;
        path_ = workshop_temp_root() / (std::string(tag) + "-" + std::to_string(++counter));
        // Whatever is already there is an earlier life of this process id's -- a run that
        // crashed, whose id the system handed out again -- and it goes the way of everything
        // this fixture removes: by the sweep that never enters a link.
        remove_tree(path_);
        std::filesystem::create_directories(path_);
    }

TempDir::~TempDir() {
        remove_tree(path_);
        // ...and this process's root once the last case in it has gone. `remove` takes an
        // EMPTY directory only, so this is the whole cleanup: it succeeds for whoever
        // happens to be last and does nothing, quietly, for everyone before them.
        std::error_code ec;
        std::filesystem::remove(workshop_temp_root(), ec);
    }

auto TempDir::file(const char* name) const -> std::string { return (path_ / name).string(); }

auto TempDir::document() const -> std::string { return file("document.json"); }

auto TempDir::path() const -> const std::filesystem::path& { return path_; }

auto slurp(const std::string& path) -> std::string {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

auto spillout(const std::string& path, const std::string& text) -> void {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
}

auto ToolSeat::on(const zengine::builder::StatusRequested&, loom::Mail& mail) -> void {
        ++described;
        (void)mail.publish(catalog);
        (void)mail.publish(next);
    }

auto ToolSeat::on(const zengine::builder::BuildRequested& ask, loom::Mail& mail) -> void {
        asked.push_back(ask.recipe);
        realize_asked.push_back(ask.realize);
        if (answers_builds) {
            (void)mail.publish(next);
        }
    }

auto mount_tool(Live& t, const std::string& recipe) -> ToolSeat* {
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

auto rect_text(const DeskShown& s, const CellRect& b) -> std::string {
    std::string out;
    const std::int64_t top = gfx::px_of_cells(b.y);
    const std::int64_t bottom = gfx::px_of_cells(b.y + b.h);
    const std::int64_t columns = s.advance > 0 ? gfx::px_of_cells(b.w) / s.advance : 0;
    for (std::int64_t y = top; s.line > 0 && y < bottom; y += s.line) {
        const std::string row = s.text_at_px(gfx::px_of_cells(b.x), y, columns);
        if (!row.empty()) {
            out += row;
            out += '\n';
        }
    }
    return out;
}

auto pane_body_cells(const PixelRect& outer) -> CellRect {
    return cells_covered(pane_interior(outer, kChromePx));
}

auto pane_body_cells(const CellRect& outer) -> CellRect {
    return pane_body_cells(pixels_of_cells(outer));
}

auto pane_body_cells(const PixelRect& outer, const Screen& sc) -> CellRect {
    return cells_covered(pane_interior(outer, sc));
}

auto stack_text(const DeskShown& s) -> std::string {
    return rect_text(s, pane_body_cells(placement_bounds(placement::kOverlayStack, 0,
                                                          kMinScreen)));
}

auto catalog_at(const Panes& panes, std::int64_t kind) -> std::size_t {
    const std::vector<CatalogRow> rows = combined_catalog(panes);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].kind == kind) {
            return i;
        }
    }
    return rows.size(); // walked off the end: the case that used it will fail loudly
}

auto release_keys(Live& t) -> void {
    const_cast<Session&>(t.session()).panes.keyboard = kNoPaneKind;
}

auto first_slot_shows_stock(Live& t) -> bool {
    const Screen sc = screen_of(t.session());
    const PaneBounds at = bounds_of(t.session().panes, t.session().setup.active, stock::kKind, sc);
    const CellRect cells = pane_body_cells(at.rect);
    return at.open && at.rect == placement_bounds(placement::kOverlayStack, 0, sc) &&
           label_at(t.shown(), cells.x, cells.y).find(stock::kName) != std::string::npos;
}

auto pane_shown(const DeskShown& shown, const Session& s, std::int64_t kind) -> std::string {
    const PaneBounds at = bounds_of(s.panes, s.setup.active, kind, screen_of(s));
    if (!at.open) {
        return {}; // a closed pane says nothing, which is the answer a case wants
    }
    return rect_text(shown, pane_body_cells(at.rect, screen_of(s)));
}

auto ref_of(std::int64_t kind) -> PaneRef {
    if (kind == stock::kKind) {
        return stock_ref(); // the stand-in has no catalog row to derive one from
    }
    if (kind == second::kKind) {
        return second_ref(); // ...and neither has the second
    }
    return pane_ref_of(kind);
}

auto stranger() -> PaneRef { return PaneRef{"third.party.tools", "history"}; }

auto no_providers() -> const Panes& {
    static const Panes empty;
    return empty;
}

auto setup_of(const std::string& name, const std::vector<std::int64_t>& kinds) -> Setup {
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

auto open_kinds(const Panes& panes) -> std::vector<std::int64_t> {
    std::vector<std::int64_t> out;
    for (const OpenPane& p : panes.open) {
        out.push_back(p.kind);
    }
    return out;
}

auto old_place_word(const setup_persist::WorkshopPanePlace& p) -> std::string {
    const bool corner = p.mode == "top-right" && p.x.value_or(0) == 0 && p.y.value_or(0) == 0;
    return corner ? std::string(setup_persist::kUnitRightColumn) : p.mode;
}

auto v3_desk(const Setup& s) -> setup_persist::v3::WorkshopSetup {
    const setup_persist::WorkshopSetup now = setup_persist::to_setup(s);
    setup_persist::v3::WorkshopSetup out;
    out.format = now.format;
    out.format_version = setup_persist::v3::kRetainedVersion;
    out.name = now.name;
    const auto word = [](const std::string& mode) {
        return mode == setup_persist::kUnitPixels ? std::string(setup_persist::v3::kUnitSubcells)
                                                  : mode;
    };
    const auto subs = [](const std::string& mode, std::int64_t v) {
        return mode == setup_persist::kUnitPixels ? v * setup_persist::v3::kSubsPerPixel : v;
    };
    for (const setup_persist::WorkshopSetupPane& p : now.panes) {
        setup_persist::v3::WorkshopSetupPane row;
        row.provider = p.provider;
        row.pane = p.pane;
        row.front = p.front;
        const std::int64_t canvas_y = p.place.mode == setup_persist::kUnitPixels
                                          ? p.place.y.value_or(0) + setup_persist::kCanvasRoomTopPx
                                          : p.place.y.value_or(0);
        row.place = setup_persist::v3::WorkshopPanePlace{word(old_place_word(p.place)),
                                                         subs(p.place.mode, p.place.x.value_or(0)),
                                                         subs(p.place.mode, canvas_y)};
        row.width = setup_persist::v3::WorkshopPaneSize{
            word(p.width.mode), subs(p.width.mode, p.width.amount.value_or(0))};
        row.height = setup_persist::v3::WorkshopPaneSize{
            word(p.height.mode), subs(p.height.mode, p.height.amount.value_or(0))};
        out.panes.push_back(std::move(row));
    }
    return out;
}

auto v3_setup_in(const setup_persist::v3::WorkshopSetup& old, Setup& out) -> Written {
    setup_persist::v4::WorkshopSetup four;
    Written landed = setup_persist::v3::to_v4(old, four);
    if (!landed.accepted) {
        return landed;
    }
    setup_persist::WorkshopSetup now;
    landed = setup_persist::v4::to_current(four, now);
    return landed.accepted ? setup_persist::setup_in(now, out) : landed;
}

auto v4_desk(const Setup& s) -> setup_persist::v4::WorkshopSetup {
    const setup_persist::WorkshopSetup now = setup_persist::to_setup(s);
    setup_persist::v4::WorkshopSetup out;
    out.format = now.format;
    out.format_version = setup_persist::v4::kRetainedVersion;
    out.name = now.name;
    for (const setup_persist::WorkshopSetupPane& p : now.panes) {
        out.panes.push_back(setup_persist::v4::WorkshopSetupPane{
            p.provider, p.pane,
            setup_persist::v5::WorkshopPanePlace{old_place_word(p.place), p.place.x.value_or(0),
                                                 p.place.y.value_or(0)},
            setup_persist::v5::WorkshopPaneSize{p.width.mode, p.width.amount.value_or(0)},
            setup_persist::v5::WorkshopPaneSize{p.height.mode, p.height.amount.value_or(0)},
            p.front});
    }
    return out;
}

auto v5_desk(const Setup& s) -> setup_persist::v5::WorkshopSetup {
    const setup_persist::WorkshopSetup now = setup_persist::to_setup(s);
    setup_persist::v5::WorkshopSetup out;
    out.format = now.format;
    out.format_version = setup_persist::v5::kRetainedVersion;
    out.name = now.name;
    for (const setup_persist::WorkshopSetupPane& p : now.panes) {
        out.panes.push_back(setup_persist::v5::WorkshopSetupPane{
            p.provider, p.pane,
            setup_persist::v5::WorkshopPanePlace{old_place_word(p.place), p.place.x.value_or(0),
                                                 p.place.y.value_or(0)},
            setup_persist::v5::WorkshopPaneSize{p.width.mode, p.width.amount.value_or(0)},
            setup_persist::v5::WorkshopPaneSize{p.height.mode, p.height.amount.value_or(0)},
            p.front, p.settings});
    }
    return out;
}

auto v11_run(const std::vector<Layout>& run) -> std::vector<session_history::v11::WorkshopLayout> {
    std::vector<session_history::v11::WorkshopLayout> out;
    for (const Layout& l : run) {
        session_history::v11::WorkshopLayout layout;
        layout.desk = v5_desk(l.desk);
        layout.link.path = l.link.path;
        layout.link.known = v5_desk(l.link.known);
        out.push_back(std::move(layout));
    }
    return out;
}

auto v11_lines(const session_history::v11::WorkshopSession& s) -> std::string {
    const session_history::v11::WorkshopSessionHeader head{
        s.format,    s.format_version, s.viewport, static_cast<std::int64_t>(s.layouts.size()),
        s.active,    s.placement,      s.choices,  s.window_room};
    std::string out = loom::compat::serialize(loom::to_value(head)) + '\n';
    for (const session_history::v11::WorkshopLayout& l : s.layouts) {
        out += loom::compat::serialize(loom::to_value(l)) + '\n';
    }
    return out;
}

auto v11_session(const std::vector<Layout>& run, std::size_t active, std::int64_t w, std::int64_t h,
            const window_persist::Placement& place,
            const std::vector<session_persist::WorkshopChoice>& choices) -> session_history::v11::WorkshopSession {
    const window_persist::WorkshopWindow window = window_persist::to_window(w, h, place);
    session_history::v11::WorkshopSession out;
    out.format = session_persist::kFormat;
    out.format_version = session_history::kV11FormatVersion;
    out.viewport = window.viewport;
    out.layouts = v11_run(run);
    out.active = static_cast<std::int64_t>(active);
    out.placement = window.placement;
    out.choices = choices;
    out.window_room = window.window_room;
    return out;
}

auto v6_desk(const Setup& s) -> setup_persist::v6::WorkshopSetup {
    const setup_persist::WorkshopSetup now = setup_persist::to_setup(s);
    setup_persist::v6::WorkshopSetup out;
    out.format = now.format;
    out.format_version = setup_persist::v6::kRetainedVersion;
    out.name = now.name;
    for (const setup_persist::WorkshopSetupPane& p : now.panes) {
        out.panes.push_back(setup_persist::v6::WorkshopSetupPane{
            p.provider, p.pane, p.place, p.width, p.height, p.front, p.settings});
    }
    out.theme = LayoutTheme{now.themes.empty() ? std::string() : now.themes.front(), 0};
    out.locks = now.locks;
    out.pins = now.pins;
    return out;
}

auto v12_session(const std::vector<Layout>& run, std::size_t active,
            const std::vector<session_persist::WorkshopChoice>& choices) -> session_history::v12::WorkshopSession {
    session_history::v12::WorkshopSession out;
    out.format = session_persist::kFormat;
    out.format_version = session_history::kV12FormatVersion;
    std::int64_t top = 0;
    for (const Layout& l : run) {
        session_history::v12::WorkshopLayout layout;
        layout.id = l.id;
        layout.desk = v6_desk(l.desk);
        layout.link.path = l.link.path;
        layout.link.known = v6_desk(l.link.known);
        out.layouts.push_back(std::move(layout));
        top = l.id > top ? l.id : top;
    }
    out.active = static_cast<std::int64_t>(active);
    out.next_id = top + 1;
    out.choices = choices;
    return out;
}

auto v7_desk(const Setup& s) -> setup_persist::v7::WorkshopSetup {
    const setup_persist::WorkshopSetup now = setup_persist::to_setup(s);
    setup_persist::v7::WorkshopSetup out;
    out.format = now.format;
    out.format_version = setup_persist::v7::kRetainedVersion;
    out.name = now.name;
    for (const setup_persist::WorkshopSetupPane& p : now.panes) {
        out.panes.push_back(setup_persist::v7::WorkshopSetupPane{
            p.provider, p.pane, p.place, p.width, p.height, p.front, p.settings, p.fold, p.dock});
    }
    out.theme = LayoutTheme{now.themes.empty() ? std::string() : now.themes.front(), 0};
    out.locks = now.locks;
    out.pins = now.pins;
    return out;
}

auto v13_session(const std::vector<Layout>& run,
                                                         std::size_t active) -> session_history::v13::WorkshopSession {
    session_history::v13::WorkshopSession out;
    out.format = session_persist::kFormat;
    out.format_version = session_history::kV13FormatVersion;
    std::int64_t top = 0;
    for (const Layout& l : run) {
        session_history::v13::WorkshopLayout layout;
        layout.id = l.id;
        layout.desk = v7_desk(l.desk);
        layout.link.path = l.link.path;
        layout.link.known = v7_desk(l.link.known);
        out.layouts.push_back(std::move(layout));
        top = std::max(top, l.id);
    }
    out.active = static_cast<std::int64_t>(active);
    out.next_id = top + 1;
    return out;
}

auto v13_lines(const session_history::v13::WorkshopSession& s) -> std::string {
    const session_history::v13::WorkshopSessionHeader head{
        s.format, s.format_version, static_cast<std::int64_t>(s.layouts.size()), s.active,
        s.next_id, s.choices};
    std::string out = loom::compat::serialize(loom::to_value(head)) + '\n';
    for (const session_history::v13::WorkshopLayout& l : s.layouts) {
        out += loom::compat::serialize(loom::to_value(l)) + '\n';
    }
    return out;
}

auto v12_lines(const session_history::v12::WorkshopSession& s) -> std::string {
    const session_history::v12::WorkshopSessionHeader head{
        s.format, s.format_version, static_cast<std::int64_t>(s.layouts.size()), s.active,
        s.next_id, s.choices};
    std::string out = loom::compat::serialize(loom::to_value(head)) + '\n';
    for (const session_history::v12::WorkshopLayout& l : s.layouts) {
        out += loom::compat::serialize(loom::to_value(l)) + '\n';
    }
    return out;
}

auto fill_to_every_bound(Setup& s) -> void {
    while (s.panes.size() < kMaxSetupPanes) {
        const std::string tail = std::to_string(s.panes.size());
        const std::string key = std::string(kMaxPaneKeyLen - tail.size(), '"') + tail;
        REQUIRE(add_pane(s, PaneRef{key, key}));
    }
    std::size_t left = kMaxPaneSettingsPerDesk;
    for (SetupPane& row : s.panes) {
        row.place = PanePlace{pane_unit::kPixels, kMaxPanePixels, kMaxPanePixels};
        row.width = PaneSize{pane_unit::kPixels, kMaxPanePixels};
        row.height = PaneSize{pane_unit::kPixels, kMaxPanePixels};
        row.settings.clear();
        for (std::size_t i = 0; i < kMaxPaneSettingsPerRow && left > 0; ++i, --left) {
            const std::string n = std::to_string(100 + i);
            PaneSetting one;
            one.key = std::string(kMaxPaneSettingKeyLen - n.size(), 'k') + n;
            one.text = std::string(kMaxPaneSettingTextLen, '"');
            row.settings.push_back(std::move(one));
        }
    }
    REQUIRE(desk_setting_count(s.panes) == kMaxPaneSettingsPerDesk);
    REQUIRE_MESSAGE(check_setup(s).accepted, check_setup(s).refusal);
}

auto forged_setup(const Setup& s, const std::string& from, const std::string& to) -> std::string {
    std::string text = setup_persist::to_text(s);
    const std::size_t at = text.find(from);
    INFO("looking for `", from, "` in: ", text);
    REQUIRE(at != std::string::npos);
    text.replace(at, from.size(), to);
    return text;
}

auto layout_keys(const Live& t) -> LayoutKeys {
    return LayoutKeys{t.session().keymap.gesture_of(Act::kLayoutNext),
                      t.session().keymap.gesture_of(Act::kLayoutPrevious),
                      t.session().keymap.gesture_of(Act::kLayoutNew),
                      t.session().keymap.gesture_of(Act::kLayoutRemove)};
}

auto press_gesture(Live& t, const Gesture& g) -> void { t.key(g.scancode, g.modifiers); }

auto tab_column(Live& t, std::size_t at) -> std::int64_t {
    const BandStatus band = band_status(t.session(), screen_of(t.session()));
    for (const LayoutTab& tab : band.tabs) {
        if (tab.at == at) {
            return tab.column;
        }
    }
    return -1;
}

auto press_tab(Live& t, std::size_t at) -> void {
    const std::int64_t column = tab_column(t, at);
    REQUIRE(column >= 0);
    t.press_canvas(column, 0);
    t.release_canvas(column, 0);
}

auto right_press_tab(Live& t, std::size_t at) -> void {
    const std::int64_t column = tab_column(t, at);
    REQUIRE(column >= 0);
    t.right_press_canvas(column, 0);
}

auto choose_context_action(Live& t, const char* id) -> bool {
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

auto open_rename_on_tab(Live& t, std::size_t at) -> void {
    // THE PACE IS SET BEFORE THE FIRST PRESS, because `InteractionClock::read` hands out
    // the current instant and THEN advances by the step -- so a `together()` between the
    // two presses would set the pace for the gesture after this one.
    t.clock.together();
    press_tab(t, at);
    press_tab(t, at);
    t.clock.apart();
}

auto type_name(Live& t, const std::string& name) -> void {
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

auto rename_live_layout(Live& t, const std::string& name) -> void {
    open_rename_on_tab(t, t.session().setup.active_at);
    type_name(t, name);
}

auto save_setup(Live& t) -> void { t.key(input::scan::kS); }

auto name_setup(Live& t, const std::string& name) -> void {
    rename_live_layout(t, name);
    save_setup(t);
}

auto setup_row(const DeskShown& c, const Screen& sc) -> std::string {
    (void)sc;
    return label_at(c, 0, 0);
}

auto workspace_row(const DeskShown& c, const Session& s, const Screen& sc) -> std::string {
    return layouts_body(s, sc).rows >= 2 ? inspector_row(c, 0, 1) : inspector_row(c, 0, 0);
}

auto layouts_region_on(const ScreenCanvas& c,
                                             const Session& s,
                                             const Screen& sc) -> const ScreenRegion* {
    const ExternalBodyPlace place = layouts_body(s, sc);
    for (const ScreenPlane& layer : c.layers) {
        for (const ScreenRegion& r : layer.texts) {
            if (r.x == place.region_x && r.y == place.region_y && r.h == place.region_h) {
                return &r;
            }
        }
    }
    return nullptr;
}

auto first_frame(Live& t) -> DeskShown {
    t.ready();
    return t.shown();
}

auto hello_ref() -> PaneRef { return PaneRef{kHelloRole, kHelloPane}; }

DoorAsker::DoorAsker(std::string role) : role_(std::move(role)) {}

auto DoorAsker::on(const SeatDo&, loom::Mail& mail) -> void {
        if (next) {
            auto what = next;
            next = nullptr;
            what(*this, mail);
        }
    }

auto DoorAsker::on(const ProjectRoot& said, loom::Mail& mail) -> void {
        ++state_.answers;
        roots.push_back(said);
        if (then_root) {
            auto what = then_root;
            then_root = nullptr;
            what(*this, mail);
        }
    }

auto DoorAsker::on(const ProjectFrontierSaid& said, loom::Mail&) -> void {
        ++state_.answers;
        frontiers.push_back(said);
    }

auto DoorAsker::on(const PlanNames& said, loom::Mail&) -> void {
        ++state_.answers;
        names.push_back(said);
    }

auto DoorAsker::on(const RecipeOutcome& said, loom::Mail&) -> void {
        ++state_.answers;
        outcomes.push_back(said);
    }

auto DoorAsker::on(const PlanRowWritten& said, loom::Mail&) -> void {
        ++state_.answers;
        rows.push_back(said);
    }

auto DoorAsker::on(const SourceOpened& said, loom::Mail& mail) -> void {
        ++state_.answers;
        opens.push_back(said);
        // WHETHER LOOM SAYS THIS ANSWERS AN ASK OF THIS WEAVE'S -- the provenance a relayed
        // answer must still carry for the ORIGINAL requester (WL-OPEN-07): the Editor spends the requester's own kept right, so this is true.
        opens_authentic.push_back(mail.answers_ask());
    }

auto DoorAsker::on(const RecipeSourceSaid& said, loom::Mail&) -> void {
        ++state_.answers;
        sources.push_back(said);
    }

auto mount_door_asker(Live& t, const char* role) -> DoorAsker* {
    auto asker = std::make_unique<DoorAsker>(std::string(role));
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
        t.bus.register_weave(std::move(asker), std::move(grant), std::string(role));
    raw->zen_set_self(id);
    raw->id = id;
    return raw;
}

auto asker_do(Live& t, DoorAsker* asker,
                     std::function<void(DoorAsker&, loom::Mail&)> what) -> void {
    asker->next = std::move(what);
    (void)t.bus.send(asker->id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                             loom::WeaveId{}, 0));
    t.bus.drain_until_idle();
}

auto open_through_door(Live& t, DoorAsker* asker, const std::string& path) -> SourceOpened {
    const std::size_t before = asker->opens.size();
    asker_do(t, asker, [path](DoorAsker& a, loom::Mail& mail) {
        a.ask(mail, kEditorRole, OpenSourceRequested{path});
    });
    REQUIRE(asker->opens.size() == before + 1);
    return asker->opens.back();
}

auto edit_source_through_door(Live& t, DoorAsker* asker,
                                                 const std::string& recipe) -> RecipeSourceSaid {
    const std::size_t before = asker->sources.size();
    asker_do(t, asker, [recipe](DoorAsker& a, loom::Mail& mail) {
        a.ask(mail, kProjectRole, RecipeSourceRequested{recipe});
    });
    REQUIRE(asker->sources.size() == before + 1);
    return asker->sources.back();
}

auto DoorHand::on(const PaneLaunchAnswered& a, loom::Mail&) -> void { launched.push_back(a); }

auto DoorHand::on(const PaneCloseAnswered& a, loom::Mail&) -> void { closed.push_back(a); }

auto DoorHand::on(const PaneToggleAnswered& a, loom::Mail&) -> void { toggled.push_back(a); }

auto DoorHand::on(const PaneSubjectActed& a, loom::Mail&) -> void { acted.push_back(a); }

auto DoorHand::on(const SeatDo&, loom::Mail& mail) -> void {
        if (next) {
            std::function<void(DoorHand&, loom::Mail&)> once;
            once.swap(next);
            once(*this, mail);
        }
    }

auto subject_row_index(const Session& s, const std::string& label,
                                     const std::string& after) -> std::size_t {
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

auto subject_row(const Session& s, const std::string& label,
                              const std::string& after) -> const Row* {
    const std::size_t at = subject_row_index(s, label, after);
    return at < s.inspected.rows.size() ? &s.inspected.rows[at] : nullptr;
}

auto subject_value(const Session& s, const std::string& label,
                                 const std::string& after) -> std::string {
    const Row* row = subject_row(s, label, after);
    REQUIRE_MESSAGE(row != nullptr, "the inspected pane has no row labelled ", label);
    return row->value();
}

auto pick(Live& t, std::int64_t kind) -> void {
    const std::vector<CatalogRow> rows = combined_catalog(t.session().panes);
    const std::size_t at = catalog_at(t.session().panes, kind);
    REQUIRE(at < rows.size());
    toggle_pane(t, rows[at].ref);
}

auto open_stock_pane(Live& t) -> void { pick(t, stock::kKind); }

SeatRows::SeatRows(std::string p, std::vector<TextRow> r, std::vector<RowPart> named) : pane(std::move(p)), rows(std::move(r)), parts(std::move(named)) {}

ProviderSeat::ProviderSeat(std::string role) : role_(std::move(role)) {}

auto ProviderSeat::on(const PaneActionRequested& a, loom::Mail& mail) -> void {
        ++state_.said;
        ++said;
        actions.push_back(a);
        action_authors.push_back(std::string(mail.authored_role()));
    }

auto ProviderSeat::on(const PaneCatalogRequested&, loom::Mail& mail) -> void {
        ++state_.said;
        ++said;
        asks.push_back(std::string(mail.authored_role()));
    }

auto ProviderSeat::on(const gfx::AreaRoom& r, loom::Mail& mail) -> void {
        ++state_.said;
        ++said;
        room_authors.push_back(std::string(mail.authored_role()));
        const std::string pane = pane_of(r.area);
        WindowDrawing& drawing = drawing_of(r.area);
        if (!drawing.room(r, mail)) {
            return;
        }
        const RowsRoom room = drawing.rows_room();
        rooms.push_back(SeatRoom{pane, r.area, room.rows(), room.columns()});
        draw(mail, pane);
    }

auto ProviderSeat::on(const gfx::DrawnRefused& refused, loom::Mail& mail) -> void {
        refusals.push_back(refused.reason);
        (void)drawing_of(refused.area).refused(refused, mail);
    }

auto ProviderSeat::on(const gfx::AreaButtonChanged& b, loom::Mail& mail) -> void {
        ++state_.said;
        ++said;
        const std::string pane = pane_of(b.area);
        WindowDrawing& drawing = drawing_of(b.area);
        const std::optional<WindowPointer> p =
            drawing.pointer(b, mail, drawing.rows_room().rows(), columns_of(b.area));
        press_authors.push_back(std::string(mail.authored_role()));
        if (!p || p->phase != canvas_pointer::kPress) {
            return;
        }
        const SeatPress press{pane, p->button, p->at.row, p->at.column, p->at.shown, keys_here_};
        any_presses.push_back(press);
        if (p->button == 1 && p->at.shown) presses.push_back(press);
        if (p->button == 3 && pass_right) {
            (void)pane_menu::pass_back(mail, role_.c_str(), pane);
        }
    }

auto ProviderSeat::on(const gfx::AreaWheelTurned& w, loom::Mail& mail) -> void {
        ++state_.said;
        ++said;
        if (drawing_of(w.area).pointer(w, mail, 0, 0)) {
            wheels.push_back(SeatWheel{pane_of(w.area), w.dy});
            wheel_authors.push_back(std::string(mail.authored_role()));
        }
    }

auto ProviderSeat::on(const PaneCanvasValueDrop& d, loom::Mail&) -> void { value_drops.push_back(d); }

auto ProviderSeat::on(const PaneKeys& k, loom::Mail& mail) -> void {
        if (mail.authored_from_role(kWorkshopProvider)) {
            keys_here_ = k.here;
        }
    }

auto ProviderSeat::on(const PaneKey& k, loom::Mail& mail) -> void {
        ++state_.said;
        ++said;
        keys.push_back(k);
        key_authors.push_back(std::string(mail.authored_role()));
        key_asks.push_back(mail.correlation());
    }

auto ProviderSeat::on(const PaneTextInput& t, loom::Mail& mail) -> void {
        ++state_.said;
        ++said;
        typed.push_back(t);
        text_authors.push_back(std::string(mail.authored_role()));
    }

auto ProviderSeat::on(const SeatDo&, loom::Mail& mail) -> void {
        if (next) {
            std::function<void(ProviderSeat&, loom::Mail&)> once;
            once.swap(next);
            once(*this, mail);
        }
    }

auto ProviderSeat::offer(loom::Mail& mail, const PaneOffered& o) -> void {
        offered(o.pane);
        (void)mail.as_role(role_).send_to_role(kWorkshopProvider, o);
    }

auto ProviderSeat::offer(loom::Mail& mail, const zengine::pane::v2::PaneOffered& o) -> void {
        offered(o.pane);
        (void)mail.as_role(role_).send_to_role(kWorkshopProvider, o);
    }

auto ProviderSeat::offer_personally(loom::Mail& mail, const PaneOffered& o) -> void {
        (void)mail.send_to_role(kWorkshopProvider, o);
    }

auto ProviderSeat::say(loom::Mail& mail, const SeatRows& c) -> void {
        said_rows_[c.pane] = c;
        draw(mail, c.pane);
    }

auto ProviderSeat::caret(loom::Mail& mail, const std::string& pane, const RowsCaret& c) -> void {
        carets_[pane] = c;
        draw(mail, pane);
    }

auto ProviderSeat::unspent(loom::Mail& mail, const std::string& pane) -> void {
        unspent_answering(mail, pane, latest_escape_ask());
    }

auto ProviderSeat::unspent_answering(loom::Mail& mail, const std::string& pane, std::uint64_t answering) -> void {
        (void)mail.as_role(role_).send_to_role(kWorkshopProvider, PaneEscapeUnspent{pane},
                                                 answering);
    }

auto ProviderSeat::unspent_anonymously(loom::Mail& mail, const std::string& pane) -> void {
        (void)mail.as_role(role_).send_to_role(kWorkshopProvider, PaneEscapeUnspent{pane}, 0);
    }

auto ProviderSeat::unspent_personally(loom::Mail& mail, const std::string& pane) -> void {
        (void)mail.send_to_role(kWorkshopProvider, PaneEscapeUnspent{pane}, latest_escape_ask());
    }

auto ProviderSeat::latest_escape_ask() const -> std::uint64_t {
        for (std::size_t i = keys.size(); i-- > 0;) {
            if (keys[i].scancode == zengine::input::scan::kEscape && i < key_asks.size()) {
                return key_asks[i];
            }
        }
        return 0;
    }

auto ProviderSeat::declare(loom::Mail& mail, const PaneActions& a) -> void {
        (void)mail.as_role(role_).send_to_role(kWorkshopProvider, a);
    }

auto ProviderSeat::declare_personally(loom::Mail& mail, const PaneActions& a) -> void {
        (void)mail.send_to_role(kWorkshopProvider, a);
    }

auto ProviderSeat::declare_v2(loom::Mail& mail, const zengine::pane::v2::PaneActions& a) -> void {
        (void)mail.as_role(role_).send_to_role(kWorkshopProvider, a);
    }

auto ProviderSeat::declare_v3(loom::Mail& mail, const zengine::pane::v3::PaneActions& a) -> void {
        (void)mail.as_role(role_).send_to_role(kWorkshopProvider, a);
    }

auto ProviderSeat::declare_v4(loom::Mail& mail, const zengine::pane::v4::PaneActions& a) -> void {
        (void)mail.as_role(role_).send_to_role(kWorkshopProvider, a);
    }

auto ProviderSeat::press_at(loom::Mail& mail, const char* role, const gfx::AreaButtonChanged& b) -> void {
        (void)mail.as_role(role_).send_to_role(role, b);
    }

auto ProviderSeat::area_for(const std::string& pane) const -> std::string { return pane_area(role_, pane); }

auto ProviderSeat::room_text(const std::string& pane) const -> gfx::TextMeasure {
        const auto d = drawings_.find(area_for(pane));
        return d == drawings_.end() ? gfx::TextMeasure{} : d->second.rows_room().text;
    }

auto ProviderSeat::room_rows(const std::string& pane) const -> std::int64_t {
        const auto d = drawings_.find(area_for(pane));
        return d == drawings_.end() ? 0 : d->second.rows_room().rows();
    }

auto ProviderSeat::room_columns(const std::string& pane) const -> std::int64_t { return columns_of(area_for(pane)); }

auto ProviderSeat::offered(const std::string& pane) -> void {
        if (std::find(offered_.begin(), offered_.end(), pane) == offered_.end()) {
            offered_.push_back(pane);
        }
    }

auto ProviderSeat::pane_of(const std::string& area) const -> std::string {
        const std::string prefix = role_ + ".";
        return area.rfind(prefix, 0) == 0 ? area.substr(prefix.size()) : std::string();
    }

auto ProviderSeat::drawing_of(const std::string& area) -> WindowDrawing& {
        return drawings_.try_emplace(area, role_, area).first->second;
    }

auto ProviderSeat::columns_of(const std::string& area) const -> std::int64_t {
        const auto d = drawings_.find(area);
        return d == drawings_.end() ? 0 : d->second.rows_room().columns();
    }

auto ProviderSeat::draw(loom::Mail& mail, const std::string& pane) -> void {
        const std::string area = area_for(pane);
        WindowDrawing& drawing = drawing_of(area);
        // A PANE THAT SAID NO ROWS YET DRAWS NOTHING: it waits, and its frame says so.
        const auto last = said_rows_.find(pane);
        if (!drawing.shown() || last == said_rows_.end()) {
            return;
        }
        const RowsRoom room = drawing.rows_room();
        if (room.rows() <= 0 || room.columns() <= 0) {
            return;
        }
        const auto caret = carets_.find(pane);
        const RowsDrawing drawn =
            rows_drawing(last->second.rows, last->second.parts, room, room.columns(),
                         caret == carets_.end() ? RowsCaret{} : caret->second, room.rows());
        drawn_rows_[area] = std::min<std::int64_t>(
            room.rows(), static_cast<std::int64_t>(last->second.rows.size()));
        drawing.draw(drawn.sketch, mail);
    }

SettingsSeat::SettingsSeat(std::string role) : role_(std::move(role)) {}

auto SettingsSeat::on(const PaneCatalogRequested&, loom::Mail&) -> void { ++asks; }

auto SettingsSeat::on(const gfx::AreaRoom& r, loom::Mail& mail) -> void {
        heard.push_back("room");
        authors.push_back(std::string(mail.authored_role()));
        const gfx::TextMeasure text = r.measure();
        rooms.push_back(SeatRoom{std::string(), r.area, text.line > 0 ? r.h / text.line : 0,
                                 text.advance > 0 ? r.w / text.advance : 0});
    }

auto SettingsSeat::on(const PaneSettings& s, loom::Mail& mail) -> void {
        heard.push_back("settings");
        handed.push_back(s);
        authors.push_back(std::string(mail.authored_role()));
    }

auto SettingsSeat::on(const SeatDo&, loom::Mail& mail) -> void {
        if (next) {
            std::function<void(SettingsSeat&, loom::Mail&)> once;
            once.swap(next);
            once(*this, mail);
        }
    }

auto SettingsSeat::offer(loom::Mail& mail, const PaneOffered& o) -> void {
        (void)mail.as_role(role_).send_to_role(kWorkshopProvider, o);
        (void)mail.as_role(role_).send_to_role(kLayoutsRole, PaneSettingsAsked{o.pane});
        if (!rooms.empty()) {
            (void)mail.as_role(role_).send_to_role(gfx::kGraphicsRole,
                                                    gfx::RoomAsked{rooms.back().area});
        }
    }

auto SettingsSeat::declare(loom::Mail& mail, const PaneSettingsDeclared& d) -> void {
        (void)mail.as_role(role_).send_to_role(kLayoutsRole, d);
    }

auto SettingsSeat::unused(loom::Mail& mail, const PaneSettingUnused& u) -> void {
        (void)mail.as_role(role_).send_to_role(kLayoutsRole, u);
    }

auto SettingsSeat::declare_personally(loom::Mail& mail, const PaneSettingsDeclared& d) -> void {
        (void)mail.send_to_role(kLayoutsRole, d);
    }

auto PaneWatcher::on(const zengine::pane::v2::PaneOffered& o, loom::Mail& mail) -> void {
        on(PaneOffered{o.pane, o.name, o.summary}, mail);
    }

auto PaneWatcher::on(const PaneOffered& o, loom::Mail& mail) -> void {
        offers.push_back(o);
        offer_authors.push_back(std::string(mail.authored_role()));
    }

auto PaneWatcher::on(const SeatDo&, loom::Mail& mail) -> void {
        if (next) {
            std::function<void(PaneWatcher&, loom::Mail&)> once;
            once.swap(next);
            once(*this, mail);
        }
    }

auto PaneWatcher::ask(loom::Mail& mail) -> void {
        (void)mail.as_role(kWorkshopProvider).publish(PaneCatalogRequested{});
    }

auto PaneWatcher::ask_personally(loom::Mail& mail) -> void { (void)mail.publish(PaneCatalogRequested{}); }

Booter::Booter(std::vector<std::string>& ok, std::vector<std::string>& no) : ok_(&ok), no_(&no) {}

auto Booter::on(const loom::Result& r, loom::Mail&) -> void { ok_->push_back(r.value); }

auto Booter::on(const loom::Ack&, loom::Mail&) -> void { ++acks; }

auto Booter::on(const loom::Refused& r, loom::Mail&) -> void { no_->push_back(r.reason); }

ControlSeat::ControlSeat(std::vector<std::string>& ok, std::vector<std::string>& no) : ok_(&ok), no_(&no) {}

auto ControlSeat::on(const loom::Result& r, loom::Mail&) -> void { ok_->push_back(r.value); }

auto ControlSeat::on(const loom::Ack&, loom::Mail&) -> void {}

auto ControlSeat::on(const loom::Refused& r, loom::Mail&) -> void { no_->push_back(r.reason); }

auto PaneRig::mount_owner() -> void {
        auto weave = std::make_unique<LayoutsOwner>(layouts_owner_host(host));
        owner = weave.get();
        owner_id = bus.register_weave(std::move(weave), layouts_grant(), std::string(kLayoutsRole));
        owner->zen_set_self(owner_id);
    }

auto PaneRig::nudge_owner() -> void {
        if (!stage_id.valid()) {
            auto stage = std::make_unique<StageHand>();
            StageHand* raw = stage.get();
            loom::Grant grant;
            grant.allow_to_role(LayoutSettingsRequested::zen_name,
                                LayoutSettingsRequested::zen_version, kLayoutsRole);
            stage_id = bus.register_weave(std::move(stage), std::move(grant), std::string(kStageRole));
            raw->zen_set_self(stage_id);
        }
        (void)bus.send(stage_id,
                       loom::Message(loom::to_value(StageNudge{}), loom::WeaveId{}, loom::WeaveId{}, 0));
    }

PaneRig::PaneRig() {
        // The windows' clock, which they count the weaver's clicks by.
        windows.clock([this] { return clock.read(); });
        // THE PAINTERS' DIRECTORY, where Workshop offers the window's skins from, as the host's own.
        host.dir = WORKSHOP_SKINS_DIR;
#ifdef WORKSHOP_THEMES_DIR
        host.shipped_themes_path = WORKSHOP_THEMES_DIR; // the shipped themes, as the build has them
#endif
        painter_id = loom::mount<Painter>(bus, said_conditions, said_transcripts, said_subjects);
        (void)count_desk_drawings(bus, workshop_id, pictures);
    }

auto PaneRig::mount_workshop() -> WorkshopWeave* {
        auto weave = std::make_unique<WorkshopWeave>(host);
        w = weave.get();
        // The press's second version is chosen by the host's answer about the role's holder,
        // wired as workshop.cpp wires it; a case that wants an OLDER host -- one that answers
        // nothing, so every press crosses as v1 -- empties `host.holder_accepts`. With `Live`'s
        // one fixture statement: the stack's stand-in, if a case admits it, is answered present
        // for a ROOM, so the launch door seats it; a case that never admits it gets the host's.
        host.holder_accepts = [this](std::string_view role, const loom::Schema& shape) {
            if (role == stock::kRole && shape.name() == gfx::AreaRoom::zen_name) {
                return true;
            }
            return holder_accepts_on(bus, role, shape);
        };
        host.holder_emits = [this](std::string_view role, const loom::Schema& shape) {
            return holder_emits_on(bus, role, shape);
        };
        host.role_holder = [this](std::string_view role) { return bus.role_holder(role); };
        host.incarnation_of = [this](loom::WeaveId id) {
            return static_cast<std::int64_t>(bus.participant(id).incarnation);
        };
        // ...and where a terminal line can go, read off the same bus at the ask, as workshop.cpp
        // wires it. A case that wants a host listing nothing empties `host.destinations`.
        host.destinations = [this] {
            return bus_destinations(bus, host.terminal != nullptr ? host.terminal->id()
                                                                  : loom::WeaveId{});
        };
        mount_owner();
        register_workshop(std::move(weave));
        return w;
    }

auto PaneRig::workshop_grant() -> loom::Grant { return zengine::workshop::workshop_grant(); }

auto PaneRig::register_workshop(std::unique_ptr<loom::Weave> weave) -> void {
        workshop_id =
            bus.register_weave(std::move(weave), workshop_grant(), std::string(kWorkshopProvider));
        w->zen_set_self(workshop_id);
        quit_watch = std::make_unique<QuitDeliveryWatch>(bus, workshop_id, host.undelivered_quits);
    }

auto PaneRig::take_workshop_off() -> std::unique_ptr<loom::Weave> {
        quit_watch.reset();
        std::unique_ptr<loom::Weave> off = bus.unregister_weave(workshop_id);
        REQUIRE(off != nullptr);
        workshop_id = loom::WeaveId{};
        return off;
    }

auto PaneRig::put_workshop_back(std::unique_ptr<loom::Weave> weave) -> void {
        REQUIRE(weave != nullptr);
        register_workshop(std::move(weave));
    }

auto PaneRig::mount_opening() -> OpeningManager* {
        auto opener = std::make_unique<OpeningManager>(
            std::string(kEditorRole), std::string(kWorkshopProvider), host.managed_pane);
        opening = opener.get();
        loom::Grant arrange;
        arrange.allow_to_role(PresentationTrialRequested::zen_name,
                              PresentationTrialRequested::zen_version, kWorkshopProvider);
        arrange.allow_to_role(zengine::workshop::v3::PresentationAdmitRequested::zen_name,
                              zengine::workshop::v3::PresentationAdmitRequested::zen_version, kWorkshopProvider);
        arrange.allow_to_role(ManagedOpenProgress::zen_name, ManagedOpenProgress::zen_version,
                              kWorkshopProvider);
        arrange.allow_to_role(ManagedOpenSettled::zen_name, ManagedOpenSettled::zen_version,
                              kWorkshopProvider);
        arrange.allow_to_role(ManagedOpenSettled::zen_name, ManagedOpenSettled::zen_version,
                              kEditorRole);
        arrange.allow_to_role(ManagedOpenProgress::zen_name, ManagedOpenProgress::zen_version,
                              kEditorRole); // the `apply` word, as workshop.cpp grants it
        arrange.allow_to_role(zengine::workshop::v3::PrepareSourceRequested::zen_name,
                              zengine::workshop::v3::PrepareSourceRequested::zen_version, kEditorRole);
        arrange.allow_to_any(SourceOpened::zen_name, SourceOpened::zen_version);
        // ...and what a shape it accepts does, from its own page, as workshop.cpp grants it.
        arrange.allow_to_any(zengine::manual::ShapeDocumentShown::zen_name,
                             zengine::manual::ShapeDocumentShown::zen_version);
        loom::allow_poke_answers(arrange); // inspectable, as the host mounts it
        opening_id = bus.register_weave(std::move(opener), std::move(arrange),
                                        std::string(kOpeningRole));
        opening->zen_set_self(opening_id);
        opening->set_authority(bus.mint_joint_authority(
            opening_id, {std::string(kEditorRole), std::string(kWorkshopProvider)}));
        return opening;
    }

auto PaneRig::mount_terminal(int shapes, bool widen) -> loom::TerminalSession* {
        loom::TerminalVocabulary vocab;
        vocab.knows(loom::schema_of<RigNote>())
            .accepts(loom::schema_of<loom::Ack>())
            .accepts(loom::schema_of<loom::Refused>());
        for (int i = 0; i < shapes; ++i) {
            vocab.knows(loom::SchemaBuilder("Extra" + std::to_string(i), 1)
                            .field("seq", loom::Kind::Int)
                            .build());
        }
        loom::Grant grant;
        grant.allow_to_role(RigNote::zen_name, RigNote::zen_version, kRigBoard);
        if (widen) {
            grant.allow_to_any(RigNote::zen_name, RigNote::zen_version);
        }
        // ...ITS ORDINARY VERB, the discovery door's find, by the host's own grant (WL-TERM-18)...
        let_terminal_find_powers(vocab, grant);
        // ...AND, WHEN A CASE ASKS, A PANE'S SETTINGS, by the host's own widening.
        if (terminal_edits_settings) {
            let_terminal_edit_settings(vocab, grant);
        }
        const loom::MountedTerminal mounted = loom::host_mount_terminal(
            bus, std::make_unique<loom::TerminalSession>("workshop", std::move(vocab)),
            std::move(grant));
        host.terminal = mounted.session;
        terminal_id = mounted.id;
        return mounted.session;
    }

auto PaneRig::mount_provider(std::string_view role) -> ProviderSeat* {
        auto seat = std::make_unique<ProviderSeat>(std::string(role));
        ProviderSeat* raw = seat.get();
        loom::Grant grant;
        grant.allow_to_any(PaneOffered::zen_name, PaneOffered::zen_version);
        grant.allow_to_any(zengine::pane::v2::PaneOffered::zen_name, zengine::pane::v2::PaneOffered::zen_version);
        grant.allow_to_any(PaneActions::zen_name, PaneActions::zen_version);
        grant.allow_to_any(zengine::pane::v2::PaneActions::zen_name, zengine::pane::v2::PaneActions::zen_version);
        grant.allow_to_any(zengine::pane::v3::PaneActions::zen_name, zengine::pane::v3::PaneActions::zen_version);
        grant.allow_to_any(zengine::pane::v4::PaneActions::zen_name, zengine::pane::v4::PaneActions::zen_version);
        // ...and that an Escape it was sent was unspent, which is how a pane asks to be put down,
        // and that a press was not its own.
        grant.allow_to_any(PaneEscapeUnspent::zen_name, PaneEscapeUnspent::zen_version);
        grant.allow_to_any(PanePassRequested::zen_name, PanePassRequested::zen_version);
        // ...its rows, drawn in its area.
        grant.allow_to_any(gfx::Drawn::zen_name, gfx::Drawn::zen_version);
        // A SEAT MAY FORGE A PRESS, deliberately: the claim under test is that a PANE refuses a
        // press it did not get from the window that shows its area, and a refusal the bus made
        // unreachable proves nothing.
        grant.allow_to_any(gfx::AreaButtonChanged::zen_name, gfx::AreaButtonChanged::zen_version);
        const loom::WeaveId id =
            bus.register_weave(std::move(seat), std::move(grant), std::string(role));
        raw->zen_set_self(id);
        seat_ids.push_back(id);
        seats_.push_back(raw);
        return raw;
    }

auto PaneRig::mount_settings_seat(std::string_view role) -> SettingsSeat* {
        auto seat = std::make_unique<SettingsSeat>(std::string(role));
        SettingsSeat* raw = seat.get();
        loom::Grant grant;
        grant.allow_to_any(PaneOffered::zen_name, PaneOffered::zen_version);
        grant.allow_to_any(PaneSettingsDeclared::zen_name, PaneSettingsDeclared::zen_version);
        grant.allow_to_role(PaneSettingsAsked::zen_name, PaneSettingsAsked::zen_version,
                            kLayoutsRole);
        grant.allow_to_role(PaneSettingUnused::zen_name, PaneSettingUnused::zen_version,
                            kLayoutsRole);
        grant.allow_to_role(gfx::RoomAsked::zen_name, gfx::RoomAsked::zen_version, gfx::kGraphicsRole);
        const loom::WeaveId id =
            bus.register_weave(std::move(seat), std::move(grant), std::string(role));
        raw->zen_set_self(id);
        settings_seats_.emplace_back(raw, id);
        return raw;
    }

auto PaneRig::unmount_settings_seat(SettingsSeat* seat) -> void {
        for (std::size_t i = 0; i < settings_seats_.size(); ++i) {
            if (settings_seats_[i].first == seat) {
                (void)bus.unregister_weave(settings_seats_[i].second);
                settings_seats_.erase(settings_seats_.begin() + static_cast<std::ptrdiff_t>(i));
                return;
            }
        }
    }

auto PaneRig::settings_seat_id(const SettingsSeat* seat) const -> loom::WeaveId {
        for (const auto& [s, id] : settings_seats_) {
            if (s == seat) {
                return id;
            }
        }
        return loom::WeaveId{};
    }

auto PaneRig::drive(SettingsSeat* seat, std::function<void(SettingsSeat&, loom::Mail&)> what) -> void {
        seat->next = std::move(what);
        (void)bus.send(settings_seat_id(seat), loom::Message(loom::to_value(SeatDo{}),
                                                             loom::WeaveId{}, loom::WeaveId{}, 0));
        bus.drain_until_idle();
    }

auto PaneRig::drive(ProviderSeat* seat, std::function<void(ProviderSeat&, loom::Mail&)> what) -> void {
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
#ifdef WORKSHOP_SO_MENU_PRESENTER

auto PaneRig::load_presenter(const char* path) -> loom::WeaveId {
        const loom::WeaveId id = load("zengine-menu-presenter", path, kPresenterRole);
        REQUIRE(id.valid());
        REQUIRE(load_refusals.empty());
        return id;
    }
#endif

auto PaneRig::load(const char* name, const char* path, const char* role) -> loom::WeaveId {
        return load(name, path, zengine::testing::role_set(role));
    }

auto PaneRig::load(const char* name, const char* path, std::vector<std::string> roles) -> loom::WeaveId {
        loom::Grant reach;
        reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
        const std::size_t before = loaded.size();
        const loom::WeaveId booter =
            loom::mount_granted<Booter>(bus, std::move(reach), loaded, load_refusals);
        bus.send_as(booter, manager,
                    loom::Message(loom::to_value(loom::LoadWeave{name, path, std::move(roles)}),
                                  booter, booter, 0));
        bus.drain_until_idle();
        if (loaded.size() <= before) {
            return loom::WeaveId{};
        }
        return loom::WeaveId{static_cast<std::uint64_t>(std::stoll(loaded.back()))};
    }

auto PaneRig::unload(const char* name) -> bool {
        const loom::WeaveId booter = loom::mount_granted<Booter>(
            bus, loom::load_capability(control), loaded, load_refusals);
        const std::size_t before = load_refusals.size();
        bus.send_as(booter, control,
                    loom::Message(loom::to_value(loom::UnloadLibrary{name}), booter, booter, 0));
        bus.drain_until_idle();
        return load_refusals.size() == before;
    }

auto PaneRig::enqueue_reload(const char* name, const std::string& path) -> void {
        const loom::WeaveId seat = loom::mount_granted<ControlSeat>(
            bus, loom::load_capability(control), loaded, load_refusals);
        bus.send_as(seat, control,
                    loom::Message(loom::to_value(loom::ReloadLibrary{name, path}), seat, seat,
                                  0));
    }

auto PaneRig::enqueue_unload(const char* name) -> void {
        const loom::WeaveId seat = loom::mount_granted<ControlSeat>(
            bus, loom::load_capability(control), loaded, load_refusals);
        bus.send_as(seat, control,
                    loom::Message(loom::to_value(loom::UnloadLibrary{name}), seat, seat, 0));
    }

auto PaneRig::mount_watcher() -> PaneWatcher* {
        auto seat = std::make_unique<PaneWatcher>();
        PaneWatcher* raw = seat.get();
        loom::Grant grant;
        grant.allow_to_any(PaneCatalogRequested::zen_name, PaneCatalogRequested::zen_version);
        grant.allow_to_any(PaneKeys::zen_name, PaneKeys::zen_version);
        grant.allow_to_any(gfx::AreaButtonChanged::zen_name, gfx::AreaButtonChanged::zen_version);
        const loom::WeaveId id =
            bus.register_weave(std::move(seat), std::move(grant), std::string(kWorkshopProvider));
        raw->zen_set_self(id);
        watcher_id = id;
        // ...AND THE LAYOUTS' ROLE BESIDE IT, so a pane that asks for its row holds one.
        (void)zengine::testing::mount_layouts_stand_in(bus);
        return raw;
    }

auto PaneRig::drive_watcher(PaneWatcher* watch, std::function<void(PaneWatcher&, loom::Mail&)> what) -> void {
        watch->next = std::move(what);
        (void)bus.send(watcher_id,
                       loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{}, loom::WeaveId{}, 0));
        bus.drain_until_idle();
    }

auto PaneRig::workshop_action(std::string_view role, const std::string& pane,
                                 const std::string& id) -> RoleAction {
        RoleAction said;
        const loom::WeaveId holder = bus.role_holder(role);
        const loom::ObserverId tap = bus.add_observer([&said, holder](const loom::BusEvent& ev) {
            if (ev.kind == loom::EventKind::Delivered && ev.target == holder &&
                ev.schema_name == PaneActionRequested::zen_name) {
                said.delivered = true;
                said.author = ev.authored_role;
            }
        });
        const loom::Ticket sent = bus.role_send_to_role_as(
            workshop_id, kWorkshopProvider, role,
            loom::Message(loom::to_value(PaneActionRequested{pane, id}), workshop_id, workshop_id,
                          0));
        said.authored = sent.valid();
        bus.drain_until_idle();
        bus.remove_observer(tap);
        return said;
    }

auto PaneRig::publish(const loom::Value& v) -> void {
        come_up();
        (void)bus.publish(loom::Message(v, loom::WeaveId{}, loom::WeaveId{}, 0));
        bus.drain_until_idle();
    }

auto PaneRig::ready() -> void {
        if (own_medium()) {
            return; // the case's own window starts the desk by its offer
        }
        come_up();
    }

auto PaneRig::extent(std::int64_t width, std::int64_t height, std::int64_t adv,
                std::int64_t line, std::int64_t cell) -> void {
        extent_px(cells_px(width), cells_px(height), adv, line, cell);
    }

auto PaneRig::extent_px(std::int64_t width, std::int64_t height, std::int64_t adv,
                   std::int64_t line, std::int64_t cell) -> void {
        REQUIRE_MESSAGE(!own_medium(), "a case that seated a window of its own sizes it there");
        (void)cell;
        const auto [a, l] = face_for(adv, line);
        windows.room(width, height, a, l);
    }

auto PaneRig::extent_on_window(std::int64_t width, std::int64_t height) -> void {
        extent(width, height, 8, 18, gfx::kCellPx);
    }

auto PaneRig::shown() -> DeskShown {
        come_up();
        REQUIRE_MESSAGE(windows.up(), "no window shows this desk");
        return windows.shown();
    }

auto PaneRig::window_button(std::int64_t cx, std::int64_t cy, bool pressed, std::int64_t button) -> void {
        come_up();
        REQUIRE_MESSAGE(windows.up(), "no window shows this desk");
        windows.hand({input::PointerButton{button, pressed, cx, cy, input::space::kCells,
                                           input::mod::kNone}});
        bus.drain_until_idle();
    }

auto PaneRig::window_click(std::int64_t cx, std::int64_t cy, std::int64_t button) -> void {
        window_button(cx, cy, true, button);
        window_button(cx, cy, false, button);
    }

auto PaneRig::hand_button(std::int64_t x, std::int64_t y, std::int64_t space, bool pressed,
                     std::int64_t button) -> void {
        come_up();
        REQUIRE_MESSAGE(windows.up(), "no window shows this desk");
        windows.hand({input::PointerButton{button, pressed, x, y, space, input::mod::kNone}});
        bus.drain_until_idle();
    }

auto PaneRig::window_queue_button(std::int64_t cx, std::int64_t cy, bool pressed,
                             std::int64_t button) -> void {
        come_up();
        windows.queue({input::PointerButton{button, pressed, cx, cy, input::space::kCells,
                                            input::mod::kNone}});
    }

auto PaneRig::window_queue_key(std::int64_t scan, std::int64_t mods) -> void {
        come_up();
        windows.queue({input::KeyPressed{scan, "", mods}});
    }

auto PaneRig::window_beat() -> void {
        windows.pump();
        bus.drain_until_idle();
    }

auto PaneRig::on_gui_window() -> void {
        gui_window = true;
        if (windows.up()) {
            const Session& s = w->session();
            windows.room(s.screen_w, s.screen_h, 8, 18);
        }
    }

auto PaneRig::face_for(std::int64_t adv, std::int64_t line) const -> std::pair<std::int64_t, std::int64_t> {
        if (gui_window && (adv <= 0 || line <= 0)) {
            return {8, 18};
        }
        return {adv, line};
    }

auto PaneRig::own_medium() -> bool {
        if (in_window && !windows.up() &&
            (bus.role_holder(gfx::kGraphicsRole).valid() ||
             bus.role_holder(std::string(gfx::kWindowRolePrefix) + gfx::kTuiWindow).valid() ||
             bus.role_holder(std::string(gfx::kWindowRolePrefix) + gfx::kGuiWindow).valid())) {
            in_window = false;
        }
        return !in_window;
    }

auto PaneRig::come_up() -> void {
        if (own_medium() || windows.up() || w == nullptr || !workshop_id.valid()) {
            return;
        }
        const Session& s = w->session();
        const auto [a, l] = face_for(s.text_advance_px, s.text_line_px);
        windows.room(s.screen_w, s.screen_h, a, l);
    }

auto PaneRig::key(std::int64_t sc, std::int64_t mods) -> void {
        publish(loom::to_value(input::KeyPressed{sc, "", mods}));
    }

auto PaneRig::text(const std::string& s) -> void { publish(loom::to_value(input::TextEntered{s})); }

auto PaneRig::window_settles() -> void {
        for (int beat = 0; beat < 4; ++beat) {
            const std::int64_t was_w = session().screen_w, was_h = session().screen_h;
            windows.pump();
            windows.pump();
            if (session().screen_w == was_w && session().screen_h == was_h) {
                return;
            }
        }
    }

auto PaneRig::close_requested() -> void {
        come_up();
        REQUIRE_MESSAGE(windows.up(), "no window shows this desk");
        (void)bus.role_publish_as(windows.id(), windows.role(),
                                    loom::Message(loom::to_value(gfx::SurfaceCloseRequested{})));
        bus.drain_until_idle();
    }

auto PaneRig::press_cell(std::int64_t cx, std::int64_t cy) -> void {
        hand_moment(input::PointerButton{1, true, cx, cy, input::space::kCells,
                                         input::mod::kNone});
    }

auto PaneRig::wheel_cell(double dy, std::int64_t cx, std::int64_t cy) -> void {
        hand_moment(input::PointerWheel{0.0, dy, cx, cy, input::space::kCells,
                                        input::mod::kNone});
    }

auto PaneRig::press_pixel(std::int64_t px, std::int64_t py) -> void {
        hand_moment(input::PointerButton{1, true, px, py, input::space::kPixels, input::mod::kNone});
    }

auto PaneRig::right_press_cell(std::int64_t cx, std::int64_t cy) -> void {
        hand_moment(input::PointerButton{3, true, cx, cy, input::space::kCells,
                                         input::mod::kNone});
    }

auto PaneRig::release_cell(std::int64_t cx, std::int64_t cy) -> void {
        hand_moment(input::PointerButton{1, false, cx, cy, input::space::kCells,
                                         input::mod::kNone});
    }

auto PaneRig::motion_cell(std::int64_t cx, std::int64_t cy) -> void {
        hand_moment(input::PointerMoved{cx, cy, 0, 0, input::space::kCells,
                                        input::mod::kNone});
    }

auto PaneRig::pick(const PaneRef& ref) -> void { toggle_pane(*this, ref); }

auto PaneRig::artifact_path(const std::string& stem) -> std::string {
        // A stem only some suites are given an image for is answered by the suite's own unit.
        if (const char* own = zengine::testing::workshop_suite_artifact(stem)) {
            return own;
        }
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
        if (stem == "zengine-notices") {
            return WORKSHOP_SO_NOTICES;
        }
        if (stem == "zengine-notices-pane") {
            return WORKSHOP_SO_NOTICES_PANE;
        }
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
#ifdef WORKSHOP_SO_FLOW_PANE
        if (stem == "zengine-flow-pane") {
            return WORKSHOP_SO_FLOW_PANE;
        }
#endif
#ifdef WORKSHOP_SO_VIEW_BUILDER
        if (stem == "zengine-view-builder") {
            return WORKSHOP_SO_VIEW_BUILDER;
        }
#endif
#ifdef WORKSHOP_SO_EDITOR_PANE
        if (stem == "zengine-editor-pane") {
            return WORKSHOP_SO_EDITOR_PANE;
        }
#endif
#ifdef WORKSHOP_SO_THEME
        if (stem == "zengine-theme") {
            return WORKSHOP_SO_THEME;
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
        return stem; // a stem this rig cannot spell refuses at the loader, by name
    }

auto PaneRig::run_plan(load::LoadPlan plan) -> load::Executed {
        loom::Grant operate;
        operate.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
        // A SECOND PLAN ON THIS RIG REPLACES THE FIRST'S OWNER: one holds the role at a time.
        plan_ = nullptr;
        mounted_plan_.reset();
        mounted_plan_ = std::make_unique<load::Mounted>(load::mount(
            bus, std::move(operate), catalog, operator_host, manager, &artifact_path));
        plan_ = &mounted_plan_->owner();
        plan_->begin(std::move(plan));
        for (int turn = 0; turn < 32; ++turn) {
            bus.pump_pending();
        }
        return plan_->outcome();
    }

auto PaneRig::wire_code_source() -> void {
        host.code_source = [this](const std::string& role) {
            if (plan_ != nullptr) {
                return provenance::code_source_of(role, bus, *plan_, host_recipes);
            }
            return provenance::code_source_of(role, bus.role_holder(role), {},
                                              host_recipes.all());
        };
    }

auto PaneRig::mount_arrangement(std::string plan_path) -> loom::WeaveId {
        REQUIRE(plan_ != nullptr); // a door with no owner would describe nothing
        auto door = std::make_unique<ArrangementDoor>(
            *plan_, std::move(plan_path),
            [this](std::string_view role, const loom::Schema& shape) {
                return holder_accepts_on(bus, role, shape);
            });
        ArrangementDoor* raw = door.get();
        loom::Grant say;
        say.allow_to_any(ResolvedArrangement::zen_name, ResolvedArrangement::zen_version);
        say.allow_to_any(zengine::workshop::v2::ResolvedArrangement::zen_name,
                         zengine::workshop::v2::ResolvedArrangement::zen_version);
        say.allow_to_any(zengine::manual::ShapeDocumentShown::zen_name,
                         zengine::manual::ShapeDocumentShown::zen_version);
        const loom::WeaveId id = bus.register_weave(std::move(door), std::move(say),
                                                    std::string(kArrangementRole));
        raw->zen_set_self(id);
        bus.drain_until_idle();
        return id;
    }

auto PaneRig::mount_powers() -> loom::WeaveId {
        auto door = std::make_unique<PowersDoor>(catalog);
        PowersDoor* raw = door.get();
        loom::Grant say;
        say.allow_to_any(PowersFound::zen_name, PowersFound::zen_version);
        say.allow_to_any(PowerDescribed::zen_name, PowerDescribed::zen_version);
        say.allow_to_any(zengine::manual::ShapeDocumentShown::zen_name,
                         zengine::manual::ShapeDocumentShown::zen_version);
        const loom::WeaveId id =
            bus.register_weave(std::move(door), std::move(say), std::string(kPowersRole));
        raw->zen_set_self(id);
        bus.drain_until_idle();
        return id;
    }

auto PaneRig::expose_host_sources() -> void {
        const op::MountReport done =
            mount_host_sources(catalog, host_sources(project_anchor, host_recipes));
        REQUIRE_MESSAGE(done.ok, done.reason);
    }

auto PaneRig::mount_sampler() -> loom::WeaveId {
        auto door = std::make_unique<SampleDoor>(catalog);
        SampleDoor* raw = door.get();
        loom::Grant say;
        say.allow_to_any(SourceSampled::zen_name, SourceSampled::zen_version);
        say.allow_to_any(zengine::manual::ShapeDocumentShown::zen_name,
                         zengine::manual::ShapeDocumentShown::zen_version);
        const loom::WeaveId id =
            bus.register_weave(std::move(door), std::move(say), std::string(kSampleRole));
        raw->zen_set_self(id);
        bus.drain_until_idle();
        return id;
    }

auto PaneRig::session() -> Session& { return const_cast<Session&>(w->session()); }

auto PaneRig::run() const -> const SetupState& { return owner->layouts().run(); }

auto PaneRig::last_notice() const -> const std::string& { return w->session().notice; }

auto PaneRig::conditions() const -> std::vector<Condition> {
        return attention_conditions(w->session(), frontier_claimed(bus));
    }

auto PaneRig::glance() const -> std::string {
        return said_conditions.empty() ? std::string()
                                       : attention_glance(said_conditions.back().rows);
    }

auto external_region_rows(const DeskShown& s, const CellRect& body) -> std::vector<std::string> {
    std::vector<std::string> out;
    const std::int64_t columns = s.advance > 0 ? gfx::px_of_cells(body.w) / s.advance : 0;
    const std::int64_t bottom = gfx::px_of_cells(body.y + body.h);
    for (std::int64_t y = gfx::px_of_cells(body.y); s.line > 0 && y < bottom; y += s.line) {
        out.push_back(s.text_at_px(gfx::px_of_cells(body.x), y, columns));
    }
    while (!out.empty() && out.back().empty()) out.pop_back();
    return out;
}

auto external_rows(const DeskShown& s, const CellRect& body) -> std::vector<std::string> {
    std::vector<std::string> out = external_region_rows(s, body);
    const std::size_t header = static_cast<std::size_t>(kExternalHeaderRows);
    out.erase(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(
                                             out.size() < header ? out.size() : header));
    return out;
}

auto good_offer() -> PaneOffered { return PaneOffered{"hello", "Hello", "a bounded external greeting"}; }

auto bytes(std::size_t n, char c) -> std::string { return std::string(n, c); }

auto external_body_rect(const Session& s, std::int64_t kind) -> CellRect {
    const Screen sc = screen_of(s);
    const ExternalBodyPlace body =
        external_body_place(bounds_of(s.panes, s.setup.active, kind, sc).rect, sc,
                            external_title_rows(s.panes, kind, s.pane_titles));
    return cells_covered(PixelRect{body.region_x, body.region_y, body.region_w, body.region_h});
}

auto info_ref() -> PaneRef { return PaneRef{"zengine.info", "info"}; }

auto notices_pane_ref() -> PaneRef { return PaneRef{"zengine.notices-pane", "notices"}; }

auto two_overlays() -> Setup {
    Setup s;
    s.name = "Arranged";
    REQUIRE(add_pane(s, stock_ref()));
    REQUIRE(add_pane(s, second_ref()));
    return s;
}

auto live(Live& t) -> Session& { return const_cast<Session&>(t.session()); }

auto place_at_canvas(Session& s, const PaneRef& ref, std::int64_t x, std::int64_t y) -> Written {
    return author_pane_place(s.setup.active, ref, x, y - screen_of(s).room_y);
}

auto authored_order(const Session& s) -> std::vector<std::int64_t> {
    return presentation_order(s.setup.active, s.panes);
}

auto index_of(const std::vector<std::int64_t>& order, std::int64_t kind) -> std::int64_t {
    for (std::size_t i = 0; i < order.size(); ++i) {
        if (order[i] == kind) {
            return static_cast<std::int64_t>(i);
        }
    }
    return -1;
}

auto painted_order(const Session& s) -> std::vector<std::int64_t> {
    return effective_pane_order(s.setup.active, s.panes);
}

auto ranks_of(const Setup& s) -> std::vector<std::int64_t> {
    std::vector<std::int64_t> out;
    for (const SetupPane& row : s.panes) {
        out.push_back(row.front);
    }
    return out;
}

auto is_permutation(const Setup& s) -> bool {
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

auto menu_bounds(const Session& s) -> PixelRect {
    return s.presented.open ? presented_bounds(s, screen_of(s)) : context_bounds(s, screen_of(s));
}

auto context_cell_x(const Session& s) -> std::int64_t {
    return gfx::cell_of_pixel(menu_bounds(s).x) + kChromeCells + 1;
}

auto context_entry_cell_y(const Session& s, std::size_t index) -> std::int64_t {
    return gfx::cell_of_pixel(menu_bounds(s).y) + kChromeCells +
           static_cast<std::int64_t>(index);
}

auto menu_shown(const Session& s) -> bool {
    return s.presented.open && s.presented.rows > 0;
}

auto context_rows_on(const DeskShown& shown, const Session& s) -> std::vector<std::string> {
    const Screen sc = screen_of(s);
    const PixelRect popup = menu_bounds(s);
    const ScreenRegion want = prose_region(prose_place(popup, sc));
    std::vector<std::string> out;
    const std::int64_t columns = shown.advance > 0 ? want.w / shown.advance : 0;
    for (std::int64_t y = want.y; shown.line > 0 && y < want.y + want.h; y += shown.line) {
        out.push_back(shown.text_at_px(want.x, y, columns));
    }
    while (!out.empty() && out.back().empty()) out.pop_back();
    return out;
}

auto open_pane(Live& t, const PaneRef& ref) -> void {
    const PaneLaunchAnswered said = seat_pane(t, ref);
    REQUIRE_MESSAGE(said.refusal.empty(), said.refusal);
    REQUIRE(has_pane(t.session().setup.active, ref));
}

auto open_at_top_right(Live& t, std::int64_t kind) -> void {
    pick(t, kind);
    REQUIRE(t.session().panes.has(kind));
    stage_desk(t, [&](Setup& desk) {
        for (SetupPane& row : desk.panes) {
            if (row.ref == ref_of(kind)) {
                row.place = PanePlace{ui::kPlaceTopRight, 0, 0};
                row.width = PaneSize{pane_unit::kPixels, cells_px(kStripCols)};
                row.height = PaneSize{ui::kSizeFill, 0};
            }
        }
    });
    const Screen sc = screen_of(t.session());
    const PaneBounds at = bounds_of(t.session().panes, t.session().setup.active, kind, sc);
    REQUIRE(at.resolved.x + at.resolved.w == sc.room_w);
    REQUIRE(at.resolved.y == sc.room_y);
    REQUIRE(at.resolved.h == sc.room_h);
}

auto select_pane(Live& t, const PaneRef& ref) -> void {
    REQUIRE(t.session().arrange.open);
    for (std::size_t guard = 0; guard <= t.session().setup.active.panes.size() + 1; ++guard) {
        if (t.session().arrange.pane == ref) {
            return;
        }
        t.key(input::scan::kTab);
    }
    FAIL("no arrangeable row for ", ref_text(ref));
}

auto enter_arrange_desk(Live& t) -> void {
    t.key(input::scan::kW);
    t.text("w");
    REQUIRE(t.session().arrange.open);
    REQUIRE(t.session().arrange.desk);
}

auto intro_ref() -> PaneRef { return PaneRef{kIntroRole, kIntroPane}; }

auto intro_row(PaneRig& r, const char* pane) -> const RuntimePane* {
    return r.session().panes.runtime.find(kIntroRole, pane);
}

auto any_row(const std::vector<TextRow>& rows, const std::string& needle) -> bool {
    for (const TextRow& r : rows) {
        if (r.text.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

auto any_row(const std::vector<std::string>& rows, const std::string& needle) -> bool {
    for (const std::string& r : rows) {
        if (r.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

auto loaded_population(std::size_t n) -> std::vector<intro::LoadedWeave> {
    std::vector<intro::LoadedWeave> out;
    for (std::size_t i = 0; i < n; ++i) {
        out.push_back(intro::LoadedWeave{"weave-" + std::to_string(i), {"role." + std::to_string(i)}});
    }
    return out;
}

auto regions_at(const ScreenCanvas& c,
                                            std::int64_t x, std::int64_t y) -> std::vector<ScreenRegion> {
    std::vector<ScreenRegion> out;
    for (const ScreenRegion& r : all_texts(c)) {
        if (gfx::cell_of_pixel(r.x) == x && gfx::cell_of_pixel(r.y) == y) {
            out.push_back(r);
        }
    }
    return out;
}

auto screen_session(std::int64_t w, std::int64_t h, std::int64_t advance,
                              std::int64_t line, std::int64_t cell_px) -> Session {
    Session s;
    s.screen_w = w;
    s.screen_h = h;
    s.text_advance_px = advance;
    s.text_line_px = line;
    s.cell_px = cell_px;
    return s;
}

auto cell_mid_px(std::int64_t cell) -> std::int64_t {
    return cell * gfx::kCellPx + gfx::kCellPx / 2;
}

auto external_pane_rect(const Session& s, std::int64_t kind) -> PixelRect {
    return bounds_of(s.panes, s.setup.active, kind, screen_of(s)).rect;
}

auto shows_canvas(const ExternalPane& pane) -> bool { return pane.held && pane.in_window; }

auto CanvasRows::empty() const noexcept -> bool { return columns <= 0 || rows <= 0; }

auto CanvasRows::row_y(std::int64_t row) const noexcept -> std::int64_t { return y + row * line; }

auto CanvasRows::column_x(std::int64_t column) const noexcept -> std::int64_t { return x + column * advance; }

auto held_area_of(std::int64_t kind) -> std::string {
    if (held_desks().empty()) return std::string();
    const Session& s = held_desks().back().session();
    const RuntimePane* row = s.panes.runtime.of_kind(kind);
    return row == nullptr ? std::string() : pane_window_area(s.panes, *row);
}

auto held_canvas_rows(const ExternalPane& pane) -> CanvasRows {
    CanvasRows out;
    if (held_desks().empty()) return out;
    const std::string area = held_area_of(pane.kind);
    const zengine::graphics::window::WinArea* a = held_desks().back().mirror->area(area);
    if (a == nullptr) return out;
    const gfx::TextMeasure face = held_desks().back().mirror->measure(area);
    out.x = pane.placed_x;
    out.y = pane.placed_y;
    out.advance = face.advance;
    out.line = face.line;
    out.width = a->w;
    out.height = a->h;
    out.columns = face.advance > 0 ? a->w / face.advance : 0;
    out.rows = face.line > 0 ? a->h / face.line : 0;
    return out;
}

auto held_row_raw(std::int64_t kind, std::int64_t row) -> std::string {
    if (held_desks().empty()) return std::string();
    const std::string area = held_area_of(kind);
    const zengine::graphics::window::WinArea* a = held_desks().back().mirror->area(area);
    if (a == nullptr) return std::string();
    std::vector<std::pair<std::int64_t, std::string>> runs;
    for (const zengine::graphics::window::KeptNode& k : a->nodes) {
        const std::string& p = k.node.path;
        const std::string row_at = "row" + std::to_string(row) + ".";
        const std::string lit_at = "lit" + std::to_string(row) + ".";
        if (p.rfind(row_at, 0) != 0 && p.rfind(lit_at, 0) != 0) continue;
        if (const auto run = gfx::operands<gfx::TextArgs>(k.node)) {
            runs.emplace_back(run->x, run->text);
        }
    }
    std::stable_sort(runs.begin(), runs.end(),
                     [](const auto& x, const auto& y) { return x.first < y.first; });
    std::string out;
    for (const auto& [x, text] : runs) out += text;
    return out;
}

auto held_runs(std::int64_t kind) -> std::vector<HeldRun> {
    std::vector<HeldRun> out;
    if (held_desks().empty()) return out;
    const std::string area = held_area_of(kind);
    const zengine::graphics::window::WinArea* a = held_desks().back().mirror->area(area);
    const gfx::TextMeasure face = held_desks().back().mirror->measure(area);
    if (a == nullptr || face.line <= 0) return out;
    std::map<std::int64_t, std::pair<std::int64_t, gfx::Color>> first; // row: x, colour
    for (const zengine::graphics::window::KeptNode& k : a->nodes) {
        // ...a rows drawing's runs (`workshop::rows_drawing`), `row<n>.` or `lit<n>.`
        const std::string& p = k.node.path;
        if ((p.rfind("row", 0) != 0 && p.rfind("lit", 0) != 0) || p.size() < 4 ||
            !std::isdigit(static_cast<unsigned char>(p[3]))) {
            continue;
        }
        const auto run = gfx::operands<gfx::TextArgs>(k.node);
        if (!run) continue;
        const std::int64_t row = gfx::floor_div_px(run->y, face.line);
        const auto at = first.find(row);
        if (at == first.end() || run->x < at->second.first) first[row] = {run->x, zengine::graphics::pool_color_of(
                zengine::graphics::window::worn<gfx::TextLook>(*a, k.node)->ink)};
    }
    for (const auto& [row, said] : first) {
        std::string text = held_row_raw(kind, row);
        text.erase(text.find_last_not_of(' ') + 1);
        out.push_back(HeldRun{row, text, said.second});
    }
    if (!out.empty() || face.advance <= 0) return out;
    // A CANVAS PICTURE'S RUNS (`workshop::canvas_sketch`): each row its runs set at their columns,
    // in the colour of the first.
    std::map<std::int64_t, std::pair<std::string, std::pair<std::int64_t, gfx::Color>>> rows;
    for (const zengine::graphics::window::KeptNode& k : a->nodes) {
        if (k.node.path.rfind("picture/", 0) != 0) continue;
        const auto run = gfx::operands<gfx::TextArgs>(k.node);
        if (!run) continue;
        const std::int64_t row = gfx::floor_div_px(run->y, face.line);
        const auto col = static_cast<std::size_t>((std::max)(std::int64_t{0}, run->x / face.advance));
        auto& [text, first_run] = rows[row];
        if (text.empty() && first_run.first == 0 && first_run.second.none()) {
            first_run = {run->x, zengine::graphics::pool_color_of(zengine::graphics::window::worn<gfx::TextLook>(*a,
                    k.node)->ink)};
        } else if (run->x < first_run.first) {
            first_run = {run->x, zengine::graphics::pool_color_of(zengine::graphics::window::worn<gfx::TextLook>(*a,
                    k.node)->ink)};
        }
        if (text.size() < col + run->text.size()) text.resize(col + run->text.size(), ' ');
        text.replace(col, run->text.size(), run->text);
    }
    for (auto& [row, said] : rows) {
        std::string text = said.first;
        text.erase(text.find_last_not_of(' ') + 1);
        out.push_back(HeldRun{row, text, said.second.second});
    }
    if (!out.empty()) return out;
    // ANY OTHER DRAWING: its rows as the window reads them, each in the colour of the leftmost text
    // drawn on it, a group's place carried to what it holds.
    std::map<std::string, std::pair<std::int64_t, std::int64_t>> origin;
    std::map<std::int64_t, std::pair<std::int64_t, gfx::Color>> role_at; // row: x, colour
    for (const zengine::graphics::window::KeptNode& k : a->nodes) {
        const std::string& p = k.node.path;
        const std::string parent = gfx::parent_of(p);
        const auto from = origin.find(parent);
        const std::int64_t ox = from == origin.end() ? 0 : from->second.first;
        const std::int64_t oy = from == origin.end() ? 0 : from->second.second;
        if (const auto g = gfx::operands<gfx::GroupArgs>(k.node)) {
            origin[p] = {ox + g->x, oy + g->y};
            continue;
        }
        if (p.rfind("parts/", 0) == 0 || p.rfind("marks/", 0) == 0 || p.rfind("sel", 0) == 0 ||
            p == "caret" || gfx::operands<gfx::TextArgs>(k.node) == nullptr)
            continue;
        const auto t = gfx::operands<gfx::TextArgs>(k.node);
        if (!t) continue;
        const std::int64_t row = gfx::floor_div_px(oy + t->y, face.line);
        const auto at = role_at.find(row);
        if (at == role_at.end() || ox + t->x < at->second.first) role_at[row] = {ox + t->x,
                zengine::graphics::pool_color_of(zengine::graphics::window::worn<gfx::TextLook>(*a, k.node)->ink)};
    }
    const std::vector<std::string> said = held_read_rows(kind);
    for (const auto& [row, leftmost] : role_at) {
        if (row < 0 || row >= static_cast<std::int64_t>(said.size())) continue;
        out.push_back(HeldRun{row, said[static_cast<std::size_t>(row)], leftmost.second});
    }
    return out;
}

auto held_area(std::int64_t kind) -> const zengine::graphics::window::WinArea* {
    if (held_desks().empty()) return nullptr;
    return held_desks().back().mirror->area(held_area_of(kind));
}

auto held_shown_node(const std::string& path) -> bool {
    return path.rfind("parts/", 0) != 0 && path.rfind("marks/", 0) != 0 &&
           path.rfind("sel", 0) != 0 && path != "caret";
}

auto held_texts(std::int64_t kind) -> std::vector<HeldText> {
    std::vector<HeldText> out;
    if (const zengine::graphics::window::WinArea* a = held_area(kind)) {
        for (const zengine::graphics::window::KeptNode& k : a->nodes) {
            if (!held_shown_node(k.node.path)) continue;
            if (const auto t = gfx::operands<gfx::TextArgs>(k.node)) {
                out.push_back(HeldText{t->x, t->y, t->text, zengine::graphics::pool_color_of(
                        zengine::graphics::window::worn<gfx::TextLook>(*a, k.node)->ink)});
            }
        }
    }
    return out;
}

auto held_rects(std::int64_t kind) -> std::vector<HeldRect> {
    std::vector<HeldRect> out;
    if (const zengine::graphics::window::WinArea* a = held_area(kind)) {
        for (const zengine::graphics::window::KeptNode& k : a->nodes) {
            if (!held_shown_node(k.node.path) || gfx::operands<gfx::RectArgs>(k.node) == nullptr) continue;
            if (const auto r = gfx::operands<gfx::RectArgs>(k.node)) {
                out.push_back(HeldRect{r->x, r->y, r->w, r->h, zengine::graphics::pool_color_of(
                        zengine::graphics::window::worn<gfx::Paint>(*a, k.node)->fill)});
            }
        }
    }
    return out;
}

auto wears_ground(const ExternalPane& pane) -> bool {
    if (held_desks().empty()) return false;
    const std::string area = held_area_of(pane.kind);
    const zengine::graphics::window::WinArea* a = held_desks().back().mirror->area(area);
    if (a == nullptr) return false;
    for (const zengine::graphics::window::KeptNode& k : a->nodes) {
        if (k.node.path != "room" && k.node.path != "ground") continue;
        const auto rect = gfx::operands<gfx::RectArgs>(k.node);
        return rect && rect->x <= 0 && rect->y <= 0 && rect->x + rect->w >= a->w &&
               rect->y + rect->h >= a->h && !zengine::graphics::window::worn<gfx::Paint>(*a, k.node)->fill.none();
    }
    return false;
}

auto held_lit_rows(std::int64_t kind) -> std::vector<HeldLitRow> {
    std::vector<HeldLitRow> out;
    if (held_desks().empty()) return out;
    const std::string area = held_area_of(kind);
    const zengine::graphics::window::WinArea* a = held_desks().back().mirror->area(area);
    const gfx::TextMeasure face = held_desks().back().mirror->measure(area);
    if (a == nullptr || face.line <= 0) return out;
    for (const zengine::graphics::window::KeptNode& k : a->nodes) {
        if (k.node.path.rfind("ground", 0) != 0 || k.node.path == "ground") continue;
        const auto rect = gfx::operands<gfx::RectArgs>(k.node);
        if (!rect) continue;
        HeldLitRow lit{gfx::floor_div_px(rect->y, face.line), zengine::graphics::pool_color_of(
                zengine::graphics::window::worn<gfx::Paint>(*a, k.node)->fill), gfx::no_color()};
        const std::string words = "lit" + k.node.path.substr(6) + ".0";
        for (const zengine::graphics::window::KeptNode& t : a->nodes) {
            if (t.node.path != words) continue;
            if (const auto run = gfx::operands<gfx::TextArgs>(t.node)) lit.ink = zengine::graphics::pool_color_of(
                    zengine::graphics::window::worn<gfx::TextLook>(*a, t.node)->ink);
        }
        out.push_back(lit);
    }
    return out;
}

auto row_cell_at(const CanvasRows& rows, std::int64_t x, std::int64_t y) -> RowCell {
    RowCell out;
    if (rows.advance <= 0 || rows.line <= 0) return out;
    out.row = gfx::floor_div_px(y - rows.y, rows.line);
    out.column = gfx::floor_div_px(x - rows.x, rows.advance);
    out.shown = out.row >= 0 && out.row < rows.rows && out.column >= 0 && out.column < rows.columns;
    return out;
}

auto held_reading(std::int64_t kind) -> HeldReading {
    HeldReading out;
    if (held_desks().empty()) return out;
    const std::string area = held_area_of(kind);
    const DrawnMirror& mirror = *held_desks().back().mirror;
    const zengine::graphics::window::WinArea* a = mirror.area(area);
    if (a == nullptr) return out;
    const gfx::TextMeasure face = mirror.measure(area);
    if (face.advance <= 0 || face.line <= 0) return out;
    const auto at = zengine::graphics::window::index_of(*a);
    std::vector<const zengine::graphics::window::KeptNode*> order;
    for (const zengine::graphics::window::KeptNode& k : a->nodes) order.push_back(&k);
    std::stable_sort(order.begin(), order.end(), [&](const auto* x, const auto* y) {
        return zengine::graphics::window::key_of(at, x->node.path) <
               zengine::graphics::window::key_of(at, y->node.path);
    });
    gfx::Sketch sketch;
    for (const auto* k : order) sketch.put(k->node, zengine::graphics::window::look_of(*a, k->node));
    for (const gfx::ElementRecord& r : a->records) sketch.say(r);
    const std::int64_t columns = a->w / face.advance;
    component::RowMap<DrawnPart> map;
    map.begin();
    for (const TextRow& row : drawn_rows(sketch, a->h / face.line, columns, map, face)) {
        const std::size_t end = row.text.find_last_not_of(' ');
        out.rows.push_back(end == std::string::npos ? std::string() : row.text.substr(0, end + 1));
        out.inks.push_back(row.ink);
    }
    while (!out.rows.empty() && out.rows.back().empty()) {
        out.rows.pop_back();
        out.inks.pop_back();
    }
    (void)map.settle();
    const auto available = [&](const DrawnPart& p) {
        for (const gfx::ElementRecord& r : a->records) {
            if (r.path == p.path) return zengine::graphics::window::record_available(r);
        }
        return true;
    };
    for (RowPart& p : row_parts(map, columns, [](const DrawnPart& p) { return p.action; }, available)) {
        // A RUN'S BLANKS ARE ITS OWN: the part's place on its row runs from the first column a run
        // it asks stands at, blanks and all, to the last such run's end.
        std::int64_t first = p.column, end = p.column + p.columns;
        for (const gfx::ElementRecord& r : a->records) {
            if (r.action != p.name) continue;
            for (const auto* k : order) {
                // ...a top-level run's, whose place is the area's (a part's group draws its own)
                if (!gfx::under(k->node.path, r.path) || k->node.path.find('/') != std::string::npos)
                    continue;
                const auto t = gfx::operands<gfx::TextArgs>(k->node);
                if (!t || gfx::floor_div_px(t->y, face.line) != p.row) continue;
                const std::int64_t from = gfx::floor_div_px(t->x, face.advance);
                first = (std::min)(first, from);
                end = (std::max)(end, from + static_cast<std::int64_t>(t->text.size()));
            }
        }
        p.column = first;
        p.columns = (std::min)(end, columns) - first;
        out.parts.emplace(p.name, p);
    }
    // THE MARKS: the caret a bar at the left of the cell it stands before, or in a room of cells
    // the character it stands before drawn again, the selection its columns again, a run a row.
    bool selected = false;
    for (const zengine::graphics::window::KeptNode& k : a->nodes) {
        if (k.node.path == "caret") {
            if (const auto line = gfx::operands<gfx::LineArgs>(k.node)) {
                out.caret.row = gfx::floor_div_px(line->y1, face.line);
                out.caret.column = gfx::floor_div_px(line->x1, face.advance);
            } else if (const auto mark = gfx::operands<gfx::TextArgs>(k.node)) {
                // in a room of cells, the character the caret stands before, drawn again
                out.caret.row = gfx::floor_div_px(mark->y, face.line);
                out.caret.column = gfx::floor_div_px(mark->x, face.advance);
            }
        } else if (k.node.path.rfind("sel", 0) == 0) {
            if (const auto run = gfx::operands<gfx::TextArgs>(k.node)) {
                const std::int64_t row = gfx::floor_div_px(run->y, face.line);
                const std::int64_t column = gfx::floor_div_px(run->x, face.advance);
                const std::int64_t end = column + static_cast<std::int64_t>(run->text.size());
                if (!selected || row < out.caret.sel_begin_row ||
                    (row == out.caret.sel_begin_row && column < out.caret.sel_begin_col)) {
                    out.caret.sel_begin_row = row;
                    out.caret.sel_begin_col = column;
                }
                if (!selected || row > out.caret.sel_end_row ||
                    (row == out.caret.sel_end_row && end > out.caret.sel_end_col)) {
                    out.caret.sel_end_row = row;
                    out.caret.sel_end_col = end;
                }
                selected = true;
            }
        }
    }
    return out;
}

auto held_read_rows(std::int64_t kind) -> std::vector<std::string> { return held_reading(kind).rows; }

auto held_caret(const ExternalPane& pane) -> RowsCaret { return held_reading(pane.kind).caret; }

auto held_canvas_text(const ExternalPane& pane) -> std::vector<std::string> {
    return held_reading(pane.kind).rows;
}

auto held_row_texts(const ExternalPane& pane) -> std::vector<std::string> {
    return held_canvas_text(pane);
}

auto held_control_parts_problem(const Session& s, std::int64_t kind) -> std::string {
    std::vector<std::string> names;
    for (const auto& [name, part] : held_reading(kind).parts) names.push_back(name);
    std::vector<std::string> rows;
    for (const PaneRows& declared : s.keymap.panes) {
        if (declared.pane != kind) continue;
        for (const PaneRow& row : declared.rows) rows.push_back(row.id);
    }
    const RuntimePane* offered = s.panes.runtime.of_kind(kind);
    return control_parts_problem(offered ? offered->provider + "/" + offered->pane
                                         : "pane " + std::to_string(kind),
                                 names, rows);
}

auto held_parts(const Session& s, std::int64_t kind) -> std::map<std::string, RowPart> {
    const std::string problem = held_control_parts_problem(s, kind);
    CHECK_MESSAGE(problem.empty(), problem);
    return held_reading(kind).parts;
}

auto held_part_text(const Session& s, std::int64_t kind, const RowPart& part) -> std::string {
    (void)s;
    if (part.row < 0) return std::string();
    const std::vector<std::string> rows = held_reading(kind).rows;
    if (part.row >= static_cast<std::int64_t>(rows.size())) return std::string();
    const std::string& row = rows[static_cast<std::size_t>(part.row)];
    const auto from = static_cast<std::size_t>(part.column);
    return from >= row.size() ? std::string() : row.substr(from, static_cast<std::size_t>(part.columns));
}

auto external_body_of(const Session& s, std::int64_t kind) -> ExternalBodyPlace {
    return external_body_place(external_pane_rect(s, kind), screen_of(s),
                               external_title_rows(s.panes, kind, s.pane_titles));
}

SelectionListener::SelectionListener(Ears& ears) : ears_(&ears) {}

auto SelectionListener::on(const intro::LoadedSelected& s, loom::Mail& mail) -> void {
        ++state_.frames;
        ears_->heard.push_back(s);
        ears_->authors.push_back(std::string(mail.authored_role()));
    }

auto canvas_rows_shown(const Session& s, const DeskShown& shown,
                                                  std::int64_t kind) -> std::vector<std::string> {
    std::vector<std::string> out;
    const ExternalPane* pane = s.panes.external_pane(kind);
    if (pane == nullptr || !shows_canvas(*pane)) return out;
    const std::int64_t columns = shown.advance > 0 ? pane->placed_w / shown.advance : 0;
    for (std::int64_t y = pane->placed_y; shown.line > 0 && y < pane->placed_y + pane->placed_h;
         y += shown.line) {
        out.push_back(shown.text_at_px(pane->placed_x, y, columns));
    }
    while (!out.empty() && out.back().empty()) out.pop_back();
    return out;
}

auto loaded_heading(const std::string& count) -> std::string {
    return "loaded weaves -- " + count + "  [" +
           std::string(static_cast<std::size_t>(intro::kSearchBoxLeast - 2), '_') + "]";
}

auto loaded_rows(PaneRig& r, std::int64_t kind) -> std::vector<std::string> {
    if (const ExternalPane* pane = r.session().panes.external_pane(kind); pane && shows_canvas(*pane)) {
        return canvas_rows_shown(r.session(), r.shown(), kind);
    }
    return external_rows(r.shown(), external_body_rect(r.session(), kind));
}

auto named_by(const std::string& row) -> std::string {
    const std::size_t at = row.find(" @");
    return at == std::string::npos ? std::string() : row.substr(2, at - 2);
}

auto is_entry_row(const std::string& row) -> bool {
    return row.size() > 2 && row.find(" @") != std::string::npos &&
           (row.rfind(intro::kUnselectedMark, 0) == 0 || row.rfind(intro::kSelectedMark, 0) == 0) &&
           row.find(intro::kElided) == std::string::npos;
}

auto composer_ref() -> PaneRef { return PaneRef{kComposerRole, kComposePane}; }

auto DesktopSeat::on(const AppActionRequested& asked, loom::Mail& mail) -> void {
        asked_.push_back(asked.id);
        last_ask_ = mail.correlation();
        if (asked.id == kDeselectId && autoanswer) {
            // ECHOED UNDER THE NUMBER IT ARRIVED ON, which is what makes this answer about
            // THIS keystroke and no other.
            (void)mail.as_role(kDesktopRole)
                .send_to_role(kWorkshopProvider, DeselectRequested{}, mail.correlation());
        }
    }

auto DesktopSeat::on(const ActionsJudged& said, loom::Mail& mail) -> void {
        verdicts_.push_back(Verdict{mail.correlation(), mail.answers_ask(), said});
        if (!said.accepted) {
            refusals_.push_back(said.refusal);
        }
    }

auto DesktopSeat::on(const ActionsWithdrawn& said, loom::Mail&) -> void {
        withdrawals_.push_back(said);
        refusals_.push_back(said.refusal);
    }

auto DesktopSeat::on(const PaneLaunchAnswered& said, loom::Mail&) -> void { launched_.push_back(said); }

auto DesktopSeat::on(const PaneCloseAnswered& said, loom::Mail&) -> void { closed_.push_back(said); }

auto DesktopSeat::on(const zengine::workshop::v1::PaneLaunchAnswered& said, loom::Mail&) -> void { launched_v1_.push_back(said); }

auto DesktopSeat::on(const zengine::workshop::v2::PaneCloseAnswered& said, loom::Mail&) -> void { closed_v2_.push_back(said); }

auto DesktopSeat::on(const zengine::workshop::v1::PaneToggleAnswered& said, loom::Mail&) -> void { toggled_v1_.push_back(said); }

auto DesktopSeat::on(const SeatDo&, loom::Mail& mail) -> void {
        if (next) {
            std::function<void(DesktopSeat&, loom::Mail&)> once;
            once.swap(next);
            once(*this, mail);
        }
    }

auto DesktopSeat::declare(loom::Mail& mail, const AppActions& a, std::uint64_t attempt) -> void {
        (void)mail.as_role(kDesktopRole).send_to_role(kWorkshopProvider, a, attempt);
    }

auto DesktopSeat::declare_personally(loom::Mail& mail, const AppActions& a) -> void {
        (void)mail.send_to_role(kWorkshopProvider, a);
    }

auto DesktopSeat::launch(loom::Mail& mail, const std::string& role, const std::string& pane) -> void {
        (void)mail.as_role(kDesktopRole)
            .send_to_role(kWorkshopProvider, PaneLaunchRequested{role, pane});
    }

auto DesktopSeat::close(loom::Mail& mail, const std::string& role, const std::string& pane) -> void {
        (void)mail.as_role(kDesktopRole)
            .send_to_role(kLayoutsRole, PaneCloseRequested{role, pane});
    }

auto DesktopSeat::launch_v1(loom::Mail& mail, const std::string& office, const std::string& pane) -> void {
        (void)mail.as_role(kDesktopRole)
            .send_to_role(kWorkshopProvider, zengine::workshop::v1::PaneLaunchRequested{office, pane});
    }

auto DesktopSeat::launch_v2(loom::Mail& mail, const std::string& office, const std::string& pane, bool keys) -> void {
        (void)mail.as_role(kDesktopRole)
            .send_to_role(kWorkshopProvider, zengine::workshop::v2::PaneLaunchRequested{office, pane, keys});
    }

auto DesktopSeat::close_v1(loom::Mail& mail, const std::string& office, const std::string& pane) -> void {
        (void)mail.as_role(kDesktopRole)
            .send_to_role(kLayoutsRole, zengine::workshop::v1::PaneCloseRequested{office, pane});
    }

auto DesktopSeat::toggle_v1(loom::Mail& mail, const std::string& office, const std::string& pane) -> void {
        (void)mail.as_role(kDesktopRole)
            .send_to_role(kWorkshopProvider, zengine::workshop::v1::PaneToggleRequested{office, pane});
    }

auto DesktopSeat::deselect_answering(loom::Mail& mail, std::uint64_t answering) -> void {
        (void)mail.as_role(kDesktopRole)
            .send_to_role(kWorkshopProvider, DeselectRequested{}, answering);
    }

auto DesktopSeat::asked() const -> const std::vector<std::string>& { return asked_; }

auto DesktopSeat::refusals() const -> const std::vector<std::string>& { return refusals_; }

auto DesktopSeat::verdicts() const -> const std::vector<Verdict>& { return verdicts_; }

auto DesktopSeat::withdrawals() const -> const std::vector<ActionsWithdrawn>& { return withdrawals_; }

auto DesktopSeat::launched() const -> const std::vector<PaneLaunchAnswered>& { return launched_; }

auto DesktopSeat::closed() const -> const std::vector<PaneCloseAnswered>& { return closed_; }

auto DesktopSeat::launched_v1() const -> const std::vector<zengine::workshop::v1::PaneLaunchAnswered>& { return launched_v1_; }

auto DesktopSeat::closed_v2() const -> const std::vector<zengine::workshop::v2::PaneCloseAnswered>& { return closed_v2_; }

auto DesktopSeat::toggled_v1() const -> const std::vector<zengine::workshop::v1::PaneToggleAnswered>& { return toggled_v1_; }

auto DesktopSeat::last_ask() const -> std::uint64_t { return last_ask_; }

auto shipped_app_actions() -> AppActions {
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

auto keymap_text(const Session& s) -> std::string {
    std::string out;
    for (const ShownBinding& b : keymap_shown(s, std::string(), std::string()).rows) {
        out += b.group + " | " + b.gesture + " | " + b.label + " | " + b.id +
               (b.authored ? " *" : "") + (b.remappable ? "" : " (not remappable)") + "\n";
    }
    return out;
}

auto live_offer_pane(Live& t, const char* role, const char* pane,
                                    const char* name) -> std::int64_t {
    auto seat = std::make_unique<ProviderSeat>(std::string(role));
    ProviderSeat* raw = seat.get();
    loom::Grant grant;
    grant.allow_to_any(PaneOffered::zen_name, PaneOffered::zen_version);
    grant.allow_to_any(gfx::Drawn::zen_name, gfx::Drawn::zen_version);
    grant.allow_to_any(PaneActions::zen_name, PaneActions::zen_version);
    grant.allow_to_any(zengine::pane::v2::PaneActions::zen_name, zengine::pane::v2::PaneActions::zen_version);
    const loom::WeaveId id =
        t.bus.register_weave(std::move(seat), std::move(grant), std::string(role));
    raw->zen_set_self(id);
    raw->next = [pane, name](ProviderSeat& p, loom::Mail& m) {
        p.offer(m, PaneOffered{pane, name, "a seated provider"});
    };
    (void)t.bus.send(id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                       loom::WeaveId{}, 0));
    t.bus.drain_until_idle();
    for (const RuntimePane& row : t.w->session().panes.runtime.entries) {
        if (row.provider == std::string(role) && row.pane == std::string(pane)) {
            return row.kind;
        }
    }
    return kNoPaneKind;
}

auto seat_pane_open(PaneRig& r, ProviderSeat* seat, const char* role,
                                   const char* pane) -> std::int64_t {
    r.drive(seat, [pane](ProviderSeat& s, loom::Mail& m) {
        s.offer(m, PaneOffered{pane, "Seat", "a recording provider"});
    });
    r.pick(PaneRef{role, pane});
    for (const RuntimePane& row : r.session().panes.runtime.entries) {
        if (row.provider == std::string(role) && row.pane == std::string(pane)) {
            return row.kind;
        }
    }
    return kNoPaneKind;
}

auto press_body(PaneRig& r, std::int64_t kind) -> void {
    const CellRect body = external_body_rect(r.session(), kind);
    r.press_cell(body.x + 1, body.y + kExternalHeaderRows);
}

auto click_body(PaneRig& r, std::int64_t kind) -> void {
    const CellRect body = external_body_rect(r.session(), kind);
    r.press_cell(body.x + 1, body.y + kExternalHeaderRows);
    r.release_cell(body.x + 1, body.y + kExternalHeaderRows);
}

auto press_outside(PaneRig& r, std::int64_t kind) -> void {
    const CellRect pane_rect = cells_covered(external_pane_rect(r.session(), kind));
    r.press_cell(pane_rect.x + 1, pane_rect.y + pane_rect.h + 1);
}

auto participation(const char* stem, const char* mode, const char* role) -> ws::ArtifactParticipation {
    ws::ArtifactParticipation a;
    a.artifact = stem;
    a.authored_provider = mode;
    a.authored_role = role;
    a.state = ws::kResolvedToken;
    return a;
}

auto resolved_provider(ws::ArtifactParticipation a, const char* identity,
                                                   std::int64_t powers) -> ws::ArtifactParticipation {
    a.provider = identity;
    a.powers = powers;
    return a;
}

auto resolved_weave(ws::ArtifactParticipation a, std::int64_t id,
                                                const char* offer) -> ws::ArtifactParticipation {
    a.weave = id;
    a.offer = offer;
    return a;
}

auto shaped_arrangement() -> ws::ResolvedArrangement {
    ws::ResolvedArrangement said;
    said.plan = "default-load-plan.json";
    said.artifacts.push_back(resolved_provider(
        participation("zengine-operators-basic", "normal", ""), "zengine.operators.basic", 2));
    said.artifacts.push_back(resolved_weave(
        participation("zengine-window-tui", "", "zengine.window.tui"), 4,
        "not-a-consumer"));
    said.artifacts.push_back(resolved_weave(
        resolved_provider(participation("zengine-timer", "normal", "zengine.timer"),
                          "zengine.timer", 1),
        7, "offered"));
    said.artifacts.push_back(resolved_weave(
        participation("zengine-composer", "", "zengine.composer"), 9, "not-a-consumer"));
    return said;
}

auto row_from(const char* identity, const char* kind, const char* provider,
                             bool composite, const char* yields) -> ws::PowerRow {
    ws::PowerRow row;
    row.identity = identity;
    row.kind = kind;
    row.provider = provider;
    row.construction = composite ? ws::kCompositeConstruction : ws::kNativeConstruction;
    row.outputs.name = yields;
    row.outputs.version = 1;
    row.signature = std::string("() -> value: ") + yields;
    return row;
}

auto source_from(const char* identity, const char* provider,
                                bool composite, const char* yields) -> ws::PowerRow {
    return row_from(identity, ws::kSourceKind, provider, composite, yields);
}

auto operator_from(const char* identity, const char* provider,
                                  bool composite, const char* yields) -> ws::PowerRow {
    return row_from(identity, ws::kOperatorKind, provider, composite, yields);
}

auto found_of(std::vector<ws::PowerRow> rows, std::int64_t providers) -> ws::PowersFound {
    ws::PowersFound said;
    said.ok = true;
    said.total = static_cast<std::int64_t>(rows.size());
    said.powers = static_cast<std::int64_t>(rows.size());
    said.providers = providers;
    said.rows = std::move(rows);
    return said;
}

auto four_cells() -> ws::PowersFound {
    return found_of({operator_from("logic.select_int", "zengine.operators.basic"),
                     source_from("math.max", "zengine.workshop.host", false, "zengine.MaxSoFar"),
                     operator_from("source.looks.like.one", "zengine.timer", /*composite=*/true),
                     source_from("zengine.recipes.catalog", "zengine.workshop.host",
                                 /*composite=*/true, "zengine.RecipeCatalog")},
                    3);
}

auto many_sources(std::size_t n) -> ws::PowersFound {
    std::vector<ws::PowerRow> rows;
    for (std::size_t i = 0; i < n; ++i) {
        const std::string id = "src." + std::string(i < 10 ? "0" : "") + std::to_string(i);
        rows.push_back(source_from(id.c_str(), "zengine.fixture"));
    }
    return found_of(std::move(rows), 1);
}

auto described_as(const ws::PowerRow& row,
                                       std::vector<ws::PowerLayer> stack) -> ws::PowerDescribed {
    ws::PowerDescribed said;
    said.ok = true;
    said.identity = row.identity;
    said.row = row;
    said.stack = std::move(stack);
    return said;
}

auto layer_from(const char* provider, bool composite,
                                 const char* about) -> ws::PowerLayer {
    ws::PowerLayer layer;
    layer.provider = provider;
    layer.construction = composite ? ws::kCompositeConstruction : ws::kNativeConstruction;
    layer.about = about;
    return layer;
}

auto showing(ws::PowersFound said) -> intro::PowersUi {
    intro::PowersUi ui;
    ui.reading = std::move(said);
    ui.read = true;
    return ui;
}

auto row_with_text(const std::vector<std::string>& rows, const std::string& needle) -> std::int64_t {
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].find(needle) != std::string::npos) {
            return static_cast<std::int64_t>(i);
        }
    }
    return -1;
}

auto pane_plan() -> load::LoadPlan {
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
    tool.weave = load::WeaveIntent{kIntroRole};
    plan.artifacts.push_back(tool);
    return plan;
}

auto open_intro_pane(PaneRig& r, const char* pane) -> std::int64_t {
    r.mount_workshop();
    const load::Executed done = r.run_plan(pane_plan());
    REQUIRE_MESSAGE(done.ok, done.refusal);
    r.mount_arrangement("default-load-plan.json");
    (void)r.mount_powers();
    r.ready();
    r.extent(160, 48);
    REQUIRE(intro_row(r, pane) != nullptr);
    r.pick(PaneRef{kIntroRole, pane});
    return intro_row(r, pane)->kind;
}

auto open_powers(PaneRig& r) -> std::int64_t {
    r.expose_host_sources();
    r.mount_workshop();
    const load::Executed done = r.run_plan(pane_plan());
    REQUIRE_MESSAGE(done.ok, done.refusal);
    r.mount_arrangement("default-load-plan.json");
    (void)r.mount_powers();
    (void)r.mount_sampler();
    r.ready();
    r.extent(160, 48);
    REQUIRE(intro_row(r, intro::kPowersPane) != nullptr);
    r.pick(PaneRef{kIntroRole, intro::kPowersPane});
    return intro_row(r, intro::kPowersPane)->kind;
}

auto pane_rows(PaneRig& r, std::int64_t kind) -> std::vector<std::string> {
    const ExternalPane* pane = r.session().panes.external_pane(kind);
    // A PANE WINDOW OF THE CASE'S OWN (`pane_window.hpp`, in `zengine.window.panes` or `.other`)
    // takes a pane's drawing from the rig's window once it tells the pane a room, so the rig's
    // shows that pane no longer: its rows are read as Workshop holds them.
    const bool drawn_elsewhere =
        r.bus.role_holder(std::string(gfx::kWindowRolePrefix) + "panes").valid() ||
        r.bus.role_holder(std::string(gfx::kWindowRolePrefix) + "other").valid();
    // ...AND A MENU OPEN STANDS OVER THE PANE IT NAMES, so the window shows its lines there: what
    // the pane says under it is read as Workshop holds it.
    const bool covered = r.session().context.open || r.session().presented.open;
    if (r.own_medium() || drawn_elsewhere || covered) {
        REQUIRE(pane != nullptr);
        return held_row_texts(*pane);
    }
    const DeskShown shown = r.shown();
    std::vector<std::string> out = pane != nullptr && shows_canvas(*pane)
                                       ? canvas_rows_shown(r.session(), shown, kind)
                                       : external_rows(shown, external_body_rect(r.session(), kind));
    if (pane == nullptr || shown.window != gfx::kTuiWindow) {
        return out;
    }
    const RowsCaret caret = held_caret(*pane);
    const std::vector<std::string> held = held_row_texts(*pane);
    if (caret.row < 0 || caret.column < 0 || static_cast<std::size_t>(caret.row) >= out.size()) {
        return out;
    }
    std::string& row = out[static_cast<std::size_t>(caret.row)];
    const auto column = static_cast<std::size_t>(caret.column);
    if (column >= row.size() || row[column] != '|') {
        return out;
    }
    const std::string under = static_cast<std::size_t>(caret.row) < held.size()
                                   ? held[static_cast<std::size_t>(caret.row)]
                                   : std::string();
    if (column < under.size()) {
        row[column] = under[column];
    } else {
        row.erase(column, 1);
        row.erase(row.find_last_not_of(' ') + 1);
    }
    return out;
}

auto band_lines(PaneRig& r) -> std::vector<std::string> {
    std::vector<std::string> out;
    const Screen sc = screen_of(r.session());
    const std::int64_t help_y = gfx::cell_of_pixel(sc.band_y);
    for (const std::int64_t y : {help_y, help_y + 1}) {
        const std::string row = inspector_row(r.shown(), 0, y);
        if (!row.empty()) {
            out.push_back(row);
        }
    }
    return out;
}

auto pane_cell_of(const Session&, std::int64_t, std::int64_t, std::int64_t column) -> std::int64_t {
    return column;
}

auto press_pane(PaneRig& r, std::int64_t kind, std::int64_t row, std::int64_t column) -> void {
    if (const ExternalBodyPlace place = external_body_of(r.session(), kind); place.fit.graphical()) {
        const RegionFit& fit = place.fit;
        r.press_pixel(fit.view.x + fit.origin_x + column * fit.advance_px + fit.advance_px / 2,
                      fit.view.y + fit.origin_y + (place.header_rows + row) * fit.line_px +
                          fit.line_px / 2);
        return;
    }
    const CellRect body = external_body_rect(r.session(), kind);
    r.press_cell(body.x + pane_cell_of(r.session(), kind, row, column),
                 body.y + kExternalHeaderRows + row);
}

auto part_drawn_at(const std::map<std::string, RowPart>& parts, std::int64_t row,
                                 std::int64_t column) -> std::string {
    std::string found;
    std::int64_t width = 0;
    for (const auto& [name, part] : parts) {
        if (part.row == row && column >= part.column && column < part.column + part.columns &&
            (found.empty() || part.columns < width)) {
            found = name;
            width = part.columns;
        }
    }
    return found;
}

auto part_drawn_at(std::int64_t kind, std::int64_t row, std::int64_t column) -> std::string {
    return part_drawn_at(held_reading(kind).parts, row, column);
}

DrawnNumbers::DrawnNumbers(loom::Switchboard& b) : bus(b) {
        id = bus.add_observer([this](const loom::BusEvent& ev) {
            if (ev.kind == loom::EventKind::Delivered && ev.payload != nullptr &&
                ev.schema_name == gfx::Drawn::zen_name) {
                const gfx::Drawn d = loom::from_value<gfx::Drawn>(*ev.payload);
                number[d.area] = d.number;
            }
        });
    }

DrawnNumbers::~DrawnNumbers() { bus.remove_observer(id); }

auto DrawnNumbers::of(const Session& s, std::int64_t kind) const -> std::int64_t {
        const RuntimePane* pane = s.panes.runtime.of_kind(kind);
        if (pane == nullptr) return 0;
        const auto at = number.find(pane_window_area(s.panes, *pane));
        return at == number.end() ? 0 : at->second;
    }

auto press_as_drawn(PaneRig& r, std::int64_t kind, std::int64_t row, std::int64_t column,
                           const std::string& part, std::int64_t drawing,
                           std::int64_t button) -> void {
    const RuntimePane* pane = r.session().panes.runtime.of_kind(kind);
    REQUIRE(pane != nullptr);
    const std::string area = pane_window_area(r.session().panes, *pane);
    const CellRect body = external_body_rect(r.session(), kind);
    gfx::AreaButtonChanged b{area, button, true, column * gfx::kCellPx + gfx::kCellPx / 2,
                             row * gfx::kCellPx + gfx::kCellPx / 2, 0, true, 0, area, part, drawing};
    // ONE NUMBER FOR THE PRESS AND ITS RELEASE, past any the window mints, told the owner and, as
    // the area's arranger, Workshop.
    static std::uint64_t presses = std::uint64_t{1} << 48;
    const std::uint64_t press = ++presses;
    for (const bool pressed : {true, false}) {
        b.pressed = pressed;
        REQUIRE(r.bus.role_send_to_role_as(r.windows.id(), r.windows.role(), pane->provider,
                                             loom::Message(loom::to_value(b), {}, {}, press))
                    .valid());
        const gfx::AreaPressed told{std::string(gfx::kTuiWindow), area, button, pressed,
                                    body.x * gfx::kCellPx + b.x,
                                    (body.y + kExternalHeaderRows) * gfx::kCellPx + b.y, true, 0,
                                    area, part, drawing};
        REQUIRE(r.bus.role_send_to_role_as(r.windows.id(), r.windows.role(), kWorkshopProvider,
                                             loom::Message(loom::to_value(told), {}, {}, press))
                    .valid());
    }
    r.bus.drain_until_idle();
}

auto focus_pane(PaneRig& r, std::int64_t kind) -> void {
    const std::vector<std::string> shown = pane_rows(r, kind);
    REQUIRE_FALSE(shown.empty());
    std::int64_t row = static_cast<std::int64_t>(shown.size()) - 1;
    const std::int64_t bound = row_with_text(shown, intro::kHostResolution);
    if (bound >= 0) {
        row = bound;
    }
    press_pane(r, kind, row, 0);
}

auto chrome_column(PaneRig& r, std::int64_t kind, const std::string& word) -> std::int64_t {
    const std::vector<std::string> shown = pane_rows(r, kind);
    if (shown.empty()) {
        return -1;
    }
    const std::size_t at = shown[0].find(word);
    return at == std::string::npos ? -1 : static_cast<std::int64_t>(at);
}

auto make_taller(PaneRig& r, const char* pane, std::int64_t cells) -> void {
    const Written wrote =
        staged_author_pane_size(r, PaneRef{kIntroRole, pane}, PaneSize{},
                         PaneSize{pane_unit::kPixels, cells_px(cells)});
    REQUIRE_MESSAGE(wrote.accepted, wrote.refusal);
    // A REPAINT, WHICH IS A ROOM GRANT, WHICH IS THIS TOOL'S ONE BEAT.
    r.extent(200, 60);
}

auto file_source(const char* path) -> std::string {
    std::ifstream in(path);
    REQUIRE_MESSAGE(in.good(), "cannot read ", path);
    std::ostringstream all;
    all << in.rdbuf();
    return all.str();
}

auto presentation_sources() -> std::vector<std::string> {
    // The host's side of the seam, by name -- the host, the
    // observation door, the rules and doors it wires (`staging`, `provenance`, `pane_doors` and
    // their shapes) -- and `pane_migration.hpp`, for another reason: it is the one file that may
    // spell the reference saved files wrote for a retired browser, and the tripwire forbidding
    // every other file that spelling would otherwise forbid it too.
    static constexpr const char* kHostSide[] = {"workshop.cpp",
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

auto keymap_file_text(const std::string& legend,
                                    const std::vector<std::pair<std::string, std::string>>& rows) -> std::string {
    keymap_persist::WorkshopKeymap f;
    f.format = keymap_persist::kFormat;
    f.format_version = keymap_persist::kFormatVersion;
    f.legend = legend;
    for (const std::pair<std::string, std::string>& r : rows) {
        f.overrides.push_back(keymap_persist::WorkshopKeymapRow{r.first, r.second});
    }
    return loom::compat::serialize(loom::to_value(f));
}

auto write_keymap_file(const std::string& path, const std::string& text) -> void {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
}

Keyed::Keyed(const std::string& path) {
        host.keymap_path = path;
        ready();
    }
