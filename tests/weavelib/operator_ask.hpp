// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_WEAVELIB_OPERATOR_ASK_HPP
#define ZENGINE_TESTS_WEAVELIB_OPERATOR_ASK_HPP

// HOW A SUITE TALKS TO THE DYNAMIC STRANGER (OPH-0 §10) — four shapes, and none
// of them carries an operator.
//
// The distinction this vocabulary exists to keep sharp: TRIGGERING the stranger
// is a message, and so is REPORTING what it found, because those are the only
// two things a suite in another image can do. The OPERATOR CALL ITSELF is not.
// It happens synchronously inside one delivery, through the injected host, and
// it enqueues nothing — which is why `OperatorEvaluateAsk` carries a
// `repetitions` count: a witness that asks for one evaluation cannot tell a
// synchronous call from a message round trip, and a witness that asks for
// sixteen and still spends the same number of bus turns can.
//
// The arguments travel as TEXT, exactly as SEM-0's in-process stranger's did and
// for the same reason: a consumer that took `std::int64_t` and `bool` would
// already know the signature, which is the thing under test. The stranger reads
// each port's KIND off the schema the host described and converts against that.

#include <zen/weave.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::testing {

/// One argument, named by the port a maker means and spelled as text.
///
/// A nested message inside a list, rather than two parallel lists, because a
/// port and its value are one fact and two lists that must stay the same length
/// are two facts pretending.
struct OperatorArgument {
    std::string port;
    std::string text;
    ZEN_SHAPE(OperatorArgument, 1, ZEN_FIELD(port), ZEN_FIELD(text));
};

/// Suite -> stranger: what does the host say this operator's contract is?
struct OperatorDescribeAsk {
    std::string identity;
    ZEN_SHAPE(OperatorDescribeAsk, 1, ZEN_FIELD(identity));
};

/// Stranger -> suite: the contract, rendered.
///
/// `inputs` and `outputs` are the ports the stranger DISCOVERED, written
/// `name:Kind` and joined by commas — Loom's own kind spellings, off the schema
/// the host handed over. Rendering rather than re-encoding is deliberate: a
/// suite comparing text is reading what the stranger actually understood, not
/// replaying bytes it never looked at.
struct OperatorSignatureSaid {
    std::int64_t status = 0;
    std::string identity;
    std::string inputs;
    std::string outputs;
    ZEN_SHAPE(OperatorSignatureSaid, 1, ZEN_FIELD(status), ZEN_FIELD(identity),
              ZEN_FIELD(inputs), ZEN_FIELD(outputs));
};

/// Suite -> stranger: spend this operator, `repetitions` times, and tell me what
/// the last one said.
struct OperatorEvaluateAsk {
    std::string identity;
    std::vector<OperatorArgument> arguments;
    std::int64_t repetitions = 1;
    ZEN_SHAPE(OperatorEvaluateAsk, 1, ZEN_FIELD(identity), ZEN_FIELD(arguments),
              ZEN_FIELD(repetitions));
};

/// Stranger -> suite: what came back.
///
/// `status` is the seam's own `ZengineOperatorStatus`, widened to the Int a Loom
/// field can carry — so a suite can tell "no host was ever offered" from "the
/// host has no such operator" from "the operator refused", which is the whole
/// point of there being five of them.
struct OperatorReadingSaid {
    bool ok = false;
    std::int64_t status = 0;
    std::string reason;
    std::string port;
    std::string answer;
    ZEN_SHAPE(OperatorReadingSaid, 1, ZEN_FIELD(ok), ZEN_FIELD(status), ZEN_FIELD(reason),
              ZEN_FIELD(port), ZEN_FIELD(answer));
};

} // namespace zengine::testing

#endif // ZENGINE_TESTS_WEAVELIB_OPERATOR_ASK_HPP
