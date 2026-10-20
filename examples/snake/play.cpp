// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// zengine-snake, the playable host: it owns the boot list and nothing else -- it draws nothing of
// the game, knows no snake rules, reads no keys, keeps no clock and pumps nobody. The world keeps
// its own rules, keys and beat; the terminal window reads the terminal and shows what owners draw;
// the directory introduces the two; and the operator weave arranges the window, chooses its skins,
// speaks the status line and sends every lifecycle command (examples/snake/README.md).

// The moments are keys, which the window gives the world and the operator alike, and the
// operator's buttons beneath its status line, each making its key's gesture: 1 chooses the other
// skin (painter code the window opens mid-game; nothing is drawn again on the bus), 2 loads the
// score weave into the running game, 3 grows the world (a graceful swap: the v1 world writes its
// letter and the v2 heir migrates it), 4 says no window weave draws an OS window; r reloads the
// world in place, n starts a new game through the poke-reset door, l lists, q quits.

#include "play_state.hpp"
#include "vocabulary.hpp"

#include "graphics/drawing.hpp"
#include "graphics/elements/button.hpp"
#include "graphics/vocabulary.hpp"
#include "input/vocabulary.hpp"
#include "timer/vocabulary.hpp"

#include <zen/kernel/admission.hpp>
#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>
#include <zen/weave/ask_book.hpp>

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
#include <optional>
#include <string>
#include <type_traits>
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

/// The operator's buttons, in the row beneath its status line: the action each asks of the
/// operator, and its words, which name the key that makes the same gesture.
constexpr const char* kSkinAction = "skin";
constexpr const char* kScoreAction = "score";
constexpr const char* kGrowAction = "grow";
constexpr const char* kReloadAction = "reload";
constexpr const char* kNewAction = "new";
constexpr const char* kListAction = "list";
constexpr const char* kQuitAction = "quit";
struct Control {
    const char* action;
    const char* label;
};
constexpr Control kControls[] = {
    {kSkinAction, "1 skin"},     {kScoreAction, "2 score"}, {kGrowAction, "3 grow"},
    {kReloadAction, "r reload"}, {kNewAction, "n new"},     {kListAction, "l list"},
    {kQuitAction, "q quit"},
};

/// What the host remembers about a command in flight, so an answer can be reported in the words
/// of the question, and so a success can flip the operator's own trackers.
struct Pending {
    std::string label;
    int action = 0; // 0 none, 2 world migrated, 3 score loaded
};
constexpr int kWorldGrown = 2;
constexpr int kScoreLoaded = 3;

/// How many commands the operator keeps in flight at once; one more is refused out loud.
constexpr std::size_t kAsksInFlight = 32;

struct OperatorContext {
    loom::AskBook asks{kAsksInFlight};        ///< the commands in flight, each settled once
    std::map<std::uint64_t, Pending> pending; ///< what each open ask was, by its local id
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

    /// An ask of the manager, opened and remembered: the correlation the command carries, or
    /// refused while the book is full.
    template <class Cmd>
    loom::AskOpened ask_manager(std::string label, int action) {
        return remember(asks.open(manager, Cmd::zen_name, Cmd::zen_version), std::move(label),
                        action);
    }

    /// An ask of the world's reset door, by role: whoever holds the world when it arrives answers.
    loom::AskOpened ask_world_reset(std::string label) {
        return remember(asks.open_to_role(kWorldRole, loom::PokeResetState::zen_name,
                                          loom::PokeResetState::zen_version),
                        std::move(label), 0);
    }

    loom::AskOpened remember(loom::AskOpened opened, std::string label, int action) {
        if (opened) {
            pending[opened.id] = Pending{std::move(label), action};
        }
        return opened;
    }
};

