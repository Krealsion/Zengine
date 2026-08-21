// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// THE DYNAMIC STRANGER (OPH-0 §10) — a real loadable weave that spends the
// host's operator truth and has no way to reach the catalog that holds it.
//
// WHAT IT KNOWS
//     an operator identity          handed to it, in a message, as a string
//     arguments                     handed to it, in a message, as TEXT
//     that a host MIGHT have offered it an operator surface
//
// WHAT IT DOES NOT KNOW, and — unlike SEM-0's in-process stranger — could not
// find out: which primitives a rule is built from, what a composition is, what
// the Timer is, what a millisecond is, and above all what the catalog TYPE even
// is. That last one is the difference this phase paid for. SEM-0's fence was a
// grep, because its stranger was handed a catalog by reference and linked the
// package containing the class; this file lives in ANOTHER IMAGE, links only the
// consumer package — which contains no catalog, no definition type and no
// primitive — and could not construct a private one to answer from if it tried.
// The proof is not the link line: it is that replacing a primitive in the HOST
// changes what this weave answers, with no edit here.
//
// ITS OWN PROSE AVOIDS THE HOST'S VOCABULARY, deliberately. The independence
// tripwire in `test_operator_host.cpp` is a plain substring search over this
// file, which is what makes it readable and unfoolable; a comment that spelled
// the names it is checking for would either break the check or force it to be
// clever about comments, and a clever tripwire is one nobody trusts.
//
// ONE SOURCE, THREE LIBRARIES (the weavelib pattern), and the difference under
// test is one declaration:
//
//   (default)                   zengine-operator-consumer-weave
//                                 writes ZENGINE_OPERATOR_CONSUMER() and takes
//                                 the offer
//   OPH_STRANGER_LEGACY         zengine-operator-consumer-weave-legacy
//                                 writes nothing at all: an ordinary weave that
//                                 never heard of operators, which must load and
//                                 run exactly as it always would
//   OPH_STRANGER_ABI            zengine-operator-consumer-weave-abi
//                                 exports the surface at a version this host
//                                 does not speak — an artifact from another era,
//                                 which must be REFUSED and not guessed at
//
// THE OPERATOR CALL IS NOT A MESSAGE. `on(OperatorEvaluateAsk)` performs every
// evaluation synchronously, inside one delivery, before it returns. The only
// bus traffic is the ask that arrived and the reading that goes back — which is
// what `repetitions` is for: sixteen evaluations cost the same two turns one
// does.

#include "operator_ask.hpp"

#include "operator/host.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

#include <charconv>
#include <cstdint>
#include <string>
#include <system_error>

#if defined(OPH_STRANGER_ABI) || defined(OPH_STRANGER_LEGACY)
/// Neither variant writes `ZENGINE_OPERATOR_CONSUMER()`, so neither has an offer
/// slot to read. One name for that, so the constructor below states the fact
/// once instead of testing for two unrelated-looking flags.
#define OPH_STRANGER_NO_SURFACE 1
#endif

