// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_WINDOW_WINDOW_HPP
#define ZENGINE_GRAPHICS_WINDOW_WINDOW_HPP

// A window, over the medium it shows on: the whole job of showing what its areas' owners drew. It
// keeps each area's nodes, records and presented elements in its state, presents an element from
// code it loads, has what changed painted with the area's skin, routes the pointer, gives keys and
// their text to the areas that hear them, and tells each owner its room. What it keeps painted,
// how it shows it and what it reads are its Medium's: characters on a terminal
// (graphics/window/window_terminal.hpp), light in an OS window (graphics/window/window_sdl.hpp).

#include "graphics/painting.hpp"
#include "graphics/vocabulary.hpp"
#include "graphics/window/compose.hpp"
#include "graphics/window/seams.hpp"
#include "graphics/window/store.hpp"

#include "input/translate.hpp"
#include "input/vocabulary.hpp"
#include "manual/vocabulary.hpp"
#include "timer/binding.hpp"

#include <zen/serialize.hpp>
#include <zen/weave.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace zengine::graphics::window {

/// How often the window reads its medium: the Input weave's own beat.
inline constexpr const char* kPumpTimerId = "zengine.window.pump";
inline constexpr std::int64_t kPumpBeatMs = 10;

/// What a window keeps across a reload: its incarnation, its areas with everything drawn in them,
/// the element code offered for each type, and the next handle it names an element by. A press it
/// holds is not kept: a reload ends every hold, untold, and a release goes where it lands.
struct WindowState {
    std::int64_t epoch = 0;
    std::vector<WinArea> areas;
    std::vector<ElementCodeOffered> codes;
    std::int64_t next_handle = 1;
    std::int64_t refusals = 0;
    ZEN_SHAPE(WindowState, 8, ZEN_FIELD(epoch), ZEN_FIELD(areas), ZEN_FIELD(codes),
              ZEN_FIELD(next_handle), ZEN_FIELD(refusals));
};

/// A window's size in cells.
struct CellExtent {
    std::int64_t cols = 0, rows = 0;
};

/// A window's size in pixels; non-positive is no window to measure.
struct PixelSize {
    std::int64_t width = 0, height = 0;
    bool measured() const { return width > 0 && height > 0; }
    friend bool operator==(const PixelSize&, const PixelSize&) = default;
};

/// A window's room as it offers it, in pixels; none where it has nothing to measure.
struct PixelRoom {
    std::int64_t width = 0, height = 0;
};

/// Where a desktop window sits on its desktop: its normal (unmaximized) top-left, in the desktop's
/// own units, and whether it is maximized now.
struct WindowPlacementValue {
    std::int64_t x = 0;
    std::int64_t y = 0;
    bool maximized = false;
    friend bool operator==(const WindowPlacementValue&, const WindowPlacementValue&) = default;
};

/// A picture read back from what the window last presented: `text/cells` on a terminal (`cell_px`
/// 0, `width` and `height` in cells, a row of `width` bytes a line) or `image/bmp` on an OS window
/// (`cell_px` the light cell pitch, `width` and `height` in pixels).
struct CapturedWindowPicture {
    std::int64_t width = 0;
    std::int64_t height = 0;
    std::int64_t cell_px = 0;
    std::string format;
    std::string bytes;
    friend bool operator==(const CapturedWindowPicture&, const CapturedWindowPicture&) = default;
};

template <class Medium>
class Window;

template <class Medium>
using WindowBase = typename zengine::With<
    zengine::manual::ManualFacet<Medium::kManual>>::template Weave<Window<Medium>, WindowState,
    loom::Accept<Drawn, ElementPut, ElementChange, AreaIntroduced, AreaWithdrawn,
                 ElementCodeOffered, GraphicsReady, WindowShowAsked, AreaShowAsked,
                 zengine::input::AttributedInput, ClipboardCopy, ClipboardTextRequested,
                 SurfacePlacementRemembered, PumpSurface, SurfaceCaptureRequested,
                 SurfaceCaptureChunkRequested, WindowShaped>,
    loom::Emit<DrawnRefused, WindowOffered, AreaRoom, AreaKeyPressed, AreaKeyReleased,
               AreaTextEntered, ActionRequested, AreaButtonChanged, AreaPointerMoved,
               AreaWheelTurned, AreaPressed, AreaHoldEnded, AreaPointerLeft, AreaHandRefused,
               WindowShown, AreaShown, AreaSettled,
               SurfaceCloseRequested, SurfacePlacement, ClipboardText, SurfaceCaptured,
               SurfaceCaptureChunk, loom::Refused>,
    zengine::timer::TimedWeave>;

/// The window over `Medium`, which names it (`kWindow`), carries its manual (`kManual`), and
/// keeps, paints, shows, lands and reads what the window asks of it.
template <class Medium>
class Window : public WindowBase<Medium> {
public:
    using WindowBase<Medium>::on;

    Window() : pump_(declare_pump()), next_press_(first_press()) {}
    template <class Device>
    explicit Window(Device device)
        : medium_(std::move(device)), pump_(declare_pump()), next_press_(first_press()) {}

    // ---- drawing -------------------------------------------------------------------------

    void on(const Drawn& d, loom::Mail& mail) {
        WinArea* a = find(d.area);
        if (a == nullptr || !mail.authored_from_role(a->owner)) {
            refuse(mail, d.area,
                   a == nullptr ? "window '" + std::string(Medium::kWindow) +
                                      "' shows no area '" + d.area + "'"
                                : "area '" + d.area + "' is drawn only by its owner, " + a->owner);
            settle(mail, d.more);
            return;
        }
        std::string why;
        if (!draw(*a, Drawer{}, d, why)) {
            refuse(mail, d.area, why);
        }
        settle(mail, d.more);
    }

    void on(const ElementPut& e, loom::Mail& mail) {
        put(mail, e);
        settle(mail, e.more);
    }

    void on(const ElementChange& c, loom::Mail& mail) {
        change(mail, c);
        settle(mail, c.more);
    }

    // ---- what the directory says --------------------------------------------------------------

