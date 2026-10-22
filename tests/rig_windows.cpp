// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The windows a Workshop rig shows its desk in (rig_windows.hpp), compiled once: the two windows'
// templates over the suites' devices are instantiated here and nowhere else a Workshop suite
// compiles.

#include "rig_windows.hpp"

#include "doctest.h"
#include "lifecycle_door.hpp"
#include "window_backends.hpp"

#include "graphics/vocabulary.hpp"

#include <zen/kernel/manager.hpp>
#include <zen/serialize.hpp>

#include <algorithm>
#include <utility>

namespace zengine::testing::rig_windows {

namespace gfx = zengine::graphics;
namespace wb = zengine::testing::window_backends;

namespace {

struct RigWindowEarsState {
    std::int64_t heard = 0;
    ZEN_SHAPE(RigWindowEarsState, 1, ZEN_FIELD(heard));
};

/// The rig's own ears on the windows: the loader of the directory, which hears the manager's
/// answer, and the asker of what a window shows.
class Ears : public loom::WeaveBase<Ears, RigWindowEarsState,
                                    loom::Accept<loom::Result, loom::Refused, gfx::WindowShown>,
                                    loom::Emit<loom::LoadWeave, gfx::WindowShowAsked,
                                               gfx::PumpSurface>> {
public:
    void on(const loom::Result& r, loom::Mail&) {
        ++state_.heard;
        loaded.push_back(r.value);
    }
    void on(const loom::Refused& r, loom::Mail&) {
        ++state_.heard;
        refused.push_back(r.reason);
    }
    void on(const gfx::WindowShown& s, loom::Mail&) {
        ++state_.heard;
        shown.push_back(s);
    }

    std::vector<std::string> loaded;
    std::vector<std::string> refused;
    std::vector<gfx::WindowShown> shown;
};

std::string office_of(const std::string& window) {
    return std::string(gfx::kWindowOfficePrefix) + window;
}

/// The role whose ink a terminal window writes as this sequence, or -1.
std::int64_t role_of_ink(const std::string& sgr) {
    for (std::int64_t r = -1; r <= 64; ++r) {
        if (sgr == gfx::window::sgr_for_role(static_cast<int>(r))) {
            return r;
        }
    }
    return -1;
}

/// The role a light colour is in either look, where exactly one role is drawn in it.
std::int64_t role_of_rgba(std::uint32_t rgba) {
    for (const auto& look : {gfx::light::night_look(), gfx::light::day_look()}) {
        std::int64_t found = -1;
        int n = 0;
        for (const auto& [role, rgb] : look.colour) {
            if (gfx::light::rgba_of(rgb) == rgba) {
                found = role;
                ++n;
            }
        }
        if (n == 1) {
            return found;
        }
    }
    return -1;
}

std::int64_t floor_div(std::int64_t v, std::int64_t d) {
    return v >= 0 ? v / d : -((-v + d - 1) / d);
}

void trim(std::string& row) { row.erase(row.find_last_not_of(' ') + 1); }

} // namespace

struct Windows::Impl {
    loom::Switchboard& bus;
    loom::WeaveId manager;
    std::string directory_so;
    Ears* ears = nullptr;
    loom::WeaveId ears_id{};
    loom::WeaveId door{};
    loom::WeaveId directory{};
    // Each device's log, kept while its window is mounted (the window holds a pointer to it).
    wb::TermLog term;
    wb::LightLog light;
    loom::WeaveId terminal_id{};
    loom::WeaveId sdl_id{};
    std::int64_t terminal_lives = 0;
    std::int64_t sdl_lives = 0;
    std::string up; ///< the window showing now
    std::uint64_t asks = 1; ///< the next ask's correlation
    std::uint64_t readings = 0;
    // THE TERMINAL'S SCREEN, as its writes left it: a glyph and the ink it was written in a cell.
    std::vector<std::string> glyphs;
    std::vector<std::vector<std::int64_t>> inks;
    bool watching = false;
    std::vector<Shown> watched;

