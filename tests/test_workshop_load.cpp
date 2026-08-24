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
//   8  PROJECTION     (INTR-1) the same two owners read as a maker-facing ANSWER:
//                     authored intent paired with resolved state, and the live
//                     catalog's contribution stacks -- derived at every ask, kept
//                     nowhere, and crossing a real bus as ordinary values.
//
// ⚠ EVERY RIG THAT LOADS THE TIMER IS A LOCAL OF ITS CASE, and that discipline is
// load-bearing in THIS binary rather than merely tidy. `test_workshop.cpp`'s own rig
// pumps to EMPTY, and a live Timer service re-arms its own beat inside its own
// handler -- so a Timer that outlived its case would hang the next one that pumped.
// Each rig here owns its own `loom::Switchboard` and its own `loom::Kernel`, both
// destroyed at the closing brace, and every drain in this file is `pump_pending()`
// in bounded turns. Do not hoist a rig to file scope and do not call `bus.drain_until_idle()`.

#include "doctest.h"

#include "operator/catalog.hpp"
#include "operator/provider_host.hpp"
#include "timer/normalize.hpp"
#include "timer/vocabulary.hpp"
#include "workshop/arrangement.hpp"
#include "workshop/arrangement_vocabulary.hpp"
#include "workshop/load_execute.hpp"
#include "workshop/load_persist.hpp"
#include "workshop/load_plan.hpp"