    /// An area introduced, or introduced again: kept, painted again where it moved or another skin
    /// paints it, emptied when another owner draws in it, and its owner told its room. A press held
    /// for it ends when it changes hands, and the pointer resting in it leaves it, each told to the
    /// owner it had; so does a press held for an area it now stands in front of (`end_covered`).
    void on(const AreaIntroduced& i, loom::Mail& mail) {
        if (!mail.authored_from_role(kGraphicsRole)) {
            return;
        }
        WinArea* a = find(i.area);
        if (a == nullptr) {
            if (this->state_.areas.size() >= kMaxAreas) {
                note("the window shows " + std::to_string(kMaxAreas) + " areas: '" + i.area +
                     "' is not shown");
                return;
            }
            this->state_.areas.push_back(WinArea{});
            a = &this->state_.areas.back();
            a->area = i.area;
        }
        const bool fresh = a->owner.empty() && a->nodes.empty() && a->records.empty();
        const bool moved = a->x != i.x || a->y != i.y || a->w != i.w || a->h != i.h ||
                           a->order != i.order;
        const bool resized = a->w != i.w || a->h != i.h;
        const bool repaint = a->library != i.library || a->skin != i.skin;
        const bool handed = a->owner != i.owner;
        const std::string old_library = a->library;
        if (moved || repaint || handed) {
            medium_.cover_area(*a);
            touch(*a);
        }
        if (handed) {
            medium_.drop_area(a->area);
            a->nodes.clear();
            a->records.clear();
            a->presented.clear();
            a->drawing = 0;
            end_holds(mail, a->area, /*withdrawn=*/false);
            leave_hovers(mail, *a);
        }
        a->owner = i.owner;
        a->arranger = i.arranger;
        a->x = i.x;
        a->y = i.y;
        a->w = i.w;
        a->h = i.h;
        a->order = i.order;
        a->keys = i.keys;
        a->pointer = i.pointer;
        a->weaver_only = i.weaver_only;
        a->wheel = i.wheel;
        a->right = i.right;
        a->followed = i.followed;
        a->releases = i.releases;
        a->text = i.text;
        a->skin = i.skin;
        a->library = i.library;
        if (moved || repaint || handed) {
            touch(*a);
        }
        // ITS ROOM ASKS A WHOLE DRAWING, which it waits for until the owner draws.
        if (!a->owner.empty() && (fresh || handed || resized || a->nodes.empty())) {
            waiting_.insert(a->area);
        }
        if (repaint) {
            remake(*a);
            close_unused(old_library);
            touch(*a);
        } else if (moved) {
            medium_.cover_area(*a);
        }
        settle(mail, false);
        room(mail, *a);
        end_covered(mail);
    }

    void on(const AreaWithdrawn& w, loom::Mail& mail) {
        if (!mail.authored_from_role(kGraphicsRole)) {
            return;
        }
        WinArea* a = find(w.area);
        if (a == nullptr) {
            return;
        }
        medium_.cover_area(*a);
        touch(*a);
        const std::string library = a->library;
        medium_.drop_area(w.area);
        end_holds(mail, w.area, /*withdrawn=*/true);
        std::erase_if(hovered_, [&](const auto& h) { return h.second == w.area; });
        waiting_.erase(w.area);
        versions_.erase(w.area);
        told_.erase(w.area);
        std::erase_if(this->state_.areas, [&](const WinArea& k) { return k.area == w.area; });
        close_unused(library);
        settle(mail, false);
    }

    /// The load plan's element code, through the directory. New code for a type the window
    /// presents draws each such element again, from the state the window kept.
    void on(const ElementCodeOffered& o, loom::Mail& mail) {
        if (!mail.authored_from_role(kGraphicsRole)) {
            return;
        }
        auto& codes = this->state_.codes;
        const auto it = std::find_if(codes.begin(), codes.end(),
                                     [&](const ElementCodeOffered& k) { return k.type == o.type; });
        if (it != codes.end() && it->library == o.library) {
            return;
        }
        const std::string old = it == codes.end() ? std::string() : it->library;
        if (it == codes.end()) {
            codes.push_back(o);
        } else {
            it->library = o.library;
        }
        if (old.empty()) {
            return;
        }
        codes_.close(old);
        for (WinArea& a : this->state_.areas) {
            for (Presented& p : a.presented) {
                if (p.type == o.type) {
                    act(mail, a, p, ElementEvent{event::kRedraw, p.handle, p.path, p.state, 0, {}, {}});
                }
            }
        }
        settle(mail, false);
    }

    /// A new directory: this window says itself again; its areas stay until it says otherwise.
    void on(const GraphicsReady&, loom::Mail& mail) {
        if (mail.authored_from_role(kGraphicsRole)) {
            offer(mail);
        }
    }

    void on(const WindowShowAsked&, loom::Mail& mail) {
        WindowShown s;
        s.window = Medium::kWindow;
        s.epoch = this->state_.epoch;
        s.rows = medium_.rows_shown(this->state_.areas);
        s.areas = static_cast<std::int64_t>(this->state_.areas.size());
        for (const WinArea& a : this->state_.areas) {
            s.nodes += static_cast<std::int64_t>(a.nodes.size());
            s.elements += static_cast<std::int64_t>(a.presented.size());
        }
        s.showings = medium_.showings();
        s.device_bytes = medium_.device_bytes();
        s.painter_calls = painters_.calls;
        s.element_calls = codes_.calls;
        s.refusals = this->state_.refusals;
        s.last_refusal = last_refusal_;
        (void)mail.answer(s);
    }

    /// What one area shows, to the office that placed it: its words and named parts as the
    /// window holds them painted, which its next showing shows at the latest; nothing more when the
    /// asker's `since` is still the area's version.
    void on(const AreaShowAsked& asked, loom::Mail& mail) {
        const WinArea* a = find(asked.area);
        if (a == nullptr) {
            (void)mail.answer(loom::Refused{"window '" + std::string(Medium::kWindow) +
                                            "' shows no area '" + asked.area + "'"});
            return;
        }
        if (a->arranger.empty() || !mail.authored_from_role(a->arranger)) {
            (void)mail.answer(loom::Refused{"area '" + asked.area +
                                            "' is read only by the office that placed it"});
            return;
        }
        AreaShown s;
        s.window = Medium::kWindow;
        s.area = a->area;
        s.epoch = this->state_.epoch;
        s.version = version_of(a->area);
        s.waiting = waiting_.count(a->area) != 0;
        const TextMeasure text = medium_.text();
        s.advance = text.advance;
        s.line = text.line;
        if (asked.since != 0 && asked.since == s.version) {
            s.same = true;
            (void)mail.answer(s);
            return;
        }
        if (const std::string why =
                medium_.read_area(this->state_.areas, *a, a->arranger, s.words, s.parts);
            !why.empty()) {
            (void)mail.answer(loom::Refused{why});
            return;
        }
        (void)mail.answer(s);
    }

