// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_WINDOW_STORE_HPP
#define ZENGINE_GRAPHICS_WINDOW_STORE_HPP

// A window's areas: the one kept copy of what each owner drew -- every node with its place in the
// paint order, and every element record -- and the judge that admits a drawing into it, every bound
// checked before anything changes.
// Reference: graphics/docs/zengine.window.tui.md.

#include "graphics/ops.hpp"
#include "graphics/vocabulary.hpp"

#include <zen/weave/lifecycle.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace zengine::graphics::window {

/// A node kept, with its rank: the order it was first put in the area, kept when it is put again.
struct KeptNode {
    Node node;
    std::int64_t rank = 0;
    /// Version 3: its node draws by eleven operators.
    ZEN_SHAPE(KeptNode, 3, ZEN_FIELD(node), ZEN_FIELD(rank));
};

/// One area as its window keeps it: where the directory placed it, who draws in it and who placed
/// it, which of its key moments and whose hands reach it, whether the wheel stops there and its
/// arranger follows it, which palette it wears, and what was drawn there: each look its nodes wear
/// once, and the nodes, each naming its look there.
struct WinArea {
    std::string area;
    std::string owner;
    std::string arranger;
    std::int64_t x = 0, y = 0, w = 0, h = 0, order = 0;
    bool keys = false;
    bool pointer = false;
    bool weaver_only = false;
    bool wheel = true;
    bool right = true;
    bool followed = false;
    bool releases = false;
    bool text = false;
    std::string palette;
    std::int64_t next_rank = 1;
    std::vector<Look> looks;
    std::vector<KeptNode> nodes;
    std::vector<ElementRecord> records;
    /// The owner's number for the drawing it last sent (`Drawn::number`).
    std::int64_t drawing = 0;
    /// Version 13: it keeps no element the window presents, since a window presents none.
    ZEN_SHAPE(WinArea, 13, ZEN_FIELD(area), ZEN_FIELD(owner), ZEN_FIELD(arranger), ZEN_FIELD(x),
              ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h), ZEN_FIELD(order), ZEN_FIELD(keys),
              ZEN_FIELD(pointer), ZEN_FIELD(weaver_only), ZEN_FIELD(wheel), ZEN_FIELD(right),
              ZEN_FIELD(followed), ZEN_FIELD(releases), ZEN_FIELD(text), ZEN_FIELD(palette),
              ZEN_FIELD(next_rank), ZEN_FIELD(looks), ZEN_FIELD(nodes), ZEN_FIELD(records),
              ZEN_FIELD(drawing));
};

/// The look a kept node wears, among its area's, or its operator's default where it names none
/// (`look_or_default`); null for a group or an image.
inline const Look* look_of(const WinArea& a, const Node& n) {
    return look_or_default(n.draw, n.look >= 0 && static_cast<std::size_t>(n.look) < a.looks.size()
                                       ? &a.looks[static_cast<std::size_t>(n.look)]
                                       : nullptr);
}

/// The look a kept node wears, read as `L` (`Paint`, `Ink` or `TextLook`); null where it wears
/// another or none.
template <class L>
const L* worn(const WinArea& a, const Node& n) {
    return look_as<L>(look_of(a, n));
}

/// The record a node means: the nearest whose subtree holds it.
inline const ElementRecord* record_of(const WinArea& a, const std::string& path) {
    const ElementRecord* hit = nullptr;
    for (const ElementRecord& r : a.records) {
        if (!path.empty() && under(path, r.path) &&
            (hit == nullptr || r.path.size() > hit->path.size())) {
            hit = &r;
        }
    }
    return hit;
}

/// Each node's record, the nearest whose subtree holds it: what a press on that node reaches.
/// Nodes no record holds are left out.
inline std::map<std::string, const ElementRecord*> records_by_node(const WinArea& a) {
    std::map<std::string, const ElementRecord*> at;
    for (const ElementRecord& r : a.records) {
        if (!r.path.empty()) {
            at[r.path] = &r;
        }
    }
    std::map<std::string, const ElementRecord*> out;
    for (const KeptNode& k : a.nodes) {
        for (std::string p = k.node.path; !p.empty(); p = parent_of(p)) {
            const auto it = at.find(p);
            if (it != at.end()) {
                out[k.node.path] = it->second;
                break;
            }
        }
    }
    return out;
}