    Impl(loom::Switchboard& b, loom::WeaveId m, std::string so)
        : bus(b), manager(m), directory_so(std::move(so)) {
        term.keeps = true;
        light.keeps = false;
        light.keeps_last = true;
        light.clipboard = std::string();
        light.holds_copies = true;
        auto made = std::make_unique<Ears>();
        ears = made.get();
        loom::Grant grant;
        grant.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
        grant.allow_to_any(gfx::WindowShowAsked::zen_name, gfx::WindowShowAsked::zen_version);
        grant.allow_to_any(gfx::PumpSurface::zen_name, gfx::PumpSurface::zen_version);
        ears_id = bus.register_weave(std::move(made), std::move(grant));
        ears->zen_set_self(ears_id);
    }

    ~Impl() {
        // The windows go before the logs their devices write to.
        for (const loom::WeaveId id : {terminal_id, sdl_id, ears_id}) {
            if (id.valid()) {
                (void)bus.unregister_weave(id);
            }
        }
    }

    void load_directory() {
        if (directory.valid()) {
            return;
        }
        const std::size_t before = ears->loaded.size();
        bus.send_as(ears_id, manager,
                    loom::Message(loom::to_value(loom::LoadWeave{"zengine-graphics", directory_so,
                                                                 gfx::kGraphicsRole}),
                                  ears_id, ears_id, 0));
        bus.drain_until_idle();
        REQUIRE_MESSAGE(ears->loaded.size() == before + 1,
                        "the rig's directory did not load: "
                            << (ears->refused.empty() ? std::string("no answer")
                                                      : ears->refused.back()));
        directory = bus.role_holder(gfx::kGraphicsRole);
        REQUIRE(directory.valid());
    }

    /// Mount a window at its office, over its device.
    template <class W, class Device>
    loom::WeaveId mount(const std::string& window, Device device) {
        auto made = std::make_unique<W>(std::move(device));
        W* raw = made.get();
        loom::Grant grant = loom::emit_default_grant(*raw);
        const loom::WeaveId id = bus.register_weave(std::move(made), std::move(grant),
                                                    office_of(window));
        REQUIRE_MESSAGE(id.valid(), "the rig's window could not hold " << office_of(window));
        raw->zen_set_self(id);
        return id;
    }

    /// Started, as Loom's control door starts a weave: it claims its device and offers itself.
    void activate(loom::WeaveId id, std::int64_t& lives) {
        if (!door.valid()) {
            door = zengine::testing::mount_door(bus);
        }
        zengine::testing::order_activation(bus, door, id, ++lives);
        bus.drain_until_idle();
    }

    void pump(const std::string& window) {
        bus.send_as_to_role(ears_id, office_of(window),
                            loom::Message(loom::to_value(gfx::PumpSurface{}), ears_id, ears_id, 0));
        bus.drain_until_idle();
    }

    /// The terminal's screen now: each of its rows cut to its width, trailing blanks off.
    Shown terminal_screen() const {
        Shown s;
        s.window = gfx::kTerminalWindow;
        const auto rows = static_cast<std::size_t>((std::max)(term.size.rows, std::int64_t{0}));
        const auto cols = static_cast<std::size_t>((std::max)(term.size.cols, std::int64_t{0}));
        s.rows.resize(rows);
        for (std::size_t r = 0; r < rows && r < glyphs.size(); ++r) {
            s.rows[r] = glyphs[r].substr(0, cols);
            trim(s.rows[r]);
        }
        return s;
    }

