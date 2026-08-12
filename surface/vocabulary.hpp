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
// V2 adds the GENERAL canvas the note above promised — `SurfaceCanvas` — but
// it does NOT dissolve the SnakeVisual coupling, and that restraint is
// deliberate. Workshop is the live consumer that pulled it: a maker tool
// has to paint an authored rectangle somewhere, and the alternatives were a
// second world beside the Skins (a Workshop-only painter) or teaching the
// Skins a Workshop-only shape. Both are worse than one general canvas that any
// Zengine app can publish. Re-expressing snake's own frame as a canvas is a
// separate, evidence-carrying move (the golden frames ARE the old drawers) and
// is not part of this addition — a general shape existing is not permission to
// migrate a proven one through it.
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

#include <cstdint>
#include <string>
#include <vector>

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

/// The visual ROLE of a canvas element — semantic, never a colour. The Skin
/// picks the actual ink, so one canvas reads correctly in a monochrome terminal
/// and in a themed window and a publisher never learns which medium it landed
/// on. (The Loom's PxRole stance, one layer down: this vocabulary DOES carry
/// geometry, because a canvas is geometry — what it still refuses to carry is
/// anything medium-specific. Cells, not pixels; roles, not RGB.) A role the
/// active Skin does not know falls back to `kFill` rather than vanishing: an
/// unknown role is still a rectangle somebody meant to be seen, and dropping it
/// would be the silent-blank fate this house refuses. Same posture as an
/// unknown text slot, opposite resolution — a slot has no place to go, a rect
/// does.
namespace role {
inline constexpr std::int64_t kFill = 0;   ///< ordinary authored material
inline constexpr std::int64_t kAccent = 1; ///< the one thing being pointed at
inline constexpr std::int64_t kMuted = 2;  ///< present, deliberately quiet
inline constexpr std::int64_t kAlert = 3;  ///< something the maker must see
} // namespace role

/// One filled rectangle, in CANVAS CELLS — the canvas's own square unit, which
/// each Skin resolves into its medium (one character column per cell in a
/// terminal, `kCanvasCellPx` pixels in a window). A cell is the honest common
/// unit: it is the coarsest thing a terminal can address, so a canvas authored
/// in cells lands somewhere real in every medium instead of being pixel-exact
/// in one and rounded into mush in the other.
///
/// Painter's order: `SurfaceCanvas::rects` is drawn front-to-back in list
/// order, so a publisher expresses "behind" by publishing earlier. There is no
/// z field and no explicit stacking policy — list order already says it, and a
/// second way to say the same thing is how two orderings come to disagree.
struct SurfaceRect {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;
    std::int64_t role = role::kFill;
    ZEN_SHAPE(SurfaceRect, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h),
              ZEN_FIELD(role));
};

/// One run of PLAIN text anchored at a canvas cell, drawn over every rect.
/// Plain means plain, exactly as in SurfaceText: no escapes, no markup.
///
/// One cell per BYTE, in every medium: the terminal skins index `text[i]` and
/// the SDL skin draws one glyph per byte, so a canvas describes one picture
/// rather than one per backend. A Skin states in its own docs which bytes it has
/// a glyph for and what it draws for the rest; what no Skin may do is drop a
/// character silently, because a publisher cannot see that happen.
struct SurfaceLabel {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::string text;
    std::int64_t role = role::kFill;
    ZEN_SHAPE(SurfaceLabel, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(text), ZEN_FIELD(role));
};

/// A whole canvas: an extent in cells, filled rectangles, and text over them.
/// The complete general drawing intent — and deliberately no more than that.
/// It is not a layout system and not a widget tree: it carries no parent/child
/// relationship, no anchors, no percentages, no policy. Whoever publishes it
/// has already decided where things go; the Skin only resolves cells into its
/// medium. That boundary is the whole reason this shape can stay this small,
/// and the reason the Loom's geometry-free semantic tree (loom::Widget) remains
/// a different, higher thing rather than something this competes with: that
/// tree describes intent a renderer must LAY OUT, this describes a picture a
/// medium must PAINT.
///
/// Elements outside the extent are the Skin's to clip. An empty canvas (no
/// rects, no labels) is a legitimate picture — it means "nothing", not "no
/// intent" — and clears whatever the previous canvas drew.
struct SurfaceCanvas {
    std::int64_t width = 0;
    std::int64_t height = 0;
    std::vector<SurfaceRect> rects;
    std::vector<SurfaceLabel> labels;
    ZEN_SHAPE(SurfaceCanvas, 1, ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(rects),
              ZEN_FIELD(labels));
};