/// Whether the part a record names applies now, as the record says it (`PartFace`). A record of
/// another shape, or one that does not read as its shape, says nothing, and its part reads
/// available.
inline bool record_available(const ElementRecord& r) {
    if (r.type != loom::schema_of<PartFace>()->name()) {
        return true;
    }
    const std::optional<PartFace> face = loom::claim_item<PartFace>(r.data);
    return !face.has_value() || face->available;
}

/// Whether something drawn takes a press at `path`, the top node painted at a point: a record
/// naming an action.
inline bool takes_press(const WinArea& a, const std::string& path) {
    if (path.empty()) {
        return false;
    }
    const ElementRecord* r = record_of(a, path);
    return r != nullptr && !r->action.empty();
}

inline std::map<std::string, const KeptNode*> index_of(const WinArea& a) {
    std::map<std::string, const KeptNode*> out;
    for (const KeptNode& k : a.nodes) {
        out[k.node.path] = &k;
    }
    return out;
}

/// The key that orders a node among the area's: its ancestors' ranks, then its own.
inline std::vector<std::int64_t> key_of(const std::map<std::string, const KeptNode*>& at,
                                        const std::string& path) {
    std::vector<std::string> chain;
    for (std::string p = path; !p.empty(); p = parent_of(p)) {
        chain.push_back(p);
    }
    std::vector<std::int64_t> key;
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        const auto k = at.find(*it);
        key.push_back(k == at.end() ? 0 : k->second->rank);
    }
    return key;
}

/// Whether `path` lies in the subtree of a path `roots` holds: the path itself, or one of its
/// ancestors, each read up to a '/' of `path`. Bounded by the path's depth, not by `roots`.
inline bool under_any(const std::string& path, const std::set<std::string, std::less<>>& roots) {
    if (roots.empty()) {
        return false;
    }
    if (roots.count(path) != 0) {
        return true;
    }
    const std::string_view p(path);
    for (std::size_t at = p.find('/'); at != std::string_view::npos; at = p.find('/', at + 1)) {
        if (roots.count(p.substr(0, at)) != 0) {
            return true;
        }
    }
    return false;
}

/// What one node's operands hold, judged on their own: "" when whole, else why not -- a sheet
/// whose pixels are not `pw * ph * 4` bytes or wider or taller than `kMaxSheetSide`, cutting more
/// than `kMaxSheetFrames` frames, or with a frame outside its pixels or a character longer than
/// `kMaxFrameGlyphBytes`; tiles whose cells are not two bytes a tile; sprites or points whose bytes
/// are not whole sprites or points; points of no size or past `kMaxPointColors` colours; a sheet
/// named by an empty or overlong path.
inline std::string judge_operands(const Node& n) {
    const std::string at = "'" + n.path + "'";
    if (const auto* s = operands<SheetArgs>(n)) {
        if (s->pw < 0 || s->ph < 0 || s->pw > kMaxSheetSide || s->ph > kMaxSheetSide ||
            s->rgba.size() != static_cast<std::size_t>(s->pw * s->ph * 4)) {
            return at + ", a sheet, holds " + std::to_string(s->rgba.size()) +
                   " bytes of pixels for " + std::to_string(s->pw) + " by " +
                   std::to_string(s->ph) + ", four bytes a pixel, each side at most " +
                   std::to_string(kMaxSheetSide);
        }
        if (s->frames.size() > kMaxSheetFrames) {
            return at + ", a sheet, cuts more than " + std::to_string(kMaxSheetFrames) + " frames";
        }
        for (std::size_t i = 0; i < s->frames.size(); ++i) {
            const Frame& f = s->frames[i];
            const bool inside = f.x >= 0 && f.y >= 0 && f.w >= 0 && f.h >= 0 &&
                                f.x <= s->pw - f.w && f.y <= s->ph - f.h;
            if (!inside) {
                return at + ", a sheet: its frame " + std::to_string(i + 1) +
                       " lies outside its pixels";
            }
            if (f.glyph.size() > kMaxFrameGlyphBytes) {
                return at + ", a sheet: its frame " + std::to_string(i + 1) +
                       "'s character is longer than " + std::to_string(kMaxFrameGlyphBytes) +
                       " bytes";
            }
        }
    } else if (const auto* t = operands<TilesArgs>(n)) {
        const bool counted =
            t->cols >= 0 && t->rows >= 0 && t->tile_w >= 0 && t->tile_h >= 0 &&
            (t->cols == 0 ||
             static_cast<std::size_t>(t->rows) <= kMaxArgsBytes / static_cast<std::size_t>(t->cols));
        if (!counted ||
            t->cells.size() != static_cast<std::size_t>(t->cols * t->rows) * kTileBytes) {
            return at + ", tiles, holds " + std::to_string(t->cells.size()) + " bytes for " +
                   std::to_string(t->cols) + " by " + std::to_string(t->rows) +
                   " tiles, two bytes a tile";
        }
    } else if (const auto* sp = operands<SpritesArgs>(n)) {
        if (sp->sprites.size() % kSpriteBytes != 0) {
            return at + ", sprites, holds " + std::to_string(sp->sprites.size()) +
                   " bytes, which are not whole sprites of " + std::to_string(kSpriteBytes);
        }
    } else if (const auto* p = operands<PointsArgs>(n)) {
        if (p->size <= 0 || p->colors.size() > kMaxPointColors ||
            p->points.size() % kPointBytes != 0) {
            return at + ", points, holds " + std::to_string(p->points.size()) + " bytes in " +
                   std::to_string(p->colors.size()) + " colours at a size of " +
                   std::to_string(p->size) + ": whole points of " + std::to_string(kPointBytes) +
                   " bytes, at most " + std::to_string(kMaxPointColors) +
                   " colours, a size of 1 or more";
        }
    }
    if (const std::string* sheet = sheet_named(n.draw)) {
        if (sheet->empty() || sheet->size() > kMaxPathBytes) {
            return at + " names its sheet by an empty path or one longer than " +
                   std::to_string(kMaxPathBytes) + " bytes";
        }
    }
    return {};
}

