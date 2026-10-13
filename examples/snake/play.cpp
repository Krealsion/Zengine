// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// zengine-snake, the playable host: it owns the boot list and nothing else -- it draws nothing of
// the game, knows no snake rules, reads no keys, keeps no clock and pumps nobody. The world keeps
// its own rules, keys and beat; the terminal window reads the terminal and shows what owners draw;
// the directory introduces the two; and the operator weave arranges the window, chooses its skins,
// speaks the status line and sends every lifecycle command (examples/snake/README.md).

// The moments are keys, which the window gives the world and the operator alike: 1 chooses the
// other skin (painter code the window opens mid-game; nothing is drawn again on the bus), 2 loads
// the score weave into the running game, 3 grows the world (a graceful swap: the v1 world writes
// its letter and the v2 heir migrates it), 4 says the SDL window waits on a later stage; r reloads
// the world in place, n starts a new game through the poke-reset door, l lists, q quits.

#include "play_state.hpp"
#include "vocabulary.hpp"

#include "graphics/canvas.hpp"
#include "graphics/vocabulary.hpp"
#include "input/vocabulary.hpp"
#include "timer/vocabulary.hpp"

#include <zen/kernel/admission.hpp>
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
#include <map>
#include <memory>
#include <string>
#include <utility>

namespace {

using namespace zengine::snake;
namespace gfx = zengine::graphics;
namespace input = zengine::input;
namespace scan = zengine::input::scan;
namespace timer = zengine::timer;

// ---- the operator ------------------------------------------------------------

/// The operator's office, the owner of the status line's area.
constexpr const char* kOperatorRole = "snake.operator";
constexpr const char* kStatusArea = "snake.operator";
constexpr const char* kWindowRole = "zengine.window.terminal";

/// The skins the operator can choose: painter code the window opens.
struct SkinChoice {
    const char* stem;
    const char* label;
};
constexpr SkinChoice kSkins[] = {
    {"zengine-paint-classic", "classic"},
    {"zengine-paint-block", "block"},
};

/// What the host remembers about a command in flight, so an answer can be reported in the words
/// of the question, and so a success can flip the operator's own trackers.
struct Pending {
    std::string label;
    int action = 0; // 0 none, 2 world migrated, 3 score loaded
};
constexpr int kWorldGrown = 2;
constexpr int kScoreLoaded = 3;

struct OperatorContext {
    std::map<std::uint64_t, Pending> pending;
    std::uint64_t next_corr = 1;
    bool quit = false;         ///< the q key; the loop reads it after the pump returns
    int skin = 0;              ///< which skin paints the window's areas
    bool world_is_v2 = false;  ///< which world generation holds the role
    bool score_loaded = false; ///< whether the score weave has an area
    std::string status;        ///< the status line, drawn again in a window started afresh
    loom::WeaveId manager{};
    std::string dir; ///< where the host resolves loadable weaves and code (beside itself)

    /// The host's stop lever: with time inside the bus, drain_until_idle() returns only when told
    /// to stop, so the quit key must stop the bus, not just set a flag.
    std::function<void()> request_stop;

