// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// THE AUTHORED LOAD PLAN (LOAD-0) — whether the running arrangement is the
// EXECUTION OF AUTHORED PROJECT INTENT, or knowledge embedded in Workshop's source.
//
// PROV-0 ended the host authoring semantics and left it authoring a LIST: two
// provider mounts and five weave boots, hard-coded, with `zengine-timer` in both
// because it is one artifact participating in two ways. Every case here is about
// that list becoming a file.
//
// THE TIERS:
//
//   1  THE PLAN       what a plan may say and what its own law refuses, in memory.
//   2  THE FILE       the codec: round trip, canonical bytes, and every way a
//                     malformed or forged file is refused visibly.
//   3  THE SHIPPED    the two plan files this repository actually installs beside
//                     the host, read as files.
//   4  EXECUTION      real artifacts, a real Kernel, a real Weave Manager: the three
//                     participation combinations, the order laws, and the offer.
//   5  REFUSAL        a missing artifact, a colliding provider, a stale ABI, a
//                     broken operator handoff, and the rollback each leaves behind.
//   6  AUTHORITY      what is on disk gains nothing; what is declared gains exactly
//                     what it declared and no neighbouring surface.
//   7  RESTART        two fresh executions of one file, and an overlay that survives
//                     between them.
//
// ⚠ EVERY RIG THAT LOADS THE TIMER IS A LOCAL OF ITS CASE, and that discipline is
// load-bearing in THIS binary rather than merely tidy. `test_workshop.cpp`'s own rig
// pumps to EMPTY, and a live Timer service re-arms its own beat inside its own
// handler -- so a Timer that outlived its case would hang the next one that pumped.
// Each rig here owns its own `loom::Switchboard` and its own `loom::Kernel`, both
// destroyed at the closing brace, and every drain in this file is `pump_pending()`
// in bounded turns. Do not hoist a rig to file scope and do not call `bus.pump()`.

#include "doctest.h"

#include "operator/catalog.hpp"
#include "operator/provider_host.hpp"
#include "timer/normalize.hpp"
#include "timer/vocabulary.hpp"
#include "workshop/load_execute.hpp"
#include "workshop/load_persist.hpp"
#include "workshop/load_plan.hpp"

#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

namespace op = zengine::op;
namespace load = zengine::workshop::load;
namespace load_persist = zengine::workshop::load_persist;
namespace tmr = zengine::timer;
using zengine::workshop::Written;

// ---- the artifact directory this suite's host resolves against ----------------
//
// A REAL DIRECTORY WITH REAL ARTIFACTS IN IT, staged once and deliberately holding
// MORE than any plan here names. That is what makes the no-scan claim measurable:
// with a lookup table, "not in the plan" and "not reachable" would be the same
// thing, and the interesting question -- does a valid provider sitting beside the
// host gain anything by being there -- could not be asked at all.
//
// The suffix rule is `HostContext::so`'s, because it is the HOST's rule; a plan
// carries a stem and no plan in this file mentions `.so` or `.dll`.

#if defined(_WIN32)
constexpr const char* kArtifactSuffix = ".dll";
#else
constexpr const char* kArtifactSuffix = ".so";
#endif

struct Stage {
    std::filesystem::path dir;

    Stage() {
        dir = std::filesystem::path(WORKSHOP_LOAD_STAGE_DIR);
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        // THE FOUR ARTIFACTS THIS SUITE STAGES, and one of them is never named by a
        // plan in tier 6 on purpose.
        put("zengine-operators-basic", PROVIDER_BASIC_SO);
        put("zengine-timer", TIMER_SO);
        put("zengine-provider-min", PROVIDER_MIN_SO);
        put("zengine-plain-weave", PLAIN_WEAVE_SO);
        put("zengine-stale-provider", PROVIDER_ABI_SO);
        put("zengine-broken-consumer", BROKEN_CONSUMER_SO);
    }

    void put(const char* stem, const char* from) const {
        std::error_code ec;
        std::filesystem::copy_file(from, dir / (std::string(stem) + kArtifactSuffix),
                                   std::filesystem::copy_options::overwrite_existing, ec);
        REQUIRE_MESSAGE(!ec, "cannot stage ", from, ": ", ec.message());
    }

    std::string so(const std::string& stem) const {
        return (dir / (stem + kArtifactSuffix)).string();
    }
};

const Stage& stage() {
    static const Stage one;
    return one;
}

// ---- what a running Timer STORED, read off a real letter ----------------------
//
// `zen.PrepareShutdown` asks the Timer to describe itself and changes nothing it
// describes, and every entry in the answer carries the delay the normalization
// produced. It is the instrument CAT-0 established, and it is deliberately not
// `host_backed()`: a diagnostic accessor decides nothing, and what a case here needs
// to know is what the running service ACTUALLY SCHEDULED.

struct WitnessState {
    std::int64_t noted = 0;
    ZEN_SHAPE(WitnessState, 1, ZEN_FIELD(noted));
};

struct Heard {
    std::int64_t letters = 0;
    std::vector<tmr::TimerHandoffEntry> entries;
};

class Witness
    : public loom::WeaveBase<Witness, WitnessState,
                             loom::Accept<loom::Bequest, tmr::TimerFired, tmr::TimerReady,
                                          tmr::TimerResolution>,
                             loom::Emit<tmr::StartTimer, loom::PrepareShutdown>> {
public:
    explicit Witness(Heard& heard) : heard_(&heard) {}

    /// HEARD AND DISCARDED, deliberately: a live Timer beats, and a weave that
    /// refused its own service's traffic would be a rig arguing with the thing it
    /// is measuring. What this witness is FOR is the letter below.
    void on(const tmr::TimerFired&, loom::Mail&) {}
    void on(const tmr::TimerReady&, loom::Mail&) {}
    void on(const tmr::TimerResolution&, loom::Mail&) {}

    void on(const loom::Bequest& letter, loom::Mail&) {
        ++heard_->letters;
        for (const loom::Bytes& item : letter.items) {
            if (const std::optional<tmr::TimerHandoff> h =
                    loom::claim_item<tmr::TimerHandoff>(item)) {
                heard_->entries = h->entries;
            }
        }
    }

private:
    Heard* heard_;
};

// ---- a host shaped exactly like `workshop.cpp` --------------------------------
//
// THE MEMBER ORDER IS THE LIFETIME CLAIM and it is the order the production host
// writes: catalog, then the surface over it, then the Kernel -- so destruction, which
// runs in reverse, takes the Kernel and its artifacts down FIRST and unmounts the
// providers LAST. The executor is declared after the Kernel for the same reason it is
// in `main()`.