    /// A moment the input office injected for a participant, its hand named: a pointer moment is
    /// landed by this window's own judge, as one its platform gave, and told as that hand's. The
    /// weaver's own moments are the platform's, which the window reads itself; keys are not taken.
    void on(const zengine::input::AttributedInput& in, loom::Mail& mail) {
        namespace i = zengine::input;
        if (!mail.authored_from_role(i::kInputRole) || in.local || in.actor <= 0) {
            return;
        }
        const Hand hand{false, in.actor};
        const i::InjectedEvent& e = in.event;
        if (e.kind == "PointerButton") {
            button(mail, i::PointerButton{e.button, e.pressed, e.x, e.y, e.space, e.modifiers},
                   hand);
        } else if (e.kind == "PointerMoved") {
            motion(mail, i::PointerMoved{e.x, e.y, e.dx, e.dy, e.space, e.modifiers}, hand);
        } else if (e.kind == "PointerWheel") {
            wheel(mail, i::PointerWheel{e.wheel_dx, e.wheel_dy, e.x, e.y, e.space, e.modifiers},
                  hand);
        } else {
            return;
        }
        settle(mail, false);
    }

    // ---- what the window answers for its platform ----------------------------------------------

    /// A weaver copied text: handed to the medium, which sets the platform's clipboard as far as it
    /// can. No showing, and no answer: a terminal cannot confirm its copy sequence.
    void on(const ClipboardCopy& c, loom::Mail&) { medium_.clipboard_copy(c.text); }

    /// A weaver asked to paste: the platform clipboard read now, answered to the asker alone.
    void on(const ClipboardTextRequested&, loom::Mail& mail) {
        const std::optional<std::string> text = medium_.clipboard_text();
        (void)mail.answer(ClipboardText{text.has_value(), text.value_or(std::string())});
    }

    /// A remembered placement offered back: the medium judges it, and where the window is after is
    /// the next `SurfacePlacement`; a resize it made offers the window again.
    void on(const SurfacePlacementRemembered& p, loom::Mail& mail) {
        medium_.place(WindowPlacementValue{p.x, p.y, p.maximized});
        report_placement(mail);
        if (medium_.remeasure()) {
            touch_all();
            offer(mail);
        }
    }

    /// SHAPED BY AN OFFICE THAT ARRANGES IN IT: its title and its size, the medium's to set. A
    /// shaping from any other office, or naming another window, changes nothing. A size that
    /// changed offers the window again, so its arrangers place for it.
    void on(const WindowShaped& s, loom::Mail& mail) {
        if (s.window != Medium::kWindow) {
            return;
        }
        const auto& areas = this->state_.areas;
        if (std::none_of(areas.begin(), areas.end(), [&mail](const WinArea& a) {
                return !a.arranger.empty() && mail.authored_from_role(a.arranger);
            })) {
            return;
        }
        medium_.shape(whole_title(s.title), PixelSize{s.width, s.height});
        if (medium_.remeasure()) {
            offer(mail);
        }
    }

    /// A title as a window takes it: each control character a space, and at most
    /// `kMaxWindowTitleBytes`, cut where a character begins.
    static std::string whole_title(const std::string& title) {
        std::string out = title.substr(0, kMaxWindowTitleBytes);
        if (out.size() < title.size()) {
            std::size_t end = out.size();
            while (end > 0 && (static_cast<unsigned char>(title[end]) & 0xC0u) == 0x80u) {
                --end;
            }
            out.resize(end);
        }
        for (char& c : out) {
            const auto u = static_cast<unsigned char>(c);
            if (u < 0x20u || u == 0x7Fu) {
                c = ' ';
            }
        }
        return out;
    }

    /// The beat's work, asked for now.
    void on(const PumpSurface&, loom::Mail& mail) { service_pump(mail); }

    /// A picture of what the window last presented: now, or on the showing that passes
    /// `after_frame`, the window's showings its frame count; one may wait at a time.
    void on(const SurfaceCaptureRequested& asked, loom::Mail& mail) {
        if (asked.after_frame >= 0 && asked.after_frame >= medium_.showings()) {
            if (pending_capture_.valid()) {
                (void)mail.answer(refused_capture("a capture is already waiting for frame " +
                                                  std::to_string(pending_after_ + 1) +
                                                  "; one waits at a time"));
                return;
            }
            pending_capture_ = mail.defer_answer();
            if (!pending_capture_.valid()) {
                (void)mail.answer(refused_capture("this delivery cannot defer its answer"));
                return;
            }
            pending_after_ = asked.after_frame;
            return;
        }
        (void)mail.answer(take_capture());
    }

    /// One chunk of the retained picture, or a refusal naming which picture is retained now.
    void on(const SurfaceCaptureChunkRequested& asked, loom::Mail& mail) {
        if (retained_.number == 0 || asked.capture != retained_.number) {
            (void)mail.answer(loom::Refused{
                retained_.number == 0
                    ? std::string("no picture is retained; ask for a capture first")
                    : "capture " + std::to_string(asked.capture) +
                          " is gone; the retained one is " + std::to_string(retained_.number)});
            return;
        }
        const std::int64_t total = static_cast<std::int64_t>(retained_.picture.bytes.size());
        if (asked.offset < 0 || asked.offset > total) {
            (void)mail.answer(loom::Refused{"offset " + std::to_string(asked.offset) +
                                            " is outside a picture of " + std::to_string(total) +
                                            " bytes"});
            return;
        }
        SurfaceCaptureChunk chunk;
        chunk.capture = retained_.number;
        chunk.offset = asked.offset;
        chunk.total = total;
        const std::int64_t n = (std::min)(total - asked.offset, kCaptureChunkBytes);
        const auto begin = retained_.picture.bytes.begin() + asked.offset;
        chunk.data.assign(begin, begin + n);
        (void)mail.answer(chunk);
    }

    /// Text copied to the platform's clipboard: an OS window's, or a terminal's by its copy
    /// sequence, which no terminal confirms.
    void clipboard_copy(std::string_view text) { medium_.clipboard_copy(text); }
    /// The platform clipboard's text now, read only when asked: empty when it holds none, and none
    /// where no read exists (a terminal, or an OS window with no window).
    std::optional<std::string> clipboard_text() { return medium_.clipboard_text(); }
    /// Where the window sits on its desktop: none on a terminal, or for an OS window with no window
    /// or one never seen unmaximized.
    std::optional<WindowPlacementValue> placement() { return medium_.placement(); }
    /// A remembered placement offered back: kept whole on a display; a terminal takes none.
    void place(const WindowPlacementValue& want) { medium_.place(want); }
    /// What the window last presented, read back; none before its first showing.
    std::optional<CapturedWindowPicture> capture() { return medium_.capture(); }

