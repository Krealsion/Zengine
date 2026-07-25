// The block TUI Skin — the terminal medium wearing the block style. A
// different library on purpose: swapping it in mid-game is the phase's
// drawing-replaced moment, and the difference must be unmistakable.

#include "skin_tui.hpp"

#include <zen/kernel/export.hpp>

namespace {

using SkinTuiBlock = zengine::surface::SkinT<
    zengine::surface::TuiMedium<zengine::surface::BlockStyle, zengine::surface::TuiTerminal>>;

} // namespace

ZEN_EXPORT_WEAVE(SkinTuiBlock)
