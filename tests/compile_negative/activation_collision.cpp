// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The compile-negative fixture for the TimedWeave activation wall: one source, five targets
// (ZENGINE_CN_CASE), so each refusal and its control are the same weave but for the thing under
// test (VM-WALL-04). 1 declares the forbidden raw activation handler; 2, the control, uses the
// supported hook; 3 hides the base handlers with no `using`; 4 re-accepts inside the hook; 5 is 3
// with no binding declared, which keeps both walls anchored in TimedWeave's constructor.

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

#elif ZENGINE_CN_CASE == 5 // ---- missing `using`, NO binding: MUST FAIL ------

class CnWeave : public zengine::timer::TimedWeave<CnWeave, CnState, loom::Accept<CnPoke>,
                                                  loom::Emit<>> {
public:
    /// No `using TimedWeave::on;` AND no `timers()` call: this weave takes the
    /// layer for its activation half and would place any schedule with the raw
    /// protocol. The hiding defect is identical to case 3; what differs is that
    /// nothing here ever instantiates `timers()`.
    void on(const CnPoke&, loom::Mail&) {}
};

#else
#error "ZENGINE_CN_CASE must be 1 (collision), 2 (hook), 3 (missing_using), 4 (cursor) or 5 (missing_using_no_binding)"
#endif

// Instantiating it is what runs the checks: BOTH walls are anchored in
// TimedWeave's constructor, so they fire for any weave that is ever built —
// including one that never calls timers().
int main() {
    CnWeave w;
    (void)w;
    return 0;
}