    // ---- life ---------------------------------------------------------------------------------

    /// Started, or started again: a fresh window draws its incarnation (a reload carries the one
    /// it had), takes its medium, paints what its state keeps, shows it, and offers itself.
    void on_timed_activation(const loom::Activated&, loom::Mail& mail) {
        if (this->state_.epoch == 0) {
            this->state_.epoch = token();
        }
        medium_.claim();
        medium_.clear();
        for (WinArea& a : this->state_.areas) {
            remake(a);
        }
        touch_all();
        settle(mail, false);
        offer(mail);
    }

private:
    auto declare_pump() {
        return this->timers().repeat(kPumpTimerId, std::chrono::milliseconds(kPumpBeatMs),
                                     &Window::on_pump);
    }

    // ---- input --------------------------------------------------------------------------------

    /// A point of the window in pixels: one in cells stands at its cell's centre.
    struct Point {
        std::int64_t x = 0, y = 0;
    };
    static Point point_of(std::int64_t x, std::int64_t y, std::int64_t space) {
        if (space != zengine::input::space::kCells) {
            return Point{x, y};
        }
        return Point{sum(times_cell(x), kCellPx / 2), sum(times_cell(y), kCellPx / 2)};
    }

    void on_pump(const zengine::timer::TimerFired&, loom::Mail& mail) { service_pump(mail); }

    /// The beat: the medium measured, its queue read, its place said, its room offered, what it
    /// read sent on in the order read, and a close its desktop asked for published. A size is read
    /// at once and a maximize only with the queue, so a maximize is said before the room it brings.
    /// A key, its release and its text go to each owner placed to hear that moment; a button, a move
    /// and the wheel as `button`, `motion` and `wheel` say; moves read together go as one, summed.
    // WL-SESSION-09 -- agents/workshop/window-file.md
    void service_pump(loom::Mail& mail) {
        namespace in = zengine::input;
        const bool remeasured = medium_.remeasure();
        const std::vector<in::InputEvent> read = medium_.poll();
        report_placement(mail);
        if (remeasured) {
            touch_all();
            offer(mail);
        }
        for (std::size_t i = 0; i < read.size(); ++i) {
            const in::InputEvent& e = read[i];
            if (const auto* k = std::get_if<in::KeyPressed>(&e)) {
                to_keys(mail, AreaKeyPressed{{}, k->scancode, k->name, k->modifiers}, Hand{});
            } else if (const auto* r = std::get_if<in::KeyReleased>(&e)) {
                to_keys(mail, AreaKeyReleased{{}, r->scancode, r->name, r->modifiers}, Hand{});
            } else if (const auto* t = std::get_if<in::TextEntered>(&e)) {
                to_keys(mail, AreaTextEntered{{}, t->text}, Hand{});
            } else if (const auto* b = std::get_if<in::PointerButton>(&e)) {
                button(mail, *b, Hand{});
            } else if (const auto* m = std::get_if<in::PointerMoved>(&e)) {
                in::PointerMoved run = *m;
                while (i + 1 < read.size()) {
                    const auto* next = std::get_if<in::PointerMoved>(&read[i + 1]);
                    if (next == nullptr || next->space != run.space) {
                        break;
                    }
                    run.x = next->x;
                    run.y = next->y;
                    run.dx = sum(run.dx, next->dx);
                    run.dy = sum(run.dy, next->dy);
                    run.modifiers = next->modifiers;
                    ++i;
                }
                motion(mail, run, Hand{});
            } else if (const auto* w = std::get_if<in::PointerWheel>(&e)) {
                wheel(mail, *w, Hand{});
            }
        }
        if (medium_.take_close()) {
            (void)mail.as_role(office()).publish(SurfaceCloseRequested{});
        }
        settle(mail, false);
    }

    /// A hand: the weaver's own, which the platform gives, or a participant's the input office
    /// names. Each holds its own presses and rests its own pointer.
    struct Hand {
        bool local = true;
        std::int64_t actor = 0;
        std::int64_t key() const { return local ? 0 : actor; }
    };

    /// Whether an area was placed to hear this key moment: a key, its release, its text.
    static bool hears(const WinArea& a, const AreaKeyPressed&) { return a.keys; }
    static bool hears(const WinArea& a, const AreaKeyReleased&) { return a.releases; }
    static bool hears(const WinArea& a, const AreaTextEntered&) { return a.text; }

    /// A key, its release or its text, to the owner of each area placed to hear that moment, front
    /// to back, naming that area and the hand that made it.
    template <class Typed>
    void to_keys(loom::Mail& mail, Typed typed, const Hand& hand) {
        typed.local = hand.local;
        typed.actor = hand.actor;
        for (const WinArea* a : front_to_back(this->state_.areas)) {
            if (hears(*a, typed) && !a->owner.empty()) {
                typed.area = a->area;
                (void)mail.as_role(office()).send_to_role(a->owner, typed);
            }
        }
    }

    /// A button pressed where the medium lands it: the element presented there, if its code takes a
    /// left press; else, in an area not placed to hear the pointer, the action its record names, to
    /// the owner; else the press itself, held for that area and told to its arranger. A release goes
    /// to the area that hand's press is held for; one whose area went while it was held, to nobody;
    /// one no hold names, where it lands. A guest's press
    /// in an area placed for the weaver's hand alone reaches nobody, and the arranger is told.
    void button(loom::Mail& mail, const zengine::input::PointerButton& b, const Hand& hand) {
        const Point at = point_of(b.x, b.y, b.space);
        const HoldKey key{hand.key(), b.button};
        if (!b.pressed) {
            const auto held = holds_.find(key);
            if (held != holds_.end()) {
                const Hold hold = held->second;
                holds_.erase(held);
                const WinArea* a = find(hold.area);
                if (a != nullptr && a->pointer && !a->owner.empty()) {
                    tell(mail, *a, b, at, hold.press, hand,
                         medium_.land(this->state_.areas, b.x, b.y, b.space, false, b.button));
                }
                return;
            }
            if (ended_.erase(key) != 0) {
                return; // its hold was ended: the release is no one's
            }
            const Landed l = medium_.land(this->state_.areas, b.x, b.y, b.space, false, b.button);
            if (l.area != nullptr && l.area->pointer && !l.area->owner.empty() &&
                reaches(*l.area, hand)) {
                AreaButtonChanged told{l.area->area, b.button, false, l.x, l.y, b.modifiers,
                                       hand.local, hand.actor};
                say_under(told, l);
                to_owner(mail, *l.area, told, 0);
            }
            return;
        }
        if (holds_.count(key) != 0) {
            end_hold(mail, key); // its release was never read
        }
        ended_.erase(key);
        const Landed l = medium_.land(this->state_.areas, b.x, b.y, b.space, false, b.button);
        if (l.area == nullptr) {
            return;
        }
        WinArea* a = find(l.area->area);
        if (!reaches(*a, hand)) {
            refuse_hand(mail, *a, at, hand);
            return;
        }
        if (b.button == 1) {
            Presented* p = presented_at(*a, l.path);
            if (p != nullptr && act(mail, *a, *p,
                                    ElementEvent{event::kPress, p->handle, p->path, p->state, 0,
                                                 {}, l.path})) {
                return;
            }
            const ElementRecord* r = record_of(*a, l.path);
            if (!a->pointer && p == nullptr && r != nullptr && !r->action.empty()) {
                (void)mail.as_role(office()).send_to_role(a->owner,
                                                          ActionRequested{a->area, r->action});
                return;
            }
        }
        if (!a->pointer || a->owner.empty()) {
            return;
        }
        const std::uint64_t press = next_press_++;
        holds_[key] = Hold{a->area, at.x, at.y, press};
        tell(mail, *a, b, at, press, hand, l);
    }

