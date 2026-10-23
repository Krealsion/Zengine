// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_WINDOW_STORE_HPP
#define ZENGINE_GRAPHICS_WINDOW_STORE_HPP

// A window's areas: the one kept copy of what each owner drew -- every node with its place in the
// paint order, every element record, and each element the window presents with its state -- and
// the judge that admits a drawing into it, every bound checked before anything changes.
// Reference: graphics/docs/zengine.window.terminal.md.

#include "graphics/vocabulary.hpp"

#include <zen/weave/lifecycle.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace zengine::graphics::window {

/// A node kept, with its rank: the order it was first put in the area, kept when it is put again.
struct KeptNode {
    Node node;
    std::int64_t rank = 0;
    ZEN_SHAPE(KeptNode, 1, ZEN_FIELD(node), ZEN_FIELD(rank));
};

/// An element the window presents: where, of which type, its drawer's reference for it, the
/// window's own handle for it, and its state, which the window keeps for the element's code.
struct Presented {
    std::string path;
    std::string type;
    std::int64_t ref = 0;
    std::int64_t handle = 0;
    loom::Bytes state;
    ZEN_SHAPE(Presented, 1, ZEN_FIELD(path), ZEN_FIELD(type), ZEN_FIELD(ref), ZEN_FIELD(handle),
              ZEN_FIELD(state));
};

/// One area as its window keeps it: where the directory placed it, who draws in it and who placed
/// it, whose hands reach it and whether the wheel stops there, which skin paints it, and what was
/// drawn there.
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
    std::string skin;
    std::string library;
    std::int64_t next_rank = 1;
    std::vector<KeptNode> nodes;
    std::vector<ElementRecord> records;
    std::vector<Presented> presented;
    /// The owner's number for the drawing it last sent (`Drawn::number`).
    std::int64_t drawing = 0;
    ZEN_SHAPE(WinArea, 7, ZEN_FIELD(area), ZEN_FIELD(owner), ZEN_FIELD(arranger), ZEN_FIELD(x),
              ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h), ZEN_FIELD(order), ZEN_FIELD(keys),
              ZEN_FIELD(pointer), ZEN_FIELD(weaver_only), ZEN_FIELD(wheel), ZEN_FIELD(right),
              ZEN_FIELD(skin),
              ZEN_FIELD(library),
              ZEN_FIELD(next_rank), ZEN_FIELD(nodes), ZEN_FIELD(records), ZEN_FIELD(presented),
              ZEN_FIELD(drawing));
};

/// Who is drawing: the owner (everything but what the window presents), or an element's code
/// (its own subtree, `scope`).
struct Drawer {
    bool element = false;
    std::string scope;
};

/// Whether `path` is the drawer's to draw.
inline bool mine(const WinArea& a, const Drawer& d, const std::string& path) {
    if (d.element) {
        return under(path, d.scope);
    }
    for (const Presented& p : a.presented) {
        if (under(path, p.path)) {
            return false;
        }
    }
    return true;
}

/// The element presented whose subtree holds a path, the nearest; nullptr when none does.
inline Presented* presented_at(WinArea& a, const std::string& path) {
    Presented* hit = nullptr;
    for (Presented& p : a.presented) {
        if (!path.empty() && under(path, p.path) &&
            (hit == nullptr || p.path.size() > hit->path.size())) {
            hit = &p;
        }
    }
    return hit;
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

/// Whether something drawn takes a press at `path`, the top node painted at a point: an element
/// presented over it, or a record naming an action.
inline bool takes_press(const WinArea& a, const std::string& path) {
    if (path.empty()) {
        return false;
    }
    for (const Presented& p : a.presented) {
        if (under(path, p.path)) {
            return true;
        }
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

/// Every bound and rule, judged before anything changes: "" when admitted, else why not. A record
/// said again at its path counts as itself, so an area at its bound still takes a change.
inline std::string judge(const WinArea& a, const Drawer& who, const Drawn& d) {
    std::set<std::string> present;
    for (const KeptNode& k : a.nodes) {
        if (!(d.whole && mine(a, who, k.node.path))) {
            present.insert(k.node.path);
        }
    }
    for (const std::string& r : d.removed) {
        if (!mine(a, who, r)) {
            return "'" + r + "' is not the sender's to remove";
        }
        for (auto it = present.begin(); it != present.end();) {
            it = under(*it, r) ? present.erase(it) : std::next(it);
        }
    }
    for (const Node& n : d.put) {
        if (n.path.empty() || n.path.size() > kMaxPathBytes ||
            static_cast<std::size_t>(std::count(n.path.begin(), n.path.end(), '/')) >=
                kMaxPathDepth) {
            return "a path is empty, longer than " + std::to_string(kMaxPathBytes) +
                   " bytes or deeper than " + std::to_string(kMaxPathDepth);
        }
        if (n.args.size() > kMaxArgsBytes) {
            return "'" + n.path + "' has operands over " + std::to_string(kMaxArgsBytes) +
                   " bytes";
        }
        if (!mine(a, who, n.path)) {
            return "'" + n.path + "' is not the sender's to draw";
        }
        const std::string parent = parent_of(n.path);
        if (!parent.empty() && present.count(parent) == 0) {
            return "'" + n.path + "' is drawn in '" + parent + "', which is not drawn";
        }
        present.insert(n.path);
    }
    if (present.size() > kMaxNodesPerArea) {
        return "area '" + a.area + "' would hold more than " + std::to_string(kMaxNodesPerArea) +
               " nodes";
    }
    std::set<std::string> records;
    for (const ElementRecord& r : a.records) {
        if (present.count(r.path) != 0 && !(d.whole && mine(a, who, r.path))) {
            records.insert(r.path);
        }
    }
    for (const std::string& u : d.unsaid) {
        records.erase(u);
    }
    for (const ElementRecord& r : d.said) {
        if (present.count(r.path) == 0) {
            return "record '" + r.path + "' has no node at its path";
        }
        if (!mine(a, who, r.path)) {
            return "record '" + r.path + "' is not the sender's to say";
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

inline Applied apply(WinArea& a, const Drawer& who, const Drawn& d) {
    Applied out;
    std::set<std::string> before;
    for (const KeptNode& k : a.nodes) {
        before.insert(k.node.path);
    }
    if (d.whole) {
        std::erase_if(a.nodes, [&](const KeptNode& k) { return mine(a, who, k.node.path); });
        std::erase_if(a.records, [&](const ElementRecord& r) { return mine(a, who, r.path); });
    }
    for (const std::string& r : d.removed) {
        std::erase_if(a.nodes, [&](const KeptNode& k) { return under(k.node.path, r); });
        std::erase_if(a.records, [&](const ElementRecord& e) { return under(e.path, r); });
    }
    for (const Node& n : d.put) {
        const auto it = std::find_if(a.nodes.begin(), a.nodes.end(),
                                     [&](const KeptNode& k) { return k.node.path == n.path; });
        if (it != a.nodes.end()) {
            it->node = n;
        } else {
            a.nodes.push_back(KeptNode{n, a.next_rank++});
        }
        out.changed.insert(n.path);
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
    const auto at = index_of(a);
    std::erase_if(a.records, [&](const ElementRecord& r) { return at.count(r.path) == 0; });
    for (const std::string& c : std::set<std::string>(out.changed)) {
        for (const KeptNode& k : a.nodes) {
            if (under(k.node.path, c)) {
                out.changed.insert(k.node.path);
            }
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
