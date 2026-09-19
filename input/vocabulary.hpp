// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INPUT_VOCABULARY_HPP
#define ZENGINE_INPUT_VOCABULARY_HPP

// The Input package's message vocabulary — the whole contract in one file, so
// there is exactly one place to diff against the current spellings.
//
// THE LAW THIS FILE IS BUILT ON (docs/reference/input.md):
//
//     Input reports coherent MOMENTS. Applications interpret GESTURES.
//
// A moment is everything the backend already knew at the instant one thing
// happened. A gesture — a drag, a resize, a selection, a click on a panel — is
// application meaning and is not spoken here at any version.
//
// A vocabulary built out of isolated FACTS (a key, a position, a button, with
// nothing said about what else was true at the same instant) costs a consumer
// three reconstructions, and all three were measured against a real application:
//
//   `%` is unreachable, because a scancode is not a character
//   a Win32 button record carries dwMousePosition and a wire that drops it makes
//     a consumer reconstruct the press location from the last MouseMoved --
//     which can be arbitrarily stale
//   with no modifier vocabulary a second directional gesture cannot be spelled
//     `Shift+hjkl`, so it costs four more literal keys
//
// Every one of those is the same defect: the backend knew, and the wire forgot.
// So the shapes below carry the facts that were SIMULTANEOUSLY TRUE, and the
// three reconstructions do not exist rather than being made more convenient.
//
// THE IDENTITY RULE (unchanged): `scancode` carries SDL scancode values — the
// wire identity of a key. Every backend's one job is to translate its native
// key identity into that space; the values live in `scan::` below (they are
// USB HID usage ids, which is what SDL_Scancode is, so no SDL dependency is
// needed to speak them). `name` is optional convenience only, never
// authoritative — a consumer that branches on `name` is trusting a courtesy.
// V1's one dressed name ("Ctrl+C", the temporary cross-backend contract snake
// and Workshop branched on) is RETIRED: Ctrl is a modifier now, and it is
// carried as one.
//
// PHYSICAL KEYS AND ENTERED TEXT ARE DIFFERENT TRUTHS, and this is the
// distinction the historical Zen Input class got right and V1 lost. A key
// transition says WHICH KEY CHANGED STATE. TextEntered says WHAT THE USER
// ACTUALLY TYPED, as the platform's own keyboard layout produced it. A consumer
// never derives one from the other: `Shift+5 -> %` is a claim about a US
// layout, not about a keyboard, and no application should be making it.
//
// EDITING CONTROLS ARE NOT ENTERED TEXT. Backspace, Enter, Escape and Tab
// arrive as key transitions and never as TextEntered, because "erase the
// character before the cursor" is not a character. What those keys MEAN —
// commit, cancel, delete — is the application's, and this package does not
// know that an editor exists. (The historical class's `start_text_input(cb)`
// owned that policy; deliberately not inherited.)
//
// OWNERSHIP (unchanged): exactly one Input weave — the holder of kInputRole —
// is the sole producer of these messages, and the only place that talks to the
// platform. Consumers only accept. There is no polling API: input is a stream
// of events, not a state to be asked about.
//
// NAMED ADDITION (recorded face-up, never silently):
//   - PumpInput v1 — the direct drive message. A weave runs only when a
//     message arrives, so something must give the Input weave execution time
//     to drain the platform. The weave arranges its OWN beat — on the
//     TimerService's hello it asks for a repeating role-addressed timer
//     (kPumpTimerId below) and polls on each firing, so no host owes it
//     laps. PumpInput stays as the door it always was — the same hands, on
//     direct request — for suites, diagnostics, and hosts running without a
//     timer service. It is not a polling API: no consumer can ask the Input
//     weave anything with it; it only opens the weave's hands.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::input {

