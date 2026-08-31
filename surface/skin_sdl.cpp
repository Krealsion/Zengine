// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The SDL Skin — the same intent, a real window. This file is deliberately
// nothing but the SDL edge: both the snake frame and the general canvas are
// planned by skin_sdl_plan.hpp as pure math (pinned on every lane, SDL or
// not); here a plan is executed against a real renderer and the SurfaceText
// slots land in the window title. Zero snake-specific or TUI-specific fields
// were added to any intent to make this medium work — that absence is the
// phase's agnosticism proof.
//
// THIS MEDIUM DRAWS LABELS. A window owns no font the way a terminal does, so
// each label byte is drawn from a cell-sized bitmap the plan owns. What it
// promises is exactly printable ASCII — see skin_sdl_glyphs.hpp, which also
// says what any other byte draws instead of vanishing.
//
// AND SINCE HD-1 IT OWNS A REAL FACE AS WELL, for one thing only: a
// `SurfaceTextRegion`, the bounded part of a canvas whose interior a medium may
// set in its own type. The face is embedded (skin_sdl_text.hpp), measured on
// open, and its metric is published upward on `SurfaceExtent` so the publisher —
// never this medium — decides how much prose fits. The two are not alternatives
// for the same element: labels are cells and always were, regions are type when
// there is type to set them in, and a medium with no face draws a region as
// labels through the same cell projection a terminal uses. Nothing else changed;
// `plan_canvas` still hands this file flat lists of opaque quads and this file
// still cannot tell a glyph from a rect.
//
// The medium owns the SDL video subsystem RAII-style for the weave's
// lifetime: construction brings SDL up (the window itself is created lazily
// on the first frame, when the board's geometry is first known), destruction
// tears the window down and releases its claim on the subsystem — load claims
// the surface, unload releases it, the same law the terminal skins live by.
//
// Degrades gracefully with no display: SDL_Init fails, the medium stays
// disabled, frames are consumed and counted with nothing to show them on
// (the Input reader's posture, pointed at output) — and it SAYS SO, with SDL's
// own reason, instead of going quietly dark. Under SDL's dummy driver (the
// headless suite) everything below runs for real except photons.
//
// THE WINDOW IS AN EAR, and three things here are consequences of one
// architectural fact: SDL has ONE process-global event queue, and it has ONE
// owner — the SDL Input reader (input/).
//
//   - THIS MEDIUM TAKES NOTHING OUT OF THE QUEUE, AND SERVICES ITS OWN WINDOW.
//     A `pump()` that drained the queue would swallow every key, click and close
//     request the reader exists to hear, so it does not poll and does not
//     filter. What it does do is `SDL_PumpEvents` on its own beat, which
//     gathers the OS's pending input INTO the queue and removes nothing.
//     One-owner is a rule about who REMOVES, and this is still not it.
//     G-1 read it as a rule about who CALLS, emptied this function, and left a
//     window's liveness depending on which reader the host booted -- with the
//     terminal reader nothing called into SDL at all and the window was flagged
//     Not Responding. That is fixed here, and the pairing's remaining honest
//     cost is measured and said out loud (`notice_unread_queue`) rather than
//     left for a person to discover as a hang.
//   - THE WINDOW IS FOCUSABLE. It was created SDL_WINDOW_NOT_FOCUSABLE
//     (WS_EX_NOACTIVATE on Windows) because a window that cannot hear must not
//     take the keys: the terminal was the game's one ear, and keys kept dying
//     in the focused SDL window until it refused focus. It can hear now, so the
//     flag is gone. The cost is stated where it lands: with the terminal reader
//     and the SDL skin together, the window will take focus and neither ear is
//     the one you are typing at. Which reader a run uses is the host's explicit
//     choice (`--input`), not something this file may infer.
//   - THE WINDOW ENABLES NATIVE TEXT EVENTS. SDL_StartTextInput takes a WINDOW,
//     so only whoever owns the window can turn it on, and this is that. It is
//     window setup and nothing more: what a typed character MEANS stays the
//     application's, and the character itself reaches it as an ordinary
//     input::TextEntered through the reader. The SDL window pointer does not
//     leave this file.
//
// The close box is still not this package's to act on. It arrives with the rest
// of the queue at the reader, which routes it as the lifecycle fact it is
// (surface::SurfaceCloseRequested — spelled in this package's vocabulary,
// because it is a fact about this application's surface), and whoever owns quit
// policy decides. A skin still never dies on its own.