struct PlanRig {
    loom::Switchboard bus;
    op::Catalog catalog;
    op::OperatorHostSurface operators{catalog};
    loom::Kernel kernel{bus};
    loom::WeaveId control = loom::mount_control(kernel, bus);
    loom::WeaveId manager = loom::mount_manager(control, bus);

    load::BootAnswers answers;
    loom::WeaveId booter;

    Heard heard;
    loom::WeaveId witness;

    /// DECLARED AFTER THE KERNEL, exactly as `main()` declares it.
    load::PlanExecutor executor{bus,     catalog, operators, mount_booter(), manager,
                                answers, [](const std::string& stem) { return stage().so(stem); }};

    PlanRig() {
        loom::Grant reach;
        reach.allow_to_any(tmr::StartTimer::zen_name, tmr::StartTimer::zen_version);
        reach.allow_to_any(loom::PrepareShutdown::zen_name, loom::PrepareShutdown::zen_version);
        witness = loom::mount_granted<Witness>(bus, std::move(reach), heard);
    }

    /// THE HOST WRITES THE GRANT, and this is that act: the plan booter may send
    /// `zen.LoadWeave` to this Manager and nothing else, to nobody else.
    loom::WeaveId mount_booter() {
        loom::Grant operate;
        operate.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
        booter = loom::mount_granted<load::PlanBooter>(bus, std::move(operate), answers);
        return booter;
    }

    void drain(int turns = 8) {
        for (int i = 0; i < turns; ++i) {
            if (bus.pump_pending() == 0) {
                return;
            }
        }
    }

    /// Schedule one timer on a live service and read back the delay it STORED.
    /// `-424242` is a value the rule cannot produce, so a miss cannot pass.
    std::int64_t scheduled_delay(loom::WeaveId service, const char* id, std::int64_t delay_ms,
                                 bool repeat) {
        bus.send_as(witness, service,
                    loom::Message(loom::to_value(tmr::StartTimer{id, delay_ms, repeat}), witness,
                                  witness, 0));
        drain(6);
        const std::int64_t before = heard.letters;
        heard.entries.clear();
        bus.send_as(witness, service,
                    loom::Message(loom::to_value(loom::PrepareShutdown{}), witness, witness, 0));
        for (int i = 0; i < 40 && heard.letters == before; ++i) {
            if (bus.pump_pending() == 0) {
                break;
            }
        }
        REQUIRE(heard.letters > before);
        for (const tmr::TimerHandoffEntry& e : heard.entries) {
            if (e.id == id) {
                return e.delay_ms;
            }
        }
        return -424242;
    }

    /// Which WeaveId the plan gave one stem, or a zero id if it loaded no weave.
    loom::WeaveId weave_of(const load::Executed& done, const char* stem) const {
        for (const load::ResolvedArtifact& a : done.resolved) {
            if (a.stem == stem) {
                return a.weave_loaded ? a.weave : loom::WeaveId{};
            }
        }
        return loom::WeaveId{};
    }
};

// ---- authoring helpers, so a case reads as a plan rather than as a builder ----

load::ArtifactIntent provides(const char* stem,
                              op::MountMode mode = op::MountMode::Ordinary) {
    load::ArtifactIntent a;
    a.stem = stem;
    a.provider = load::ProviderIntent{mode};
    return a;
}

load::ArtifactIntent weaves(const char* stem, const char* role) {
    load::ArtifactIntent a;
    a.stem = stem;
    a.weave = load::WeaveIntent{role};
    return a;
}

load::ArtifactIntent both(const char* stem, const char* role,
                          op::MountMode mode = op::MountMode::Ordinary) {
    load::ArtifactIntent a;
    a.stem = stem;
    a.provider = load::ProviderIntent{mode};
    a.weave = load::WeaveIntent{role};
    return a;
}

load::LoadPlan plan_of(std::vector<load::ArtifactIntent> rows) {
    load::LoadPlan p;
    p.artifacts = std::move(rows);
    return p;
}

/// A path inside the staging directory. It goes through `stage()` so the directory
/// exists whatever order doctest runs the cases in -- a case that wrote into a
/// directory another case happened to create first would be a case that passes
/// because of its neighbours.
std::string stage_file(const char* name) {
    return (stage().dir / name).string();
}

std::string file_text(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE_MESSAGE(in.good(), "cannot read ", path);
    std::ostringstream all;
    all << in.rdbuf();
    return all.str();
}

/// The delay a correct `timer.normalize_delay` makes of (-500, repeating).
constexpr std::int64_t kAuthoredDelay = -500;
constexpr std::int64_t kHonestAnswer = 1;
/// ...and what a `math.max` overlaid as a MIN makes of the same pair.
constexpr std::int64_t kOverlaidAnswer = -500;

} // namespace

// =============================================================================
// 1. THE PLAN — what one may say, and what its own law refuses
// =============================================================================

TEST_CASE("one artifact record may request provider participation alone") {
    const load::LoadPlan p = plan_of({provides("zengine-operators-basic")});
    REQUIRE(load::check_plan(p).accepted);
    CHECK(p.artifacts[0].provider.has_value());
    CHECK_FALSE(p.artifacts[0].weave.has_value());
    CHECK(p.artifacts[0].provider->mode == op::MountMode::Ordinary);
}

TEST_CASE("one artifact record may request weave participation alone") {
    const load::LoadPlan p = plan_of({weaves("zengine-composer", "zengine.composer")});
    REQUIRE(load::check_plan(p).accepted);
    CHECK_FALSE(p.artifacts[0].provider.has_value());
    REQUIRE(p.artifacts[0].weave.has_value());
    CHECK(p.artifacts[0].weave->role == "zengine.composer");
}

TEST_CASE("one artifact record may request BOTH, and that is why Timer appears once") {
    const load::LoadPlan p = plan_of({both("zengine-timer", "zengine.timer")});
    REQUIRE(load::check_plan(p).accepted);
    REQUIRE(p.artifacts.size() == 1);
    CHECK(p.artifacts[0].provider.has_value());
    CHECK(p.artifacts[0].weave.has_value());
}

TEST_CASE("an artifact requesting NEITHER surface is refused, by name") {
    load::ArtifactIntent nothing;
    nothing.stem = "zengine-timer";
    const Written no = load::check_plan(plan_of({nothing}));
    CHECK_FALSE(no.accepted);
    CHECK(no.refusal.find("zengine-timer") != std::string::npos);
    CHECK(no.refusal.find("neither provider nor weave") != std::string::npos);
}

