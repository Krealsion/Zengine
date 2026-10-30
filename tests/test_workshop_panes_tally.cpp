// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite -- the example pane, Tally (`examples/tally-pane/tally.cpp`), through
// the real Workshop: a plan row loads it, it offers its pane, a weaver's Space adds one, its count
// rides a reload in place, and Workshop's `loaded:` words say what the kernel holds it to. Each
// case is a body over the image a plan row names: run on the native build, and under the `box`
// gate on the module built from the same source (`zengine_wasm_weave`), through the same row and
// the same Kernel, so the boxed pane answers as the native one does or a twin goes red.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

#include <filesystem>
#include <string>

namespace {

constexpr const char* kTallyOffice = "example.tally";
constexpr const char* kNativeStem = "zengine-example-tally";
/// The same pane as a module: a stem spelled with WebAssembly's suffix, which the host keeps.
constexpr const char* kBoxedStem = "zengine-example-tally.wasm";

inline PaneRef tally_ref() { return PaneRef{kTallyOffice, "tally"}; }

/// A LIVE WORKSHOP WITH TALLY LOADED BY ONE PLAN ROW naming `stem`, its pane on the desk.
struct TallyRig {
    PaneRig r;
    std::string stem;
    load::Executed done;
    TempDir dir;

    explicit TallyRig(const char* image) : stem(image), dir("tally") {
        r.mount_workshop();
        load::LoadPlan plan;
        load::ArtifactIntent row;
        row.stem = stem;
        row.weave = load::WeaveIntent{kTallyOffice};
        plan.artifacts.push_back(row);
        done = r.run_plan(plan);
        REQUIRE_MESSAGE(done.ok, done.refusal);
        r.ready();
        r.extent(120, 40);
        REQUIRE_MESSAGE(r.session().panes.runtime.find(kTallyOffice, "tally") != nullptr,
                        std::string(stem + " offered no `tally` pane"));
        r.pick(tally_ref());
        REQUIRE(r.session().panes.has(kind()));
    }

    std::int64_t kind() {
        const std::optional<std::int64_t> k = resolve_pane(tally_ref(), r.session().panes);
        REQUIRE(k.has_value());
        return *k;
    }

    std::string text() {
        std::string all;
        for (const std::string& one : pane_rows(r, kind())) {
            all += one + "\n";
        }
        return all;
    }

    /// PRESSED INTO, then Space: the pane's declared action, reached from Workshop by name.
    void add_one() {
        press_pane(r, kind(), 0, 0);
        REQUIRE(r.session().panes.keyboard == kind());
        r.key(input::scan::kSpace);
    }

    /// A COPY OF `image` beside the case, named apart, so a reload opens a file of its own as a
    /// rebuilt product would be: a module keeps its suffix, which is how the kernel is not asked
    /// to tell it apart (it reads the first bytes either way).
    std::string copy_of(const std::string& image, const std::string& name) {
        const std::filesystem::path to = dir.path() / name;
        std::filesystem::copy_file(image, to, std::filesystem::copy_options::overwrite_existing);
        return to.generic_string();
    }

    /// RELOAD THE ROW'S RUNNING WEAVE IN PLACE from `path`, as the kernel's control door does, and
    /// let the reloaded incarnation offer itself and draw again.
    void reload_from(const std::string& path) {
        const loom::WeaveId before = r.kernel.weave_id(stem);
        const loom::ReloadResult reloaded = r.kernel.reload_from(stem, path);
        REQUIRE_MESSAGE(reloaded.ok, reloaded.error);
        CHECK(r.kernel.weave_id(stem) == before);
        r.bus.drain_until_idle();
    }

