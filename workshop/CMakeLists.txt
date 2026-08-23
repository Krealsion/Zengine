# The Workshop package — the maker-facing surface (W-0: draw one rectangle,
# select it, inspect it, edit it, refuse a bad edit).
#
# Nothing here is a loadable weave. Workshop's own weave is mounted IN-PROCESS by
# its host, because W-0 gives nobody a reason to unload it: the reloadable-weave
# machinery is real and proven next door, and reaching for it here would be
# ceremony bought with nothing. The weaves this host DOES load are other
# packages' — the Skin, the Input producer, the Timer service — through the same
# ordinary LoadWeave commands snake's host sends.
#
# The vocabulary and the logic travel as a header-only interface target, the
# pattern input/ and surface/ already use: the suite includes the same headers the
# host does, so what the tests pin is what ships. Portable — it exists even where
# no kernel does.
#
# It consumes two package vocabularies and the split between them is the W-1
# result: `ui` owns what a maker AUTHORS and what a viewport MAKES of it;
# `surface` owns what gets PAINTED. Workshop is the application that joins them,
# and it is the only one of the three that knows about both.
#
# It links loom::core EXPLICITLY since W-5, and not merely by inheritance from
# the ui vocabulary. `workshop/persist.hpp` includes <zen/serialize.hpp> and
# calls into the Loom's compat codec and its gate, so this package now has its
# own direct, linked dependency on that surface — a consumer that got it for
# free through a sibling target would keep building right up until the sibling
# stopped needing it.
# ...and loom::terminal EXPLICITLY since WT-1, by the same rule. `workshop/screen.hpp`
# includes <zen/terminal/transcript.hpp> to render a participant's own record and
# `workshop/weave.hpp` includes <zen/terminal/session.hpp> and the shared command
# grammar in <zen/terminal/input_lex.hpp>; `name_of(TranscriptKind)` and
# `TerminalSession::send/ask` are compiled symbols, not header-only, so this is a
# real link dependency and is named rather than inherited.
#
# It is portable, like the rest of this target: the terminal core is in Loom's export
# set unconditionally and needs no kernel. The HOST below still gates on loom::kernel,
# for the reason it always did -- it loads .so weaves -- and that gate is unrelated.
#
# ...and zengine-builder-vocabulary since BLD-0, for the narrowest possible reason.
# `workshop/panel.hpp` includes `builder/vocabulary.hpp` because the Builder PANEL holds a
# copy of `builder::BuildStatus` and Workshop's weave sends `builder::BuildRequested` — the
# shapes of a conversation, and nothing else. It does NOT include builder/runner.hpp or
# builder/run.hpp, and this target does not carry them: a presentation that could reach the
# code that starts a process would be a presentation with process authority in it, however
# carefully it declined to use it. Which header a consumer names is the visible half of that
# split; the enforcing half is the grant the HOST writes below.
add_library(zengine-workshop-vocabulary INTERFACE)
target_include_directories(zengine-workshop-vocabulary INTERFACE ${CMAKE_SOURCE_DIR})
target_link_libraries(zengine-workshop-vocabulary INTERFACE zengine-surface-vocabulary
                                                            zengine-ui-vocabulary
                                                            zengine-component
                                                            zengine-builder-vocabulary
                                                            loom::core
                                                            loom::terminal)

# The host needs a Loom that can host .so weaves, for the same honest reason
# snake's does: it loads a Skin and an Input producer.
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: workshop host skipped — this Loom cannot host .so weaves (no loom::kernel)")
    return()
endif()