TEST_CASE("a weave declaration with no role is refused, and the artifact is named") {
    const Written no = load::check_plan(plan_of({weaves("zengine-timer", "")}));
    CHECK_FALSE(no.accepted);
    CHECK(no.refusal.find("zengine-timer") != std::string::npos);
    CHECK(no.refusal.find("needs a role") != std::string::npos);
}

TEST_CASE("the same artifact declared TWICE is refused rather than executed twice") {
    const Written no = load::check_plan(
        plan_of({provides("zengine-timer"), weaves("zengine-timer", "zengine.timer")}));
    CHECK_FALSE(no.accepted);
    CHECK(no.refusal.find("declared twice") != std::string::npos);
    // ...and the refusal says what to do instead, because the two-list shape is
    // exactly the mistake a maker carries over from the old host.
    CHECK(no.refusal.find("one record") != std::string::npos);
}

TEST_CASE("an artifact stem may not climb out of the host's artifact directory") {
    for (const char* hostile : {"../../../tmp/evil", "..", "sub/../../etc/x", "a\\b"}) {
        const Written no = load::check_plan(plan_of({provides(hostile)}));
        CHECK_MESSAGE(!no.accepted, "stem `", hostile, "` was accepted");
    }
    // ...and the two rules are said separately, because they are two facts.
    CHECK(load::check_artifact_stem("a/b").refusal.find("path separator") != std::string::npos);
    CHECK(load::check_artifact_stem("a..b").refusal.find("`..`") != std::string::npos);
}

TEST_CASE("a stem or a role that is not a NAME is refused") {
    CHECK_FALSE(load::check_artifact_stem("").accepted);
    CHECK_FALSE(load::check_artifact_stem("has space").accepted);
    CHECK_FALSE(load::check_artifact_stem(std::string(load::kMaxArtifactStemLen + 1, 'x')).accepted);
    CHECK(load::check_artifact_stem(std::string(load::kMaxArtifactStemLen, 'x')).accepted);
    CHECK_FALSE(load::check_weave_role("has space").accepted);
    CHECK_FALSE(load::check_weave_role(std::string(load::kMaxWeaveRoleLen + 1, 'x')).accepted);
    CHECK(load::check_weave_role(std::string(load::kMaxWeaveRoleLen, 'x')).accepted);
}

TEST_CASE("a plan larger than a plan may be is refused before anything is executed") {
    load::LoadPlan big;
    for (std::size_t i = 0; i <= load::kMaxPlanArtifacts; ++i) {
        big.artifacts.push_back(provides(("stem-" + std::to_string(i)).c_str()));
    }
    const Written no = load::check_plan(big);
    CHECK_FALSE(no.accepted);
    CHECK(no.refusal.find(std::to_string(load::kMaxPlanArtifacts)) != std::string::npos);
}

TEST_CASE("an EMPTY plan is legal: what a project must contain is not the format's call") {
    CHECK(load::check_plan(load::LoadPlan{}).accepted);
}

// =============================================================================
// 2. THE FILE — one codec, and every way a bad one is refused
// =============================================================================

TEST_CASE("a plan round-trips through the codec, value for value") {
    const load::LoadPlan p = plan_of({provides("zengine-operators-basic"),
                                      weaves("zengine-skin-tui-classic", "zengine.skin"),
                                      both("zengine-timer", "zengine.timer"),
                                      provides("zengine-provider-min", op::MountMode::Overlay)});
    const std::string text = load_persist::to_text(p);
    const load_persist::LoadedPlan back = load_persist::from_text(text);
    REQUIRE_MESSAGE(back.outcome.accepted, back.outcome.refusal);
    CHECK(back.plan == p);
    // ...and a second write of a loaded plan is BYTE-IDENTICAL to the first, which
    // is what makes a plan file diffable rather than merely parseable.
    CHECK(load_persist::to_text(back.plan) == text);
}

TEST_CASE("the ORDER survives the file, because the order is the plan's meaning") {
    const load::LoadPlan p = plan_of({provides("c"), provides("a"), provides("b")});
    const load_persist::LoadedPlan back = load_persist::from_text(load_persist::to_text(p));
    REQUIRE(back.outcome.accepted);
    REQUIRE(back.plan.artifacts.size() == 3);
    CHECK(back.plan.artifacts[0].stem == "c");
    CHECK(back.plan.artifacts[1].stem == "a");
    CHECK(back.plan.artifacts[2].stem == "b");
}

TEST_CASE("the overlay INTENT is what survives, spelled as a word rather than a number") {
    const std::string text =
        load_persist::to_text(plan_of({provides("p", op::MountMode::Overlay)}));
    CHECK(text.find("\"overlay\"") != std::string::npos);
    CHECK(text.find("\"normal\"") == std::string::npos);
    const load_persist::LoadedPlan back = load_persist::from_text(text);
    REQUIRE(back.outcome.accepted);
    CHECK(back.plan.artifacts[0].provider->mode == op::MountMode::Overlay);
}

TEST_CASE("a HAND-WRITTEN plan needs no content id and tolerates whitespace") {
    // The form a person actually writes: indented, and with no schema hash in it.
    const load_persist::LoadedPlan got = load_persist::from_text(R"({
        "zen": 1, "schema": "WorkshopLoadFile", "version": 1,
        "fields": {
          "format": "zengine-workshop-load-plan",
          "format_version": "1",
          "artifacts": [
            { "artifact": "zengine-timer",
              "provider": [ { "mode": "normal" } ],
              "weave": [ { "role": "zengine.timer" } ] }
          ]
        }
      })");
    REQUIRE_MESSAGE(got.outcome.accepted, got.outcome.refusal);
    REQUIRE(got.plan.artifacts.size() == 1);
    CHECK(got.plan.artifacts[0].provider.has_value());
    CHECK(got.plan.artifacts[0].weave->role == "zengine.timer");
}

TEST_CASE("an unrecognised provider mode names BOTH what was found and what would work") {
    const load_persist::LoadedPlan no = load_persist::from_text(
        R"({"zen":1,"schema":"WorkshopLoadFile","version":1,"fields":{)"
        R"("format":"zengine-workshop-load-plan","format_version":"1","artifacts":[)"
        R"({"artifact":"p","provider":[{"mode":"replace"}],"weave":[]}]}})");
    CHECK_FALSE(no.outcome.accepted);
    CHECK(no.outcome.refusal.find("replace") != std::string::npos);
    CHECK(no.outcome.refusal.find("normal or overlay") != std::string::npos);
}

