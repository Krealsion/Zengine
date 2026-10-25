// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// A live external consumer of a pane's settings, from the installed package alone: a pane a
// stranger writes on the installed settings facet declares the setting it takes to the office
// that keeps settings (`kLayoutsRole`), asks that office for its row when it arrives, and reads
// what it is handed; a stand-in keeper judges the declaration and hands the pane its row, and a
// forger and a stand-in Workshop hand it settings under their own offices. A failed check returns
// non-zero, which run.cmake's exit test catches.

#include "activation/activation.hpp"
#include "workshop/pane_settings.hpp"
#include "workshop/pane_settings_taken.hpp"

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

using Settings = ws::PaneSettingsFacet<kPane, kDial>;

class Dial
    : public zengine::With<Settings>::Weave<Dial, Nothing, loom::Accept<Poke>,
                                            loom::Emit<ws::PaneSettingsDeclared>> {
public:
    /// ITS DECLARATION, judged by the installed judge before it is sent.
    void on(const Poke&, loom::Mail& mail) {
        const ws::PaneSettingsDeclared declared{kPane, {legend_row()}};
        sound = ws::pane_settings_declared_problem(declared).empty();
        (void)mail.as_role(kDial).send_to_role(ws::kLayoutsRole, declared);
    }
    /// What the facet takes the row against: the declared setting, as v2 says it.
    static std::vector<ws::v2::PaneSettingRow> setting_rows() {
        return {ws::widen_setting_row(legend_row())};
    }
    /// WHAT IT IS HANDED, from the layouts' office alone, taken whole by the installed facet: its
    /// own key read, a value it cannot use said back in the installed words, its default kept.
    void on_settings(const ws::SettingsTaken& taken, loom::Mail&) {
        legend = taken.flag("legend");
        problem = taken.refusal("legend");
        ++heard;
    }
    bool held() const { return this->template facet<Settings>().held(); }
    bool sound = false;
    bool legend = true;
    std::string problem;
    int heard = 0;
};

// ---- the stand-in keeper, the forger and a stand-in Workshop ----------------------------------

class Keeper : public loom::WeaveBase<Keeper, Nothing,
                                      loom::Accept<ws::PaneSettingsDeclared, ws::PaneSettingsAsked,
                                                   ws::PaneSettingUnused, Poke>,
                                      loom::Emit<ws::v2::PaneSettings>> {
public:
    void on(const ws::PaneSettingsDeclared& d, loom::Mail& mail) {
        if (mail.authored_role() == kDial && ws::pane_settings_declared_problem(d).empty()) {
            declared = d.rows;
        }
    }
    /// THE PANE'S ASK, from its office: handed what the row keeps now.
    void on(const ws::PaneSettingsAsked& a, loom::Mail& mail) {
        if (mail.authored_role() == kDial && a.pane == kPane) {
            ++asks;
            hand(mail);
        }
    }
    void on(const ws::PaneSettingUnused& u, loom::Mail& mail) {
        if (mail.authored_role() == kDial) {
            unused.push_back(u);
        }
    }
    /// ...and a row the layout keeps from now on.
    void on(const Poke& p, loom::Mail& mail) {
        kept = p.settings;
        hand(mail);
    }
    void hand(loom::Mail& mail) {
        (void)mail.as_role(ws::kLayoutsRole)
            .send_to_role(kDial, ws::v2::PaneSettings{kPane, ++revision, kept});
    }
    std::vector<ws::PaneSettingRow> declared;
    std::vector<ws::PaneSetting> kept;
    std::vector<ws::PaneSettingUnused> unused;
    std::int64_t revision = 0;
    int asks = 0;
};

class Hander : public loom::WeaveBase<Hander, Nothing, loom::Accept<Poke>,
                                      loom::Emit<ws::v2::PaneSettings>> {
public:
    explicit Hander(const char* office) : office_(office) {}
    void on(const Poke& p, loom::Mail& mail) {
        (void)mail.as_role(office_)
            .send_to_role(kDial, ws::v2::PaneSettings{kPane, 99, p.settings});
    }

private:
    const char* office_;
};

