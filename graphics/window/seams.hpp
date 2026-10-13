// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_WINDOW_SEAMS_HPP
#define ZENGINE_GRAPHICS_WINDOW_SEAMS_HPP

// The code a window opens and calls in its own handler: skins (painter code) and element code,
// each opened by library path as operator providers are (operator/image.hpp), its version read
// first, and every answer read through the one gate. Ours and a stranger's are opened alike.
// Reference: graphics/docs/graphics.md, "The two seams".

#include "graphics/element_abi.h"
#include "graphics/painter_abi.h"
#include "graphics/painting.hpp"

#include "operator/image.hpp"

#include <zen/weave/lifecycle.hpp>

#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace zengine::graphics::window {

namespace detail {

inline ZenByteSink sink(loom::Bytes& into) {
    return ZenByteSink{&into, [](void* ctx, const std::uint8_t* data, std::size_t len) {
                           auto* b = static_cast<loom::Bytes*>(ctx);
                           b->insert(b->end(), data, data + len);
                       }};
}
inline ZenByteSink sink(std::string& into) {
    return ZenByteSink{&into, [](void* ctx, const std::uint8_t* data, std::size_t len) {
                           static_cast<std::string*>(ctx)->append(
                               reinterpret_cast<const char*>(data), len);
                       }};
}

/// One library's table of type T, found by `symbol`: nullptr with `why` when it is not there.
template <class T>
const T* open_table(zengine::op::ImageShare& share, const std::string& library, const char* symbol,
                    const char* what, std::string& why) {
    if (!share.open()) {
        why = "could not open '" + library + "' to look for " + what;
        return nullptr;
    }
    void* found = share.symbol(symbol);
    if (found == nullptr) {
        why = "'" + library + "' exports no " + what;
        return nullptr;
    }
    using Entry = const T* (*)();
    Entry entry = nullptr;
    std::memcpy(&entry, &found, sizeof(entry));
    return entry();
}

} // namespace detail

/// A window's skins, opened on first use and closed when no area paints with them.
class Painters {
public:
    const ZenginePainterV1* get(const std::string& library, std::string& why) {
        const auto it = loaded_.find(library);
        if (it != loaded_.end()) {
            return it->second->table;
        }
        auto image = std::make_unique<Loaded>(library);
        const ZenginePainterV1* t = detail::open_table<ZenginePainterV1>(
            image->share, library, ZENGINE_PAINTER_SYMBOL, "skin", why);
        if (t == nullptr) {
            return nullptr;
        }
        if (t->abi_version != ZENGINE_PAINTER_ABI_VERSION || t->paint == nullptr) {
            why = "'" + library + "' offers painter seam version " +
                  std::to_string(t->abi_version) + ", not " +
                  std::to_string(ZENGINE_PAINTER_ABI_VERSION);
            return nullptr;
        }
        image->table = t;
        loaded_[library] = std::move(image);
        return t;
    }

    void close(const std::string& library) { loaded_.erase(library); }

    /// One call across the seam: the task's bytes in, the answer's bytes out and read.
    std::optional<PaintResult> paint(const ZenginePainterV1* skin, const PaintTask& task,
                                     std::string& why) {
        const loom::Bytes in = loom::bequeath_item(task);
        loom::Bytes out;
        std::string reason;
        ++calls;
        if (skin->paint(in.data(), in.size(), detail::sink(out), detail::sink(reason)) != 0) {
            why = "the skin refused to paint: " + reason;
            return std::nullopt;
        }
        auto result = loom::claim_item<PaintResult>(out);
        if (!result) {
            why = "the skin answered something that does not read as a PaintResult";
        }
        return result;
    }

    std::int64_t calls = 0;

private:
    struct Loaded {
        explicit Loaded(const std::string& path) : share(path) {}
        zengine::op::ImageShare share;
        const ZenginePainterV1* table = nullptr;
    };
    std::map<std::string, std::unique_ptr<Loaded>> loaded_;
};

/// A window's element code, opened and described on first use.
class ElementCodes {
public:
    struct Code {
        const ZengineElementV1* table = nullptr;
        ElementDescribed described;
    };

    const Code* get(const std::string& library, std::string& why) {
        const auto it = loaded_.find(library);
        if (it != loaded_.end()) {
            return &it->second->code;
        }
        auto image = std::make_unique<Loaded>(library);
        const ZengineElementV1* t = detail::open_table<ZengineElementV1>(
            image->share, library, ZENGINE_ELEMENT_SYMBOL, "element code", why);
        if (t == nullptr) {
            return nullptr;
        }
        if (t->abi_version != ZENGINE_ELEMENT_ABI_VERSION || t->describe == nullptr ||
            t->act == nullptr) {
            why = "'" + library + "' offers element seam version " +
                  std::to_string(t->abi_version) + ", not " +
                  std::to_string(ZENGINE_ELEMENT_ABI_VERSION);
            return nullptr;
        }
        loom::Bytes out;
        std::string reason;
        if (t->describe(detail::sink(out), detail::sink(reason)) != 0) {
            why = "'" + library + "' could not say what it is: " + reason;
            return nullptr;
        }
        const auto described = loom::claim_item<ElementDescribed>(out);
        if (!described) {
            why = "'" + library + "' described itself in a shape that does not read";
            return nullptr;
        }
        image->code = Code{t, *described};
        const Code* code = &image->code;
        loaded_[library] = std::move(image);
        return code;
    }

    void close(const std::string& library) { loaded_.erase(library); }

    /// One call across the seam: the event's bytes in, the outcome's bytes out and read.
    std::optional<ElementOutcome> act(const Code& code, const ElementEvent& event,
                                      std::string& why) {
        const loom::Bytes in = loom::bequeath_item(event);
        loom::Bytes out;
        std::string reason;
        ++calls;
        if (code.table->act(in.data(), in.size(), detail::sink(out), detail::sink(reason)) != 0) {
            why = reason;
            return std::nullopt;
        }
        auto outcome = loom::claim_item<ElementOutcome>(out);
        if (!outcome) {
            why = "its code answered something that does not read as an ElementOutcome";
        }
        return outcome;
    }

    std::int64_t calls = 0;

private:
    struct Loaded {
        explicit Loaded(const std::string& path) : share(path) {}
        zengine::op::ImageShare share;
        Code code;
    };
    std::map<std::string, std::unique_ptr<Loaded>> loaded_;
};

} // namespace zengine::graphics::window

#endif // ZENGINE_GRAPHICS_WINDOW_SEAMS_HPP