#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/serialize.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace op = zengine::op;
namespace workshop = zengine::workshop;
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
        // INTR-1's genericity witness: three powers no projection in this repository
        // names, mounted at run time so a view over the store has to find them itself.
        put("zengine-provider-a", PROVIDER_A_SO);
    }

    /// STAGE `from` UNDER `stem`: make the staged destination hold the CURRENT bytes
    /// of the source artifact, whether or not a previous run already left a file
    /// there. That last clause is the whole contract. `load-plan-stage` lives in the
    /// build tree and nothing deletes it, so the SECOND run of this binary in one
    /// build tree stages over its own previous output -- every call here is a REPEAT,
    /// and a repeat that cannot tolerate its own last result makes a second run mean
    /// something different from the first.
    ///
    /// ⚠ THE PROPERTY IS NOT "calling this twice changes nothing". The artifacts are
    /// rebuilt between runs, so a repeat must carry the NEW bytes across; leaving
    /// whatever is already there would satisfy the word `idempotent` and lose the
    /// meaning. Both halves are pinned in tier 0 below.
    void put(const char* stem, const char* from) const {
        const std::filesystem::path dest = dir / (std::string(stem) + kArtifactSuffix);
        // `put` DELETES its destination, so a source that IS the destination would be
        // destroyed rather than copied. No caller aims one there and none may: the
        // stage is where artifacts land, never where they come from.
        REQUIRE_MESSAGE(std::filesystem::path(from) != dest,
                        "a staged artifact may not be its own source: ", dest.string());
        // REMOVE FIRST, AND UNCONDITIONALLY. `copy_options::overwrite_existing` was
        // supposed to make the repeat ordinary and on MinGW it does not: libstdc++
        // asks whether source and destination are the SAME FILE *before* it consults
        // the option, and it asks with `st_dev`/`st_ino` -- which MinGW's `stat`
        // answers (drive, 0) for every file on the drive. So every destination that
        // already exists compares equal to its own source, the copy is refused
        // `file_exists`, and the PREVIOUS RUN'S bytes stay on disk. Measured: MinGW
        // refuses, MSVC and Linux overwrite. Unconditional rather than branched on the
        // platform, so the one toolchain that needs this is not the only one running it.
        std::error_code ec;
        std::filesystem::remove(dest, ec);
        REQUIRE_MESSAGE(!ec, "cannot clear the staged ", dest.string(), ": ", ec.message());
        // ...and NO `overwrite_existing` here, on purpose. The destination is gone, so
        // a destination that still exists means the removal above did not do what it
        // said -- `file_exists` from this call is that news rather than a shrug.
        std::filesystem::copy_file(from, dest, ec);
        // `from` is spelled as a `std::string` because doctest stringifies a bare
        // `const char*` as its ADDRESS. A staging failure that names a pointer, a
        // reason and no file at all is a diagnostic that has to be re-derived by hand.
        REQUIRE_MESSAGE(!ec, "cannot stage ", std::string(from), " as ", dest.string(), ": ",
                        ec.message());
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

// ---- the asker INTR-1's door answers ------------------------------------------
//
// A PARTICIPANT AND NOT A CALL. The door is a weave in an office and its answers come
// back through Loom's own answer path, so what a case here drives is the same
// conversation the loaded Introspection artifact has -- one office asking another,
// values crossing, nothing shared.
//
// IT CAN SPEAK BOTH WAYS ON PURPOSE. `ask` authors deliberately as an office and
// `ask_personally` does not, because the claim under test is that the DOOR refuses
// anonymous speech; a refusal the bus made unreachable would prove nothing.

struct Nudge {
    ZEN_SHAPE(Nudge, 1);
};

struct AskerState {
    std::int64_t nudges = 0;
    ZEN_SHAPE(AskerState, 1, ZEN_FIELD(nudges));
};

/// What an asker heard back, and whether Loom ATTESTED each answer.
struct Answered {
    std::vector<workshop::ResolvedArrangement> arrangements;
    std::vector<workshop::ResolvedPowers> powers;
    std::vector<bool> attested; ///< `mail.answers_ask()` on each answer, in arrival order
};

class Asker : public loom::WeaveBase<Asker, AskerState,
                                     loom::Accept<Nudge, workshop::ResolvedArrangement,
                                                  workshop::ResolvedPowers>,
                                     loom::Emit<workshop::ArrangementRequested,
                                                workshop::PowersRequested>> {
public:
    explicit Asker(Answered& into) : into_(&into) {}

    void on(const Nudge&, loom::Mail& mail) {
        ++state_.nudges;
        if (next) {
            std::function<void(Asker&, loom::Mail&)> once;
            once.swap(next);
            once(*this, mail);
        }
    }
    void on(const workshop::ResolvedArrangement& a, loom::Mail& mail) {
        into_->arrangements.push_back(a);
        into_->attested.push_back(mail.answers_ask());
    }
    void on(const workshop::ResolvedPowers& p, loom::Mail& mail) {
        into_->powers.push_back(p);
        into_->attested.push_back(mail.answers_ask());
    }

    /// The office this asker holds, so a case can drive both authorship spellings.
    static constexpr const char* kOffice = "zengine.test.asker";

    std::function<void(Asker&, loom::Mail&)> next;

private:
    Answered* into_;
};

struct PlanRig {
    /// DECLARED BEFORE THE BUS, exactly as `workshop.cpp` reads its plan before it
    /// builds one: the arrangement door holds this by reference and the bus owns the
    /// door, so a plan declared after the bus would be destroyed first.
    load::LoadPlan authored;
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

    // ---- INTR-1: the projection, and the door that answers it -----------------

    /// RETAIN the plan and perform it, which is what `main()` does: the authored rows
    /// outlive the run because the projection pairs them with what the run produced.
    load::Executed perform(load::LoadPlan plan) {
        authored = std::move(plan);
        return executor.run(authored);
    }

    /// MOUNT THE HOST'S OBSERVATION DOOR, with the production grant spelled out.
    ///
    /// THE TWO RULES ARE COPIED FROM `workshop.cpp` DELIBERATELY rather than minted
    /// from the Emit set: `emit_default_grant` would give this weave `to_any` for
    /// everything it declares, which is what the host writes anyway here -- but a rig
    /// that derived the grant could not notice the host quietly widening it.
    loom::WeaveId mount_door(std::string plan_path = std::string()) {
        auto door = std::make_unique<workshop::ArrangementDoor>(authored, executor, catalog,
                                                                std::move(plan_path));
        workshop::ArrangementDoor* raw = door.get();
        loom::Grant say;
        say.allow_to_any(workshop::ResolvedArrangement::zen_name,
                         workshop::ResolvedArrangement::zen_version);
        say.allow_to_any(workshop::ResolvedPowers::zen_name,
                         workshop::ResolvedPowers::zen_version);
        door_ = bus.register_weave(std::move(door), std::move(say),
                                   std::string(workshop::kArrangementRole));
        raw->zen_set_self(door_);
        return door_;
    }

    /// MOUNT AN ASKER IN AN OFFICE OF ITS OWN, granted exactly the two questions.
    loom::WeaveId mount_asker() {
        auto seat = std::make_unique<Asker>(projected);
        Asker* raw = seat.get();
        loom::Grant grant;
        grant.allow_to_any(workshop::ArrangementRequested::zen_name,
                           workshop::ArrangementRequested::zen_version);
        grant.allow_to_any(workshop::PowersRequested::zen_name,
                           workshop::PowersRequested::zen_version);
        asker_seat_ = raw;
        asker_ = bus.register_weave(std::move(seat), std::move(grant), std::string(Asker::kOffice));
        raw->zen_set_self(asker_);
        return asker_;
    }

    /// Make the asker perform one sentence INSIDE ITS OWN DELIVERY, which is what
    /// gives `mail.as_role(...)` a real authorship moment for Loom to verify.
    void drive_asker(std::function<void(Asker&, loom::Mail&)> what) {
        REQUIRE(asker_seat_ != nullptr);
        asker_seat_->next = std::move(what);
        (void)bus.send(asker_, loom::Message(loom::to_value(Nudge{}), loom::WeaveId{},
                                             loom::WeaveId{}, 0));
        drain(8);
    }

    /// The two askings, in the two authorship spellings.
    void ask_arrangement() {
        drive_asker([](Asker&, loom::Mail& m) {
            (void)m.as_role(Asker::kOffice)
                .send_to_role(workshop::kArrangementRole, workshop::ArrangementRequested{});
        });
    }
    void ask_powers() {
        drive_asker([](Asker&, loom::Mail& m) {
            (void)m.as_role(Asker::kOffice)
                .send_to_role(workshop::kArrangementRole, workshop::PowersRequested{});
        });
    }
    void ask_powers_personally() {
        drive_asker([](Asker&, loom::Mail& m) {
            (void)m.send_to_role(workshop::kArrangementRole, workshop::PowersRequested{});
        });
    }
    void ask_arrangement_personally() {
        drive_asker([](Asker&, loom::Mail& m) {
            (void)m.send_to_role(workshop::kArrangementRole, workshop::ArrangementRequested{});
        });
    }

    Answered projected;

private:
    loom::WeaveId door_{};
    loom::WeaveId asker_{};
    Asker* asker_seat_ = nullptr;
};

// ---- reading a projected answer ------------------------------------------------

/// One artifact's row of a projected arrangement, or nothing.
const workshop::ArtifactParticipation* row_of(const workshop::ResolvedArrangement& said,
                                              const char* stem) {
    for (const workshop::ArtifactParticipation& a : said.artifacts) {
        if (a.artifact == stem) {
            return &a;
        }
    }
    return nullptr;
}

/// One power's projected stack, or nothing.
const workshop::PowerStack* stack_of(const workshop::ResolvedPowers& said, const char* power) {
    for (const workshop::PowerStack& p : said.powers) {
        if (p.power == power) {
            return &p;
        }
    }
    return nullptr;
}

/// WHO IS CURRENTLY ACTIVE FOR ONE POWER, read the way the shape says to read it: the
/// LAST contribution of the stack. A helper rather than a repeated `back()`, so a case
/// reads as the question it is asking.
std::string active_provider(const workshop::ResolvedPowers& said, const char* power) {
    const workshop::PowerStack* p = stack_of(said, power);
    if (p == nullptr || p->contributions.empty()) {
        return "(unresolved)";
    }
    return p->contributions.back().provider;
}

/// Every provider shadowed UNDER the active one, deepest first.
std::vector<std::string> shadowed_providers(const workshop::ResolvedPowers& said,
                                            const char* power) {
    std::vector<std::string> out;
    const workshop::PowerStack* p = stack_of(said, power);
    if (p == nullptr) {
        return out;
    }
    for (std::size_t i = 0; i + 1 < p->contributions.size(); ++i) {
        out.push_back(p->contributions[i].provider);
    }
    return out;
}

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

/// ...and its opposite, for the tier-0 cases that need a source whose bytes they
/// chose. `trunc` is the point: a witness for "the destination converges on the
/// current source" is worthless if its own source does not.
void write_file(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    REQUIRE_MESSAGE(out.good(), "cannot write ", path);
    out << text;
    out.close();
    REQUIRE_MESSAGE(out.good(), "cannot write ", path);
}

/// The delay a correct `timer.normalize_delay` makes of (-500, repeating).
constexpr std::int64_t kAuthoredDelay = -500;
constexpr std::int64_t kHonestAnswer = 1;
/// ...and what a `math.max` overlaid as a MIN makes of the same pair.
constexpr std::int64_t kOverlaidAnswer = -500;

} // namespace

