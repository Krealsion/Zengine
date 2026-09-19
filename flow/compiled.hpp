// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_FLOW_COMPILED_HPP
#define ZENGINE_FLOW_COMPILED_HPP

#include "flow/native.hpp"
#include "maker/weave.hpp"
#include "operator/image.hpp"

#include <charconv>
#include <memory>
#include <string>
#include <vector>

namespace zengine::flow {

// A numbered view of existing catalog identities, not a second operator store.
// The numbers address immutable authoring metadata, never provider implementations.
// This also carries names containing NUL through the existing NUL-terminated ABI.
class IndexedHost {
public:
    IndexedHost(const op::Catalog& catalog, std::vector<std::string> identities)
        : catalog_(&catalog), identities_(std::move(identities)) {
        table_.abi_version = ZENGINE_OPERATOR_ABI_VERSION;
        table_.ctx = this;
        table_.describe = &describe;
        table_.evaluate = &evaluate;
    }
    IndexedHost(const IndexedHost&) = delete;
    IndexedHost& operator=(const IndexedHost&) = delete;
    const ZengineOperatorHostApiV1* api() const noexcept { return &table_; }

private:
    const std::string* identity(const char* key) const noexcept {
        if (key == nullptr) return nullptr;
        const std::string_view text(key);
        std::size_t index = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), index);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
            index >= identities_.size()) return nullptr;
        return &identities_[index];
    }
    static ZengineOperatorStatus describe(void* context, const char* key, ZenByteSink sink) {
        try {
            const auto& self = *static_cast<IndexedHost*>(context);
            const auto* name = self.identity(key);
            const auto* found = name == nullptr ? nullptr : self.catalog_->find(*name);
            if (found == nullptr) return ZENGINE_OP_ERR_NOT_FOUND;
            emit_bytes(sink, loom::serialize(op::encode_operator_desc(key,
                *found->inputs(), *found->outputs())));
            return ZENGINE_OP_OK;
        } catch (...) { return ZENGINE_OP_ERR_HOST_FAILED; }
    }
    static ZengineOperatorStatus evaluate(void* context, const char* key,
            const std::uint8_t* bytes, std::size_t size, ZenByteSink answer, ZenByteSink reason) {
        try {
            const auto& self = *static_cast<IndexedHost*>(context);
            const auto* name = self.identity(key);
            if (name == nullptr) return ZENGINE_OP_ERR_NOT_FOUND;
            auto result = self.catalog_->evaluate(*name, loom::parse(std::string_view(
                reinterpret_cast<const char*>(bytes), size)));
            if (!result) {
                emit_bytes(reason, result.reason());
                return ZENGINE_OP_ERR_REFUSED;
            }
            emit_bytes(answer, loom::serialize(result.value()));
            return ZENGINE_OP_OK;
        } catch (...) { return ZENGINE_OP_ERR_HOST_FAILED; }
    }
    const op::Catalog* catalog_;
    std::vector<std::string> identities_;
    ZengineOperatorHostApiV1 table_{};
};

// Open only artifacts the host has elected to execute. ImageShare may run native
// initialization before any descriptor is read; this is not a sandbox or policy door.
// Keep the catalog alive past the module, and the module alive past loaded weaves.
class CompiledModule : public std::enable_shared_from_this<CompiledModule> {
public:
    static std::shared_ptr<CompiledModule> open(op::Catalog& catalog, const std::string& path) {
        return std::shared_ptr<CompiledModule>(new CompiledModule(catalog, path));
    }
    const maker::Definition& definition() const noexcept { return definition_; }
    const std::string& path() const noexcept { return path_; }

    op::MountReport mount(op::MountMode mode = op::MountMode::Ordinary) {
        auto definitions = maker::definitions_of(definition_);
        const auto keeper = shared_from_this();
        for (std::size_t i = 0; i < definition_.on.size(); ++i) {
            const auto& interpreted = definitions[i];
            definitions[i] = op::OperatorDef(interpreted.identity(), interpreted.inputs(),
                interpreted.outputs(), [keeper, i](const loom::Value& arguments) {
                    return keeper->execute(i, arguments);
                });
        }
        return catalog_->mount(definition_.provider(), std::move(definitions), mode, keeper);
    }

