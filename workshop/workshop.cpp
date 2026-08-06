// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// zengine-workshop — the first Workshop surface. One rectangle, selected,
// inspected, edited, refused.
//
// It is an ordinary Zengine application and holds no privilege snake does not:
// keys arrive from the Input package's weave, time from the Timer package's,
// pixels-or-characters from whichever Skin holds `zengine.skin`, and the host
// owns the boot list and nothing else. Workshop paints by PUBLISHING intent — a
// SurfaceCanvas — exactly as the world publishes a SnakeVisual, and it never
// touches the terminal.
//
// WHAT IS AND IS NOT WORKSHOP'S HERE, because the whole phase turns on it:
//
//   the authored object   a real zengine::ui::Element in the weave's own state:
//                         gated, schema-carrying, poke-inspectable. There is no
//                         shadow model -- the element the maker selects IS the
//                         element the canvas is painted from and the inspector
//                         reads through. W-1 moved the TYPE out to the UI
//                         package; the object is no less Workshop's state for
//                         being spelled in a shared vocabulary.
//   the geometry          NOT Workshop's, since W-1. `ui::resolve` turns the
//                         authored extents into a scene and `ui::hit` says what
//                         is under a cell; this file computes neither, and the
//                         canvas, the inspector and the pointer all read one
//                         scene.
//   the session           selection, workspace extent, drafts. Plain members,
//                         never state (the Skin's `announced_` stance).
//   the screen            screen.hpp, pure, pinned by the suite.
//
// THE INPUT REALITY, named where a reader will hit it. The locked input
// vocabulary carries a SCANCODE -- a physical key -- with no modifier and no
// character concept (input/vocabulary.hpp). So typing has to be rebuilt here
// from scancodes, and it can only ever produce lowercase ASCII: the terminal
// backend maps 'A' and 'a' to the same scancode, so the two are not
// distinguishable facts on this wire. A maker cannot type a capital letter into
// a Workshop name, and cannot type `%` at all. That is a real hole in the
// vocabulary, not a shortcut taken here.

#include "screen.hpp"

#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"
#include "timer/vocabulary.hpp"

#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>

