// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_NEOVIM_HOST_HPP
#define ZENGINE_NEOVIM_HOST_HPP

// One conversation with one Neovim, owned end to end: the process (`child.hpp`), the framing and
// books (`rpc.hpp`), the screen (`grid.hpp`), the module Zengine installs (`lua.hpp`), and the
// facts its notifications keep current (buffer, modified, mode). A weave holds one and decides
// what each fact means. `start` spawns, asks who it is, attaches as its UI, installs the module;
// `pump` moves bytes, never blocking; `call` and `ask_later` are answered in a later pump, the
// second within a bound; `finish` ends Neovim.
// Workshop law: agents/workshop/neovim.md

// Time: this owner reads no clock. Its bounds -- a question asked later, the startup, an exit's
// grace -- are measured on the time `pump` is handed, a reading of its host's wait's clock: each
// begins at the first time handed after it was set, since the time a handler last heard may be
// long past, and `due_ms` says when the next one comes, for the wait to wake its holder then.
// Only the end blocks, on the operating system's time (`finish`, through `child.hpp`).

// Readiness, as measured: `nvim_get_api_info` and `nvim_get_mode` answer whatever Neovim is
// doing, but `nvim_exec_lua` waits out a "Press ENTER" prompt. So a Neovim is ready when the
// module's install answers, and meanwhile the fast mode is asked: `{mode = 'r', blocking = true}`
// is a prompt, and starting fails with the screen's own words rather than waiting for a
// keystroke. A startup that neither answers nor prompts within `startup_ms` fails, said so.

// Failures, in their owners' words: not available (the operating system's), unsupported (below
// `kMinApiLevel`, naming the version), at a prompt (the last rows Neovim drew), protocol (a
// frame or screen event this model cannot read), exited (the status and the end of stderr). A
// failed owner answers every later call with its failure, and a callback whose answer cannot come
// runs with that failure exactly once.

// A request sent is never withdrawn: msgpack-RPC has no cancel, and Neovim runs a held request
// once its wait ends, in order (measured, suite `neovim_live`). A bound ends the waiting, not the
// request: `ask_later` says `Outstanding` and hands the late answer to `late`.

#include "editor/neovim/child.hpp"
#include "editor/neovim/grid.hpp"
#include "editor/neovim/lua.hpp"
#include "editor/neovim/msgpack.hpp"
#include "editor/neovim/rpc.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::neovim {

/// THE OLDEST NEOVIM THIS OWNER SPEAKS TO: API level 13 is Neovim 0.11. The declared, tested
/// range is 0.11.6 and 0.12.5 (editor/docs/neovim.md).
inline constexpr std::int64_t kMinApiLevel = 13;

/// How long a quitting Neovim is given before it is forced. Measured: `qa!` exits in 3-4 ms.
inline constexpr int kQuitGraceMs = 500;

struct Version {
    std::int64_t major = 0;
    std::int64_t minor = 0;
    std::int64_t patch = 0;
    std::int64_t api_level = 0;
    std::int64_t api_compatible = 0;

    std::string text() const {
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }
    /// Inside the declared range this repository tests (0.11.x and 0.12.x).
    bool tested() const noexcept { return major == 0 && (minor == 11 || minor == 12); }
};

/// WHAT NEOVIM'S NOTIFICATIONS SAID ABOUT THE CURRENT BUFFER, last.
struct DocFacts {
    bool known = false;
    std::int64_t buf = 0;
    std::string name;
    bool modified = false;
    std::int64_t tick = 0;
    std::string buftype;
};

/// A copy Neovim made to `+` or `*`, as its clipboard provider said it.
struct ClipboardCopy {
    std::vector<std::string> lines;
    std::string regtype;
};

/// WHAT ONE `pump` (and every read `start` and `answer_paste` made before it) OBSERVED.
struct Observed {
    bool flushed = false;
    bool doc_changed = false;
    bool mode_changed = false;
    bool became_ready = false;
    bool failed = false;
    bool ended = false;
    std::vector<ClipboardCopy> copies;
    std::vector<std::uint32_t> paste_requests;

