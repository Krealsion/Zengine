// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// A stranger's pane that draws in a window, from the installed package alone: the pane is built on
// the installed rows facet (`workshop/pane_facets.hpp`), so its rows go to the graphics window that
// shows its area as a drawing (`rows_drawing`), and a press there is read against the rows last
// drawn. A stand-in window (a `zengine.window.<name>` office) gives the area its room, takes the
// drawing and presses a row; a forger gives a room under its own office. A failed check returns
// non-zero, which run.cmake's exit test catches.

#include "workshop/pane_facets.hpp"
#include "workshop/pane_rows.hpp"

#include "graphics/implied.hpp"
#include "graphics/ops.hpp"
#include "graphics/role.hpp"
#include "graphics/vocabulary.hpp"
#include "manual/manual.hpp"

#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace ws = zengine::workshop;
namespace gfx = zengine::graphics;

namespace {

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("pane-window consumer: FAILED -- %s\n", what);
        ++failures;
    }
}

constexpr const char* kOffice = "stranger.tally";
constexpr const char* kPane = "tally";
constexpr const char* kWindowName = "w1";
constexpr const char* kWindowOffice = "zengine.window.w1";
constexpr const char* kForger = "stranger.forger";
/// The pane's area in a window, `<office>.<pane>`, as every pane's is.
const std::string kArea = ws::pane_area(kOffice, kPane);

/// THE PANE'S MANUAL: a page with no section, which the facet answers a document request from.
constexpr zengine::manual::Manual kManual{kOffice, "Tally", "stranger.tally", nullptr, 0};

/// WHAT THIS PROGRAM ASKS A PARTICIPANT TO DO NEXT: a room for the area, or a press at a point.
struct Poke {
    std::int64_t x = 0, y = 0;
    bool press = false;
    ZEN_SHAPE(Poke, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(press));
};

struct Nothing {
    ZEN_SHAPE(Nothing, 1);
};

// ---- the pane a stranger writes ----------------------------------------------------------------

using Rows = ws::PaneRowsFacet<kPane, kOffice, kManual, std::string>;

class Tally : public zengine::With<Rows>::Weave<Tally, Nothing, loom::Accept<>, loom::Emit<>> {
public:
    ws::v2::PaneOffered offering() const { return ws::v2::PaneOffered{kPane, "Tally", "a count"}; }
    /// THE ROOM, said by the window that shows the area: the rows drawn there.
    void on_room(loom::Mail& mail) { say(mail); }
    /// A first-button press on a row: the count row adds one.
    void press(const ws::RowPress& at, loom::Mail& mail) {
        const std::string* meant = room().map().at(at.row, at.column);
        if (meant != nullptr && *meant == "count") {
            ++count;
            say(mail);
        }
    }
    void say(loom::Mail& mail) {
        room().begin();
        (void)room().push(ws::TextRow{"Tally: " + std::to_string(count), gfx::role::kAccent},
                          std::string("count"));
        (void)room().push(ws::TextRow{"press the count to add one", gfx::role::kMuted});
        room().publish(
            mail, [](const std::string& m) { return "row:" + m; }, [](const std::string&) { return true; });
    }
    Rows& room() { return this->template facet<Rows>(); }
    int count = 0;
};

// ---- the stand-in window -----------------------------------------------------------------------

class Window : public loom::WeaveBase<Window, Nothing, loom::Accept<gfx::Drawn, Poke>,
                                      loom::Emit<gfx::AreaRoom, gfx::AreaButtonChanged>> {
public:
    /// WHAT THE PANE DREW in its area: the words of every text node put, the parts it names.
    void on(const gfx::Drawn& d, loom::Mail& mail) {
        if (mail.authored_role() != kOffice || d.area != kArea) {
            return;
        }
        ++drawings;
        whole = d.whole;
        for (const gfx::Node& n : d.put) {
            if (n.op == gfx::op::kText) {
                if (const auto a = gfx::from_body<gfx::TextArgs>(n.args)) {
                    std::string text = a->text; // a run a part holds is drawn across its columns
                    while (!text.empty() && text.back() == ' ') text.pop_back();
                    if (!text.empty()) words.push_back(text);
                }
            }
        }
        for (const gfx::ElementRecord& r : d.said) {
            parts.push_back(r.action);
        }
    }
    /// A ROOM for the area, as this window's office, or a press and its release at a point.
    void on(const Poke& p, loom::Mail& mail) {
        if (!p.press) {
            gfx::AreaRoom r;
            r.area = kArea;
            r.window = kWindowName;
            r.epoch = 1;
            r.w = 400;
            r.h = 160;
            r.cell_px = 0;
            r.text_advance_px = 8;
            r.text_line_px = 16;
            (void)mail.as_role(kWindowOffice).send_to_role(kOffice, r);
            return;
        }
        (void)mail.as_role(kWindowOffice)
            .send_to_role(kOffice, gfx::AreaButtonChanged{kArea, 1, true, p.x, p.y, 0, true, 0});
        (void)mail.as_role(kWindowOffice)
            .send_to_role(kOffice, gfx::AreaButtonChanged{kArea, 1, false, p.x, p.y, 0, true, 0});
    }
    int drawings = 0;
    bool whole = false;
    std::vector<std::string> words;
    std::vector<std::string> parts;
};

