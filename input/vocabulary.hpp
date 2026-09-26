// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INPUT_VOCABULARY_HPP
#define ZENGINE_INPUT_VOCABULARY_HPP

// The Input package's message vocabulary: the whole contract in one file. Input reports
// coherent moments -- everything the backend knew at the instant one thing happened -- and
// applications interpret gestures (a drag, a selection, a click on a panel), which are not
// spoken here. `scancode` carries SDL scancode values (USB HID usage ids, `scan::`), the wire
// identity of a key; `name` is a courtesy, never authoritative.
// Reference: docs/reference/input.md.

// A key transition says which key changed state; `TextEntered` says what the platform's layout
// produced, and a consumer never derives one from the other. Editing controls (Backspace,
// Enter, Escape, Tab) are key transitions, never text. Exactly one Input weave, the holder of
// `kInputRole`, produces these shapes, and there is no polling API. `PumpInput` is a drive door
// for suites and timer-less hosts: it opens the weave's hands and asks nothing.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::input {

/// SDL scancode values (== USB HID usage ids) for every key the backends
/// translate. THE numeric space of `scancode`; 0 means "unknown key".
/// Spelled as literals, pinned as literals in the suite — a typo here is a red
/// test against the SDL headers' own values, not a silently different world.
namespace scan {
inline constexpr std::int64_t kUnknown = 0;
inline constexpr std::int64_t kA = 4;
inline constexpr std::int64_t kB = 5;
inline constexpr std::int64_t kC = 6;
inline constexpr std::int64_t kD = 7;
inline constexpr std::int64_t kE = 8;
inline constexpr std::int64_t kF = 9;
inline constexpr std::int64_t kG = 10;
inline constexpr std::int64_t kH = 11;
inline constexpr std::int64_t kI = 12;
inline constexpr std::int64_t kJ = 13;
inline constexpr std::int64_t kK = 14;
inline constexpr std::int64_t kL = 15;
inline constexpr std::int64_t kM = 16;
inline constexpr std::int64_t kN = 17;
inline constexpr std::int64_t kO = 18;
inline constexpr std::int64_t kP = 19;
inline constexpr std::int64_t kQ = 20;
inline constexpr std::int64_t kR = 21;
inline constexpr std::int64_t kS = 22;
inline constexpr std::int64_t kT = 23;
inline constexpr std::int64_t kU = 24;
inline constexpr std::int64_t kV = 25;
inline constexpr std::int64_t kW = 26;
inline constexpr std::int64_t kX = 27;
inline constexpr std::int64_t kY = 28;
inline constexpr std::int64_t kZ = 29;
inline constexpr std::int64_t k1 = 30;
inline constexpr std::int64_t k2 = 31;
inline constexpr std::int64_t k3 = 32;
inline constexpr std::int64_t k4 = 33;
inline constexpr std::int64_t k5 = 34;
inline constexpr std::int64_t k6 = 35;
inline constexpr std::int64_t k7 = 36;
inline constexpr std::int64_t k8 = 37;
inline constexpr std::int64_t k9 = 38;
inline constexpr std::int64_t k0 = 39;
inline constexpr std::int64_t kReturn = 40;
inline constexpr std::int64_t kEscape = 41;
inline constexpr std::int64_t kBackspace = 42;
inline constexpr std::int64_t kTab = 43;
inline constexpr std::int64_t kSpace = 44;
inline constexpr std::int64_t kMinus = 45;
inline constexpr std::int64_t kEquals = 46;
inline constexpr std::int64_t kLeftBracket = 47;
inline constexpr std::int64_t kRightBracket = 48;
inline constexpr std::int64_t kBackslash = 49;
inline constexpr std::int64_t kSemicolon = 51;
inline constexpr std::int64_t kApostrophe = 52;
inline constexpr std::int64_t kGrave = 53;
inline constexpr std::int64_t kComma = 54;
inline constexpr std::int64_t kPeriod = 55;
inline constexpr std::int64_t kSlash = 56;
/// The editing keys: SDL already delivered these values (that backend passes scancodes
/// through), and the names let a consumer bind them without writing the number down. Every
/// backend produces all three -- the POSIX parser names the CSI spellings, the Win32 table the
/// virtual keys -- and each backend's reach, modifiers included, is in translate.hpp.
inline constexpr std::int64_t kHome = 74;
inline constexpr std::int64_t kDelete = 76;
inline constexpr std::int64_t kEnd = 77;
inline constexpr std::int64_t kRight = 79;
inline constexpr std::int64_t kLeft = 80;
inline constexpr std::int64_t kDown = 81;
inline constexpr std::int64_t kUp = 82;
} // namespace scan

