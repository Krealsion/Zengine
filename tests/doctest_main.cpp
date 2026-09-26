// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The one translation unit that compiles doctest and provides main() for EVERY Zengine runtime
// test binary; a suite includes "doctest.h" and defines no config macro. A run that selected no
// case is a FAILURE (POP-01): stock doctest answers SUCCESS and exits 0 when a filter matches
// nothing, and 2.4.11 has no option that refuses it, so this main() reads the run's population
// from TestRunStats and refuses an empty one by name. It is Zengine's own copy, not Loom's: this
// repository builds against an INSTALLED Loom, which ships no test metadata.

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <cstdio>

namespace {

/// What the run reported about its own size. Written by the listener below
/// (which doctest owns and deletes), read by main() afterwards.
struct RunCensus {
    bool ran = false;
    unsigned cases_selected = 0;
};

RunCensus& census() {
    static RunCensus c;
    return c;
}

/// A listener, not a reporter: listeners are prepended to whatever reporter the
/// command line chose, so this observes every run without replacing the console
/// output. It exists only to remember how many cases passed the filters.
struct PopulationListener : doctest::IReporter {
    explicit PopulationListener(const doctest::ContextOptions&) {}

    void test_run_end(const doctest::TestRunStats& stats) override {
        census().ran = true;
        census().cases_selected = stats.numTestCasesPassingFilters;
    }

    // Everything else is deliberately inert.
    void report_query(const doctest::QueryData&) override {}
    void test_run_start() override {}
    void test_case_start(const doctest::TestCaseData&) override {}
    void test_case_reenter(const doctest::TestCaseData&) override {}
    void test_case_end(const doctest::CurrentTestCaseStats&) override {}
    void test_case_exception(const doctest::TestCaseException&) override {}
    void subcase_start(const doctest::SubcaseSignature&) override {}
    void subcase_end() override {}
    void log_assert(const doctest::AssertData&) override {}
    void log_message(const doctest::MessageData&) override {}
    void test_case_skipped(const doctest::TestCaseData&) override {}
};

} // namespace

DOCTEST_REGISTER_LISTENER("zengine-population", 0, PopulationListener);

int main(int argc, char** argv) {
#if defined(_WIN32)
    // THE SUITES ARE HOSTS TOO, and say what a bad image means the way Workshop does: a
    // refusal, never a modal. A case that stages a non-library as a rebuilt product hung
    // a CI runner inside `ntdll!ZwRaiseHardError` under `LoadLibrary` until the job's
    // timeout (measured through gdb on the runner); a desktop session refused
    // the same file with error 193 and the case passed. The lane must not depend on which.
    ::SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
#endif
    doctest::Context context(argc, argv);
    const int result = context.run();

    // Only a REAL RUN that selected nothing is refused: doctest calls test_run_end only outside
    // query mode, so `--count`, the `--list-*` modes, `--help`, `--version` and `--no-run` stay a
    // silent zero exit -- the modes tests/check_population.cmake takes its inventory with.
    if (census().ran && census().cases_selected == 0) {
        std::fprintf(stderr,
                     "\n"
                     "===============================================================================\n"
                     "[zengine] EMPTY TEST POPULATION -- this run selected 0 test cases.\n"
                     "[zengine] A named verification target that executes nothing has not passed; it\n"
                     "[zengine] has no evidence at all. Something the filter names is gone: a renamed\n"
                     "[zengine] or deleted TEST_CASE, a case compiled out by an #if, or a stale\n"
                     "[zengine] filter.\n"
                     "[zengine] Command line:");
        for (int i = 1; i < argc; ++i) {
            std::fprintf(stderr, " %s", argv[i]);
        }
        std::fprintf(stderr,
                     "\n"
                     "===============================================================================\n");
        return 70; // distinct from doctest's own EXIT_FAILURE, so the cause is legible
    }

    return result;
}