    /// WHAT THE TERMINAL WAS WRITTEN SINCE, laid onto its screen: a cursor move (`ESC[row;colH`)
    /// and the glyphs after it, each in the ink last set (`ESC[...m`); the copy sequence (OSC) is
    /// passed over. A showing is one write, kept whole while a watch stands.
    void fold() {
        for (const std::string& bytes : term.writes) {
            std::int64_t row = -1, col = -1, ink = -1;
            bool drew = false;
            for (std::size_t k = 0; k < bytes.size(); ++k) {
                if (bytes[k] == '\x1b' && k + 1 < bytes.size() && bytes[k + 1] == '[') {
                    // A control sequence: its parameters, then the one byte that ends it.
                    std::size_t j = k + 2;
                    while (j < bytes.size() && bytes[j] >= 0x30 && bytes[j] <= 0x3f) {
                        ++j;
                    }
                    if (j < bytes.size() && bytes[j] == 'H') {
                        const std::string args = bytes.substr(k + 2, j - k - 2);
                        const std::size_t semi = args.find(';');
                        if (semi != std::string::npos) {
                            row = std::stoll(args.substr(0, semi)) - 1;
                            col = std::stoll(args.substr(semi + 1)) - 1;
                        }
                    } else if (j < bytes.size() && bytes[j] == 'm') {
                        ink = role_of_ink(bytes.substr(k, j - k + 1));
                    }
                    k = j;
                    continue;
                }
                if (bytes[k] == '\x1b' && k + 1 < bytes.size() && bytes[k + 1] == ']') {
                    const std::size_t end = bytes.find('\x07', k);
                    k = end == std::string::npos ? bytes.size() : end;
                    continue;
                }
                if (row >= 0 && col >= 0) {
                    const auto r = static_cast<std::size_t>(row);
                    const auto c = static_cast<std::size_t>(col);
                    if (glyphs.size() <= r) {
                        glyphs.resize(r + 1);
                        inks.resize(r + 1);
                    }
                    if (glyphs[r].size() <= c) {
                        glyphs[r].resize(c + 1, ' ');
                        inks[r].resize(c + 1, -1);
                    }
                    glyphs[r][c] = bytes[k];
                    inks[r][c] = ink;
                    drew = true;
                }
                ++col;
            }
            if (watching && drew) {
                watched.push_back(terminal_screen());
            }
        }
        term.writes.clear();
        term.written.clear();
    }

    /// The OS window's last picture as text: each character of a run where it stands, inside its
    /// area, a front area over the ones behind, as the window reads itself back.
    Shown light_screen() const {
        Shown s;
        s.window = gfx::kSdlWindow;
        s.advance = light.face.advance;
        s.line = light.face.line;
        if (s.advance <= 0 || s.line <= 0) {
            return s;
        }
        const std::int64_t cols = light.size.width / s.advance;
        const std::int64_t lines = light.size.height / s.line;
        const auto width = static_cast<std::size_t>((std::max)(cols, std::int64_t{0}));
        s.rows.assign(static_cast<std::size_t>((std::max)(lines, std::int64_t{0})),
                      std::string(width, ' '));
        if (!light.frames.empty()) {
            for (const auto& a : light.frames.back().areas) {
                for (const auto& prims : a.nodes) {
                    for (const auto& t : prims.texts) {
                        const std::int64_t y = a.y + t.y;
                        const std::int64_t r = floor_div(y, s.line);
                        std::int64_t x = a.x + t.x;
                        for (const char ch : t.text) {
                            const std::int64_t c = floor_div(x, s.advance);
                            const bool inside = x >= a.x && x + s.advance <= a.x + a.w &&
                                                y >= a.y && y < a.y + a.h;
                            if (inside && c >= 0 && r >= 0 && c < cols && r < lines) {
                                s.rows[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)] =
                                    ch;
                            }
                            x += s.advance;
                        }
                    }
                }
            }
        }
        for (std::string& row : s.rows) {
            trim(row);
        }
        return s;
    }

    /// What the window answers `WindowShowAsked`, its rows.
    std::vector<std::string> answered() {
        const std::size_t before = ears->shown.size();
        bus.send_as_to_role(ears_id, office_of(up),
                            loom::Message(loom::to_value(gfx::WindowShowAsked{}), ears_id, ears_id,
                                          asks++));
        bus.drain_until_idle();
        REQUIRE_MESSAGE(ears->shown.size() == before + 1, "the window did not say what it shows");
        std::vector<std::string> rows = ears->shown.back().rows;
        ears->shown.clear();
        return rows;
    }
};