// ---- a forger: a room given under another office ---------------------------------------------

class Forger : public loom::WeaveBase<Forger, Nothing, loom::Accept<Poke>, loom::Emit<gfx::AreaRoom>> {
public:
    void on(const Poke&, loom::Mail& mail) {
        gfx::AreaRoom r;
        r.area = kArea;
        r.window = kWindowName;
        r.epoch = 2;
        r.w = 800;
        r.h = 320;
        (void)mail.as_role(kForger).send_to_role(kOffice, r);
    }
};

template <class T>
T* seat(loom::Switchboard& bus, loom::WeaveId& id, loom::Grant grant, const char* office) {
    auto owned = std::make_unique<T>();
    T* raw = owned.get();
    id = bus.register_weave(std::move(owned), std::move(grant), std::string(office));
    raw->zen_set_self(id);
    return raw;
}

void poke(loom::Switchboard& bus, loom::WeaveId id, Poke p) {
    (void)bus.send(id, loom::Message(loom::to_value(p), loom::WeaveId{}, loom::WeaveId{}, 0));
    bus.drain_until_idle();
}

} // namespace

int main() {
    loom::Switchboard bus;
    loom::WeaveId window_id{};
    loom::WeaveId pane_id{};
    loom::WeaveId forger_id{};
    loom::Grant window_speaks;
    window_speaks.allow_to_role(gfx::AreaRoom::zen_name, gfx::AreaRoom::zen_version, kOffice);
    window_speaks.allow_to_role(gfx::AreaButtonChanged::zen_name, gfx::AreaButtonChanged::zen_version,
                                kOffice);
    Window* window = seat<Window>(bus, window_id, window_speaks, kWindowOffice);
    loom::Grant pane_speaks;
    pane_speaks.allow_to_role(gfx::Drawn::zen_name, gfx::Drawn::zen_version, kWindowOffice);
    pane_speaks.allow_to_role(gfx::RoomAsked::zen_name, gfx::RoomAsked::zen_version,
                              gfx::kGraphicsRole); // asked once a life, of no directory here
    Tally* pane = seat<Tally>(bus, pane_id, pane_speaks, kOffice);
    loom::Grant forger_speaks;
    forger_speaks.allow_to_role(gfx::AreaRoom::zen_name, gfx::AreaRoom::zen_version, kOffice);
    (void)seat<Forger>(bus, forger_id, forger_speaks, kForger);

    poke(bus, forger_id, Poke{});
    check(window->drawings == 0 && pane->room().rows() == 0,
          "a room another office gives is not the window's: nothing is drawn");

    poke(bus, window_id, Poke{});
    check(pane->room().rows() == 10 && pane->room().columns() == 50,
          "the window's room is the pane's rows and columns, a line and an advance of its text each");
    check(window->drawings == 1 && window->whole, "the pane's rows are drawn in its area, whole");
    const bool counted = window->words.size() == 2 && window->words[0] == "Tally: 0" &&
                         window->words[1] == "press the count to add one";
    check(counted, "each row is a line of text in the drawing");
    if (!counted) {
        for (const std::string& w : window->words) std::printf("  drawn: '%s'\n", w.c_str());
    }
    check(window->parts.size() == 1 && window->parts[0] == "row:count",
          "the row a meaning stands on is a part the drawing names");

    window->words.clear();
    window->parts.clear();
    poke(bus, window_id, Poke{3 * 8, 0, true});
    check(pane->count == 1, "a press on the count row in the window is the pane's own press");
    check(window->drawings == 2 && !window->whole && !window->words.empty() &&
              window->words[0] == "Tally: 1",
          "and the change is drawn there, as what changed");
    poke(bus, window_id, Poke{3 * 8, 16, true});
    check(pane->count == 1, "a press on a row that means nothing does nothing");

    if (failures == 0) {
        std::printf("pane-window consumer: ok -- a stranger's pane drew its rows in the window that "
                    "shows its area, named its part, heard a press there, and passed over a forged "
                    "room\n");
    }
    return failures == 0 ? 0 : 1;
}