add_executable(zengine-workshop workshop.cpp)
target_link_libraries(zengine-workshop
    PRIVATE loom::kernel loom::switchboard loom::core loom::terminal loom::history
            zengine-workshop-vocabulary
            zengine-input-vocabulary zengine-surface-vocabulary zengine-ui-vocabulary
            zengine-timer-vocabulary zengine-builder-vocabulary
            # The two durable names in Introspection's boot line, and NOTHING of its
            # view: the host says which stem to load and into which office, exactly as
            # it does for the Timer. It does not include `loaded.hpp` and could not
            # present a row of it.
            zengine-introspection-view
            # ...and the Composer's two durable names, on exactly the same terms and
            # for the same reason: the host says which stem to load and into which
            # office. It does not include `draft.hpp` or `view.hpp` and could not
            # present a row of either.
            zengine-composer-vocabulary
            # ...and the operator package, because since CAT-0 this host OWNS the
            # process's operator catalog and dresses it in the C table a loaded
            # consumer can hold. `zengine-operator` is the HOST half of that seam
            # (the catalog, the definitions, `OperatorHostSurface`, `OperatorOffer`);
            # a loaded artifact links the consumer half and nothing else. Both are
            # header-only INTERFACE targets, so this adds no shared object -- what it
            # adds on UNIX is the dynamic loader, which `OperatorOffer` needs to
            # resolve a second exported symbol of the image the Kernel opens.
            # ...AND SINCE PROV-0 THIS HOST PUBLISHES NOTHING INTO IT. The catalog
            # starts EMPTY and is filled by mounting provider artifacts, so what
            # `zengine-operator` supplies here is the machinery -- the layered store,
            # `OperatorHostSurface`, `OperatorOffer` and `mount_provider` -- and not one
            # operator. Workshop no longer includes `timer/normalize.hpp` at all: it
            # knows which artifacts to mount and how to host what they supply, and
            # nothing about what any of it means.
            zengine-operator
            zengine-warnings zengine-sanitize)

# ---- The one build recipe this host hands its build runner (BLD-0) ----------------------
#
# THE TARGET'S EXISTING BUILD RECIPE, NOT A SECOND BUILD SYSTEM. Zengine is built by CMake,
# so "build zengine-snake" spelled honestly is the very command a maker would type:
#
#     ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target zengine-snake
#
# Nothing here compiles anything itself, knows a compiler, or reimplements a dependency
# rule. If the recipe is stale the real build system says so, in its own words, on the
# panel.
#
# BAKED AT CONFIGURE TIME, and both halves are deliberate:
#
#   the cmake     ${CMAKE_COMMAND} is THE cmake that configured this tree, by absolute
#                 path. Resolving `cmake` from PATH at run time would let a Workshop
#                 started from a different shell drive a different CMake against a tree it
#                 did not configure -- a build that works on one machine and produces an
#                 unexplainable diagnostic on another.
#   the build dir ${CMAKE_BINARY_DIR} is THIS build tree, absolute. Deriving it from the
#                 executable's own directory at run time would be a guess that happens to
#                 be right for the single-config generators this repository's lanes use and
#                 quietly wrong for a multi-config one, where the binary sits one directory
#                 deeper. A copied binary therefore still names the tree it was built in,
#                 and if that tree is gone the build fails honestly rather than picking a
#                 nearby one.
#
# ZENGINE-SNAKE IS THE TARGET, and the choice is not arbitrary. It is real, it is always
# built here, and -- the part that matters -- it is NOT one of the weaves this running
# Workshop has loaded. Building a target whose artifact is staged beside this host would
# have the build overwrite a shared library the process has mapped, which on Linux changes
# code under a running program and on Windows simply fails on a locked file. The obvious
# next wish, `zengine-workshop` rebuilding itself, is the same hazard aimed at the host
# binary; that it is out of scope here is a consequence of Build+Load being a later phase
# and not a coincidence.
target_compile_definitions(zengine-workshop PRIVATE
    "ZENGINE_BUILDER_CMAKE=\"${CMAKE_COMMAND}\""
    "ZENGINE_BUILDER_BUILD_DIR=\"${CMAKE_BINARY_DIR}\""
    "ZENGINE_BUILDER_TARGET=\"zengine-snake\"")