    /// WHAT THE WINDOW DREW UNDER THE HAND, said with a button's moment: the area that shows it,
    /// the part its top node there is named by, and that area's drawing number.
    static void say_under(AreaButtonChanged& told, const Landed& l) {
        if (l.under == nullptr) {
            return;
        }
        told.under = l.under->area;
        const ElementRecord* r = record_of(*l.under, l.under_path);
        told.part = r != nullptr ? r->action : std::string();
        told.drawing = l.under->drawing;
    }

    /// Whether a hand's moment may reach an area: the weaver's always, a guest's where the area
    /// was not placed for the weaver's hand alone.
    static bool reaches(const WinArea& a, const Hand& hand) { return hand.local || !a.weaver_only; }

    /// A guest's press or wheel refused in an area placed for the weaver's hand alone: told to the
    /// area's arranger, at the window's point, and to nobody else.
    void refuse_hand(loom::Mail& mail, const WinArea& a, const Point& at, const Hand& hand) {
        if (!a.arranger.empty()) {
            (void)mail.as_role(office()).send_to_role(
                a.arranger,
                AreaHandRefused{std::string(Medium::kWindow), a.area, hand.actor, at.x, at.y});
        }
    }

    /// Whether the area's arranger follows it, and so is told what the window says of it unasked.
    static bool followed(const WinArea& a) { return !a.arranger.empty() && a.followed; }

    /// A button's moment, to the owner of the area its press is held for or landed in at the
    /// area's point, with what the window drew under the hand (`l`), and to the area's arranger
    /// following it, when another office, at the window's: both under the press's number, and both
    /// naming the hand.
    void tell(loom::Mail& mail, const WinArea& a, const zengine::input::PointerButton& b,
              const Point& at, std::uint64_t press, const Hand& hand, const Landed& l) {
        AreaButtonChanged told{a.area, b.button, b.pressed, sum(at.x, -a.x), sum(at.y, -a.y),
                               b.modifiers, hand.local, hand.actor};
        say_under(told, l);
        to_owner(mail, a, told, press);
        if (followed(a) && a.arranger != a.owner) {
            (void)mail.as_role(office()).send_to_role(
                a.arranger,
                AreaPressed{std::string(Medium::kWindow), a.area, b.button, b.pressed, at.x, at.y,
                            hand.local, hand.actor, told.under, told.part, told.drawing},
                press);
        }
    }

    /// A move: to each area a press the hand holds holds the pointer for, at that area's point;
    /// with none held, to the area it lands in when that area hears the pointer and the hand reaches
    /// it, the area the hand's last unheld move landed in told first that the pointer left it.
    void motion(loom::Mail& mail, zengine::input::PointerMoved m, const Hand& hand) {
        namespace in = zengine::input;
        if (m.space == in::space::kCells) {
            m.dx = times_cell(m.dx);
            m.dy = times_cell(m.dy);
        }
        const Point at = point_of(m.x, m.y, m.space);
        std::set<std::string> told;
        bool holding = false;
        for (const auto& [key, hold] : holds_) {
            if (key.first != hand.key()) {
                continue;
            }
            holding = true;
            const WinArea* a = find(hold.area);
            if (a == nullptr || !a->pointer || a->owner.empty() ||
                !told.insert(hold.area).second) {
                continue;
            }
            to_owner(mail, *a,
                     AreaPointerMoved{a->area, sum(at.x, -a->x), sum(at.y, -a->y), m.dx, m.dy,
                                      m.modifiers, hand.local, hand.actor},
                     hold.press);
        }
        if (holding) {
            return;
        }
        const Landed l = medium_.land(this->state_.areas, m.x, m.y, m.space);
        const WinArea* over = l.area != nullptr && l.area->pointer && !l.area->owner.empty() &&
                                      reaches(*l.area, hand)
                                  ? l.area
                                  : nullptr;
        const std::string now = over != nullptr ? over->area : std::string();
        std::string& hovered = hovered_[hand.key()];
        if (!hovered.empty() && hovered != now) {
            const WinArea* left = find(hovered);
            if (left != nullptr) {
                to_owner_and_arranger(mail, *left, AreaPointerLeft{hovered});
            }
        }
        hovered = now;
        if (now.empty()) {
            hovered_.erase(hand.key());
        }
        if (over != nullptr) {
            to_owner(mail, *over,
                     AreaPointerMoved{over->area, l.x, l.y, m.dx, m.dy, m.modifiers, hand.local,
                                      hand.actor},
                     0);
        }
    }

    /// The wheel, to the area it lands in when that area hears the pointer, past an area in front
    /// that hears the pointer and lets the wheel through; a guest's, in an area placed for the
    /// weaver's hand alone, refused as its press is.
    void wheel(loom::Mail& mail, const zengine::input::PointerWheel& w, const Hand& hand) {
        const Landed l = medium_.land(this->state_.areas, w.x, w.y, w.space, true);
        if (l.area == nullptr || !l.area->pointer || l.area->owner.empty()) {
            return;
        }
        if (!reaches(*l.area, hand)) {
            refuse_hand(mail, *l.area, point_of(w.x, w.y, w.space), hand);
            return;
        }
        to_owner(mail, *l.area,
                 AreaWheelTurned{l.area->area, w.dx, w.dy, l.x, l.y, w.modifiers, hand.local,
                                 hand.actor},
                 0);
    }

