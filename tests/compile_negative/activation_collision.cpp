// THE COMPILE-NEGATIVE FIXTURE for the TimedWeave activation wall.
//
// ONE SOURCE, THREE TARGETS, so the negative and its positive control cannot
// drift apart: they are the same weave, differing only in whether the forbidden
// handler is declared. A negative that is not paired with a control proves the
// build broke, not that it broke for the intended reason.
//
//   ZENGINE_CN_CASE=collision    the forbidden raw on(zen.Activated) -> MUST FAIL
//   ZENGINE_CN_CASE=hook         the same weave using the supported hook -> MUST COMPILE
//   ZENGINE_CN_CASE=missing_using   a domain handler with no `using` -> MUST FAIL
//
// The first and third must fail for DIFFERENT, named reasons: the two
// diagnostics distinguish "the base handlers are hidden entirely" from "the raw
// activation handler was illegally replaced", and the lane greps for each.

#include "timer/binding.hpp"
#include "timer/vocabulary.hpp"

#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <chrono>

using namespace std::chrono_literals;

namespace {

struct CnState {
    std::int64_t ticks = 0;
    ZEN_SHAPE(CnState, 1, ZEN_FIELD(ticks));
};

/// An ordinary domain message, so the `using` requirement is genuinely in play.
struct CnPoke {
    std::int64_t n = 0;
    ZEN_SHAPE(CnPoke, 1, ZEN_FIELD(n));
};

} // namespace

#if ZENGINE_CN_CASE == 1 // ---- collision: MUST FAIL --------------------------

class CnWeave : public zengine::timer::TimedWeave<CnWeave, CnState, loom::Accept<CnPoke>,
                                                  loom::Emit<>> {
public:
    using TimedWeave::on;

    CnWeave() : tick_(timers().repeat("cn.tick", 10ms, &CnWeave::on_tick)) {}

    void on(const CnPoke&, loom::Mail&) {}

    /// THE FORBIDDEN HANDLER. It would become the dispatch target and the
    /// bindings would never reconcile.
    void on(const loom::Activated&, loom::Mail&) { ++state_.ticks; }

private:
    void on_tick(const zengine::timer::TimerFired&, loom::Mail&) { ++state_.ticks; }
    Handle tick_;
};

#elif ZENGINE_CN_CASE == 2 // ---- the positive control: MUST COMPILE ----------

class CnWeave : public zengine::timer::TimedWeave<CnWeave, CnState, loom::Accept<CnPoke>,
                                                  loom::Emit<>> {
public:
    using TimedWeave::on;

    CnWeave() : tick_(timers().repeat("cn.tick", 10ms, &CnWeave::on_tick)) {}

    void on(const CnPoke&, loom::Mail&) {}

    /// The supported extension point — the same domain work, legally.
    void on_timed_activation(const loom::Activated&, loom::Mail&) { ++state_.ticks; }

private:
    void on_tick(const zengine::timer::TimerFired&, loom::Mail&) { ++state_.ticks; }
    Handle tick_;
};

#elif ZENGINE_CN_CASE == 3 // ---- missing `using`: MUST FAIL, differently ------

class CnWeave : public zengine::timer::TimedWeave<CnWeave, CnState, loom::Accept<CnPoke>,
                                                  loom::Emit<>> {
public:
    CnWeave() : tick_(timers().repeat("cn.tick", 10ms, &CnWeave::on_tick)) {}

    /// No `using TimedWeave::on;` — this hides every base handler.
    void on(const CnPoke&, loom::Mail&) {}

private:
    void on_tick(const zengine::timer::TimerFired&, loom::Mail&) { ++state_.ticks; }
    Handle tick_;
};

#elif ZENGINE_CN_CASE == 4 // ---- the hook re-accepting: MUST FAIL ------------

class CnWeave : public zengine::timer::TimedWeave<CnWeave, CnState, loom::Accept<CnPoke>,
                                                  loom::Emit<>> {
public:
    using TimedWeave::on;

    CnWeave() : tick_(timers().repeat("cn.tick", 10ms, &CnWeave::on_tick)) {}

    void on(const CnPoke&, loom::Mail&) {}

    /// THERE IS ONE ACTIVATION CURSOR AND IT HAS ALREADY SPOKEN. The hook runs
    /// INSIDE an accepted activation, so re-accepting could only double-count a
    /// decision the binding layer already made once. `activation()` hands back a
    /// `const&` and `accept` is non-const, so the attempt does not compile —
    /// const-correctness is the wall, and this fixture is what proves it is
    /// still standing rather than merely true today.
    void on_timed_activation(const loom::Activated& a, loom::Mail& mail) {
        (void)activation().accept(mail, a);
    }

private:
    void on_tick(const zengine::timer::TimerFired&, loom::Mail&) { ++state_.ticks; }
    Handle tick_;
};

#else
#error "ZENGINE_CN_CASE must be 1 (collision), 2 (hook), 3 (missing_using) or 4 (cursor)"
#endif

// Instantiating it is what runs the checks: the collision wall is anchored in
// TimedWeave's constructor, so it fires for any weave that is ever built —
// including one that never calls timers().
int main() {
    CnWeave w;
    (void)w;
    return 0;
}