/// SDL scancode values (== USB HID usage ids) for every key the backends
/// translate today. THE numeric space of `scancode`; 0 means "unknown key".
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
/// THE EDITING KEYS, NAMED SINCE HD-3 — and naming is the whole of what HD-3
/// added. These three values already arrived on the SDL wire: that backend
/// passes SDL's scancode through untranslated (translate_sdl.hpp says so in as
/// many words), so a maker pressing Delete has always produced
/// `KeyPressed{76, "", …}`. What did not exist was a name, which meant a
/// consumer could only bind one by writing the number down — the one thing this
/// namespace exists to stop.
///
/// SINCE TEXT-0 EVERY BACKEND CAN PRODUCE ALL THREE. HD-3 deliberately left the
/// two terminal paths narrow — widening an input backend "for symmetry" is how
/// a backend acquires a key nobody has ever pressed on it — and the consumer
/// that ended the symmetry argument is `component::TextBox::consume`, which
/// binds these keys on every backend at once. So the POSIX parser now names the
/// CSI spellings (`\x1b[H`, `\x1b[3~`, the `1;m`-modified forms) and the Win32
/// console table names VK_HOME/VK_END/VK_DELETE. Each backend's honest reach —
/// including which modifiers each can vouch for on these keys — is in
/// translate.hpp, exactly as it was.
inline constexpr std::int64_t kHome = 74;
inline constexpr std::int64_t kDelete = 76;
inline constexpr std::int64_t kEnd = 77;
inline constexpr std::int64_t kRight = 79;
inline constexpr std::int64_t kLeft = 80;
inline constexpr std::int64_t kDown = 81;
inline constexpr std::int64_t kUp = 82;
} // namespace scan

/// The SEMANTIC modifiers, as a bitmask — the `modifiers` field's numeric
/// space. Semantic, not physical: a backend that distinguishes left and right
/// Ctrl reports one kCtrl either way, because "was Ctrl held" is the question
/// every consumer actually asks. The physical key keeps its own identity in
/// `scancode` when the backend reports it as a key at all.
///
/// WHAT A CLEAR BIT MEANS, stated because a bare absence otherwise fails open
/// in the widening direction: a bit is set only when the backend OBSERVED that
/// modifier held at this event's moment. A backend that cannot observe a
/// modifier never sets it — so `modifiers == kNone` means "nothing this backend
/// can see was held", and each backend's exact reach is documented in
/// translate.hpp rather than implied here.
///
/// The honest limits, both real and both named at the door:
///   - a POSIX terminal reports no modifier state at all for ordinary keys. It
///     reports the RESULTING BYTE. kShift is therefore inferred from an
///     uppercase letter — which CapsLock also produces — and kCtrl from the
///     control byte a terminal genuinely sends for Ctrl+letter. kSuper is
///     unreachable there and is never claimed.
///   - the Win32 console reports Shift as one bit (no left/right), reports
///     left and right Ctrl and Alt separately (both fold to one semantic bit),
///     and has no Super/GUI bit at all.
namespace mod {
inline constexpr std::int64_t kNone = 0;
inline constexpr std::int64_t kShift = 1;
inline constexpr std::int64_t kCtrl = 2;
inline constexpr std::int64_t kAlt = 4;
inline constexpr std::int64_t kSuper = 8;
} // namespace mod

/// What a pointer coordinate MEANS — the `space` field's numeric space.
///
/// It exists because "position" without a unit is the same defect as a button
/// without a position: a number whose meaning the consumer has to guess from
/// which backend it thinks is loaded. A terminal cell and an SDL pixel are both
/// small non-negative integers, and nothing but this field tells them apart.
/// The two terminal backends report kCells; the SDL backend reports kPixels on
/// every pointer shape (translate_sdl.hpp) — which is exactly the arrival this
/// field was declared against: a graphical backend could not change what the
/// old value meant, because the new unit came stamped. A consumer that does not
/// recognise the space should ignore the event rather than assume its own.
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

/// The user entered text — what the platform's own keyboard layout produced,
/// as UTF-8. This is the ONLY truthful route to a character: `%` arrives here
/// as "%", and no consumer anywhere computes it from a key identity.
///
/// It is a separate message from the key transition and not a field on it,
/// because the two are genuinely different facts with different populations:
/// a key press may produce no text (an arrow, Escape, Ctrl+C), and text may
/// arrive with no key this package can name (`%` on a layout where the shifted
/// digit is not scancode 5 — which is most of them).
///
/// WHAT THIS IS NOT. It is not an editor, an IME, a composition buffer, a
/// selection or a clipboard. `text` is normally one character; it is a string
/// because a UTF-8 character is up to four bytes and because a backend may
/// legitimately deliver more than one at a time. Dead keys and IME composition
/// are NOT implemented: on both current backends what arrives is what the
/// platform already committed as ordinary text, and a composing sequence is
/// whatever those platforms do without this package's help.
struct TextEntered {
    std::string text;
    ZEN_SHAPE(TextEntered, 1, ZEN_FIELD(text));
};

/// The pointer moved — position, the delta from the previous reported position,
/// what the numbers mean, and what was held while it happened.
///
/// `x`/`y`/`dx`/`dy` are integers because both backends report integers; `space`
/// says which integers. The old shape's doubles bought nothing and cost every
/// consumer a bounded narrowing cast on the hot path.
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