TEST_CASE("a row declaring one surface TWICE is refused: the wire says list, the law says one") {
    const load_persist::LoadedPlan no = load_persist::from_text(
        R"({"zen":1,"schema":"WorkshopLoadFile","version":1,"fields":{)"
        R"("format":"zengine-workshop-load-plan","format_version":"1","artifacts":[)"
        R"({"artifact":"p","provider":[{"mode":"normal"},{"mode":"overlay"}],"weave":[]}]}})");
    CHECK_FALSE(no.outcome.accepted);
    CHECK(no.outcome.refusal.find("more than once") != std::string::npos);
}

TEST_CASE("a weave row missing its role is refused as a MISSING FIELD by the gate") {
    const load_persist::LoadedPlan no = load_persist::from_text(
        R"({"zen":1,"schema":"WorkshopLoadFile","version":1,"fields":{)"
        R"("format":"zengine-workshop-load-plan","format_version":"1","artifacts":[)"
        R"({"artifact":"p","provider":[],"weave":[{}]}]}})");
    CHECK_FALSE(no.outcome.accepted);
    CHECK(no.outcome.refusal.find("role") != std::string::npos);
}

TEST_CASE("a field the shape does not declare is refused rather than ignored") {
    const load_persist::LoadedPlan no = load_persist::from_text(
        R"({"zen":1,"schema":"WorkshopLoadFile","version":1,"fields":{)"
        R"("format":"zengine-workshop-load-plan","format_version":"1","artifacts":[)"
        R"({"artifact":"p","provider":[],"weave":[{"role":"r"}],"trusted":"yes"}]}})");
    CHECK_FALSE(no.outcome.accepted);
    CHECK(no.outcome.refusal.find("trusted") != std::string::npos);
}

TEST_CASE("a file from another version is refused by ITS NUMBER, before its rows are judged") {
    const load_persist::LoadedPlan no = load_persist::from_text(
        R"({"zen":1,"schema":"WorkshopLoadFile","version":2,"fields":{)"
        R"("format":"zengine-workshop-load-plan","format_version":"2","artifacts":[]}})");
    CHECK_FALSE(no.outcome.accepted);
    CHECK(no.outcome.refusal.find("load plan version 2") != std::string::npos);
    CHECK(no.outcome.refusal.find("reads version 1") != std::string::npos);
}

TEST_CASE("a forged file whose envelope is this version and whose FIELD is not still refuses") {
    const load_persist::LoadedPlan no = load_persist::from_text(
        R"({"zen":1,"schema":"WorkshopLoadFile","version":1,"fields":{)"
        R"("format":"zengine-workshop-load-plan","format_version":"7","artifacts":[]}})");
    CHECK_FALSE(no.outcome.accepted);
    CHECK(no.outcome.refusal.find("load plan version 7") != std::string::npos);
}

TEST_CASE("handing Workshop one of its OTHER two files is named rather than half-read") {
    const load_persist::LoadedPlan no = load_persist::from_text(
        R"({"zen":1,"schema":"WorkshopLoadFile","version":1,"fields":{)"
        R"("format":"zengine-workshop-setup","format_version":"1","artifacts":[]}})");
    CHECK_FALSE(no.outcome.accepted);
    CHECK(no.outcome.refusal.find("zengine-workshop-setup") != std::string::npos);
}

TEST_CASE("bytes that are not a plan at all are refused, and nothing throws") {
    for (const char* junk : {"", "{", "not json", "[]", R"({"zen":1})"}) {
        const load_persist::LoadedPlan no = load_persist::from_text(junk);
        CHECK_MESSAGE(!no.outcome.accepted, "`", junk, "` was accepted as a plan");
        CHECK_FALSE(no.outcome.refusal.empty());
    }
}

TEST_CASE("a plan the LAW refuses is refused by the codec too, in the law's own words") {
    const load_persist::LoadedPlan no = load_persist::from_text(
        R"({"zen":1,"schema":"WorkshopLoadFile","version":1,"fields":{)"
        R"("format":"zengine-workshop-load-plan","format_version":"1","artifacts":[)"
        R"({"artifact":"..evil","provider":[{"mode":"normal"}],"weave":[]}]}})");
    CHECK_FALSE(no.outcome.accepted);
    CHECK(no.outcome.refusal.find("`..`") != std::string::npos);
}

TEST_CASE("a plan file that is not there is refused by the file layer, by path") {
    const load_persist::LoadedPlan no =
        load_persist::load_file(stage_file("nope.json"));
    CHECK_FALSE(no.outcome.accepted);
    CHECK(no.outcome.refusal.find("nope.json") != std::string::npos);
}

TEST_CASE("a plan file saved and loaded again is the same plan") {
    const load::LoadPlan p = plan_of({provides("zengine-operators-basic"),
                                      both("zengine-timer", "zengine.timer")});
    const std::string path =
        stage_file("roundtrip.json");
    REQUIRE(load_persist::save_file(path, p).accepted);
    const load_persist::LoadedPlan back = load_persist::load_file(path);
    REQUIRE_MESSAGE(back.outcome.accepted, back.outcome.refusal);
    CHECK(back.plan == p);
}

// =============================================================================
// 3. THE SHIPPED PLANS — the files this repository installs, read as files
// =============================================================================

TEST_CASE("the shipped default plan is a legal plan, and it is the terminal arrangement") {
    const load_persist::LoadedPlan got = load_persist::from_text(file_text(WORKSHOP_DEFAULT_PLAN));
    REQUIRE_MESSAGE(got.outcome.accepted, got.outcome.refusal);
    const load::LoadPlan& p = got.plan;
    REQUIRE(p.artifacts.size() == 6);

    CHECK(p.artifacts[0].stem == "zengine-operators-basic");
    CHECK(p.artifacts[1].stem == "zengine-skin-tui-classic");
    CHECK(p.artifacts[2].stem == "zengine-input");
    CHECK(p.artifacts[3].stem == "zengine-timer");
    CHECK(p.artifacts[4].stem == "zengine-introspection");
    CHECK(p.artifacts[5].stem == "zengine-composer");

    // THE BASIC PROVIDER PRECEDES THE TIMER, and that is authored list order rather
    // than anything inferred: the Timer's composition names powers the first row
    // supplies, and nothing in this system works that out for itself.
    CHECK(p.artifacts[0].provider.has_value());
    CHECK_FALSE(p.artifacts[0].weave.has_value());
    CHECK(p.artifacts[3].provider.has_value());
    CHECK(p.artifacts[3].weave.has_value());
    CHECK(p.artifacts[3].weave->role == tmr::kTimerRole);
}