    void merge(Observed&& o) {
        flushed = flushed || o.flushed;
        doc_changed = doc_changed || o.doc_changed;
        mode_changed = mode_changed || o.mode_changed;
        became_ready = became_ready || o.became_ready;
        failed = failed || o.failed;
        ended = ended || o.ended;
        for (ClipboardCopy& c : o.copies) {
            copies.push_back(std::move(c));
        }
        for (std::uint32_t id : o.paste_requests) {
            paste_requests.push_back(id);
        }
    }
};

class Host {
public:
    enum class Phase : std::uint8_t { Idle, Starting, Ready, Failed, Ended };

    struct Options {
        LaunchSpec launch;
        bool attach_ui = true;
        std::int64_t rows = 24;
        std::int64_t columns = 80;
        int startup_ms = 15000;
    };

    using Callback = std::function<void(const rpc::Response&)>;

    Host() = default;
    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;
    ~Host() { (void)finish(kQuitGraceMs); }

    // ---- starting -------------------------------------------------------------------------

    /// SPAWN AND BEGIN THE CONVERSATION. False (and `failure()` in words) when nothing runs.
    bool start(Options options) {
        if (phase_ != Phase::Idle) {
            return phase_ == Phase::Starting || phase_ == Phase::Ready;
        }
        options_ = std::move(options);
        ChildStart started = start_child(options_.launch);
        if (!started.started) {
            fail("Neovim is not available: " + started.trouble);
            return false;
        }
        child_ = std::move(started.child);
        phase_ = Phase::Starting;
        startup_.set(options_.startup_ms);
        (void)call("nvim_get_api_info", rpc::params(), [this](const rpc::Response& r) {
            on_api_info(r);
        });
        if (options_.attach_ui) {
            msgpack::Value::Map opts;
            opts.push_back({msgpack::Value::str("ext_linegrid"), msgpack::Value::boolean(true)});
            opts.push_back({msgpack::Value::str("ext_hlstate"), msgpack::Value::boolean(true)});
            opts.push_back({msgpack::Value::str("rgb"), msgpack::Value::boolean(true)});
            attached_rows_ = options_.rows;
            attached_columns_ = options_.columns;
            (void)call("nvim_ui_attach",
                       rpc::params(msgpack::Value::integer(options_.columns),
                                   msgpack::Value::integer(options_.rows),
                                   msgpack::Value::map(std::move(opts))),
                       [this](const rpc::Response& r) {
                           if (!r.error.is_nil() && phase_ == Phase::Starting) {
                               fail("Neovim refused to attach this pane as its interface: " +
                                    rpc::error_text(r.error));
                           }
                       });
        }
        carried_.merge(pump_io());
        return phase_ != Phase::Failed;
    }

    Phase phase() const noexcept { return phase_; }
    bool ready() const noexcept { return phase_ == Phase::Ready; }
    bool alive() const noexcept { return phase_ == Phase::Starting || phase_ == Phase::Ready; }
    const std::string& failure() const noexcept { return failure_; }
    const Version& version() const noexcept { return version_; }
    std::int64_t channel() const noexcept { return channel_; }
    bool clipboard_installed() const noexcept { return clipboard_; }
    std::int64_t pid() const noexcept { return child_.pid(); }
    const std::string& error_tail() const noexcept { return child_.error_tail(); }

    // ---- the conversation -------------------------------------------------------------------

    /// SEND ONE REQUEST. `cb` runs inside a later pump with the answer -- or, when the answer
    /// can no longer come, with this owner's failure as its error. False when nothing was sent
    /// (the owner is not alive, or the session refused); `cb` does not run then.
    bool call(std::string_view method, const msgpack::Value& params, Callback cb) {
        if (!alive()) {
            return false;
        }
        const std::optional<std::uint32_t> id = session_.request(method, params);
        if (!id.has_value()) {
            fail("Neovim could not be asked: " + session_.refusal());
            return false;
        }
        callbacks_.emplace(*id, std::move(cb));
        return true;
    }

    bool notify(std::string_view method, const msgpack::Value& params) {
        if (!alive()) {
            return false;
        }
        if (!session_.notify(method, params)) {
            fail("Neovim could not be told: " + session_.refusal());
            return false;
        }
        return true;
    }

    /// WHAT BECAME OF A REQUEST ASKED WITHIN A BOUND (`ask_later`).
    enum class Asked : std::uint8_t {
        Answered,    ///< within the bound, in `got` -- Neovim's answer, or this owner's failure
                     ///< as its error when Neovim ended first (`alive()` is false then)
        Unsent,      ///< nothing reached Neovim, and nothing will (`why`)
        Outstanding, ///< sent and not answered within the bound (`why`), and NOT WITHDRAWN:
                     ///< Neovim still holds it, and `late` will hear what became of it
    };

