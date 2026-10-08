// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// A live external consumer of a pane's settings, from the installed package alone: a pane a
// stranger writes declares the setting it takes and reads what it is handed; a stand-in host (the
// `zengine.workshop` office) judges the declaration and hands the pane its settings; a forger
// hands it settings under its own office. A failed check returns non-zero, which run.cmake's exit
// test catches.

#include "workshop/pane_settings.hpp"

#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace ws = zengine::workshop;

namespace {

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("pane-settings consumer: FAILED -- %s\n", what);
        ++failures;
    }
}

constexpr const char* kWorkshop = "zengine.workshop";
constexpr const char* kDial = "stranger.dial";
constexpr const char* kForger = "stranger.forger";
constexpr const char* kPane = "dial";

/// WHAT THIS PROGRAM ASKS A PARTICIPANT TO SAY NEXT: a hand-off of `settings` to the dial, or, to
/// the dial, its declaration.
struct Poke {
    std::vector<ws::PaneSetting> settings;
    ZEN_SHAPE(Poke, 1, ZEN_FIELD(settings));
};

struct Nothing {
    ZEN_SHAPE(Nothing, 1);
};

/// THE ONE SETTING THE DIAL TAKES: its legend, a flag that is on unless a layout says otherwise.
ws::PaneSettingRow legend_row() {
    ws::PaneSettingRow row;
    row.key = "legend";
    row.flag = true;
    return row;
}

// ---- the pane a stranger writes ----------------------------------------------------------------

class Dial : public loom::WeaveBase<Dial, Nothing, loom::Accept<ws::PaneSettings, Poke>,
                                    loom::Emit<ws::PaneSettingsDeclared>> {
public:
    /// ITS DECLARATION, judged by the installed judge before it is sent.
    void on(const Poke&, loom::Mail& mail) {
        const ws::PaneSettingsDeclared declared{kPane, {legend_row()}};
        sound = ws::pane_settings_declared_problem(declared).empty();
        (void)mail.as_role(kDial).send_to_role(kWorkshop, declared);
    }
    /// WHAT IT IS HANDED, from Workshop's office alone, taken whole: its own key read, a value it
    /// cannot use said in the installed words, its default kept.
    void on(const ws::PaneSettings& handed, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshop) || handed.pane != kPane) {
            ++ignored;
            return;
        }
        legend = true;
        problem.clear();
        if (const ws::PaneSetting* s = ws::find_pane_setting(handed.settings, "legend")) {
            problem = ws::pane_setting_refused_by(legend_row(), *s);
            if (problem.empty()) {
                legend = *s->flag;
            }
        }
        ++heard;
    }
    bool sound = false;
    bool legend = true;
    std::string problem;
    int heard = 0;
    int ignored = 0;
};

// ---- the stand-in host -----------------------------------------------------------------------

class Host : public loom::WeaveBase<Host, Nothing, loom::Accept<ws::PaneSettingsDeclared, Poke>,
                                    loom::Emit<ws::PaneSettings>> {
public:
    void on(const ws::PaneSettingsDeclared& d, loom::Mail& mail) {
        if (mail.authored_role() == kDial && ws::pane_settings_declared_problem(d).empty()) {
            declared = d.rows;
        }
    }
    void on(const Poke& p, loom::Mail& mail) {
        (void)mail.as_role(kWorkshop).send_to_role(kDial, ws::PaneSettings{kPane, p.settings});
    }
    std::vector<ws::PaneSettingRow> declared;
};

// ---- a forger: settings handed under another office ------------------------------------------

class Forger : public loom::WeaveBase<Forger, Nothing, loom::Accept<Poke>,
                                      loom::Emit<ws::PaneSettings>> {
public:
    void on(const Poke& p, loom::Mail& mail) {
        (void)mail.as_role(kForger).send_to_role(kDial, ws::PaneSettings{kPane, p.settings});
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
    loom::WeaveId host_id{};
    loom::WeaveId dial_id{};
    loom::WeaveId forger_id{};
    loom::Grant host_speaks;
    host_speaks.allow_to_role(ws::PaneSettings::zen_name, ws::PaneSettings::zen_version, kDial);
    Host* host = seat<Host>(bus, host_id, host_speaks, kWorkshop);
    loom::Grant dial_speaks;
    dial_speaks.allow_to_role(ws::PaneSettingsDeclared::zen_name,
                              ws::PaneSettingsDeclared::zen_version, kWorkshop);
    Dial* dial = seat<Dial>(bus, dial_id, dial_speaks, kDial);
    loom::Grant forger_speaks;
    forger_speaks.allow_to_role(ws::PaneSettings::zen_name, ws::PaneSettings::zen_version, kDial);
    (void)seat<Forger>(bus, forger_id, forger_speaks, kForger);

    poke(bus, dial_id, Poke{});
    check(dial->sound, "the pane's declaration is sound by the installed judge");
    check(host->declared.size() == 1 && host->declared.front().key == "legend",
          "the host took the declaration the pane sent");

    poke(bus, host_id, Poke{{ws::PaneSetting{"legend", false, {}, {}}}});
    check(dial->heard == 1 && !dial->legend && dial->problem.empty(),
          "a flag the pane takes is the pane's to use");
    poke(bus, host_id, Poke{{}});
    check(dial->heard == 2 && dial->legend, "an empty hand-off is every setting at its default");
    poke(bus, host_id, Poke{{ws::PaneSetting{"legend", {}, {}, std::string("hidden")}}});
    check(dial->legend && dial->problem == "legend takes on or off, not a text `hidden`",
          "a value the pane cannot use leaves its default, said in the installed words");

    poke(bus, forger_id, Poke{{ws::PaneSetting{"legend", false, {}, {}}}});
    check(dial->ignored == 1 && dial->legend, "settings another office hands are not Workshop's");

    if (failures == 0) {
        std::printf("pane-settings consumer: ok -- a declaration judged and taken, settings handed "
                    "and read whole, an unusable value said, and a forger's hand-off passed over\n");
    }
    return failures == 0 ? 0 : 1;
}