/// The semantic modifiers, as a bitmask: left and right fold, since "was Ctrl held" is the
/// question consumers ask. A bit is set only when the backend observed that modifier held at
/// this event's moment, so `kNone` means "nothing this backend can see was held". A POSIX
/// terminal reports the resulting byte: kShift is inferred from an uppercase letter (CapsLock
/// too), kCtrl from a control byte, and kSuper never. The Win32 console has one Shift bit, left
/// and right Ctrl and Alt, and no Super.
namespace mod {
inline constexpr std::int64_t kNone = 0;
inline constexpr std::int64_t kShift = 1;
inline constexpr std::int64_t kCtrl = 2;
inline constexpr std::int64_t kAlt = 4;
inline constexpr std::int64_t kSuper = 8;
} // namespace mod

/// What a pointer coordinate means: a terminal cell and an SDL pixel are both small integers,
/// and only this field tells them apart. The terminal backends report kCells, the SDL backend
/// kPixels; a consumer that does not recognise the space ignores the event.
namespace space {
inline constexpr std::int64_t kUnknown = 0;
inline constexpr std::int64_t kCells = 1;
inline constexpr std::int64_t kPixels = 2;
} // namespace space

/// A key went down, and what else was true when it did. Auto-repeat arrives as
/// repeated presses (SDL's own stance); there is deliberately no repeat flag.
///
/// v2: `modifiers` — the semantic modifiers held AT THIS TRANSITION, so a
/// consumer never joins a key event to keyboard state read later.
struct KeyPressed {
    std::int64_t scancode = 0;
    std::string name;
    std::int64_t modifiers = mod::kNone;
    ZEN_SHAPE(KeyPressed, 2, ZEN_FIELD(scancode), ZEN_FIELD(name), ZEN_FIELD(modifiers));
};

/// A key came up. Backends whose platform reports no release (a terminal
/// reports completed keystrokes only) synthesize one immediately after the
/// press, so consumers never see a key stuck down forever.
struct KeyReleased {
    std::int64_t scancode = 0;
    std::string name;
    std::int64_t modifiers = mod::kNone;
    ZEN_SHAPE(KeyReleased, 2, ZEN_FIELD(scancode), ZEN_FIELD(name), ZEN_FIELD(modifiers));
};

/// The user entered text: what the platform's own layout produced, as UTF-8 -- the only
/// truthful route to a character (`%` arrives as "%"). A separate message, because a key press
/// may produce no text and text may arrive with no key this package can name. Not an editor,
/// IME, composition buffer, selection or clipboard; a string because a character is up to four
/// bytes and a backend may deliver several. What arrives is what the platform committed.
struct TextEntered {
    std::string text;
    ZEN_SHAPE(TextEntered, 1, ZEN_FIELD(text));
};

/// The pointer moved: the position, the delta from the previous reported position, what the
/// numbers mean and what was held. Integers, since every backend reports integers; `space` says
/// which.
struct PointerMoved {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t dx = 0;
    std::int64_t dy = 0;
    std::int64_t space = space::kUnknown;
    std::int64_t modifiers = mod::kNone;
    ZEN_SHAPE(PointerMoved, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(dx), ZEN_FIELD(dy),
              ZEN_FIELD(space), ZEN_FIELD(modifiers));
};

/// A pointer button changed, and where the pointer was when it did: 1 left, 2 middle, 3 right.
/// Every backend has the coordinates on the button record itself, so no consumer answers
/// "where was the click" from the last motion -- which a console that sends no motion while
/// unfocused would make arbitrarily wrong.
struct PointerButton {
    std::int64_t button = 0;
    bool pressed = false;
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t space = space::kUnknown;
    std::int64_t modifiers = mod::kNone;
    ZEN_SHAPE(PointerButton, 1, ZEN_FIELD(button), ZEN_FIELD(pressed), ZEN_FIELD(x),
              ZEN_FIELD(y), ZEN_FIELD(space), ZEN_FIELD(modifiers));
};