namespace {

namespace op = zengine::op;
using zengine::testing::OperatorArgument;
using zengine::testing::OperatorDescribeAsk;
using zengine::testing::OperatorEvaluateAsk;
using zengine::testing::OperatorReadingSaid;
using zengine::testing::OperatorSignatureSaid;

/// The ports this stranger discovered, written the way a reader reads them.
std::string render_ports(const loom::Schema& schema) {
    std::string out;
    for (const loom::Field& f : schema.fields()) {
        if (!out.empty()) {
            out += ",";
        }
        out += f.name;
        out += ":";
        out += loom::name_of(f.type.kind);
    }
    return out;
}

/// A text argument, converted against the kind the PORT declares. Canonical
/// spellings only: a reader that guessed would be inventing an argument nobody
/// wrote.
bool cell_from_text(loom::Kind kind, const std::string& text, loom::Cell& out, std::string& why) {
    switch (kind) {
    case loom::Kind::Int: {
        std::int64_t value = 0;
        const char* first = text.data();
        const char* last = first + text.size();
        const std::from_chars_result r = std::from_chars(first, last, value);
        if (r.ec != std::errc{} || r.ptr != last) {
            why = "'" + text + "' is not an Int";
            return false;
        }
        out = loom::Cell::integer(value);
        return true;
    }
    case loom::Kind::Bool:
        if (text == "true") {
            out = loom::Cell::boolean(true);
            return true;
        }
        if (text == "false") {
            out = loom::Cell::boolean(false);
            return true;
        }
        why = "'" + text + "' is not a Bool";
        return false;
    default:
        why = std::string("this reader understands Int and Bool ports only, not ") +
              loom::name_of(kind);
        return false;
    }
}

std::string render(const loom::Cell& cell, std::string& why) {
    switch (cell.kind()) {
    case loom::Kind::Int:
        return std::to_string(cell.as_int());
    case loom::Kind::Bool:
        return cell.as_bool() ? "true" : "false";
    default:
        why = std::string("this reader cannot render a ") + loom::name_of(cell.kind());
        return {};
    }
}

struct StrangerState {
    /// How many evaluations this instance has performed — kept in the state so a
    /// suite can read it through the poke door, and so a RELOADED instance
    /// starting at zero is visible.
    std::int64_t evaluations = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(StrangerState, 1, ZEN_FIELD(evaluations));
};

class OperatorStrangerWeave
    : public loom::WeaveBase<OperatorStrangerWeave, StrangerState,
                             loom::Accept<OperatorDescribeAsk, OperatorEvaluateAsk>,
                             loom::Emit<OperatorSignatureSaid, OperatorReadingSaid>> {
public:
#if defined(OPH_STRANGER_NO_SURFACE)
    /// The two variants that declare no v1 surface. There is no slot to read —
    /// `ZENGINE_OPERATOR_CONSUMER()` is what emits one — so this instance starts
    /// unbound and every ask below answers ZENGINE_OP_ERR_NO_HOST. That the
    /// difference between an operator-aware artifact and an ordinary one is
    /// exactly ONE DECLARATION at the bottom of this file is the claim.
    OperatorStrangerWeave() = default;
#else
    /// TAKE THE OFFER IN THE CONSTRUCTOR — which is to say, inside `create()`,
    /// the very first moment this instance exists. That is the earliest point a
    /// consumer could legitimately need an operator, and proving the host is
    /// already reachable HERE is what makes "the surface is available before it
    /// is needed" a fact rather than a hope about delivery ordering.
    ///
    /// A copy, so it keeps working after the host has withdrawn the offer for
    /// whatever it loads next. An unbound one is a perfectly ordinary state and
    /// every ask below answers ZENGINE_OP_ERR_NO_HOST rather than crashing.
    OperatorStrangerWeave() : operators_(op::OperatorHost::offered()) {}
#endif

    void on(const OperatorDescribeAsk& ask, loom::Mail& mail) {
        const op::HostSignature sig = operators_.describe(ask.identity);
        OperatorSignatureSaid said;
        said.status = static_cast<std::int64_t>(sig.status);
        said.identity = sig.identity;
        if (sig.ok()) {
            said.inputs = render_ports(*sig.inputs);
            said.outputs = render_ports(*sig.outputs);
        }
        (void)mail.send(mail.sender(), said);
    }

    void on(const OperatorEvaluateAsk& ask, loom::Mail& mail) {
        OperatorReadingSaid said = evaluate(ask);
        (void)mail.send(mail.sender(), said);
    }

private:
    /// EVERYTHING HERE IS SYNCHRONOUS. Describe, build the pack, spend the
    /// operator `repetitions` times, read the answer. Not one line of it touches
    /// `mail`, and the whole function runs to completion before the caller's
    /// delivery returns.
    OperatorReadingSaid evaluate(const OperatorEvaluateAsk& ask) {
        OperatorReadingSaid said;
        const op::HostSignature contract = operators_.describe(ask.identity);
        if (!contract.ok()) {
            said.status = static_cast<std::int64_t>(contract.status);
            said.reason = contract.status == ZENGINE_OP_ERR_NO_HOST
                              ? "no operator host was offered to this weave"
                              : "this host publishes no '" + ask.identity + "'";
            return said;
        }

        loom::Value pack(contract.inputs);
        for (const OperatorArgument& arg : ask.arguments) {
            const loom::Field* port = contract.inputs->find(arg.port);
            if (port == nullptr) {
                said.status = ZENGINE_OP_ERR_REFUSED;
                said.reason = "'" + ask.identity + "' has no input port named '" + arg.port + "'";
                return said;
            }
            loom::Cell cell = loom::Cell::integer(0);
            if (!cell_from_text(port->type.kind, arg.text, cell, said.reason)) {
                said.status = ZENGINE_OP_ERR_REFUSED;
                return said;
            }
            pack.set(arg.port, std::move(cell));
        }

        // Anything still missing is the HOST GATE's refusal, said in the gate's
        // own words across the seam. This reader counts nothing and checks no
        // arity.
        op::HostAnswer answer;
        const std::int64_t times = ask.repetitions < 1 ? 1 : ask.repetitions;
        for (std::int64_t i = 0; i < times; ++i) {
            answer = operators_.evaluate(contract, pack);
            ++state_.evaluations;
            if (!answer.ok()) {
                break;
            }
        }

        said.status = static_cast<std::int64_t>(answer.status);
        if (!answer.ok()) {
            said.reason = answer.reason;
            return said;
        }
        said.port = contract.outputs->fields()[0].name;
        said.answer = render(*answer.value->at(0), said.reason);
        said.ok = said.reason.empty();
        return said;
    }

    op::OperatorHost operators_;
};

} // namespace

ZEN_EXPORT_WEAVE(OperatorStrangerWeave)

#if defined(OPH_STRANGER_ABI)
// AN ARTIFACT FROM ANOTHER ERA, written by hand because that is what it is: a
// consumer surface at a version this host does not speak. There is deliberately
// no production door for declaring a wrong version — a macro parameter that let
// a shipped weave claim to be v99 would be a door whose only user is a test.
//
// ITS `offer` IS NOT NULL, and that is the sharp part. A null one would let the
// host refuse for a second reason and the witness could not tell which reason it
// refused for; a real function pointer means the ONLY thing wrong with this
// artifact is the number, and the refusal must name the number. If a host ever
// called it anyway, the reason string would be the wrong one and the case would
// go red.
extern "C" {
static ZengineOperatorStatus oph_stranger_stale_offer(const ZengineOperatorHostApiV1*) {
    return ZENGINE_OP_ERR_ABI;
}
ZEN_KERNEL_EXPORT const ZengineOperatorConsumerV1* zengine_operator_consumer(void) {
    static const ZengineOperatorConsumerV1 table = {
        .abi_version = ZENGINE_OPERATOR_ABI_VERSION + 1u, .offer = oph_stranger_stale_offer};
    return &table;
}
}
#elif defined(OPH_STRANGER_LEGACY)
// Nothing. An ordinary weave that never heard of operators — which must load,
// run and answer exactly as it did before this seam existed. Its `describe` and
// `evaluate` answer ZENGINE_OP_ERR_NO_HOST, which is the floor and not a fault.
#else
ZENGINE_OPERATOR_CONSUMER();
#endif