TEST_CASE("the Timer appears exactly ONCE in the shipped plan, participating in two ways") {
    const load_persist::LoadedPlan got = load_persist::from_text(file_text(WORKSHOP_DEFAULT_PLAN));
    REQUIRE(got.outcome.accepted);
    int seen = 0;
    for (const load::ArtifactIntent& a : got.plan.artifacts) {
        if (a.stem == "zengine-timer") {
            ++seen;
            CHECK(a.provider.has_value());
            CHECK(a.weave.has_value());
        }
    }
    CHECK(seen == 1);
}

TEST_CASE("the shipped plans carry no operator identity, no suffix and no path") {
    for (const char* path : {WORKSHOP_DEFAULT_PLAN, WORKSHOP_GRAPHICAL_PLAN}) {
        const std::string text = file_text(path);
        // PROVIDERS OWN THE VOCABULARY. A plan that copied an operator identity into
        // the project file would be a host authoring semantics one indirection out.
        for (const char* forbidden : {"math.max", "logic.select_int", "timer.normalize_delay",
                                      // ...and the platform, which the host spells and
                                      // the plan must not, or one plan stops being two
                                      // platforms' plan.
                                      ".so", ".dll", "/", "\\"}) {
            CHECK_MESSAGE(text.find(forbidden) == std::string::npos, path, " names '", forbidden,
                          "'");
        }
    }
}

TEST_CASE("the graphical plan differs from the default in exactly the two medium rows") {
    const load_persist::LoadedPlan tui = load_persist::from_text(file_text(WORKSHOP_DEFAULT_PLAN));
    const load_persist::LoadedPlan sdl =
        load_persist::from_text(file_text(WORKSHOP_GRAPHICAL_PLAN));
    REQUIRE(tui.outcome.accepted);
    REQUIRE_MESSAGE(sdl.outcome.accepted, sdl.outcome.refusal);
    REQUIRE(tui.plan.artifacts.size() == sdl.plan.artifacts.size());
    int different = 0;
    for (std::size_t i = 0; i < tui.plan.artifacts.size(); ++i) {
        if (!(tui.plan.artifacts[i] == sdl.plan.artifacts[i])) {
            ++different;
            // ...and what differs is the STEM. Presentation and input stay two
            // independent rows: neither is deduced from the other.
            CHECK(tui.plan.artifacts[i].weave->role == sdl.plan.artifacts[i].weave->role);
            CHECK(tui.plan.artifacts[i].stem != sdl.plan.artifacts[i].stem);
        }
    }
    CHECK(different == 2);
}

// =============================================================================
// 4. EXECUTION — real artifacts, a real Kernel, a real Weave Manager
// =============================================================================

TEST_CASE("a provider-only record mounts a provider and loads NO weave") {
    PlanRig rig;
    const load::Executed done = rig.executor.run(plan_of({provides("zengine-operators-basic")}));
    REQUIRE_MESSAGE(done.ok, done.refusal);
    REQUIRE(done.resolved.size() == 1);
    CHECK(done.resolved[0].provider_mounted);
    CHECK(done.resolved[0].provider == "zengine.operators.basic");
    CHECK(done.resolved[0].contributed == 2);
    CHECK_FALSE(done.resolved[0].weave_loaded);
    CHECK(rig.catalog.find("math.max") != nullptr);
    // ...AND NO KERNEL WENT LOOKING FOR A WEAVE. PROV-0 proved a provider is not a
    // weave; this proves a plan cannot make one out of it by accident.
    CHECK_FALSE(rig.kernel.is_loaded("zengine-operators-basic"));
}

TEST_CASE("a weave-only record loads a weave and mounts NO provider") {
    PlanRig rig;
    const load::Executed done =
        rig.executor.run(plan_of({weaves("zengine-plain-weave", "test.plain")}));
    REQUIRE_MESSAGE(done.ok, done.refusal);
    REQUIRE(done.resolved.size() == 1);
    CHECK(done.resolved[0].weave_loaded);
    CHECK(done.resolved[0].weave.value != 0);
    CHECK(done.resolved[0].role == "test.plain");
    CHECK_FALSE(done.resolved[0].provider_mounted);
    CHECK(rig.catalog.providers().empty());
    CHECK(rig.catalog.size() == 0);
    // AN ORDINARY WEAVE MEETS THE OFFER AND IS SIMPLY NOT A CONSUMER, which is a
    // normal outcome and not a diagnostic.
    CHECK(done.resolved[0].offer == op::OfferOutcome::NotAConsumer);
}

TEST_CASE("a provider+weave record does BOTH, from ONE row, in one pass") {
    PlanRig rig;
    const load::Executed done = rig.executor.run(
        plan_of({provides("zengine-operators-basic"), both("zengine-timer", tmr::kTimerRole)}));
    REQUIRE_MESSAGE(done.ok, done.refusal);
    REQUIRE(done.resolved.size() == 2);
    const load::ResolvedArtifact& timer = done.resolved[1];
    CHECK(timer.provider_mounted);
    CHECK(timer.provider == "zengine.timer");
    CHECK(timer.contributed == 1);
    CHECK(timer.weave_loaded);
    CHECK(timer.role == tmr::kTimerRole);
    // THE OFFER WAS TAKEN, which is what makes the loaded Timer host-backed.
    CHECK(timer.offer == op::OfferOutcome::Offered);
    CHECK(rig.scheduled_delay(timer.weave, "beat", kAuthoredDelay, true) == kHonestAnswer);
}

TEST_CASE("WITHIN one record the provider is mounted BEFORE the weave is created") {
    // THE ONLY DIFFERENCE BETWEEN THESE TWO PLANS IS THAT THE SECOND ROW ALSO ASKS
    // FOR PROVIDER PARTICIPATION, and that is the whole demonstration: a host-backed
    // Timer VALIDATES `timer.normalize_delay` inside its own constructor, so the
    // contribution has to be in the catalog before `create()` runs.
    {
        PlanRig without;
        const load::Executed no = without.executor.run(plan_of(
            {provides("zengine-operators-basic"), weaves("zengine-timer", tmr::kTimerRole)}));
        CHECK_FALSE(no.ok);
        CHECK(no.refusal.find("zengine-timer") != std::string::npos);
        CHECK(no.refusal.find("weave load refused") != std::string::npos);
    }
    {
        PlanRig with;
        const load::Executed yes = with.executor.run(
            plan_of({provides("zengine-operators-basic"), both("zengine-timer", tmr::kTimerRole)}));
        CHECK_MESSAGE(yes.ok, yes.refusal);
    }
}

