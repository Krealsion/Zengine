// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The loadable weave `zengine-graphics`, the directory: which windows exist, where each area
// stands and who draws in it, which skin paints it, and which code presents each element type. It
// introduces an area to its window once the window and the area's skin are known, and the window
// tells the owner its room; it is told again of every change, and it stays off the path a picture
// takes.
// Reference: graphics/docs/zengine.graphics.md, its manual, compiled in and answered.

#include "graphics/vocabulary.hpp"

#include "activation/activation.hpp"
#include "manual/vocabulary.hpp"
#include "manuals/zengine.graphics.hpp" // this office's manual, compiled in from its page

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace {

namespace gfx = zengine::graphics;

/// An area as the directory keeps it: where it was placed, and the skin chosen for it.
struct DirectoryArea {
    gfx::AreaPlaced placed;
    std::string skin;
    ZEN_SHAPE(DirectoryArea, 2, ZEN_FIELD(placed), ZEN_FIELD(skin));
};

struct DirectoryState {
    std::vector<gfx::WindowOffered> windows;
    std::vector<DirectoryArea> areas;
    std::vector<gfx::SkinOffered> skins;
    std::vector<gfx::ElementCodeOffered> codes;
    ZEN_SHAPE(DirectoryState, 2, ZEN_FIELD(windows), ZEN_FIELD(areas), ZEN_FIELD(skins),
              ZEN_FIELD(codes));
};

class Directory
    : public zengine::With<
          zengine::manual::ManualFacet<zengine::manual::pages::zengine_graphics::kManual>>::
          Weave<Directory, DirectoryState,
                loom::Accept<gfx::WindowOffered, gfx::AreaPlaced, gfx::SkinOffered, gfx::SkinChosen,
                             gfx::ElementCodeOffered, gfx::RoomAsked, loom::Activated>,
                loom::Emit<gfx::AreaIntroduced, gfx::AreaWithdrawn, gfx::ElementCodeOffered,
                           gfx::GraphicsReady>> {
public:
    /// A window, as its own office: every code offered, then each of its areas introduced, which
    /// the window answers with each owner's room -- in that order, so an element a fresh window is
    /// asked to present finds its code there.
    void on(const gfx::WindowOffered& w, loom::Mail& mail) {
        if (!mail.authored_from_role(std::string(gfx::kWindowOfficePrefix) + w.window)) {
            return;
        }
        auto it = std::find_if(state_.windows.begin(), state_.windows.end(),
                               [&](const gfx::WindowOffered& k) { return k.window == w.window; });
        if (it == state_.windows.end()) {
            state_.windows.push_back(w);
        } else {
            *it = w;
        }
        for (const gfx::ElementCodeOffered& c : state_.codes) {
            (void)mail.as_role(gfx::kGraphicsRole).send_to_role(office_of(w.window), c);
        }
        for (const DirectoryArea& a : state_.areas) {
            if (a.placed.window == w.window) {
                introduce(mail, a);
            }
        }
    }

    /// The arranger: where an area stands. Moved to another window, it leaves the one it was in.
    void on(const gfx::AreaPlaced& p, loom::Mail& mail) {
        DirectoryArea* a = find(p.area);
        if (a == nullptr) {
            state_.areas.push_back(DirectoryArea{p, {}});
            a = &state_.areas.back();
        } else {
            if (a->placed.window != p.window && window_of(a->placed.window) != nullptr) {
                (void)mail.as_role(gfx::kGraphicsRole)
                    .send_to_role(office_of(a->placed.window), gfx::AreaWithdrawn{p.area});
            }
            a->placed = p;
        }
        introduce(mail, *a);
    }

    void on(const gfx::SkinOffered& s, loom::Mail& mail) {
        auto it = std::find_if(state_.skins.begin(), state_.skins.end(),
                               [&](const gfx::SkinOffered& k) { return k.skin == s.skin; });
        if (it != state_.skins.end() && it->library == s.library) {
            return;
        }
        if (it == state_.skins.end()) {
            state_.skins.push_back(s);
        } else {
            *it = s;
        }
        for (const DirectoryArea& a : state_.areas) {
            if (a.skin == s.skin) {
                introduce(mail, a);
            }
        }
    }

    void on(const gfx::SkinChosen& c, loom::Mail& mail) {
        DirectoryArea* a = find(c.area);
        if (a == nullptr || a->skin == c.skin) {
            return;
        }
        a->skin = c.skin;
        introduce(mail, *a);
    }

    void on(const gfx::ElementCodeOffered& c, loom::Mail& mail) {
        auto it = std::find_if(state_.codes.begin(), state_.codes.end(),
                               [&](const gfx::ElementCodeOffered& k) { return k.type == c.type; });
        if (it != state_.codes.end() && it->library == c.library) {
            return;
        }
        if (it == state_.codes.end()) {
            state_.codes.push_back(c);
        } else {
            *it = c;
        }
        for (const gfx::WindowOffered& w : state_.windows) {
            (void)mail.as_role(gfx::kGraphicsRole).send_to_role(office_of(w.window), c);
        }
    }

    /// An owner that started again, as its office: the area introduced again, so the window that
    /// shows it answers with the room.
    void on(const gfx::RoomAsked& r, loom::Mail& mail) {
        const DirectoryArea* a = find(r.area);
        if (a != nullptr && mail.authored_from_role(a->placed.owner)) {
            introduce(mail, *a);
        }
    }

    /// Started afresh or again: everyone offers, places and chooses again.
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (activation_.accept(mail, a)) {
            (void)mail.as_role(gfx::kGraphicsRole).publish(gfx::GraphicsReady{});
        }
    }

private:
    static std::string office_of(const std::string& window) {
        return std::string(gfx::kWindowOfficePrefix) + window;
    }

    DirectoryArea* find(const std::string& area) {
        for (DirectoryArea& a : state_.areas) {
            if (a.placed.area == area) {
                return &a;
            }
        }
        return nullptr;
    }

    const gfx::WindowOffered* window_of(const std::string& window) const {
        for (const gfx::WindowOffered& w : state_.windows) {
            if (w.window == window) {
                return &w;
            }
        }
        return nullptr;
    }

    const gfx::SkinOffered* skin_of(const std::string& skin) const {
        for (const gfx::SkinOffered& s : state_.skins) {
            if (s.skin == skin) {
                return &s;
            }
        }
        return nullptr;
    }

    /// The area to its window, once the window and the area's skin are known: never an
    /// introduction a look would have to be changed after.
    void introduce(loom::Mail& mail, const DirectoryArea& a) {
        const gfx::SkinOffered* skin = skin_of(a.skin);
        if (window_of(a.placed.window) == nullptr || skin == nullptr) {
            return;
        }
        const gfx::AreaPlaced& p = a.placed;
        (void)mail.as_role(gfx::kGraphicsRole)
            .send_to_role(office_of(p.window),
                          gfx::AreaIntroduced{p.area, p.owner, p.x, p.y, p.w, p.h, p.order,
                                              p.keys, p.pointer, skin->skin, skin->library});
    }

    zengine::ActivationCursor activation_;
};

} // namespace

ZEN_EXPORT_WEAVE(Directory)