#include "skin.hpp"
#include "skin_sdl_plan.hpp"
#include "skin_sdl_text.hpp"

#include <zen/kernel/export.hpp>

#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace {

using namespace zengine::surface;

/// What went wrong, in SDL's own words, on plain stderr.
///
/// The V1 posture was "politely dark": SDL_Init fails, `ok_` goes false, every
/// frame after that is consumed and nothing is ever shown. It is a correct
/// degradation and a terrible diagnosis — real time has been spent on a WSL
/// whose fetched SDL3 has only the dummy and offscreen video drivers, where the
/// only symptom available was "no window". A surface that cannot exist should say
/// why it cannot exist.
///
/// stderr, not a SurfaceText: the most likely thing to have failed is the
/// surface, and a message about a missing surface delivered to the surface is
/// not a message. Same argument the boot weave already makes for a refused
/// load. This is four lines and one `if` per SDL call that can fail, not a
/// diagnostic framework.
void complain(const char* what) {
    const char* why = SDL_GetError();
    std::fprintf(stderr, "zengine-skin-sdl: %s failed: %s\n", what,
                 (why != nullptr && why[0] != '\0') ? why : "(SDL gave no reason)");
    std::fflush(stderr);
}

/// Give back this weave's claim on SDL, and turn the lights off if nobody else
/// is still in the room.
///
/// SDL_Quit alone would be wrong: the SDL Input reader holds the same subsystem
/// in the same process, and SDL_Quit shuts everything down regardless
/// of who is still using it — a Skin swap would deafen the reader.
/// SDL_QuitSubSystem alone is wrong in the other direction, and MEASURED so: it
/// releases the subsystem and leaves SDL's own global state allocated, which the
/// sanitizer lane reports as a leak at exit (it did, ~8.5 KB per run, the day
/// this stopped calling SDL_Quit).
///
/// So the release is refcounted and the final shutdown is conditional, and the
/// condition is asked of SDL rather than tracked privately: `SDL_WasInit(0)` is
/// the mask of subsystems still up, so an empty mask means this was the last
/// holder. A private counter shared between two separately-loaded weave
/// libraries is exactly the thing that cannot exist here; SDL already keeps the
/// only copy that could be right.
void release_sdl() {
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    if (SDL_WasInit(0) == 0) {
        SDL_Quit();
    }
}