// =============================================================================
// 0. THE STAGE — the fixture's own operation, because a suite that cannot run
//    twice cannot mean the same thing twice
// =============================================================================
//
// EVERY OTHER TIER IN THIS FILE RESTS ON `Stage`, and until QR-6 nothing asserted
// that it worked. It did not: `load-plan-stage` sits in the build tree, nothing
// deletes it, and a developer who ran this binary a second time in the same tree got
// 41 failed cases out of 683 on MinGW -- from the staging copy, before a single load
// plan was judged. A green that a second run cannot reproduce is not a green about
// the code; it is a green about the state of a directory.
//
// So the fixture's own operation gets cases, and there are two of them because the
// defect has two halves. The first is that the repeat must be ALLOWED. The second is
// that the repeat must MEAN something -- and a repair that merely stopped reporting
// `file_exists` would pass the first and leave the previous run's artifact in place,
// which is the failure that does not announce itself.

TEST_CASE("staging over what a previous run left behind is an ordinary repeat") {
    // One process, two calls, one destination. That is the shape a second run in one
    // build tree has, minus the process boundary the repeat itself does not care about.
    const std::string source = stage_file("qr6-source.bin");
    const std::string bytes = "an artifact's worth of bytes";
    write_file(source, bytes);

    stage().put("qr6-restaged-witness", source.c_str());
    stage().put("qr6-restaged-witness", source.c_str());

    CHECK(file_text(stage().so("qr6-restaged-witness")) == bytes);
}

TEST_CASE("a repeat CONVERGES the destination onto the current source, not the old one") {
    // IDEMPOTENT STAGING IS NOT "twice changes nothing": the artifacts are rebuilt
    // between runs, so the second staging of a stem carries bytes the first one had
    // never seen. What repeats is the REQUEST, and what it converges on is the source
    // as it stands now.
    //
    // v2 is SHORTER than v1 deliberately. A destination written into without being
    // truncated first would end with v1's tail still attached, and two versions of
    // equal length could not tell that apart from a clean overwrite.
    const std::string source = stage_file("qr6-changing-source.bin");
    const std::string v1 = "version one, and deliberately the longer of the two";
    const std::string v2 = "version two";

    write_file(source, v1);
    stage().put("qr6-converging-witness", source.c_str());
    REQUIRE(file_text(stage().so("qr6-converging-witness")) == v1);

    write_file(source, v2);
    stage().put("qr6-converging-witness", source.c_str());
    CHECK(file_text(stage().so("qr6-converging-witness")) == v2);
}

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

// =============================================================================
// 8. PROJECTION — the same two owners, read as an answer (INTR-1)
// =============================================================================
//
// EVERY CASE BELOW ASKS THE LIVING STORES. Nothing here builds a fixture arrangement
// or a fixture catalog: a plan is performed over real artifacts, and what the
// projection says is compared against what the executor and the catalog actually
// hold. A projection that agreed with a copy of its subject would prove nothing
// about the subject.

