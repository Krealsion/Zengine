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

// WL-KEY-09 -- agents/workshop/keyboard.md
std::vector<std::string> help_pairs(const Keymap& k, KeyContext ctx) {
    std::vector<std::string> out;
    // A fold: the run of actions it covers (in catalog order, keyed on the first), the
    // gestures that make it true, and the folded pair it becomes.
    struct Fold {
        Act first;
        Act rest[3];
        std::size_t others;
        Gesture wants[4];
        const char* pair;
    };
    namespace sc = input::scan;
    namespace mo = input::mod;
    static const Fold kFolds[] = {
        {Act::kObjectLeft,
         {Act::kObjectDown, Act::kObjectUp, Act::kObjectRight},
         3,
         {{sc::kH, mo::kNone}, {sc::kJ, mo::kNone}, {sc::kK, mo::kNone}, {sc::kL, mo::kNone}},
         "hjkl move"},
        {Act::kObjectNarrower,
         {Act::kObjectTaller, Act::kObjectShorter, Act::kObjectWider},
         3,
         {{sc::kH, mo::kShift},
          {sc::kJ, mo::kShift},
          {sc::kK, mo::kShift},
          {sc::kL, mo::kShift}},
         "shift+hjkl size"},
        {Act::kInfoUp,
         {Act::kInfoDown, Act::kNone, Act::kNone},
         1,
         {{sc::kUp, mo::kNone}, {sc::kDown, mo::kNone}, {}, {}},
         "up/down row"},
        {Act::kWorkspaceNarrower,
         {Act::kWorkspaceWider, Act::kNone, Act::kNone},
         1,
         {{sc::kLeftBracket, mo::kNone}, {sc::kRightBracket, mo::kNone}, {}, {}},
         "[ ] workspace"},
    };
    const auto fold_holding = [&k](const Fold& f) {
        if (k.gesture_of(f.first) != f.wants[0]) {
            return false;
        }
        for (std::size_t i = 0; i < f.others; ++i) {
            if (k.gesture_of(f.rest[i]) != f.wants[i + 1]) {
                return false;
            }
        }
        return true;
    };
    const auto folded_member = [&](Act a, const Fold*& holds) {
        for (const Fold& f : kFolds) {
            if (!fold_holding(f)) {
                continue;
            }
            if (f.first == a) {
                holds = &f;
                return true;
            }
            for (std::size_t i = 0; i < f.others; ++i) {
                if (f.rest[i] == a) {
                    holds = nullptr; // covered by the fold its first member emitted
                    return true;
                }
            }
        }
        return false;
    };
    const auto take = [&](bool concrete) {
        for (const ActionRow& row : kActionCatalog) {
            const bool is_concrete = row.context != KeyContext::kGlobal &&
                                     row.context != KeyContext::kNoText &&
                                     row.context != KeyContext::kNoEditor;
            if (is_concrete != concrete || !active_in(row.context, ctx)) {
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
            const Fold* holds = nullptr;
            if (ctx == KeyContext::kCommand && folded_member(row.act, holds)) {
                if (holds != nullptr) {
                    out.push_back(holds->pair);
                }
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
    return out;
}

// WL-DOC-17 -- agents/workshop/document.md; WL-GEO-08 -- agents/workshop/geometry.md
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
    s.workspace_w = fresh.room_w;
    s.workspace_h = fresh.room_h;
    return true;
}

// WL-DOC-05, WL-DOC-12, WL-DOC-18 -- agents/workshop/document.md
ui::Scene workspace_scene(const WorkshopDoc& d, const Session& s) {
    return ui::resolve(d.elements, ui::Viewport{s.workspace_w, s.workspace_h});
}

// WL-DOC-05 -- agents/workshop/document.md; WL-INFO-06 -- agents/workshop/info-body.md
std::vector<Row> inspector_rows(WorkshopDoc& d, const Session& s) {
    std::vector<Row> rows;
    const std::int64_t id = s.selected;
    if (doc::find(d, id) == nullptr) {
        return rows;
    }
    const std::int64_t ww = s.workspace_w;
    const std::int64_t wh = s.workspace_h;

    rows.push_back(Row::show("Identity", [id] { return "#" + std::to_string(id); }));
    rows.push_back(Row::edit("Name", doc::name_of(d, id)));
    // Context comes BEFORE the four numbers it gives meaning to, because that is
    // the order the reading has to happen in: `X 2` is not an answer until you
    // know 2 of what. It is one more `Row::edit` over one more property, and
    // that is the measurement -- a relationship is not a different kind of thing
    // needing a different kind of editor. It is
    // NOT labelled `Parent`: nothing here is a parent, and a familiar word that
    // implies ownership, clipping and cascade-delete would be the tool telling a
    // maker something the document does not do.
    rows.push_back(Row::edit("Context", doc::context_of(d, id)));
    rows.push_back(Row::edit("X", doc::x_of(d, id)));
    rows.push_back(Row::edit("Y", doc::y_of(d, id)));
    rows.push_back(Row::edit("Width", doc::width_of(d, id)));
    rows.push_back(Row::edit("Height", doc::height_of(d, id)));
    rows.push_back(Row::show("Resolved", [&d, id, ww, wh] {
        const ui::Scene scene = ui::resolve(d.elements, ui::Viewport{ww, wh});
        const ui::Placed* placed = ui::placed_for(scene, id);
        if (placed == nullptr) {
            return std::string("-");
        }
        return std::to_string(placed->rect.w) + " x " + std::to_string(placed->rect.h) + " cells";
    }));
    return rows;
}

std::size_t first_editable(const std::vector<Row>& rows) {
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].editable()) {
            return i;
        }
    }
    return 0;
}

void refocus(WorkshopDoc& d, Session& s) {
    s.rows = inspector_rows(d, s);
    s.cursor = first_editable(s.rows);
}

// WL-INFO-06 -- agents/workshop/info-body.md
void refocus_keeping_draft(WorkshopDoc& d, Session& s) {
    const std::vector<Row> was = std::move(s.rows);
    const std::size_t cursor = s.cursor;
    s.rows = inspector_rows(d, s);
    for (std::size_t i = 0; i < s.rows.size() && i < was.size(); ++i) {
        if (s.rows[i].label() == was[i].label()) {
            s.rows[i].resume(was[i]);
        }
    }
    // AND THE CURSOR STAYS WHERE THE MAKER LEFT IT. It is the other half of "their hands are
    // still on it": a resize that moved the highlight to the first editable row would make a
    // maker who was reading Height look at Name instead, for no reason they could see.
    s.cursor = cursor < s.rows.size() ? cursor : first_editable(s.rows);
}

std::size_t position_of(const WorkshopDoc& d, std::int64_t id) {
    for (std::size_t i = 0; i < d.elements.size(); ++i) {
        if (d.elements[i].id == id) {
            return i;
        }
    }
    return d.elements.size();
}

namespace detail {

std::string pad(std::string text, std::size_t width) {
    if (text.size() > width) {
        text.resize(width);
        return text;
    }
    text.append(width - text.size(), ' ');
    return text;
}

// WL-INFO-05 -- agents/workshop/info-body.md
// WL-RGN-05 -- agents/workshop/regions.md
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
