// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `screen.hpp`'s sections -- spelling the effective bindings -- compiled once into
// `zengine-workshop-logic` and linked by the host and every suite; the declarations, the
// constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/document.md (+7 registers; agents/workshop.md routes)

#include "screen.hpp"

namespace zengine::workshop {

// ---- Spelling the effective bindings -----------------------------------------------------

// WL-KEY-02 -- agents/workshop/keyboard.md
std::string hotkey_text(const Keymap& k, Act a) {
    return gesture_text(k.gesture_of(a));
}

std::string arrows_text(const Keymap& k, Act left, Act right, Act up, Act down) {
    const bool folded = k.gesture_of(left) == Gesture{input::scan::kLeft, input::mod::kNone} &&
                        k.gesture_of(right) ==
                            Gesture{input::scan::kRight, input::mod::kNone} &&
                        k.gesture_of(up) == Gesture{input::scan::kUp, input::mod::kNone} &&
                        k.gesture_of(down) == Gesture{input::scan::kDown, input::mod::kNone};
    if (folded) {
        return "arrows";
    }
    return hotkey_text(k, left) + "/" + hotkey_text(k, right) + "/" + hotkey_text(k, up) +
           "/" + hotkey_text(k, down);
}

// WL-KEY-09, WL-KEY-15 -- agents/workshop/keyboard.md
std::vector<std::string> help_pairs(const Keymap& k, KeyContext ctx, std::int64_t pane) {
    std::vector<std::string> out;
    // A FOCUSED PANE'S OWN ROWS COME FIRST, as a built-in context's own rows do: what the
    // pane declared, spelled through the same effective map dispatch reads, so an override
    // a maker authored for a pane's id is what the band teaches. A row with no gesture
    // teaches no key, for the reason `take` gives below.
    if (ctx == KeyContext::kPane) {
        if (const PaneRows* rows = k.pane_rows(pane)) {
            for (const PaneRow& row : rows->rows) {
                if (!is_bound(row.gesture)) {
                    continue;
                }
                out.push_back(gesture_text(row.gesture) + " " + row.label);
            }
        }
    }
    // ⭐ THE LEGEND'S FOLDS WERE HERE -- `hjkl move`, `shift+hjkl size` and `[ ] workspace`, three
    // spellings for families of object rows -- and retired with the canvas's keys. Every row the
    // legend teaches now is its own pair.
    const auto take = [&](bool concrete) {
        for (const ActionRow& row : kActionCatalog) {
            const bool is_concrete = row.context != KeyContext::kGlobal &&
                                     row.context != KeyContext::kNoText &&
                                     row.context != KeyContext::kUnlessOwned;
            // THE LEGEND TEACHES WHAT WOULD RUN, so a row the focused pane has stood in
            // for is not spelled here -- the pane's own row for it already is, first
            // (WL-KEY-15).
            if (is_concrete != concrete || !k.row_active(row, ctx, pane)) {
                continue;
            }
            // A ROW WITH NO GESTURE TEACHES NO KEY. The legend's whole job is
            // `gesture label` pairs, and its scarcest resource is columns; a pair whose
            // gesture half is `?` spends them saying that a key does not exist. The action
            // is still reachable -- from the surface that names it, and from a maker's own
            // binding, which puts the row back here the moment there is one to spell.
            if (!is_bound(k.row_gesture(row))) {
                continue;
            }
            std::string pair = gesture_text(k.row_gesture(row));
            pair += " ";
            pair += row.label;
            // TWO ROWS OF ONE ACTION CAN COME TO ONE SPELLING (an override moves them
            // both), and one meaning said twice in one band is noise, not truth.
            bool repeated = false;
            for (const std::string& seen : out) {
                if (seen == pair) {
                    repeated = true;
                    break;
                }
            }
            if (!repeated) {
                out.push_back(std::move(pair));
            }
        }
    };
    take(true);
    take(false);
    // ⭐ AND THE APPLICATION'S ROWS ABOVE EVERY MODE, as they are in force (WL-DESK-07): a launch
    // a maker moved is taught where they moved it, and one they disabled is not taught at all.
    for (const AppRow& row : k.app) {
        if (row.precedence == app_precedence::kAboveModes && k.app_row_active(row, ctx, pane)) {
            out.push_back(gesture_text(row.gesture) + " " + row.label);
        }
    }
    return out;
}

// WL-GEO-08 -- agents/workshop/geometry.md
bool adopt_screen(Session& s, std::int64_t want_w, std::int64_t want_h,
                  std::int64_t want_advance_px, std::int64_t want_line_px,
                  std::int64_t want_cell_px) {
    const std::int64_t advance = want_advance_px > 0 ? want_advance_px : 0;
    const std::int64_t line = want_line_px > 0 ? want_line_px : 0;
    // THE CANVAS'S DEVICE UNIT NEEDS NO CEILING OF ITS OWN. It arrives on the bus
    // like every other field of the shape, so a negative number is data rather than an
    // error — and non-positive is already the vocabulary's "my device unit IS the cell",
    // which is the reading that changes nothing. Above zero there is no number to refuse:
    // `surface::device_of_subs` and `subs_exact_in_device` are total over every positive
    // multiplier by their own saturation, and inventing a plausibility bound here would be
    // this application deciding how big somebody else's pixel is allowed to be.
    const std::int64_t cell = want_cell_px > 0 ? want_cell_px : 0;
    const Screen fresh = screen_of(want_w, want_h, advance, line);
    if (fresh.w == s.screen_w && fresh.h == s.screen_h && advance == s.text_advance_px &&
        line == s.text_line_px && cell == s.cell_px) {
        return false;
    }
    s.screen_w = fresh.w;
    s.screen_h = fresh.h;
    s.text_advance_px = advance;
    s.text_line_px = line;
    s.cell_px = cell;
    return true;
}

// (`first_editable` WAS HERE -- where a row cursor landed on a fresh list -- and left with the
// last row cursor this host kept, the host Pane Manager's.)

// ⭐ `workspace_scene`, `inspector_rows`, `refocus` AND `position_of` WERE HERE -- the object
// document resolved against the workspace, its inspector rows and the name of what they addressed
// -- and retired with the prototype canvas.

namespace detail {

std::string pad(std::string text, std::size_t width) {
    if (text.size() > width) {
        text.resize(width);
        return text;
    }
    text.append(width - text.size(), ' ');
    return text;
}

// WL-TEXT-05 -- agents/workshop/text-box.md
std::string fit(std::string text, std::int64_t width) {
    if (width <= 0) {
        return {};
    }
    const std::size_t room = static_cast<std::size_t>(width);
    if (text.size() <= room) {
        return text; // it fits, so nothing about it changes -- not even its role
    }
    const std::size_t mark = std::char_traits<char>::length(kElided);
    if (room <= mark) {
        return std::string(kElided).substr(0, room);
    }
    text.resize(room - mark);
    text += kElided;
    return text;
}

// WL-PROJ-10 -- agents/workshop/project.md
std::size_t path_root_cue(const std::string& p) {
    if (p.size() >= 2 && p[0] == '/' && p[1] == '/') {
        const std::size_t at = p.find('/', 2); // `//server/` -- the name AND its separator
        return at == std::string::npos ? p.size() : at + 1;
    }
    if (!p.empty() && p[0] == '/') {
        return 1;
    }
    if (p.size() >= 3 && p[1] == ':' && p[2] == '/') {
        return 3;
    }
    return 0;
}

// WL-PROJ-10 -- agents/workshop/project.md; WL-TAB-03 -- agents/workshop/tab-run.md
std::string fit_path(const std::string& path, std::int64_t width) {
    if (width <= 0) {
        return {};
    }
    const std::size_t room = static_cast<std::size_t>(width);
    if (path.size() <= room) {
        return path; // it fits, so nothing about it changes
    }
    const std::size_t mark = std::char_traits<char>::length(kElided);
    const std::size_t root = path_root_cue(path);
    if (root + mark + 1 > room) {
        return fit(path, width); // no room for root + mark + one cell of tail
    }
    std::string tail = path.substr(path.size() - (room - root - mark));
    const std::size_t boundary = tail.find('/');
    if (boundary != std::string::npos && boundary + 1 < tail.size()) {
        tail = tail.substr(boundary); // start the tail at a whole component
    }
    return path.substr(0, root) + kElided + tail;
}

} // namespace detail

} // namespace zengine::workshop
