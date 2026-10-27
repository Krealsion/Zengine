// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_FIRST_RUN_SETUP_HPP
#define ZENGINE_WORKSHOP_FIRST_RUN_SETUP_HPP

// The quick setup's names and what its pane says, pure: the office and its pane, the one setting
// it keeps, its rows, and the lines drawn for a given moment, each line with the item a press on
// it chooses. The weave (first_run.cpp) holds the moment; a suite reads the lines without a bus.
// Reference: workshop/docs/zengine.first-run.md; law: agents/workshop/first-run.md.

#include "workshop/pane_text.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace zengine::first_run {

/// The office and its one pane.
inline constexpr const char* kFirstRunRole = "zengine.first-run";
inline constexpr const char* kSetupPane = "setup";

/// THE ONE SETTING IT KEEPS, office-wide in Workshop's settings file: the quick setup was answered
/// or skipped. Cleared in the gear, it shows again at the next start.
inline constexpr const char* kAnsweredKey = "answered";

/// THE DESK A FRESH START OPENS: Workshop's Layouts row, Info and Notices. The quick setup shows
/// itself only while no other pane is open, so it never comes in front of a pane a hand already
/// opened.
// WL-FIRST-01 -- agents/workshop/first-run.md
inline bool fresh_desk(const std::vector<std::pair<std::string, std::string>>& open) {
    for (const auto& [office, pane] : open) {
        const bool layouts = office == "zengine.workshop" && pane == "layouts";
        const bool info = office == "zengine.info" && pane == "info";
        const bool notices = office == "zengine.notices-pane" && pane == "notices";
        if (!layouts && !info && !notices) {
            return false;
        }
    }
    return true;
}

/// The editor choices the Neovim box switches between, as the shipped plans author them.
inline constexpr const char* kNeovimChoice = "neovim";
inline constexpr const char* kStandardChoice = "standard";

/// Its rows: the cursor, and choosing what is under it.
inline constexpr const char* kActionUp = "first-run.up";
inline constexpr const char* kActionDown = "first-run.down";
inline constexpr const char* kActionChoose = "first-run.choose";

/// What a press or the choose row acts on: the Neovim box, a setup's Start (`setup_item(i)`, the
/// i-th setup the launcher listed), The story, Done, Skip.
enum class Item : std::int64_t { kNeovim = 0, kDone = 1, kSkip = 2, kStory = 3, kNone = -1 };
inline constexpr std::int64_t kFirstSetupItem = 100;
inline Item setup_item(std::size_t i) {
    return static_cast<Item>(kFirstSetupItem + static_cast<std::int64_t>(i));
}
/// The setup an item starts, by its place in the listing; -1 for an item that is no setup's.
inline std::int64_t setup_of(Item item) {
    const auto n = static_cast<std::int64_t>(item);
    return n >= kFirstSetupItem ? n - kFirstSetupItem : -1;
}

/// ONE SETUP THE LAUNCHER LISTED: its name, as `Launch` takes it, its title and its one line.
struct SetupRow {
    std::string name;
    std::string title;
    std::string line;
};

/// THE MOMENT THE PANE SHOWS: the box as the switch last said it, a switch in flight, the switch's
/// last word or the consent it asks, the setups the launcher listed (or why none are), the pane's
/// own notice, and the cursor.
struct Moment {
    bool neovim = false;
    bool switching = false;
    std::string switch_said;
    bool consent_asked = false;
    std::vector<SetupRow> setups;
    std::string setups_note = "asking the launcher which setups it can start";
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

/// THE PAGES IT NAMES: where each ready-to-use setup and the first five minutes are told.
inline constexpr const char* kSetupsPage = "workshop/docs/demo-setups.md";
inline constexpr const char* kGettingStartedPage = "workshop/docs/getting-started.md";

/// EACH LINE READ WHOLE IN `columns`: a line wider than the room goes on in the rows beneath it,
/// at its own indent and two more, each row keeping the line's ink and the item a press on it
/// chooses. No room said yet (`columns` 0) leaves the lines as written.
template <class Line>
std::vector<Line> wrapped(const std::vector<Line>& lines, std::int64_t columns) {
    if (columns <= 0) {
        return lines;
    }
    std::vector<Line> out;
    for (const Line& l : lines) {
        const std::size_t first = l.text.find_first_not_of(' ');
        if (static_cast<std::int64_t>(l.text.size()) <= columns || first == std::string::npos) {
            out.push_back(l);
            continue;
        }
        const std::size_t lead = std::min(first, static_cast<std::size_t>(columns / 2));
        const std::string indent(lead, ' ');
        for (const std::string& piece : workshop::pane_text::wrap(
                 l.text.substr(lead), columns - static_cast<std::int64_t>(lead))) {
            Line row = l;
            row.text = indent + piece;
            out.push_back(row);
        }
    }
    return out;
}

/// THE ITEMS, in the order the cursor moves through them.
inline std::vector<Item> items_of(const Moment& m) {
    std::vector<Item> out{Item::kNeovim};
    for (std::size_t i = 0; i < m.setups.size(); ++i) {
        out.push_back(setup_item(i));
    }
    out.push_back(Item::kStory);
    out.push_back(Item::kDone);
    out.push_back(Item::kSkip);
    return out;
}

/// THE LINES OF THE QUICK SETUP, top to bottom: the editor block, the setups the launcher lists
/// with Start on each and The story beside them, then Done and Skip; the notice last, when there
/// is one. Each is read whole in `columns` (`wrapped`).
// WL-FIRST-02 -- agents/workshop/first-run.md
inline std::vector<Line> lines_of(const Moment& m, std::int64_t columns = 0) {
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
    out.push_back({"Ready-to-use setups -- each starts in a Workshop of its own", Line::Ink::kHeading});
    for (std::size_t i = 0; i < m.setups.size(); ++i) {
        const SetupRow& s = m.setups[i];
        const Item item = setup_item(i);
        out.push_back({mark(item) + "[ Start ] " + (s.title.empty() ? s.name : s.title) +
                           (s.line.empty() ? std::string() : " -- " + s.line),
                       ink(item), item});
    }
    if (m.setups.empty()) {
        out.push_back({"  " + m.setups_note, Line::Ink::kMuted});
    }
    out.push_back({"  Each one is told in " + std::string(kSetupsPage), Line::Ink::kMuted});
    out.push_back({mark(Item::kStory) + "[ The story ] close this and open Start here, a step at "
                                        "a time",
                   ink(Item::kStory), Item::kStory});
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
    return wrapped(out, columns);
}

/// The cursor one item up or down among `m`'s items, stopping at the ends.
inline Item stepped(const Moment& m, Item at, int by) {
    const std::vector<Item> items = items_of(m);
    std::size_t here = 0;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (items[i] == at) {
            here = i;
        }
    }
    if (by < 0) {
        return items[here == 0 ? 0 : here - 1];
    }
    return items[here + 1 >= items.size() ? items.size() - 1 : here + 1];
}

} // namespace zengine::first_run

#endif // ZENGINE_WORKSHOP_FIRST_RUN_SETUP_HPP
