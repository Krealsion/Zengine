// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_HOST_PUMP_HPP
#define ZENGINE_WORKSHOP_HOST_PUMP_HPP

// THE HOST'S TURN OF THE BUS, AND THE BOUNDARY A NATIVE OWNER'S SHOWING RUNS INSIDE.
//
// A joint publication is shown to each of its owners before that owner runs again, and what
// the showing came to is Loom's record: Applied, Declined, or Failed -- and a Failed owner is
// HELD until it is reloaded or removed. A loaded owner answers across its ABI as a status and
// never throws into the host. A NATIVE owner can say Failed in two ways (Loom's
// `weave_contract.hpp`): by returning it, or by throwing, which Loom records and then re-raises
// at whatever turn of the bus is running.
//
// THIS HOST TAKES THE FIRST WAY, AT A BOUNDARY IT OWNS. A native owner's application runs
// inside `contain_showing`: what that code throws is caught there, its words are written in
// the host's `ShowingFailures` book against the owner that threw, and the hook answers Failed
// -- recorded by Loom, the owner held, the operator told, with no exception to re-raise.
// `serve_until_idle` tells what the book holds, and catches nothing in order to explain it.
//
// ⚠ AN EXCEPTION CANNOT BE ATTRIBUTED FROM THE BUS AFTER THE FACT. A delivery refused
// `ApplicationFailed` just before an exception, and an owner Loom holds, are equally true when
// a host observer throws on the very next event -- or throws on that refusal's own
// notification, which replaces the owner's exception by ordinary C++ propagation before Loom
// can re-raise it. Only the code around the owner's own application knows the owner threw, so
// the words are captured there; every exception that reaches the turn -- a handler's, an
// observer's, a native owner's thrown outside the boundary -- leaves it exactly as it came.

#include <zen/switchboard/switchboard.hpp>

#include <exception>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// ONE NATIVE SHOWING THAT DID NOT COMPLETE, in the words of the owner that threw.
struct ShowingFailure {
    loom::WeaveId owner{}; ///< the weave whose application of its published claim threw
    std::string words;     ///< what it threw, as it said it
};

/// THE HOST'S BOOK OF NATIVE SHOWINGS THAT DID NOT COMPLETE. Written by `contain_showing` and
/// emptied by `serve_until_idle`, nothing else; it holds no owner and repairs nothing -- the
/// hold is Loom's record, and the book only keeps what the record has no room for: the words.
class ShowingFailures {
public:
    void note(loom::WeaveId owner, std::string words) {
        failures_.push_back(ShowingFailure{owner, std::move(words)});
    }
    /// Everything written since the last take, in the order it was written.
    std::vector<ShowingFailure> take() { return std::exchange(failures_, {}); }
    bool empty() const noexcept { return failures_.empty(); }

private:
    std::vector<ShowingFailure> failures_;
};

/// THE BOUNDARY. Run `apply` -- one native owner's application of a published claim, answering
/// Applied, or Declined for the expected non-application it is -- and turn anything it throws
/// into Loom's explicit Failed, the words kept in `book` against `owner`. With no book to keep
/// them, the exception is re-raised instead: Loom records the failure all the same and
/// re-raises it at the host's turn, where it propagates, described by nobody.
template <class Apply>
loom::Weave::PublishedClaim contain_showing(ShowingFailures* book, loom::WeaveId owner,
                                            Apply&& apply) {
    try {
        return std::forward<Apply>(apply)();
    } catch (const std::exception& e) {
        if (book == nullptr) {
            throw;
        }
        book->note(owner, e.what());
    } catch (...) {
        if (book == nullptr) {
            throw;
        }
        book->note(owner, "(not a std::exception)");
    }
    return loom::Weave::PublishedClaim::Failed;
}

/// WHAT ONE TURN OF THE HOST LOOP CAME TO: every native showing that did not complete in it,
/// in the order the boundaries wrote them, and what Loom's record says of each owner now.
struct ServedTurn {
    struct Failed {
        loom::WeaveId owner{};  ///< the owner whose application threw
        std::string office;     ///< the office it holds, if any
        bool held = false;      ///< Loom still holds it when the turn ends
        std::string diagnostic; ///< the sentence handed to `tell`
    };
    std::vector<Failed> failed;
};

/// SERVE THE BUS UNTIL IT IS IDLE, then tell what the boundaries wrote meanwhile: one sentence
/// per failure, handed to `tell` (a journal line, a printed line). Told on every exit -- also
/// when an exception is leaving, which is not examined, and leaves unchanged once it is told.
ServedTurn serve_until_idle(loom::Switchboard& bus, ShowingFailures& book,
                            const std::function<void(const std::string&)>& tell);

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_HOST_PUMP_HPP
