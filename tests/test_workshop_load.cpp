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

/// MOUNT A PLAN BOOTER AND KEEP THE PARTICIPANT, which is what a realization owner
/// needs: `loom::mount_granted` hands back only a WeaveId, and the owner is wired to
/// the weave itself so a settled answer can wake it (BOOT-0).
///
/// THE HOST WRITES THE GRANT, and this is that act: the plan booter may send
/// `zen.LoadWeave` to this Manager and nothing else, to nobody else. It is spelled out
/// rather than derived from the Emit set for `mount_door`'s reason -- a rig that minted
/// the grant could not notice a host quietly widening it.
inline load::PlanBooter& mount_booter_in(loom::Switchboard& bus, loom::WeaveId manager,
                                         load::BootAnswers& answers, loom::WeaveId& id) {
    loom::Grant operate;
    operate.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
    auto speaker = std::make_unique<load::PlanBooter>(answers);
    load::PlanBooter& voice = *speaker;
    id = bus.register_weave(std::move(speaker), std::move(operate));
    voice.zen_set_self(id);
    return voice;
}

struct PlanRig {
    loom::Switchboard bus;
    op::Catalog catalog;
    op::OperatorHostSurface operators{catalog};
    loom::Kernel kernel{bus};
    loom::WeaveId control = loom::mount_control(kernel, bus);
    loom::WeaveId manager = loom::mount_manager(control, bus);

    load::BootAnswers answers;
    loom::WeaveId booter;
    load::PlanBooter& voice = mount_booter_in(bus, manager, answers, booter);

    Heard heard;
    loom::WeaveId witness;

    /// DECLARED AFTER THE KERNEL, exactly as `main()` declares it.
    load::PlanExecutor executor{bus,    catalog, operators, voice,
                                manager, answers, [](const std::string& stem) {
                                    return stage().so(stem);
                                }};

    PlanRig() {
        loom::Grant reach;
        reach.allow_to_any(tmr::StartTimer::zen_name, tmr::StartTimer::zen_version);
        reach.allow_to_any(loom::PrepareShutdown::zen_name, loom::PrepareShutdown::zen_version);
        witness = loom::mount_granted<Witness>(bus, std::move(reach), heard);
    }

    /// SPEND TURNS. No predicate, so the number IS the whole semantics -- this is a
    /// "settle the world" helper and not a wait, and it makes no claim about anything
    /// arriving. The empty-turn return it used to carry was pure optimisation here
    /// (pumping an empty queue delivers nothing either way) and it is gone anyway, so
    /// this file holds one rule about what an empty turn means: nothing.
    void drain(int turns = 8) {
        for (int i = 0; i < turns; ++i) {
            bus.pump_pending();
        }
    }

    /// ⭐ REALIZE A WHOLE PLAN, BY BEING THE HOST (BOOT-0).
    ///
    /// THE TURNING IS HERE, IN THE CALLER, AND THAT IS THE WHOLE POINT OF THIS
    /// HELPER'S EXISTENCE. Production Workshop begins realization and goes back to its
    /// ordinary loop; a test case has no ordinary loop, so it turns the crank itself --
    /// exactly as a script driving any Loom host does, and exactly as `main` does not.
    /// `PlanExecutor` contains no `pump_pending`, no drain, no wait and no fuse; every
    /// turn any plan in this file ever takes is on THIS line.
    ///
    /// A FIXED COUNT AND NO PREDICATE. It makes no claim that anything arrived -- a
    /// case that cares asserts the state afterwards, and `state()` says which of the
    /// five it is rather than leaving a reader to infer settlement from a loop exit.
    load::Executed realize(load::LoadPlan plan, int turns = 32) {
        executor.begin(std::move(plan));
        drain(turns);
        return executor.outcome();
    }