    const load::ResolvedArtifact& row() {
        REQUIRE(done.resolved.size() == 1);
        return done.resolved[0];
    }
};

void offers_its_pane_and_counts(const char* image) {
    TallyRig t(image);
    // ITS PANE, BY THE NAME IT OFFERED, DRAWING ITS COUNT FROM ZERO.
    CHECK(t.text().find("Tally: 0") != std::string::npos);
    CHECK(t.text().find("press into me") != std::string::npos);
    // EACH SPACE ADDS ONE, AND THE PICTURE SAYS SO.
    t.add_one();
    CHECK_MESSAGE(t.text().find("Tally: 1") != std::string::npos, t.text());
    t.r.key(input::scan::kSpace);
    CHECK_MESSAGE(t.text().find("Tally: 2") != std::string::npos, t.text());
    // ...AND ANOTHER KEY ADDS NOTHING: only the action it declared reaches it.
    t.r.key(input::scan::kA);
    CHECK_MESSAGE(t.text().find("Tally: 2") != std::string::npos, t.text());
}

void keeps_its_count_across_a_reload_in_place(const char* image) {
    TallyRig t(image);
    t.add_one();
    t.r.key(input::scan::kSpace);
    REQUIRE(t.text().find("Tally: 2") != std::string::npos);
    // THE SAME IMAGE, REBUILT: its state rides the reload, and the reloaded pane draws it.
    const std::string rebuilt = t.copy_of(PaneRig::artifact_path(t.stem), "rebuilt-" + t.stem);
    t.reload_from(rebuilt);
    REQUIRE(t.r.session().panes.has(t.kind()));
    CHECK_MESSAGE(t.text().find("Tally: 2") != std::string::npos, t.text());
    // ...AND IT GOES ON COUNTING FROM THERE.
    t.add_one();
    CHECK_MESSAGE(t.text().find("Tally: 3") != std::string::npos, t.text());
}

/// What Workshop's `loaded:` line says of the row, given the kernel's word for it.
void is_said_as_the_kernel_holds_it(const char* image, bool boxed) {
    TallyRig t(image);
    const std::string held = t.r.kernel.containment(t.stem);
    const std::string said = load::loaded_words(t.row(), held);
    CHECK(said.rfind(t.stem + " | weave #", 0) == 0);
    CHECK(said.find(" as example.tally ") != std::string::npos);
    CHECK_MESSAGE(said.find(" | held: " + held) != std::string::npos, said);
    if (boxed) {
        CHECK_MESSAGE(held.rfind("boxed (WebAssembly, in-process, Wasmtime ", 0) == 0, held);
        CHECK(held.find("reach: no filesystem, no network, no clock, no process") !=
              std::string::npos);
    } else {
        CHECK(held == loom::Kernel::containment_note());
    }
}

} // namespace

// ============================================================================
// The host's one rule spells a module's stem as it is
// ============================================================================

TEST_CASE("a stem spelled with WebAssembly's suffix names that file, and any other stem names "
          "this platform's library") {
    const std::string library =
#if defined(_WIN32)
        "/d/zengine-example-tally.dll";
#else
        "/d/zengine-example-tally.so";
#endif
    CHECK(HostContext::so_in("/d", kNativeStem) == library);
    CHECK(HostContext::so_in("/d", kBoxedStem) == "/d/zengine-example-tally.wasm");
    // The suffix is the stem's end, and a stem that is only the suffix names no module.
    CHECK(HostContext::so_in("/d", "a.wasm.b") != "/d/a.wasm.b");
    CHECK(HostContext::so_in("/d", ".wasm") != "/d/.wasm");
}

// ============================================================================
// Tally, native
// ============================================================================

TEST_CASE("Tally loaded by a plan row offers its pane, and each Space a weaver gives it adds one") {
    offers_its_pane_and_counts(kNativeStem);
}

TEST_CASE("Tally keeps its count across a reload in place and goes on counting") {
    keeps_its_count_across_a_reload_in_place(kNativeStem);
}

TEST_CASE("Workshop's loaded words say a native Tally is held as the kernel's note says") {
    is_said_as_the_kernel_holds_it(kNativeStem, false);
}

#ifdef WORKSHOP_WASM_TALLY
// ============================================================================
// Tally, boxed: the same bodies over the module built from the same source
// ============================================================================

TEST_CASE("boxed: Tally loaded by a plan row offers its pane, and each Space a weaver gives it "
          "adds one") {
    offers_its_pane_and_counts(kBoxedStem);
}

TEST_CASE("boxed: Tally keeps its count across a reload in place and goes on counting") {
    keeps_its_count_across_a_reload_in_place(kBoxedStem);
}

TEST_CASE("boxed: Workshop's loaded words say Tally is held in its box") {
    is_said_as_the_kernel_holds_it(kBoxedStem, true);
}

TEST_CASE("boxed: a native Tally reloads into its boxed build and back, its count kept and its "
          "holding said each time") {
    TallyRig t(kNativeStem);
    t.add_one();
    REQUIRE(t.text().find("Tally: 1") != std::string::npos);
    CHECK(t.r.kernel.containment(t.stem) == loom::Kernel::containment_note());

    // INTO THE MODULE: the same office, the same weave, the count carried; the kernel now holds it
    // in a box, and Workshop's words for the row say so.
    t.reload_from(t.copy_of(WORKSHOP_WASM_TALLY, "boxed.wasm"));
    CHECK_MESSAGE(t.text().find("Tally: 1") != std::string::npos, t.text());
    const std::string boxed = t.r.kernel.containment(t.stem);
    CHECK_MESSAGE(boxed.rfind("boxed (WebAssembly", 0) == 0, boxed);
    CHECK(load::loaded_words(t.row(), boxed).find(" | held: boxed (WebAssembly") !=
          std::string::npos);
    t.add_one();
    CHECK_MESSAGE(t.text().find("Tally: 2") != std::string::npos, t.text());

    // ...AND BACK TO A NATIVE BUILD: the count the box kept rides out of it.
    t.reload_from(t.copy_of(WORKSHOP_SO_TALLY, std::string("back-") + kNativeStem));
    CHECK_MESSAGE(t.text().find("Tally: 2") != std::string::npos, t.text());
    CHECK(t.r.kernel.containment(t.stem) == loom::Kernel::containment_note());
}
#endif