    // Bracket exactly one Kernel::load/reload. Each created instance copies the
    // table, whose context lives in this module. Nested/concurrent offers are unsupported
    // by the underlying operator-consumer ABI, as with OperatorOffer.
    class Offer {
    public:
        explicit Offer(std::shared_ptr<CompiledModule> module) : module_(std::move(module)) {
            const auto entry = reinterpret_cast<const ZengineOperatorConsumerV1* (*)()>(
                module_->image_.symbol("zengine_operator_consumer"));
            if (entry == nullptr) throw std::runtime_error("artifact has no operator consumer");
            consumer_ = entry();
            if (consumer_ == nullptr || consumer_->abi_version != ZENGINE_OPERATOR_ABI_VERSION ||
                consumer_->offer == nullptr ||
                consumer_->offer(module_->roots_->api()) != ZENGINE_OP_OK)
                throw std::runtime_error("artifact refused the compiled-module offer");
        }
        ~Offer() { consumer_->offer(nullptr); }
        Offer(const Offer&) = delete;
        Offer& operator=(const Offer&) = delete;
    private:
        std::shared_ptr<CompiledModule> module_;
        const ZengineOperatorConsumerV1* consumer_ = nullptr;
    };

    Offer offer() { return Offer(shared_from_this()); }

private:
    CompiledModule(op::Catalog& catalog, std::string path)
        : image_(path), path_(std::move(path)), catalog_(&catalog) {
        if (!image_.open()) throw std::runtime_error("cannot open compiled Flow artifact " + path_);
        const auto entry = reinterpret_cast<ZengineFlowEntryV1>(
            image_.symbol("zengine_flow_artifact"));
        if (entry == nullptr) throw std::runtime_error("artifact has no Flow surface");
        api_ = entry();
        if (api_ == nullptr || api_->abi_version != ZENGINE_FLOW_ABI_VERSION ||
            api_->definition == nullptr || api_->execute == nullptr)
            throw std::runtime_error("unsupported Flow artifact ABI");
        std::string bytes;
        if (api_->definition(op::detail::sink_into(bytes)) != ZENGINE_OP_OK)
            throw std::runtime_error("artifact could not describe its definition");
        auto admitted = maker::read_definition(bytes);
        if (!admitted) throw std::runtime_error(admitted.reason);
        definition_ = std::move(admitted.definition);
        std::vector<std::string> roots;
        for (const auto& trigger : definition_.on) roots.push_back(definition_.trigger_identity(trigger));
        roots_ = std::make_unique<IndexedHost>(catalog, std::move(roots));
    }

    loom::Cell execute(std::size_t index, const loom::Value& arguments) const {
        std::vector<std::string> names;
        for (const auto& node : definition_.on[index].body.nodes) names.push_back(node.identity);
        IndexedHost steps(*catalog_, std::move(names));
        const auto bytes = loom::serialize(arguments);
        std::string answer, reason;
        const auto result = api_->execute(index, steps.api(),
            reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size(),
            op::detail::sink_into(answer), op::detail::sink_into(reason));
        if (result != ZENGINE_OP_OK)
            throw op::Refusal(reason.empty() ? "compiled body failed" : reason);
        auto admitted = loom::admit(loom::parse(answer), result_schema(definition_, definition_.on[index]));
        if (!admitted) throw op::Refusal(admitted.first_error().message());
        return *admitted.value().at(0);
    }
    op::ImageShare image_;
    std::string path_;
    op::Catalog* catalog_;
    const ZengineFlowArtifactV1* api_ = nullptr;
    maker::Definition definition_;
    std::unique_ptr<IndexedHost> roots_;
};

} // namespace zengine::flow
#endif
