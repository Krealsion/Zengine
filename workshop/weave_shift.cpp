// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// Double Shift: Shift tapped twice, alone and quickly, recognized per hand from Shift's presses
// and the release between them, and answered as the key `double-shift` wherever a row in force
// answers to it. A held Shift repeats as presses with no release, so the release is what tells a
// tap from a hold. Workshop law: agents/workshop/keyboard.md (+1 register).

#include "weave.hpp"

namespace zengine::workshop {

// WL-KEY-19 -- agents/workshop/keyboard.md
bool WorkshopWeave::shift_tapped(const input::KeyPressed& k, loom::Mail& mail) {
    Hand& h = hand();
    const std::int64_t now = interaction_now();
    // A PRESS WHILE THIS HAND'S SHIFT IS HELD is the key repeating: that hold is no tap, until
    // its release, and the pair is lost.
    if (h.shift_tap.held) {
        h.shift_tap = {};
        h.shift_tap.held = true;
        h.shift_tap.repeated = true;
        return false;
    }
    const bool alone = (k.modifiers & ~input::mod::kShift) == 0;
    const bool second = h.shift_tap.tapped && alone && now >= h.shift_tap.first_ms &&
                        now - h.shift_tap.first_ms <= kDoubleClickMs;
    h.shift_tap = {};
    if (!second) {
        if (alone) {
            h.shift_tap.held = true;
            h.shift_tap.first_ms = now;
        }
        return false;
    }
    // THE PAIR: answered by the key path as `double-shift`, where a row in force answers to it;
    // otherwise this press goes on as every Shift press does.
    const KeyContext ctx = keyboard_context(session_);
    const std::int64_t pane = keyboard_pane();
    const bool answered =
        session_.keymap.app_action_for(app_precedence::kAboveModes, ctx, kDoubleShift,
                                       input::mod::kNone, pane) != nullptr ||
        session_.keymap.above_mode_action(ctx, kDoubleShift, input::mod::kNone, pane) !=
            Act::kNone ||
        session_.keymap.action_for(ctx, kDoubleShift, input::mod::kNone, pane) != Act::kNone ||
        (ctx == KeyContext::kPane &&
         session_.keymap.pane_action_for(pane, kDoubleShift, input::mod::kNone) != nullptr);
    if (!answered) {
        return false;
    }
    on(input::KeyPressed{kDoubleShift, key_name_of(kDoubleShift), input::mod::kNone}, mail);
    return true;
}

// WL-KEY-19 -- agents/workshop/keyboard.md
void WorkshopWeave::on(const input::KeyReleased& k, loom::Mail& mail) {
    if (duplicate_input(mail) || !is_shift_scan(k.scancode)) {
        return;
    }
    Hand& h = hand();
    if (h.shift_tap.held) {
        const bool tap = !h.shift_tap.repeated;
        h.shift_tap.held = false;
        h.shift_tap.repeated = false;
        h.shift_tap.tapped = tap;
    }
}

} // namespace zengine::workshop
