// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SURFACE_VOCABULARY_HPP
#define ZENGINE_SURFACE_VOCABULARY_HPP

// The Surface package's message vocabulary — deliberately tiny.
//
// The rule this package installs: no game, world, or panel weave talks to the
// terminal, a window, or a renderer. They PUBLISH visual intent; a **Skin** —
// an ordinary, replaceable loadable weave holding the `zengine.skin` role —
// claims the actual surface and turns intent into output. Swap the Skin and
// the same intent lands on a different medium; that replaceability is the
// whole point, and the suite proves it with a terminal Skin and an SDL Skin
// consuming identical messages.
//
// V1 intent is two shapes deep, and the first is borrowed:
//   - `SnakeVisual` (snake's own locked shape) is the first real canvas
//     payload. The Skins accept it directly — a V1 coupling, named face-up:
//     the general medium-agnostic canvas vocabulary that would dissolve it is
//     a later phase, not this one. What V1 proves is the PATTERN (intent in,
//     pixels/characters out, zero medium-specific fields in the intent).
//   - `SurfaceText` (here) is the one intent this package adds: a line of
//     plain text for a named slot. The host's status line and the score
//     weave's tally are its first publishers.
//
// Claiming and releasing the surface are NOT messages. A Skin claims its
// medium in its constructor and releases it in its destructor (the Input
// package's reader move: load takes the terminal's hand, unload gives it
// back), and the role system is the arbiter of "exactly one active Skin":
// roles are singleton — loading a second Skin into `zengine.skin` while one
// holds it is a clean Refused, pinned in the suite. The Weave Manager's
// swap already delivers the unload first, so a swap is release-then-claim by
// construction. A surface-arbiter weave (multiple surfaces, negotiation) is a
// later package's ground; V1 needs none.

#include <zen/weave/shape.hpp>

#include <string>

namespace zengine::surface {

/// One line of PLAIN text for a named slot ("status", "score", ...). Plain
/// means plain: no escape codes, no markup — how a slot looks is the Skin's
/// business, which is exactly what lets the same intent land in a terminal
/// row, a window title, or anything a future Skin dreams up. Slots unknown to
/// the active Skin are dropped without ceremony (pub-sub: intent is an offer,
/// not a command).
struct SurfaceText {
    std::string slot;
    std::string text;
    ZEN_SHAPE(SurfaceText, 1, ZEN_FIELD(slot), ZEN_FIELD(text));
};

/// The active Skin's hello: published exactly once per incarnation, on the
/// first message it handles after claiming its surface (a weave runs only on
/// message — the same lazy-first-wake stance as the v2 world's inheritance
/// claim). Text publishers re-publish their current line when they hear it,
/// so a freshly loaded or swapped-in Skin starts complete instead of waiting
/// for each slot's next natural event. Publishers that never hear it (no Skin
/// loaded) lose nothing: their intent was landing on no one anyway.
struct SurfaceReady {
    ZEN_SHAPE(SurfaceReady, 1);
};

/// Give the active Skin execution time — the PumpInput precedent, pointed at
/// output: a weave runs only when a message arrives, and a Skin whose medium
/// is a real OS window must service that window's event queue even when no
/// intent is flowing (a dead-quiet world starves a frame-driven pump, and an
/// unpumped Windows window is flagged unresponsive — found live: the busy
/// cursor). The day this shape's note promised has arrived: a Skin now
/// arranges its OWN beat (kPumpTimerId below, asked of the Timer package on
/// its own ACTIVATION), so no host owes it laps. PumpSurface stays as the same
/// hands on direct request — for suites, diagnostics, and timer-less hosts.
struct PumpSurface {
    ZEN_SHAPE(PumpSurface, 1);
};

/// The role that IS surface ownership. Singleton by the Loom's role rules, so
/// "exactly one active Skin owns the primary surface" is enforced ground, not
/// convention. Address the Skin by role, never by id — the successor after a
/// swap is a different weave; only the role carries intent across.
inline constexpr const char* kSkinRole = "zengine.skin";

/// The two slots V1 publishers actually speak. Nothing reserves them — a slot
/// name is a convention between publisher and Skin, exactly like a schema
/// name — but spelling them once keeps the two sides from drifting.
inline constexpr const char* kSlotStatus = "status";
inline constexpr const char* kSlotScore = "score";

/// The Skin's heartbeat, asked of the Timer package on its own ACTIVATION —
/// and asked AGAIN on TimerReady, because a skin loaded before any timer
/// service exists sends an ask that goes nowhere (rejected at the library/
/// schema seam — see TimerReady in timer/vocabulary.hpp) and must be able to
/// retry: announcing is ONCE, asking is REPEATABLE, and they are deliberately
/// not the same call any more. A repeating role-addressed timer. Role-addressed
/// is the load-bearing half —
/// the beat belongs to kSkinRole, so a swapped-in successor inherits it
/// without asking (on a dead-quiet bus a fresh window-owning skin would
/// otherwise never get its queue serviced — the exact wedge the old
/// host-sent PumpSurface existed to prevent, dead by construction for a SKIN
/// swap). Terminal media no-op the firing, exactly as they no-op'd the pump.
///
/// The succession that holds here is the standing timer's, across holders of
/// kSkinRole. It is not the Timer service's own: swapping `zengine.timer`
/// ends every beat in the system, this one with it, and today nothing
/// re-lights them — the window would go unserviced again for the same reason
/// the world would stop. See Drive in timer/vocabulary.hpp.
inline constexpr const char* kPumpTimerId = "zengine.skin.pump";
inline constexpr std::int64_t kPumpBeatMs = 10;

} // namespace zengine::surface

#endif // ZENGINE_SURFACE_VOCABULARY_HPP