const std::string& Shown::row_at_px(std::int64_t y) const {
    static const std::string none;
    if (y < 0 || line <= 0) {
        return none;
    }
    const auto at = static_cast<std::size_t>(y / line);
    return at < rows.size() ? rows[at] : none;
}

std::string Shown::text_at_px(std::int64_t x, std::int64_t y, std::int64_t columns) const {
    const std::string& row = row_at_px(y);
    if (x < 0 || advance <= 0) {
        return std::string();
    }
    const auto from = static_cast<std::size_t>(x / advance);
    if (from >= row.size()) {
        return std::string();
    }
    std::string out = columns < 0 ? row.substr(from)
                                  : row.substr(from, static_cast<std::size_t>(columns));
    trim(out);
    return out;
}

std::string Shown::text() const {
    std::string out;
    for (const std::string& row : rows) {
        out += row;
        out += '\n';
    }
    return out;
}

Windows::Windows(loom::Switchboard& bus, loom::WeaveId manager, std::string directory)
    : impl_(std::make_unique<Impl>(bus, manager, std::move(directory))) {}

Windows::~Windows() = default;

void Windows::room(std::int64_t width_px, std::int64_t height_px, std::int64_t advance,
                   std::int64_t line) {
    Impl& m = *impl_;
    m.load_directory();
    if (advance <= 0 || line <= 0) {
        // A CELL MEDIUM: the terminal window, its room in whole cells.
        m.term.size = gfx::window::TerminalSize{width_px / gfx::kCellPx, height_px / gfx::kCellPx};
        if (!m.terminal_id.valid()) {
            m.terminal_id =
                m.mount<wb::FakeTerminalWindow>(gfx::kTerminalWindow, wb::FakeTerminal{&m.term});
            m.up = gfx::kTerminalWindow;
            m.activate(m.terminal_id, m.terminal_lives);
        } else if (m.up != gfx::kTerminalWindow) {
            m.up = gfx::kTerminalWindow;
            m.activate(m.terminal_id, m.terminal_lives);
        } else {
            m.pump(gfx::kTerminalWindow);
        }
        return;
    }
    // A FACE: the OS window, measured with it. A face it did not set text in before is taken as
    // a window started again takes it, since the window measures its face when it claims it.
    const gfx::TextMeasure face{advance, line};
    const bool new_face = !(m.light.face == face);
    m.light.size = gfx::window::PixelSize{width_px, height_px};
    m.light.face = face;
    if (!m.sdl_id.valid()) {
        m.sdl_id = m.mount<wb::FakeSdlWindow>(gfx::kSdlWindow, wb::FakeSdlBackend{&m.light});
        m.up = gfx::kSdlWindow;
        m.activate(m.sdl_id, m.sdl_lives);
    } else if (m.up != gfx::kSdlWindow || new_face) {
        m.up = gfx::kSdlWindow;
        m.activate(m.sdl_id, m.sdl_lives);
    } else {
        m.pump(gfx::kSdlWindow);
    }
}

bool Windows::up() const { return !impl_->up.empty(); }

std::string Windows::window() const { return impl_->up; }

std::string Windows::office() const { return up() ? office_of(impl_->up) : std::string(); }

loom::WeaveId Windows::id() const {
    if (impl_->up == gfx::kTerminalWindow) {
        return impl_->terminal_id;
    }
    if (impl_->up == gfx::kSdlWindow) {
        return impl_->sdl_id;
    }
    return loom::WeaveId{};
}