TEST_CASE("the offer BRACKETS the load and is withdrawn: a second load is a second handoff") {
    PlanRig rig;
    const load::Executed done = rig.executor.run(
        plan_of({provides("zengine-operators-basic"), both("zengine-timer", tmr::kTimerRole)}));
    REQUIRE_MESSAGE(done.ok, done.refusal);
    // OUTSIDE THE BRACKET THE SLOT IS EMPTY. Nothing here can read the loaded image's
    // module slot from this side; what a case CAN say is that the running instance
    // kept its own copy and still answers through THIS host -- which is the property
    // the withdrawal exists to make safe, and the one a maker depends on.
    CHECK(rig.scheduled_delay(rig.weave_of(done, "zengine-timer"), "a", kAuthoredDelay, true) ==
          kHonestAnswer);
}

TEST_CASE("the executor keeps enough resolved truth to answer for every row") {
    PlanRig rig;
    const load::Executed done =
        rig.executor.run(plan_of({provides("zengine-operators-basic"),
                                  weaves("zengine-plain-weave", "test.plain"),
                                  both("zengine-timer", tmr::kTimerRole)}));
    REQUIRE_MESSAGE(done.ok, done.refusal);
    REQUIRE(done.resolved.size() == 3);
    CHECK(rig.executor.resolved().size() == 3);
    // IN AUTHORED ORDER, which is what a teardown and a report both need.
    CHECK(done.resolved[0].stem == "zengine-operators-basic");
    CHECK(done.resolved[1].stem == "zengine-plain-weave");
    CHECK(done.resolved[2].stem == "zengine-timer");
}

// =============================================================================
// 5. REFUSAL — visible, named by artifact and step, and rolled back
// =============================================================================

TEST_CASE("a missing artifact is refused in the LOADER's own words, naming the artifact") {
    PlanRig rig;
    const load::Executed done =
        rig.executor.run(plan_of({weaves("zengine-not-on-this-disk", "test.absent")}));
    CHECK_FALSE(done.ok);
    CHECK(done.refusal.find("artifact 'zengine-not-on-this-disk'") != std::string::npos);
    CHECK(done.refusal.find("weave load refused") != std::string::npos);
    CHECK(done.resolved.empty());
}

TEST_CASE("a missing PROVIDER artifact is refused by the mount, naming the step") {
    PlanRig rig;
    const load::Executed done =
        rig.executor.run(plan_of({provides("zengine-not-on-this-disk")}));
    CHECK_FALSE(done.ok);
    CHECK(done.refusal.find("provider mount refused") != std::string::npos);
    CHECK(rig.catalog.providers().empty());
}

TEST_CASE("a missing artifact leaves the authored plan exactly as it was written") {
    // UNRESOLVED INTENT REMAINS INTENT. The runtime refuses; nothing edits the plan.
    const load::LoadPlan authored = plan_of({weaves("zengine-not-on-this-disk", "test.absent")});
    const std::string before = load_persist::to_text(authored);
    PlanRig rig;
    const load::Executed done = rig.executor.run(authored);
    REQUIRE_FALSE(done.ok);
    CHECK(load_persist::to_text(authored) == before);
    CHECK(authored.artifacts.size() == 1);
}

TEST_CASE("an ORDINARY provider collision still refuses, and the catalog is untouched") {
    PlanRig rig;
    const load::Executed done = rig.executor.run(
        plan_of({provides("zengine-operators-basic"), provides("zengine-provider-min")}));
    CHECK_FALSE(done.ok);
    CHECK(done.refusal.find("artifact 'zengine-provider-min'") != std::string::npos);
    CHECK(done.refusal.find("needs an explicit overlay") != std::string::npos);
    // The FIRST provider still stands: an artifact is the atomic unit, not the plan.
    CHECK(rig.catalog.mounted("zengine.operators.basic"));
    CHECK_FALSE(rig.catalog.mounted("zengine.min"));
}

TEST_CASE("a provider from another era is still refused on its NUMBER, through the plan") {
    PlanRig rig;
    const load::Executed done = rig.executor.run(plan_of({provides("zengine-stale-provider")}));
    CHECK_FALSE(done.ok);
    CHECK(done.refusal.find("provider mount refused") != std::string::npos);
    CHECK(done.refusal.find("this host speaks v") != std::string::npos);
}

TEST_CASE("a BROKEN operator handoff refuses the artifact rather than downgrading it") {
    // CAT-0's correction, in the executor: an image that DOES export a consumer
    // surface and cannot complete the handoff is NOT the same as an ordinary weave
    // that was never offered anything. Loading it anyway would silently swap this
    // host's semantic authority for whatever the image carries.
    PlanRig rig;
    const load::Executed done =
        rig.executor.run(plan_of({weaves("zengine-broken-consumer", "test.broken")}));
    CHECK_FALSE(done.ok);
    CHECK(done.refusal.find("operator handoff refused") != std::string::npos);
    CHECK(done.refusal.find("this host speaks v") != std::string::npos);
    // ...and it was refused BEFORE the load, so nothing of it is running.
    CHECK_FALSE(rig.kernel.is_loaded("zengine-broken-consumer"));
}

TEST_CASE("provider mount succeeds, weave load fails: THIS RECORD'S mount is rolled back") {
    PlanRig rig;
    // `zengine-timer` is a real provider AND a real weave, asked for under a role the
    // Kernel already gave to somebody else -- so the mount succeeds and the load does
    // not, which is the halfway state artifact-level atomicity exists for.
    const load::Executed done =
        rig.executor.run(plan_of({provides("zengine-operators-basic"),
                                  weaves("zengine-plain-weave", tmr::kTimerRole),
                                  both("zengine-timer", tmr::kTimerRole)}));
    CHECK_FALSE(done.ok);
    CHECK(done.refusal.find("artifact 'zengine-timer'") != std::string::npos);
    CHECK(done.refusal.find("weave load refused") != std::string::npos);

    // THE MOUNT THIS RECORD MADE IS GONE. Nothing of `zengine-timer` is contributing
    // to this host, because the artifact it came from is not participating.
    CHECK_FALSE(rig.catalog.mounted("zengine.timer"));
    CHECK(rig.catalog.find(tmr::kNormalizeDelay) == nullptr);
    // ...and the EARLIER artifacts still stand, which is the honest scope of the
    // promise: one artifact is the atomic unit, and a whole-plan transaction was
    // deliberately not built.
    CHECK(rig.catalog.mounted("zengine.operators.basic"));
    CHECK(done.resolved.size() == 2);
}

