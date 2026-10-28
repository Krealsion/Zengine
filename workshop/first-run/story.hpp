// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_FIRST_RUN_STORY_HPP
#define ZENGINE_WORKSHOP_FIRST_RUN_STORY_HPP

// "Start here", pure: the story's five steps, each step's tick and who made it, the ticks as the
// text of one office-wide setting, and the lines the pane draws for a given moment, each line with
// the item a press on it chooses. The weave (first_run.cpp) hears the facts that tick a step; a
// suite reads the lines without a bus.
// Reference: workshop/docs/zengine.first-run.md; law: agents/workshop/first-run.md.

#include "workshop/first-run/setup.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::first_run {

/// The "Start here" pane, and the setting that keeps its ticks across starts.
inline constexpr const char* kStoryPane = "story";
inline constexpr const char* kStoryKey = "story";

/// Its rows: the cursor, and choosing what is under it.
inline constexpr const char* kStoryUp = "story.up";
inline constexpr const char* kStoryDown = "story.down";
inline constexpr const char* kStoryChoose = "story.choose";

/// THE STORY'S STEPS, in order.
inline constexpr std::size_t kSteps = 5;

/// What one step asks, where Show me opens, the next move it says there, and whose fact ticks it.
struct Step {
    const char* title;
    const char* what;
    const char* office; ///< the pane Show me opens: its office and pane
    const char* pane;
    const char* move;   ///< the step's next move, said when Show me opened its pane
    const char* by;     ///< whose fact ticks it, as the line says it
};

// WL-FIRST-04 -- agents/workshop/first-run.md
inline const std::array<Step, kSteps>& steps() {
    static const std::array<Step, kSteps> all{{
        {"Make a view", "lay out a panel in the View Builder and run it", "zengine.view.builder",
         "view-builder",
         "place a number field, a button and a label, save the view, then press Run",
         "a running view is on the desk"},
        {"Wire its button into Flow", "drag the button's intent onto Flow and run the flow",
         "zengine.flow", "flow",
         "drag the running view's button onto Flow, author what it does, then run the flow",
         "an office accepted what the button said"},
        {"Show Flow's answer in the view", "drag the field Flow emits onto the view's label",
         "zengine.flow", "flow", "drag the field the flow emits onto the view's label",
         "the view was told a field it shows"},
        {"Make it an app",
         "save the view, export its flow beside it under the view's name, then the View Builder's "
         "Export app",
         "zengine.view.builder", "view-builder",
         "save the view, export its flow from Flow beside it under the view's name (my.view, "
         "my.flow), then press Export app: the app is written in the project's apps/",
         "the launcher lists the app"},
        {"Start it, and close Workshop", "the View Builder's Run app, then quit",
         "zengine.view.builder", "view-builder",
         "press Run app; the app goes on when you quit Workshop",
         "the launcher started the app, or says it runs"},
    }};
    return all;
}

/// WHO TICKED A STEP: nobody yet, its owner's fact, or the weaver's own hand.
enum class Tick : std::int64_t { kNone = 0, kFact = 1, kHand = 2 };
using Ticks = std::array<Tick, kSteps>;

/// THE TICKS AS THE SETTING KEEPS THEM: each ticked step's number, `h` after one the weaver
/// ticked by hand, space-separated, `1 2h 4`. At most `kMaxStoryText` bytes.
inline constexpr std::size_t kMaxStoryText = 16;

inline std::string story_text(const Ticks& ticks) {
    std::string out;
    for (std::size_t i = 0; i < kSteps; ++i) {
        if (ticks[i] == Tick::kNone) {
            continue;
        }
        out += (out.empty() ? "" : " ") + std::to_string(i + 1) +
               (ticks[i] == Tick::kHand ? "h" : "");
    }
    return out;
}

/// ...and read back: a word that names no step is passed over.
inline Ticks ticks_of(const std::string& text) {
    Ticks out{};
    std::string word;
    for (const char c : text + " ") {
        if (c != ' ') {
            word += c;
            continue;
        }
        if (!word.empty() && word[0] >= '1' && word[0] <= '0' + static_cast<char>(kSteps) &&
            (word.size() == 1 || (word.size() == 2 && word[1] == 'h'))) {
            out[static_cast<std::size_t>(word[0] - '1')] = word.size() == 2 ? Tick::kHand : Tick::kFact;
        }
        word.clear();
    }
    return out;
}