/// Whether every node an area would hold after `d` that draws from a sheet names a sheet the area
/// would hold: "" when each does, else why not.
inline std::string judge_sheets(const WinArea& a, const Drawn& d,
                                const std::set<std::string>& present) {
    std::map<std::string, const Operands*> now;
    for (const KeptNode& k : a.nodes) {
        if (present.count(k.node.path) != 0) {
            now[k.node.path] = &k.node.draw;
        }
    }
    for (const Node& n : d.put) {
        now[n.path] = &n.draw;
    }
    for (const auto& [path, draw] : now) {
        const std::string* sheet = sheet_named(*draw);
        if (sheet == nullptr) {
            continue;
        }
        const auto it = now.find(*sheet);
        if (it == now.end()) {
            return "'" + path + "' draws from the sheet '" + *sheet + "', which is not drawn";
        }
        if (!std::holds_alternative<SheetArgs>(it->second->is)) {
            return "'" + path + "' draws from '" + *sheet + "', which is drawn as " +
                   std::string(Operands::zen_case_names[it->second->is.index()]) +
                   ", not as a sheet";
        }
    }
    return {};
}

/// The path a node reads besides its own: the sheet it draws from, else its own path.
inline const std::string& read_from(const Node& n) {
    const std::string* sheet = sheet_named(n.draw);
    return sheet != nullptr ? *sheet : n.path;
}

/// The operands a kept node's colours are read from: the sheet it draws from, where the area holds
/// it, else its own.
inline const Operands& colours_of(const std::map<std::string, const KeptNode*>& at, const Node& n) {
    const auto from = at.find(read_from(n));
    return from != at.end() ? from->second->node.draw : n.draw;
}

/// THE NODES A PAINTER IS HANDED to paint `paths` of an area (`at`, its nodes by path): each with
/// every ancestor the area holds, a node drawn in their space (`send`), and the sheet it draws
/// from, to be read and painted nothing where it is not among `paths` (`read`).
struct ToPaint {
    std::set<std::string> send;
    std::set<std::string> read;
};
inline ToPaint to_paint(const std::map<std::string, const KeptNode*>& at,
                        const std::set<std::string>& paths) {
    ToPaint out;
    for (const std::string& p : paths) {
        const auto k = at.find(p);
        if (k == at.end()) {
            continue;
        }
        out.send.insert(p);
        for (std::string q = parent_of(p); !q.empty(); q = parent_of(q)) {
            if (at.count(q) != 0) {
                out.send.insert(q);
            }
        }
        const std::string& sheet = read_from(k->second->node);
        if (sheet != p && at.count(sheet) != 0) {
            out.read.insert(sheet);
            out.send.insert(sheet);
        }
    }
    return out;
}