namespace {

using namespace zengine::workshop;
namespace input = zengine::input;
namespace scan = zengine::input::scan;
namespace surface = zengine::surface;
namespace timer = zengine::timer;
namespace ui = zengine::ui;

/// One scancode back to the character it stood for, or 0 for "not a character".
///
/// LOWERCASE ONLY, and not by choice: see the header note. Reconstructing this
/// table is itself the finding -- an application that wants text has to know the
/// keyboard, which is exactly the knowledge the Input package exists to own.
char character_of(std::int64_t scancode) {
    if (scancode >= scan::kA && scancode <= scan::kZ) {
        return static_cast<char>('a' + (scancode - scan::kA));
    }
    if (scancode >= scan::k1 && scancode <= scan::k9) {
        return static_cast<char>('1' + (scancode - scan::k1));
    }
    switch (scancode) {
    case scan::k0: return '0';
    case scan::kSpace: return ' ';
    case scan::kMinus: return '-';
    case scan::kPeriod: return '.';
    case scan::kComma: return ',';
    case scan::kSlash: return '/';
    case scan::kSemicolon: return ';';
    case scan::kLeftBracket: return '[';
    case scan::kRightBracket: return ']';
    default: return 0;
    }
}

/// What the host needs from the weave and cannot get by message: the stop lever.
struct HostContext {
    bool quit = false;
    std::function<void()> request_stop;
    std::string dir;
    std::string so(const char* stem) const;
};

/// The boot weave: it asks the Weave Manager to load the packages Workshop runs
/// on, and — the part that matters — it HEARS THE ANSWERS.
///
/// It exists because the first live run of this host had no such weave, and the
/// cost was immediate and instructive. "Failures are values": the Manager answers
/// its asker with zen.Result or zen.Refused. A root send carries no asker, so
/// every one of those answers was addressed to nobody, and a Workshop whose Skin
/// had refused to load looked exactly like a Workshop whose Skin had loaded — a
/// blank terminal and a host that exited saying the bus went quiet. The
/// diagnosis was not in the program at all.
///
/// So the asking is a weave with a grant, the way snake's operator is. It holds
/// the reach to the Manager — which is kernel reach, transitively, and is the
/// dangerous grant — and Workshop's own weave deliberately does not. Two
/// responsibilities, two grants: this one operates, that one authors.
struct BootState {
    std::int64_t answered = 0;
    std::int64_t refused = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(BootState, 1, ZEN_FIELD(answered), ZEN_FIELD(refused));
};

class BootWeave : public loom::WeaveBase<BootWeave, BootState,
                                        loom::Accept<loom::Result, loom::Ack, loom::Refused>,
                                        loom::Emit<loom::LoadWeave, surface::SurfaceText>> {
public:
    void on(const loom::Result&, loom::Mail&) { ++state_.answered; }
    void on(const loom::Ack&, loom::Mail&) { ++state_.answered; }

    /// A refusal is the only thing here worth saying out loud, and it is said
    /// twice on purpose: as status intent for whoever is painting, and on plain
    /// stdout — because the most likely thing to have been refused is the Skin,
    /// and a message about a missing painter delivered to the painter is not a
    /// message.
    void on(const loom::Refused& r, loom::Mail& mail) {
        ++state_.answered;
        ++state_.refused;
        std::printf("zengine-workshop - boot refused: %s\n", r.reason.c_str());
        std::fflush(stdout);
        mail.publish(surface::SurfaceText{surface::kSlotStatus,
                                          "[workshop] boot refused: " + r.reason});
    }
};

/// The Workshop weave: the authored document, the session, and the keys.
class WorkshopWeave
    : public loom::WeaveBase<WorkshopWeave, WorkshopDoc,
                             loom::Accept<input::KeyPressed, input::MouseButton, input::MouseMoved,
                                          surface::SurfaceReady>,
                             loom::Emit<surface::SurfaceCanvas, surface::SurfaceText>> {
public:
    explicit WorkshopWeave(HostContext& host) : host_(&host) {
        // The document a maker opens onto. Deliberately boring, and deliberately
        // TWO rectangles sharing nothing but a name pattern -- `panel` and
        // `panel` would be the same object in the old builder, and here they are
        // #1 and #2. The wide one is authored as a SHARE so the very first screen
        // already shows an authored intent and its resolved value side by side.
        doc::add(state_, "panel", 3, 2, ui::Extent{ui::kExtentPercent, 60},
                 ui::Extent{ui::kExtentCells, 6});
        doc::add(state_, "panel", 6, 10, ui::Extent{ui::kExtentCells, 14},
                 ui::Extent{ui::kExtentCells, 4});
        session_.selected = state_.elements.empty() ? 0 : state_.elements.front().id;
        rebuild_rows();
    }

    /// A Skin claimed the surface and said hello: give it the whole screen. The
    /// operator weave's precedent, and the only thing Workshop needs in order to
    /// paint for the first time -- so load order decides nothing here either.
    void on(const surface::SurfaceReady&, loom::Mail& mail) { repaint(mail); }

    void on(const input::KeyPressed& k, loom::Mail& mail) {
        // Ctrl+C is the one key that means the same thing in every mode. It
        // arrives as a scancode plus a courtesy name because V1 has no modifier
        // vocabulary; the host branches on it exactly as snake's does, and with
        // the same expiry.
        if (k.scancode == scan::kC && k.name == "Ctrl+C") {
            quit();
            return;
        }
        if (editing_row() != nullptr) {
            typing(k, mail);
        } else {
            command(k, mail);
        }
        repaint(mail);
    }

    /// The pointer, where one exists. MouseButton carries no position, so the
    /// place a click landed has to come from the last MouseMoved -- two messages
    /// to say one thing, and the reason this weave keeps `pointer_`.
    ///
    /// Nothing on the canonical Linux lane sends either message: the POSIX
    /// terminal backend produces keystrokes only (input/input.cpp), so pointer
    /// selection is live only where a pointer exists at all. The keyboard path is
    /// the portable one, and the part that answers WHICH object -- now
    /// `ui::hit` over the same scene the canvas was painted from -- is pinned by
    /// the suite rather than left to a medium that may not report.
    void on(const input::MouseMoved& m, loom::Mail&) {
        pointer_x_ = static_cast<std::int64_t>(m.x);
        pointer_y_ = static_cast<std::int64_t>(m.y);
    }

    void on(const input::MouseButton& b, loom::Mail& mail) {
        if (b.button != 1 || !b.pressed) {
            return;
        }
        // Canvas cells, from the terminal cell the pointer is over: the canvas
        // starts at terminal row 3 (the Skin's layout convention), and the
        // workspace starts one row into the canvas.
        const std::int64_t cx = pointer_x_ - kWorkspaceX;
        const std::int64_t cy = pointer_y_ - 2 - kWorkspaceY;
        const ui::Scene scene = workspace_scene(state_, session_);
        const ui::Placed* hit = ui::hit(scene, cx, cy);
        if (hit != nullptr) {
            select(hit->id);
            say("selected #" + std::to_string(hit->id) + " by pointer", false);
        }
        repaint(mail);
    }

private:
    Row* editing_row() {
        for (Row& r : session_.rows) {
            if (r.editing()) {
                return &r;
            }
        }
        return nullptr;
    }

    /// Editing mode: every character key goes to the DRAFT, including the ones
    /// that are commands otherwise. `q` types a q here, and that is the whole
    /// reason Ctrl+C is handled above this branch.
    void typing(const input::KeyPressed& k, loom::Mail&) {
        Row* row = editing_row();
        switch (k.scancode) {
        case scan::kReturn: {
            const Commit result = row->commit();
            if (result == Commit::Accepted) {
                say("committed " + row->label() + " = " + row->value(), false);
            } else {
                // Two different failures, and the row already words each one for
                // its own kind: an unparseable draft reads "not <what would have
                // worked>", a refused value carries the setter's own reason. The
                // first live run appended the expected form AGAIN here, which said
                // it twice and then ran off the end of the line -- the notice is
                // one line, so a line's worth is all it may spend.
                (void)result;
                say(row->label() + ": " + row->refusal(), true);
            }
            break;
        }
        case scan::kEscape:
            row->cancel();
            say("edit cancelled -- nothing was written", false);
            break;
        case scan::kBackspace: row->backspace(); break;
        default: {
            const char c = character_of(k.scancode);
            if (c != 0) {
                row->type(c);
            }
            break;
        }
        }
    }

    /// Command mode.
    void command(const input::KeyPressed& k, loom::Mail&) {
        switch (k.scancode) {
        case scan::kTab: select_next(); break;
        case scan::kUp:
            if (session_.cursor > 0) {
                --session_.cursor;
            }
            break;
        case scan::kDown:
            if (session_.cursor + 1 < session_.rows.size()) {
                ++session_.cursor;
            }
            break;
        case scan::kReturn: begin_edit(); break;
        case scan::kLeftBracket: resize_workspace(-4); break;
        case scan::kRightBracket: resize_workspace(+4); break;
        case scan::kQ: quit(); break;
        default: break;
        }
    }

    void begin_edit() {
        if (session_.cursor >= session_.rows.size()) {
            return;
        }
        Row& row = session_.rows[session_.cursor];
        if (!row.editable()) {
            say(row.label() + " is not authored -- it is what the workspace makes of the "
                              "authored value",
                true);
            return;
        }
        row.begin();
        say("editing " + row.label() + " -- enter commits, esc cancels", false);
    }

    /// Resize the workspace: NO authored value changes, and a share visibly
    /// resolves to something else. One keystroke, and the difference between an
    /// authored fact and a resolved one stops being an argument.
    void resize_workspace(std::int64_t delta) {
        std::int64_t want = session_.workspace_w + delta;
        if (want < kWorkspaceMinW) {
            want = kWorkspaceMinW;
        }
        if (want > kWorkspaceW) {
            want = kWorkspaceW;
        }
        session_.workspace_w = want;
        rebuild_rows(); // the resolved row closes over the extent it resolves against
        say("workspace is now " + std::to_string(want) +
                " cells wide -- authored values unchanged",
            false);
    }

    void select_next() {
        if (state_.elements.empty()) {
            return;
        }
        std::size_t at = 0;
        for (std::size_t i = 0; i < state_.elements.size(); ++i) {
            if (state_.elements[i].id == session_.selected) {
                at = i;
                break;
            }
        }
        select(state_.elements[(at + 1) % state_.elements.size()].id);
    }

    void select(std::int64_t id) {
        if (id == session_.selected) {
            return;
        }
        session_.selected = id;
        rebuild_rows();
    }

    /// The rows are rebuilt, never patched. Each one reads through its property
    /// every time it is displayed, so there is no cached value to refresh and no
    /// "refresh the inspector" call anywhere in this file -- the second half of
    /// the old builder's per-row plumbing, also gone.
    void rebuild_rows() { refocus(state_, session_); }

    void say(std::string text, bool bad) {
        session_.notice = std::move(text);
        session_.notice_is_bad = bad;
    }

    void repaint(loom::Mail& mail) {
        mail.publish(paint(state_, session_));
        mail.publish(surface::SurfaceText{
            surface::kSlotStatus,
            "[workshop] " + std::to_string(state_.elements.size()) + " objects | selected #" +
                std::to_string(session_.selected)});
    }

    void quit() {
        host_->quit = true;
        if (host_->request_stop) {
            host_->request_stop();
        }
    }

    HostContext* host_;
    Session session_;
    std::int64_t pointer_x_ = 0; ///< the last position a MouseMoved reported...
    std::int64_t pointer_y_ = 0; ///< ...because MouseButton does not carry one
};

std::string exe_dir() {
#if defined(_WIN32)
    char buf[MAX_PATH];
    const DWORD n = ::GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return ".";
    }
    std::string path(buf, n);
    const std::size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? "." : path.substr(0, slash);
#else
    char buf[4096];
    const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) {
        return ".";
    }
    buf[n] = '\0';
    std::string path(buf);
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? "." : path.substr(0, slash);
#endif
}