    /// WHAT A REQUEST ASKED LATER CAME TO (`ask_later`): `Answered` with `got` -- Neovim's answer,
    /// or this owner's failure as its error when Neovim ended first -- or `Outstanding` with `why`,
    /// sent and not withdrawn.
    struct Outcome {
        Asked asked = Asked::Answered;
        std::optional<rpc::Response> got;
        std::string why;
        std::uint64_t flushes = 0; ///< the screen's flushes when the answer arrived
    };
    using Done = std::function<void(Outcome)>;

    /// ONE REQUEST, NEVER WAITED FOR: sent now with the fast mode asked beside it, and `done` run
    /// exactly once inside a later pump -- with the answer, or `Outstanding` as soon as the fast
    /// mode says Neovim waits for input, or at the first pump handed a time `ms` past the bound's
    /// beginning (the first time handed after this ask). Then `late` hears what became of an
    /// outstanding request, exactly once. It runs inside Host code, so it records what it heard and
    /// asks nothing. False, with `why`, when nothing was sent; `done` never runs.
    bool ask_later(std::string_view method, const msgpack::Value& params, int ms, Done done,
                   std::string& why, Callback late = {}) {
        auto state = std::make_shared<Later>();
        state->done = std::move(done);
        state->late = std::move(late);
        state->bound.set(ms < 0 ? 0 : ms);
        if (!call(method, params, [this, state](const rpc::Response& r) {
                if (!state->settled) {
                    state->settled = true;
                    Done d = std::move(state->done);
                    d(Outcome{Asked::Answered, r, std::string(), grid_.flushes()});
                } else if (state->late) {
                    Callback l = std::move(state->late);
                    l(r);
                }
            })) {
            why = failure_.empty() ? std::string("Neovim is not running") : failure_;
            return false;
        }
        (void)call("nvim_get_mode", rpc::params(), [state](const rpc::Response& r) {
            const msgpack::Value* b = r.result.get("blocking");
            if (!state->settled && b != nullptr && b->as_bool(false)) {
                state->settled = true;
                Done d = std::move(state->done);
                d(Outcome{Asked::Outstanding, std::nullopt,
                          "Neovim is waiting for input (a prompt, or an unfinished command)"});
            }
        });
        later_.push_back(std::move(state));
        return true;
    }

    /// Requests asked later and not yet answered or said outstanding.
    std::size_t asked_later() const noexcept { return later_.size(); }

    /// TYPE INTO NEOVIM: keys in Neovim's notation (`keys.hpp`).
    bool input(std::string_view keys) {
        return notify("nvim_input", rpc::params(msgpack::Value::str(std::string(keys))));
    }

    /// THE ROOM CHANGED: ask Neovim to redraw for it.
    bool resize(std::int64_t rows, std::int64_t columns) {
        if (!options_.attach_ui || (rows == attached_rows_ && columns == attached_columns_)) {
            return true;
        }
        attached_rows_ = rows;
        attached_columns_ = columns;
        return notify("nvim_ui_try_resize",
                      rpc::params(msgpack::Value::integer(columns), msgpack::Value::integer(rows)));
    }

    /// ANSWER A PASTE Neovim's clipboard provider asked for: `[lines, regtype]`.
    bool answer_paste(std::uint32_t id, const std::vector<std::string>& lines, std::string_view regtype) {
        if (!alive()) {
            return false;
        }
        msgpack::Value::Array rows;
        for (const std::string& l : lines) {
            rows.push_back(msgpack::Value::str(l));
        }
        const bool ok = session_.respond(
            id, msgpack::Value::nil(),
            rpc::params(msgpack::Value::array(std::move(rows)), msgpack::Value::str(std::string(regtype))));
        carried_.merge(pump_io());
        return ok;
    }

    /// MOVE BYTES AND APPLY EVERY EVENT THAT ARRIVED, then say every bound the time has passed.
    /// Never blocks. `now_ms` is the time the host's wait handed with the wake this pump answers;
    /// none for a pump between wakes, which passes no bound and begins none.
    Observed pump(std::optional<std::int64_t> now_ms) {
        hand(now_ms);
        Observed out = std::move(carried_);
        carried_ = Observed{};
        out.merge(pump_io());
        expire_later();
        return out;
    }