Shown Windows::shown() {
    Impl& m = *impl_;
    if (!up()) {
        return Shown{};
    }
    Shown out;
    if (m.up == gfx::kTerminalWindow) {
        m.fold();
        out = m.terminal_screen();
    } else {
        out = m.light_screen();
    }
    // THE SCREEN AGAINST THE WINDOW'S OWN WORD, while asking delivers nothing else.
    if (m.readings++ % 32 == 0 && m.bus.pending() == 0) {
        const std::vector<std::string> said = m.answered();
        CHECK_MESSAGE(said == out.rows,
                      "the screen read off the window's device is not what the window says it "
                      "shows:\n" << out.text());
    }
    return out;
}

void Windows::pump() {
    if (up()) {
        impl_->pump(impl_->up);
    }
}

void Windows::hand(const std::vector<zengine::input::InputEvent>& moments) {
    if (!up()) {
        return;
    }
    queue(moments);
    impl_->pump(impl_->up);
}

void Windows::queue(const std::vector<zengine::input::InputEvent>& moments) {
    Impl& m = *impl_;
    for (const zengine::input::InputEvent& e : moments) {
        if (m.up == gfx::kTerminalWindow) {
            m.term.moments.push_back(e);
        } else if (m.up == gfx::kSdlWindow) {
            m.light.queued.events.push_back(e);
        }
    }
}

std::vector<Quad> Windows::quads() {
    Impl& m = *impl_;
    const bool sdl = m.up == gfx::kSdlWindow;
    REQUIRE_MESSAGE(sdl, "quads are the OS window's");
    std::vector<Quad> out;
    if (m.light.frames.empty()) {
        return out;
    }
    for (const auto& a : m.light.frames.back().areas) {
        for (const auto& prims : a.nodes) {
            for (const auto& q : prims.quads) {
                out.push_back(Quad{a.area, a.x + q.x, a.y + q.y, q.w, q.h, role_of_rgba(q.rgba)});
            }
        }
    }
    return out;
}

std::vector<Cell> Windows::cells() {
    Impl& m = *impl_;
    const bool terminal = m.up == gfx::kTerminalWindow;
    REQUIRE_MESSAGE(terminal, "cells are the terminal window's");
    m.fold();
    std::vector<Cell> out;
    const auto rows = static_cast<std::size_t>((std::max)(m.term.size.rows, std::int64_t{0}));
    const auto cols = static_cast<std::size_t>((std::max)(m.term.size.cols, std::int64_t{0}));
    for (std::size_t r = 0; r < rows && r < m.glyphs.size(); ++r) {
        for (std::size_t c = 0; c < cols && c < m.glyphs[r].size(); ++c) {
            out.push_back(Cell{static_cast<std::int64_t>(c), static_cast<std::int64_t>(r),
                               m.glyphs[r][c], m.inks[r][c]});
        }
    }
    return out;
}

void Windows::watch() {
    Impl& m = *impl_;
    const bool terminal = m.up == gfx::kTerminalWindow;
    REQUIRE_MESSAGE(terminal, "only the terminal window's showings are kept");
    m.fold();
    m.watched.clear();
    m.watching = true;
}

std::vector<Shown> Windows::showings() {
    Impl& m = *impl_;
    m.fold();
    return m.watched;
}

void Windows::set_clipboard(std::optional<std::string> text) {
    impl_->light.clipboard = std::move(text);
}

std::optional<std::string> Windows::clipboard() const { return impl_->light.clipboard; }

const std::vector<std::string>& Windows::copies() const { return impl_->light.copies; }

int Windows::clipboard_reads() const { return impl_->light.reads; }

void Windows::set_placement(std::optional<Placement> at) {
    if (at.has_value()) {
        impl_->light.placement = gfx::window::WindowPlacementValue{at->x, at->y, at->maximized};
    } else {
        impl_->light.placement.reset();
    }
}

std::vector<Placement> Windows::placed() const {
    std::vector<Placement> out;
    for (const gfx::window::WindowPlacementValue& p : impl_->light.placed) {
        out.push_back(Placement{p.x, p.y, p.maximized});
    }
    return out;
}

loom::WeaveId Windows::directory() const { return impl_->directory; }

} // namespace zengine::testing::rig_windows
