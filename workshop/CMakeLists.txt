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
add_library(zengine-workshop-vocabulary INTERFACE)
target_include_directories(zengine-workshop-vocabulary INTERFACE ${CMAKE_SOURCE_DIR})
target_link_libraries(zengine-workshop-vocabulary INTERFACE zengine-surface-vocabulary
                                                            zengine-ui-vocabulary)

# The host needs a Loom that can host .so weaves, for the same honest reason
# snake's does: it loads a Skin and an Input producer.
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: workshop host skipped — this Loom cannot host .so weaves (no loom::kernel)")
    return()
endif()

add_executable(zengine-workshop workshop.cpp)
target_link_libraries(zengine-workshop
    PRIVATE loom::kernel loom::switchboard loom::core zengine-workshop-vocabulary
            zengine-input-vocabulary zengine-surface-vocabulary zengine-ui-vocabulary
            zengine-timer-vocabulary zengine-warnings)

# The weaves this host loads must sit beside its binary — the host resolves them
# from its own directory (workshop.cpp's exe_dir), exactly as snake's does.
#
# COPIED rather than aimed here: RUNTIME_OUTPUT_DIRECTORY is one value per target,
# and snake already points these same .so files at its own runtime directory. Two
# hosts wanting the same weave is the normal case, not a conflict to resolve by
# one of them winning — so the shared artifacts stay where they are built and each
# host takes a copy. (Deployment is the host's concern; those packages still do
# not know who runs them.)
set(zengine_workshop_weaves zengine-skin-tui-classic zengine-input zengine-timer)
foreach(weave IN LISTS zengine_workshop_weaves)
    add_custom_command(TARGET zengine-workshop POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:${weave}> $<TARGET_FILE_DIR:zengine-workshop>
        COMMENT "workshop: staging ${weave} beside the host")
endforeach()
