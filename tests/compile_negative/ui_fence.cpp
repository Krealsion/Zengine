// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The authored/resolved fence, DEMONSTRATED FIRING.
//
// ui/vocabulary.hpp asserts the fence about its own Element, and that assertion
// is real — but a static_assert nobody ever violates proves only that the
// well-formed case passes. Whether the fence would CATCH a violation is a
// different claim, and the only honest way to make it is to write the violation
// and read the compiler's refusal.
//
// (This is the one place W-1 came out ahead of the Loom implementation it did
// not relocate: loom::Widget carries the same kind of fence and nothing in the
// Loom tree ever asks it to fire.)
//
// One source, three cases, selected by ZENGINE_CN_CASE, so the positive control
// and the two refusals share every line except the thing under test — a build
// that broke for an unrelated reason cannot read as evidence of the fence.
//
//   1  a resolvable dimension spelled as a bare number   -> must REFUSE
//   2  a resolved rectangle cached on the authored node  -> must REFUSE
//   3  a well-formed authored element                    -> must BUILD
//
// Registered by tests/CMakeLists.txt through zengine_compile_test(), which
// attaches the diagnostic each case must be judged on. The messages below are
// therefore part of the contract: tests/test_population.txt names the substring
// CTest looks for, so a fence that stopped firing — or started failing for a
// different reason — is a red rather than "the compiler returned non-zero".

#include "ui/vocabulary.hpp"

#include <cstdint>
#include <string>

#ifndef ZENGINE_CN_CASE
#error "ZENGINE_CN_CASE must be defined (1, 2 or 3)"
#endif

namespace {

#if ZENGINE_CN_CASE == 1

// The collapse this whole package exists to prevent: the maker authored `60%`,
// the viewport made `28` of it, and someone stored the 28 where the 60% lives.
// It is the easiest mistake in the world to make, because `width` is exactly the
// spelling an authored width wants — which is why the fence's first half is
// type-aware and not name-based.
struct AuthoredElement {
    std::int64_t id = 0;
    std::string label;
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t width = 0;
    std::int64_t height = 0;
};

static_assert(zengine::ui::authored_only_v<AuthoredElement>,
              "a resolvable dimension must be an authored Extent, never a resolved number");

#elif ZENGINE_CN_CASE == 2

// The other way to collapse it: keep the authored extents honestly, and cache
// the resolved rectangle on the element beside them. The type check alone passes
// this one — the extents really are Extents — so the name-based half is what has
// to catch it.
struct AuthoredElement {
    std::int64_t id = 0;
    std::string label;
    std::int64_t x = 0;
    std::int64_t y = 0;
    zengine::ui::Extent width;
    zengine::ui::Extent height;
    std::int64_t w = 0; ///< "just cache what it resolved to last time"
    std::int64_t h = 0;
};

static_assert(zengine::ui::authored_only_v<AuthoredElement>,
              "no resolved rectangle may live on an authored element");

#elif ZENGINE_CN_CASE == 3

// The positive control: the same shape, authored properly. Without this, "the
// build refused" would be satisfied by a fixture that never compiled for any
// reason at all.
struct AuthoredElement {
    std::int64_t id = 0;
    std::string label;
    std::int64_t x = 0;
    std::int64_t y = 0;
    zengine::ui::Extent width;
    zengine::ui::Extent height;
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