    template <class Moment>
    void to_owner(loom::Mail& mail, const WinArea& a, const Moment& moment, std::uint64_t press) {
        (void)mail.as_role(office()).send_to_role(a.owner, moment, press);
    }

    /// What became of the pointer in an area, to its owner and to its arranger when another office:
    /// no other office hears where the pointer goes.
    template <class Said>
    void to_owner_and_arranger(loom::Mail& mail, const WinArea& a, const Said& said) {
        if (!a.owner.empty()) {
            (void)mail.as_role(office()).send_to_role(a.owner, said);
        }
        if (followed(a) && a.arranger != a.owner) {
            (void)mail.as_role(office()).send_to_role(a.arranger, said);
        }
    }

    /// Every hand's pointer resting in an area, told that it left: the area changed hands.
    void leave_hovers(loom::Mail& mail, const WinArea& a) {
        bool told = false;
        for (auto it = hovered_.begin(); it != hovered_.end();) {
            if (it->second != a.area) {
                ++it;
                continue;
            }
            if (!told) {
                to_owner_and_arranger(mail, a, AreaPointerLeft{a.area});
                told = true;
            }
            it = hovered_.erase(it);
        }
    }

    /// A press held for an area, ended with no release: forgotten, and told to the area's owner
    /// and arranger while the area still names them.
    void end_hold(loom::Mail& mail, const std::pair<std::int64_t, std::int64_t>& key,
                  bool gone = false) {
        const auto held = holds_.find(key);
        if (held == holds_.end()) {
            return;
        }
        const std::string area = held->second.area;
        holds_.erase(held);
        if (gone) {
            ended_.insert(key);
        }
        const WinArea* a = find(area);
        if (a != nullptr) {
            to_owner_and_arranger(mail, *a, AreaHoldEnded{area, key.second});
        }
    }

    /// Every press held for `area` ended: it was withdrawn -- its release then reaching nobody --
    /// or handed to another owner, who hears the release where it lands.
    void end_holds(loom::Mail& mail, const std::string& area, bool withdrawn) {
        std::vector<std::pair<std::int64_t, std::int64_t>> ended;
        for (const auto& [key, hold] : holds_) {
            if (hold.area == area) {
                ended.push_back(key);
            }
        }
        for (const auto& key : ended) {
            end_hold(mail, key, withdrawn);
        }
    }

    /// Every press held for an area that another area placed to hear the pointer now stands in
    /// front of, where the press was made.
    void end_covered(loom::Mail& mail) {
        std::vector<std::pair<std::int64_t, std::int64_t>> ended;
        for (const auto& [key, hold] : holds_) {
            const WinArea* held = find(hold.area);
            if (held == nullptr) {
                continue;
            }
            for (const WinArea& a : this->state_.areas) {
                if (a.area != held->area && a.pointer && (key.second != 3 || a.right) &&
                    a.order > held->order && hold.x >= a.x && hold.y >= a.y &&
                    hold.x < a.x + a.w && hold.y < a.y + a.h) {
                    ended.push_back(key);
                    break;
                }
            }
        }
        for (const auto& key : ended) {
            end_hold(mail, key);
        }
    }

    // ---- elements -----------------------------------------------------------------------------

    void put(loom::Mail& mail, const ElementPut& e) {
        WinArea* a = find(e.area);
        if (a == nullptr || !mail.authored_from_role(a->owner)) {
            refuse(mail, e.area, "the sender does not draw in area '" + e.area + "' here");
            return;
        }
        auto it = std::find_if(a->presented.begin(), a->presented.end(),
                               [&](const Presented& p) { return p.path == e.path; });
        if (it == a->presented.end() && !mine(*a, Drawer{}, e.path)) {
            refuse(mail, e.area, "'" + e.path + "' is not the sender's to present an element at");
            return;
        }
        std::string why;
        const ElementCodes::Code* code = code_for(e.type, why);
        if (code == nullptr) {
            refuse(mail, e.area, "element '" + e.path + "': " + why);
            return;
        }
        if (it == a->presented.end()) {
            if (a->presented.size() >= kMaxElementsPerArea) {
                refuse(mail, e.area, "area '" + e.area + "' presents " +
                                         std::to_string(kMaxElementsPerArea) +
                                         " elements already: '" + e.path + "' is not presented");
                return;
            }
            a->presented.push_back(Presented{e.path, e.type, e.ref, this->state_.next_handle++, {}});
            it = std::prev(a->presented.end());
        } else if (it->type != e.type) {
            it->type = e.type;
            it->state.clear();
        }
        Presented trial = *it;
        trial.ref = e.ref;
        if (!act(mail, *a, trial, ElementEvent{event::kPut, trial.handle, e.path, trial.state, 0,
                                               e.data, {}})) {
            if (it->state.empty()) {
                a->presented.erase(it); // never presented: nothing of it stays
            }
            return;
        }
        auto kept = std::find_if(a->presented.begin(), a->presented.end(),
                                 [&](const Presented& p) { return p.path == e.path; });
        *kept = trial;
    }

    void change(loom::Mail& mail, const ElementChange& c) {
        for (WinArea& a : this->state_.areas) {
            if (!mail.authored_from_role(a.owner)) {
                continue;
            }
            for (Presented& p : a.presented) {
                if (p.ref == c.ref) {
                    (void)act(mail, a, p, ElementEvent{event::kChange, p.handle, p.path, p.state,
                                                       c.kind, c.change, {}});
                    return;
                }
            }
        }
        refuse(mail, "", "no element this window presents is the sender's by reference " +
                             std::to_string(c.ref));
    }

    /// The code presenting a type, opened on first use.
    const ElementCodes::Code* code_for(const std::string& type, std::string& why) {
        const auto& codes = this->state_.codes;
        const auto it = std::find_if(codes.begin(), codes.end(),
                                     [&](const ElementCodeOffered& k) { return k.type == type; });
        if (it == codes.end()) {
            why = "no code offered presents '" + type + "'";
            return nullptr;
        }
        return codes_.get(it->library, why);
    }