/// Every bound and rule, judged before anything changes: "" when admitted, else why not. A record
/// said again at its path counts as itself, so an area at its bound still takes a change.
inline std::string judge(const WinArea& a, const Drawn& d) {
    std::set<std::string> present;
    if (!d.whole) {
        for (const KeptNode& k : a.nodes) {
            present.insert(k.node.path);
        }
    }
    // EACH PATH THE AREA WOULD HOLD ASKS ONCE whether it or one of its ancestors was removed.
    if (!d.removed.empty()) {
        const std::set<std::string, std::less<>> removed(d.removed.begin(), d.removed.end());
        std::erase_if(present, [&](const std::string& p) { return under_any(p, removed); });
    }
    if (d.looks.size() > kMaxNodesPerArea) {
        return "a drawing says more than " + std::to_string(kMaxNodesPerArea) + " looks";
    }
    for (const Node& n : d.put) {
        if (n.path.empty() || n.path.size() > kMaxPathBytes ||
            static_cast<std::size_t>(std::count(n.path.begin(), n.path.end(), '/')) >=
                kMaxPathDepth) {
            return "a path is empty, longer than " + std::to_string(kMaxPathBytes) +
                   " bytes or deeper than " + std::to_string(kMaxPathDepth);
        }
        if (operand_bytes(n.draw) > kMaxArgsBytes) {
            return "'" + n.path + "' has operands over " + std::to_string(kMaxArgsBytes) +
                   " bytes";
        }
        if (n.look != kNoLook &&
            (n.look < 0 || static_cast<std::size_t>(n.look) >= d.looks.size())) {
            return "'" + n.path + "' wears look " + std::to_string(n.look) +
                   ", which its drawing does not say";
        }
        const Look* look = n.look == kNoLook ? nullptr : &d.looks[static_cast<std::size_t>(n.look)];
        if (!wears(n.draw, look)) {
            const std::string op(operator_of(n));
            const std::string an = op.find_first_of("aeiou") == 0 ? "an " : "a ";
            return "'" + n.path + "', " + an + op + ", wears a look of " +
                   std::string(Look::zen_case_names[look->is.index()]) + ", which " + an + op +
                   " does not";
        }
        const std::string parent = parent_of(n.path);
        if (!parent.empty() && present.count(parent) == 0) {
            return "'" + n.path + "' is drawn in '" + parent + "', which is not drawn";
        }
        if (std::string why = judge_operands(n); !why.empty()) {
            return why;
        }
        present.insert(n.path);
    }
    if (present.size() > kMaxNodesPerArea) {
        return "area '" + a.area + "' would hold more than " + std::to_string(kMaxNodesPerArea) +
               " nodes";
    }
    // EVERY NODE THAT DRAWS FROM A SHEET NAMES ONE THE AREA WOULD HOLD, kept or put now.
    if (std::string why = judge_sheets(a, d, present); !why.empty()) {
        return why;
    }
    std::set<std::string> records;
    if (!d.whole) {
        for (const ElementRecord& r : a.records) {
            if (present.count(r.path) != 0) {
                records.insert(r.path);
            }
        }
    }
    for (const std::string& u : d.unsaid) {
        records.erase(u);
    }
    for (const ElementRecord& r : d.said) {
        if (present.count(r.path) == 0) {
            return "record '" + r.path + "' has no node at its path";
        }
        records.insert(r.path);
    }
    if (records.size() > kMaxElementsPerArea) {
        return "area '" + a.area + "' would hold more than " +
               std::to_string(kMaxElementsPerArea) + " elements";
    }
    return {};
}

/// What a drawing changed: the paths put (and every node beneath a put group, whose space
/// moved), and every path that went.
struct Applied {
    std::set<std::string> changed;
    std::vector<std::string> gone;
};

/// Drops from an area the looks no node wears, each node naming its look where it now stands.
inline void keep_worn_looks(WinArea& a) {
    std::vector<std::int64_t> to(a.looks.size(), kNoLook);
    for (const KeptNode& k : a.nodes) {
        if (k.node.look >= 0 && static_cast<std::size_t>(k.node.look) < to.size()) {
            to[static_cast<std::size_t>(k.node.look)] = 0;
        }
    }
    std::vector<Look> kept;
    for (std::size_t i = 0; i < a.looks.size(); ++i) {
        if (to[i] != kNoLook) {
            to[i] = static_cast<std::int64_t>(kept.size());
            kept.push_back(std::move(a.looks[i]));
        }
    }
    a.looks = std::move(kept);
    for (KeptNode& k : a.nodes) {
        if (k.node.look >= 0) {
            k.node.look = to[static_cast<std::size_t>(k.node.look)];
        }
    }
}

