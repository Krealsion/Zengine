// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_FOLD_HPP
#define ZENGINE_OPERATOR_FOLD_HPP

// The fold, the evaluator's one form: a node that spends its operator reference once for each
// count from a start toward an exclusive limit by a step, threading an accumulator from an initial
// value. Its ports derive from its body, `(start, limit, step : Int, initial : T, ...) -> T`, the
// body's other ports wired from scope like any argument; the count is computed first, so no
// counted value is reached by repeated addition. Only `Catalog` spends a fold.
// Reference: docs/reference/operator-providers.md.

#include "operator/operator.hpp"

#include <zen/schema.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace zengine::op {

/// The most times one fold counts. A fold that would count more is refused before its body is
/// spent once; how much one evaluation spends in all is `kEvaluationSpends`'.
inline constexpr std::uint64_t kMaxFoldCount = 10000;

/// The fold's own ports, ahead of its body's other ports.
inline constexpr const char* kFoldStart = "start";
inline constexpr const char* kFoldLimit = "limit";
inline constexpr const char* kFoldStep = "step";
inline constexpr const char* kFoldInitial = "initial";

/// What a discoverer reads of the form: its name, what it is for and its shape.
inline constexpr const char* kFoldForm = "fold";
inline constexpr const char* kFoldAbout =
    "runs an operator once for each count from a start toward a limit by a step, threading an "
    "accumulator: a total, a count, a repeated step";
inline constexpr const char* kFoldSignature =
    "(start: Int, limit: Int, step: Int, initial: T, ...) -> T; body (accumulator: T, count: Int, "
    "...) -> T";

/// A fold's ports, derived from its body: the four of its own, then the body's other ports in the
/// body's order, and the one answer, the body's.
struct FoldPorts {
    std::vector<loom::Field> inputs;
    loom::Field answer;
};

/// Derive a fold's ports from its body and the two body ports it threads, or say in words why
/// that body cannot be a fold's: one answer, an Int port for the count, another port of the
/// answer's type for the accumulator, and no other port named as the fold's own.
inline FoldPorts fold_ports(const OperatorDef& body, const Fold& fold) {
    const std::string& id = body.identity();
    const std::vector<loom::Field>& outs = body.outputs()->fields();
    if (outs.size() != 1) {
        throw std::invalid_argument("'" + id + "' answers " + std::to_string(outs.size()) +
                                    " ports, and a fold threads one");
    }
    const loom::Field* count = body.inputs()->find(fold.count);
    if (count == nullptr) {
        throw std::invalid_argument("'" + id + "' has no port '" + fold.count + "' for the count");
    }
    if (count->type.kind != loom::Kind::Int) {
        throw std::invalid_argument("'" + id + "' port '" + fold.count + "' is " +
                                    loom::name_of(count->type.kind) + ", and the count is an Int");
    }
    if (fold.accumulator == fold.count) {
        throw std::invalid_argument("the count and the accumulator are two ports; '" + id +
                                    "' was given '" + fold.count + "' for both");
    }
    const loom::Field* acc = body.inputs()->find(fold.accumulator);
    if (acc == nullptr) {
        throw std::invalid_argument("'" + id + "' has no port '" + fold.accumulator +
                                    "' for the accumulator");
    }
    if (!detail::same_type(acc->type, outs[0].type)) {
        throw std::invalid_argument("'" + id + "' port '" + fold.accumulator + "' is " +
                                    loom::name_of(acc->type.kind) + " and it answers " +
                                    loom::name_of(outs[0].type.kind) +
                                    "; the accumulator and the answer are one type");
    }
    FoldPorts out;
    const loom::TypeRef integer = loom::type_of(loom::Kind::Int);
    out.inputs = {loom::Field{kFoldStart, integer, true}, loom::Field{kFoldLimit, integer, true},
                  loom::Field{kFoldStep, integer, true}, loom::Field{kFoldInitial, acc->type, true}};
    for (const loom::Field& f : body.inputs()->fields()) {
        if (f.name == fold.count || f.name == fold.accumulator) {
            continue;
        }
        for (const char* own : {kFoldStart, kFoldLimit, kFoldStep, kFoldInitial}) {
            if (f.name == own) {
                throw std::invalid_argument("'" + id + "' has a port named '" + f.name +
                                            "', which the fold's own ports already use");
            }
        }
        out.inputs.push_back(f);
    }
    out.answer = outs[0];
    return out;
}

/// How many times a fold counts from `start` toward `limit` by `step`: none when the step points
/// away from the limit, as C's loop. Exact for every Int; `step` is never 0 here.
inline std::uint64_t fold_count(std::int64_t start, std::int64_t limit, std::int64_t step) noexcept {
    std::uint64_t span = 0;
    std::uint64_t stride = 0;
    if (step > 0) {
        if (limit <= start) {
            return 0;
        }
        span = static_cast<std::uint64_t>(limit) - static_cast<std::uint64_t>(start);
        stride = static_cast<std::uint64_t>(step);
    } else {
        if (limit >= start) {
            return 0;
        }
        span = static_cast<std::uint64_t>(start) - static_cast<std::uint64_t>(limit);
        stride = std::uint64_t{0} - static_cast<std::uint64_t>(step);
    }
    return span / stride + (span % stride != 0 ? 1 : 0);
}

/// The `k`th count, `start + k * step`, computed from `k` rather than by repeated addition; it lies
/// between the start and the limit, so it is an Int whatever the step.
inline std::int64_t fold_value(std::int64_t start, std::int64_t step, std::uint64_t k) noexcept {
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(start) +
                                     k * static_cast<std::uint64_t>(step));
}

} // namespace zengine::op

#endif // ZENGINE_OPERATOR_FOLD_HPP