/// Is every step ticked?
inline bool story_done(const Ticks& ticks) {
    for (const Tick t : ticks) {
        if (t == Tick::kNone) {
            return false;
        }
    }
    return true;
}

/// What a press or the choose row acts on: a step's box (`2 * step`) or its Show me
/// (`2 * step + 1`); `kStoryItems` of them, in order.
inline constexpr std::int64_t kStoryItems = static_cast<std::int64_t>(2 * kSteps);
inline constexpr std::int64_t kNoStoryItem = -1;

/// THE MOMENT THE PANE SHOWS: the ticks, the cursor, and its notice -- a hint (a step's next move
/// after Show me), or why a press, a write or a launch did not land.
struct StoryMoment {
    Ticks ticks{};
    std::int64_t cursor = 0;
    std::string notice;
    bool hint = false; ///< the notice is a hint, drawn plain; anything else is drawn as an alert
};

/// One line drawn: its text, how it is inked, and the item a press on it chooses.
struct StoryLine {
    std::string text;
    enum class Ink { kPlain, kHeading, kMuted, kAlert, kCursor, kDone } ink = Ink::kPlain;
    std::int64_t item = kNoStoryItem;
};

/// THE LINES OF "START HERE", top to bottom: a heading, then for each step its box and what it
/// asks, whose fact ticks it, and its Show me; a line when the story is done; the notice last.
/// Each is read whole in `columns` (`wrapped`).
// WL-FIRST-04 -- agents/workshop/first-run.md
inline std::vector<StoryLine> story_lines(const StoryMoment& m, std::int64_t columns = 0) {
    std::vector<StoryLine> out;
    const auto mark = [&](std::int64_t i) { return std::string(m.cursor == i ? "> " : "  "); };
    out.push_back({"Start here -- the story, a step at a time", StoryLine::Ink::kHeading});
    out.push_back({"  each box is ticked when its owner says it happened, or by your press"});
    for (std::size_t s = 0; s < kSteps; ++s) {
        const Step& step = steps()[s];
        const auto box = static_cast<std::int64_t>(2 * s);
        const Tick t = m.ticks[s];
        const std::string mark_box = t == Tick::kNone ? "[ ]" : "[x]";
        const std::string who = t == Tick::kHand   ? " -- ticked by you"
                                : t == Tick::kFact ? " -- done"
                                                   : "";
        out.push_back({""});
        out.push_back({mark(box) + mark_box + " " + std::to_string(s + 1) + ". " + step.title + who,
                       m.cursor == box ? StoryLine::Ink::kCursor
                       : t != Tick::kNone ? StoryLine::Ink::kDone
                                          : StoryLine::Ink::kPlain,
                       box});
        out.push_back({"      " + std::string(step.what) + "; ticked when " + step.by,
                       StoryLine::Ink::kMuted});
        out.push_back({mark(box + 1) + "    [ Show me ]",
                       m.cursor == box + 1 ? StoryLine::Ink::kCursor : StoryLine::Ink::kPlain,
                       box + 1});
    }
    if (story_done(m.ticks)) {
        out.push_back({""});
        out.push_back({"The story is done: Start here stays in the Pane Manager", StoryLine::Ink::kHeading});
    }
    if (!m.notice.empty()) {
        out.push_back({""});
        out.push_back({m.notice, m.hint ? StoryLine::Ink::kPlain : StoryLine::Ink::kAlert});
    }
    return wrapped(out, columns);
}

/// The cursor one item up or down, stopping at the ends.
inline std::int64_t story_stepped(std::int64_t at, int by) {
    const std::int64_t next = at + by;
    return next < 0 ? 0 : next >= kStoryItems ? kStoryItems - 1 : next;
}

} // namespace zengine::first_run

#endif // ZENGINE_WORKSHOP_FIRST_RUN_STORY_HPP