/// The host's hand on the bus and the window's arranger: it places the areas, offers the skins
/// and the board's code, chooses the skins, draws the status line and its buttons, holds the
/// reach (the manager, target-scoped, and the world's poke-reset door by role), and hears every
/// answer.
class OperatorWeave
    : public gfx::With<gfx::ActionFacet>::Weave<
          OperatorWeave, OperatorState,
          loom::Accept<loom::Result, loom::Ack, loom::Refused, input::KeyPressed,
                       gfx::GraphicsReady>,
          loom::Emit<loom::LoadWeave, loom::SwapWeave, loom::ReloadWeave, loom::ListLoaded,
                     loom::PokeResetState, gfx::AreaPlaced, gfx::SkinOffered, gfx::SkinChosen,
                     gfx::ElementCodeOffered>> {
public:
    using Weave::on;

    explicit OperatorWeave(OperatorContext& ctx)
        : Weave(kOperatorRole, kStatusArea), ctx_(&ctx) {}

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

    /// The command keys. Steering is the world's, and unknown keys are nobody's error.
    void on(const input::KeyPressed& k, loom::Mail& mail) {
        if (!mail.authored_from_role(kWindowRole)) {
            return;
        }
        switch (k.scancode) {
        case scan::k1: flip_skin(mail); break;
        case scan::k4:
            status("no window weave draws an OS window: this one is the terminal");
            break;
        case scan::k2: load_score(mail); break;
        case scan::k3: grow_world(mail); break;
        case scan::kR: reload_world(mail); break;
        case scan::kL: list_loaded(mail); break;
        case scan::kN: reset_world(mail); break;
        case scan::kQ: quit(); break;
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

    /// A button pressed in the window: the gesture of the key it names.
    void on_action(const std::string& action, loom::Mail& mail) {
        if (action == kSkinAction) {
            flip_skin(mail);
        } else if (action == kScoreAction) {
            load_score(mail);
        } else if (action == kGrowAction) {
            grow_world(mail);
        } else if (action == kReloadAction) {
            reload_world(mail);
        } else if (action == kNewAction) {
            reset_world(mail);
        } else if (action == kListAction) {
            list_loaded(mail);
        } else if (action == kQuitAction) {
            quit();
        }
    }

    /// The status line, plain ASCII: how it looks is the skin's business. Beneath it, a button
    /// for each gesture, each set by the measure of the window that shows the area.
    void picture(gfx::Sketch& f) const {
        const gfx::TextMeasure m = canvas().text();
        f.text("status", 0, 0, ctx_->status, gfx::by_role(gfx::role::kFill));
        std::int64_t x = 0;
        for (const Control& k : kControls) {
            const auto characters = static_cast<std::int64_t>(std::string(k.label).size()) + 2;
            gfx::Button button(k.action, k.label);
            button.set_measure(m);
            button.set_bounds(x, m.line, characters * m.advance, m.line);
            button.sketch(f, k.action);
            x += (characters + 1) * m.advance;
        }
    }

private:
    // ---- the gestures, made alike by a key and by its button ----

    void flip_skin(loom::Mail& mail) {
        ctx_->skin = 1 - ctx_->skin;
        choose_skins(mail);
        status(std::string("skin -> ") + kSkins[ctx_->skin].label);
    }

    void load_score(loom::Mail& mail) {
        command(mail, "load score weave (late)", kScoreLoaded,
                loom::LoadWeave{"snake-score", ctx_->so("snake-score"), kScoreRole});
    }

    void grow_world(loom::Mail& mail) {
        command(mail, "grow the world (graceful v1->v2)", kWorldGrown,
                loom::SwapWeave{kWorldRole, "snake-world-v2", ctx_->so("snake-world-v2"),
                                /*graceful=*/true});
    }

    void reload_world(loom::Mail& mail) {
        const char* stem = ctx_->world_is_v2 ? "snake-world-v2" : "snake-world-v1";
        command(mail, "reload world in place (state rides the gate)", 0,
                loom::ReloadWeave{stem, ctx_->so(stem)});
    }

    void reset_world(loom::Mail& mail) {
        const std::string label = "new game (poke reset)";
        if (const loom::AskOpened opened = ctx_->ask_world_reset(label)) {
            mail.send_to_role(kWorldRole, loom::PokeResetState{}, opened.correlation);
            status(label + " ...");
        } else {
            not_sent(label);
        }
    }

    void list_loaded(loom::Mail& mail) { command(mail, "loaded", 0, loom::ListLoaded{}); }

    /// The one exit: mark the wish for the loop and stop the bus so the loop gets to read it.
    void quit() {
        ctx_->quit = true;
        if (ctx_->request_stop) {
            ctx_->request_stop();
        }
    }

    /// The window as the operator arranges it: the skins and the board's code offered, the status
    /// line and its buttons across the top, the world beneath them, the score beside it once
    /// loaded; every area hears the keys, and each wears the chosen skin.
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
                                     0, 200 * c, 2 * c, 1, true});
        to_directory(gfx::AreaPlaced{kWorldArea, gfx::kTerminalWindow, kWorldRole, 0, 2 * c,
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
                                          52 * c, 2 * c, 50 * c, c, 0, false});
    }

    void choose_skins(loom::Mail& mail) {
        for (const char* area : {kStatusArea, kWorldArea, kScoreRole}) {
            (void)mail.as_role(kOperatorRole)
                .send_to_role(gfx::kGraphicsRole, gfx::SkinChosen{area, kSkins[ctx_->skin].label});
        }
    }

    template <class Cmd>
    void command(loom::Mail& mail, const std::string& label, int action, const Cmd& cmd) {
        if (const loom::AskOpened opened = ctx_->ask_manager<Cmd>(label, action)) {
            mail.send(ctx_->manager, cmd, opened.correlation);
            status(label + " ...");
        } else {
            not_sent(label);
        }
    }

    /// A command the book has no room for: never sent, and said so.
    void not_sent(const std::string& label) {
        status(label + " not sent: " + std::to_string(ctx_->asks.outstanding()) +
               " commands still wait for an answer");
    }

    /// The status line's words, drawn at the end of the delivery that said them.
    void status(const std::string& text) {
        ctx_->status = "[zen] " + text + "   (wasd | 4 sdl)";
    }

    void answered(loom::Mail& mail, const std::string& outcome, bool refused) {
        ++state_.answers;
        // An ask of the manager settles by its correlation and the manager's stamp; an ask by
        // role, whose respondent was unknown when asking, also needs Loom's attested answer. The
        // manager relays its control door's answers as ordinary speech, so only the role's
        // answer is asked for that attestation.
        const loom::PendingAsk* open = ctx_->asks.match(mail.correlation(), mail.sender());
        const std::optional<loom::PendingAsk> settled =
            open != nullptr && (!open->to_role() || mail.answers_ask())
                ? ctx_->asks.settle(mail.correlation(), mail.sender())
                : std::nullopt;
        const auto it = settled ? ctx_->pending.find(settled->id) : ctx_->pending.end();
        if (it == ctx_->pending.end()) {
            status("unsolicited answer from weave " + std::to_string(mail.sender().value) + " " +
                   outcome);
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
        status(it->second.label + " " + outcome);
        ctx_->pending.erase(it);
    }

    OperatorContext* ctx_;
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
    // ...the directory, as the window's arranger and as a drawer asking its room, and the window,
    // for the status line.
    reach.allow_to_role(gfx::AreaPlaced::zen_name, gfx::AreaPlaced::zen_version,
                        gfx::kGraphicsRole);
    reach.allow_to_role(gfx::SkinOffered::zen_name, gfx::SkinOffered::zen_version,
                        gfx::kGraphicsRole);
    reach.allow_to_role(gfx::SkinChosen::zen_name, gfx::SkinChosen::zen_version,
                        gfx::kGraphicsRole);
    reach.allow_to_role(gfx::ElementCodeOffered::zen_name, gfx::ElementCodeOffered::zen_version,
                        gfx::kGraphicsRole);
    reach.allow_to_role(gfx::RoomAsked::zen_name, gfx::RoomAsked::zen_version, gfx::kGraphicsRole);
    reach.allow_to_role(gfx::Drawn::zen_name, gfx::Drawn::zen_version, kWindowRole);
    auto weave = std::make_unique<OperatorWeave>(ctx);
    OperatorWeave* raw = weave.get();
    const loom::WeaveId op = bus.register_weave(std::move(weave), std::move(reach), kOperatorRole);
    raw->zen_set_self(op);

    // Startup commands are sent AS the operator, asked in the same book its key commands use.
    const auto boot = [&](std::string label, const auto& cmd) {
        using Cmd = std::decay_t<decltype(cmd)>;
        if (const loom::AskOpened opened = ctx.ask_manager<Cmd>(std::move(label), 0)) {
            bus.send_as(op, manager,
                        loom::Message(loom::to_value(cmd), op, op, opened.correlation));
        }
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
