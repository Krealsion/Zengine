// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// tile-defense-stage, the game's arranger: the first window that offers itself is the game's, its
// whole room the game's area (the picture's own size where the window measures none), hearing its
// keys and its pointer, in the night palette; and that
// window asked once each life to take the game's title and the size its picture wants -- which a
// GUI window takes and a terminal keeps as its own. Reference: examples/tile-defense/README.md.

#include "picture.hpp"
#include "vocabulary.hpp"

#include "activation/activation.hpp"
#include "graphics/themes.hpp" // generated: the shipped themes, as palettes
#include "graphics/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <string>

namespace {

using namespace zengine::tile_defense;

/// The palette the game's area wears, of the shipped themes.
inline gfx::PaletteOffered palette() { return gfx::themes::night(); }

/// What the stage keeps across a reload: the window it chose, its last offer, and the life of it
/// the stage shaped.
struct TileStageState {
    std::string window;
    gfx::WindowOffered offered;
    std::int64_t shaped_epoch = 0;
    ZEN_SHAPE(TileStageState, 1, ZEN_FIELD(window), ZEN_FIELD(offered), ZEN_FIELD(shaped_epoch));
};

class TileDefenseStage
    : public loom::WeaveBase<
          TileDefenseStage, TileStageState,
          loom::Accept<loom::Activated, gfx::GraphicsReady, gfx::WindowOffered, gfx::AreaSettled>,
          loom::Emit<gfx::AreaPlaced, gfx::PaletteOffered, gfx::PaletteChosen, gfx::WindowShaped>> {
public:
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (activation_.accept(mail, a)) {
            arrange(mail);
        }
    }

    /// A graphics directory started, afresh or again: everything said to it again.
    void on(const gfx::GraphicsReady&, loom::Mail& mail) {
        if (mail.authored_from_role(gfx::kGraphicsRole)) {
            arrange(mail);
        }
    }

    /// A window offered itself, as its role: the first is the game's, and each offer of it -- a
    /// new life, a new size -- places the game in its whole room again.
    void on(const gfx::WindowOffered& w, loom::Mail& mail) {
        if (!mail.authored_from_role(std::string(gfx::kWindowRolePrefix) + w.window) ||
            (!state_.window.empty() && w.window != state_.window)) {
            return;
        }
        state_.window = w.window;
        state_.offered = w;
        arrange(mail);
    }

    /// What the game's area shows settled: the window holds this stage as the area's arranger now,
    /// so it is told the game's title and size, once each life of it.
    void on(const gfx::AreaSettled& s, loom::Mail& mail) {
        if (s.area != kGameArea || s.window != state_.window || state_.offered.epoch == 0 ||
            state_.shaped_epoch == state_.offered.epoch ||
            !mail.authored_from_role(std::string(gfx::kWindowRolePrefix) + s.window)) {
            return;
        }
        state_.shaped_epoch = state_.offered.epoch;
        const Layout l = Layout::of(state_.offered.cell_px,
                                    gfx::TextMeasure{state_.offered.text_advance_px, state_.offered.text_line_px});
        (void)mail.as_role(kStageRole)
            .send_to_role(std::string(gfx::kWindowRolePrefix) + state_.window,
                          gfx::WindowShaped{state_.window, "Tile defense", l.width(),
                                                  l.height()});
    }

private:
    /// The palette offered and chosen, and the game's area placed in the window's whole room.
    void arrange(loom::Mail& mail) {
        (void)mail.as_role(kStageRole).send_to_role(gfx::kGraphicsRole, palette());
        (void)mail.as_role(kStageRole)
            .send_to_role(gfx::kGraphicsRole, gfx::PaletteChosen{kGameArea, palette().palette});
        if (state_.window.empty()) {
            return;
        }
        const gfx::WindowOffered& w = state_.offered;
        const Layout l = Layout::of(w.cell_px, gfx::TextMeasure{w.text_advance_px, w.text_line_px});
        gfx::AreaPlaced placed{kGameArea, state_.window, kGameRole, 0, 0,
                               w.width > 0 ? w.width : l.width(), w.height > 0 ? w.height : l.height(),
                               0, true, true};
        placed.followed = true;
        (void)mail.as_role(kStageRole).send_to_role(gfx::kGraphicsRole, placed);
    }

    zengine::ActivationCursor activation_;
};

} // namespace

ZEN_EXPORT_WEAVE(TileDefenseStage)
