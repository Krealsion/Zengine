// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// zengine-snake, the playable host: it owns the boot list and its loop's rest, and nothing else --
// it draws nothing of the game, knows no snake rules, reads no keys and keeps no time: between
// turns it rests as long as the Timer's beat allows (timer/rest.hpp). The world keeps its own
// rules, keys and beat; the TUI window reads the terminal and shows what owners draw;
// the directory introduces the two; and the console weave arranges the window, chooses its
// palette, speaks the status line and sends every lifecycle command (examples/snake/README.md).

// The moments are keys, which the window gives the world and the console alike, and the
// console's buttons beneath its status line, each making its key's gesture: 1 the other palette,
// 2 the score weave, 3 the world grown (a graceful swap: v1's letter, v2's migration), 4 no GUI
// window, 5 the pilot flown or landed; r reloads the world, n starts afresh, l lists, q quits.
// `--pilot` flies the pilot from birth and `--pause-at <step>` pauses the first game after that
// step: the same game every run, held still on the same step.

#include "play_state.hpp"
#include "vocabulary.hpp"

#include "graphics/drawing.hpp"
#include "graphics/elements/button.hpp"
#include "graphics/themes.hpp" // generated: the shipped themes, as palettes
#include "graphics/vocabulary.hpp"
#include "input/vocabulary.hpp"
#include "timer/os_clock.hpp"
#include "timer/rest.hpp"

#include <zen/host/reactor.hpp>
#include "timer/vocabulary.hpp"
#include "workshop/bus_tally.hpp"

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
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using namespace zengine::snake;
namespace gfx = zengine::graphics;
namespace input = zengine::input;
namespace scan = zengine::input::scan;
namespace timer = zengine::timer;

// ---- the console ------------------------------------------------------------

/// The console's role, the owner of the status line's area.
constexpr const char* kConsoleRole = "snake.console";
constexpr const char* kStatusArea = "snake.console";
constexpr const char* kWindowRole = "zengine.window.tui";

/// The palettes the console offers and chooses between: two of the shipped themes, made unlike
/// each other so a choice mid-game is unmistakable.
inline std::vector<gfx::PaletteOffered> palettes() {
    return {gfx::themes::night(), gfx::themes::block()};
}

/// The console's buttons, in the row beneath its status line: the action each asks of the
/// console, and its words, which name the key that makes the same gesture. `skin` is the game's
/// word for the look a palette gives it.
constexpr const char* kPaletteAction = "skin";
constexpr const char* kScoreAction = "score";
constexpr const char* kGrowAction = "grow";
constexpr const char* kReloadAction = "reload";
constexpr const char* kNewAction = "new";
constexpr const char* kListAction = "list";
constexpr const char* kQuitAction = "quit";
constexpr const char* kPilotAction = "pilot";
struct Control {
    const char* action;
    const char* label;
};
constexpr Control kControls[] = {
    {kPaletteAction, "1 skin"}, {kScoreAction, "2 score"},  {kGrowAction, "3 grow"},
    {kPilotAction, "5 pilot"},  {kReloadAction, "r reload"}, {kNewAction, "n new"},
    {kListAction, "l list"},    {kQuitAction, "q quit"},
};

/// What the host remembers about a command in flight, so an answer can be reported in the words
/// of the question, and so a success can flip the console's own trackers.
struct Pending {
    std::string label;
    int action = 0; // 0 none, else one of the moments below
};
constexpr int kWorldGrown = 2;
constexpr int kScoreLoaded = 3;
constexpr int kPilotFlown = 4;
constexpr int kPilotLanded = 5;
constexpr int kWorldReset = 6;
constexpr int kWorldLoaded = 7;

/// How many commands the console keeps in flight at once; one more is refused out loud.
constexpr std::size_t kAsksInFlight = 32;

struct ConsoleContext {
    loom::AskBook asks{kAsksInFlight};        ///< the commands in flight, each settled once
    std::map<std::uint64_t, Pending> pending; ///< what each open ask was, by its local id
    bool quit = false;         ///< the q key; the loop reads it after the pump returns
    int palette = 0;           ///< which of the two palettes the window's areas wear
    bool world_is_v2 = false;  ///< which world generation holds the role
    bool score_loaded = false; ///< whether the score weave has an area
    bool pilot_flying = false; ///< whether the pilot holds its role, and with it the wheel
    std::int64_t pause_at = 0; ///< the step the first game pauses itself after (`--pause-at`)
    std::string status;        ///< the status line, drawn again in a window started afresh
    loom::WeaveId manager{};
    loom::WeaveId control{}; ///< the kernel's door, which lands the pilot by its role
    std::string dir; ///< where the host resolves loadable weaves and code (beside itself)