inline Applied apply(WinArea& a, const Drawn& d) {
    Applied out;
    // EACH LOOK THE DRAWING SAYS, ONCE IN THE AREA: a node is kept naming the area's look.
    // Each found in one step, the area's looks indexed once.
    std::unordered_map<Look, std::int64_t, LookHash> place_of;
    place_of.reserve(a.looks.size() + d.looks.size());
    for (std::size_t i = 0; i < a.looks.size(); ++i) {
        place_of.try_emplace(a.looks[i], static_cast<std::int64_t>(i));
    }
    std::vector<std::int64_t> look_in_area(d.looks.size(), kNoLook);
    for (std::size_t i = 0; i < d.looks.size(); ++i) {
        const auto [it, fresh] =
            place_of.try_emplace(d.looks[i], static_cast<std::int64_t>(a.looks.size()));
        if (fresh) {
            a.looks.push_back(d.looks[i]);
        }
        look_in_area[i] = it->second;
    }
    const auto kept = [&](const Node& n) {
        Node k = n;
        k.look = n.look == kNoLook ? kNoLook : look_in_area[static_cast<std::size_t>(n.look)];
        return k;
    };
    std::set<std::string> before;
    for (const KeptNode& k : a.nodes) {
        before.insert(k.node.path);
    }
    if (d.whole) {
        a.nodes.clear();
        a.records.clear();
    }
    if (!d.removed.empty()) {
        const std::set<std::string, std::less<>> removed(d.removed.begin(), d.removed.end());
        std::erase_if(a.nodes, [&](const KeptNode& k) { return under_any(k.node.path, removed); });
        std::erase_if(a.records, [&](const ElementRecord& e) { return under_any(e.path, removed); });
    }
    // EACH PUT FINDS ITS NODE IN ONE PASS OVER THE AREA, the puts indexed by path and the later of
    // a path put twice winning: a node put again keeps its rank, and a new path takes the next, in
    // the order it was first put.
    std::unordered_map<std::string_view, std::size_t> last;
    last.reserve(d.put.size());
    for (std::size_t i = 0; i < d.put.size(); ++i) {
        last.insert_or_assign(std::string_view(d.put[i].path), i);
        out.changed.insert(d.put[i].path);
    }
    std::vector<bool> held(d.put.size(), false);
    if (!last.empty()) {
        for (KeptNode& k : a.nodes) {
            const auto it = last.find(k.node.path);
            if (it != last.end()) {
                k.node = kept(d.put[it->second]);
                held[it->second] = true;
            }
        }
    }
    for (const Node& n : d.put) {
        const std::size_t later = last.at(n.path);
        if (!held[later]) {
            held[later] = true;
            a.nodes.push_back(KeptNode{kept(d.put[later]), a.next_rank++});
        }
    }
    for (const ElementRecord& r : d.said) {
        const auto it = std::find_if(a.records.begin(), a.records.end(),
                                     [&](const ElementRecord& k) { return k.path == r.path; });
        if (it != a.records.end()) {
            *it = r;
        } else {
            a.records.push_back(r);
        }
    }
    for (const std::string& u : d.unsaid) {
        std::erase_if(a.records, [&](const ElementRecord& r) { return r.path == u; });
    }
    keep_worn_looks(a);
    const auto at = index_of(a);
    std::erase_if(a.records, [&](const ElementRecord& r) { return at.count(r.path) == 0; });
    // EVERY NODE BENEATH A PUT GROUP CHANGED TOO, its space moved: each node asks of its own
    // ancestors whether one was put.
    const std::set<std::string, std::less<>> put(out.changed.begin(), out.changed.end());
    for (const KeptNode& k : a.nodes) {
        if (under_any(k.node.path, put)) {
            out.changed.insert(k.node.path);
        }
    }
    // ...AND EVERY NODE THAT DRAWS FROM A SHEET PUT NOW, which it reads.
    for (const KeptNode& k : a.nodes) {
        const std::string* sheet = sheet_named(k.node.draw);
        if (sheet != nullptr && out.changed.count(*sheet) != 0) {
            out.changed.insert(k.node.path);
        }
    }
    for (const std::string& p : before) {
        if (at.count(p) == 0) {
            out.gone.push_back(p);
        }
    }
    return out;
}

} // namespace zengine::graphics::window

#endif // ZENGINE_GRAPHICS_WINDOW_STORE_HPP