    /// One event for an element's code: its drawing applied as its own and its state kept, only
    /// when the window accepts the drawing; a refusal is told to the element's drawer. True when
    /// the event was taken.
    bool act(loom::Mail& mail, WinArea& a, Presented& p, const ElementEvent& event) {
        std::string why;
        const ElementCodes::Code* code = code_for(p.type, why);
        if (code == nullptr) {
            refuse_to(mail, a.owner, a.area, "element '" + p.path + "': " + why);
            return false;
        }
        auto out = codes_.act(*code, event, why);
        if (!out) {
            refuse_to(mail, a.owner, a.area, "element '" + p.path + "': " + why);
            return false;
        }
        if (event.kind == event::kPress && !out->took) {
            return false;
        }
        Drawn d = out->drawn;
        d.area = a.area;
        if (d.whole || !d.put.empty() || !d.removed.empty() || !d.said.empty() ||
            !d.unsaid.empty()) {
            if (!draw(a, Drawer{true, p.path}, d, why)) {
                refuse_to(mail, a.owner, a.area, "element '" + p.path + "': " + why);
                return false;
            }
        }
        p.state = out->state;
        return true;
    }

    // ---- painting and showing -------------------------------------------------------------

    /// A drawing judged, applied, and what it changed painted and kept painted.
    bool draw(WinArea& a, const Drawer& who, const Drawn& d, std::string& why) {
        why = judge(a, who, d);
        if (!why.empty()) {
            return false;
        }
        for (const KeptNode& k : a.nodes) {
            if (touches(a, who, d, k.node.path)) {
                medium_.cover(a, k.node.path);
            }
        }
        const Applied applied = apply(a, who, d);
        for (const std::string& gone : applied.gone) {
            medium_.drop(a, gone);
        }
        paint(a, applied.changed);
        if (d.whole || !applied.changed.empty() || !applied.gone.empty() || !d.said.empty() ||
            !d.unsaid.empty()) {
            touch(a);
        }
        if (!who.element) {
            waiting_.erase(a.area);
            a.drawing = d.number;
        }
        if (d.whole && !who.element) {
            std::erase_if(a.presented, [&](const Presented& p) {
                return index_of(a).count(p.path) == 0 && !applied.changed.count(p.path);
            });
        }
        return true;
    }

    static bool touches(const WinArea& a, const Drawer& who, const Drawn& d,
                        const std::string& path) {
        if (d.whole && mine(a, who, path)) {
            return true;
        }
        for (const std::string& r : d.removed) {
            if (under(path, r)) {
                return true;
            }
        }
        for (const Node& n : d.put) {
            if (under(path, n.path)) {
                return true;
            }
        }
        return false;
    }

    /// Some of an area's nodes painted by its medium, each sent with every ancestor present (a
    /// node is drawn in their space); a skin that will not paint is the last refusal.
    void paint(const WinArea& a, const std::set<std::string>& paths) {
        if (paths.empty()) {
            return;
        }
        const auto at = index_of(a);
        std::set<std::string> send;
        for (const std::string& p : paths) {
            if (at.count(p) == 0) {
                continue;
            }
            send.insert(p);
            for (std::string q = parent_of(p); !q.empty(); q = parent_of(q)) {
                if (at.count(q) != 0) {
                    send.insert(q);
                }
            }
        }
        if (send.empty()) {
            return;
        }
        PaintTask task;
        const TextMeasure text = medium_.text();
        task.text_advance = text.advance;
        task.text_line = text.line;
        for (const KeptNode& k : a.nodes) {
            if (send.count(k.node.path) != 0) {
                task.nodes.push_back(
                    PaintNode{k.node.path, key_of(at, k.node.path), k.node.op, k.node.args});
            }
        }
        std::string why;
        medium_.paint(painters_, a, task, paths, why);
        if (!why.empty()) {
            note("area '" + a.area + "': " + why);
        }
    }

    /// Paint all of an area afresh.
    void remake(const WinArea& a) {
        medium_.cover_area(a);
        medium_.drop_area(a.area);
        std::set<std::string> all;
        for (const KeptNode& k : a.nodes) {
            all.insert(k.node.path);
        }
        paint(a, all);
    }

    /// A drawer's word decides when its beat's drawing shows: one saying more follows holds the
    /// showing back; any other handler shows what it changed, says where the window is now, and
    /// answers a capture that showing let through.
    void settle(loom::Mail& mail, bool more) {
        if (!more) {
            medium_.show(this->state_.areas);
            tell_settled(mail);
            report_placement(mail);
            capture_if_due(mail);
        }
    }

    /// WHAT EACH AREA SHOWS, SETTLED: told to the office that placed it and follows it, unasked,
    /// for each area whose version moved since it was last told.
    void tell_settled(loom::Mail& mail) {
        for (const WinArea& a : this->state_.areas) {
            if (!followed(a)) {
                continue;
            }
            const std::int64_t version = version_of(a.area);
            std::int64_t& told = told_[a.area];
            if (told == version) {
                continue;
            }
            told = version;
            (void)mail.as_role(office()).send_to_role(
                a.arranger, AreaSettled{std::string(Medium::kWindow), a.area, version});
        }
    }

    /// Where the window sits, said as its office when it changed and only then: never on a
    /// medium with no desktop place, since `(0,0)` is a real place.
    void report_placement(loom::Mail& mail) {
        const std::optional<WindowPlacementValue> now = medium_.placement();
        if (!now.has_value() || (last_placement_.has_value() && *now == *last_placement_)) {
            return;
        }
        last_placement_ = *now;
        (void)mail.as_role(office()).publish(SurfacePlacement{now->x, now->y, now->maximized});
    }

    static SurfaceCaptured refused_capture(std::string why) {
        SurfaceCaptured c;
        c.ok = false;
        c.refusal = std::move(why);
        return c;
    }

    /// The medium's last picture read back and retained in place of the one before; none to read
    /// is said, and a picture over the bound is refused whole rather than cut.
    SurfaceCaptured take_capture() {
        std::optional<CapturedWindowPicture> picture = medium_.capture();
        if (!picture.has_value()) {
            return refused_capture("this medium has no picture to read back right now");
        }
        if (static_cast<std::int64_t>(picture->bytes.size()) > kMaxCaptureBytes) {
            return refused_capture("the picture is " + std::to_string(picture->bytes.size()) +
                                   " bytes, over the " + std::to_string(kMaxCaptureBytes) +
                                   " this window retains");
        }
        retained_.number = ++captures_;
        retained_.frame = medium_.showings();
        retained_.picture = std::move(*picture);
        SurfaceCaptured c;
        c.ok = true;
        c.capture = retained_.number;
        c.frame = retained_.frame;
        c.width = retained_.picture.width;
        c.height = retained_.picture.height;
        c.cell_px = retained_.picture.cell_px;
        c.format = retained_.picture.format;
        c.bytes = static_cast<std::int64_t>(retained_.picture.bytes.size());
        return c;
    }

