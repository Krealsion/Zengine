// zengine-snake — the playable host.
//
// The host is deliberately THIN: it owns the boot list — nothing else. It
// draws nothing, owns no screen, knows no snake rules, performs no lifecycle
// itself, reads no keys, and — since the Timer package — keeps no clock,
// never sleeps, and pumps nobody. Everything crosses the bus:
//
//   keys      → the zengine-input weave (the Input package's producer) owns
//               the platform's input side and publishes KeyPressed/…; the
//               snake-controls adapter turns steering keys into SnakeTurn;
//               the host's own operator weave accepts KeyPressed for the
//               command keys.
//   time      → the Timer package: the zengine-timer weave (holding
//               zengine.timer) owns the monotonic clock and the one nap in
//               the system, and delivers TimerFired beats. The snake-clock
//               adapter turns its 120ms ask into SnakeTick for whoever holds
//               snake.world; the input weave and the active skin keep
//               themselves serviced on role-addressed beats of their own.
//               The host's whole contribution is the WIND: one Drive message
//               at boot; the service re-winds itself every beat after.
//   drawing   → the Surface package: the world publishes SnakeVisual, the
//               operator and the score weave publish SurfaceText, and the
//               active SKIN — a replaceable weave holding the zengine.skin
//               role — claims the terminal or a window and paints. Since this
//               package's Surface migration, the host does not even own the
//               screen: loading the skin claims it, unloading releases it.
//   operating → zen.LoadWeave / zen.SwapWeave / zen.ReloadWeave / zen.ListLoaded,
//               sent by the operator weave whose grant reaches exactly the
//               Weave Manager, with every answer arriving back at it and
//               spoken as status intent.
//
// The three phase moments are three keys (drawing's moment now swaps SKINS —
// the same replacement story, one package lower):
//   1  swap the skin (hard swap — the painting code is unloaded, dlclosed,
//      and different painting code takes the surface, mid-game)
//   2  load the score weave into the already-running game
//   3  grow the world: a GRACEFUL swap — the v1 world writes its letter, the
//      v2 heir claims it by role and migrates the state
// plus 4 (swap to the SDL skin, where deployed — a real window consuming the
// exact same intent), r (reload the world in place), n (new game via the
// substrate's poke-reset door), l (list), q (quit).

#include "vocabulary.hpp"

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
#include <map>
#include <string>
#include <utility>

namespace {

using namespace zengine::snake;
namespace input = zengine::input;
namespace scan = zengine::input::scan;
namespace surface = zengine::surface;
namespace timer = zengine::timer;

// ---- the operator ------------------------------------------------------------

/// The skins the operator can put on the surface. Index doubles as the
/// tracker value, so a successful swap knows what it swapped to.
struct SkinChoice {
    const char* stem;
    const char* label;
};
constexpr SkinChoice kSkins[] = {
    {"zengine-skin-tui-classic", "classic"},
    {"zengine-skin-tui-block", "block"},
    {"zengine-skin-sdl", "sdl window"},
};
constexpr int kSkinClassic = 0;
constexpr int kSkinBlock = 1;
constexpr int kSkinSdl = 2;

/// What the host remembers about a command in flight, so an answer can be
/// reported in the words of the question, and so a success can flip the
/// operator's own trackers (which skin paints, which world generation runs).
struct Pending {
    std::string label;
    int action = 0; // 0 none, 2 world-migrated, 10+i skin i swapped in
};

struct OperatorContext {
    std::map<std::uint64_t, Pending> pending;
    std::uint64_t next_corr = 1;
    bool quit = false;        ///< the q key; the loop reads it after the pump returns
    int skin = kSkinClassic;  ///< which skin holds the surface right now
    bool world_is_v2 = false; ///< which world generation holds the role
    std::string last_status;  ///< re-published whenever a skin says hello
    loom::WeaveId manager{};
    std::string dir; ///< where the host resolves loadable weaves (beside itself)

    /// The host's stop lever, handed to the operator: with time inside the
    /// bus, pump() runs the whole game and returns only when told to stop —
    /// so the quit key must stop the bus, not just set a flag for a loop
    /// body that would otherwise never come around.
    std::function<void()> request_stop;