/// The wheel turned, where the pointer was, and what was held. +1.0 per notch
/// away from the user (SDL's convention); `dx` is horizontal wheels. The
/// deltas stay fractional because a high-resolution wheel genuinely reports a
/// fraction of a detent; the position beside them is a cell or a pixel like any
/// other pointer position.
struct PointerWheel {
    double dx = 0;
    double dy = 0;
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t space = space::kUnknown;
    std::int64_t modifiers = mod::kNone;
    ZEN_SHAPE(PointerWheel, 1, ZEN_FIELD(dx), ZEN_FIELD(dy), ZEN_FIELD(x), ZEN_FIELD(y),
              ZEN_FIELD(space), ZEN_FIELD(modifiers));
};

/// The drive message: empty by design, it carries no question and returns no answer.
struct PumpInput {
    ZEN_SHAPE(PumpInput, 1);
};

// ---- An input session: an agent's ordered queue into the same producer ------------------
//
// A second source of moments for the one Input weave: a participant (a guest's proxy, a test
// weave) hands it moments, and the weave publishes them as the same shapes, in order, to the
// same consumers -- so injection tests everything from the bus onward, not the platform edge.
// One session at a time, owned by the bus-stamped sender that opened it; closed by its holder
// or by an office on its behalf, which releases every key and button it still held.
//
// A key moment names a scancode in 1..`kMaxScancode`, and a session holds at most
// `kMaxHeldKeys` keys down: each batch is judged against the held state it would leave, moment
// by moment, and refused whole if it would pass the bound. Nothing expires. A session is not
// focus: a person at the keyboard is still heard, interleaved in arrival order.

/// OPEN A SESSION. Answered `InputSessionOpened`, or `zen.Refused` naming the session that
/// stands in the way.
struct InputSessionRequested {
    std::string purpose; ///< one line, for a person reading the inventory
    ZEN_SHAPE(InputSessionRequested, 1, ZEN_FIELD(purpose));
};

struct InputSessionOpened {
    std::int64_t session = 0;
    ZEN_SHAPE(InputSessionOpened, 1, ZEN_FIELD(session));
};

/// CLOSE A SESSION. By its holder, personally; or by an OFFICE speaking deliberately, naming
/// the holder it closes on behalf of (`holder` is that holder's WeaveId as a number, read only
/// when the closer is not the holder). An office may say `session` 0 for "whatever that holder
/// holds" -- the door closing a dead guest's session knows the guest, not the number. Answered
/// `zen.Ack`, or `zen.Refused`. Every key and button the session still held is released, as
/// ordinary published moments, before the Ack.
struct InputSessionClosed {
    std::int64_t session = 0;
    std::int64_t holder = 0;
    ZEN_SHAPE(InputSessionClosed, 1, ZEN_FIELD(session), ZEN_FIELD(holder));
};

/// One moment an agent composed. `kind` says which of the published shapes it becomes and
/// which fields are read; the rest are ignored. The kinds are the shapes' own names, so an
/// agent that knows the Input reference knows the spelling.
struct InjectedEvent {
    std::string kind; ///< "KeyPressed" "KeyReleased" "TextEntered" "PointerMoved" "PointerButton" "PointerWheel"
    std::int64_t scancode = 0;  ///< KeyPressed / KeyReleased: `scan::`
    std::string name;           ///< KeyPressed / KeyReleased: the courtesy spelling, may be empty
    std::int64_t modifiers = 0; ///< every kind but TextEntered: `mod::`
    std::string text;           ///< TextEntered: UTF-8, as the platform would have committed it
    std::int64_t button = 0;    ///< PointerButton: 1 left, 2 middle, 3 right
    bool pressed = false;       ///< PointerButton
    std::int64_t x = 0;         ///< pointer kinds: the position, in `space`
    std::int64_t y = 0;
    std::int64_t dx = 0;        ///< PointerMoved: the delta
    std::int64_t dy = 0;
    std::int64_t space = 0;     ///< pointer kinds: `space::kCells` or `space::kPixels`; unknown is refused
    double wheel_dx = 0;        ///< PointerWheel: the notches
    double wheel_dy = 0;
    ZEN_SHAPE(InjectedEvent, 1, ZEN_FIELD(kind), ZEN_FIELD(scancode), ZEN_FIELD(name),
              ZEN_FIELD(modifiers), ZEN_FIELD(text), ZEN_FIELD(button), ZEN_FIELD(pressed),
              ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(dx), ZEN_FIELD(dy), ZEN_FIELD(space),
              ZEN_FIELD(wheel_dx), ZEN_FIELD(wheel_dy));
};

