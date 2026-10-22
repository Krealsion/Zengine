// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_FIRST_RUN_SETUP_HPP
#define ZENGINE_WORKSHOP_FIRST_RUN_SETUP_HPP

// The quick setup's names and what its pane says, pure: the office and its pane, the one setting
// it keeps, its rows, and the lines drawn for a given moment, each line with the item a press on
// it chooses. The weave (first_run.cpp) holds the moment; a suite reads the lines without a bus.
// Reference: workshop/docs/zengine.first-run.md; law: agents/workshop/first-run.md.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::first_run {

/// The office and its one pane.
inline constexpr const char* kFirstRunRole = "zengine.first-run";
inline constexpr const char* kSetupPane = "setup";

/// THE ONE SETTING IT KEEPS, office-wide in Workshop's settings file: the quick setup was answered
/// or skipped. Cleared in the gear, it shows again at the next start.
inline constexpr const char* kAnsweredKey = "answered";

/// The editor choices the Neovim box switches between, as the shipped plans author them.
inline constexpr const char* kNeovimChoice = "neovim";
inline constexpr const char* kStandardChoice = "standard";

/// Its rows: the cursor, and choosing what is under it.
inline constexpr const char* kActionUp = "first-run.up";
inline constexpr const char* kActionDown = "first-run.down";
inline constexpr const char* kActionChoose = "first-run.choose";

/// What a press or the choose row acts on: the Neovim box, Done, Skip.
enum class Item : std::int64_t { kNeovim = 0, kDone = 1, kSkip = 2, kNone = -1 };
inline constexpr std::int64_t kItems = 3;

/// THE MOMENT THE PANE SHOWS: the box as the switch last said it, a switch in flight, the switch's
/// last word or the consent it asks, the pane's own notice, and the cursor.
struct Moment {
    bool neovim = false;
    bool switching = false;
    std::string switch_said;
    bool consent_asked = false;
    std::string notice;
    Item cursor = Item::kNeovim;
};

/// One line drawn: its text, whether it is muted, alerting or the cursor's, and the item a press
/// on it chooses (`kNone` for a line that chooses nothing).
struct Line {
    std::string text;
    enum class Ink { kPlain, kHeading, kMuted, kAlert, kCursor } ink = Ink::kPlain;
    Item item = Item::kNone;
};

/// THE PAGES IT NAMES, until the launcher lists the setups: where each ready-to-use setup and the
/// first five minutes are told.
inline constexpr const char* kSetupsPage = "workshop/docs/demo-setups.md";
inline constexpr const char* kGettingStartedPage = "workshop/docs/getting-started.md";

/// THE LINES OF THE QUICK SETUP, top to bottom: the editor block, the way to the setups, then Done
/// and Skip; the notice last, when there is one.
// WL-FIRST-02 -- agents/workshop/first-run.md
inline std::vector<Line> lines_of(const Moment& m) {
    const auto mark = [&](Item i) { return std::string(m.cursor == i ? "> " : "  "); };
    const auto ink = [&](Item i) { return m.cursor == i ? Line::Ink::kCursor : Line::Ink::kPlain; };
    std::vector<Line> out;
    out.push_back({"Quick setup -- answered once, changed any time later", Line::Ink::kHeading});
    out.push_back({""});
    out.push_back({"Editing text", Line::Ink::kHeading});
    out.push_back({mark(Item::kNeovim) + (m.neovim ? "[x]" : "[ ]") + " Edit text with Neovim",
                   ink(Item::kNeovim), Item::kNeovim});
    if (m.switching) {
        out.push_back({"      switching...", Line::Ink::kMuted});
    } else if (!m.switch_said.empty()) {
        out.push_back({"      " + m.switch_said,
                       m.consent_asked ? Line::Ink::kAlert : Line::Ink::kMuted});
    } else {
        out.push_back({"      the Editor pane becomes Neovim, and stays so at the next start",
                       Line::Ink::kMuted});
    }
    out.push_back({""});
    out.push_back({"Ready-to-use setups", Line::Ink::kHeading});
    out.push_back({"  Each runs in a Workshop of its own: " + std::string(kSetupsPage),
                   Line::Ink::kMuted});
    out.push_back({"  Your first five minutes: " + std::string(kGettingStartedPage),
                   Line::Ink::kMuted});
    out.push_back({""});
    out.push_back({mark(Item::kDone) + "[ Done ] keep these answers", ink(Item::kDone),
                   Item::kDone});
    out.push_back({mark(Item::kSkip) + "[ Skip ] change nothing more", ink(Item::kSkip),
                   Item::kSkip});
    if (!m.notice.empty()) {
        out.push_back({""});
        out.push_back({m.notice, Line::Ink::kAlert});
    }
    return out;
}

/// The cursor one item up or down, stopping at the ends.
inline Item stepped(Item at, int by) {
    const std::int64_t next = static_cast<std::int64_t>(at) + by;
    return next < 0 ? Item::kNeovim
                    : next >= kItems ? Item::kSkip : static_cast<Item>(next);
}

} // namespace zengine::first_run

#endif // ZENGINE_WORKSHOP_FIRST_RUN_SETUP_HPP