TEST_CASE("INTR-1: the projection pairs AUTHORED intent with RESOLVED state, row by row") {
    PlanRig rig;
    const load::Executed done =
        rig.perform(plan_of({provides("zengine-operators-basic"),
                             both("zengine-timer", tmr::kTimerRole),
                             weaves("zengine-plain-weave", "test.plain")}));
    REQUIRE_MESSAGE(done.ok, done.refusal);

    const workshop::ResolvedArrangement said =
        workshop::describe_arrangement(rig.authored, rig.executor.resolved(), "a-plan.json");
    // ONE ROW PER AUTHORED ROW, IN AUTHORED ORDER. The plan is what is walked, so the
    // order a person wrote is the order a maker reads -- which matters because
    // inter-artifact order is authored policy and an overlay has to sit after what it
    // covers.
    REQUIRE(said.artifacts.size() == 3);
    CHECK(said.artifacts[0].artifact == "zengine-operators-basic");
    CHECK(said.artifacts[1].artifact == "zengine-timer");
    CHECK(said.artifacts[2].artifact == "zengine-plain-weave");
    CHECK(said.plan == "a-plan.json");

    // ---- what a person wrote, and what this run made of it --------------------
    const workshop::ArtifactParticipation* basic = row_of(said, "zengine-operators-basic");
    REQUIRE(basic != nullptr);
    CHECK(basic->authored_provider == std::string(load_persist::kModeNormal));
    CHECK(basic->authored_role.empty());
    CHECK(basic->performed);
    CHECK(basic->provider == "zengine.operators.basic");
    CHECK(basic->powers == 2);

    const workshop::ArtifactParticipation* timer = row_of(said, "zengine-timer");
    REQUIRE(timer != nullptr);
    CHECK(timer->authored_provider == std::string(load_persist::kModeNormal));
    CHECK(timer->authored_role == std::string(tmr::kTimerRole));
    CHECK(timer->provider == "zengine.timer");
    CHECK(timer->powers == 1);
    CHECK(timer->offer == std::string(workshop::kOfferedToken));

    // ---- AND THE TWO HALVES ARE DIFFERENT STRINGS, WHICH IS THE POINT ----------
    //
    // The stem is `zengine-timer`, the authored role is `zengine.timer`, and the
    // resolved provider identity is `zengine.timer` -- three names for one artifact,
    // two of which happen to read alike. A projection that carried them in one field,
    // or in two fields not named for which kind each is, would have made the coincidence
    // invisible.
    CHECK(timer->artifact != timer->provider);
    CHECK(timer->weave != 0);
    CHECK(std::to_string(timer->weave) != timer->authored_role);
}

TEST_CASE("INTR-1: the WeaveId is this run's and the role is the file's") {
    // THE DISTINCTION REQUIREMENT 10 ASKS FOR, MEASURED RATHER THAN ASSERTED: two
    // fresh runs of one authored plan produce the same authored role and, because each
    // mints its own, WeaveIds that are facts about a process.
    load::LoadPlan one = plan_of({provides("zengine-operators-basic"),
                                  both("zengine-timer", tmr::kTimerRole)});
    std::string role_a;
    std::string role_b;
    std::int64_t weave_a = 0;
    std::int64_t weave_b = 0;
    {
        PlanRig rig;
        REQUIRE(rig.perform(one).ok);
        const workshop::ResolvedArrangement said =
            workshop::describe_arrangement(rig.authored, rig.executor.resolved(), "");
        role_a = row_of(said, "zengine-timer")->authored_role;
        weave_a = row_of(said, "zengine-timer")->weave;
    }
    {
        PlanRig rig;
        REQUIRE(rig.perform(one).ok);
        const workshop::ResolvedArrangement said =
            workshop::describe_arrangement(rig.authored, rig.executor.resolved(), "");
        role_b = row_of(said, "zengine-timer")->authored_role;
        weave_b = row_of(said, "zengine-timer")->weave;
    }
    // THE AUTHORED HALF IS THE SAME BECAUSE IT CAME FROM THE SAME FILE.
    CHECK(role_a == role_b);
    CHECK(role_a == std::string(tmr::kTimerRole));
    // ...and both runs minted a real id. Whether the two numbers agree is a Kernel's
    // business and is deliberately NOT asserted: a case that pinned it would be pinning
    // an allocator, and the claim here is only that a WeaveId is not durable intent.
    CHECK(weave_a != 0);
    CHECK(weave_b != 0);
    CHECK(std::to_string(weave_a) != role_a);
}

TEST_CASE("INTR-1: the Timer is ONE row whose provider and weave are two fields of it") {
    PlanRig rig;
    REQUIRE(rig.perform(plan_of({provides("zengine-operators-basic"),
                                 both("zengine-timer", tmr::kTimerRole)}))
                .ok);
    const workshop::ResolvedArrangement said =
        workshop::describe_arrangement(rig.authored, rig.executor.resolved(), "");

    // LOAD-0'S CENTRAL RESULT, CARRIED INTO OBSERVATION. Counted rather than found,
    // because "appears once" is a statement about the whole list.
    std::size_t rows = 0;
    for (const workshop::ArtifactParticipation& a : said.artifacts) {
        rows += a.artifact == "zengine-timer" ? 1u : 0u;
    }
    CHECK(rows == 1);

    const workshop::ArtifactParticipation* timer = row_of(said, "zengine-timer");
    REQUIRE(timer != nullptr);
    // ONE ROW, TWO PARTICIPATIONS, BOTH REAL.
    CHECK_FALSE(timer->provider.empty());
    CHECK(timer->weave != 0);
    CHECK_FALSE(timer->authored_provider.empty());
    CHECK_FALSE(timer->authored_role.empty());
    // AND THE KERNEL AGREES ABOUT THE HALF THAT IS ITS OWN.
    CHECK(rig.kernel.is_loaded("zengine-timer"));
}