TEST_CASE("a provider mount that fails stops the record before its weave is attempted") {
    // One artifact cannot be declared twice in ONE plan -- the plan law refuses that
    // before anything runs -- so the runtime shape of the same collision is a second
    // execution against a host that already holds the mount. What matters is that the
    // record STOPS: a mount that refused must not be followed by the load it was
    // supposed to make possible.
    PlanRig rig;
    REQUIRE(rig.executor.run(plan_of({provides("zengine-operators-basic"),
                                      provides("zengine-timer")}))
                .ok);
    const load::Executed done =
        rig.executor.run(plan_of({both("zengine-timer", tmr::kTimerRole)}));
    CHECK_FALSE(done.ok);
    CHECK(done.refusal.find("provider mount refused") != std::string::npos);
    CHECK(done.refusal.find("already mounted") != std::string::npos);
    CHECK_FALSE(rig.kernel.is_loaded("zengine-timer"));
}

// =============================================================================
// 6. AUTHORITY — presence grants nothing; a declaration grants exactly itself
// =============================================================================

TEST_CASE("FILESYSTEM PRESENCE IS NOT LOAD AUTHORITY: an unlisted valid provider does nothing") {
    // `zengine-provider-min` is a real, valid, mountable provider artifact and it is
    // sitting in the very directory this host resolves stems against. No row names
    // it, so it contributes nothing, is opened by nothing, and is not asked.
    PlanRig rig;
    REQUIRE(std::filesystem::exists(stage().so("zengine-provider-min")));
    const load::Executed done = rig.executor.run(plan_of({provides("zengine-operators-basic")}));
    REQUIRE_MESSAGE(done.ok, done.refusal);
    CHECK(rig.catalog.providers().size() == 1);
    CHECK_FALSE(rig.catalog.mounted("zengine.min"));
    CHECK(done.resolved.size() == 1);
    // ...and the same is true of every other artifact staged beside it.
    CHECK_FALSE(rig.kernel.is_loaded("zengine-plain-weave"));
    CHECK_FALSE(rig.kernel.is_loaded("zengine-timer"));
}

TEST_CASE("a WEAVE-ONLY declaration does not mount the provider that artifact exports") {
    // `zengine-timer` exports all three surfaces from one image. Asked for as a weave
    // and nothing else, it contributes nothing -- because "load this participant" and
    // "let this artifact change the host's semantic world" are different intentions.
    PlanRig rig;
    const load::Executed done = rig.executor.run(plan_of(
        {provides("zengine-operators-basic"), weaves("zengine-timer", tmr::kTimerRole)}));
    // The load itself refuses, because a host-backed Timer needs the very rule its own
    // provider surface would have supplied -- which is the point said twice: nothing
    // mounted it for it.
    CHECK_FALSE(done.ok);
    CHECK_FALSE(rig.catalog.mounted("zengine.timer"));
    CHECK(rig.catalog.find(tmr::kNormalizeDelay) == nullptr);
}

TEST_CASE("a PROVIDER-ONLY declaration does not load the weave that artifact exports") {
    PlanRig rig;
    const load::Executed done = rig.executor.run(
        plan_of({provides("zengine-operators-basic"), provides("zengine-timer")}));
    REQUIRE_MESSAGE(done.ok, done.refusal);
    CHECK(rig.catalog.mounted("zengine.timer"));
    CHECK(rig.catalog.find(tmr::kNormalizeDelay) != nullptr);
    CHECK_FALSE(rig.kernel.is_loaded("zengine-timer"));
    CHECK_FALSE(done.resolved[1].weave_loaded);
    // NO OFFER WAS MADE EITHER, because no weave was being created to offer to.
    CHECK(done.resolved[1].offer == op::OfferOutcome::NotAConsumer);
}

TEST_CASE("provider teardown happens after the consumer weave stops spending it") {
    // The lifetime `main()` states by declaration order, exercised rather than
    // asserted: this scope's destruction runs the Kernel's destructor -- and the
    // loaded Timer's -- while the catalog it evaluates through is still alive, and
    // only then unmounts the providers. Under ASan a reversed order is a
    // use-after-free here.
    {
        PlanRig rig;
        const load::Executed done = rig.executor.run(
            plan_of({provides("zengine-operators-basic"), both("zengine-timer", tmr::kTimerRole)}));
        REQUIRE_MESSAGE(done.ok, done.refusal);
        CHECK(rig.scheduled_delay(rig.weave_of(done, "zengine-timer"), "beat", kAuthoredDelay,
                                  true) == kHonestAnswer);
    }
    CHECK(true); // reached, with nothing dangling on the way out
}

TEST_CASE("unmounting one record's provider drops its contributions and nothing else") {
    PlanRig rig;
    const load::Executed done = rig.executor.run(
        plan_of({provides("zengine-operators-basic"), provides("zengine-timer")}));
    REQUIRE_MESSAGE(done.ok, done.refusal);
    CHECK(rig.catalog.size() == 3);
    CHECK(rig.executor.unmount(done.resolved[1]));
    CHECK_FALSE(rig.catalog.mounted("zengine.timer"));
    CHECK(rig.catalog.find(tmr::kNormalizeDelay) == nullptr);
    CHECK(rig.catalog.find("math.max") != nullptr);
    // A row that mounted nothing has nothing to unmount, and says so.
    CHECK_FALSE(rig.executor.unmount(load::ResolvedArtifact{}));
}

// =============================================================================
// 7. RESTART — the same file, twice, with nothing edited in between
// =============================================================================

TEST_CASE("two fresh executions of ONE file reconstruct the SAME arrangement") {
    const std::string path =
        stage_file("restart.json");
    REQUIRE(load_persist::save_file(
                path, plan_of({provides("zengine-operators-basic"),
                               weaves("zengine-plain-weave", "test.plain"),
                               both("zengine-timer", tmr::kTimerRole)}))
                .accepted);

    // NO C++ CHANGES BETWEEN THE RUNS, and no state carried between them: each block
    // reads the file from disk and builds its whole world from what it says.
    auto run = [&path] {
        const load_persist::LoadedPlan read = load_persist::load_file(path);
        REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
        return read.plan;
    };

    std::int64_t first_answer = 0;
    std::size_t first_powers = 0;
    {
        PlanRig a;
        const load::Executed done = a.executor.run(run());
        REQUIRE_MESSAGE(done.ok, done.refusal);
        first_powers = a.catalog.size();
        first_answer = a.scheduled_delay(a.weave_of(done, "zengine-timer"), "beat",
                                         kAuthoredDelay, true);
        CHECK(a.catalog.providers().size() == 2);
        CHECK(done.resolved.size() == 3);
    }
    {
        PlanRig b;
        const load::Executed done = b.executor.run(run());
        REQUIRE_MESSAGE(done.ok, done.refusal);
        CHECK(b.catalog.size() == first_powers);
        CHECK(b.catalog.providers().size() == 2);
        CHECK(done.resolved.size() == 3);
        CHECK(done.resolved[2].provider_mounted);
        CHECK(done.resolved[2].weave_loaded);
        CHECK(b.scheduled_delay(b.weave_of(done, "zengine-timer"), "beat", kAuthoredDelay, true) ==
              first_answer);
    }
    CHECK(first_answer == kHonestAnswer);
}

