// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_FLOW_NATIVE_HPP
#define ZENGINE_FLOW_NATIVE_HPP

#include "flow/native_abi.h"
#include "maker/runtime.hpp"
#include "operator/host.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace zengine::flow {

inline void emit_bytes(ZenByteSink sink, std::string_view bytes) {
    if (sink.write != nullptr)
        sink.write(sink.ctx, reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
}

inline std::string hex(std::string_view bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (char byte : bytes) {
        const auto c = static_cast<unsigned char>(byte);
        out += digits[c >> 4u];
        out += digits[c & 15u];
    }
    return out;
}

inline std::string unhex(std::string_view text) {
    if (text.size() % 2 != 0) throw std::invalid_argument("odd length hex data");
    auto digit = [](char c) -> unsigned {
        if (c >= '0' && c <= '9') return static_cast<unsigned>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<unsigned>(c - 'a' + 10);
        throw std::invalid_argument("invalid hex data");
    };
    std::string out;
    for (std::size_t i = 0; i < text.size(); i += 2)
        out += static_cast<char>((digit(text[i]) << 4u) | digit(text[i + 1]));
    return out;
}

inline maker::Definition definition_from_hex(std::string_view text) {
    auto admitted = maker::read_definition(unhex(text));
    if (!admitted) throw std::invalid_argument(admitted.reason);
    return std::move(admitted.definition);
}

// The generated statements own wiring and execution order. This object owns one
// call's schema checks and packing; it never receives or walks a graph.
class Step {
public:
    Step(op::OperatorHost host, std::string root, std::size_t index,
         std::string identity, loom::ContentId inputs, loom::ContentId outputs)
        : host_(host), context_("'" + root + "' step " + std::to_string(index)),
          signature_(host_.describe(std::to_string(index))), arguments_(schema(identity)) {
        if (signature_.inputs->content_id() != inputs ||
            signature_.outputs->content_id() != outputs)
            throw op::Refusal(context_ + ": '" + identity +
                "' is not the signature this composition was authored against");
    }

    void input(std::size_t port, const loom::Value& inputs, std::string_view field) {
        if (port >= signature_.inputs->fields().size()) return;
        const auto* cell = inputs.get(field);
        if (cell == nullptr)
            throw op::Refusal(context_ + ": no input named '" + std::string(field) + "'");
        value(port, *cell);
    }

    void value(std::size_t port, const loom::Cell& cell) {
        if (port < signature_.inputs->fields().size())
            arguments_.set(signature_.inputs->fields()[port].name, cell);
    }

    loom::Value run() const {
        auto answer = host_.evaluate(signature_, arguments_);
        if (!answer) throw op::Refusal(answer.reason.empty() ?
            context_ + ": operator host failed (" + std::to_string(answer.status) + ")" :
            answer.reason);
        return std::move(*answer.value);
    }

private:
    std::shared_ptr<const loom::Schema> schema(const std::string& identity) const {
        if (!signature_)
            throw op::Refusal(context_ + ": unresolved operator reference '" + identity + "'");
        return signature_.inputs;
    }
    op::OperatorHost host_;
    std::string context_;
    op::HostSignature signature_;
    loom::Value arguments_;
};

inline std::shared_ptr<const loom::Schema> result_schema(const maker::Definition& definition,
                                                        const maker::On& trigger) {
    return loom::make_schema(definition.trigger_identity(trigger) + ".out", 1,
        {loom::Field{"value", definition.state->find(trigger.output)->type, true}});
}

using NativeRunner = loom::Value (*)(std::size_t, op::OperatorHost, const loom::Value&);

inline ZengineOperatorStatus execute_native(const maker::Definition& definition,
        NativeRunner run, std::size_t index, const ZengineOperatorHostApiV1* steps,
        const std::uint8_t* bytes, std::size_t size, ZenByteSink answer, ZenByteSink reason) noexcept {
    try {
        if (index >= definition.on.size()) throw std::invalid_argument("unknown trigger index");
        const auto& trigger = definition.on[index];
        const auto identity = definition.trigger_identity(trigger);
        const auto input_schema = maker::pack_schema(identity, *definition.state, *trigger.message);
        auto in = loom::admit(loom::parse(std::string_view(
            reinterpret_cast<const char*>(bytes), size)), input_schema);
        if (!in) throw op::Refusal("'" + identity + "' refused its arguments: " +
                                  in.first_error().message());
        auto value = run(index, op::OperatorHost::over(steps), in.value());
        loom::Value out(result_schema(definition, trigger));
        out.set("value", *value.at(0));
        auto gated = loom::admit(std::move(out), *result_schema(definition, trigger));
        if (!gated) throw op::Refusal("'" + identity +
            "' produced an answer its own output schema refuses: " + gated.first_error().message());
        emit_bytes(answer, loom::serialize(gated.value()));
        return ZENGINE_OP_OK;
    } catch (const op::Refusal& e) {
        emit_bytes(reason, e.reason());
        return ZENGINE_OP_ERR_REFUSED;
    } catch (const std::exception& e) {
        emit_bytes(reason, e.what());
        return ZENGINE_OP_ERR_REFUSED;
    } catch (...) {
        emit_bytes(reason, "compiled body failed");
        return ZENGINE_OP_ERR_HOST_FAILED;
    }
}

// The ordinary Loom weave half spends the outer trigger through the live host as
// well. Its numbered host view is supplied by CompiledModule::offer(), per instance.
class NativeWeave : public maker::Runtime {
public:
    explicit NativeWeave(maker::Definition definition)
        : Runtime(std::move(definition)), host_(op::OperatorHost::offered()) {
        if (!host_) throw std::runtime_error("generated Flow weave needs a compiled-module offer");
    }
private:
    op::Evaluation evaluate_body(const maker::On& trigger, const loom::Value& message) override {
        const auto index = static_cast<std::size_t>(&trigger - definition_.on.data());
        const auto signature = host_.describe(std::to_string(index));
        if (!signature) return op::Evaluation::refuse("unresolved trigger `" +
            definition_.trigger_identity(trigger) + "`: its body is not mounted");
        auto answer = host_.evaluate(signature, maker::pack(state(), message, signature.inputs));
        if (!answer) return op::Evaluation::refuse(answer.reason);
        return op::Evaluation::accept(std::move(*answer.value));
    }
    op::OperatorHost host_;
};

} // namespace zengine::flow
#endif