# The weaves this host loads must sit beside its binary — the host resolves them
# from its own directory (workshop.cpp's exe_dir), exactly as snake's does.
#
# COPIED rather than aimed here: RUNTIME_OUTPUT_DIRECTORY is one value per target,
# and snake already points these same .so files at its own runtime directory. Two
# hosts wanting the same weave is the normal case, not a conflict to resolve by
# one of them winning — so the shared artifacts stay where they are built and each
# host takes a copy. (Deployment is the host's concern; those packages still do
# not know who runs them.)
#
# The SDL skin joins the list only where this configuration built it. `--skin`
# (workshop.cpp) is what SELECTS one at launch; staging is what makes the chosen
# stem resolvable beside the host. Both halves are needed and they are different
# acts — that split is the whole of what P29 measured.
# `zengine-introspection` is the fourth, and the first entry here that is a TOOL
# rather than a service the application cannot run without. It is staged and booted
# on the same terms as the other three because that is what the host's boot list is
# for; what makes it different is on the other side of the pane seam, where Workshop
# never learns what it presents (INTR-0).
#
# `zengine-composer` is the fifth, and it is the second TOOL. What makes it worth a
# line of its own here is that the host boots it exactly as it boots the fourth --
# one stem, one office -- and gains no knowledge of what it does. Workshop compiled
# nothing for the Compose pane, mints no kind for it, and learns its row from a live
# offer; the two tools do not know about each other either, and the fact one reacts
# to the other is a decision inside `composer.cpp` that this host cannot see.
#
# `zengine-operators-basic` is the sixth, and it is the first entry here that is NOT A
# WEAVE -- which is why this list stopped being called one (PROV-0). No Kernel loads it,
# it has no role, no office and no line in the boot sequence; the host opens it directly,
# reads two operator definitions out of it, and holds it. It has to sit beside the binary
# for exactly the same reason every other entry does, and it is staged by the same rule,
# because "where do I find the file" is a deployment question and does not care what the
# file turns out to be.
set(zengine_workshop_artifacts zengine-skin-tui-classic zengine-input zengine-timer
                               zengine-introspection zengine-composer
                               zengine-operators-basic)
if(TARGET zengine-skin-sdl)
    list(APPEND zengine_workshop_artifacts zengine-skin-sdl)
endif()
if(TARGET zengine-input-sdl)
    list(APPEND zengine_workshop_artifacts zengine-input-sdl)
endif()
#
# STAGED ON THE WEAVE'S OWN CLOCK, not on the host's -- and that is a repair with a measured
# cost behind it. A `POST_BUILD` command runs when its TARGET is (re)linked, so with the host
# already up to date, `cmake --build .` copied nothing: edit a Skin, rebuild everything
# successfully, and the Workshop host went on loading the previous one from beside itself.
# Silently, with the fresh library sitting one directory away. Twenty minutes of WT-1a went
# into a measurement taken against a thirty-minute-old Skin because of it, and it produces the
# worst shape a build defect has -- "I fixed it and nothing changed".
#
# So each copy is a rule with its own OUTPUT and its own DEPENDS, which is what makes the
# build graph carry the fact: the stamp is older than the weave, therefore copy. The host
# depends on the whole set, so building the host stages what the host will load. (Snake does
# not need this: it AIMS these weaves at its own directory with RUNTIME_OUTPUT_DIRECTORY and
# has no copy to go stale. Workshop is the second host, and the second host is the one that
# copies.)
# The host's own directory, SPELLED OUT rather than asked of the target. `$<TARGET_FILE_DIR:
# zengine-workshop>` would make these rules depend on the host that depends on them, and CMake
# says so out loud: "the inter-target dependency graph contains a cycle". The host sets no
# output directory, so this is where it lands, and the multi-config arm is the one line that
# keeps that true for a generator this repository's lanes do not use.
if(CMAKE_CONFIGURATION_TYPES)
    set(zengine_workshop_dir ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
else()
    set(zengine_workshop_dir ${CMAKE_CURRENT_BINARY_DIR})
endif()
set(zengine_workshop_staged "")
foreach(artifact IN LISTS zengine_workshop_artifacts)
    set(stamp ${CMAKE_CURRENT_BINARY_DIR}/staged-${artifact}.stamp)
    add_custom_command(OUTPUT ${stamp}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:${artifact}> ${zengine_workshop_dir}
        COMMAND ${CMAKE_COMMAND} -E touch ${stamp}
        DEPENDS ${artifact}
        COMMENT "workshop: staging ${artifact} beside the host")
    list(APPEND zengine_workshop_staged ${stamp})
endforeach()
add_custom_target(zengine-workshop-staging DEPENDS ${zengine_workshop_staged})
add_dependencies(zengine-workshop zengine-workshop-staging)

# And the shared SDL3 the two SDL-backed weaves link. New in G-1: SDL used to be
# archived INTO the one weave that used it, and two weaves cannot share an archived
# copy (cmake/ZengineSdl.cmake says why). A shared library has to be findable at run
# time, which on Windows means the application directory and nowhere else.
zengine_stage_sdl(zengine-workshop)