TEST_CASE("INTR-1: a provider-only artifact is visible, and never wears a weave") {
    PlanRig rig;
    REQUIRE(rig.perform(plan_of({provides("zengine-operators-basic")})).ok);
    const workshop::ResolvedArrangement said =
        workshop::describe_arrangement(rig.authored, rig.executor.resolved(), "");
    const workshop::ArtifactParticipation* basic = row_of(said, "zengine-operators-basic");
    REQUIRE(basic != nullptr);

    // `provider != weave`, ALL THE WAY INTO PRESENTATION. No WeaveId is fabricated, no
    // role is invented, and the Kernel has never heard of this file.
    CHECK(basic->weave == 0);
    CHECK(basic->authored_role.empty());
    CHECK_FALSE(rig.kernel.is_loaded("zengine-operators-basic"));

    // ...AND NO OFFER OUTCOME IS REPORTED, WHICH IS THE TRAP THIS CASE EXISTS FOR.
    // `load::ResolvedArtifact::offer` is `NotAConsumer` here because that is the FIELD'S
    // DEFAULT and no offer was ever made -- so a projection that copied the enum
    // straight through would publish a default as an observation, and a maker would
    // read a handoff outcome about an artifact no Kernel constructed anything from.
    REQUIRE(rig.executor.resolved().size() == 1);
    CHECK(rig.executor.resolved()[0].offer == op::OfferOutcome::NotAConsumer);
    CHECK(basic->offer.empty());
}

TEST_CASE("INTR-1: a weave-only artifact is visible, and never wears a provider") {
    PlanRig rig;
    REQUIRE(rig.perform(plan_of({weaves("zengine-plain-weave", "test.plain")})).ok);
    const workshop::ResolvedArrangement said =
        workshop::describe_arrangement(rig.authored, rig.executor.resolved(), "");
    const workshop::ArtifactParticipation* plain = row_of(said, "zengine-plain-weave");
    REQUIRE(plain != nullptr);

    // NOTHING IS INFERRED FROM WHAT THE IMAGE COULD HAVE EXPORTED. The projection says
    // what the plan requested and what actually resolved, and this row requested one
    // surface.
    CHECK(plain->authored_provider.empty());
    CHECK(plain->provider.empty());
    CHECK(plain->powers == 0);
    CHECK(plain->authored_role == "test.plain");
    CHECK(plain->weave != 0);
    // AN ORDINARY WEAVE MET THE OFFER PATH AND IS SIMPLY NOT A CONSUMER, which is not
    // a diagnostic and is said as the ordinary thing it is.
    CHECK(plain->offer == std::string(workshop::kNotAConsumerToken));
    CHECK_FALSE(rig.catalog.mounted("zengine.plain"));
}

TEST_CASE("INTR-1: the authored MODE exists ONLY in the plan, and is read from there") {
    // TWO RUNS THAT DIFFER IN ONE AUTHORED WORD. The overlay row resolves to exactly
    // the same provider identity, the same contribution count and the same everything
    // else -- so the ONLY field that can carry the difference is the authored one, and
    // `load::ResolvedArtifact` has no mode field for a projection to have read instead.
    workshop::ArtifactParticipation ordinary;
    workshop::ArtifactParticipation overlaid;
    {
        PlanRig rig;
        REQUIRE(rig.perform(plan_of({provides("zengine-provider-min")})).ok);
        ordinary = *row_of(
            workshop::describe_arrangement(rig.authored, rig.executor.resolved(), ""),
            "zengine-provider-min");
    }
    {
        PlanRig rig;
        REQUIRE(rig.perform(plan_of({provides("zengine-operators-basic"),
                                     provides("zengine-provider-min", op::MountMode::Overlay)}))
                    .ok);
        overlaid = *row_of(
            workshop::describe_arrangement(rig.authored, rig.executor.resolved(), ""),
            "zengine-provider-min");
    }
    CHECK(ordinary.authored_provider == std::string(load_persist::kModeNormal));
    CHECK(overlaid.authored_provider == std::string(load_persist::kModeOverlay));
    // ...and every resolved field is identical, which is what makes the sentence above
    // a measurement rather than a description.
    CHECK(ordinary.provider == overlaid.provider);
    CHECK(ordinary.powers == overlaid.powers);
    CHECK(ordinary.performed == overlaid.performed);
    CHECK(ordinary.offer == overlaid.offer);
    // AND THE WORD IS THE FILE'S OWN. The projection spells an authored mode with
    // `load_persist::mode_word` -- the very function that writes the plan file -- so a
    // maker who wrote `overlay` reads `overlay`, and the two spellings cannot drift
    // because there is only one.
    CHECK(load_persist::to_text(plan_of({provides("zengine-provider-min",
                                                  op::MountMode::Overlay)}))
              .find("\"overlay\"") != std::string::npos);
}

