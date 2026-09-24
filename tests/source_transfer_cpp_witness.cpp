// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// Program test `source_transfer_cpp` -- THE GENERATED C++, COMPILED AND DELIBERATELY INVOKED.
//
// Each golden here is the exact text the `source_transfer` suite pins as the generator's output:
//
//   source_transfer_ensure_timer.generated.hpp   a saved `EnsureTimer` command dropped on a C++
//                                                document -- admitted by the real `EnsureTimer`
//                                                schema and compared byte for byte with it
//   source_transfer_string_bytes.generated.hpp   a value whose strings hold what a plain literal
//                                                cannot keep: NULs leading, inside and trailing,
//                                                beside the bytes an escape could swallow, and in
//                                                the schema's and a field's name -- compared, schema
//                                                and bytes, with `string_bytes_sample()`
//
// They are included the way a maker would paste them -- after the headers they say they need --
// compiled against the installed Loom package this tree consumes with the tree's own warnings,
// and called. Nothing is sent: generating the code sent nothing, and neither does running it.

#include <zen/gate.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <string>

#include "timer/vocabulary.hpp"

#include "source_transfer_ensure_timer.generated.hpp"
#include "source_transfer_samples.hpp"
#include "source_transfer_string_bytes.generated.hpp"

#include <cstdio>

namespace {

int ensure_timer() {
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

int string_bytes() {
    const loom::Value input = source_transfer_samples::string_bytes_sample();
    const loom::Value generated = make_editor_materials_bytes_v1();
    if (!loom::same_identity(generated.schema(), input.schema())) {
        std::printf("FAIL: the generated schema is not the value's own (name %zu bytes, expected %zu)\n",
                    generated.schema().name().size(), input.schema().name().size());
        return 3;
    }
    const loom::Admission admitted = loom::admit(generated, input.schema());
    if (!admitted) {
        std::printf("FAIL: the generated value does not admit against its own schema: %s\n",
                    admitted.first_error().message().c_str());
        return 4;
    }
    for (std::size_t i = 0; i < input.field_count(); ++i) {
        const loom::Cell* want = input.at(i);
        const loom::Cell* got = generated.at(i);
        if (want != nullptr && want->kind() == loom::Kind::Text &&
            (got == nullptr || got->kind() != loom::Kind::Text || got->as_text() != want->as_text())) {
            std::printf("FAIL: field %zu's text is %zu bytes, expected %zu\n", i,
                        got != nullptr && got->kind() == loom::Kind::Text ? got->as_text().size() : 0,
                        want->as_text().size());
            return 5;
        }
    }
    if (loom::serialize(admitted.value()) != loom::serialize(input)) {
        std::printf("FAIL: the generated value's bytes differ from the value it came from\n");
        return 6;
    }
    std::printf("PASS: make_editor_materials_bytes_v1() is the sample's own schema and bytes, "
                "`inner` %zu bytes, every NUL kept\n",
                generated.get("inner")->as_text().size());
    return 0;
}

} // namespace

int main() {
    if (const int failed = ensure_timer(); failed != 0) {
        return failed;
    }
    return string_bytes();
}