    std::string so(const char* stem) const;
};

/// The host's hand on the bus: it holds the reach (the manager, target-scoped
/// — the dangerous grant — plus the world's poke-reset door by role), issues
/// every lifecycle command, and hears every answer. It LISTENS the same way
/// snake does (command keys arrive as published KeyPressed), and it SPEAKS
/// its status the same way snake draws: as published intent — SurfaceText on
/// the "status" slot — painted by whichever skin holds the surface. A fresh
/// skin's SurfaceReady hello gets the current status re-published, so the
/// line survives the painter being replaced. Consumer obligation: answers are
/// matched against our own outstanding correlations; anything else is
/// reported as noise, acted on never.
struct OperatorState {
    std::int64_t answers = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(OperatorState, 1, ZEN_FIELD(answers));
};

class OperatorWeave
    : public loom::WeaveBase<OperatorWeave, OperatorState,
                             loom::Accept<loom::Result, loom::Ack, loom::Refused,
                                          input::KeyPressed, surface::SurfaceReady>,
                             loom::Emit<loom::LoadWeave, loom::SwapWeave, loom::ReloadWeave,
                                        loom::ListLoaded, loom::PokeResetState,
                                        surface::SurfaceText>> {
public:
    explicit OperatorWeave(OperatorContext& ctx) : ctx_(&ctx) {}

    void on(const loom::Result& r, loom::Mail& mail) { answered(mail, "-> " + r.value); }
    void on(const loom::Ack&, loom::Mail& mail) { answered(mail, "-> done"); }
    void on(const loom::Refused& r, loom::Mail& mail) {
        answered(mail, "-> refused: " + r.reason);
    }

    /// A skin claimed the surface and said hello: give it the current status
    /// line so the operator's row survives the painter being replaced.
    void on(const surface::SurfaceReady&, loom::Mail& mail) {
        if (!ctx_->last_status.empty()) {
            mail.publish(surface::SurfaceText{surface::kSlotStatus, ctx_->last_status});
        }
    }

    /// The command keys. Steering is not here — the snake-controls adapter
    /// owns it — and unknown keys are nobody's error.
    void on(const input::KeyPressed& k, loom::Mail& mail) {
        switch (k.scancode) {
        case scan::k1: {
            const int next = ctx_->skin == kSkinClassic ? kSkinBlock : kSkinClassic;
            swap_skin(mail, next);
            break;
        }
        case scan::k4:
            swap_skin(mail, kSkinSdl);
            break;
        case scan::k2:
            command(mail, "load score weave (late)", 0,
                    loom::LoadWeave{"snake-score", ctx_->so("snake-score"), ""});
            break;
        case scan::k3:
            command(mail, "grow the world (graceful v1->v2)", 2,
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
            // V1 carries no modifiers, so Ctrl+C survives only as the backends'
            // dressed convenience name. Quitting on it is a courtesy the host
            // chooses to trust; the scancode stays the authority for the key.
            if (k.name == "Ctrl+C") {
                quit();
            }
            break;
        default: break;
        }
    }

private:
    void swap_skin(loom::Mail& mail, int next) {
        const SkinChoice& s = kSkins[next];
        command(mail, std::string("swap skin -> ") + s.label, 10 + next,
                loom::SwapWeave{surface::kSkinRole, s.stem, ctx_->so(s.stem),
                                /*graceful=*/false});
    }

    template <class Cmd>
    void command(loom::Mail& mail, std::string label, int action, const Cmd& cmd) {
        const std::uint64_t corr = ctx_->next_corr++;
        ctx_->pending[corr] = Pending{label, action};
        mail.send(ctx_->manager, cmd, corr);
        status(mail, label + " ...");
    }

    /// The status line, spoken as intent. ASCII only (the house charset rule)
    /// and PLAIN — how it looks is the skin's business.
    void status(loom::Mail& mail, const std::string& text) {
        ctx_->last_status = "[zen] " + text +
                            "   (wasd steer | 1 skin | 2 score | 3 grow | 4 sdl | "
                            "r reload | n new | l list | q quit)";
        mail.publish(surface::SurfaceText{surface::kSlotStatus, ctx_->last_status});
    }

    void answered(loom::Mail& mail, const std::string& outcome) {
        ++state_.answers;
        const auto it = ctx_->pending.find(mail.correlation());
        if (it == ctx_->pending.end()) {
            status(mail, "unsolicited answer from weave " + std::to_string(mail.sender().value) +
                             " " + outcome);
            return;
        }
        // A successful moment flips the trackers here, in the answer's own
        // handler — the answer, not the wish, is what flips them.
        if (outcome.find("refused") == std::string::npos) {
            const int action = it->second.action;
            if (action >= 10) {
                ctx_->skin = action - 10;
            } else if (action == 2) {
                ctx_->world_is_v2 = true;
            }
        }
        status(mail, it->second.label + " " + outcome);
        ctx_->pending.erase(it);
    }

    /// The one exit: mark the wish for the loop and stop the bus so the loop
    /// gets to read it.
    void quit() {
        ctx_->quit = true;
        if (ctx_->request_stop) {
            ctx_->request_stop();
        }
    }

    OperatorContext* ctx_;
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

/// The platform's loadable-weave suffix. One spelling, used for every stem.
constexpr const char* kWeaveSuffix =
#if defined(_WIN32)
    ".dll";
#else
    ".so";
#endif

std::string OperatorContext::so(const char* stem) const { return dir + "/" + stem + kWeaveSuffix; }

} // namespace

int main() {
    // The honest line, in plain scrollback (the host owns no screen to put it
    // anywhere else — and it should outlive the session anyway): this host
    // isolates nothing anywhere, and on Windows it is the explicit
    // development/demo backend.
    std::printf("zengine-snake - containment: %s\n", loom::Kernel::containment_note());
    std::fflush(stdout);

    loom::Switchboard bus;
    loom::Kernel kernel(bus);
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
    // ...plus the world's reset door, by role (the n key)...
    reach.allow_to_role(loom::PokeResetState::zen_name, loom::PokeResetState::zen_version,
                        kWorldRole);
    // ...plus the right to SPEAK: the status line is a published intent now,
    // and a publish is authorized per-recipient against the sender's grant —
    // an operator that may command the steward but not talk to a skin would
    // have a working game and a permanently blank status row (the live pty
    // run found exactly that; the suite pins this recipe now). Any-target on
    // purpose: the operator addresses no skin, it speaks to whoever listens.
    reach.allow_to_any(surface::SurfaceText::zen_name, surface::SurfaceText::zen_version);
    const loom::WeaveId op = loom::mount_granted<OperatorWeave>(bus, std::move(reach), ctx);

    // Startup commands are sent AS the operator (send_as stamps it and
    // authorizes against ITS grant — the host holds root but spends a real
    // capability), with the same in-flight bookkeeping its key commands use.
    const auto boot = [&](std::string label, const auto& cmd) {
        const std::uint64_t corr = ctx.next_corr++;
        ctx.pending[corr] = Pending{std::move(label), 0};
        bus.send_as(op, manager, loom::Message(loom::to_value(cmd), op, op, corr));
    };

    // Birth of the game: the same gesture as everything else — ask the steward.
    // The SKIN is first (loading it claims the screen; everything after paints
    // through it). Everything time-hungry is loaded BEFORE the wind below, so
    // the TimerService's hello reaches a fully assembled cast and every
    // package asks for its beat on the first breath.
    boot("claim surface (classic skin)",
         loom::LoadWeave{kSkins[kSkinClassic].stem, ctx.so(kSkins[kSkinClassic].stem),
                         surface::kSkinRole});
    boot("load world v1", loom::LoadWeave{"snake-world-v1", ctx.so("snake-world-v1"), kWorldRole});
    boot("load snake controls", loom::LoadWeave{"snake-controls", ctx.so("snake-controls"), ""});
    boot("load input weave",
         loom::LoadWeave{"zengine-input", ctx.so("zengine-input"), input::kInputRole});
    boot("load timer service",
         loom::LoadWeave{"zengine-timer", ctx.so("zengine-timer"), timer::kTimerRole});
    boot("load snake clock", loom::LoadWeave{"snake-clock", ctx.so("snake-clock"), ""});

    // The boot breath: deliver the boot list to completion BEFORE winding.
    // Loading is a conversation, not a call — the Manager answers LoadWeave
    // by asking the kernel door, one delivery later — so a wind queued behind
    // the boot sends would resolve the timer role before any load ran and
    // refuse into the vacancy (found live: the pilot's very first run).
    bus.pump();

    // Wind the clock: the one breath the host gives time — the first Drive.
    // The TimerService re-winds itself every beat after this; its hello wakes
    // the clock adapter (120ms -> SnakeTick), the input weave, and the skin
    // into asking for their own beats. The host owes nothing per lap.
    bus.send_to_role(timer::kTimerRole, loom::Message(loom::to_value(timer::Drive{})));

    // The whole game runs inside pump(): the beat chain keeps the queue alive,
    // the TimerService's nap paces it, and the operator's quit stops the bus.
    // A pump that instead returns QUIESCENT — an empty queue — means nothing
    // in this process will ever speak again (there is no clock outside it):
    // say so honestly and leave, rather than spin on a dead bus. That is also
    // where a deployment with no timer service lands, right after boot.
    while (!ctx.quit) {
        bus.pump();
        if (!ctx.quit && bus.pending() == 0) {
            std::printf("zengine-snake - the bus went quiet without a quit "
                        "(no timer service deployed?): time is gone, exiting.\n");
            break;
        }
    }
    return 0;
}