TEST_CASE("INTR-1: an authored artifact the run never reached keeps its intent, marked") {
    // NOT A PRODUCTION STATE -- a refused plan exits the host before a pane exists
    // (INTR-1 §19). What it measures is that the projection walks the AUTHORED list, so
    // a partial arrangement cannot read as a complete one.
    PlanRig rig;
    const load::Executed done = rig.perform(plan_of({provides("zengine-operators-basic"),
                                                     weaves("zengine-not-here", "test.ghost"),
                                                     weaves("zengine-plain-weave", "test.plain")}));
    REQUIRE_FALSE(done.ok);

    const workshop::ResolvedArrangement said =
        workshop::describe_arrangement(rig.authored, rig.executor.resolved(), "");
    REQUIRE(said.artifacts.size() == 3);
    CHECK(said.artifacts[0].performed);
    CHECK_FALSE(said.artifacts[1].performed);
    CHECK_FALSE(said.artifacts[2].performed);
    // THE INTENT SURVIVES A ROW THAT NEVER RAN, which is what the authored half is for.
    CHECK(said.artifacts[1].authored_role == "test.ghost");
    CHECK(said.artifacts[2].authored_role == "test.plain");
    // ...and nothing resolved is invented for either.
    CHECK(said.artifacts[1].weave == 0);
    CHECK(said.artifacts[2].provider.empty());
}

// ---- the powers, and the store they come out of ------------------------------

TEST_CASE("INTR-1: powers are derived from the LIVE catalog, and every stack is whole") {
    PlanRig rig;
    REQUIRE(rig.perform(plan_of({provides("zengine-operators-basic"),
                                 both("zengine-timer", tmr::kTimerRole)}))
                .ok);
    const workshop::ResolvedPowers said = workshop::describe_powers(rig.catalog);

    // EVERY IDENTITY THE CATALOG RESOLVES, AND NOTHING ELSE. Compared against the
    // catalog's own answer rather than against a written list, so a provider that grew
    // a power tomorrow moves both sides together.
    std::vector<std::string> projected;
    for (const workshop::PowerStack& p : said.powers) {
        projected.push_back(p.power);
    }
    CHECK(projected == rig.catalog.identities());
    CHECK(said.providers == rig.catalog.providers());

    // AND THE ACTIVE CONTRIBUTION IS THE ONE `find` ANSWERS, identity by identity --
    // the one-store claim, checked against the very call an evaluation makes.
    for (const workshop::PowerStack& p : said.powers) {
        REQUIRE_FALSE(p.contributions.empty());
        const op::OperatorDef* active = rig.catalog.find(p.power);
        REQUIRE(active != nullptr);
        CHECK(p.contributions.back().composite == active->is_composite());
        CHECK(p.contributions.size() == rig.catalog.contributions(p.power).size());
    }

    // NATIVE AND COMPOSITE ARE THE DEFINITION'S OWN ANSWER, not a classifier: the
    // Timer's delay rule is composed from two primitives and says so.
    CHECK(active_provider(said, "math.max") == "zengine.operators.basic");
    CHECK(stack_of(said, "math.max")->contributions.back().composite == false);
    CHECK(active_provider(said, tmr::kNormalizeDelay) == "zengine.timer");
    CHECK(stack_of(said, tmr::kNormalizeDelay)->contributions.back().composite == true);
}

TEST_CASE("INTR-1: THE OVERLAY WITNESS -- baseline, covered, and revealed again") {
    PlanRig rig;
    const load::Executed done = rig.perform(plan_of({provides("zengine-operators-basic"),
                                                     both("zengine-timer", tmr::kTimerRole)}));
    REQUIRE_MESSAGE(done.ok, done.refusal);
    const loom::WeaveId timer = rig.weave_of(done, "zengine-timer");

    // ---- BASELINE -------------------------------------------------------------
    {
        const workshop::ResolvedPowers said = workshop::describe_powers(rig.catalog);
        CHECK(active_provider(said, "math.max") == "zengine.operators.basic");
        CHECK(shadowed_providers(said, "math.max").empty());
    }
    // ...and what the RUNNING Timer computes agrees, which is what makes the projection
    // a fact about the system rather than about a data structure.
    CHECK(rig.scheduled_delay(timer, "beat", kAuthoredDelay, true) == kHonestAnswer);

    // ---- THE OVERLAY, MOUNTED AT RUN TIME AND NOT AUTHORED ANYWHERE ------------
    const op::MountResult covered = op::mount_provider(
        rig.catalog, stage().so("zengine-provider-min"), op::MountMode::Overlay);
    REQUIRE_MESSAGE(covered.ok, covered.reason);
    {
        const workshop::ResolvedPowers said = workshop::describe_powers(rig.catalog);
        CHECK(active_provider(said, "math.max") == "zengine.operators.test.min");
        REQUIRE(shadowed_providers(said, "math.max").size() == 1);
        CHECK(shadowed_providers(said, "math.max")[0] == "zengine.operators.basic");
        // THE COMPOSITE OVER IT MOVED WITHOUT BEING TOUCHED. Its own contribution is
        // unchanged and unshadowed; what changed is a leaf it names.
        CHECK(active_provider(said, tmr::kNormalizeDelay) == "zengine.timer");
        CHECK(shadowed_providers(said, tmr::kNormalizeDelay).empty());
    }
    // AND THE TIMER, NEITHER REBUILT NOR TOLD, NOW SCHEDULES THE OTHER ANSWER.
    CHECK(rig.scheduled_delay(timer, "beat2", kAuthoredDelay, true) == kOverlaidAnswer);

    // ---- UNMOUNTED: THE ONE UNDERNEATH IS REVEALED, NOT REBUILT ----------------
    const op::OperatorDef* before = rig.catalog.find("math.max");
    REQUIRE(rig.catalog.unmount("zengine.operators.test.min"));
    {
        const workshop::ResolvedPowers said = workshop::describe_powers(rig.catalog);
        CHECK(active_provider(said, "math.max") == "zengine.operators.basic");
        CHECK(shadowed_providers(said, "math.max").empty());
    }
    CHECK(rig.catalog.find("math.max") != before);
    CHECK(rig.scheduled_delay(timer, "beat3", kAuthoredDelay, true) == kHonestAnswer);
}