class SdlMedium {
public:
    SdlMedium() {
        ok_ = SDL_Init(SDL_INIT_VIDEO);
        if (!ok_) {
            complain("SDL_Init(SDL_INIT_VIDEO)");
            // A FAILED SDL_Init STILL ALLOCATED. SDL brings its own globals up
            // before it discovers it cannot bring a video driver up, and leaves
            // them behind; measured at ~7 KB by the sanitizer lane the day this
            // path first existed, which is the whole reason that lane exists.
            // The same conditional release the destructor uses, because "this
            // failed" is still "this is no longer holding anything".
            release_sdl();
        }
    }
    ~SdlMedium() {
        // The face first: its glyph atlas lives in textures this renderer owns, so
        // the engine has to give them back before the renderer they belong to goes
        // away. Ordinary destruction order would have run this after, which is a
        // free-after-free that only a sanitizer lane would have named.
        text_.close();
        if (renderer_ != nullptr) {
            SDL_DestroyRenderer(renderer_);
        }
        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
        }
        if (ok_) {
            release_sdl();
        }
    }
    SdlMedium(const SdlMedium&) = delete;
    SdlMedium& operator=(const SdlMedium&) = delete;

    void frame(const zengine::snake::SnakeVisual& v, bool) {
        if (!ok_) {
            return;
        }
        if (!ensure_window(v)) {
            return;
        }
        pump();
        // The drawable can now be LARGER than the picture — the window is
        // resizable and a board's extent is the board's — so the surface is
        // cleared before the plan is drawn. Without this, the pixels outside a
        // board that no longer fills its window are whatever was there before.
        SDL_SetRenderDrawColor(renderer_, kCanvasBackground.r, kCanvasBackground.g,
                               kCanvasBackground.b, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer_);
        for (const PlanRect& r : plan_frame(v)) {
            SDL_SetRenderDrawColor(renderer_, r.r, r.g, r.b, SDL_ALPHA_OPAQUE);
            const SDL_FRect fr{static_cast<float>(r.x), static_cast<float>(r.y),
                               static_cast<float>(r.w), static_cast<float>(r.h)};
            SDL_RenderFillRect(renderer_, &fr);
        }
        SDL_RenderPresent(renderer_);
    }

    /// The general canvas, in a window — the same three lines `frame` uses, and
    /// deliberately so.
    ///
    /// `plan_canvas` hands back each layer's rectangles AND labels as one list of
    /// opaque quads, so there is nothing here that knows what a label is. That
    /// is the whole shape of the answer to "rectangles drawn, labels dropped":
    /// looping over one primitive list and never reading another is a mistake a second
    /// loop could make again — so the edge has no second thing
    /// to draw. What a glyph looks like, where it lands and what happens to a
    /// byte with no glyph are all decided in skin_sdl_plan.hpp / skin_sdl_glyphs.hpp,
    /// where every lane's suite can read them, SDL built or not.
    void canvas(const SurfaceCanvas& c, bool) {
        if (!ok_) {
            return;
        }
        if (!ensure_sized_window(canvas_window_size(c))) {
            return;
        }
        pump();
        SDL_SetRenderDrawColor(renderer_, kCanvasBackground.r, kCanvasBackground.g,
                               kCanvasBackground.b, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer_);
        const SurfaceExtent metric = extent();
        // ONE WHOLE PLANE AT A TIME, IN THE PUBLISHER'S ORDER (WIND-2a).
        //
        // Two lists per layer, one picture, and which list a text region lands in is
        // decided by exactly one fact: whether THIS REGION'S BOUNDS hold a row of the face
        // this medium is reporting (`fit_region(...).graphical()`, HD-5). With no face that
        // is false for every region and the quads contain them all as bitmap labels; with a
        // face open it is true for every region big enough, and those are drawn in type. A
        // region one cell tall is smaller than the face's own line, so it stays in the quads
        // -- which is the Inspector's editable row, and which before HD-5 was in neither
        // list and drawn by nobody. So there is still no configuration in which the same
        // words are drawn twice, and still no `if` here to get that wrong.
        //
        // AND THE INTERLEAVING IS THE PLAN'S, NOT THIS LOOP'S. Draining every layer's quads
        // and then every layer's real-face regions is exactly the two global bands WIND-2a
        // removed from the terminal medium, and it would be unmeasurable here -- so
        // `plan_canvas` hands back the layers already ordered and this edge walks them. The
        // only ordering decision left in this file is the one the compiler enforces: the
        // quads of a layer, then its regions, then the next layer.
        const auto execute = [this](const PlanLayer& layer) {
            for (const PlanRect& r : layer.quads) {
                SDL_SetRenderDrawColor(renderer_, r.r, r.g, r.b, SDL_ALPHA_OPAQUE);
                const SDL_FRect fr{static_cast<float>(r.x), static_cast<float>(r.y),
                                   static_cast<float>(r.w), static_cast<float>(r.h)};
                SDL_RenderFillRect(renderer_, &fr);
            }
            for (const PlanTextRegion& p : layer.regions) {
                text_.draw(renderer_, p);
            }
        };
        for (const PlanLayer& layer : plan_canvas(c, metric, drawable())) {
            execute(layer);
        }
        // THE ATTENTION CHIP, AFTER THE WHOLE PICTURE: what this medium makes of
        // the `score` slot IN the window rather than only on it. It is FURNITURE and so it
        // is composed here rather than by the publisher -- the same reason the title is not
        // a plane of the canvas -- and it goes through the identical two lists, so a chip
        // cannot be drawn by a path the picture is not. An empty slot composes an empty
        // layer and draws nothing at all, which is how the indicator disappears when the
        // last condition resolves.
        execute(plan_attention_chip(score_, c, metric, drawable()));
        SDL_RenderPresent(renderer_);
    }

    void note(std::string_view slot, std::string_view text) {
        if (slot == kSlotStatus) {
            status_ = std::string(text);
        } else if (slot == kSlotScore) {
            score_ = std::string(text);
        } else {
            return;
        }
        if (ok_ && window_ != nullptr) {
            SDL_SetWindowTitle(window_, title_of(status_, score_).c_str());
        }
    }

    /// SERVICE THIS WINDOW'S CONVERSATION WITH THE OS, AND TAKE NOTHING OUT OF THE QUEUE.
    ///
    /// TWO JOBS THAT SDL_PollEvent DOES AT ONCE, and separating them is the whole of this
    /// function. `while (SDL_PollEvent(&ev)) {}` — drain the queue and drop it — is the
    /// honest thing for an output-only medium and is the single most destructive thing this
    /// medium could do: SDL_PollEvent REMOVES what it returns, so a skin that kept calling it
    /// would not merely be a second poller, it would be a thief, and every key, click and
    /// close request the reader exists to hear would vanish here microseconds before the
    /// reader looked. That reasoning was right and it still stands.
    ///
    /// What it concluded was wrong. G-1 answered it by doing NOTHING here and leaving the
    /// window to be serviced as a side effect of the reader's own poll — which made a Skin's
    /// liveness depend on which INPUT weave the host happened to boot, two independent
    /// choices a host must keep independent (which medium PAINTS and which weave HEARS are
    /// two rows of a composition, not one -- see `workshop/load_plan.hpp`). Run the SDL skin with the terminal reader and nobody calls into SDL's
    /// event machinery at all: the window comes up, never processes another OS message,
    /// Windows flags it Not Responding, and the tool looks broken. Found live, in the
    /// graphical Workshop, on exactly that pair of flags.
    ///
    /// `SDL_PumpEvents` is the half that was wanted. It gathers the pending OS input INTO
    /// the queue and removes nothing — it is the call SDL_PollEvent makes internally before
    /// it takes anything — and SDL's own header names this situation: "if you are not
    /// polling or waiting for events (e.g. you are filtering them), then you must call
    /// SDL_PumpEvents() to force an event queue update". So the queue still has exactly one
    /// OWNER, meaning one component that REMOVES from it, and that is still not this one.
    /// Whether a reader is loaded no longer decides whether a window is alive.
    ///
    /// It is safe to pump twice. When the SDL reader IS loaded its poll pumps as well; the
    /// call gathers whatever the OS has and is not a state machine anybody can get out of
    /// step. It runs on the host's one thread, which is the thread that brought the video
    /// subsystem up in this medium's constructor.
    void pump() {
        if (!ok_ || window_ == nullptr) {
            return; // nothing has an OS conversation yet
        }
        SDL_PumpEvents();
        notice_unread_queue();
    }

    /// A maker copied text: put it on the REAL platform clipboard (TEXT-0). This is the
    /// medium where the offer lands somewhere every other application on the machine can
    /// paste from. A failure is complained about in SDL's own words and costs nothing
    /// else: the copy is already true inside this process, because it travelled the bus to
    /// get here.
    void clipboard_copy(const std::string& text) {
        if (!ok_) {
            return;
        }
        if (!SDL_SetClipboardText(text.c_str())) {
            complain("SDL_SetClipboardText");
        }
    }

    /// A maker asked to paste: the platform clipboard's text AT THIS MOMENT (QR-11).
    ///
    /// THIS IS THE ONE PLACE IN THE PROCESS THAT READS THE SYSTEM CLIPBOARD, and it runs
    /// only under a `ClipboardTextRequested` — the SDL Input reader stopped watching
    /// clipboard events entirely, because ambient host state is not this application's to
    /// observe. The Medium owns the platform clipboard in both directions: `clipboard_copy`
    /// writes it, this reads it, each on a maker's explicit gesture.
    ///
    /// An empty clipboard — or one holding something that is not text — answers an EMPTY
    /// string, not nullopt: this platform can be read, and "no text" is its current truth,
    /// which a paste honours by inserting nothing. nullopt is reserved for the state where
    /// no read exists at all (no surface claimed, or a medium like the terminal's).
    std::optional<std::string> clipboard_text() {
        if (!ok_) {
            return std::nullopt;
        }
        if (!SDL_HasClipboardText()) {
            return std::string();
        }
        char* text = SDL_GetClipboardText(); // SDL's buffer; freed here, never handed on
        std::string out = text != nullptr ? std::string(text) : std::string();
        SDL_free(text);
        return out;
    }

    /// WHERE THIS WINDOW SITS ON THE DESKTOP (WUX-3) — asked on the beat like the extent,
    /// answered in SDL's own window coordinates, and always about the NORMAL window.
    ///
    /// `SDL_GetWindowPosition` reports wherever the frame currently is, which while
    /// maximized is the work area's corner — a position nobody chose and nobody wants
    /// back. So the normal position is SAMPLED only while the window is not maximized and
    /// REMEMBERED across the maximized stretch: what this answers during a maximize is
    /// "the window is maximized, and its normal frame is at the place you last saw it" —
    /// which is the place the platform returns it to on unmaximize, within the platform's
    /// own tolerance. A window that has never once been observed unmaximized has no
    /// normal position to report, and answers nothing rather than a guess.
    std::optional<SurfacePlacement> placement() {
        if (!ok_ || window_ == nullptr) {
            return std::nullopt;
        }
        const bool maximized = (SDL_GetWindowFlags(window_) & SDL_WINDOW_MAXIMIZED) != 0;
        if (!maximized) {
            int x = 0;
            int y = 0;
            if (SDL_GetWindowPosition(window_, &x, &y)) {
                normal_x_ = x;
                normal_y_ = y;
                have_normal_ = true;
            }
        }
        if (!have_normal_) {
            return std::nullopt;
        }
        return SurfacePlacement{normal_x_, normal_y_, maximized};
    }

    /// A REMEMBERED PLACEMENT, OFFERED BACK (WUX-3): judge it against the desktop that
    /// exists NOW, and apply what is safe.
    ///
    /// The judgment is `placement_within` (skin_sdl_plan.hpp), pure and pinned on every
    /// lane; what this supplies is the live inputs — every current display's USABLE bounds
    /// (`SDL_GetDisplayUsableBounds`: the desktop less the platform's own taskbar/dock
    /// reservations) and the window's current size in the same coordinate space
    /// (`SDL_GetWindowSize`, deliberately not the drawable: positions are window
    /// coordinates, not render pixels). No display truth means no move — an uninformed
    /// move is the blind replay the law refuses — and the maximize is applied AFTER the
    /// position, so the platform's own unmaximize returns the frame to the place this
    /// call put it.
    void place(const SurfacePlacementRemembered& want) {
        if (!ok_ || window_ == nullptr) {
            return;
        }
        std::vector<DesktopSpan> usable;
        int display_count = 0;
        if (SDL_DisplayID* ids = SDL_GetDisplays(&display_count)) {
            for (int i = 0; i < display_count; ++i) {
                SDL_Rect r{};
                if (SDL_GetDisplayUsableBounds(ids[i], &r)) {
                    usable.push_back(DesktopSpan{r.x, r.y, r.w, r.h});
                }
            }
            SDL_free(ids);
        }
        int w = 0;
        int h = 0;
        if (!SDL_GetWindowSize(window_, &w, &h)) {
            return; // a window whose size cannot be asked is not one to move blind
        }
        const std::optional<DesktopPoint> at =
            placement_within(want.x, want.y, w, h, usable);
        if (at.has_value() &&
            !SDL_SetWindowPosition(window_, static_cast<int>(at->x),
                                   static_cast<int>(at->y))) {
            complain("SDL_SetWindowPosition");
        }
        if (want.maximized && !SDL_MaximizeWindow(window_)) {
            complain("SDL_MaximizeWindow");
        }
    }

    /// HOW MUCH ROOM THIS WINDOW HAS, in canvas cells — the one question this
    /// medium answers rather than obeys.
    ///
    /// Measured every time it is asked, from the renderer's own output size, and
    /// floored to whole cells by `extent_of_drawable` (pure, in the plan header,
    /// pinned on every lane). No window and no working renderer means no answer:
    /// {0,0} is "I have no opinion", which the shell turns into silence rather
    /// than into a claim that there is no room.
    ///
    /// AND HOW BIG ONE CHARACTER IS, since HD-1 -- the second half of the same
    /// answer and, unlike the first, one this medium can give only when it has a
    /// real face open. The numbers come from `SdlTypeface`, which measured them
    /// from the opened font; they are never authored here and never guessed. With
    /// no face they are zero, which the vocabulary spells "text is a cell" and
    /// which is exactly what the bitmap letterform draws -- so a publisher
    /// wrapping against this metric is always wrapping against the thing that will
    /// actually be painted.
    ///
    /// AND HOW BIG ONE CANVAS CELL IS, since WUX-6 -- the third half, and the one
    /// that is true of this medium whether or not a face ever opened. See the
    /// assignment below for why it is consulted from the plan's own constant.
    SurfaceExtent extent() const {
        if (!ok_ || window_ == nullptr) {
            return SurfaceExtent{};
        }
        SurfaceExtent e = extent_of_drawable(drawable());
        e.text_advance_px = text_.advance_px();
        e.text_line_px = text_.line_px();
        // AND THE CANVAS'S OWN DEVICE UNIT (WUX-6) -- the third answer, and the only
        // one of the three this medium can give with no face at all. It is
        // `kCanvasCellPx` because that is what `plan_canvas` and `extent_of_drawable`
        // above LAY THIS CANVAS OUT AT, consulted here rather than restated: one
        // owner, so a maker's geometry spelled in pixels and a quad drawn in pixels
        // are the same number by construction. It is reported whether or not a font
        // opened, which is the half of this that the text metric cannot say.
        e.cell_px = kCanvasCellPx;
        return e;
    }

