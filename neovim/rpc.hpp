// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_NEOVIM_RPC_HPP
#define ZENGINE_NEOVIM_RPC_HPP

// MSGPACK-RPC, AS ONE CONVERSATION WITH ONE NEOVIM.
//
// Three message kinds cross (`:help msgpack-rpc`):
//
//     [0, id, method, params]     a request   -- either side may send one
//     [1, id, error, result]      a response  -- to the other side's request
//     [2, method, params]         a notification
//
// This session owns the FRAMING and the BOOKS, and no transport: bytes a transport read are
// `receive`d, and `outbound()` is what a transport should write next. It never blocks, never
// sleeps and never owns a thread; a caller decides when bytes move.
//
// ⚠ A RESPONSE IS MATCHED BY ITS ID AND BY NOTHING ELSE. Neovim does not answer in request order:
// a request that runs `vim.wait()` lets later requests be served inside the wait, so the answer to
// the second request can arrive first (measured, on Windows and Linux).
// `Response::id` is therefore the only key a caller may use, and an id this session never issued
// is a protocol error rather than an answer to somebody's question.
//
// ⚠ AND THE OUTBOUND QUEUE IS BOUNDED. A child that stops reading its stdin cannot make this
// process hold an unbounded amount of what it was going to be told: a request that would take the
// queue past `max_outbound` is refused at the call, in words, and nothing of it is queued.

#include "neovim/msgpack.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace zengine::neovim::rpc {

struct Response {
    std::uint32_t id = 0;
    msgpack::Value error;  ///< nil when the request succeeded
    msgpack::Value result;
};

struct Notification {
    std::string method;
    msgpack::Value params; ///< always an array
};

struct Request {
    std::uint32_t id = 0;
    std::string method;
    msgpack::Value params;
};

/// The framing broke or the other side said something no conversation allows. The session is
/// finished after one of these: `broken()` is true and every later `next()` repeats nothing.
struct ProtocolError {
    std::string why;
};

using Event = std::variant<Response, Notification, Request, ProtocolError>;

/// The words Neovim puts in an error response: `[type, message]`, or a bare string from older
/// servers. Empty for a nil error.
inline std::string error_text(const msgpack::Value& error) {
    if (error.is_nil()) {
        return std::string();
    }
    if (error.is_array()) {
        const std::string& message = error.at(1).as_str();
        return message.empty() ? std::string("an error with no message") : message;
    }
    if (error.is_str()) {
        return error.as_str();
    }
    return std::string("an error of an unexpected kind");
}

class Session {
public:
    struct Bounds {
        msgpack::Limits inbound{};
        std::size_t max_outbound = 64u * 1024u * 1024u;
        std::size_t max_pending = 4096; ///< requests awaiting an answer at once
    };

    Session() : Session(Bounds{}) {}
    explicit Session(Bounds bounds) : bounds_(bounds), decoder_(bounds.inbound) {}

    /// Queue a request. `params` must be an array. Returns the id, or nullopt when nothing was
    /// queued -- the session is broken, the queue would outgrow its bound, or too many requests
    /// are already outstanding; `refusal()` says which.
    std::optional<std::uint32_t> request(std::string_view method, const msgpack::Value& params) {
        std::string frame;
        const std::uint32_t id = next_id_;
        {
            msgpack::Writer w(frame);
            w.array_header(4);
            w.uinteger(0);
            w.uinteger(id);
            w.str(method);
            w.value(params);
        }
        if (!admit(frame, true)) {
            return std::nullopt;
        }
        ++next_id_;
        if (next_id_ == 0) {
            next_id_ = 1; // 0 is never issued, so a zero id always names nothing
        }
        pending_.insert(id);
        outbound_ += frame;
        return id;
    }

    /// Queue a notification. False when nothing was queued (`refusal()` says why).
    bool notify(std::string_view method, const msgpack::Value& params) {
        std::string frame;
        {
            msgpack::Writer w(frame);
            w.array_header(3);
            w.uinteger(2);
            w.str(method);
            w.value(params);
        }
        if (!admit(frame, false)) {
            return false;
        }
        outbound_ += frame;
        return true;
    }

    /// Answer a request the other side made. False when nothing was queued.
    bool respond(std::uint32_t id, const msgpack::Value& error, const msgpack::Value& result) {
        std::string frame;
        {
            msgpack::Writer w(frame);
            w.array_header(4);
            w.uinteger(1);
            w.uinteger(id);
            w.value(error);
            w.value(result);
        }
        if (!admit(frame, false)) {
            return false;
        }
        outbound_ += frame;
        return true;
    }

