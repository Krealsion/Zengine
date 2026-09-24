// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_NEOVIM_HOST_HPP
#define ZENGINE_NEOVIM_HOST_HPP

// ONE CONVERSATION WITH ONE NEOVIM, OWNED END TO END: the process (`child.hpp`), the framing and
// the books (`rpc.hpp`), the screen (`grid.hpp`), the module Zengine installs in it (`lua.hpp`),
// and the facts its notifications keep current -- the current buffer, whether it is modified,
// the mode. A weave holds one of these and decides what each fact means; this decides nothing
// about Workshop, a pane, a transfer or a switch.
//
//     start(options)   spawn, then ask (fast) who it is, attach as its UI, install the module
//     pump()           move bytes both ways and apply every event that arrived; never blocks
//     call(m, p, cb)   one request; `cb` runs inside a later `pump`, with the answer
//     ask(...)         one request waited for within a bound -- the only waiting this owner does,
//                      and only when a caller asked for it; unanswered is not withdrawn
//     call_now(...)    a question asked that way, with nobody to hear a late answer
//     finish(grace)    ask Neovim to leave (`qa!`), give it `grace`, then force
//
// ---- READINESS, AS MEASURED ------------------------------------------------------------
//
// `nvim_get_api_info` and `nvim_get_mode` are FAST: Neovim answers them whatever it is doing.
// `nvim_exec_lua` is not: at a "Press ENTER" prompt -- a broken configuration, a message that
// filled the screen -- it waits until the prompt is gone (measured). So a Neovim is READY when the
// module's own install answers, and while that is outstanding this owner asks the fast mode
// beside it: `{mode = 'r', blocking = true}` is a prompt, and starting FAILS with the screen's own
// words rather than waiting for a keystroke nobody will type. A startup that neither answers nor
// prompts within `startup_ms` fails too, said so.
//
// ---- WHAT FAILS, AND IN WHOSE WORDS ---------------------------------------------------------
//
//   not available   the program was not found, or could not be started -- the operating
//                   system's words
//   unsupported     an API level below `kMinApiLevel`, naming the version found
//   at a prompt     the last rows Neovim drew, which is where it said what went wrong
//   protocol        a frame that is not msgpack-RPC, or a screen event this model cannot read
//   exited          Neovim's exit status and the end of what it wrote to stderr
//
// A failed or ended owner answers every later call with its failure, and a callback whose answer
// can no longer come is run with that failure as its error -- exactly once, never silently
// dropped.
//
// ---- A REQUEST SENT IS NEVER WITHDRAWN ------------------------------------------------------
//
// msgpack-RPC has no cancel, and Neovim runs a request it held while it waited for input once the
// wait ends, in the order the requests were sent (measured, suite `neovim_live`). A bound that
// runs out therefore ends the WAITING, never the request: `ask` says `Outstanding`, not refused,
// and hands the answer that arrives later to the caller's `late`, so a caller whose request
// changes something keeps owning it until that answer comes.

#include "neovim/child.hpp"
#include "neovim/grid.hpp"
#include "neovim/lua.hpp"
#include "neovim/msgpack.hpp"
#include "neovim/rpc.hpp"

#include <chrono>
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
/// range is 0.11.6 and 0.12.5 (docs/workshop/neovim.md).
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

