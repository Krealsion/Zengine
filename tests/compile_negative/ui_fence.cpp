// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The authored/solved fence, shown firing: a static_assert nobody violates proves only that the
// well-formed case passes, so each refusal is written and the compiler's words are judged
// (VM-WALL-04). One source, three cases by ZENGINE_CN_CASE: 1 spells a resolvable dimension as a
// bare number and 2 caches a solved rectangle on the authored part, both refused; 3 is the
// well-formed control. The diagnostics are the contract: the manifest names each judged substring.

#include "ui/vocabulary.hpp"

#include <cstdint>
#include <string>

#ifndef ZENGINE_CN_CASE
#error "ZENGINE_CN_CASE must be defined (1, 2 or 3)"
#endif

namespace {

#if ZENGINE_CN_CASE == 1

// The collapse this whole package exists to prevent: the weaver authored a share,
// the room made `336` pixels of it, and someone stored the 336 where the share lives.
// It is the easiest mistake in the world to make, because `width` is exactly the
// spelling an authored width wants -- which is why the fence's first half is
// type-aware and not name-based.
struct AuthoredElement {
    std::int64_t id = 0;
    std::string label;
    zengine::ui::Place place;
    std::int64_t width = 0;
    std::int64_t height = 0;
};

static_assert(zengine::ui::authored_only_v<AuthoredElement>,
              "a resolvable dimension must be an authored Size, never a solved number");

#elif ZENGINE_CN_CASE == 2

// The other way to collapse it: keep the authored sizes honestly, and cache the
// solved rectangle on the part beside them. The type check alone passes this one --
// the sizes really are Sizes -- so the name-based half is what has to catch it.
struct AuthoredElement {
    std::int64_t id = 0;
    std::string label;
    zengine::ui::Place place;
    zengine::ui::Size width;
    zengine::ui::Size height;
    std::int64_t w = 0; ///< "just cache what it solved to last time"
    std::int64_t h = 0;
};

static_assert(zengine::ui::authored_only_v<AuthoredElement>,
              "no resolved rectangle may live on an authored element");

#elif ZENGINE_CN_CASE == 3

// The positive control: the same part, authored properly. Without this, "the
// build refused" would be satisfied by a fixture that never compiled for any
// reason at all.
struct AuthoredElement {
    std::int64_t id = 0;
    std::string label;
    zengine::ui::Place place;
    zengine::ui::Size width;
    zengine::ui::Size height;
};

static_assert(zengine::ui::authored_only_v<AuthoredElement>,
              "a well-formed authored element must pass the fence");

#else
#error "ZENGINE_CN_CASE must be 1, 2 or 3"
#endif

} // namespace

int main() {
    AuthoredElement e;
    e.id = 1;
    return static_cast<int>(e.id) - 1;
}