    /// Bytes a transport read from the other side, in order.
    void receive(std::string_view bytes) { decoder_.feed(bytes); }

    /// The next whole event, or nullopt when none is complete yet.
    std::optional<Event> next() {
        if (broken_) {
            return std::nullopt;
        }
        msgpack::Decoder::Result r = decoder_.next();
        if (r.status == msgpack::Decoder::Status::NeedMore) {
            return std::nullopt;
        }
        if (r.status != msgpack::Decoder::Status::Value) {
            return breaks("Neovim sent bytes that are not msgpack-RPC: " + r.why);
        }
        const msgpack::Value& m = r.value;
        if (!m.is_array() || m.size() < 3 || !m.at(0).is_int()) {
            return breaks("Neovim sent a message that is not an RPC frame");
        }
        switch (m.at(0).as_int()) {
        case 0: {
            if (m.size() != 4 || !m.at(1).is_int() || !m.at(2).is_str() || !m.at(3).is_array()) {
                return breaks("Neovim sent a malformed request");
            }
            Request q;
            q.id = static_cast<std::uint32_t>(m.at(1).as_uint());
            q.method = m.at(2).as_str();
            q.params = m.at(3);
            return Event{std::move(q)};
        }
        case 1: {
            if (m.size() != 4 || !m.at(1).is_int()) {
                return breaks("Neovim sent a malformed response");
            }
            const std::uint64_t raw = m.at(1).as_uint();
            const std::uint32_t id = static_cast<std::uint32_t>(raw);
            if (raw > UINT32_MAX || pending_.erase(id) == 0) {
                return breaks("Neovim answered request " + std::to_string(raw) +
                              ", which this session never sent or already heard");
            }
            Response a;
            a.id = id;
            a.error = m.at(2);
            a.result = m.at(3);
            return Event{std::move(a)};
        }
        case 2: {
            if (m.size() != 3 || !m.at(1).is_str() || !m.at(2).is_array()) {
                return breaks("Neovim sent a malformed notification");
            }
            Notification n;
            n.method = m.at(1).as_str();
            n.params = m.at(2);
            return Event{std::move(n)};
        }
        default:
            return breaks("Neovim sent an RPC frame of unknown type " +
                          std::to_string(m.at(0).as_int()));
        }
    }

    /// What a transport should write next. It removes what it wrote from the front.
    std::string& outbound() noexcept { return outbound_; }
    std::size_t outbound_size() const noexcept { return outbound_.size(); }

    std::size_t pending() const noexcept { return pending_.size(); }
    bool is_pending(std::uint32_t id) const { return pending_.count(id) != 0; }
    std::size_t buffered_inbound() const noexcept { return decoder_.buffered(); }

    bool broken() const noexcept { return broken_; }
    const std::string& refusal() const noexcept { return refusal_; }

private:
    bool admit(const std::string& frame, bool is_request) {
        if (broken_) {
            return false; // `refusal_` still holds why the conversation ended
        }
        if (outbound_.size() + frame.size() > bounds_.max_outbound) {
            refusal_ = "Neovim is not reading what it is sent: " +
                       std::to_string(outbound_.size()) + " bytes are already waiting, and " +
                       std::to_string(frame.size()) + " more would pass the bound of " +
                       std::to_string(bounds_.max_outbound);
            return false;
        }
        if (is_request && pending_.size() >= bounds_.max_pending) {
            refusal_ = std::to_string(pending_.size()) +
                       " requests to Neovim are already unanswered";
            return false;
        }
        return true;
    }

    std::optional<Event> breaks(std::string why) {
        broken_ = true;
        refusal_ = why;
        return Event{ProtocolError{std::move(why)}};
    }

    Bounds bounds_;
    msgpack::Decoder decoder_;
    std::string outbound_;
    std::uint32_t next_id_ = 1;
    std::set<std::uint32_t> pending_;
    bool broken_ = false;
    std::string refusal_;
};

/// Build an array value from any number of values -- the one helper request call sites want.
template <class... Args>
inline msgpack::Value params(Args&&... args) {
    msgpack::Value::Array a;
    a.reserve(sizeof...(Args));
    (a.push_back(std::forward<Args>(args)), ...);
    return msgpack::Value::array(std::move(a));
}

} // namespace zengine::neovim::rpc

#endif // ZENGINE_NEOVIM_RPC_HPP
