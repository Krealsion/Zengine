// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PALETTE_ENTRIES_HPP
#define ZENGINE_WORKSHOP_PALETTE_ENTRIES_HPP
// What the palette lists, read off what Workshop says to any presenter -- the keymap in force and
// the pane inventory -- and how a typed filter orders it. Pure: the weave owns when to read and
// whom to believe, and keeps no catalog of its own. Reference: workshop/docs/zengine.palette.md.
#include "workshop/command_vocabulary.hpp"
#include "workshop/desktop_seam_vocabulary.hpp"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace zengine::palette {

/// THE BOUNDS: the entries held, the filter's bytes, the choices remembered for the run.
inline constexpr std::size_t kMaxEntries = 1024;
inline constexpr std::size_t kMaxFilterBytes = 64;
inline constexpr std::size_t kMaxHistory = 32;

/// ONE ENTRY: what it runs as (`workshop::CommandRunRequested`'s kind and address), and what a
/// line says of it -- its label, whose it is, and its key as Workshop spelled it, `-` for none
/// and `off` for one the weaver switched off. A switched-off row and a pane not here are listed
/// and run nothing.
struct Entry {
    std::string kind, office, pane, id;
    std::string label, owner, key;
    std::size_t more_keys = 0;
    bool off = false;
    bool gone = false;
    std::string address() const { return kind + "|" + office + "|" + pane + "|" + id; }
};

/// THE ENTRIES, and how many the bound left out and from where.
struct Listing {
    std::vector<Entry> entries;
    std::size_t left_out = 0;
    std::string left_from;
};

/// Does `group`, as `KeymapShown` names where a row is answered, name a place a hand reaches by
/// a key from anywhere -- above the modes, or command mode? A mode open only while the hand is in
/// it (the context actions, arranging, naming) is not.
inline bool reachable_group(const std::string& group) {
    return group.rfind("above every mode", 0) == 0 || group == "command mode";
}

/// THE LIST, from the keymap in force and the inventory: every row a hand can request outside a
/// transient mode -- Workshop's, the application's above every mode, and each open pane's in the
/// mode it is in -- then every pane. The text box's own keys, the palette's own rows and the key
/// that opens it are not entries. A row with several keys is one entry, its first key shown.
inline Listing entries_of(const workshop::KeymapShown& keys, const workshop::PaneInventory& inv) {
    Listing out;
    std::map<std::pair<std::string, std::string>, const workshop::InventoryPane*> panes;
    for (const workshop::InventoryPane& p : inv.panes) {
        panes[{p.office, p.pane}] = &p;
    }
    std::map<std::string, std::size_t> seen;
    std::size_t actions_left = 0, panes_left = 0;
    const auto add = [&](Entry e, std::size_t& left) {
        if (out.entries.size() >= kMaxEntries) {
            ++left;
            return;
        }
        out.entries.push_back(std::move(e));
    };
    for (const workshop::ShownBinding& row : keys.rows) {
        if (!row.remappable || row.id.empty() || row.office == workshop::kPaletteRole ||
            row.id.rfind("desktop.palette", 0) == 0) {
            continue;
        }
        Entry e;
        e.kind = workshop::kCommandAction;
        e.office = row.office;
        e.pane = row.pane;
        e.id = row.id;
        e.label = row.label;
        if (row.office == workshop::kDesktopRole && row.pane.empty()) {
            if (row.group.rfind("above every mode", 0) != 0) {
                continue; // the default rows answer only where nothing took the key
            }
            e.owner = "Desktop";
        } else if (row.pane.empty()) {
            if (!reachable_group(row.group)) {
                continue;
            }
            e.owner = "Workshop";
        } else {
            const auto at = panes.find({row.office, row.pane});
            if (at == panes.end() || !at->second->open) {
                continue; // a pane's rows are in force only while it is on the desk
            }
            e.owner = at->second->name;
        }
        e.off = row.authored && row.gesture.empty();
        e.key = e.off ? "off" : row.shown.empty() ? "-" : row.shown;
        const auto earlier = seen.find(e.address());
        if (earlier != seen.end()) {
            if (earlier->second < out.entries.size()) {
                ++out.entries[earlier->second].more_keys;
            }
            continue;
        }
        seen[e.address()] = out.entries.size();
        add(std::move(e), actions_left);
    }
    // EACH PANE, and the key that opens it where an application row says it opens it.
    for (const workshop::InventoryPane& p : inv.panes) {
        if (p.office == workshop::kPaletteRole) {
            continue;
        }
        Entry e;
        e.kind = workshop::kCommandPane;
        e.office = p.office;
        e.pane = p.pane;
        e.label = p.name;
        e.owner = "pane";
        e.gone = !p.available;
        e.key = "-";
        const std::string to = "to " + p.office + "/" + p.pane;
        for (const workshop::ShownBinding& row : keys.rows) {
            for (const workshop::ActionCause& c : row.causes) {
                if (c.subject == to && !row.shown.empty() && e.key == "-") {
                    e.key = row.shown;
                }
            }
        }
        add(std::move(e), panes_left);
    }
    out.left_out = actions_left + panes_left;
    out.left_from = actions_left > 0 && panes_left > 0 ? "actions and panes"
                    : actions_left > 0                 ? "actions"
                                                       : panes_left > 0 ? "panes" : "";
    return out;
}

