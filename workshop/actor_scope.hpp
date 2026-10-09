// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_ACTOR_SCOPE_HPP
#define ZENGINE_WORKSHOP_ACTOR_SCOPE_HPP

// WHAT A GUEST'S HAND MAY DO BESIDE WHAT ITS GRANT SAYS: the action classes an owner asks for its
// act -- build, write, open -- judged against the row the guest door admitted the actor's session
// under and the guests file's `host` (docs/workshop/external-host.md). Pure: the host answers the
// facts, this says yes or the refusal in words. An actor no guest door admitted is the weaver's own
// participant, and no class narrows it.

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace zengine::workshop::scope {

/// The three action classes. An act names every class it is: the Builder's role line writes a
/// plan row and builds, so it asks `build` and `write` both.
inline constexpr const char* kBuild = "build";
inline constexpr const char* kWrite = "write";
inline constexpr const char* kOpen = "open";

/// The power that reads the desk's words and pictures, as the guests file spells it
/// (`guests::kPowerCapture`).
inline constexpr const char* kPowerCapture = "capture";

/// ONE GUEST SESSION'S ADMITTED ROW, as the guest door recorded it when it admitted the session:
/// the row's own powers and the version of the file it was written in. `admitted` is false for an
/// actor no guest door admitted.
struct GuestRowFacts {
    bool admitted = false;
    std::string name;
    std::vector<std::string> powers;
    std::int64_t version = 0;

    bool may(std::string_view power) const {
        return std::find(powers.begin(), powers.end(), power) != powers.end();
    }
};

/// WHOSE HOST THIS IS, the guests file's `host`: on a weaver's host a guest writes no file, types
/// into no pane and reaches none of the editor office, the Terminal and the Hotkeys pane; on a
/// development host, the agent's own, a guest with `input` reaches them.
struct HostFact {
    bool development = false;
    std::string guests_file; ///< the guests file this run read, absolute; empty when none
};

/// Whether `asked` names the same file as `file`, by the file system's own answer: case, links
/// and spellings of one file are one file. A path that names nothing names no file.
inline bool same_file(const std::string& asked, const std::string& file) {
    if (asked.empty() || file.empty()) return false;
    std::error_code ec;
    const bool same = std::filesystem::equivalent(std::filesystem::path(asked),
                                                  std::filesystem::path(file), ec);
    return !ec && same;
}

/// THE JUDGEMENT OF ONE CLASS FOR ONE ACTOR: empty when the class is the actor's, else the refusal
/// in words. `subject` is what a class `open` opens.
inline std::string judge(const GuestRowFacts& row, const HostFact& host, std::string_view action_class,
                         const std::string& subject = {}) {
    if (!row.admitted) return {};
    const std::string who = "guest '" + row.name + "'";
    if (action_class == kBuild) {
        if (row.may(kBuild)) return {};
        return who + " may not build here: its row has no `build`, and only `build` reaches the "
                     "Builder's builds and loads";
    }
    if (action_class == kWrite) {
        if (host.development) return {};
        return "this is a weaver's host: only the weaver's hand writes a file here, and " + who +
               " is not the weaver";
    }
    if (action_class == kOpen) {
        if (host.development || !same_file(subject, host.guests_file)) return {};
        return "this is a weaver's host: the guests file opens only by the weaver's hand";
    }
    return "this Workshop knows no action class '" + std::string(action_class) + "'";
}

/// Every class at once: the first refusal, or empty when each is the actor's.
inline std::string judge_all(const GuestRowFacts& row, const HostFact& host,
                             const std::vector<std::string>& classes, const std::string& subject = {}) {
    for (const std::string& c : classes) {
        std::string refused = judge(row, host, c, subject);
        if (!refused.empty()) return refused;
    }
    return {};
}

/// THE CLASSES A SEND IS, by the shape it names: each shape a guest's grant names whose owner
/// treats it as an act that writes or opens. A relay -- the Composer, a stored command -- sends as
/// its own office, so its owner's question about the sender meets no guest; the send is judged at
/// its approval instead, for the hand that asked it (`judge_send`).
inline std::vector<std::string> classes_of_send(std::string_view shape) {
    if (shape == "InventoryToolboxSave" || shape == "WorkshopQuitRequested") return {kWrite};
    if (shape == "OpenSourceRequested") return {kOpen};
    return {};
}

/// A SEND ON A GUEST'S BEHALF, judged by the classes its shape is. An open names its path in the
/// message, which the approval does not see, so on a weaver's host it is refused whole: the
/// guests file opens only by the weaver's hand. Empty when the hand is not a guest's, the shape is
/// no class, or this is a development host.
inline std::string judge_send(const GuestRowFacts& row, const HostFact& host, std::string_view shape) {
    for (const std::string& c : classes_of_send(shape)) {
        if (c == kOpen) {
            if (!row.admitted || host.development) continue;
            return "this is a weaver's host: an open sent on guest '" + row.name +
                   "''s behalf shows Workshop no path, and only the weaver's hand opens one so";
        }
        if (std::string refused = judge(row, host, c); !refused.empty()) return refused;
    }
    return {};
}

/// ON A WEAVER'S HOST A PLACE ONLY THE WEAVER'S HAND REACHES refuses a guest's keys, text, presses,
/// wheels and drops whole: `place` names it in the refusal. Empty when the hand is not a guest's, or this is
/// a development host. Which places those are is the dispatch's (`WorkshopWeave::refused_toward`).
inline std::string refuse_toward(const GuestRowFacts& row, const HostFact& host, std::string_view place) {
    if (!row.admitted || host.development) return {};
    return "this is a weaver's host: " + std::string(place) + " answers only the weaver's hand, and guest '" +
           row.name + "' is not the weaver";
}

/// ON A WEAVER'S HOST A GUEST'S TYPED TEXT RESTS IN NO PANE AND NO LINE: so nothing the weaver
/// commits or saves holds a guest's words as the weaver's. Empty when the text may go.
inline std::string refuse_text(const GuestRowFacts& row, const HostFact& host) {
    if (!row.admitted || host.development) return {};
    return "this is a weaver's host: a guest's typed text rests in no pane, and guest '" + row.name +
           "' typed it";
}

/// ...AND NOR DOES AN ITEM A GUEST WOULD CARRY, a value or a reference: dropped on a pane it would
/// stand there as a draft the weaver's own Submit, commit or save sends as the weaver's, so the
/// carry never begins. Empty when it may.
inline std::string refuse_carry(const GuestRowFacts& row, const HostFact& host) {
    if (!row.admitted || host.development) return {};
    return "this is a weaver's host: a guest's carried item rests in no pane, so guest '" + row.name +
           "' carries nothing here";
}

} // namespace zengine::workshop::scope

#endif // ZENGINE_WORKSHOP_ACTOR_SCOPE_HPP
