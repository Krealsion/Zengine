#ifndef ZENGINE_INPUT_VOCABULARY_HPP
#define ZENGINE_INPUT_VOCABULARY_HPP

// The Input package's message vocabulary — the whole contract in one file, so
// there is exactly one place to diff against the locked spellings.
//
// THE LOCKED CONTRACT (Vision chat; quoted in the Input phase prompt) is five
// shapes: KeyPressed v1, KeyReleased v1, MouseButton v1, MouseMoved v1,
// MouseWheel v1. They are spelled here as ZEN_SHAPE structs whose derived
// schemas are field-for-field identical to the contract's SchemaBuilder
// spellings; the input suite pins that identity by content-id, so a drift
// between this file and the contract is a red test, not an opinion.
//
// THE IDENTITY RULE (locked): `scancode` carries SDL scancode values — the
// wire identity of a key. Every backend's one job is to translate its native
// key identity into that space; the values live in `scan::` below (they are
// USB HID usage ids, which is what SDL_Scancode is, so no SDL dependency is
// needed to speak them). `name` is optional convenience only, never
// authoritative — a consumer that branches on `name` is trusting a courtesy.
//
// OWNERSHIP (locked): exactly one Input weave — the holder of kInputRole — is
// the sole producer of the five messages, and the only place that talks to the
// platform. Consumers only accept. There is no polling API in V1: input is a
// stream of events, not a state to be asked about.
//
// NAMED ADDITION (the contract proved insufficient here; recorded face-up per
// the phase's report-back rule, never silently):
//   - PumpInput v1 — the drive message. A weave runs only when a message
//     arrives, and the substrate has no timers yet, so something must give the
//     Input weave execution time to drain the platform. Who produces it (the
//     host loop today, a timer weave later) is deliberately unspecified — the
//     exact stance SnakeTick took, for the exact reason. It is not a polling
//     API: no consumer can ask the Input weave anything with it; it only opens
//     the weave's hands.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>

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
inline constexpr std::int64_t kRight = 79;
inline constexpr std::int64_t kLeft = 80;
inline constexpr std::int64_t kDown = 81;
inline constexpr std::int64_t kUp = 82;
} // namespace scan

/// A key went down. Auto-repeat arrives as repeated presses (SDL's own
/// stance); V1 deliberately carries no repeat flag or modifiers.
struct KeyPressed {
    std::int64_t scancode = 0;
    std::string name;
    ZEN_SHAPE(KeyPressed, 1, ZEN_FIELD(scancode), ZEN_FIELD(name));
};

/// A key came up. Backends whose platform reports no release (a terminal
/// reports completed keystrokes only) synthesize one immediately after the
/// press, so consumers never see a key stuck down forever.
struct KeyReleased {
    std::int64_t scancode = 0;
    std::string name;
    ZEN_SHAPE(KeyReleased, 1, ZEN_FIELD(scancode), ZEN_FIELD(name));
};

/// A mouse button changed. 1 = left, 2 = middle, 3 = right (locked).
struct MouseButton {
    std::int64_t button = 0;
    bool pressed = false;
    ZEN_SHAPE(MouseButton, 1, ZEN_FIELD(button), ZEN_FIELD(pressed));
};

/// The pointer moved. Position units are the backend's surface (pixels on a
/// graphical backend, character cells on a console); dx/dy are the delta from
/// the previous position this backend reported.
struct MouseMoved {
    double x = 0;
    double y = 0;
    double dx = 0;
    double dy = 0;
    ZEN_SHAPE(MouseMoved, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(dx), ZEN_FIELD(dy));
};

/// The wheel turned. +1.0 per notch away from the user (SDL's convention);
/// dx is horizontal wheels.
struct MouseWheel {
    double dx = 0;
    double dy = 0;
    ZEN_SHAPE(MouseWheel, 1, ZEN_FIELD(dx), ZEN_FIELD(dy));
};

/// The drive message (the named addition — see the header comment). Empty by
/// design: it carries no question and returns no answer.
struct PumpInput {
    ZEN_SHAPE(PumpInput, 1);
};

/// The role slot the Input weave holds: the address "whoever provides input",
/// which outlives any particular implementation being swapped in or out.
inline constexpr const char* kInputRole = "zengine.input";

} // namespace zengine::input

#endif // ZENGINE_INPUT_VOCABULARY_HPP