/// One canvas cell in a graphical medium. The terminal needs no such number —
/// its cell IS a character — so this lives here as the one place a window-owning
/// Skin gets the conversion, rather than each inventing its own scale.
inline constexpr std::int64_t kCanvasCellPx = 12;

/// HOW MUCH ROOM THE ACTIVE SURFACE HAS, in canvas cells — the medium answering
/// the one question a publisher cannot answer for itself.
///
/// Every other shape here travels intent -> medium. This one travels the other
/// way, and it is the only fact that does: a canvas is authored in cells and a
/// medium resolves cells into its own units, so how many cells there is room for
/// is a fact ONLY the medium holds. Before G-2 a publisher had to guess, which in
/// practice meant a constant, which in practice meant a window sized to the
/// picture rather than a picture sized to the window. `Workshop`'s own screen
/// header said so in as many words — "a canvas has no notion of the medium's
/// size, and giving it one is a Surface question, not a Workshop one". This is
/// that Surface question, answered.
///
/// IT IS AN OFFER, NOT AN INSTRUCTION, exactly like every other intent on this
/// bus in the other direction. A publisher that ignores it keeps publishing
/// whatever extent it likes and the Skin clips, which is the contract
/// `SurfaceCanvas` already states. What changes is only that a publisher which
/// WANTS to fill its medium now can.
///
/// PUBLISHED WHEN IT CHANGES, and not otherwise — a Skin's own beat is what
/// notices a person dragging a window edge, so the fact arrives on the pump
/// rather than on an event nobody owns. A medium with no answer (a terminal Skin,
/// or a window that does not exist yet) publishes NOTHING rather than publishing
/// zeroes: "I have no opinion" and "there is no room" are different sentences,
/// and only one of them should move a publisher's screen.
///
/// It names no surface for the same reason `SurfaceCloseRequested` names no
/// window: `kSkinRole` is a singleton, so "the active surface" is a complete
/// address.
struct SurfaceExtent {
    std::int64_t width = 0;
    std::int64_t height = 0;
    ZEN_SHAPE(SurfaceExtent, 1, ZEN_FIELD(width), ZEN_FIELD(height));
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

/// THE SURFACE THIS APPLICATION IS BEING SHOWN ON HAS BEEN ASKED TO CLOSE.
///
/// A window manager's close box, or the platform's equivalent. It is a
/// LIFECYCLE fact and emphatically not an input moment, and the distinction is
/// the whole reason it is a shape of its own rather than a synthesized
/// `KeyPressed{Q}`: a maker who binds `q` to quit has authored a policy, and an
/// operating system asking a window to go away has not. Whoever hears this
/// applies its own quit policy — including deciding not to.
///
/// IT NAMES NO WINDOW, and that is deliberate rather than unfinished. There is
/// exactly one application surface in this architecture: `kSkinRole` is a
/// singleton, so "the active surface" is a complete address. A window id would
/// either be SDL's (a backend number leaking into a medium-agnostic
/// vocabulary) or a Zen one (a multi-window identity law written from a
/// one-window witness). Neither is earned. When a second surface exists, the
/// fact this shape is missing will be obvious and it can be added then.
///
/// WHO PUBLISHES IT is a per-medium question with an awkward but honest answer.
/// A terminal has no close box and no terminal Skin ever sends this. SDL
/// reports window lifecycle and input through ONE process-global event queue,
/// so the weave that owns that queue — the SDL Input reader — is the only thing
/// in the process that can see the request. Owning the queue does not make the
/// request input; it makes the reader a router for one fact that is not its
/// own, and the fact is spelled here, in the vocabulary that owns the
/// application's surface, so no consumer has to read it as a key.
struct SurfaceCloseRequested {
    ZEN_SHAPE(SurfaceCloseRequested, 1);
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