TEST_CASE("a PERSISTED overlay row changes what a fresh run's Timer schedules") {
    // The product payoff PROV-0 opened and LOAD-0 made durable: a deliberate semantic
    // substitution is authored project arrangement in a file, not ad hoc runtime test
    // code. `zengine-provider-min` supplies `math.max` as a MIN at the same signature,
    // so `timer.normalize_delay` -- a composition nobody rewrote -- answers differently.
    const std::string overlaid = stage_file("with-overlay.json");
    const std::string baseline = stage_file("without-overlay.json");
    REQUIRE(load_persist::save_file(
                overlaid, plan_of({provides("zengine-operators-basic"),
                                   provides("zengine-provider-min", op::MountMode::Overlay),
                                   both("zengine-timer", tmr::kTimerRole)}))
                .accepted);
    REQUIRE(load_persist::save_file(baseline,
                                    plan_of({provides("zengine-operators-basic"),
                                             both("zengine-timer", tmr::kTimerRole)}))
                .accepted);

    std::int64_t with = 0;
    std::int64_t without = 0;
    {
        PlanRig a;
        const load_persist::LoadedPlan read = load_persist::load_file(overlaid);
        REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
        const load::Executed done = a.executor.run(read.plan);
        REQUIRE_MESSAGE(done.ok, done.refusal);
        CHECK(a.catalog.providers().size() == 3);
        with = a.scheduled_delay(a.weave_of(done, "zengine-timer"), "beat", kAuthoredDelay, true);
    }
    {
        PlanRig b;
        const load_persist::LoadedPlan read = load_persist::load_file(baseline);
        REQUIRE(read.outcome.accepted);
        const load::Executed done = b.executor.run(read.plan);
        REQUIRE_MESSAGE(done.ok, done.refusal);
        CHECK(b.catalog.providers().size() == 2);
        without = b.scheduled_delay(b.weave_of(done, "zengine-timer"), "beat", kAuthoredDelay,
                                    true);
    }
    // THE OVERLAY IS AUTHORED PRECEDENCE, NOT ACCIDENTAL LOAD TIMING: the two runs
    // differ by one row of one file and by nothing else.
    CHECK(with == kOverlaidAnswer);
    CHECK(without == kHonestAnswer);
    CHECK(with != without);
}

TEST_CASE("the SAME overlay artifact without the overlay WORD is refused, not silently applied") {
    // Which is what makes the row above an authored decision rather than a
    // consequence of where it sits in the list.
    PlanRig rig;
    const load::Executed done =
        rig.executor.run(plan_of({provides("zengine-operators-basic"),
                                  provides("zengine-provider-min"),
                                  both("zengine-timer", tmr::kTimerRole)}));
    CHECK_FALSE(done.ok);
    CHECK(done.refusal.find("needs an explicit overlay") != std::string::npos);
}

TEST_CASE("MEASURED: Timer before the basic provider is legal today, and here is why") {
    // ⚠ THE OBVIOUS ORDER WITNESS IS FALSE AGAINST THIS SOURCE, and it is pinned as
    // false rather than dressed up. Putting the Timer's row FIRST works:
    //
    //   the MOUNT needs nothing -- a composition crosses as STRUCTURE (PROV-0), so
    //   `timer.normalize_delay` arrives as a graph whose nodes merely NAME `math.max`;
    //   the CREATE needs only the composite -- the Timer describes
    //   `timer.normalize_delay` across the seam and compares port schemas, which the
    //   Timer's own provider has just supplied;
    //   the SPEND is the only step that needs `math.max`, and by the time anything
    //   spends, the plan has finished and the second row has mounted it.
    //
    // So the inter-artifact order between these two rows is NOT load-bearing today.
    // Writing a case that pretended otherwise would be manufacturing a dependency
    // failure this system does not have.
    PlanRig rig;
    const load::Executed done = rig.executor.run(plan_of(
        {both("zengine-timer", tmr::kTimerRole), provides("zengine-operators-basic")}));
    CHECK_MESSAGE(done.ok, done.refusal);
    CHECK(rig.scheduled_delay(rig.weave_of(done, "zengine-timer"), "beat", kAuthoredDelay, true) ==
          kHonestAnswer);
}

TEST_CASE("INTER-artifact order IS authored policy, and an overlay is where it shows") {
    // THE REAL ORDER-SENSITIVE CONSTRAINT IN THIS SOURCE. An overlay covers what is
    // ALREADY there; a provider mounted Ordinary refuses to cover what is already
    // there. So the same three rows in two orders are two different outcomes, and
    // nothing reorders anything to rescue the wrong one.
    {
        PlanRig right;
        const load::Executed done = right.executor.run(
            plan_of({provides("zengine-operators-basic"),
                     provides("zengine-provider-min", op::MountMode::Overlay),
                     both("zengine-timer", tmr::kTimerRole)}));
        REQUIRE_MESSAGE(done.ok, done.refusal);
        CHECK(right.scheduled_delay(right.weave_of(done, "zengine-timer"), "beat", kAuthoredDelay,
                                    true) == kOverlaidAnswer);
    }
    {
        PlanRig wrong;
        // The overlay first: it covers nothing, installs, and then the ORDINARY mount
        // it was meant to cover collides with it. Refused, by artifact and by step,
        // in the catalog's own words.
        const load::Executed done = wrong.executor.run(
            plan_of({provides("zengine-provider-min", op::MountMode::Overlay),
                     provides("zengine-operators-basic"),
                     both("zengine-timer", tmr::kTimerRole)}));
        CHECK_FALSE(done.ok);
        CHECK(done.refusal.find("artifact 'zengine-operators-basic'") != std::string::npos);
        CHECK(done.refusal.find("provider mount refused") != std::string::npos);
        CHECK(done.refusal.find("needs an explicit overlay") != std::string::npos);
        // ...and the Timer never ran at all, because the plan stopped where it broke.
        CHECK_FALSE(wrong.kernel.is_loaded("zengine-timer"));
    }
    // AUTHORED ORDER IS THE WHOLE V0 DEPENDENCY MODEL: the two plans hold the same
    // three rows and differ only in which one is written first.
}