    /// The waiting capture, answered on the showing that passed its frame, and only then.
    void capture_if_due(loom::Mail& mail) {
        if (!pending_capture_.valid() || medium_.showings() <= pending_after_) {
            return;
        }
        loom::DeferredAnswer due = std::move(pending_capture_);
        pending_capture_ = loom::DeferredAnswer{};
        (void)loom::answer_deferred(due, mail, take_capture());
    }

    // ---- small things ---------------------------------------------------------------------

    static std::string office() { return std::string(kWindowOfficePrefix) + Medium::kWindow; }

    /// Pointer arithmetic held at int64's ends rather than past them.
    static std::int64_t sum(std::int64_t a, std::int64_t b) {
        constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
        constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
        if (b > 0 && a > kMax - b) {
            return kMax;
        }
        if (b < 0 && a < kMin - b) {
            return kMin;
        }
        return a + b;
    }
    static std::int64_t times_cell(std::int64_t cells) {
        constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
        constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
        if (cells > kMax / kCellPx) {
            return kMax;
        }
        if (cells < kMin / kCellPx) {
            return kMin;
        }
        return cells * kCellPx;
    }

    /// A random token of one width, its top bit set: a loaded weave cannot read its own id, and
    /// a window started afresh must differ from every one before it.
    static std::int64_t token() {
        std::random_device seed;
        const std::uint64_t r = (static_cast<std::uint64_t>(seed()) << 32) ^ seed();
        return static_cast<std::int64_t>((r & 0x1fffffffffffffffULL) | (1ULL << 61));
    }

    /// The number the first press is given: random, so a window reloaded or started again names no
    /// press another of its lives named.
    static std::uint64_t first_press() { return static_cast<std::uint64_t>(token()); }

    WinArea* find(const std::string& area) {
        for (WinArea& a : this->state_.areas) {
            if (a.area == area) {
                return &a;
            }
        }
        return nullptr;
    }

    void close_unused(const std::string& library) {
        if (library.empty()) {
            return;
        }
        for (const WinArea& a : this->state_.areas) {
            if (a.library == library) {
                return;
            }
        }
        painters_.close(library);
        medium_.forget(library);
    }

    /// This window, as its own office, to whoever follows it: the directory, and an arranger that
    /// lays out in its room.
    void offer(loom::Mail& mail) {
        const PixelRoom size = medium_.offered();
        const TextMeasure text = medium_.text();
        (void)mail.as_role(office()).publish(
            WindowOffered{Medium::kWindow, size.width, size.height, this->state_.epoch,
                          Medium::kCellDevicePx, text.advance, text.line});
    }

    /// An area's room, to its owner: this window, its incarnation, the area's size, the references
    /// of the elements presented there, whether it holds none of the owner's drawing, and how this
    /// window sets text.
    void room(loom::Mail& mail, const WinArea& a) {
        if (a.owner.empty()) {
            return;
        }
        const TextMeasure text = medium_.text();
        AreaRoom r{a.area, Medium::kWindow, this->state_.epoch, a.w, a.h, {},
                   a.nodes.empty() && a.records.empty() && a.presented.empty(),
                   Medium::kCellDevicePx, text.advance, text.line};
        for (const Presented& p : a.presented) {
            r.elements.push_back(p.ref);
        }
        (void)mail.as_role(office()).send_to_role(a.owner, r);
    }

    void refuse(loom::Mail& mail, const std::string& area, const std::string& reason) {
        note(reason);
        (void)mail.as_role(office()).send(mail.sender(), DrawnRefused{area, reason});
    }

    void refuse_to(loom::Mail& mail, const std::string& owner, const std::string& area,
                   const std::string& reason) {
        note(reason);
        (void)mail.as_role(office()).send_to_role(owner, DrawnRefused{area, reason});
    }

    void note(const std::string& why) {
        ++this->state_.refusals;
        last_refusal_ = why;
    }

    /// The one picture retained, numbered: an incarnation's, as the medium that showed it is.
    struct Retained {
        std::int64_t number = 0;
        std::int64_t frame = 0;
        CapturedWindowPicture picture;
    };

    /// A press held for an area until its button's release: the area, where the press was made in
    /// the window's pixels, and the press's number.
    struct Hold {
        std::string area;
        std::int64_t x = 0, y = 0;
        std::uint64_t press = 0;
    };
    /// A hold's key: the hand (0 the weaver's own, else its actor), and the button.
    using HoldKey = std::pair<std::int64_t, std::int64_t>;

    /// What an area shows may have moved: it, and every area its rectangle meets, take a new
    /// version, so a reading asked since an older one is said again.
    void touch(const WinArea& changed) {
        for (const WinArea& a : this->state_.areas) {
            if (a.area == changed.area ||
                (a.x < changed.x + changed.w && a.x + a.w > changed.x &&
                 a.y < changed.y + changed.h && a.y + a.h > changed.y)) {
                versions_[a.area] = ++next_version_;
            }
        }
    }

    /// Every area's showing may have moved: the window was started, or measured again.
    void touch_all() {
        for (const WinArea& a : this->state_.areas) {
            versions_[a.area] = ++next_version_;
        }
    }

    std::int64_t version_of(const std::string& area) {
        const auto it = versions_.find(area);
        if (it != versions_.end()) {
            return it->second;
        }
        return versions_[area] = ++next_version_;
    }

    Medium medium_;
    typename WindowBase<Medium>::Handle pump_;
    std::map<HoldKey, Hold> holds_;
    /// The presses whose area the window withdrew or handed over while held, until their release:
    /// it reaches nobody. A hold an area in front ended is released where it lands.
    std::set<HoldKey> ended_;
    std::uint64_t next_press_; ///< from a random start, so a reload names no press again
    /// Each hand's area hearing the pointer that its last unheld move landed in.
    std::map<std::int64_t, std::string> hovered_;
    /// Each area's version, from a random start, so a reload or a fresh window names no version
    /// another of its lives named.
    std::map<std::string, std::int64_t> versions_;
    std::int64_t next_version_ = token() >> 8;
    /// Each area's version last told its arranger as settled (`AreaSettled`).
    std::map<std::string, std::int64_t> told_;
    /// The areas whose owner was told a room asking a whole drawing, and has drawn none since.
    std::set<std::string> waiting_;
    Painters painters_;
    ElementCodes codes_;
    std::string last_refusal_;
    std::optional<WindowPlacementValue> last_placement_; ///< the last place said, this incarnation
    Retained retained_;
    std::int64_t captures_ = 0;
    loom::DeferredAnswer pending_capture_;
    std::int64_t pending_after_ = -1;
};

} // namespace zengine::graphics::window

#endif // ZENGINE_GRAPHICS_WINDOW_WINDOW_HPP