    /// WHEN THIS OWNER'S NEXT BOUND COMES, on the clock it is handed: the soonest of a question's,
    /// the startup's (and its next look for a prompt) and an exit's grace; a time already past (0)
    /// when a bound waits for its first time, so the wait wakes its holder to hand one; none when
    /// no bound runs. Pumped at that time, it says what passed.
    std::optional<std::int64_t> due_ms() const {
        std::optional<std::int64_t> soonest;
        bool beginning = false;
        const auto take = [&](const Bound& b) {
            if (!b.running) {
                return;
            }
            if (!b.from) {
                beginning = true;
                return;
            }
            const std::int64_t at = *b.from + b.ms;
            soonest = soonest && *soonest < at ? *soonest : at;
        };
        for (const std::shared_ptr<Later>& s : later_) {
            if (!s->settled) {
                take(s->bound);
            }
        }
        if (phase_ == Phase::Starting) {
            take(startup_);
            if (channel_ != 0 && !mode_asked_) {
                take(mode_look_);
            }
        }
        if (alive()) {
            take(exit_grace_);
        }
        if (beginning) {
            return std::int64_t{0};
        }
        return soonest;
    }

    /// The one bounded wait: until output may be available, Neovim has gone, or `ms` passed.
    bool wait(int ms) { return alive() ? child_.wait(ms) : true; }

    /// What that wait waits on, for a host's one wait to hold instead: nothing while no Neovim
    /// runs. `*now` when there may be output already.
    loom::host::WaitSet wait_set(bool* now) const {
        *now = false;
        if (phase_ == Phase::Idle || !child_.holds()) {
            return loom::host::WaitSet{};
        }
        return child_.wait_set(now);
    }

    /// Whether this host must be pumped again soon whatever Neovim says: bytes the pipe has not
    /// taken are written only when it pumps. Its bounds are `due_ms`.
    bool on_time() const noexcept { return session_.outbound_size() > 0; }

    const Grid& grid() const noexcept { return grid_; }
    /// The first character of the mode Neovim last reported (`v`, `V`, Ctrl-V, `s`, ...), or 0.
    char visual_kind() const noexcept { return mode_.empty() ? '\0' : mode_[0]; }
    const std::string& mode() const noexcept { return mode_; }
    const DocFacts& doc() const noexcept { return doc_; }
    bool leaving() const noexcept { return leaving_; }
    std::size_t pending_calls() const noexcept { return callbacks_.size(); }

    // ---- ending -------------------------------------------------------------------------------

    /// ASK NEOVIM TO LEAVE, THEN FORCE. Idempotent. Every outstanding callback is run with the
    /// ending as its error. The one call that blocks: the quit is written within `kSayLastMs` and
    /// Neovim given `grace_ms` to leave, on the operating system's time (`child.hpp`).
    Child::Ended finish(int grace_ms) {
        if (phase_ == Phase::Idle) {
            return Child::Ended{};
        }
        if (child_.holds() && !child_.exited()) {
            if (alive() && session_.notify("nvim_command", rpc::params(msgpack::Value::str("qa!")))) {
                child_.say_last(session_.outbound(), kSayLastMs);
            }
        }
        Child::Ended ended = child_.finish(grace_ms);
        if (alive()) {
            phase_ = Phase::Ended;
            if (failure_.empty()) {
                failure_ = "Neovim was ended by this Workshop";
            }
        }
        orphan_callbacks();
        return ended;
    }

private:
    /// How long the quit is given to reach the pipe before Neovim is forced.
    static constexpr int kSayLastMs = 50;
    /// How often a starting Neovim is asked whether it stopped at a prompt.
    static constexpr std::int64_t kPromptLookMs = 100;
    /// How long an exit whose output never ends waits for it.
    static constexpr std::int64_t kExitGraceMs = 1000;

    /// ONE BOUND, on the time this owner is handed: set when it starts, it begins at the first
    /// time handed after that (`hand`), and has passed once `ms` more has been handed.
    struct Bound {
        bool running = false;
        std::optional<std::int64_t> from;
        std::int64_t ms = 0;

