// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_HOST_PUMP_HPP
#define ZENGINE_WORKSHOP_HOST_PUMP_HPP

// The host's turn of the bus, and the boundary a native owner's showing runs inside: what a native
// owner's application throws is caught at `contain_showing`, written in the host's
// `ShowingFailures` book and answered as Loom's Failed, since only the code around the owner's
// own application can attribute the exception. Anything else reaching the turn leaves it as it
// came.

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