constexpr const char* kWeaveSuffix =
#if defined(_WIN32)
    ".dll";
#else
    ".so";
#endif

std::string HostContext::so(const char* stem) const { return dir + "/" + stem + kWeaveSuffix; }

} // namespace

int main() {
    // The honest line, in plain scrollback, exactly as snake's host prints it:
    // this host isolates nothing.
    std::printf("zengine-workshop - containment: %s\n", loom::Kernel::containment_note());
    std::fflush(stdout);

    loom::Switchboard bus;
    loom::Kernel kernel(bus);
    const loom::WeaveId control = loom::mount_control(kernel, bus);
    const loom::WeaveId manager = loom::mount_manager(control, bus);

    HostContext host;
    host.dir = exe_dir();
    host.request_stop = [&bus] { bus.stop(); };

    // Workshop's own reach: the right to SPEAK its screen, and nothing else. It
    // commands no lifecycle, loads no weave and reaches no manager -- the boot
    // list below is the host's. A maker tool with a live document does not need
    // the dangerous grant to do W-0's job, and giving it one "because Workshop
    // will eventually need it" is exactly the specialness this phase is
    // supposed to be counting.
    loom::Grant speak;
    speak.allow_to_any(surface::SurfaceCanvas::zen_name, surface::SurfaceCanvas::zen_version);
    speak.allow_to_any(surface::SurfaceText::zen_name, surface::SurfaceText::zen_version);
    loom::mount_granted<WorkshopWeave>(bus, std::move(speak), host);

    // The boot weave's reach: the Manager, target-scoped, plus the right to speak
    // a status line. This is the dangerous grant in this process, and it is held
    // by the weave whose whole job is one round of loading.
    loom::Grant operate;
    operate.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
    operate.allow_to_any(surface::SurfaceText::zen_name, surface::SurfaceText::zen_version);
    const loom::WeaveId booter = loom::mount_granted<BootWeave>(bus, std::move(operate));

    // Boot, as ordinary LoadWeave commands sent AS the boot weave -- so the
    // Manager's answers come back to something that can hear them, and a refused
    // load is a fact this program can report instead of a silence it exits on.
    // The Skin is first: loading it claims the terminal, and its hello is what
    // makes Workshop paint.
    const auto boot = [&](const char* stem, const char* role) {
        bus.send_as(booter, manager,
                    loom::Message(loom::to_value(loom::LoadWeave{stem, host.so(stem), role}),
                                  booter, booter));
    };
    boot("zengine-skin-tui-classic", surface::kSkinRole);
    boot("zengine-input", input::kInputRole);
    boot("zengine-timer", timer::kTimerRole);

    // Everything runs inside pump(): the input weave's own beat keeps the queue
    // alive, the Timer service's nap paces it, and `q` stops the bus. A pump
    // that returns with an empty queue means nothing in this process will ever
    // speak again -- say so and leave rather than spin, snake's stance and for
    // the same reason.
    while (!host.quit) {
        bus.pump();
        if (!host.quit && bus.pending() == 0) {
            std::printf("zengine-workshop - the bus went quiet without a quit "
                        "(no timer service deployed?): exiting.\n");
            break;
        }
    }
    return 0;
}