        void set(std::int64_t length) {
            running = true;
            from.reset();
            ms = length;
        }
        bool passed(const std::optional<std::int64_t>& now) const {
            return running && from && now && *now - *from >= ms;
        }
    };

    /// One request asked later: its outcome is said once, and an answer after that goes to `late`.
    struct Later {
        Done done;
        Callback late;
        Bound bound;
        bool settled = false;
    };

    /// THE TIME THE WAIT HANDED, kept as the latest, and every bound set since begun at it.
    void hand(const std::optional<std::int64_t>& now_ms) {
        if (!now_ms) {
            return;
        }
        if (!now_ || *now_ms > *now_) {
            now_ = now_ms;
        }
        const auto begin = [this](Bound& b) {
            if (b.running && !b.from) {
                b.from = now_;
            }
        };
        for (std::shared_ptr<Later>& s : later_) {
            begin(s->bound);
        }
        begin(startup_);
        begin(mode_look_);
        begin(exit_grace_);
    }

    /// EVERY REQUEST ASKED LATER WHOSE BOUND HAS PASSED is said outstanding; settled ones are let go.
    void expire_later() {
        if (later_.empty()) {
            return;
        }
        std::vector<std::shared_ptr<Later>> held = std::move(later_);
        later_.clear();
        for (std::shared_ptr<Later>& s : held) {
            if (!s->settled && s->bound.passed(now_)) {
                s->settled = true;
                Done d = std::move(s->done);
                d(Outcome{Asked::Outstanding, std::nullopt,
                          "Neovim did not answer within " + std::to_string(s->bound.ms) + " ms"});
            }
            if (!s->settled) {
                later_.push_back(std::move(s));
            }
        }
    }

    void fail(std::string why) {
        if (phase_ == Phase::Failed || phase_ == Phase::Ended) {
            return;
        }
        failure_ = std::move(why);
        phase_ = Phase::Failed;
        failed_now_ = true;
        (void)child_.finish(0);
        orphan_callbacks();
    }

    void orphan_callbacks() {
        std::map<std::uint32_t, Callback> gone = std::move(callbacks_);
        callbacks_.clear();
        for (auto& [id, cb] : gone) {
            rpc::Response r;
            r.id = id;
            r.error = msgpack::Value::str(failure_.empty() ? std::string("Neovim ended") : failure_);
            if (cb) {
                cb(r);
            }
        }
    }

    void on_api_info(const rpc::Response& r) {
        if (!r.error.is_nil()) {
            fail("Neovim did not say who it is: " + rpc::error_text(r.error));
            return;
        }
        channel_ = r.result.at(0).as_int();
        if (const msgpack::Value* v = r.result.at(1).get("version"); v != nullptr) {
            const auto field = [v](const char* name) {
                const msgpack::Value* f = v->get(name);
                return f != nullptr ? f->as_int() : 0;
            };
            version_.major = field("major");
            version_.minor = field("minor");
            version_.patch = field("patch");
            version_.api_level = field("api_level");
            version_.api_compatible = field("api_compatible");
        }
        if (version_.api_level < kMinApiLevel) {
            fail("Neovim " + version_.text() + " (API level " + std::to_string(version_.api_level) +
                 ") is older than this Workshop supports -- it needs Neovim 0.11 or newer");
            return;
        }
        (void)call("nvim_exec_lua",
                   rpc::params(msgpack::Value::str(lua::kModule),
                               rpc::params(msgpack::Value::integer(channel_))),
                   [this](const rpc::Response& installed) { on_module(installed); });
    }

    void on_module(const rpc::Response& r) {
        if (phase_ != Phase::Starting) {
            return;
        }
        if (!r.error.is_nil()) {
            fail("Neovim could not run Zengine's module: " + rpc::error_text(r.error));
            return;
        }
        if (const msgpack::Value* c = r.result.get("clipboard"); c != nullptr) {
            clipboard_ = c->as_bool(false);
        }
        phase_ = Phase::Ready;
        ready_now_ = true;
    }