/// A pointer button changed, AND WHERE THE POINTER WAS WHEN IT DID.
/// 1 = left, 2 = middle, 3 = right.
///
/// The position is the whole reason this shape has a version 1 of its own. Both
/// backends hand their reader the coordinates on the button record itself — the
/// Win32 console puts dwMousePosition on every MOUSE_EVENT_RECORD, an SGR report
/// spells the column and row inside the press sequence — and V1's MouseButton
/// dropped them, forcing consumers to answer "where was the click" from the last
/// motion event. That answer can be arbitrarily wrong: a console generates no
/// motion records while it lacks focus, so the first click after refocusing was
/// reported wherever the pointer had silently been. The fact is preserved here
/// because it was never the consumer's to reconstruct.
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

/// The drive message (the named addition — see the header comment). Empty by
/// design: it carries no question and returns no answer.
struct PumpInput {
    ZEN_SHAPE(PumpInput, 1);
};

// ---- An input SESSION: an agent's ordered queue into the same producer ------------------
//
// THE LAW ABOVE DOES NOT MOVE: exactly one Input weave produces the input shapes. What a
// session adds is a second SOURCE of moments for that one producer -- a participant on this
// bus (a guest's proxy, a test weave, a script) that hands the weave moments it composed, and
// the weave PUBLISHES them as the same `KeyPressed`, `TextEntered`, `PointerButton` and the
// rest, in the order handed, from the same identity, to the same consumers. Nothing
// downstream can tell an injected moment from a platform one, and that is the point: the
// consumer under test is the real one, and injection tests everything from the bus onward.
// What it does NOT test is the platform edge -- the console reader, the SDL queue, the OS
// translation -- which only a real hand on a real device exercises.
//
// ONE SESSION AT A TIME. A session belongs to the bus-stamped sender that opened it (never a
// name in a payload); a second opener is refused `busy` and told which session stands. It is
// closed by its holder, or ON ITS BEHALF by an office speaking deliberately (the host's
// custodian of connections closing a guest's session when the guest is gone) -- never by a
// stranger's personal word. Closing publishes a release for every key and button the session
// still held down, so no held input survives a session into unrelated work.
//
// WHAT A SESSION MAY HOLD DOWN IS BOUNDED, AND SO IS WHAT CLOSING IT COSTS. A key moment names a
// scancode in SDL's space, 1..`kMaxScancode` -- the same values every backend already speaks, so
// an injected key means on every backend what a platform one means -- and a session holds at
// most `kMaxHeldKeys` keys down at once (buttons are 1..3 already). Each batch is judged against
// the held state it WOULD leave, moment by moment, before anything is published: a press of a
// key already down is an auto-repeat and holds nothing new, a release makes room, and a batch
// that would pass the bound at any moment -- even one that lets go again later in the same batch
// -- is refused whole, naming the moment. The bound is on what one session holds, never on how
// long it holds it: nothing expires, and a key held legitimately stays held until it is released
// or the session closes.
//
// A SESSION IS NOT FOCUS, NOT A LEASE ON THE KEYBOARD AND NOT A CLAIM AGAINST THE PLATFORM.
// A person at the keyboard is still heard while a session is open; the weave interleaves the
// two sources in the order it received them, and an application that wants arbitration
// between a human and an agent decides it there, with both sources attributable by the
// session id a consumer can ask about. Refusals are answers; silence never is.

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

/// The weave's own heartbeat, asked of the Timer package on its hello: a
/// repeating role-addressed timer — the beat belongs to kInputRole, not to
/// one incarnation, so a swapped-in successor inherits it without asking.
/// 10ms is the poll cadence the old pumped host loop gave this weave; the
/// package owns its own pace now.
///
/// That inheritance is the STANDING TIMER's, and it holds (measured: a role
/// beat follows its role across holders). What it rests on is the Timer
/// service's own liveness, which is a separate question with a less happy
/// answer today: a swap of `zengine.timer` ends every beat in the system,
/// this one included, and nothing re-lights them. See Drive in
/// timer/vocabulary.hpp — the two successions are different, and only one of
/// them works.
///
/// The beat also carries the terminal parser's only clock: a lone ESC is held
/// until the next poll can say whether a sequence followed it (translate.hpp),
/// so the Escape key resolves within one beat and never guesses.
inline constexpr const char* kPumpTimerId = "zengine.input.pump";
inline constexpr std::int64_t kPumpBeatMs = 10;

} // namespace zengine::input

#endif // ZENGINE_INPUT_VOCABULARY_HPP