TEST_CASE("INTR-1: a provider nobody wrote into the projection appears anyway") {
    // THE GENERICITY WITNESS. `zengine-provider-a` supplies three identities this
    // repository's panes and projections have never heard of, two of them composite.
    // Nothing in `workshop/arrangement.hpp` or `introspection/resolved.hpp` names any
    // of them, and there is no branch for a provider that is not one of the shipped
    // two -- so appearing is what a projection over the store DOES.
    PlanRig rig;
    REQUIRE(rig.perform(plan_of({provides("zengine-operators-basic")})).ok);
    CHECK(stack_of(workshop::describe_powers(rig.catalog), "prov.function.1") == nullptr);

    const op::MountResult added =
        op::mount_provider(rig.catalog, stage().so("zengine-provider-a"), op::MountMode::Ordinary);
    REQUIRE_MESSAGE(added.ok, added.reason);

    const workshop::ResolvedPowers said = workshop::describe_powers(rig.catalog);
    for (const char* power : {"prov.function.1", "prov.function.2", "prov.function.3"}) {
        REQUIRE_MESSAGE(stack_of(said, power) != nullptr, power);
        CHECK(active_provider(said, power) == "zengine.provider.a");
    }
    // ...and the leaf is native while the two over it are composed, straight off the
    // definitions and with nothing here classifying anything.
    CHECK(stack_of(said, "prov.function.3")->contributions.back().composite == false);
    CHECK(stack_of(said, "prov.function.2")->contributions.back().composite == true);
    CHECK(stack_of(said, "prov.function.1")->contributions.back().composite == true);
    // The shipped powers are untouched beside them.
    CHECK(active_provider(said, "math.max") == "zengine.operators.basic");
}

TEST_CASE("INTR-1: the host published nothing, so no contribution claims the host") {
    // PROV-0's law read through the projection: `workshop.cpp` authors no operator, so
    // the empty `provider` -- which is `op::Contribution`'s word for "the host itself
    // published this" -- appears nowhere in a Workshop-shaped arrangement.
    PlanRig rig;
    REQUIRE(rig.perform(plan_of({provides("zengine-operators-basic"),
                                 both("zengine-timer", tmr::kTimerRole)}))
                .ok);
    const workshop::ResolvedPowers said = workshop::describe_powers(rig.catalog);
    REQUIRE_FALSE(said.powers.empty());
    for (const workshop::PowerStack& p : said.powers) {
        for (const workshop::PowerContribution& c : p.contributions) {
            CHECK_FALSE(c.provider.empty());
        }
    }
}

// ---- the door: who may ask, what crosses, and what it keeps -------------------

TEST_CASE("INTR-1: the door answers an OFFICE, and answers anonymous speech nothing") {
    PlanRig rig;
    REQUIRE(rig.perform(plan_of({provides("zengine-operators-basic")})).ok);
    rig.mount_door("plan.json");
    rig.mount_asker();

    // PERSONAL SPEECH FROM A WEAVE THAT HOLDS AN OFFICE. Holding is not speaking-for,
    // and the door has nobody nameable to be answerable to.
    rig.ask_powers_personally();
    rig.ask_arrangement_personally();
    CHECK(rig.projected.powers.empty());
    CHECK(rig.projected.arrangements.empty());

    // ...and the deliberately authored ones are answered, so the refusals above are
    // about AUTHORSHIP and not about the door having stopped talking.
    rig.ask_powers();
    rig.ask_arrangement();
    REQUIRE(rig.projected.powers.size() == 1);
    REQUIRE(rig.projected.arrangements.size() == 1);
    CHECK(rig.projected.arrangements[0].plan == "plan.json");

    // EVERY ANSWER CAME BACK THROUGH LOOM'S ANSWER DOOR, attested. That is the bound an
    // asker cannot forge and a stranger cannot supply, and it is why the loaded tool
    // checks `answers_ask()` before it projects a row.
    REQUIRE(rig.projected.attested.size() == 2);
    for (const bool attested : rig.projected.attested) {
        CHECK(attested);
    }
}