    /// WHILE STARTING: a prompt fails the start, and so does a startup that outlives its bound.
    void watch_startup() {
        if (phase_ != Phase::Starting) {
            return;
        }
        if (startup_.passed(now_)) {
            fail("Neovim did not become ready within " +
                 (options_.startup_ms >= 1000 && options_.startup_ms % 1000 == 0
                      ? std::to_string(options_.startup_ms / 1000) + " seconds"
                      : std::to_string(options_.startup_ms) + " ms") +
                 screen_words());
            return;
        }
        if (channel_ != 0 && !mode_asked_ && (!mode_look_.running || mode_look_.passed(now_))) {
            mode_asked_ = true;
            mode_look_.set(kPromptLookMs);
            (void)call("nvim_get_mode", rpc::params(), [this](const rpc::Response& r) {
                mode_asked_ = false;
                if (phase_ != Phase::Starting) {
                    return;
                }
                const msgpack::Value* m = r.result.get("mode");
                const msgpack::Value* b = r.result.get("blocking");
                if (m != nullptr && b != nullptr && b->as_bool(false) && !m->as_str().empty() &&
                    m->as_str()[0] == 'r') {
                    fail("Neovim stopped at a prompt while starting" + screen_words());
                }
            });
        }
    }

    /// THE LAST THING NEOVIM DREW, for a failure's words. A prompt screen is the buffer's rows,
    /// then the message, then the prompt: so the filler and chrome rows are skipped, the rows it
    /// drew as errors or warnings are said when there are any, and otherwise the first rows of
    /// what is left (where a message begins) -- then the last row, which is the prompt itself.
    std::string screen_words() const {
        std::vector<std::string> alerts;
        std::vector<std::string> rows;
        for (std::int64_t r = 0; r < grid_.rows(); ++r) {
            std::string text;
            std::uint32_t groups = 0;
            std::int64_t chrome = 0;
            for (std::int64_t c = 0; c < grid_.columns(); ++c) {
                const Cell& cell = grid_.at(r, c);
                text += cell.text.size() == 1 ? cell.text : std::string("?");
                const std::uint32_t g = grid_.groups(cell.hl);
                groups |= g;
                chrome += (g & (ui_group::kStatusLine | ui_group::kTabLine)) != 0 ? 1 : 0;
            }
            while (!text.empty() && text.back() == ' ') {
                text.pop_back();
            }
            if (text.empty() || (grid_.groups_at(r, 0) & ui_group::kEndOfBuffer) != 0 ||
                chrome * 2 >= grid_.columns()) {
                continue;
            }
            rows.push_back(text);
            if ((groups & (ui_group::kErrorMsg | ui_group::kWarningMsg)) != 0 && alerts.size() < 4) {
                alerts.push_back(text);
            }
        }
        if (rows.empty()) {
            return std::string();
        }
        std::vector<std::string> said_rows = alerts;
        if (said_rows.empty()) {
            const std::size_t first = rows.size() - 1 < 4 ? rows.size() - 1 : 4;
            said_rows.assign(rows.begin(), rows.begin() + static_cast<std::ptrdiff_t>(first));
        }
        if (said_rows.empty() || said_rows.back() != rows.back()) {
            said_rows.push_back(rows.back());
        }
        std::string said = ": ";
        for (std::size_t i = 0; i < said_rows.size(); ++i) {
            if (i > 0) {
                said += " / ";
            }
            said += said_rows[i];
        }
        return said.size() > 700 ? said.substr(0, 697) + "..." : said;
    }

    Observed pump_io() {
        Observed out;
        if (phase_ == Phase::Idle) {
            return out;
        }
        if (child_.holds()) {
            const Child::Pump p = child_.pump(session_.outbound(), inbound_, kReadPerPump);
            if (!inbound_.empty()) {
                session_.receive(inbound_);
                inbound_.clear();
            }
            while (alive()) {
                std::optional<rpc::Event> ev = session_.next();
                if (!ev.has_value()) {
                    break;
                }
                apply(*ev, out);
            }
            if (alive() && !p.trouble.empty() && !p.exited) {
                fail("the pipe to Neovim failed: " + p.trouble);
            }
            if (p.exited && !p.output_ended && !exit_grace_.running) {
                exit_grace_.set(kExitGraceMs);
            }
            // AN EXIT WHOSE OUTPUT NEVER ENDS -- something Neovim started still holds its end --
            // is an exit all the same once its grace has passed.
            const bool gone = p.output_ended || (p.exited && exit_grace_.passed(now_));
            if (alive() && gone) {
                // THE LAST WORDS ARE IN: stderr is drained once more, then the status is reaped.
                std::string none;
                std::string drained;
                (void)child_.pump(none, drained, 0);
                const Child::Ended ended = child_.finish(0);
                const std::string tail = child_.error_tail();
                std::string said = leaving_ ? "Neovim exited" : "Neovim exited unexpectedly";
                said += " (status " + std::to_string(ended.status) + ")";
                if (!tail.empty()) {
                    said += ": " + tail.substr(tail.size() > 400 ? tail.size() - 400 : 0);
                }
                failure_ = said;
                phase_ = Phase::Ended;
                ended_now_ = true;
                orphan_callbacks();
            }
        }
        watch_startup();
        out.became_ready = std::exchange(ready_now_, false) || out.became_ready;
        out.failed = std::exchange(failed_now_, false) || out.failed;
        out.ended = std::exchange(ended_now_, false) || out.ended;
        return out;
    }

