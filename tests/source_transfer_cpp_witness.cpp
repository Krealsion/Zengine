// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// Program test `source_transfer_cpp` -- THE GENERATED C++, COMPILED AND DELIBERATELY INVOKED.
//
// `source_transfer_ensure_timer.generated.hpp` is the exact text the `source_transfer` suite pins
// as the generator's output for a saved `EnsureTimer` command dropped on a C++ document. Here it is
// included the way a maker would paste it -- after the two Loom headers it says it needs -- compiled
// against the installed Loom package this tree consumes, called once, and its value admitted by the
// real `EnsureTimer` schema and compared byte for byte with the command it was generated from.
// Nothing is sent: generating the code sent nothing, and neither does running it.

#include <zen/gate.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include "timer/vocabulary.hpp"

#include "source_transfer_ensure_timer.generated.hpp"

#include <cstdio>

int main() {
    const loom::Value generated = make_ensure_timer_v1();
    const loom::Admission admitted =
        loom::admit(generated, *loom::schema_of<zengine::timer::EnsureTimer>());
    if (!admitted) {
        std::printf("FAIL: the generated value does not admit as EnsureTimer v1: %s\n",
                    admitted.first_error().message().c_str());
        return 1;
    }
    zengine::timer::EnsureTimer expected;
    expected.id = "editor-materials.beat";
    expected.delay_ms = 250;
    expected.repeat = true;
    expected.preferred = "keep-remaining";
    expected.fallback = "restart";
    if (loom::serialize(admitted.value()) != loom::serialize(loom::to_value(expected))) {
        std::printf("FAIL: the generated value's bytes differ from the command it came from\n");
        return 2;
    }
    const auto back = loom::from_value<zengine::timer::EnsureTimer>(admitted.value());
    std::printf("PASS: make_ensure_timer_v1() is EnsureTimer v1 {id=%s delay_ms=%lld repeat=%d}, "
                "byte-identical to the saved command\n",
                back.id.c_str(), static_cast<long long>(back.delay_ms), back.repeat ? 1 : 0);
    return 0;
}