TEST_CASE("INTR-1: the door speaks ONLY when asked -- there is no beat in it") {
    PlanRig rig;
    REQUIRE(rig.perform(plan_of({provides("zengine-operators-basic")})).ok);
    const loom::WeaveId door = rig.mount_door();
    rig.mount_asker();

    std::vector<std::string> said;
    const loom::ObserverId tap = rig.bus.add_observer([&](const loom::BusEvent& e) {
        if (e.sender == door && !e.schema_name.empty()) {
            said.push_back(e.schema_name);
        }
    });
    // A LONG QUIET, MEASURED FROM THE BUS. Nothing is asked and the bus is pumped
    // until it empties, many times over: a door with a timer, a poll or a repaint
    // would have to say something here.
    rig.drain(64);
    CHECK(said.empty());

    rig.ask_powers();
    rig.ask_arrangement();
    rig.bus.remove_observer(tap);

    // TWO ASKS, TWO ANSWERS, AND NOTHING ELSE IN THE WHOLE VOCABULARY. The negative
    // half is the interesting one: knowing what every provider supplies did not come
    // with a way to touch any of it.
    std::vector<std::string> distinct = said;
    std::sort(distinct.begin(), distinct.end());
    distinct.erase(std::unique(distinct.begin(), distinct.end()), distinct.end());
    const std::vector<std::string> allowed{"ResolvedArrangement", "ResolvedPowers"};
    CHECK(distinct == allowed);
    CHECK(said.size() == 2);
}

TEST_CASE("INTR-1: the door keeps nothing, so a change between two asks is in the second") {
    PlanRig rig;
    REQUIRE(rig.perform(plan_of({provides("zengine-operators-basic"),
                                 both("zengine-timer", tmr::kTimerRole)}))
                .ok);
    rig.mount_door();
    rig.mount_asker();

    rig.ask_powers();
    REQUIRE(rig.projected.powers.size() == 1);
    CHECK(active_provider(rig.projected.powers[0], "math.max") == "zengine.operators.basic");

    // NOTHING NOTIFIES ANYBODY. The overlay is mounted straight into the host's own
    // catalog, the door is not told, no event exists to tell it, and nothing polls.
    const op::MountResult covered = op::mount_provider(
        rig.catalog, stage().so("zengine-provider-min"), op::MountMode::Overlay);
    REQUIRE_MESSAGE(covered.ok, covered.reason);
    CHECK(rig.projected.powers.size() == 1); // no answer arrived on its own

    rig.ask_powers();
    REQUIRE(rig.projected.powers.size() == 2);
    CHECK(active_provider(rig.projected.powers[1], "math.max") == "zengine.operators.test.min");
    REQUIRE(shadowed_providers(rig.projected.powers[1], "math.max").size() == 1);
    CHECK(shadowed_providers(rig.projected.powers[1], "math.max")[0] == "zengine.operators.basic");
    // THE FIRST ANSWER IS UNMOVED, because it was a value the asker already holds and
    // not a window onto anything.
    CHECK(active_provider(rig.projected.powers[0], "math.max") == "zengine.operators.basic");
}

TEST_CASE("INTR-1: what crosses is a VALUE -- it survives bytes and holds no address") {
    PlanRig rig;
    REQUIRE(rig.perform(plan_of({provides("zengine-operators-basic"),
                                 both("zengine-timer", tmr::kTimerRole)}))
                .ok);
    const workshop::ResolvedArrangement arrangement =
        workshop::describe_arrangement(rig.authored, rig.executor.resolved(), "p.json");
    const workshop::ResolvedPowers powers = workshop::describe_powers(rig.catalog);

    // THE ROUND TRIP A STRANGER WOULD PERFORM. In-process Loom hands a value across
    // without serializing, so this is the claim made deliberately rather than
    // incidentally: everything the door says is expressible as bytes and admittable at
    // the reader's own schema, which is exactly what a `Catalog*` or a
    // `std::vector<ResolvedArtifact>&` would not be.
    {
        const loom::Unverified u = loom::parse(loom::serialize(loom::to_value(arrangement)));
        const loom::Admission ok =
            loom::admit(u, loom::schema_of<workshop::ResolvedArrangement>());
        REQUIRE_MESSAGE(ok.ok(), "arrangement did not admit");
        const workshop::ResolvedArrangement back =
            loom::from_value<workshop::ResolvedArrangement>(ok.value());
        REQUIRE(back.artifacts.size() == arrangement.artifacts.size());
        CHECK(back.plan == arrangement.plan);
        CHECK(back.artifacts[1].provider == arrangement.artifacts[1].provider);
        CHECK(back.artifacts[1].weave == arrangement.artifacts[1].weave);
        CHECK(back.artifacts[1].authored_role == arrangement.artifacts[1].authored_role);
    }
    {
        const loom::Unverified u = loom::parse(loom::serialize(loom::to_value(powers)));
        const loom::Admission ok = loom::admit(u, loom::schema_of<workshop::ResolvedPowers>());
        REQUIRE_MESSAGE(ok.ok(), "powers did not admit");
        const workshop::ResolvedPowers back =
            loom::from_value<workshop::ResolvedPowers>(ok.value());
        CHECK(back.providers == powers.providers);
        REQUIRE(back.powers.size() == powers.powers.size());
        CHECK(active_provider(back, "math.max") == active_provider(powers, "math.max"));
        CHECK(stack_of(back, tmr::kNormalizeDelay)->contributions.back().composite);
    }
}