/// `text` case folded.
inline std::string folded(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

/// The words of a filter, case folded.
inline std::vector<std::string> words_of(const std::string& filter) {
    std::vector<std::string> out;
    std::string word;
    for (const char c : filter + " ") {
        if (c == ' ') {
            if (!word.empty()) {
                out.push_back(folded(word));
            }
            word.clear();
        } else {
            word += c;
        }
    }
    return out;
}

/// DOES THE ENTRY MATCH: each word found in its label, its owner's name or its id, case folded --
/// as Flow's search line matches a power.
inline bool matches(const Entry& e, const std::vector<std::string>& words) {
    const std::string label = folded(e.label), owner = folded(e.owner), id = folded(e.id);
    for (const std::string& w : words) {
        if (label.find(w) == std::string::npos && owner.find(w) == std::string::npos &&
            id.find(w) == std::string::npos) {
            return false;
        }
    }
    return true;
}

/// THE ENTRIES A FILTER SHOWS, in order: those chosen earlier this run first, the most recent
/// first; then, when a word names a pane, the panes before the actions; then the order listed.
inline std::vector<std::size_t> shown(const Listing& l, const std::string& filter,
                                      const std::vector<std::string>& history) {
    const std::vector<std::string> words = words_of(filter);
    std::vector<std::size_t> out;
    std::vector<bool> taken(l.entries.size(), false);
    for (auto h = history.rbegin(); h != history.rend(); ++h) {
        for (std::size_t i = 0; i < l.entries.size(); ++i) {
            if (!taken[i] && l.entries[i].address() == *h && matches(l.entries[i], words)) {
                out.push_back(i);
                taken[i] = true;
            }
        }
    }
    bool names_pane = false;
    for (const Entry& e : l.entries) {
        if (e.kind != workshop::kCommandPane || words.empty()) {
            continue;
        }
        for (const std::string& w : words) {
            names_pane = names_pane || folded(e.label).find(w) != std::string::npos;
        }
    }
    const auto pass = [&](bool panes) {
        for (std::size_t i = 0; i < l.entries.size(); ++i) {
            if (!taken[i] && (l.entries[i].kind == workshop::kCommandPane) == panes &&
                matches(l.entries[i], words)) {
                out.push_back(i);
                taken[i] = true;
            }
        }
    };
    pass(names_pane);
    pass(!names_pane);
    return out;
}

/// `history` with `address` chosen now: moved to the end, the oldest forgotten past the bound.
inline std::vector<std::string> remembered(std::vector<std::string> history,
                                           const std::string& address) {
    history.erase(std::remove(history.begin(), history.end(), address), history.end());
    history.push_back(address);
    if (history.size() > kMaxHistory) {
        history.erase(history.begin(),
                      history.begin() + static_cast<std::ptrdiff_t>(history.size() - kMaxHistory));
    }
    return history;
}

} // namespace zengine::palette
#endif