/// PUBLISH THESE MOMENTS, IN THIS ORDER, through the session. Judged whole: a batch with an
/// unknown kind, an unknown pointer space, a key outside 1..`kMaxScancode`, a moment that would
/// hold more than `kMaxHeldKeys` keys down, or more than `kMaxInjectedEvents` moments publishes
/// nothing and is refused with the offending index. Answered `InputInjected`.
struct InjectInput {
    std::int64_t session = 0;
    std::vector<InjectedEvent> events;
    ZEN_SHAPE(InjectInput, 1, ZEN_FIELD(session), ZEN_FIELD(events));
};

/// Move from the session's last pointer position over elapsed time. bend=0 is linear;
/// otherwise it is the perpendicular offset of both cubic Bezier control points, in
/// that position's coordinate space. A pending motion excludes other injected batches.
/// Answered InputInjected at the endpoint; that answer reports publication, not delivery.
struct PointerMotionRequested {
    std::int64_t session = 0;
    std::int64_t x = 0, y = 0, duration_ms = 0;
    double bend = 0;
    ZEN_SHAPE(PointerMotionRequested, 1, ZEN_FIELD(session), ZEN_FIELD(x), ZEN_FIELD(y),
              ZEN_FIELD(duration_ms), ZEN_FIELD(bend));
};

/// The input office's attributed companion to the legacy event. Consumers that authorize
/// UI operations use this statement only with authenticated zengine.input authorship.
/// Local platform input has local=true and actor=0; injected input names its actual holder.
struct AttributedInput {
    bool local = false;
    std::int64_t actor = 0;
    InjectedEvent event;
    ZEN_SHAPE(AttributedInput, 1, ZEN_FIELD(local), ZEN_FIELD(actor), ZEN_FIELD(event));
};

/// WHAT THE WEAVE DID WITH A BATCH: the session's own sequence numbers of the first and last
/// moment it published. `admitted` is the count, and it equals the batch or the batch was
/// refused. ADMITTED IS NOT PROCESSED: a moment published here is on the bus in order, and
/// what a consumer makes of it is that consumer's, later, on its own delivery.
struct InputInjected {
    std::int64_t session = 0;
    std::int64_t admitted = 0;
    std::int64_t first_seq = 0;
    std::int64_t last_seq = 0;
    ZEN_SHAPE(InputInjected, 1, ZEN_FIELD(session), ZEN_FIELD(admitted), ZEN_FIELD(first_seq),
              ZEN_FIELD(last_seq));
};

/// The most moments one `InjectInput` may carry. A bound on what one delivery can put on the
/// bus, so an agent's batch is a batch and not a flood dressed as one message. It bounds ONE
/// delivery; what a session accumulates across deliveries is bounded below.
inline constexpr std::size_t kMaxInjectedEvents = 64;

/// The largest scancode a key moment may name: SDL's scancode space is 0..511
/// (`SDL_SCANCODE_COUNT` is 512), and 0 is "unknown key", which no one can press or release on
/// purpose. A key moment outside 1..kMaxScancode is refused, with its batch.
inline constexpr std::int64_t kMaxScancode = 511;

/// The most keys one session holds down at once. More than any chord a hand plays, and a
/// fixed ceiling on what a session keeps and on what closing it publishes.
inline constexpr std::size_t kMaxHeldKeys = 16;

/// The role slot the Input weave holds: the address "whoever provides input",
/// which outlives any particular implementation being swapped in or out.
inline constexpr const char* kInputRole = "zengine.input";

/// The weave's own beat: a repeating role-addressed timer, held by `kInputRole` so a
/// swapped-in successor inherits it, and re-established by the timer binding whenever the
/// Timer says it is ready. It is also the terminal parser's only clock: a lone ESC is held until
/// the next poll can say whether a sequence followed, so Escape resolves within one beat.
inline constexpr const char* kPumpTimerId = "zengine.input.pump";
inline constexpr std::int64_t kPumpBeatMs = 10;

} // namespace zengine::input

#endif // ZENGINE_INPUT_VOCABULARY_HPP