private:
    /// SAY, ONCE, WHEN NOTHING IS TAKING WHAT THIS WINDOW HEARS.
    ///
    /// The window is an ear as well as a surface, and it takes focus. Boot it beside a reader
    /// that is watching something else — the terminal — and a maker types into a window
    /// whose keys nobody collects, including its close box. That is a legitimate pairing of
    /// two independent flags and it is not this file's to refuse; it is this file's to make
    /// legible, the same posture the whole of `complain` above exists for.
    ///
    /// IT MEASURES, IT DOES NOT INFER. Nothing here reads a flag, asks who is registered, or
    /// guesses a reader from the skin's own name — which is the deduction the host refuses to
    /// write down and this medium is in no position to write down for it. It counts the
    /// queue, non-destructively (`SDL_PeepEvents` with a null buffer walks the whole queue and
    /// removes nothing), and a queue that has grown past a thousand events is a queue nobody
    /// is emptying: a live reader drains it completely every beat, so reaching this number
    /// with one running would take a hundred thousand events a second, sustained.
    ///
    /// Once, and then never again — a complaint on every beat would be the noise that teaches
    /// a person to stop reading stderr.
    void notice_unread_queue() {
        if (said_unread_) {
            return;
        }
        const int waiting =
            SDL_PeepEvents(nullptr, 0, SDL_PEEKEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
        if (waiting < kUnreadQueue) {
            return;
        }
        said_unread_ = true;
        std::fprintf(stderr,
                     "zengine-skin-sdl: %d events are queued for this window and nothing is "
                     "taking them.\n"
                     "zengine-skin-sdl: the window still draws, but it is not the ear this run "
                     "is listening with --\n"
                     // WHAT TO LOAD, NOT WHICH FLAG TO TYPE (LOAD-0). This used to name
                     // `--input zengine-input-sdl`, which was one host's command line
                     // spoken by a package that has several hosts -- and that flag does
                     // not exist any more. What a Skin honestly knows is which ROLE has
                     // to be held and by what, which is true for every host that loads it.
                     "zengine-skin-sdl: type at the terminal instead, or run a composition "
                     "that loads `zengine-input-sdl` as `zengine.input`.\n",
                     waiting);
        std::fflush(stderr);
    }

    /// How many unread events make "nobody is draining this" a measurement rather than a
    /// coincidence. A reader empties the queue on each of its own 10ms beats, so a thousand
    /// standing events cannot be a drained queue caught mid-beat.
    static constexpr int kUnreadQueue = 1000;

    bool ensure_window(const zengine::snake::SnakeVisual& v) {
        return ensure_sized_window(window_size_of(v));
    }

    /// THE WINDOW NEVER SHOWS LESS THAN THE PICTURE ASKS FOR, AND IS OTHERWISE THE
    /// PERSON'S.
    ///
    /// One rule, and both halves of it are load-bearing. Before G-2 the window WAS
    /// the picture: created at the asked size and re-sized to every later one, with
    /// no resize handle for anybody to take hold of. That is correct for a publisher
    /// that cannot be told how much room there is, and it is exactly wrong for one
    /// that can — a canvas publisher hearing `SurfaceExtent` sizes itself to the
    /// window, so a medium that then sized the window to the canvas would be two
    /// parties resizing each other. (It converges rather than oscillating, because
    /// a canvas rounds DOWN to whole cells — but it converges by nibbling the
    /// window a few pixels smaller every time somebody drags it, which is the
    /// window fighting the hand that holds it.)
    ///
    /// So: created at the asked size, with that size as its MINIMUM, resizable, and
    /// thereafter grown only by a picture that genuinely does not fit. Every case
    /// this medium actually has falls out of it rather than being special-cased:
    ///
    ///   a canvas publisher that heard the extent  asks for what fits: never grows
    ///   a board (SnakeVisual) that grew mid-run    asks for more: the window grows
    ///   a person dragging the edge                 nothing here answers back
    ///   a person dragging it too small             SDL's own minimum refuses
    ///
    /// THE MINIMUM IS THE FIRST PICTURE'S OWN SIZE, set once and never moved. Moving
    /// it with each picture would re-break the loop the rest of this avoids: a
    /// canvas that follows the window would ratchet the minimum up to whatever the
    /// window last was, and the window could then never be made smaller again.
    bool ensure_sized_window(const PlanSize& want) {
        if (want.w <= 0 || want.h <= 0) {
            return false;
        }
        if (window_ == nullptr) {
            // SDL_WINDOW_RESIZABLE, because a larger window is now a larger usable
            // surface rather than a larger copy of a fixed one: the publisher is
            // told the room (SurfaceExtent) and answers with a picture that fills
            // it. Not SDL_WINDOW_NOT_FOCUSABLE, because this window is an ear as
            // well as a surface — see the header.
            window_ = SDL_CreateWindow(title_of(status_, score_).c_str(),
                                       static_cast<int>(want.w), static_cast<int>(want.h),
                                       SDL_WINDOW_RESIZABLE);
            if (window_ == nullptr) {
                complain("SDL_CreateWindow");
                return false;
            }
            // The floor under every later drag, in the picture's own terms. A
            // failure is not fatal to drawing and not worth a line of stderr: the
            // window still works, it can just be dragged smaller than its picture,
            // and the picture is clipped exactly as the canvas contract says.
            SDL_SetWindowMinimumSize(window_, static_cast<int>(want.w),
                                     static_cast<int>(want.h));
            renderer_ = SDL_CreateRenderer(window_, nullptr);
            if (renderer_ == nullptr) {
                complain("SDL_CreateRenderer");
                SDL_DestroyWindow(window_);
                window_ = nullptr;
                return false;
            }
            // The window exists, so native text events can be turned on — and
            // only whoever holds the window pointer can turn them on, which is
            // why this call is here and not in the Input package. It is window
            // setup: this medium gains no opinion about what text MEANS, and the
            // characters go to the reader like every other event.
            //
            // A failure here is not fatal to drawing, so it is reported and the
            // surface still comes up: a readable window with no typing is a
            // better answer than no window, and the difference must be legible
            // rather than "the letters just do not arrive".
            if (!SDL_StartTextInput(window_)) {
                complain("SDL_StartTextInput");
            }
            // THE FACE IS OPENED WITH THE RENDERER, because it belongs to the
            // renderer: SDL_ttf's text engine caches its glyph atlas in textures
            // this renderer owns, so the two have exactly one lifetime between
            // them. A failure here is reported by `open` itself and changes
            // nothing about whether the window comes up -- a readable-in-bitmap
            // Workshop is a better answer than no Workshop, and the difference is
            // legible on stderr rather than being "the letters look wrong".
            (void)text_.open(renderer_);
            return true;
        }
        const PlanSize have = drawable();
        if (want.w > have.w || want.h > have.h) {
            SDL_SetWindowSize(window_, static_cast<int>(want.w > have.w ? want.w : have.w),
                              static_cast<int>(want.h > have.h ? want.h : have.h));
        }
        return true;
    }

    /// WHAT THIS MEDIUM IS ACTUALLY DRAWING ON, in pixels — asked of SDL, never
    /// remembered.
    ///
    /// `SDL_GetRenderOutputSize` and not a private `size_` field, because after G-2
    /// this medium is no longer the only party changing the number: a person
    /// dragging the window edge changes it, and no message tells this weave they
    /// did. A cached size would be right until the first drag and confidently wrong
    /// after it. It is also the renderer's OWN output size rather than the window's,
    /// which is the space `plan_canvas`'s quads are expressed in — the same number
    /// the drawing uses, asked of the same object.
    PlanSize drawable() const {
        int w = 0;
        int h = 0;
        if (renderer_ == nullptr || !SDL_GetRenderOutputSize(renderer_, &w, &h)) {
            return PlanSize{};
        }
        return PlanSize{w, h};
    }

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    std::int64_t normal_x_ = 0; ///< the NORMAL window's last observed position (WUX-3)...
    std::int64_t normal_y_ = 0;
    bool have_normal_ = false;  ///< ...and whether it has ever been observed at all
    SdlTypeface text_; ///< the real face, when there is one; see skin_sdl_text.hpp
    std::string status_;
    std::string score_;
    bool ok_ = false;
    bool said_unread_ = false; ///< the queue complaint is said once per incarnation
};

using SkinSdl = SkinT<SdlMedium>;

} // namespace

ZEN_EXPORT_WEAVE(SkinSdl)