    /// The host's stop lever: the quit key ends the turn it is read in, so the loop reads the wish
    /// before it rests.
    std::function<void()> request_stop;

    std::string so(const char* stem) const;

    /// An ask of the manager, opened and remembered: the correlation the command carries, or
    /// refused while the book is full.
    template <class Cmd>
    loom::AskOpened ask_manager(std::string label, int action) {
        return remember(asks.open(manager, Cmd::zen_name, Cmd::zen_version), std::move(label),
                        action);
    }

    /// The same, of the kernel's door.
    template <class Cmd>
    loom::AskOpened ask_control(std::string label, int action) {
        return remember(asks.open(control, Cmd::zen_name, Cmd::zen_version), std::move(label),
                        action);
    }

    /// An ask of one of the world's poke doors, by role: whoever holds the world when it arrives
    /// answers.
    template <class Poke>
    loom::AskOpened ask_world(std::string label, int action) {
        return remember(asks.open_to_role(kWorldRole, Poke::zen_name, Poke::zen_version),
                        std::move(label), action);
    }

    loom::AskOpened remember(loom::AskOpened opened, std::string label, int action) {
        if (opened) {
            pending[opened.id] = Pending{std::move(label), action};
        }
        return opened;
    }
};

/// The host's hand on the bus and the window's arranger: it places the areas, offers the palettes
/// and the board's code, chooses the palette, draws the status line and its buttons, holds the
/// reach (the manager and the kernel's door, target-scoped, and the world's poke doors by role),
/// and hears every answer.
class ConsoleWeave
    : public gfx::With<gfx::ActionFacet>::Weave<
          ConsoleWeave, ConsoleState,
          loom::Accept<loom::Result, loom::Ack, loom::Refused, gfx::AreaKeyPressed,
                       gfx::GraphicsReady>,
          loom::Emit<loom::LoadWeave, loom::SwapWeave, loom::ReloadWeave, loom::ListLoaded,
                     loom::UnloadRole, loom::PokeResetState, loom::PokeWrite, gfx::AreaPlaced,
                     gfx::PaletteOffered, gfx::PaletteChosen, gfx::ElementCodeOffered>> {
public:
    using Weave::on;

    explicit ConsoleWeave(ConsoleContext& ctx)
        : Weave(kConsoleRole, kStatusArea), ctx_(&ctx) {}

    // The three standard answers. Refusal-ness is carried as a FLAG from the handler that knows
    // it by TYPE: the shape is the fact.
    void on(const loom::Result& r, loom::Mail& mail) {
        answered(mail, "-> " + r.value, /*refused=*/false);
    }
    void on(const loom::Ack&, loom::Mail& mail) { answered(mail, "-> done", /*refused=*/false); }
    void on(const loom::Refused& r, loom::Mail& mail) {
        answered(mail, "-> refused: " + r.reason, /*refused=*/true);
    }

    /// A directory started: the window arranged again, from what the console knows.
    void on(const gfx::GraphicsReady&, loom::Mail& mail) {
        if (mail.authored_from_role(gfx::kGraphicsRole)) {
            arrange(mail);
        }
    }

    /// The command keys. Steering is the world's, and unknown keys are nobody's error.
    void on(const gfx::AreaKeyPressed& k, loom::Mail& mail) {
        if (!mail.authored_from_role(kWindowRole)) {
            return;
        }
        switch (k.scancode) {
        case scan::k1: flip_palette(mail); break;
        case scan::k4:
            status("no window weave draws a GUI window: this one is the terminal");
            break;
        case scan::k2: load_score(mail); break;
        case scan::k3: grow_world(mail); break;
        case scan::k5: fly_or_land(mail); break;
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
        if (action == kPaletteAction) {
            flip_palette(mail);
        } else if (action == kScoreAction) {
            load_score(mail);
        } else if (action == kGrowAction) {
            grow_world(mail);
        } else if (action == kPilotAction) {
            fly_or_land(mail);
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

    /// The status line, plain ASCII: how it looks is the palette's business. Beneath it, a button
    /// for each gesture, each set by the measure of the window that shows the area.
    void picture(gfx::Sketch& f) const {
        const gfx::TextMeasure m = canvas().text();
        f.text("status", 0, 0, ctx_->status, gfx::colors::text);
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

    void flip_palette(loom::Mail& mail) {
        ctx_->palette = 1 - ctx_->palette;
        choose_palette(mail);
        status("skin -> " + palettes()[static_cast<std::size_t>(ctx_->palette)].palette);
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

    /// The pilot flown, or landed. Flown, it takes the wheel itself; landed by its role at the
    /// kernel's door, it hears nothing of it, so the keys go back to the player once the door
    /// says it has gone.
    void fly_or_land(loom::Mail& mail) {
        if (!ctx_->pilot_flying) {
            command(mail, "fly the pilot", kPilotFlown,
                    loom::LoadWeave{"snake-pilot", ctx_->so("snake-pilot"), kPilotRole});
            return;
        }
        const std::string label = "land the pilot";
        if (const loom::AskOpened opened =
                ctx_->ask_control<loom::UnloadRole>(label, kPilotLanded)) {
            mail.send(ctx_->control, loom::UnloadRole{kPilotRole}, opened.correlation);
            status(label + " ...");
        } else {
            not_sent(label);
        }
    }

    void reset_world(loom::Mail& mail) {
        const std::string label = "new game (poke reset)";
        if (const loom::AskOpened opened =
                ctx_->ask_world<loom::PokeResetState>(label, kWorldReset)) {
            mail.send_to_role(kWorldRole, loom::PokeResetState{}, opened.correlation);
            status(label + " ...");
        } else {
            not_sent(label);
        }
    }

    /// One of the world's fields written, by role: the keys handed over, or a pause asked for.
    void write_world(loom::Mail& mail, const std::string& label, const char* field,
                     const std::string& value) {
        if (const loom::AskOpened opened = ctx_->ask_world<loom::PokeWrite>(label, 0)) {
            mail.send_to_role(kWorldRole, loom::PokeWrite{field, value}, opened.correlation);
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

    /// The window as the console arranges it: the palettes and the board's code offered, the
    /// status line and its buttons across the top, the world beneath them, the score beside it
    /// once loaded; every area hears the keys, and each wears the chosen palette.
    void arrange(loom::Mail& mail) {
        const auto to_directory = [&](const auto& m) {
            (void)mail.as_role(kConsoleRole).send_to_role(gfx::kGraphicsRole, m);
        };
        for (const gfx::PaletteOffered& p : palettes()) {
            to_directory(p);
        }
        to_directory(gfx::ElementCodeOffered{"SnakeBoardData", ctx_->so("snake-board")});
        choose_palette(mail); // before the places, so each area is introduced once, wearing it
        const std::int64_t c = gfx::kCellPx;
        to_directory(gfx::AreaPlaced{kStatusArea, gfx::kTuiWindow, kConsoleRole, 0,
                                     0, 200 * c, 2 * c, 1, true});
        to_directory(gfx::AreaPlaced{kWorldArea, gfx::kTuiWindow, kWorldRole, 0, 2 * c,
                                     50 * c, 28 * c, 0, true});
        if (ctx_->score_loaded) {
            place_score(mail);
        }
    }

    void place_score(loom::Mail& mail) {
        const std::int64_t c = gfx::kCellPx;
        (void)mail.as_role(kConsoleRole)
            .send_to_role(gfx::kGraphicsRole,
                          gfx::AreaPlaced{kScoreRole, gfx::kTuiWindow, kScoreRole,
                                          52 * c, 2 * c, 50 * c, c, 0, false});
    }

    void choose_palette(loom::Mail& mail) {
        const std::string chosen = palettes()[static_cast<std::size_t>(ctx_->palette)].palette;
        for (const char* area : {kStatusArea, kWorldArea, kScoreRole}) {
            (void)mail.as_role(kConsoleRole)
                .send_to_role(gfx::kGraphicsRole, gfx::PaletteChosen{area, chosen});
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

    /// The status line's words, drawn at the end of the delivery that said them, and who steers.
    void status(const std::string& text) {
        ctx_->status = "[zen] " + text + "   (" +
                       (ctx_->pilot_flying ? "the pilot steers" : "wasd") + " | p pause | 4 sdl)";
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
        const Pending done = it->second;
        ctx_->pending.erase(it);
        if (!refused) {
            if (done.action == kWorldGrown) {
                ctx_->world_is_v2 = true;
            } else if (done.action == kScoreLoaded) {
                ctx_->score_loaded = true;
                place_score(mail);
                choose_palette(mail);
            } else if (done.action == kPilotFlown) {
                ctx_->pilot_flying = true;
            } else if (done.action == kPilotLanded) {
                ctx_->pilot_flying = false;
            }
        }
        status(done.label + " " + outcome);
        if (refused && done.action == kPilotLanded) {
            // The door refuses an unload by role when no loaded library holds the role: the pilot
            // has gone already, and the keys are still the player's to have back.
            ctx_->pilot_flying = false;
            write_world(mail, "keys back to the player", "keys", "true");
            return;
        }
        if (refused) {
            return;
        }
        // What a moment leaves to do once it is done: the keys back to the player when the pilot
        // has gone; back to the pilot when a reset gave the world its defaults while it flies;
        // the first game's pause asked for once the world is there.
        if (done.action == kPilotLanded) {
            write_world(mail, "keys back to the player", "keys", "true");
        } else if (done.action == kWorldReset && ctx_->pilot_flying) {
            write_world(mail, "keys kept from the player", "keys", "false");
        } else if (done.action == kWorldLoaded && ctx_->pause_at > 0) {
            write_world(mail, "pause after step " + std::to_string(ctx_->pause_at), "pause_at",
                        std::to_string(ctx_->pause_at));
        }
    }

    ConsoleContext* ctx_;
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

std::string ConsoleContext::so(const char* stem) const { return dir + "/" + stem + kWeaveSuffix; }

} // namespace

int main(int argc, char** argv) {
    // The two ways to start: as a player, or with the pilot flying from birth and, if asked, the
    // first game pausing itself after a step.
    bool fly_at_birth = false;
    std::int64_t pause_at = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--pilot") {
            fly_at_birth = true;
        } else if (arg == "--pause-at" && i + 1 < argc) {
            char* end = nullptr;
            pause_at = std::strtoll(argv[++i], &end, 10);
            if (end == argv[i] || *end != '\0' || pause_at < 1) {
                pause_at = -1;
            }
        } else {
            pause_at = -1;
        }
        if (pause_at < 0) {
            std::fprintf(stderr, "usage: zengine-snake [--pilot] [--pause-at <step>]\n");
            return 2;
        }
    }

    // The honest line, in plain scrollback before the window takes the terminal: this host
    // isolates nothing anywhere, and on Windows it is the explicit development/demo backend.
    std::printf("zengine-snake - containment: %s\n", loom::Kernel::containment_note());
    std::fflush(stdout);

    // Where the bus spent its time (workshop/bus_tally.hpp), said once the weaves are gone and the
    // window has given the terminal back: declared before the bus, so it speaks after it.
    struct SaidLast {
        std::vector<std::string> lines;
        ~SaidLast() {
            for (const std::string& line : lines) {
                std::printf("zengine-snake - %s\n", line.c_str());
            }
            std::fflush(stdout);
        }
    } said_last;

    loom::Switchboard bus;
    zengine::workshop::BusTally tally(bus);
    // The demo loads exactly the artifacts its own build just produced, so it says so rather than
    // relying on a default the Loom no longer has.
    loom::Kernel kernel(
        bus, loom::trust_every_artifact("the snake demo loads only its own build output"));
    const loom::WeaveId control = loom::mount_control(kernel, bus);
    const loom::WeaveId manager = loom::mount_manager(control, bus);

    ConsoleContext ctx;
    ctx.manager = manager;
    ctx.control = control;
    ctx.pause_at = pause_at;
    ctx.dir = exe_dir();
    ctx.request_stop = [&bus] { bus.stop(); };
    loom::Grant reach; // the manager (the dangerous grant, target-scoped)...
    reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
    reach.allow(loom::SwapWeave::zen_name, loom::SwapWeave::zen_version, manager);
    reach.allow(loom::ReloadWeave::zen_name, loom::ReloadWeave::zen_version, manager);
    reach.allow(loom::ListLoaded::zen_name, loom::ListLoaded::zen_version, manager);
    // ...the kernel's door, for one thing: landing the pilot by its role...
    reach.allow(loom::UnloadRole::zen_name, loom::UnloadRole::zen_version, control);
    // ...the world's reset and write doors, by role (the n key; the keys, a pause)...
    reach.allow_to_role(loom::PokeResetState::zen_name, loom::PokeResetState::zen_version,
                        kWorldRole);
    reach.allow_to_role(loom::PokeWrite::zen_name, loom::PokeWrite::zen_version, kWorldRole);
    // ...the directory, as the window's arranger and as a drawer asking its room, and the window,
    // for the status line.
    reach.allow_to_role(gfx::AreaPlaced::zen_name, gfx::AreaPlaced::zen_version,
                        gfx::kGraphicsRole);
    reach.allow_to_role(gfx::PaletteOffered::zen_name, gfx::PaletteOffered::zen_version,
                        gfx::kGraphicsRole);
    reach.allow_to_role(gfx::PaletteChosen::zen_name, gfx::PaletteChosen::zen_version,
                        gfx::kGraphicsRole);
    reach.allow_to_role(gfx::ElementCodeOffered::zen_name, gfx::ElementCodeOffered::zen_version,
                        gfx::kGraphicsRole);
    reach.allow_to_role(gfx::RoomAsked::zen_name, gfx::RoomAsked::zen_version, gfx::kGraphicsRole);
    reach.allow_to_role(gfx::Drawn::zen_name, gfx::Drawn::zen_version, kWindowRole);
    auto weave = std::make_unique<ConsoleWeave>(ctx);
    ConsoleWeave* raw = weave.get();
    const loom::WeaveId op = bus.register_weave(std::move(weave), std::move(reach), kConsoleRole);
    raw->zen_set_self(op);

    // Startup commands are sent AS the console, asked in the same book its key commands use.
    const auto boot = [&](std::string label, const auto& cmd, int action = 0) {
        using Cmd = std::decay_t<decltype(cmd)>;
        if (const loom::AskOpened opened = ctx.ask_manager<Cmd>(std::move(label), action)) {
            bus.send_as(op, manager,
                        loom::Message(loom::to_value(cmd), op, op, opened.correlation));
        }
    };

    // Birth of the game: the directory first (its hello is what has the console arrange the
    // window), then the window that takes the terminal, the world, the pilot when it flies from
    // birth, and the timer service, whose loading is what starts time: every weave arranges its
    // own beat on its own activation, so the pilot hears the first game from its first step.
    boot("load directory", loom::LoadWeave{"zengine-graphics", ctx.so("zengine-graphics"),
                                           gfx::kGraphicsRole});
    boot("load TUI window", loom::LoadWeave{"zengine-window-tui",
                                                 ctx.so("zengine-window-tui"), kWindowRole});
    boot("load world v1", loom::LoadWeave{"snake-world-v1", ctx.so("snake-world-v1"), kWorldRole},
         kWorldLoaded);
    if (fly_at_birth) {
        boot("fly the pilot",
             loom::LoadWeave{"snake-pilot", ctx.so("snake-pilot"), kPilotRole}, kPilotFlown);
    }
    boot("load timer service",
         loom::LoadWeave{"zengine-timer", ctx.so("zengine-timer"), timer::kTimerRole});

    // The whole game runs in this loop: a turn, then one wait on the window's terminal for as long
    // as the Timer's beat allows. A turn that leaves nothing queued means nothing here will ever
    // speak again.
    zengine::timer::MonotonicClock clock;
    zengine::timer::Rest<zengine::timer::MonotonicClock> rest(clock);
    loom::host::Reactor wait(bus);
    wait.trust(kWindowRole);
    while (!ctx.quit) {
        (void)bus.pump_pending();
        if (ctx.quit) {
            break;
        }
        if (bus.pending() == 0) {
            std::printf("zengine-snake - the bus went quiet without a quit "
                        "(no timer service deployed?): time is gone, exiting.\n");
            break;
        }
        (void)wait.after_turn(rest.quiet_ms(bus));
    }
    said_last.lines = tally.lines();
    said_last.lines.push_back("rested " + std::to_string(wait.lasted()) +
                              " times between turns until the Timer's deadline, and was woken " +
                              std::to_string(wait.woken()) + " times before it");
    return 0;
}