    /// REALIZE A SECOND PLAN ON THIS SAME RUNTIME, WITH A SECOND OWNER.
    ///
    /// AN OWNER REALIZES ONE PLAN (BOOT-0). It holds a cursor into that plan and one
    /// load conversation, so `begin` refuses a second rather than abandoning a row that
    /// may still be in flight -- which is a real difference from the straight-line
    /// `run()` this replaced, where every call was a fresh stack frame and the object
    /// held nothing between them.
    ///
    /// A HOST THAT WANTED A SECOND ARRANGEMENT WOULD BUILD A SECOND OWNER, against the
    /// same catalog and the same Kernel, so that is exactly what this does: its own
    /// booter, its own answer record, the SAME runtime underneath. That is what makes a
    /// runtime provider collision reachable at all -- one plan cannot name an artifact
    /// twice, so the second mount has to come from a second execution.
    load::Executed realize_again(load::LoadPlan plan, int turns = 32) {
        extra_answers.push_back(std::make_unique<load::BootAnswers>());
        loom::WeaveId id{};
        load::PlanBooter& speaker = mount_booter_in(bus, manager, *extra_answers.back(), id);
        extra_owners.push_back(std::make_unique<load::PlanExecutor>(
            bus, catalog, operators, speaker, manager, *extra_answers.back(),
            [](const std::string& stem) { return stage().so(stem); }));
        extra_owners.back()->begin(std::move(plan));
        drain(turns);
        return extra_owners.back()->outcome();
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
        // A FUSE FOLLOWED BY AN ASSERTION (QR-9), which is what makes the number
        // harmless: the letter arriving is the stop, and running out of turns without
        // it is a red rather than a shrug. It does NOT stop on an empty turn -- an
        // empty queue is not a statement that the letter is not coming.
        for (int i = 0; i < 40 && heard.letters == before; ++i) {
            bus.pump_pending();
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

    /// MOUNT THE HOST'S OBSERVATION DOOR, with the production grant spelled out.
    ///
    /// THE TWO RULES ARE COPIED FROM `workshop.cpp` DELIBERATELY rather than minted
    /// from the Emit set: `emit_default_grant` would give this weave `to_any` for
    /// everything it declares, which is what the host writes anyway here -- but a rig
    /// that derived the grant could not notice the host quietly widening it.
    loom::WeaveId mount_door(std::string plan_path = std::string()) {
        return mount_door_over(executor, std::move(plan_path));
    }

    /// ...AND THE SAME DOOR OVER SOMEBODY ELSE'S OWNER, so a case can point it at a
    /// realization that is deliberately stuck mid-row. The door is unchanged; what
    /// changes is which owner it is reading.
    loom::WeaveId mount_door_over(const load::PlanExecutor& owner,
                                  std::string plan_path = std::string()) {
        auto door = std::make_unique<workshop::ArrangementDoor>(owner, catalog,
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

    /// ⚠ DECLARED LAST, SO THEY ARE DESTROYED FIRST, and that is a lifetime claim and
    /// not tidiness: a second owner holds an `op::OperatorOffer` and provider
    /// identities, and must go before the Kernel, the catalog and the surface -- the
    /// same order `main()` gets from declaring its executor after its Kernel. The
    /// answer records go after the owners that read them, for the same reason.
    std::vector<std::unique_ptr<load::BootAnswers>> extra_answers;
    std::vector<std::unique_ptr<load::PlanExecutor>> extra_owners;

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

/// WHAT `zengine-provider-min` DECLARES ITSELF TO BE. An artifact's provider identity
/// is not its stem and never was; naming it once here is what keeps a `mounted(...)`
/// check from quietly asking about a provider nothing claims to be -- which passes
/// whenever the answer is meant to be false, and is therefore worth nothing.
inline constexpr const char* kMinProvider = "zengine.operators.test.min";

/// The delay a correct `timer.normalize_delay` makes of (-500, repeating).
constexpr std::int64_t kAuthoredDelay = -500;
constexpr std::int64_t kHonestAnswer = 1;
/// ...and what a `math.max` overlaid as a MIN makes of the same pair.
constexpr std::int64_t kOverlaidAnswer = -500;

// ---- QR-9: the two participants a load conversation can be settled BY -----------

/// A PARTICIPANT THIS HOST ADMITS, saying something perfectly well-formed about
/// somebody else's conversation.
///
/// NOT A FORGERY AND NOT AN ATTACK, which is the whole reason it is the falsifier.
/// `zen.Result`, `zen.Ack` and `zen.Refused` are a UNIVERSAL vocabulary: any weave a
/// host grants them may legitimately send one to any weave that accepts them, and
/// the plan booter accepts all three. The only thing wrong with what this weave says
/// is that its correlation names no conversation the booter opened.
struct StrayState {
    std::int64_t idle = 0;
    ZEN_SHAPE(StrayState, 1, ZEN_FIELD(idle));
};

class Stray : public loom::WeaveBase<Stray, StrayState, loom::Accept<Nudge>,
                                     loom::Emit<loom::Result, loom::Refused>> {
public:
    void on(const Nudge&, loom::Mail&) {}
};

/// The correlation a stray answer carries. It cannot collide with a real one: the
/// booter's counter is its own, starts at zero and pre-increments, so the first
/// conversation in any rig here is 1 and no case performs 909 loads. The focused
/// fixture below pins that counter's shape, so this stays true.
constexpr std::uint64_t kStrayCorrelation = 909;

/// A RESPONDENT THAT TAKES THE ANSWER AWAY WITH IT (ANS-02) -- the smallest thing
/// that makes a load conversation genuinely unresolved while the queue is empty.
///
/// It is the shape FRIC-R2 measured: `defer_answer()` moves the answer right OUT of
/// the queue and into a capability the respondent holds, so `pending()` reads 0 with
/// the answer still owed. Nothing here stands in for the Weave Manager's protocol --
/// it implements no `zen.LoadWeave` semantics at all. What it stands in for is the
/// one property the real Manager does not have today: taking longer to answer than
/// the turn its request arrived on.
struct SlowAnswers {
    bool held = false;
    loom::DeferredAnswer answer;
    /// Whoever asked, remembered from the delivery so a LATE answer can be aimed back.
    loom::WeaveId asker{};
};

struct SlowState {
    std::int64_t asked = 0;
    ZEN_SHAPE(SlowState, 1, ZEN_FIELD(asked));
};

/// "SAY IT AGAIN, WITH A CORRELATION OF MY CHOOSING." The one move a bus-stamped
/// sender check cannot catch, and therefore the only honest falsifier for the OTHER
/// half of the wall: the weave that WAS asked, speaking with perfect standing, about
/// a conversation that is not the one outstanding.
struct SayAgain {
    std::int64_t correlation = 0;
    std::string value;
    ZEN_SHAPE(SayAgain, 1, ZEN_FIELD(correlation), ZEN_FIELD(value));
};

class SlowManager : public loom::WeaveBase<SlowManager, SlowState,
                                           loom::Accept<loom::LoadWeave, Nudge, SayAgain>,
                                           loom::Emit<loom::Result>> {
public:
    explicit SlowManager(SlowAnswers& into) : into_(&into) {}

    void on(const loom::LoadWeave&, loom::Mail& mail) {
        ++state_.asked;
        into_->asker = mail.reply_to().valid() ? mail.reply_to() : mail.sender();
        into_->answer = mail.defer_answer();
        into_->held = into_->answer.valid();
    }

    /// THE UNRELATED DELIVERY that lets the held answer be spent. A deferred answer
    /// is spent from a LATER handler, so something must wake this weave -- which is
    /// exactly why an empty queue is not the end of the conversation. What it spends
    /// carries the correlation the ASK was delivered with, restored by the bus from
    /// the deferred record; this fixture never gets to choose it.
    void on(const Nudge&, loom::Mail& mail) {
        if (!into_->held) {
            return;
        }
        into_->held = false;
        (void)loom::answer_deferred(into_->answer, mail, loom::Result{"4242"});
    }

    /// ...and an ORDINARY send, where it does choose it.
    void on(const SayAgain& c, loom::Mail& mail) {
        if (!into_->asker.valid()) {
            return;
        }
        (void)mail.send(into_->asker, loom::Result{c.value},
                        static_cast<std::uint64_t>(c.correlation));
    }

private:
    SlowAnswers* into_;
};

/// The weave id the fixture's spent answer names, so a case can tell it apart from
/// anything a real Kernel would mint.
constexpr std::uint64_t kSlowWeaveId = 4242;

/// WHAT ACTUALLY REACHED THE BOOTER, so no case below can pass because a stray answer
/// never arrived at all. A wall that is never knocked on is not a wall that held.
struct AnswersSeen {
    int at_booter = 0;      ///< answer-shaped messages DELIVERED to the plan booter
    int stray = 0;          ///< ...of which carried the stray correlation
    int refused_by_bus = 0; ///< any answer shape the bus refused instead of delivering
};

/// Watch every `zen.Result` / `zen.Ack` / `zen.Refused` this bus moves, and record the
/// ones that reached `booter`. The correlation is on the envelope, so the tap can tell
/// a stray from a real settlement without guessing.
void watch_answers(loom::Switchboard& bus, loom::WeaveId booter, AnswersSeen& seen) {
    bus.add_observer([booter, &seen](const loom::BusEvent& e) {
        const bool answer_shape = e.schema_name == loom::Result::zen_name ||
                                  e.schema_name == loom::Ack::zen_name ||
                                  e.schema_name == loom::Refused::zen_name;
        if (!answer_shape) {
            return;
        }
        if (e.kind == loom::EventKind::Refused) {
            ++seen.refused_by_bus;
            return;
        }
        if (e.kind != loom::EventKind::Delivered || e.target != booter) {
            return;
        }
        ++seen.at_booter;
        if (e.correlation == kStrayCorrelation) {
            ++seen.stray;
        }
    });
}

/// A PARTICIPANT WITH NOTHING TO DO WITH LOADING, so "the host is still working" is
/// measured on traffic the plan has never heard of.
struct BystanderState {
    std::int64_t nudges = 0;
    ZEN_SHAPE(BystanderState, 1, ZEN_FIELD(nudges));
};

class Bystander : public loom::WeaveBase<Bystander, BystanderState, loom::Accept<Nudge>,
                                         loom::Emit<>> {
public:
    explicit Bystander(std::int64_t& count) : count_(&count) {}
    void on(const Nudge&, loom::Mail&) { ++*count_; }

private:
    std::int64_t* count_;
};

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
    const load::Executed done = rig.realize(plan_of({provides("zengine-operators-basic")}));
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
        rig.realize(plan_of({weaves("zengine-plain-weave", "test.plain")}));
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
    const load::Executed done = rig.realize(
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
        const load::Executed no = without.realize(plan_of(
            {provides("zengine-operators-basic"), weaves("zengine-timer", tmr::kTimerRole)}));
        CHECK_FALSE(no.ok);
        CHECK(no.refusal.find("zengine-timer") != std::string::npos);
        CHECK(no.refusal.find("weave load refused") != std::string::npos);
    }
    {
        PlanRig with;
        const load::Executed yes = with.realize(
            plan_of({provides("zengine-operators-basic"), both("zengine-timer", tmr::kTimerRole)}));
        CHECK_MESSAGE(yes.ok, yes.refusal);
    }
}

TEST_CASE("the offer BRACKETS the load and is withdrawn: a second load is a second handoff") {
    PlanRig rig;
    const load::Executed done = rig.realize(
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
        rig.realize(plan_of({provides("zengine-operators-basic"),
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
        rig.realize(plan_of({weaves("zengine-not-on-this-disk", "test.absent")}));
    CHECK_FALSE(done.ok);
    CHECK(done.refusal.find("artifact 'zengine-not-on-this-disk'") != std::string::npos);
    CHECK(done.refusal.find("weave load refused") != std::string::npos);
    CHECK(done.resolved.empty());
}

TEST_CASE("a missing PROVIDER artifact is refused by the mount, naming the step") {
    PlanRig rig;
    const load::Executed done =
        rig.realize(plan_of({provides("zengine-not-on-this-disk")}));
    CHECK_FALSE(done.ok);
    CHECK(done.refusal.find("provider mount refused") != std::string::npos);
    CHECK(rig.catalog.providers().empty());
}

TEST_CASE("a missing artifact leaves the authored plan exactly as it was written") {
    // UNRESOLVED INTENT REMAINS INTENT. The runtime refuses; nothing edits the plan.
    const load::LoadPlan authored = plan_of({weaves("zengine-not-on-this-disk", "test.absent")});
    const std::string before = load_persist::to_text(authored);
    PlanRig rig;
    const load::Executed done = rig.realize(authored);
    REQUIRE_FALSE(done.ok);
    CHECK(load_persist::to_text(authored) == before);
    CHECK(authored.artifacts.size() == 1);
}

TEST_CASE("an ORDINARY provider collision still refuses, and the catalog is untouched") {
    PlanRig rig;
    const load::Executed done = rig.realize(
        plan_of({provides("zengine-operators-basic"), provides("zengine-provider-min")}));
    CHECK_FALSE(done.ok);
    CHECK(done.refusal.find("artifact 'zengine-provider-min'") != std::string::npos);
    CHECK(done.refusal.find("needs an explicit overlay") != std::string::npos);
    // The FIRST provider still stands: an artifact is the atomic unit, not the plan.
    CHECK(rig.catalog.mounted("zengine.operators.basic"));
    CHECK_FALSE(rig.catalog.mounted(kMinProvider));
}

TEST_CASE("a provider from another era is still refused on its NUMBER, through the plan") {
    PlanRig rig;
    const load::Executed done = rig.realize(plan_of({provides("zengine-stale-provider")}));
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
        rig.realize(plan_of({weaves("zengine-broken-consumer", "test.broken")}));
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
        rig.realize(plan_of({provides("zengine-operators-basic"),
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
    REQUIRE(rig.realize(plan_of({provides("zengine-operators-basic"),
                                      provides("zengine-timer")}))
                .ok);
    // A SECOND OWNER, SAME RUNTIME (BOOT-0): one owner realizes one plan, so a second
    // execution is a second owner -- which is also what a host doing this would build.
    const load::Executed done =
        rig.realize_again(plan_of({both("zengine-timer", tmr::kTimerRole)}));
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
    const load::Executed done = rig.realize(plan_of({provides("zengine-operators-basic")}));
    REQUIRE_MESSAGE(done.ok, done.refusal);
    CHECK(rig.catalog.providers().size() == 1);
    CHECK_FALSE(rig.catalog.mounted(kMinProvider));
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
    const load::Executed done = rig.realize(plan_of(
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
    const load::Executed done = rig.realize(
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
        const load::Executed done = rig.realize(
            plan_of({provides("zengine-operators-basic"), both("zengine-timer", tmr::kTimerRole)}));
        REQUIRE_MESSAGE(done.ok, done.refusal);
        CHECK(rig.scheduled_delay(rig.weave_of(done, "zengine-timer"), "beat", kAuthoredDelay,
                                  true) == kHonestAnswer);
    }
    CHECK(true); // reached, with nothing dangling on the way out
}

TEST_CASE("unmounting one record's provider drops its contributions and nothing else") {
    PlanRig rig;
    const load::Executed done = rig.realize(
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
        const load::Executed done = a.realize(run());
        REQUIRE_MESSAGE(done.ok, done.refusal);
        first_powers = a.catalog.size();
        first_answer = a.scheduled_delay(a.weave_of(done, "zengine-timer"), "beat",
                                         kAuthoredDelay, true);
        CHECK(a.catalog.providers().size() == 2);
        CHECK(done.resolved.size() == 3);
    }
    {
        PlanRig b;
        const load::Executed done = b.realize(run());
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
        const load::Executed done = a.realize(read.plan);
        REQUIRE_MESSAGE(done.ok, done.refusal);
        CHECK(a.catalog.providers().size() == 3);
        with = a.scheduled_delay(a.weave_of(done, "zengine-timer"), "beat", kAuthoredDelay, true);
    }
    {
        PlanRig b;
        const load_persist::LoadedPlan read = load_persist::load_file(baseline);
        REQUIRE(read.outcome.accepted);
        const load::Executed done = b.realize(read.plan);
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
        rig.realize(plan_of({provides("zengine-operators-basic"),
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
    const load::Executed done = rig.realize(plan_of(
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
        const load::Executed done = right.realize(
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
        const load::Executed done = wrong.realize(
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
        rig.realize(plan_of({provides("zengine-operators-basic"),
                             both("zengine-timer", tmr::kTimerRole),
                             weaves("zengine-plain-weave", "test.plain")}));
    REQUIRE_MESSAGE(done.ok, done.refusal);

    const workshop::ResolvedArrangement said =
        workshop::describe_arrangement(rig.executor, "a-plan.json");
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
    CHECK(basic->state == std::string(workshop::kResolvedToken));
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
        REQUIRE(rig.realize(one).ok);
        const workshop::ResolvedArrangement said =
            workshop::describe_arrangement(rig.executor, "");
        role_a = row_of(said, "zengine-timer")->authored_role;
        weave_a = row_of(said, "zengine-timer")->weave;
    }
    {
        PlanRig rig;
        REQUIRE(rig.realize(one).ok);
        const workshop::ResolvedArrangement said =
            workshop::describe_arrangement(rig.executor, "");
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
    REQUIRE(rig.realize(plan_of({provides("zengine-operators-basic"),
                                 both("zengine-timer", tmr::kTimerRole)}))
                .ok);
    const workshop::ResolvedArrangement said =
        workshop::describe_arrangement(rig.executor, "");

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
    REQUIRE(rig.realize(plan_of({provides("zengine-operators-basic")})).ok);
    const workshop::ResolvedArrangement said =
        workshop::describe_arrangement(rig.executor, "");
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
    REQUIRE(rig.realize(plan_of({weaves("zengine-plain-weave", "test.plain")})).ok);
    const workshop::ResolvedArrangement said =
        workshop::describe_arrangement(rig.executor, "");
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
        REQUIRE(rig.realize(plan_of({provides("zengine-provider-min")})).ok);
        ordinary = *row_of(
            workshop::describe_arrangement(rig.executor, ""),
            "zengine-provider-min");
    }
    {
        PlanRig rig;
        REQUIRE(rig.realize(plan_of({provides("zengine-operators-basic"),
                                     provides("zengine-provider-min", op::MountMode::Overlay)}))
                    .ok);
        overlaid = *row_of(
            workshop::describe_arrangement(rig.executor, ""),
            "zengine-provider-min");
    }
    CHECK(ordinary.authored_provider == std::string(load_persist::kModeNormal));
    CHECK(overlaid.authored_provider == std::string(load_persist::kModeOverlay));
    // ...and every resolved field is identical, which is what makes the sentence above
    // a measurement rather than a description.
    CHECK(ordinary.provider == overlaid.provider);
    CHECK(ordinary.powers == overlaid.powers);
    CHECK(ordinary.state == overlaid.state);
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
    // What it measures is that the projection walks the AUTHORED list, so a partial
    // arrangement cannot read as a complete one.
    //
    // ⭐ AND BOOT-0 SPLIT THE MARK IN TWO. Before, rows 2 and 3 were both
    // `performed = false` and there was no wire spelling that could tell "this is the
    // artifact that REFUSED" from "this is an artifact nothing ever tried". They are
    // different facts about a maker's project and they are two tokens now.
    PlanRig rig;
    const load::Executed done = rig.realize(plan_of({provides("zengine-operators-basic"),
                                                     weaves("zengine-not-here", "test.ghost"),
                                                     weaves("zengine-plain-weave", "test.plain")}));
    REQUIRE_FALSE(done.ok);

    const workshop::ResolvedArrangement said =
        workshop::describe_arrangement(rig.executor, "");
    REQUIRE(said.artifacts.size() == 3);
    CHECK(said.artifacts[0].state == std::string(workshop::kResolvedToken));
    CHECK(said.artifacts[1].state == std::string(workshop::kRefusedToken));
    CHECK(said.artifacts[2].state == std::string(workshop::kAuthoredToken));
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
    REQUIRE(rig.realize(plan_of({provides("zengine-operators-basic"),
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
    const load::Executed done = rig.realize(plan_of({provides("zengine-operators-basic"),
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
    REQUIRE(rig.realize(plan_of({provides("zengine-operators-basic")})).ok);
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
    REQUIRE(rig.realize(plan_of({provides("zengine-operators-basic"),
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
    REQUIRE(rig.realize(plan_of({provides("zengine-operators-basic")})).ok);
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
    REQUIRE(rig.realize(plan_of({provides("zengine-operators-basic")})).ok);
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
    REQUIRE(rig.realize(plan_of({provides("zengine-operators-basic"),
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
    REQUIRE(rig.realize(plan_of({provides("zengine-operators-basic"),
                                 both("zengine-timer", tmr::kTimerRole)}))
                .ok);
    const workshop::ResolvedArrangement arrangement =
        workshop::describe_arrangement(rig.executor, "p.json");
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

// =============================================================================
// 9. SETTLEMENT -- a load conversation ends because ITS OWN answer arrived (QR-9)
// =============================================================================
//
// TWO DEFECTS, ONE PATH, AND THEY FAIL IN OPPOSITE DIRECTIONS. One made the executor
// stop waiting too early (an empty bounded turn read as "no answer is coming"); the
// other made it stop for the wrong reason (any admitted answer shape read as "my load
// answered"). Between them the adapter could answer the question "did MY load
// conversation settle?" with a yes it had not earned and a no it could not support.
//
// WHAT EACH ARM MEASURED ON THE UNREPAIRED SOURCE, before any of this was written:
//
//   a stray zen.Result   ->  the plan reported ok, naming WeaveId 424242 -- the number
//                            the STRAY chose, which no Kernel ever minted
//   a stray zen.Refused  ->  "artifact 'zengine-plain-weave': weave load refused:
//                            somebody else's refusal", for a load that had succeeded
//   a stray zen.Result   ->  a MISSING artifact reported as loaded, ok = true
//   an empty turn        ->  the wait gave up on turn 2, pending() == 0, with the
//                            answer genuinely owed -- and it arrived afterwards
//
// NOTHING HERE CHANGES THE LOAD PROTOCOL. Tier 4 and tier 5 already drive the real
// `zen.LoadWeave` both ways round; this tier asks only how the asker decides that one
// of those two endings has happened TO IT.

TEST_CASE("QR-9: an admitted answer with the WRONG correlation does not settle this load") {
    // DECLARED BEFORE THE RIG so the observer's capture outlives the bus that calls it.
    AnswersSeen seen;
    PlanRig rig;
    watch_answers(rig.bus, rig.booter, seen);

    // ENQUEUED BEFORE THE CONVERSATION OPENS, which is what puts it INSIDE it: the
    // executor's first pumped turn dispatches the backlog present at entry, and this
    // is at the head of it. Nothing is forged -- the stray speaks as itself, with a
    // grant this host wrote, in a vocabulary every participant shares.
    const loom::WeaveId stray = loom::mount<Stray>(rig.bus);
    rig.bus.send_as(stray, rig.booter,
                    loom::Message(loom::to_value(loom::Result{"424242"}), stray, stray,
                                  kStrayCorrelation));

    const load::Executed done =
        rig.realize(plan_of({weaves("zengine-plain-weave", "test.plain")}));

    REQUIRE(seen.stray == 1);        // it really was handed to the booter...
    CHECK(seen.refused_by_bus == 0); // ...admitted, not stopped at the door
    REQUIRE(done.ok);
    const loom::WeaveId got = rig.weave_of(done, "zengine-plain-weave");
    CHECK(got.valid());
    CHECK(got.value != 424242u); // the id the STRAY named
    CHECK(rig.answers.answered);
    CHECK_FALSE(rig.answers.awaiting()); // settled, and by its own answer
}

TEST_CASE("QR-9: a stray zen.Refused is not this load's refusal") {
    AnswersSeen seen;
    PlanRig rig;
    watch_answers(rig.bus, rig.booter, seen);

    // THE ARM AN EYE ON `zen.Result` ALONE WOULD MISS, and the more damaging of the
    // two: a refusal aimed at nobody in particular took a load that had already
    // succeeded and reported it as refused, in words the Manager never said.
    const loom::WeaveId stray = loom::mount<Stray>(rig.bus);
    rig.bus.send_as(stray, rig.booter,
                    loom::Message(loom::to_value(loom::Refused{"a refusal owed to somebody else"}),
                                  stray, stray, kStrayCorrelation));

    const load::Executed done =
        rig.realize(plan_of({weaves("zengine-plain-weave", "test.plain")}));

    REQUIRE(seen.stray == 1);
    CHECK(done.ok);
    CHECK(done.refusal.empty());
    CHECK_FALSE(rig.answers.refused);
    CHECK(rig.answers.reason.empty());
    CHECK(rig.weave_of(done, "zengine-plain-weave").valid());
}

TEST_CASE("QR-9: a stray zen.Result cannot answer for an artifact that is not there") {
    AnswersSeen seen;
    PlanRig rig;
    watch_answers(rig.bus, rig.booter, seen);

    const loom::WeaveId stray = loom::mount<Stray>(rig.bus);
    rig.bus.send_as(stray, rig.booter,
                    loom::Message(loom::to_value(loom::Result{"424242"}), stray, stray,
                                  kStrayCorrelation));

    // THE WORST SHAPE THE DEFECT TOOK. A success arriving from nowhere did not merely
    // mislabel a load -- it CANCELLED a refusal, so a plan naming an artifact that is
    // not on this disk completed, and the host went on to run an arrangement it had
    // not assembled.
    const load::Executed done =
        rig.realize(plan_of({weaves("zengine-not-on-this-disk", "test.absent")}));

    REQUIRE(seen.stray == 1);
    CHECK_FALSE(done.ok);
    CHECK(done.refusal.find("artifact 'zengine-not-on-this-disk'") != std::string::npos);
    CHECK(done.refusal.find("weave load refused") != std::string::npos);
    CHECK(done.resolved.empty());
    CHECK(rig.answers.refused);
    CHECK_FALSE(rig.answers.awaiting());
}

TEST_CASE("QR-9: a correlation is not a secret, so the SENDER is the other half of the wall") {
    AnswersSeen seen;
    PlanRig rig;
    watch_answers(rig.bus, rig.booter, seen);

    // THE NUMBER IS GUESSABLE, AND THAT IS NOT A FLAW TO BE FIXED HERE. This record's
    // counter is its own and starts at zero, so the first conversation on any fresh
    // host is 1 -- a number every other asker in the process is equally likely to be
    // using, and one anybody may put on any message. A correlation IDENTIFIES which
    // conversation an answer is about (Loom's ANS-05); what says the answer came from
    // the weave that was actually asked is the sender, and the BUS stamps that.
    const loom::WeaveId stray = loom::mount<Stray>(rig.bus);
    rig.bus.send_as(stray, rig.booter,
                    loom::Message(loom::to_value(loom::Result{"424242"}), stray, stray,
                                  /*correlation=*/1));

    const load::Executed done =
        rig.realize(plan_of({weaves("zengine-plain-weave", "test.plain")}));

    REQUIRE(seen.at_booter >= 1); // the lucky guess really was handed to the booter
    CHECK(seen.refused_by_bus == 0);
    REQUIRE(done.ok);
    CHECK(rig.weave_of(done, "zengine-plain-weave").value != 424242u);
    CHECK(rig.answers.answered);
    CHECK_FALSE(rig.answers.awaiting());
    CHECK(seen.at_booter == 2); // ...and the real answer arrived after it, unhurried
}

TEST_CASE("QR-9: both real arms settle on their OWN correlated answer, and on nothing else") {
    // THE PROTOCOL IS UNCHANGED, and this is the case that says so from the asker's
    // side: one answer reaches the booter per load, it carries this conversation's
    // correlation, and it is what ends the wait.
    SUBCASE("a real artifact loads, and the success is its own") {
        AnswersSeen seen;
        PlanRig rig;
        watch_answers(rig.bus, rig.booter, seen);

        const load::Executed done =
            rig.realize(plan_of({weaves("zengine-plain-weave", "test.plain")}));

        CHECK(done.ok);
        CHECK(seen.at_booter == 1); // one conversation, one answer
        CHECK(seen.stray == 0);
        CHECK(rig.answers.answered);
        CHECK_FALSE(rig.answers.refused);
        CHECK_FALSE(rig.answers.awaiting());
        CHECK(rig.weave_of(done, "zengine-plain-weave").valid());
    }
    SUBCASE("a missing artifact refuses, and the refusal is its own") {
        AnswersSeen seen;
        PlanRig rig;
        watch_answers(rig.bus, rig.booter, seen);

        const load::Executed done =
            rig.realize(plan_of({weaves("zengine-not-on-this-disk", "test.absent")}));

        CHECK_FALSE(done.ok);
        CHECK(seen.at_booter == 1);
        CHECK(seen.stray == 0);
        CHECK(rig.answers.answered);
        CHECK(rig.answers.refused);
        CHECK_FALSE(rig.answers.reason.empty()); // the loader's own words, not this layer's
        CHECK_FALSE(rig.answers.awaiting());
    }
}

TEST_CASE("QR-9: a turn that delivers nothing does not mean no answer is coming") {
    // A FOCUSED FIXTURE AND NOT THE REAL MANAGER, deliberately. The real
    // `zen.LoadWeave` answers deterministically before the queue can empty (FRIC-R2
    // measured four turns, success and refusal alike), so the production path cannot
    // itself produce the observation this case has to make. What is under test is the
    // INFERENCE, not the Manager: a respondent that DEFERS its answer holds it outside
    // the queue entirely, which is the substrate's own ANS-02 and needs no timing.
    loom::Switchboard bus;
    SlowAnswers slow_state;
    load::BootAnswers answers;
    const loom::WeaveId slow = loom::mount<SlowManager>(bus, slow_state);
    loom::Grant operate;
    operate.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, slow);
    const loom::WeaveId booter =
        loom::mount_granted<load::PlanBooter>(bus, std::move(operate), answers);

    // THE COUNTER'S SHAPE, pinned here because `kStrayCorrelation` above depends on it:
    // this record's numbering is its own, starts at zero and pre-increments.
    const std::uint64_t first = answers.ask(slow);
    CHECK(first == 1);
    CHECK(answers.awaiting());

    bus.send_as(booter, slow,
                loom::Message(loom::to_value(
                                  loom::LoadWeave{"unanswered", "unanswered", "test.unanswered"}),
                              booter, booter, first));

    // THE WAIT THE PRODUCTION EXECUTOR NOW SPELLS: bounded turns, with the CONVERSATION
    // as the condition. The empty turn is observed rather than acted on.
    bool zero_work_turn = false;
    for (int turn = 0; turn < 8 && answers.awaiting(); ++turn) {
        if (bus.pump_pending() == 0) {
            zero_work_turn = true;
        }
    }

    // THE DELETED INFERENCE, TERM BY TERM. "zero deliveries this turn" is true;
    // "nothing is queued" is true; and every conclusion the old early-out drew from
    // the pair is false.
    REQUIRE(zero_work_turn);
    CHECK(bus.pending() == 0);
    CHECK(answers.awaiting());     // the conversation is genuinely unresolved
    CHECK_FALSE(answers.answered);
    REQUIRE(slow_state.held);      // and the answer is OWED, held off the queue

    // WHAT WOULD SETTLE IT, asked while it is still open -- because settling closes it.
    CHECK(answers.settles(first, slow));
    CHECK_FALSE(answers.settles(first + 1, slow)); // right respondent, wrong conversation
    CHECK_FALSE(answers.settles(first, booter));   // right conversation, wrong speaker

    // ...AND THE ANSWER ARRIVES AFTERWARDS ANYWAY. An unrelated delivery is what lets
    // the respondent spend what it was holding, which is the whole reason an empty
    // queue proves nothing: the next thing to happen had not happened yet.
    (void)bus.send(slow,
                   loom::Message(loom::to_value(Nudge{}), loom::WeaveId{}, loom::WeaveId{}, 0));
    for (int turn = 0; turn < 8 && answers.awaiting(); ++turn) {
        bus.pump_pending();
    }
    CHECK(answers.answered);
    CHECK_FALSE(answers.refused);
    CHECK(answers.weave == kSlowWeaveId);
    CHECK_FALSE(answers.awaiting());
    CHECK_FALSE(answers.settles(first, slow)); // and a duplicate of it is now inert

    // A SECOND CONVERSATION NEVER REUSES THE FIRST'S NUMBER, so a late answer to the
    // one just closed cannot settle the one just opened.
    const std::uint64_t second = answers.ask(slow);
    CHECK(second == first + 1);
    CHECK_FALSE(answers.answered);
    CHECK_FALSE(answers.settles(first, slow));
}

TEST_CASE("QR-9: an answer from the weave that WAS asked, about another conversation, "
          "settles nothing") {
    // THE HALF A SENDER CHECK CANNOT SEE. In the three cases above the impostor was a
    // different weave, so the bus-stamped sender already gave it away. Here the
    // speaker is the exact respondent this conversation is waiting on, granted the
    // shape, answering the asker it really was asked by -- and the only thing wrong
    // with what it says is the number naming which conversation it is about. This is
    // the arm that requires the correlation, and the shape a stale answer to a
    // conversation that has moved on actually takes.
    loom::Switchboard bus;
    SlowAnswers slow_state;
    load::BootAnswers answers;
    const loom::WeaveId slow = loom::mount<SlowManager>(bus, slow_state);
    loom::Grant operate;
    operate.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, slow);
    const loom::WeaveId booter =
        loom::mount_granted<load::PlanBooter>(bus, std::move(operate), answers);

    const std::uint64_t mine = answers.ask(slow);
    bus.send_as(booter, slow,
                loom::Message(loom::to_value(
                                  loom::LoadWeave{"unanswered", "unanswered", "test.unanswered"}),
                              booter, booter, mine));
    for (int turn = 0; turn < 4 && answers.awaiting(); ++turn) {
        bus.pump_pending();
    }
    REQUIRE(slow_state.held); // the conversation is outstanding, and the answer is owed

    // ...and now, WHILE IT IS OUTSTANDING, the respondent says something admissible
    // about a different one.
    (void)bus.send(slow, loom::Message(loom::to_value(SayAgain{
                                           static_cast<std::int64_t>(mine) + 1, "424242"}),
                                       loom::WeaveId{}, loom::WeaveId{}, 0));
    for (int turn = 0; turn < 4; ++turn) {
        bus.pump_pending();
    }
    CHECK(answers.awaiting()); // unmoved
    CHECK_FALSE(answers.answered);
    CHECK(answers.weave == 0);

    // ...AND SO IS AN ANSWER TO THE CONVERSATION THAT HAS NOT BEEN OPENED YET, which is
    // the same defect read forwards instead of backwards.
    (void)bus.send(slow, loom::Message(loom::to_value(SayAgain{
                                           static_cast<std::int64_t>(mine) + 7, "424242"}),
                                       loom::WeaveId{}, loom::WeaveId{}, 0));
    for (int turn = 0; turn < 4; ++turn) {
        bus.pump_pending();
    }
    CHECK(answers.awaiting());
    CHECK_FALSE(answers.answered);

    // WHAT DOES SETTLE IT is the one carrying this conversation's own number.
    (void)bus.send(slow,
                   loom::Message(loom::to_value(Nudge{}), loom::WeaveId{}, loom::WeaveId{}, 0));
    for (int turn = 0; turn < 4 && answers.awaiting(); ++turn) {
        bus.pump_pending();
    }
    CHECK(answers.answered);
    CHECK(answers.weave == kSlowWeaveId);
    CHECK_FALSE(answers.awaiting());
}

TEST_CASE("BOOT-0: an unanswered load stays UNANSWERED, and nothing invents a third answer") {
    // ⭐ THE CASE THE FUSE USED TO OWN, AND WHAT BOOT-0 DID TO IT.
    //
    // This used to read: the local guard of 64 dispatch turns expires, `run()` returns,
    // and the plan stops with a sentence about a wait this host gave up on. That fuse
    // existed for exactly one reason -- a straight-line executor had to return to its
    // caller -- and a persistent owner does not. So the whole path is deleted, and what
    // is left is the truthful behaviour: the conversation is outstanding, the row is
    // loading, the plan has not advanced, and NOBODY HAS SAID ANYTHING.
    //
    // THE THING THAT MUST NOT COME BACK IS A TIMEOUT AS A SETTLEMENT. However many
    // turns this host spends, an unanswered load is not a refusal, not a failure, not a
    // cancellation and not "the answer became impossible" -- those are somebody else's
    // state, and this host knows only its own.
    PlanRig rig;
    SlowAnswers slow_state;
    load::BootAnswers answers;
    const loom::WeaveId slow = loom::mount<SlowManager>(rig.bus, slow_state);
    loom::WeaveId booter{};
    load::PlanBooter& voice = mount_booter_in(rig.bus, slow, answers, booter);

    // THE PRODUCTION OWNER, pointed at a respondent that never answers. Everything
    // else is real: a real artifact on disk, the real operator offer around it, the
    // real send. Only the answer is missing.
    load::PlanExecutor stalled{rig.bus, rig.catalog, rig.operators, voice, slow, answers,
                               [](const std::string& stem) { return stage().so(stem); }};
    stalled.begin(plan_of({weaves("zengine-plain-weave", "test.stalled")}));

    // ...AND FAR MORE TURNS THAN THE OLD FUSE ALLOWED. Sixty-four was the number that
    // used to end this wait; a hundred is spent here precisely so that a fuse restored
    // under any spelling turns this case red.
    rig.drain(100);

    CHECK(stalled.state() == load::Realization::Loading);
    CHECK(stalled.outcome().ok == false);
    CHECK(stalled.refusal().empty()); // NOBODY REFUSED ANYTHING
    CHECK(stalled.resolved().empty());
    CHECK(stalled.state_of("zengine-plain-weave") == load::RowState::Loading);

    // THE RECORD HEARD NO ANSWER, marked none refused, kept no reason of anybody
    // else's -- and is still waiting, because that is what is true.
    CHECK_FALSE(answers.answered);
    CHECK_FALSE(answers.refused);
    CHECK(answers.reason.empty());
    CHECK(answers.awaiting());
    CHECK(answers.book().outstanding() == 1);

    // ...AND THE FAR END IS UNTOUCHED. It still holds the answer right it was given.
    REQUIRE(slow_state.held);
}

// =============================================================================
// 10. THE RECORD IS LOOM'S NOW (FRIC-2) -- what the load adapter stopped owning
// =============================================================================
//
// QR-9 gave this path a correct one-slot conversation record. It was correct and it was
// the THIRD hand-written copy of the same invariant in this workspace, so FRIC-2
// harvested it: `loom::AskBook` is the asker-side record, and `BootAnswers` is now a
// small adapter that spends it. Section 9 above is UNCHANGED and still passes; those
// cases are the parity evidence, because every one of them now runs through the generic
// mechanism without a line of them being edited.
//
// What is left to prove here is what the migration ADDED -- the record's own facts, and
// the behaviour at the edge the fuse leaves behind.
//
// ---- AND WHAT QR-10 THEN SUBTRACTED ------------------------------------------------
//
// FRIC-2 read the expired fuse as a conversation this host was still party to, gave the
// book four slots so several of them could coexist, and refused a fifth load by name.
// Measured on that source, six sequential stalled loads left FOUR records open --
// correlations 1..4, all naming the same respondent -- and rounds five and six were
// refused with "this host is still tracking 4 earlier load conversations that were never
// answered". Nothing was ever going to read those four: `load_weave` had returned, its
// caller had stopped the plan, and no code in this host can resume, query or settle one
// of them again. So the fuse now FORGETS the ask it stopped waiting for; the book holds
// the one conversation this adapter actually has; and the capacity refusal is gone with
// the state that produced it.
//
// THE LINE THE CASES BELOW WALK: forgetting is LOCAL. Nothing is sent, no
// `DeferredAnswer` is revoked, and the respondent's answer right is exactly what it was.

TEST_CASE("FRIC-2: the load record SPENDS the reusable book, and the book says what it asked") {
    loom::Switchboard bus;
    SlowAnswers slow_state;
    load::BootAnswers answers;
    const loom::WeaveId slow = loom::mount<SlowManager>(bus, slow_state);
    loom::Grant operate;
    operate.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, slow);
    const loom::WeaveId booter =
        loom::mount_granted<load::PlanBooter>(bus, std::move(operate), answers);
    (void)booter;

    // ONE CONVERSATION IS ALL IT EVER HAS (QR-10). The room FRIC-2 added existed only so
    // conversations an expired fuse abandoned could pile up; the fuse forgets them now,
    // so the honest bound is the one load this adapter asks for at a time.
    CHECK(answers.book().capacity() == 1);
    CHECK_FALSE(answers.book().awaiting());

    const std::uint64_t first = answers.ask(slow);
    CHECK(first == 1); // the counter shape, unchanged: its own, from zero, pre-incremented
    REQUIRE(answers.book().outstanding() == 1);

    // THE FACTS AN ASKER RECORD HOLDS, and each one answers somebody: which conversation,
    // who may settle it, and what was asked.
    const loom::PendingAsk& mine = answers.book().entries().front();
    CHECK(mine.correlation == first);
    CHECK(mine.respondent == slow);
    CHECK_FALSE(mine.to_role()); // a load is asked of ONE Manager, never of an office
    CHECK(mine.shape == loom::LoadWeave::zen_name);
    CHECK(mine.version == loom::LoadWeave::zen_version);

    // ...and the adapter questions are that record, not a second copy of it.
    CHECK(answers.awaiting());
    CHECK(answers.asking() == first);
    CHECK(answers.settles(first, slow));
    answers.settled();
    CHECK_FALSE(answers.awaiting());
    CHECK(answers.asking() == 0);
    CHECK(answers.book().outstanding() == 0);
}

TEST_CASE("QR-10 + BOOT-0: the OWNER'S DEATH is what stops caring, and it forgets") {
    // ⭐ QR-10'S LAW, WITH THE OWNER THAT NOW HAS IT. "Stopping a wait and forgetting an
    // ask are different facts" -- and BOOT-0 removed the only thing that used to stop a
    // wait: a fuse expiring inside a straight-line call. A persistent owner never gives
    // up on a clock, so the one honest occasion left for `forget` is the owner genuinely
    // ceasing to exist. Its destructor says so, and this is that.
    //
    // THE TEST IS CONTINUATION, NOT LOOP EXIT: what matters is that after the owner is
    // gone the book is empty, the respondent is untouched, and the NEXT conversation on
    // the same record is a new number that settles on its own answer.
    PlanRig rig;
    SlowAnswers slow_state;
    const loom::WeaveId slow = loom::mount<SlowManager>(rig.bus, slow_state);
    loom::WeaveId stalled_booter{};
    load::PlanBooter& voice = mount_booter_in(rig.bus, slow, rig.answers, stalled_booter);

    {
        // ONE LOAD THAT NOBODY ANSWERS, through the real owner: a real artifact on disk,
        // the real operator offer around it, the real send. Only the answer is missing.
        load::PlanExecutor stalled{rig.bus,  rig.catalog, rig.operators, voice,
                                   slow,     rig.answers,
                                   [](const std::string& stem) { return stage().so(stem); }};
        stalled.begin(plan_of({weaves("zengine-plain-weave", "test.stalled")}));
        rig.drain(8);
        REQUIRE(stalled.state() == load::Realization::Loading);
        // WHILE IT LIVES IT IS STILL WAITING, which is the half a fuse used to get wrong.
        REQUIRE(rig.answers.awaiting());
        REQUIRE(rig.answers.book().outstanding() == 1);
        REQUIRE(slow_state.held);
    } // <-- the owner dies here, with a row in flight

    // THE LOCAL BOOK SAYS WHAT THIS HOST IS DOING, and there is no longer a this-host to
    // be doing it: the record is gone because nothing can ever read it again.
    CHECK_FALSE(rig.answers.awaiting());
    CHECK(rig.answers.book().outstanding() == 0);
    CHECK(rig.answers.asking() == 0);

    // ...AND THE FAR END IS UNTOUCHED. No message was sent, no DeferredAnswer was
    // revoked, and the respondent holds the answer right it held a moment ago. This is
    // the whole of the difference between forgetting and cancelling, and Loom has a word
    // for only one of them.
    REQUIRE(slow_state.held);
    CHECK(slow_state.answer.valid());

    // ...AND A REAL LOAD, THROUGH THE REAL MANAGER, SETTLES NORMALLY BESIDE IT. The next
    // conversation is a new one -- the forgotten number is not handed back -- and it ends
    // on its own answer.
    const load::Executed done =
        rig.realize(plan_of({weaves("zengine-plain-weave", "test.plain")}));
    CHECK(done.ok);
    CHECK(rig.weave_of(done, "zengine-plain-weave").valid());
    CHECK(rig.answers.answered);
    CHECK_FALSE(rig.answers.refused);
    CHECK_FALSE(rig.answers.awaiting());
    CHECK(rig.answers.book().outstanding() == 0);
    REQUIRE(slow_state.held); // and the abandoned conversation's answer is still owed
}

TEST_CASE("QR-10: a late answer to a forgotten load settles nothing -- including the NEXT ask") {
    // THE SHARPEST CORRELATION WITNESS IN THE PHASE, and it is deliberately driven by ONE
    // respondent: if the two conversations were told apart by their SENDER, this case
    // would pass for a reason that has nothing to do with what it claims.
    loom::Switchboard bus;
    SlowAnswers slow_state;
    load::BootAnswers answers;
    const loom::WeaveId slow = loom::mount<SlowManager>(bus, slow_state);
    loom::Grant operate;
    operate.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, slow);
    const loom::WeaveId booter =
        loom::mount_granted<load::PlanBooter>(bus, std::move(operate), answers);

    // ---- ASK A, DEFERRED, THEN LOCALLY FORGOTTEN --------------------------------
    const std::uint64_t a = answers.ask(slow);
    bus.send_as(booter, slow,
                loom::Message(loom::to_value(loom::LoadWeave{"A", "A", "test.a"}), booter,
                              booter, a));
    for (int turn = 0; turn < 4 && answers.awaiting(); ++turn) {
        bus.pump_pending();
    }
    REQUIRE(slow_state.held);
    answers.stopped_waiting();
    REQUIRE_FALSE(answers.awaiting());
    REQUIRE(answers.book().outstanding() == 0);

    // ---- ASK B, A GENUINELY NEW CONVERSATION -----------------------------------
    const std::uint64_t b = answers.ask(slow);
    // FORGETTING DOES NOT HAND A NUMBER BACK. If it did, the answer A's respondent is
    // still holding would settle B -- which is the whole reason this matters.
    CHECK(b != a);
    CHECK(b > a);
    CHECK(answers.awaiting());
    CHECK(answers.book().outstanding() == 1);

    // ---- NOW SPEND A'S LATE ANSWER ---------------------------------------------
    // Nudge makes the respondent spend what it deferred; the bus restores the correlation
    // the ASK was delivered with, so this fixture cannot choose it. It is A's number,
    // arriving after A stopped being anybody's business here.
    (void)bus.send(slow,
                   loom::Message(loom::to_value(Nudge{}), loom::WeaveId{}, loom::WeaveId{}, 0));
    for (int turn = 0; turn < 6; ++turn) {
        bus.pump_pending();
    }
    CHECK_FALSE(slow_state.held);  // it really was spent -- the wall was knocked on
    CHECK_FALSE(answers.answered); // ...and it settled nothing
    CHECK(answers.weave == 0);
    CHECK_FALSE(answers.refused);
    CHECK(answers.book().outstanding() == 1); // B, untouched
    CHECK(answers.awaiting());
    CHECK(answers.asking() == b);

    // ...AND B STILL SETTLES ON B. The late arrival neither closed it nor poisoned it.
    (void)bus.send(slow, loom::Message(loom::to_value(SayAgain{static_cast<std::int64_t>(b),
                                                              "77"}),
                                       loom::WeaveId{}, loom::WeaveId{}, 0));
    for (int turn = 0; turn < 4 && answers.awaiting(); ++turn) {
        bus.pump_pending();
    }
    CHECK(answers.answered);
    CHECK(answers.weave == 77u);
    CHECK_FALSE(answers.awaiting());
    CHECK(answers.book().outstanding() == 0);

    // ...and A's number is still nobody's: a copy of that late answer is inert forever.
    CHECK_FALSE(answers.settles(a, slow));
}

TEST_CASE("QR-10: repeated local abandonment does not fill the book, and refuses nothing") {
    // MORE ROUNDS THAN THE CAPACITY FRIC-2 NEEDED (four), so a book that still
    // accumulated would have been refusing loads by round five.
    //
    // EACH ROUND IS NOW AN OWNER'S WHOLE LIFE (BOOT-0). The abandonment site moved from
    // an expiring fuse to the destructor of an owner that ceased to exist with a row in
    // flight; the claim is the same one and it is measured the same way -- sixteen
    // abandoned conversations, and a book that is empty before and after every one.
    PlanRig rig;
    SlowAnswers slow_state;
    const loom::WeaveId slow = loom::mount<SlowManager>(rig.bus, slow_state);
    loom::WeaveId stalled_booter{};
    load::PlanBooter& voice = mount_booter_in(rig.bus, slow, rig.answers, stalled_booter);

    constexpr int kRounds = 16;
    for (int i = 0; i < kRounds; ++i) {
        REQUIRE(rig.answers.book().outstanding() == 0); // the baseline, before each ask
        {
            load::PlanExecutor stalled{rig.bus,  rig.catalog, rig.operators, voice,
                                       slow,     rig.answers,
                                       [](const std::string& stem) {
                                           return stage().so(stem);
                                       }};
            stalled.begin(plan_of({weaves("zengine-plain-weave", "test.stalled")}));
            rig.drain(4);
            REQUIRE(stalled.state() == load::Realization::Loading);
            // NO ROUND MINTS A REFUSAL. Neither the fuse sentence that is gone nor the
            // capacity sentence FRIC-2 needed: nothing refused, because nothing did.
            REQUIRE(stalled.refusal().empty());
            REQUIRE(rig.answers.book().outstanding() == 1);
        }
        CHECK(rig.answers.book().outstanding() == 0); // ...and back to baseline

        // SPEND THE HELD ANSWER BETWEEN ROUNDS, for a reason that is about the OTHER
        // resource entirely: this fixture's respondent holds one DeferredAnswer at a time
        // and drops the previous one on the floor, and a dropped answer right costs a slot
        // in Loom's own bounded deferred table until its owner dies. That table is NOT
        // what this case is about, and letting it fill would let a red here be read as a
        // claim about the asker's book. So each round's answer is spent, arrives late, and
        // settles nothing -- which is also the claim above, sixteen times.
        (void)rig.bus.send(slow, loom::Message(loom::to_value(Nudge{}), loom::WeaveId{},
                                               loom::WeaveId{}, 0));
        rig.drain(4);
        CHECK_FALSE(rig.answers.answered);
        CHECK(rig.answers.book().outstanding() == 0);
    }

    // AND THE NEXT LOAD STILL WORKS, through the real Manager, on its own answer.
    const load::Executed done =
        rig.realize(plan_of({weaves("zengine-plain-weave", "test.plain")}));
    CHECK(done.ok);
    CHECK(rig.answers.answered);
    CHECK_FALSE(rig.answers.awaiting());
    CHECK(rig.answers.book().outstanding() == 0);
}

TEST_CASE("QR-10: the book refuses a second conversation rather than displacing the first") {
    // THE BOUND IS ONE, AND THAT IS AN ASSERTION RATHER THAN AN ACCIDENT. Nothing in this
    // adapter opens a conversation while another is outstanding -- load_weave asks and
    // then waits -- so if that ever changed the book would say so instead of quietly
    // obliging. An asker refuses the NEW ask and keeps what it is waiting on; shedding
    // the oldest is loom::relay's policy, for a participant with no question of its own.
    loom::Switchboard bus;
    SlowAnswers slow_state;
    load::BootAnswers answers;
    const loom::WeaveId slow = loom::mount<SlowManager>(bus, slow_state);

    const std::uint64_t first = answers.ask(slow);
    REQUIRE(first != 0);
    CHECK(answers.book().full());

    const std::uint64_t second = answers.ask(slow);
    CHECK(second == 0);                       // refused, and nothing to send
    CHECK(answers.book().outstanding() == 1); // ...with the first untouched
    CHECK(answers.book().entries().front().correlation == first);
}

TEST_CASE("QR-10: a load conversation that cannot be opened refuses, and never succeeds quietly") {
    // THE GUARD THE CAPACITY REFUSAL LEFT BEHIND. The book is empty at every ask(), so
    // the one thing still able to refuse a conversation is the respondent -- and an
    // owner handed no valid Weave Manager must say so. Deleting this branch along with
    // the workaround would have been the worse defect of the two: with no conversation
    // open, an owner that sent anyway would sit in `Loading` forever waiting to be woken
    // by an answer it could never recognise.
    PlanRig rig;
    load::BootAnswers answers;
    loom::WeaveId booter{};
    load::PlanBooter& voice = mount_booter_in(rig.bus, rig.manager, answers, booter);
    load::PlanExecutor nowhere{rig.bus,         rig.catalog, rig.operators, voice,
                               loom::WeaveId{}, answers,
                               [](const std::string& stem) { return stage().so(stem); }};

    nowhere.begin(plan_of({weaves("zengine-plain-weave", "test.nowhere")}));
    const load::Executed done = nowhere.outcome();

    // REFUSED INSIDE `begin`, SYNCHRONOUSLY, because nothing was ever asked of anybody:
    // this is not a fact that has to come back from a host turn.
    CHECK(nowhere.state() == load::Realization::Failed);
    CHECK_FALSE(done.ok);
    CHECK(done.refusal.find("no load conversation could be opened") != std::string::npos);
    CHECK(done.refusal.find("no load was commanded") != std::string::npos);
    CHECK(done.resolved.empty());
    // NOT AN ANSWER, and not a refusal anybody made: nothing was ever sent.
    CHECK_FALSE(answers.answered);
    CHECK_FALSE(answers.refused);
    CHECK(answers.reason.empty());
    CHECK(answers.book().outstanding() == 0);
}

TEST_CASE("FRIC-2: dropping a load conversation is local, and claims nothing of the far end") {
    loom::Switchboard bus;
    SlowAnswers slow_state;
    load::BootAnswers answers;
    const loom::WeaveId slow = loom::mount<SlowManager>(bus, slow_state);
    loom::Grant operate;
    operate.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, slow);
    const loom::WeaveId booter =
        loom::mount_granted<load::PlanBooter>(bus, std::move(operate), answers);

    const std::uint64_t mine = answers.ask(slow);
    bus.send_as(booter, slow,
                loom::Message(loom::to_value(
                                  loom::LoadWeave{"unanswered", "unanswered", "test.unanswered"}),
                              booter, booter, mine));
    for (int turn = 0; turn < 4 && answers.awaiting(); ++turn) {
        bus.pump_pending();
    }
    REQUIRE(slow_state.held);

    // `settled()` is the adapter word for "there is no longer one outstanding". It removes
    // the LOCAL record and that is the only thing it does: nothing crosses the bus,
    // because Loom has no cancellation vocabulary for it to cross with.
    answers.settled();
    CHECK_FALSE(answers.awaiting());
    CHECK(answers.book().outstanding() == 0);
    REQUIRE(slow_state.held); // the respondent was never told anything

    // ...AND THE ANSWER STILL ARRIVES, settles nothing, and does not put the record back.
    // An answer to a conversation this host stopped tracking is a true fact about the
    // world; it is simply not this host business any more.
    (void)bus.send(slow,
                   loom::Message(loom::to_value(Nudge{}), loom::WeaveId{}, loom::WeaveId{}, 0));
    for (int turn = 0; turn < 4; ++turn) {
        bus.pump_pending();
    }
    CHECK_FALSE(answers.answered);
    CHECK(answers.weave == 0);
    CHECK_FALSE(answers.awaiting());
    CHECK(answers.book().outstanding() == 0);
}

// =============================================================================
// 11. REALIZATION IS LIVING NOW (BOOT-0) — it survives the frame that started it
// =============================================================================
//
// Every case above this line drives a plan to a conclusion and then asks what came of
// it. That is what a plan executor was: a call that did not return until the whole
// project had settled, because the continuation of a weave load was a stack frame and
// a stack frame cannot be put down. To hear its own answer it turned the bus itself.
//
// The cases below are about the MIDDLE. A persistent owner issues one load and RETURNS
// TO ITS HOST with a row in flight, so for the first time there is an observable state
// between "not started" and "finished", and things can be asked and done while it
// holds:
//
//   §11.1  begin() comes back with the project unrealized, and the HOST's own turns
//          are what carry it -- there is no other clock
//   §11.2  ordinary unrelated work is delivered while a row is outstanding, for as
//          long as it stays outstanding
//   §11.3  authored order is still strict and serial: one row in flight, ever
//   §11.4  the operator offer's custody spans host turns, and the artifact that needs
//          it still gets it
//   §11.5  loaded is not live: the control door's activation is why a load's ANSWER is
//          the row's completion fact
//   §11.6  a refusal mid-flight rolls back its own row and stops, with everything
//          before it standing
//   §11.7  the arrangement projection is truthful at every point on that timeline
//
// ⚠ EVERY TURN ANY PLAN IN THIS FILE TAKES IS SPENT BY THE TEST. `PlanRig::realize`
// and the loops below are the caller being a host; `PlanExecutor` has no pump, no
// drain, no wait and no fuse. `test_operator_provider.cpp` reads the source for those
// words, which is the half a running test cannot prove.

// ---- §11.1 the boundary itself -------------------------------------------------

TEST_CASE("BOOT-0: begin() returns with the project UNREALIZED, and says exactly where") {
    PlanRig rig;
    rig.executor.begin(plan_of({provides("zengine-operators-basic"),
                                weaves("zengine-plain-weave", "test.plain")}));

    // ⭐ NOT ONE DISPATCH TURN HAS BEEN SPENT, and the state is already interesting.
    // Row 0 is a provider mount -- synchronous, host-native, owed to nobody -- so it is
    // DONE. Row 1 is a conversation, so it is COMMANDED and unanswered, and the owner
    // came back rather than turning the crank until it heard itself.
    CHECK(rig.executor.state() == load::Realization::Loading);
    CHECK(rig.executor.position() == 1);
    REQUIRE(rig.executor.resolved().size() == 1);
    CHECK(rig.executor.resolved()[0].stem == "zengine-operators-basic");
    CHECK(rig.catalog.mounted("zengine.operators.basic"));

    // ...AND THE ROW IN FLIGHT HAS NOT HAPPENED. The command is on the queue and
    // nothing has delivered it.
    CHECK_FALSE(rig.kernel.is_loaded("zengine-plain-weave"));
    CHECK(rig.bus.pending() > 0);
    CHECK(rig.answers.awaiting());
    CHECK(rig.executor.asking() != 0);
    CHECK(rig.executor.state_of("zengine-plain-weave") == load::RowState::Loading);

    // ...AND `outcome()` REFUSES TO CALL THAT A SUCCESS. `ok` is completion, not
    // absence-of-failure: a plan with a row in flight has finished nothing.
    CHECK_FALSE(rig.executor.outcome().ok);
    CHECK(rig.executor.refusal().empty());

    // THE HOST'S OWN TURNS ARE THE ONLY CLOCK. Nothing else in this process is running.
    rig.drain(8);
    CHECK(rig.executor.state() == load::Realization::Complete);
    CHECK(rig.executor.position() == 2);
    CHECK(rig.kernel.is_loaded("zengine-plain-weave"));
    CHECK(rig.executor.outcome().ok);
}

TEST_CASE("BOOT-0: a plan of provider-only rows finishes inside begin(), turning nothing") {
    // THE OTHER SIDE OF THE SAME LAW. `advance()` performs every transition it can
    // already know the answer to before it returns, so a project that owes nobody
    // anything is realized with the queue untouched -- and STILL without a pump.
    PlanRig rig;
    const std::size_t before = rig.bus.pending();
    // TWO PROVIDERS THAT SUPPLY DIFFERENT POWERS, so the plan is about the boundary and
    // not about a collision: `zengine-provider-a` carries three identities nothing else
    // in this suite names.
    rig.executor.begin(plan_of({provides("zengine-operators-basic"),
                                provides("zengine-provider-a")}));

    CHECK(rig.executor.state() == load::Realization::Complete);
    CHECK(rig.executor.position() == 2);
    CHECK(rig.executor.outcome().ok);
    CHECK(rig.catalog.providers().size() == 2);
    // NOTHING WAS SENT AND NOTHING WAS ASKED: a provider mount is not a conversation.
    CHECK(rig.bus.pending() == before);
    CHECK_FALSE(rig.answers.awaiting());
}

// ---- §11.2 the host is not blocked ---------------------------------------------

TEST_CASE("BOOT-0: unrelated work is delivered while a plan row is still outstanding") {
    // ⭐ THE WITNESS THAT SEPARATES "MOVED THE LOOP" FROM "DELETED IT". If `begin()`
    // still turned the bus until its answer came, nothing else could be delivered in
    // between -- the process would be inside one call. So: point the owner at a
    // respondent that DEFERS forever (Loom's ANS-02, no timing involved), and then do
    // ordinary host work, at length, while the row hangs.
    PlanRig rig;
    SlowAnswers slow_state;
    load::BootAnswers answers;
    const loom::WeaveId slow = loom::mount<SlowManager>(rig.bus, slow_state);
    loom::WeaveId booter{};
    load::PlanBooter& voice = mount_booter_in(rig.bus, slow, answers, booter);

    std::int64_t nudges = 0;
    const loom::WeaveId bystander = loom::mount<Bystander>(rig.bus, nudges);

    load::PlanExecutor stalled{rig.bus, rig.catalog, rig.operators, voice, slow, answers,
                               [](const std::string& stem) { return stage().so(stem); }};

    // ⭐ ORDINARY WORK ALREADY ON THE QUEUE WHEN REALIZATION BEGINS -- the sharpest form
    // of "the owner does not drive Loom", and the one that holds even against a
    // respondent that never answers. If `begin()` turned the crank under ANY spelling,
    // this delivery would have happened by the time it returned. The host has not had
    // its turn yet, so it must not have.
    (void)rig.bus.send(bystander, loom::Message(loom::to_value(Nudge{}), loom::WeaveId{},
                                                loom::WeaveId{}, 0));
    stalled.begin(plan_of({weaves("zengine-plain-weave", "test.stalled")}));
    CHECK(nudges == 0);

    rig.drain(4);
    CHECK(nudges == 1); // ...and the host's own turn delivered it, as it always would
    REQUIRE(stalled.state() == load::Realization::Loading);
    REQUIRE(slow_state.held); // the answer is genuinely owed and genuinely absent

    // ORDINARY HOST WORK, WHILE THE PROJECT IS HALF-REALIZED. Twenty deliveries to a
    // participant that has never heard of a load plan, every one of them arriving.
    for (int i = 0; i < 20; ++i) {
        (void)rig.bus.send(bystander, loom::Message(loom::to_value(Nudge{}), loom::WeaveId{},
                                                    loom::WeaveId{}, 0));
        rig.drain(1);
        // ...AND THE ROW IS STILL EXACTLY WHERE IT WAS. It is not failing, not timing
        // out and not being retried: it is unanswered, which is what is true.
        REQUIRE(stalled.state() == load::Realization::Loading);
    }
    CHECK(nudges == 21);
    CHECK(stalled.refusal().empty());
    CHECK(answers.awaiting());

    // ...AND WHEN THE ANSWER FINALLY COMES, REALIZATION CONTINUES FROM THERE. Nothing
    // resumed a wait, because nothing was waiting.
    (void)rig.bus.send(slow, loom::Message(loom::to_value(Nudge{}), loom::WeaveId{},
                                           loom::WeaveId{}, 0));
    rig.drain(6);
    CHECK(stalled.state() == load::Realization::Complete);
    CHECK(answers.weave == kSlowWeaveId);
    CHECK_FALSE(answers.awaiting());
    CHECK(nudges == 21); // and the bystander was not disturbed by any of it
}

// ---- §11.3 authored order, still strict and still serial -----------------------

TEST_CASE("BOOT-0: three mixed rows advance in AUTHORED order, one in flight at a time") {
    // ⭐ PERSISTENCE IS NOT PERMISSION FOR CONCURRENCY. The owner could now, in
    // principle, have several conversations open at once. It must not, and this walks
    // the whole timeline turn by turn to say so: at every instant, at most one row is
    // `loading`, every row before the cursor is `resolved`, and every row after it has
    // not been touched.
    //
    //   row A  provider-only        -- settles inside begin(), synchronously
    //   row B  provider + weave     -- mounts, then commands a load, then returns
    //   row C  weave-only           -- must not begin until B has settled
    PlanRig rig;
    rig.executor.begin(plan_of({provides("zengine-operators-basic"),
                                both("zengine-timer", tmr::kTimerRole),
                                weaves("zengine-plain-weave", "test.plain")}));

    // A IS DONE AND B IS IN FLIGHT, before a single turn.
    REQUIRE(rig.executor.position() == 1);
    REQUIRE(rig.executor.state() == load::Realization::Loading);
    CHECK(rig.executor.state_of("zengine-operators-basic") == load::RowState::Resolved);
    CHECK(rig.executor.state_of("zengine-timer") == load::RowState::Loading);
    CHECK(rig.executor.state_of("zengine-plain-weave") == load::RowState::Authored);
    // ...AND C HAS NOT BEEN TOUCHED IN ANY WAY A KERNEL COULD SEE.
    CHECK_FALSE(rig.kernel.is_loaded("zengine-plain-weave"));

    // WALK EVERY TURN. `seen_c_loading_before_b_resolved` is the whole falsifier: if
    // the owner ever ran ahead, this catches the instant it did.
    bool c_began_early = false;
    bool two_at_once = false;
    std::size_t cursor = rig.executor.position();
    for (int turn = 0; turn < 24; ++turn) {
        rig.bus.pump_pending();
        const load::RowState b = rig.executor.state_of("zengine-timer");
        const load::RowState c = rig.executor.state_of("zengine-plain-weave");
        if (c != load::RowState::Authored && b != load::RowState::Resolved) {
            c_began_early = true;
        }
        if (b == load::RowState::Loading && c == load::RowState::Loading) {
            two_at_once = true;
        }
        // THE CURSOR NEVER GOES BACKWARDS.
        REQUIRE(rig.executor.position() >= cursor);
        cursor = rig.executor.position();
    }
    CHECK_FALSE(c_began_early);
    CHECK_FALSE(two_at_once);

    CHECK(rig.executor.state() == load::Realization::Complete);
    CHECK(rig.executor.position() == 3);
    REQUIRE(rig.executor.resolved().size() == 3);
    // AUTHORED ORDER IS THE ORDER THEY WERE PUT INTO THE RUNTIME, which is also what a
    // later reversal would walk backwards.
    CHECK(rig.executor.resolved()[0].stem == "zengine-operators-basic");
    CHECK(rig.executor.resolved()[1].stem == "zengine-timer");
    CHECK(rig.executor.resolved()[2].stem == "zengine-plain-weave");
    // ...and the book is empty, because every conversation ended on its own answer.
    CHECK_FALSE(rig.answers.awaiting());
    CHECK(rig.answers.book().outstanding() == 0);
}

TEST_CASE("BOOT-0: an OVERLAY row still begins only after the row it covers has settled") {
    // AUTHORED ORDER IS WHY AN OVERLAY IS VALID (LOAD-0), and an overlay is where the
    // order genuinely bites: it installs over what is already there, so a row that ran
    // early would install over nothing and the ordinary mount would then collide with
    // it. Serialization is what makes that deterministic, and persistence did not
    // relax it.
    PlanRig rig;
    rig.executor.begin(plan_of({provides("zengine-operators-basic"),
                                both("zengine-timer", tmr::kTimerRole),
                                provides("zengine-provider-min", op::MountMode::Overlay)}));

    // THE OVERLAY HAS NOT MOUNTED WHILE THE TIMER'S LOAD IS OUTSTANDING, even though
    // its own step is synchronous and could have been performed at any moment.
    //
    // ⚠ THE IDENTITY IS THE ONE THE ARTIFACT DECLARES ABOUT ITSELF, never the stem: a
    // check written against `zengine-provider-min` would be asking the catalog about a
    // provider no artifact has ever claimed to be, and would pass for the wrong reason
    // at both ends of this case.
    REQUIRE(rig.executor.state() == load::Realization::Loading);
    CHECK(rig.executor.state_of("zengine-provider-min") == load::RowState::Authored);
    CHECK_FALSE(rig.catalog.mounted(kMinProvider));

    rig.drain(12);
    CHECK(rig.executor.state() == load::Realization::Complete);
    CHECK(rig.executor.state_of("zengine-provider-min") == load::RowState::Resolved);
    CHECK(rig.catalog.mounted(kMinProvider));
    // ...AND IT WENT ON TOP, which is the fact the authored position buys.
    REQUIRE(rig.executor.resolved().size() == 3);
    CHECK(rig.executor.resolved()[2].stem == "zengine-provider-min");
}

// ---- §11.4 the offer's custody now spans host turns ----------------------------

TEST_CASE("BOOT-0: the operator offer is STILL STANDING when create() runs, host turns later") {
    // ⭐ THE CENTRAL LIFETIME PROOF OF THE PHASE. `op::OperatorOffer` is neither
    // copyable nor movable and its DESTRUCTOR is the withdrawal, so in the old
    // straight-line executor its bracket was literally a `{ }` inside the call that
    // waited -- which is the real reason that call could not return. It is a
    // `std::optional` member now, and its bracket spans an unbounded number of the
    // host's own turns.
    //
    // ---- THE INSTRUMENT, AND WHY IT IS A REFUSAL -------------------------------
    //
    // A Timer that IS offered a host must spend that host's `timer.normalize_delay`
    // and REFUSES when the host publishes none; a Timer that met NO offer falls back
    // to its own local catalog and loads perfectly happily. So a plan that offers an
    // incomplete host and gets a refusal has proved the offer was in force inside
    // `create()` -- which the Kernel calls several deliveries below the command, and
    // which nothing in this process can get between.
    //
    // A "the delay it stored is this host's number" check would NOT be the witness
    // here, and the difference matters: with the Timer's own provider mounted, the
    // host's rule and the artifact's local copy are the SAME rule, so both paths
    // answer identically. The refusal is the only thing the two differ on.
    SUBCASE("an offer that reached create() refuses a host that cannot satisfy it") {
        PlanRig rig;
        rig.executor.begin(plan_of({provides("zengine-operators-basic"),
                                    weaves("zengine-timer", tmr::kTimerRole)}));

        // RETURNED, WITH THE ROW IN FLIGHT AND THE OFFER HELD.
        REQUIRE(rig.executor.state() == load::Realization::Loading);

        // ...AND `create()` HAPPENS SEVERAL TURNS FROM HERE. Counted rather than
        // assumed, because "the offer outlived the frame that made it" is exactly the
        // claim: one turn would not have been a claim about anything.
        int turns = 0;
        while (rig.executor.state() == load::Realization::Loading && turns < 24) {
            rig.bus.pump_pending();
            ++turns;
        }
        CHECK(turns > 1);

        CHECK(rig.executor.state() == load::Realization::Failed);
        CHECK(rig.executor.refusal().find("artifact 'zengine-timer'") != std::string::npos);
        CHECK(rig.executor.refusal().find("weave load refused") != std::string::npos);
    }
    SUBCASE("...and the same row with its provider mounted takes the offer and runs") {
        // THE CONTROL. Only one authored word differs -- the row also asks for provider
        // participation -- and now the host CAN satisfy what the Timer validates. The
        // offer is the same offer, held across the same turns.
        PlanRig rig;
        rig.executor.begin(plan_of({provides("zengine-operators-basic"),
                                    both("zengine-timer", tmr::kTimerRole)}));
        REQUIRE(rig.executor.state() == load::Realization::Loading);
        rig.drain(12);
        REQUIRE(rig.executor.state() == load::Realization::Complete);
        REQUIRE(rig.executor.resolved().size() == 2);

        const load::ResolvedArtifact& timer = rig.executor.resolved()[1];
        CHECK(timer.offer == op::OfferOutcome::Offered);
        // ...AND THE RUNNING SERVICE ANSWERS THROUGH THIS HOST, twice, long after the
        // bracket closed: the instance kept its own copy of the table, which is exactly
        // what the unconditional withdrawal is designed to leave safe.
        CHECK(rig.scheduled_delay(timer.weave, "beat", kAuthoredDelay, true) == kHonestAnswer);
        CHECK(rig.scheduled_delay(timer.weave, "again", kAuthoredDelay, true) == kHonestAnswer);
    }
}

TEST_CASE("BOOT-0: the offer is withdrawn BEFORE the next authored row begins") {
    // THE OTHER HALF OF CUSTODY, and the one a persistent holder could get wrong: an
    // offer that outlived its row would still be standing when the NEXT artifact was
    // opened, and that artifact would be loaded under a handoff nobody authored for it.
    //
    // MEASURED THROUGH THE ROW RECORDS, which is where the outcome of each handoff
    // lives: the Timer's row says the offer was taken, and the ordinary weave loaded
    // after it says it was never a consumer -- a `NotAConsumer` that is a fact about
    // ITS OWN offer, made and withdrawn around ITS OWN load.
    PlanRig rig;
    const load::Executed done = rig.realize(plan_of({provides("zengine-operators-basic"),
                                                     both("zengine-timer", tmr::kTimerRole),
                                                     weaves("zengine-plain-weave", "test.plain")}));
    REQUIRE_MESSAGE(done.ok, done.refusal);
    REQUIRE(done.resolved.size() == 3);
    CHECK(done.resolved[1].offer == op::OfferOutcome::Offered);
    CHECK(done.resolved[2].offer == op::OfferOutcome::NotAConsumer);
    // ...and the Timer loaded under the first is still host-backed afterwards.
    CHECK(rig.scheduled_delay(rig.weave_of(done, "zengine-timer"), "beat", kAuthoredDelay,
                              true) == kHonestAnswer);
}

// ---- §11.5 loaded is not live --------------------------------------------------

TEST_CASE("BOOT-0: the row's completion fact is the ANSWER, because the door activates first") {
    // ⭐ WHY THE OWNER MUST NOT SHORTCUT THROUGH `Kernel::load`. The control door does
    // three things in order -- load, announce `zen.Activated`, answer -- and only the
    // door can do the middle one: `Switchboard::announce_as` is private and the only
    // public route is through a `Mail`, i.e. from inside a delivery. A host in `main()`
    // can mint the authority and has no `Mail`, so it cannot make a loaded weave live.
    //
    // SO A ROW IS NOT DONE WHEN THE KERNEL HAS IT. It is done when the answer arrives,
    // which is strictly later and is the only fact that implies the whole sequence.
    PlanRig rig;
    rig.executor.begin(plan_of({provides("zengine-operators-basic"),
                                both("zengine-timer", tmr::kTimerRole)}));

    // WALK TO THE INSTANT THE KERNEL FIRST HAS IT, and look at the owner.
    bool saw_loaded_before_settled = false;
    for (int turn = 0; turn < 24; ++turn) {
        rig.bus.pump_pending();
        if (rig.kernel.is_loaded("zengine-timer") &&
            rig.executor.state_of("zengine-timer") == load::RowState::Loading) {
            saw_loaded_before_settled = true;
        }
        if (rig.executor.state() != load::Realization::Loading) {
            break;
        }
    }
    // KERNEL-LOADED CAME FIRST AND THE OWNER DID NOT BELIEVE IT. If the owner had been
    // reading `is_loaded`, it would have called the row done a delivery too early --
    // with `zen.Activated` still queued.
    CHECK(saw_loaded_before_settled);

    rig.drain(8);
    REQUIRE(rig.executor.state() == load::Realization::Complete);

    // ⭐ AND THE ARTIFACT IS BEHAVIOURALLY ALIVE, not merely registered. A Timer that
    // never received `zen.Activated` never authors its beat chain; this one schedules,
    // stores and hands back a delay, which is a thing only a live service does.
    const loom::WeaveId timer = rig.executor.resolved()[1].weave;
    CHECK(rig.kernel.is_loaded("zengine-timer"));
    CHECK(rig.scheduled_delay(timer, "beat", kAuthoredDelay, true) == kHonestAnswer);
}

TEST_CASE("BOOT-0: a direct Kernel::load produces a REGISTERED Timer that never breathes") {
    // ⭐ THE FALSIFIER FOR THE CASE ABOVE, and the reason the owner keeps commanding the
    // Manager instead of calling the loader it could reach. Same artifact, same Kernel,
    // same process -- and the shortcut's Timer is a dead arrangement.
    //
    // It is the same shape a mutation of the owner would produce, run as an ordinary
    // case rather than as a source edit, because the two paths can stand side by side.
    PlanRig rig;
    REQUIRE(rig.realize(plan_of({provides("zengine-operators-basic"),
                                 provides("zengine-timer")}))
                .ok);

    const loom::LoadResult direct =
        rig.kernel.load("zengine-timer-direct", stage().so("zengine-timer"), "test.dead-timer");
    REQUIRE(direct.ok);
    CHECK(rig.kernel.is_loaded("zengine-timer-direct"));

    // REGISTERED, ROUTABLE, ROLE-BOUND -- AND IT NEVER RECEIVED `zen.Activated`, so it
    // authored no beat chain and there is nothing for it to have scheduled. The
    // instrument that answers `kHonestAnswer` for a live service answers with the
    // miss value here, because no letter ever comes back.
    rig.drain(8);
    CHECK(rig.scheduled_delay(direct.id, "beat", kAuthoredDelay, true) != kHonestAnswer);
}

// ---- §11.6 a refusal mid-flight ------------------------------------------------

TEST_CASE("BOOT-0: a row that refuses AFTER the host resumed rolls back only its own mount") {
    // ⭐ ARTIFACT-LEVEL ATOMICITY, WITHOUT A STACK FRAME TO UNWIND. The rollback used to
    // be the tail of a function whose caller was still on the stack; it is now performed
    // inside whatever delivery brought the refusal, several host turns after the call
    // that started the plan returned.
    //
    // THE HALFWAY ROW IS `zengine-timer` DECLARED PROVIDER+WEAVE, asked for under a role
    // an EARLIER authored row already took: the mount succeeds and the load does not,
    // which is the only shape in which one artifact can be half-participating.
    PlanRig rig;
    rig.executor.begin(plan_of({provides("zengine-operators-basic"),
                                weaves("zengine-plain-weave", tmr::kTimerRole),
                                both("zengine-timer", tmr::kTimerRole),
                                weaves("zengine-provider-a", "test.never")}));

    // ---- WALK TO THE HALFWAY INSTANT, which only exists because realization returns
    // to its host between steps. The row's provider is mounted; its weave is a
    // conversation nobody has answered yet.
    bool saw_half_participating = false;
    for (int turn = 0; turn < 24; ++turn) {
        if (rig.executor.state_of("zengine-timer") == load::RowState::Loading &&
            rig.catalog.mounted("zengine.timer")) {
            saw_half_participating = true;
            break;
        }
        rig.bus.pump_pending();
    }
    CHECK(saw_half_participating);

    rig.drain(16);

    // THE REFUSAL ARRIVED AS AN ORDINARY ANSWER, several host turns after the call that
    // started the plan returned -- and stopped it.
    CHECK(rig.executor.state() == load::Realization::Failed);
    CHECK(rig.executor.refusal().find("artifact 'zengine-timer'") != std::string::npos);
    CHECK(rig.executor.refusal().find("weave load refused") != std::string::npos);
    CHECK(rig.executor.state_of("zengine-timer") == load::RowState::Refused);

    // THIS ROW'S OWN CONTRIBUTION IS GONE, rolled back inside that delivery.
    CHECK_FALSE(rig.catalog.mounted("zengine.timer"));
    CHECK(rig.catalog.find(tmr::kNormalizeDelay) == nullptr);
    // ...AND THE EARLIER ROWS ARE UNTOUCHED. No whole-plan transaction was built and
    // none is wanted: the host is told which artifact stopped it and what still stands.
    CHECK(rig.catalog.mounted("zengine.operators.basic"));
    REQUIRE(rig.executor.resolved().size() == 2);
    CHECK(rig.executor.resolved()[0].stem == "zengine-operators-basic");
    CHECK(rig.executor.resolved()[1].stem == "zengine-plain-weave");
    CHECK(rig.kernel.is_loaded("zengine-plain-weave"));

    // ...AND NOTHING AFTER IT WAS ATTEMPTED, however many turns the host spends.
    CHECK(rig.executor.state_of("zengine-provider-a") == load::RowState::Authored);
    CHECK_FALSE(rig.kernel.is_loaded("zengine-provider-a"));
    rig.drain(16);
    CHECK(rig.executor.state() == load::Realization::Failed);
    CHECK_FALSE(rig.kernel.is_loaded("zengine-provider-a"));
    // ...and the owner is not holding a conversation it will never hear the end of.
    CHECK_FALSE(rig.answers.awaiting());
}

TEST_CASE("BOOT-0: a stray answer cannot advance a plan that is waiting for its own") {
    // THE WALL IS THE ONE QR-9 AND FRIC-2 ALREADY BUILT, and BOOT-0 must not have
    // weakened it while moving the code past it. What is new is the CONSEQUENCE: a
    // stray that got through would no longer merely corrupt one wait -- it would drive
    // the whole rest of the project from inside somebody else's conversation.
    PlanRig rig;
    SlowAnswers slow_state;
    load::BootAnswers answers;
    const loom::WeaveId slow = loom::mount<SlowManager>(rig.bus, slow_state);
    loom::WeaveId booter{};
    load::PlanBooter& voice = mount_booter_in(rig.bus, slow, answers, booter);
    const loom::WeaveId stray = loom::mount<Stray>(rig.bus);

    load::PlanExecutor stalled{rig.bus, rig.catalog, rig.operators, voice, slow, answers,
                               [](const std::string& stem) { return stage().so(stem); }};
    stalled.begin(plan_of({weaves("zengine-plain-weave", "test.stalled"),
                           weaves("zengine-provider-min", "test.next")}));
    rig.drain(4);
    REQUIRE(stalled.state() == load::Realization::Loading);
    const std::uint64_t mine = stalled.asking();
    REQUIRE(mine != 0);

    SUBCASE("a WRONG CORRELATION from a participant this host admits advances nothing") {
        (void)rig.bus.send_as(stray, booter,
                              loom::Message(loom::to_value(loom::Result{"9999"}), stray, stray,
                                            kStrayCorrelation));
        rig.drain(4);
        CHECK(stalled.state() == load::Realization::Loading);
        CHECK(stalled.position() == 0);
        CHECK(stalled.resolved().empty());
        CHECK_FALSE(answers.answered);
        CHECK(answers.awaiting());
    }
    SUBCASE("a WRONG SENDER carrying the RIGHT correlation advances nothing either") {
        // THE LUCKY GUESS. The stray names this conversation exactly; what it cannot
        // do is be the weave that was asked, because the bus stamps the sender.
        (void)rig.bus.send_as(stray, booter,
                              loom::Message(loom::to_value(loom::Result{"9999"}), stray, stray,
                                            mine));
        rig.drain(4);
        CHECK(stalled.state() == load::Realization::Loading);
        CHECK(stalled.position() == 0);
        CHECK(stalled.resolved().empty());
        CHECK_FALSE(answers.answered);
        CHECK(answers.awaiting());
    }
    SUBCASE("...and the RIGHT respondent with the RIGHT correlation does advance it") {
        // THE CONTROL, so the two refusals above are not passing because nothing
        // could ever have settled this row.
        (void)rig.bus.send(slow, loom::Message(loom::to_value(Nudge{}), loom::WeaveId{},
                                               loom::WeaveId{}, 0));
        rig.drain(4);
        CHECK(stalled.position() == 1);
        CHECK(stalled.resolved().size() == 1);
        // ...and the plan carried on into row 1 BY ITSELF, from inside that delivery.
        //
        // ⚠ WHICH IS ALSO WHY `answers.answered` IS NOT WHAT IS ASKED HERE. It is
        // payload, cleared by the next `ask()` -- and the owner opened row 1's
        // conversation inside the very handler that settled row 0's, so by the time
        // anything can look, the record is already about the NEXT load. The cursor is
        // the fact; the payload field is one row's transient.
        CHECK(stalled.state() == load::Realization::Loading);
        CHECK(stalled.state_of("zengine-provider-min") == load::RowState::Loading);
        CHECK(answers.awaiting());
    }
}

// ---- §11.7 the projection, at every point on the timeline ----------------------

TEST_CASE("BOOT-0: the arrangement is truthful BEFORE, DURING and AFTER realization") {
    // ⭐ THE FOUR STATES, READ OFF ONE OWNER AT FOUR MOMENTS. Three of the four could
    // not be observed at all before this phase: the whole plan settled inside one call,
    // so a reader either saw nothing or saw the end.
    PlanRig rig;

    // ---- BEFORE: authored intent, and nothing else ---------------------------
    {
        const workshop::ResolvedArrangement said =
            workshop::describe_arrangement(rig.executor, "p.json");
        CHECK(said.plan == "p.json");
        CHECK(said.artifacts.empty()); // no plan has been begun; there is nothing to say
    }

    rig.executor.begin(plan_of({provides("zengine-operators-basic"),
                                both("zengine-timer", tmr::kTimerRole),
                                weaves("zengine-plain-weave", "test.plain")}));

    // ---- DURING: one resolved, one loading, one not attempted ----------------
    {
        REQUIRE(rig.executor.state() == load::Realization::Loading);
        const workshop::ResolvedArrangement said =
            workshop::describe_arrangement(rig.executor, "p.json");
        REQUIRE(said.artifacts.size() == 3);
        CHECK(said.artifacts[0].state == std::string(workshop::kResolvedToken));
        CHECK(said.artifacts[1].state == std::string(workshop::kLoadingToken));
        CHECK(said.artifacts[2].state == std::string(workshop::kAuthoredToken));

        // EXACT COUNTS, NEVER A PERCENTAGE: the list length is the denominator and the
        // tokens are the numerator, so a reader counts and nobody asserts.
        std::size_t resolved = 0;
        for (const workshop::ArtifactParticipation& a : said.artifacts) {
            resolved += a.state == std::string(workshop::kResolvedToken) ? 1u : 0u;
        }
        CHECK(resolved == 1);

        // A LOADING ROW KEEPS ITS AUTHORED INTENT AND CLAIMS NO RESOLVED FIELD. Its
        // provider IS mounted at this instant -- within one row the mount precedes the
        // load -- and the arrangement does not report it, because what came of the row
        // is not decided and a refusal would roll that mount back.
        CHECK(said.artifacts[1].authored_role == std::string(tmr::kTimerRole));
        CHECK(said.artifacts[1].authored_provider == std::string(load_persist::kModeNormal));
        CHECK(said.artifacts[1].provider.empty());
        CHECK(said.artifacts[1].weave == 0);
        CHECK(said.artifacts[1].offer.empty());

        // ...AND THE POWER IS VISIBLE THROUGH THE QUESTION THAT OWNS IT. Two questions,
        // two owners, two currencies -- the catalog is read live, so the mount that the
        // arrangement is not yet claiming is already in `ResolvedPowers`.
        CHECK(rig.catalog.mounted("zengine.timer"));
        CHECK(stack_of(workshop::describe_powers(rig.catalog), "timer.normalize_delay") !=
              nullptr);
    }

    rig.drain(16);

    // ---- AFTER: every row resolved, with what each one produced ---------------
    {
        REQUIRE(rig.executor.state() == load::Realization::Complete);
        const workshop::ResolvedArrangement said =
            workshop::describe_arrangement(rig.executor, "p.json");
        REQUIRE(said.artifacts.size() == 3);
        for (const workshop::ArtifactParticipation& a : said.artifacts) {
            CHECK(a.state == std::string(workshop::kResolvedToken));
        }
        CHECK(said.artifacts[1].provider == "zengine.timer");
        CHECK(said.artifacts[1].weave != 0);
        CHECK(said.artifacts[1].offer == std::string(workshop::kOfferedToken));
        // AND THE TIMER IS STILL ONE ROW, participating in two ways.
        CHECK(said.artifacts[1].artifact == "zengine-timer");
    }
}

TEST_CASE("BOOT-0: after a refusal the arrangement says which row stopped it, and which never ran") {
    PlanRig rig;
    const load::Executed done = rig.realize(plan_of({provides("zengine-operators-basic"),
                                                     weaves("zengine-not-on-this-disk", "test.gone"),
                                                     weaves("zengine-plain-weave", "test.plain")}));
    REQUIRE_FALSE(done.ok);

    const workshop::ResolvedArrangement said =
        workshop::describe_arrangement(rig.executor, "p.json");
    REQUIRE(said.artifacts.size() == 3);
    CHECK(said.artifacts[0].state == std::string(workshop::kResolvedToken));
    CHECK(said.artifacts[1].state == std::string(workshop::kRefusedToken));
    CHECK(said.artifacts[2].state == std::string(workshop::kAuthoredToken));
    // ⭐ THE TWO UNRESOLVED ROWS ARE DIFFERENT SENTENCES. Before BOOT-0 both were
    // `performed = false` and a maker could not tell the artifact that broke from the
    // ones nothing had tried.
    CHECK(said.artifacts[1].state != said.artifacts[2].state);
    // ...and neither invents a resolved fact.
    CHECK(said.artifacts[1].weave == 0);
    CHECK(said.artifacts[2].weave == 0);
    // WHILE THE FIRST ROW STILL STANDS, in the runtime and in the answer.
    CHECK(said.artifacts[0].provider == "zengine.operators.basic");
    CHECK(rig.catalog.mounted("zengine.operators.basic"));
}

TEST_CASE("BOOT-0: the DOOR answers `loading` across the real seam, mid-flight") {
    // THE PROJECTION ABOVE IS READ DIRECTLY; this is the same fact crossing the bus to
    // a participant in an office, which is how the loaded Introspection artifact gets
    // it. The row is held open by a respondent that defers, so the ask genuinely lands
    // in the middle rather than at a moment a turn count happened to produce.
    PlanRig rig;
    SlowAnswers slow_state;
    load::BootAnswers answers;
    const loom::WeaveId slow = loom::mount<SlowManager>(rig.bus, slow_state);
    loom::WeaveId booter{};
    load::PlanBooter& voice = mount_booter_in(rig.bus, slow, answers, booter);

    load::PlanExecutor stalled{rig.bus, rig.catalog, rig.operators, voice, slow, answers,
                               [](const std::string& stem) { return stage().so(stem); }};
    rig.mount_door_over(stalled, "mid.json");
    rig.mount_asker();

    stalled.begin(plan_of({provides("zengine-operators-basic"),
                           weaves("zengine-plain-weave", "test.stalled"),
                           weaves("zengine-provider-min", "test.never")}));
    rig.drain(4);
    REQUIRE(stalled.state() == load::Realization::Loading);

    rig.ask_arrangement();
    REQUIRE(rig.projected.arrangements.size() == 1);
    const workshop::ResolvedArrangement& said = rig.projected.arrangements.front();
    CHECK(said.plan == "mid.json");
    REQUIRE(said.artifacts.size() == 3);
    CHECK(said.artifacts[0].state == std::string(workshop::kResolvedToken));
    CHECK(said.artifacts[1].state == std::string(workshop::kLoadingToken));
    CHECK(said.artifacts[2].state == std::string(workshop::kAuthoredToken));
    // ...AND IT IS LOOM'S OWN ANSWER, attested rather than merely correlated.
    REQUIRE(rig.projected.attested.size() == 1);
    CHECK(rig.projected.attested.front());

    // THE ASK CHANGED NOTHING. A projection derives and does not act: the row is still
    // in flight and the plan has not moved.
    CHECK(stalled.state() == load::Realization::Loading);
    CHECK(stalled.position() == 1);
}