template <class T>
T* seat(loom::Switchboard& bus, loom::WeaveId& id, loom::Grant grant, const char* office,
        std::unique_ptr<T> owned) {
    T* raw = owned.get();
    id = bus.register_weave(std::move(owned), std::move(grant), std::string(office));
    raw->zen_set_self(id);
    return raw;
}

void poke(loom::Switchboard& bus, loom::WeaveId id, Poke p) {
    (void)bus.send(id, loom::Message(loom::to_value(p), loom::WeaveId{}, loom::WeaveId{}, 0));
    bus.drain_until_idle();
}

loom::Grant hands_to_dial() {
    loom::Grant g;
    g.allow_to_role(ws::v2::PaneSettings::zen_name, ws::v2::PaneSettings::zen_version, kDial);
    return g;
}

} // namespace

int main() {
    loom::Switchboard bus;
    loom::WeaveId keeper_id{};
    loom::WeaveId dial_id{};
    loom::WeaveId forger_id{};
    loom::WeaveId workshop_id{};
    Keeper* keeper =
        seat(bus, keeper_id, hands_to_dial(), ws::kLayoutsRole, std::make_unique<Keeper>());
    loom::Grant dial_speaks;
    dial_speaks.allow_to_role(ws::PaneSettingsDeclared::zen_name,
                              ws::PaneSettingsDeclared::zen_version, ws::kLayoutsRole);
    dial_speaks.allow_to_role(ws::PaneSettingsAsked::zen_name, ws::PaneSettingsAsked::zen_version,
                              ws::kLayoutsRole);
    dial_speaks.allow_to_role(ws::PaneSettingUnused::zen_name, ws::PaneSettingUnused::zen_version,
                              ws::kLayoutsRole);
    Dial* dial = seat(bus, dial_id, dial_speaks, kDial, std::make_unique<Dial>());
    (void)seat(bus, forger_id, hands_to_dial(), kForger, std::make_unique<Hander>(kForger));
    (void)seat(bus, workshop_id, hands_to_dial(), kWorkshop, std::make_unique<Hander>(kWorkshop));

    check(!dial->held(), "a pane that has heard nothing holds no row");
    poke(bus, dial_id, Poke{});
    check(dial->sound, "the pane's declaration is sound by the installed judge");
    check(keeper->declared.size() == 1 && keeper->declared.front().key == "legend",
          "the keeper took the declaration the pane sent");
    check(keeper->asks == 1, "the pane asked the layouts' office for its row once, as it arrived");
    check(dial->held() && dial->heard == 1 && dial->legend,
          "the row it asked for is held, every setting at its default");

    poke(bus, keeper_id, Poke{{ws::PaneSetting{"legend", false, {}, {}}}});
    check(dial->heard == 2 && !dial->legend && dial->problem.empty(),
          "a flag the pane takes is the pane's to use");
    poke(bus, keeper_id, Poke{{ws::PaneSetting{"legend", {}, {}, std::string("hidden")}}});
    check(dial->legend && dial->problem == "legend takes on or off, not a text `hidden`",
          "a value the pane cannot use leaves its default, said in the installed words");
    check(keeper->unused.size() == 1 && keeper->unused.front().key == "legend" &&
              keeper->unused.front().revision == keeper->revision &&
              keeper->unused.front().why == "it takes on or off",
          "what the pane cannot use is said back to the layouts' office, at the revision handed");

    poke(bus, forger_id, Poke{{ws::PaneSetting{"legend", false, {}, {}}}});
    poke(bus, workshop_id, Poke{{ws::PaneSetting{"legend", false, {}, {}}}});
    check(dial->heard == 3 && dial->legend,
          "settings another office hands, Workshop's own included, are not the layouts' office's");
    check(keeper->asks == 1, "a pane that holds its row asks for it no more");

    if (failures == 0) {
        std::printf("pane-settings consumer: ok -- a declaration judged and taken, the row asked "
                    "for and held, settings handed and read whole, an unusable value said back, "
                    "and another office's hand-off passed over\n");
    }
    return failures == 0 ? 0 : 1;
}