    std::string so(const char* stem) const;
};

/// The host's hand on the bus and the window's arranger: it places the areas, offers the skins
/// and the board's code, chooses the skins, draws the status line, holds the reach (the manager,
/// target-scoped, and the world's poke-reset door by role), and hears every answer.
class OperatorWeave
    : public loom::WeaveBase<
          OperatorWeave, OperatorState,
          loom::Accept<loom::Result, loom::Ack, loom::Refused, input::KeyPressed,
                       gfx::GraphicsReady, gfx::AreaRoom, gfx::DrawnRefused>,
          loom::Emit<loom::LoadWeave, loom::SwapWeave, loom::ReloadWeave, loom::ListLoaded,
                     loom::PokeResetState, gfx::AreaPlaced, gfx::SkinOffered, gfx::SkinChosen,
                     gfx::ElementCodeOffered, gfx::Drawn>> {
public:
    explicit OperatorWeave(OperatorContext& ctx) : ctx_(&ctx) {}

    // The three standard answers. Refusal-ness is carried as a FLAG from the handler that knows
    // it by TYPE: the shape is the fact.
    void on(const loom::Result& r, loom::Mail& mail) {
        answered(mail, "-> " + r.value, /*refused=*/false);
    }
    void on(const loom::Ack&, loom::Mail& mail) { answered(mail, "-> done", /*refused=*/false); }
    void on(const loom::Refused& r, loom::Mail& mail) {
        answered(mail, "-> refused: " + r.reason, /*refused=*/true);
    }

    /// A directory started: the window arranged again, from what the operator knows.
    void on(const gfx::GraphicsReady&, loom::Mail& mail) {
        if (mail.authored_from_role(gfx::kGraphicsRole)) {
            arrange(mail);
        }
    }

    /// The status line's window: drawn whole when the window is new or started again.
    void on(const gfx::AreaRoom& r, loom::Mail& mail) {
        if (mail.authored_from_role(gfx::kGraphicsRole) && r.area == kStatusArea &&
            canvas_.set_window(r.window, r.epoch)) {
            speak(mail);
        }
    }

    void on(const gfx::DrawnRefused&, loom::Mail&) {}

    /// The command keys. Steering is the world's, and unknown keys are nobody's error.
    void on(const input::KeyPressed& k, loom::Mail& mail) {
        if (!mail.authored_from_role(kWindowRole)) {
            return;
        }
        switch (k.scancode) {
        case scan::k1:
            ctx_->skin = 1 - ctx_->skin;
            choose_skins(mail);
            status(mail, std::string("skin -> ") + kSkins[ctx_->skin].label);
            break;
        case scan::k4:
            status(mail, "the SDL window waits on a later stage; this one is the terminal");
            break;
        case scan::k2:
            command(mail, "load score weave (late)", kScoreLoaded,
                    loom::LoadWeave{"snake-score", ctx_->so("snake-score"), kScoreRole});
            break;
        case scan::k3:
            command(mail, "grow the world (graceful v1->v2)", kWorldGrown,
                    loom::SwapWeave{kWorldRole, "snake-world-v2", ctx_->so("snake-world-v2"),
                                    /*graceful=*/true});
            break;
        case scan::kR: {
            const char* stem = ctx_->world_is_v2 ? "snake-world-v2" : "snake-world-v1";
            command(mail, "reload world in place (state rides the gate)", 0,
                    loom::ReloadWeave{stem, ctx_->so(stem)});
            break;
        }
        case scan::kL:
            command(mail, "loaded", 0, loom::ListLoaded{});
            break;
        case scan::kN: {
            const std::uint64_t corr = ctx_->next_corr++;
            ctx_->pending[corr] = Pending{"new game (poke reset)", 0};
            mail.send_to_role(kWorldRole, loom::PokeResetState{}, corr);
            status(mail, "new game (poke reset) ...");
            break;
        }
        case scan::kQ:
            quit();
            break;
        case scan::kC:
            // Ctrl is measured, not inferred from a name: the modifier rides the transition it
            // was held for (input/docs/input.md).
            if ((k.modifiers & input::mod::kCtrl) != 0) {
                quit();
            }
            break;
        default: break;
        }
    }

private:
    /// The window as the operator arranges it: the skins and the board's code offered, the status
    /// line across the top, the world beneath it, the score beside it once loaded; every area
    /// hears the keys, and each wears the chosen skin.
    void arrange(loom::Mail& mail) {
        const auto to_directory = [&](const auto& m) {
            (void)mail.as_role(kOperatorRole).send_to_role(gfx::kGraphicsRole, m);
        };
        for (const SkinChoice& s : kSkins) {
            to_directory(gfx::SkinOffered{s.label, ctx_->so(s.stem)});
        }
        to_directory(gfx::ElementCodeOffered{"SnakeBoardData", ctx_->so("snake-board")});
        const std::int64_t c = gfx::kCellPx;
        to_directory(gfx::AreaPlaced{kStatusArea, gfx::kTerminalWindow, kOperatorRole, 0,
                                     0, 120 * c, c, 1, true});
        to_directory(gfx::AreaPlaced{kWorldArea, gfx::kTerminalWindow, kWorldRole, 0, c,
                                     50 * c, 28 * c, 0, true});
        if (ctx_->score_loaded) {
            place_score(mail);
        }
        choose_skins(mail);
    }

    void place_score(loom::Mail& mail) {
        const std::int64_t c = gfx::kCellPx;
        (void)mail.as_role(kOperatorRole)
            .send_to_role(gfx::kGraphicsRole,
                          gfx::AreaPlaced{kScoreRole, gfx::kTerminalWindow, kScoreRole,
                                          52 * c, c, 50 * c, c, 0, false});
    }

    void choose_skins(loom::Mail& mail) {
        for (const char* area : {kStatusArea, kWorldArea, kScoreRole}) {
            (void)mail.as_role(kOperatorRole)
                .send_to_role(gfx::kGraphicsRole, gfx::SkinChosen{area, kSkins[ctx_->skin].label});
        }
    }

    template <class Cmd>
    void command(loom::Mail& mail, std::string label, int action, const Cmd& cmd) {
        const std::uint64_t corr = ctx_->next_corr++;
        ctx_->pending[corr] = Pending{label, action};
        mail.send(ctx_->manager, cmd, corr);
        status(mail, label + " ...");
    }

    /// The status line, plain ASCII: how it looks is the skin's business.
    void status(loom::Mail& mail, const std::string& text) {
        ctx_->status = "[zen] " + text +
                       "   (wasd steer | 1 skin | 2 score | 3 grow | 4 sdl | r reload | n new | "
                       "l list | q quit)";
        speak(mail);
    }

    void speak(loom::Mail& mail) {
        gfx::Sketch f;
        f.text("status", 0, 0, ctx_->status, gfx::by_role(gfx::role::kFill));
        canvas_.draw(f);
        canvas_.show(mail);
    }

    void answered(loom::Mail& mail, const std::string& outcome, bool refused) {
        ++state_.answers;
        const auto it = ctx_->pending.find(mail.correlation());
        if (it == ctx_->pending.end()) {
            status(mail, "unsolicited answer from weave " + std::to_string(mail.sender().value) +
                             " " + outcome);
            return;
        }
        // A successful moment flips the trackers here, in the answer's own handler: the answer,
        // not the wish, is what flips them.
        if (!refused) {
            if (it->second.action == kWorldGrown) {
                ctx_->world_is_v2 = true;
            } else if (it->second.action == kScoreLoaded) {
                ctx_->score_loaded = true;
                place_score(mail);
                choose_skins(mail);
            }
        }
        status(mail, it->second.label + " " + outcome);
        ctx_->pending.erase(it);
    }

    /// The one exit: mark the wish for the loop and stop the bus so the loop gets to read it.
    void quit() {
        ctx_->quit = true;
        if (ctx_->request_stop) {
            ctx_->request_stop();
        }
    }

    OperatorContext* ctx_;
    gfx::Canvas canvas_{kOperatorRole, kStatusArea};
};

std::string exe_dir() {
#if defined(_WIN32)
    // In the UTF-8 code page a path's bytes can outnumber its UTF-16 units, so the buffer grows
    // until the whole path fits: up to three bytes for each of a path's 32,767 units at most.
    std::string path(MAX_PATH, '\0');
    for (;;) {
        const DWORD n =
            ::GetModuleFileNameA(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (n == 0) {
            return ".";
        }
        if (n < path.size()) {
            path.resize(n);
            break;
        }
        if (path.size() > std::size_t{3} * 32767) {
            return ".";
        }
        path.resize(path.size() * 2);
    }
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

/// The platform's loadable-library suffix. One spelling, used for every stem.
constexpr const char* kWeaveSuffix =
#if defined(_WIN32)
    ".dll";
#else
    ".so";
#endif

std::string OperatorContext::so(const char* stem) const { return dir + "/" + stem + kWeaveSuffix; }

} // namespace

int main() {
    // The honest line, in plain scrollback before the window takes the terminal: this host
    // isolates nothing anywhere, and on Windows it is the explicit development/demo backend.
    std::printf("zengine-snake - containment: %s\n", loom::Kernel::containment_note());
    std::fflush(stdout);

    loom::Switchboard bus;
    // The demo loads exactly the artifacts its own build just produced, so it says so rather than
    // relying on a default the Loom no longer has.
    loom::Kernel kernel(
        bus, loom::trust_every_artifact("the snake demo loads only its own build output"));
    const loom::WeaveId control = loom::mount_control(kernel, bus);
    const loom::WeaveId manager = loom::mount_manager(control, bus);

    OperatorContext ctx;
    ctx.manager = manager;
    ctx.dir = exe_dir();
    ctx.request_stop = [&bus] { bus.stop(); };
    loom::Grant reach; // the manager (the dangerous grant, target-scoped)...
    reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
    reach.allow(loom::SwapWeave::zen_name, loom::SwapWeave::zen_version, manager);
    reach.allow(loom::ReloadWeave::zen_name, loom::ReloadWeave::zen_version, manager);
    reach.allow(loom::ListLoaded::zen_name, loom::ListLoaded::zen_version, manager);
    // ...the world's reset door, by role (the n key)...
    reach.allow_to_role(loom::PokeResetState::zen_name, loom::PokeResetState::zen_version,
                        kWorldRole);
    // ...the directory, as the window's arranger, and the window, for the status line.
    reach.allow_to_role(gfx::AreaPlaced::zen_name, gfx::AreaPlaced::zen_version,
                        gfx::kGraphicsRole);
    reach.allow_to_role(gfx::SkinOffered::zen_name, gfx::SkinOffered::zen_version,
                        gfx::kGraphicsRole);
    reach.allow_to_role(gfx::SkinChosen::zen_name, gfx::SkinChosen::zen_version,
                        gfx::kGraphicsRole);
    reach.allow_to_role(gfx::ElementCodeOffered::zen_name, gfx::ElementCodeOffered::zen_version,
                        gfx::kGraphicsRole);
    reach.allow_to_role(gfx::Drawn::zen_name, gfx::Drawn::zen_version, kWindowRole);
    auto weave = std::make_unique<OperatorWeave>(ctx);
    OperatorWeave* raw = weave.get();
    const loom::WeaveId op = bus.register_weave(std::move(weave), std::move(reach), kOperatorRole);
    raw->zen_set_self(op);

    // Startup commands are sent AS the operator, with the same bookkeeping its key commands use.
    const auto boot = [&](std::string label, const auto& cmd) {
        const std::uint64_t corr = ctx.next_corr++;
        ctx.pending[corr] = Pending{std::move(label), 0};
        bus.send_as(op, manager, loom::Message(loom::to_value(cmd), op, op, corr));
    };

    // Birth of the game: the directory first (its hello is what has the operator arrange the
    // window), then the window that takes the terminal, the world, and the timer service, whose
    // loading is what starts time: every weave arranges its own beat on its own activation.
    boot("load directory", loom::LoadWeave{"zengine-graphics", ctx.so("zengine-graphics"),
                                           gfx::kGraphicsRole});
    boot("load terminal window", loom::LoadWeave{"zengine-window-terminal",
                                                 ctx.so("zengine-window-terminal"), kWindowRole});
    boot("load world v1", loom::LoadWeave{"snake-world-v1", ctx.so("snake-world-v1"), kWorldRole});
    boot("load timer service",
         loom::LoadWeave{"zengine-timer", ctx.so("zengine-timer"), timer::kTimerRole});

    // The whole game runs inside `drain_until_idle()`, which returns only on quit while the beat
    // chain lives; a drain that comes back idle means nothing here will ever speak again.
    while (!ctx.quit) {
        bus.drain_until_idle();
        if (!ctx.quit && bus.pending() == 0) {
            std::printf("zengine-snake - the bus went quiet without a quit "
                        "(no timer service deployed?): time is gone, exiting.\n");
            break;
        }
    }
    return 0;
}