    void apply(rpc::Event& ev, Observed& out) {
        if (auto* r = std::get_if<rpc::Response>(&ev)) {
            auto it = callbacks_.find(r->id);
            if (it == callbacks_.end()) {
                return;
            }
            Callback cb = std::move(it->second);
            callbacks_.erase(it);
            if (cb) {
                cb(*r);
            }
            return;
        }
        if (auto* n = std::get_if<rpc::Notification>(&ev)) {
            if (n->method == "redraw") {
                Applied applied = grid_.apply(n->params);
                if (!applied.refusal.empty()) {
                    fail("Neovim sent a screen this Workshop cannot read: " + applied.refusal);
                    return;
                }
                out.flushed = out.flushed || applied.flushed;
                return;
            }
            const msgpack::Value& payload = n->params.at(0);
            if (n->method == "zengine_doc") {
                doc_.known = true;
                doc_.buf = payload.get("buf") != nullptr ? payload.get("buf")->as_int() : 0;
                doc_.name = payload.get("name") != nullptr ? payload.get("name")->as_str() : std::string();
                doc_.modified = payload.get("modified") != nullptr && payload.get("modified")->as_bool(false);
                doc_.tick = payload.get("tick") != nullptr ? payload.get("tick")->as_int() : 0;
                doc_.buftype = payload.get("buftype") != nullptr ? payload.get("buftype")->as_str() : std::string();
                out.doc_changed = true;
                return;
            }
            if (n->method == "zengine_mode") {
                mode_ = payload.as_str();
                out.mode_changed = true;
                return;
            }
            if (n->method == "zengine_clipboard_copy") {
                ClipboardCopy copy;
                for (const msgpack::Value& line : payload.at(0).as_array()) {
                    copy.lines.push_back(line.as_str());
                }
                copy.regtype = payload.at(1).as_str();
                out.copies.push_back(std::move(copy));
                return;
            }
            if (n->method == "zengine_leaving") {
                leaving_ = true;
                return;
            }
            return;
        }
        if (auto* q = std::get_if<rpc::Request>(&ev)) {
            if (q->method == "zengine_clipboard_paste") {
                out.paste_requests.push_back(q->id);
                return;
            }
            (void)session_.respond(q->id, msgpack::Value::str("Zengine does not answer " + q->method),
                                   msgpack::Value::nil());
            return;
        }
        if (auto* e = std::get_if<rpc::ProtocolError>(&ev)) {
            fail(e->why);
        }
    }

    static constexpr std::size_t kReadPerPump = 4u * 1024u * 1024u;

    Options options_;
    Child child_;
    rpc::Session session_;
    std::string inbound_;
    std::map<std::uint32_t, Callback> callbacks_;
    std::vector<std::shared_ptr<Later>> later_;
    Grid grid_;
    Phase phase_ = Phase::Idle;
    std::string failure_;
    Version version_;
    std::int64_t channel_ = 0;
    bool clipboard_ = false;
    std::string mode_;
    DocFacts doc_;
    bool leaving_ = false;
    std::int64_t attached_rows_ = 0;
    std::int64_t attached_columns_ = 0;
    std::optional<std::int64_t> now_; ///< the latest time handed
    Bound startup_;
    Bound mode_look_;  ///< until a starting Neovim is asked again whether it is at a prompt
    Bound exit_grace_;
    bool mode_asked_ = false;
    bool ready_now_ = false;
    bool failed_now_ = false;
    bool ended_now_ = false;
    Observed carried_;
};

} // namespace zengine::neovim

#endif // ZENGINE_NEOVIM_HOST_HPP
