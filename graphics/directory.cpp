// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The loadable weave `zengine-graphics`, the directory: which windows exist, where each area
// stands and who draws in it, which palette it wears, the palettes offered, and which code presents
// each element type. It introduces an area to its window once the window is known, and the window
// tells the owner its room; it is told again of every change, passes every palette to every window,
// and stays off the path a picture takes.
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

/// An area as the directory keeps it: where it was placed and by which office, and the palette
/// chosen for it.
struct DirectoryArea {
    gfx::AreaPlaced placed;
    std::string arranger;
    std::string palette;
    ZEN_SHAPE(DirectoryArea, 8, ZEN_FIELD(placed), ZEN_FIELD(arranger), ZEN_FIELD(palette));
};

struct DirectoryState {
    std::vector<gfx::WindowOffered> windows;
    std::vector<DirectoryArea> areas;
    std::vector<gfx::PaletteOffered> palettes;
    std::vector<gfx::ElementCodeOffered> codes;
    ZEN_SHAPE(DirectoryState, 11, ZEN_FIELD(windows), ZEN_FIELD(areas), ZEN_FIELD(palettes),
              ZEN_FIELD(codes));
};

class Directory
    : public zengine::With<
          zengine::manual::ManualFacet<zengine::manual::pages::zengine_graphics::kManual>>::
          Weave<Directory, DirectoryState,
                loom::Accept<gfx::WindowOffered, gfx::AreaPlaced, gfx::PaletteOffered,
                             gfx::PaletteChosen, gfx::ElementCodeOffered, gfx::RoomAsked,
                             loom::Activated>,
                loom::Emit<gfx::AreaIntroduced, gfx::AreaWithdrawn, gfx::ElementCodeOffered,
                           gfx::PaletteOffered, gfx::GraphicsReady>> {
public:
    /// A window, as its own office: every code and palette offered, then each of its areas
    /// introduced, which the window answers with each owner's room -- in that order, so an element
    /// a fresh window is asked to present finds its code there, and an area its palette.
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
        for (const gfx::PaletteOffered& p : state_.palettes) {
            (void)mail.as_role(gfx::kGraphicsRole).send_to_role(office_of(w.window), p);
        }
        for (const DirectoryArea& a : state_.areas) {
            if (a.placed.window == w.window) {
                introduce(mail, a);
            }
        }
    }

    /// The arranger: where an area stands, kept with the office the placing was authored as, which
    /// the window names the area's arranger. Moved to another window, it leaves the one it was in.
    void on(const gfx::AreaPlaced& p, loom::Mail& mail) {
        DirectoryArea* a = find(p.area);
        if (a == nullptr) {
            state_.areas.push_back(DirectoryArea{p, {}, {}});
            a = &state_.areas.back();
        } else {
            if (a->placed.window != p.window && window_of(a->placed.window) != nullptr) {
                (void)mail.as_role(gfx::kGraphicsRole)
                    .send_to_role(office_of(a->placed.window), gfx::AreaWithdrawn{p.area});
            }
            a->placed = p;
        }
        a->arranger = std::string(mail.authored_role());
        introduce(mail, *a);
    }

    /// A palette, kept at its latest revision and passed to every window, which paints every area
    /// wearing it again. One offered past the directory's bound is not kept, and no window hears it.
    void on(const gfx::PaletteOffered& p, loom::Mail& mail) {
        auto it = std::find_if(state_.palettes.begin(), state_.palettes.end(),
                               [&](const gfx::PaletteOffered& k) { return k.palette == p.palette; });
        if (it != state_.palettes.end() && it->revision >= p.revision) {
            return;
        }
        if (it == state_.palettes.end()) {
            if (state_.palettes.size() >= gfx::kMaxPalettes) {
                return;
            }
            state_.palettes.push_back(p);
        } else {
            *it = p;
        }
        for (const gfx::WindowOffered& w : state_.windows) {
            (void)mail.as_role(gfx::kGraphicsRole).send_to_role(office_of(w.window), p);
        }
    }

    /// Which palette an area wears: kept for an area not placed yet, so an arranger that chooses
    /// before it places has its area introduced once, wearing it.
    void on(const gfx::PaletteChosen& c, loom::Mail& mail) {
        DirectoryArea* a = find(c.area);
        if (a == nullptr) {
            if (state_.areas.size() < gfx::kMaxAreas) {
                gfx::AreaPlaced unplaced;
                unplaced.area = c.area;
                state_.areas.push_back(DirectoryArea{unplaced, {}, c.palette});
            }
            return;
        }
        if (a->palette == c.palette) {
            return;
        }
        a->palette = c.palette;
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

    /// The area to its window, once the window is known, wearing the palette chosen for it: one
    /// the window holds none of yet paints in the window's own until it is offered.
    void introduce(loom::Mail& mail, const DirectoryArea& a) {
        if (window_of(a.placed.window) == nullptr) {
            return;
        }
        const gfx::AreaPlaced& p = a.placed;
        (void)mail.as_role(gfx::kGraphicsRole)
            .send_to_role(office_of(p.window),
                          gfx::AreaIntroduced{p.area, p.owner, a.arranger, p.x, p.y, p.w, p.h,
                                              p.order, p.keys, p.pointer, a.palette,
                                              p.weaver_only, p.wheel, p.right, p.followed,
                                              p.releases, p.text});
    }

    zengine::ActivationCursor activation_;
};

} // namespace

ZEN_EXPORT_WEAVE(Directory)