/// WHAT ONE `pump` (and every pump a `call_now` made before it) OBSERVED.
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
        started_at_ = Clock::now();
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

    /// WHAT BECAME OF A REQUEST WAITED FOR WITHIN A BOUND (`ask`).
    enum class Asked : std::uint8_t {
        Answered,    ///< within the bound, in `got` -- Neovim's answer, or this owner's failure
                     ///< as its error when Neovim ended first (`alive()` is false then)
        Unsent,      ///< nothing reached Neovim, and nothing will (`why`)
        Outstanding, ///< sent and not answered within the bound (`why`), and NOT WITHDRAWN:
                     ///< Neovim still holds it, and `late` will hear what became of it
    };

    /// ONE REQUEST, WAITED FOR WITHIN `ms`. Every event that arrives meanwhile is applied exactly
    /// as a pump applies it, and what those pumps observed is handed to the next `pump()`. It asks
    /// the fast mode beside the request, so a Neovim waiting at a prompt or in an unfinished
    /// command is said to be waiting (`Outstanding`, at once) rather than timed out.
    ///
    /// When it returns `Outstanding`, `late` runs EXACTLY ONCE, inside a later pump: with the
    /// answer when Neovim runs the request, or with this owner's failure as its error when Neovim
    /// ends first. It runs inside Host code, so it records what it heard and asks nothing.
    Asked ask(std::string_view method, const msgpack::Value& params, int ms,
              std::optional<rpc::Response>& got, std::string& why, Callback late = {}) {
        // THE WAITER'S STATE OUTLIVES THIS CALL: an answer that arrives after the bound ran out is
        // still delivered to its callback, which hands it to `late` rather than to a frame that is
        // gone.
        struct Waiting {
            std::optional<rpc::Response> got;
            bool mode_answered = false;
            bool blocking = false;
            bool left = false;
            Callback late;
        };
        auto state = std::make_shared<Waiting>();
        state->late = std::move(late);
        if (!call(method, params, [state](const rpc::Response& r) {
                if (!state->left) {
                    state->got = r;
                } else if (state->late) {
                    state->late(r);
                }
            })) {
            why = failure_.empty() ? std::string("Neovim is not running") : failure_;
            return Asked::Unsent;
        }
        (void)call("nvim_get_mode", rpc::params(), [state](const rpc::Response& r) {
            state->mode_answered = true;
            if (const msgpack::Value* b = r.result.get("blocking"); b != nullptr) {
                state->blocking = b->as_bool(false);
            }
        });
        const auto deadline = Clock::now() + std::chrono::milliseconds(ms < 0 ? 0 : ms);
        for (;;) {
            carried_.merge(pump_io());
            if (state->got.has_value()) {
                got = std::move(state->got);
                return Asked::Answered;
            }
            if (!alive()) {
                // DEFENSIVE: `orphan_callbacks` answers every callback when an owner ends, so the
                // ending is normally `got` above. It is the answer here too, and nothing later is.
                rpc::Response ended;
                ended.error = msgpack::Value::str(failure_.empty() ? std::string("Neovim ended") : failure_);
                got = std::move(ended);
                state->left = true;
                state->late = nullptr;
                return Asked::Answered;
            }
            if (state->mode_answered && state->blocking) {
                why = "Neovim is waiting for input (a prompt, or an unfinished command)";
                state->left = true;
                return Asked::Outstanding;
            }
            const auto now = Clock::now();
            if (now >= deadline) {
                why = "Neovim did not answer within " + std::to_string(ms) + " ms";
                state->left = true;
                return Asked::Outstanding;
            }
            const auto left =
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
            (void)child_.wait(static_cast<int>(left < 10 ? left : 10));
        }
    }

    /// A QUESTION, ANSWERED WITHIN `ms` OR NOT AT ALL (`why`): `ask` with nobody to hear a late
    /// answer. Neovim still runs a request it was sent, so this suits a request whose running late
    /// changes nothing its caller said -- a read. A request that changes something is `ask`ed with
    /// a `late`, and its caller owns it until it is answered.
    std::optional<rpc::Response> call_now(std::string_view method, const msgpack::Value& params,
                                          int ms, std::string& why) {
        std::optional<rpc::Response> got;
        if (ask(method, params, ms, got, why) != Asked::Answered) {
            return std::nullopt;
        }
        if (!alive() && !got->error.is_nil()) {
            why = rpc::error_text(got->error);
            return std::nullopt;
        }
        return got;
    }

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

    /// MOVE BYTES AND APPLY EVERY EVENT THAT ARRIVED. Never blocks.
    Observed pump() {
        Observed out = std::move(carried_);
        carried_ = Observed{};
        out.merge(pump_io());
        return out;
    }

    /// The one bounded wait: until output may be available, Neovim has gone, or `ms` passed.
    bool wait(int ms) { return alive() ? child_.wait(ms) : true; }

    const Grid& grid() const noexcept { return grid_; }
    /// The first character of the mode Neovim last reported (`v`, `V`, Ctrl-V, `s`, ...), or 0.
    char visual_kind() const noexcept { return mode_.empty() ? '\0' : mode_[0]; }
    const std::string& mode() const noexcept { return mode_; }
    const DocFacts& doc() const noexcept { return doc_; }
    bool leaving() const noexcept { return leaving_; }
    std::size_t pending_calls() const noexcept { return callbacks_.size(); }

    // ---- ending -------------------------------------------------------------------------------

    /// ASK NEOVIM TO LEAVE, THEN FORCE. Idempotent. Every outstanding callback is run with the
    /// ending as its error.
    Child::Ended finish(int grace_ms) {
        if (phase_ == Phase::Idle) {
            return Child::Ended{};
        }
        if (child_.holds() && !child_.exited()) {
            if (alive() && session_.notify("nvim_command", rpc::params(msgpack::Value::str("qa!")))) {
                std::string ignored;
                const auto deadline = Clock::now() + std::chrono::milliseconds(50);
                while (session_.outbound_size() > 0 && Clock::now() < deadline) {
                    (void)child_.pump(session_.outbound(), ignored, 1u << 16);
                    (void)child_.wait(1);
                }
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
    using Clock = std::chrono::steady_clock;

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
        const auto now = Clock::now();
        if (now - started_at_ > std::chrono::milliseconds(options_.startup_ms)) {
            fail("Neovim did not become ready within " +
                 (options_.startup_ms >= 1000 && options_.startup_ms % 1000 == 0
                      ? std::to_string(options_.startup_ms / 1000) + " seconds"
                      : std::to_string(options_.startup_ms) + " ms") +
                 screen_words());
            return;
        }
        if (channel_ != 0 && !mode_asked_ && now - last_mode_ask_ > std::chrono::milliseconds(100)) {
            mode_asked_ = true;
            last_mode_ask_ = now;
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
            if (p.exited && !p.output_ended && exited_at_ == Clock::time_point{}) {
                exited_at_ = Clock::now();
            }
            // AN EXIT WHOSE OUTPUT NEVER ENDS -- something Neovim started still holds its end --
            // is an exit all the same once a second has passed.
            const bool gone = p.output_ended ||
                              (p.exited && Clock::now() - exited_at_ > std::chrono::seconds(1));
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
    Clock::time_point started_at_{};
    Clock::time_point last_mode_ask_{};
    Clock::time_point exited_at_{};
    bool mode_asked_ = false;
    bool ready_now_ = false;
    bool failed_now_ = false;
    bool ended_now_ = false;
    Observed carried_;
};

} // namespace zengine::neovim

#endif // ZENGINE_NEOVIM_HOST_HPP
