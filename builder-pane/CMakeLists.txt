# The Builder pane package — the maker's seat at the build, as a loadable weave rather than
# C++ inside the Workshop host. It arrives through the external pane protocol exactly as
# introspection/, composer/ and files/ do, and Workshop compiles nothing for it.
#
# ONE TARGET, AND NO PLATFORM HALF. Files needed a small static library because its browser
# spends two out-of-line platform bodies (`leaves_the_tree`, `host_filesystem_roots`); this
# pane spends none. Everything it does is compose text from values it was told, so the weave
# is one translation unit and there is nothing for a suite to share a compilation of.
#
# WHY IT IS NOT IN `builder/`. That package is the TOOL and the runner, and its own
# CMakeLists says what it is: header-only, no loadable weave, and no kernel needed — a
# consumer that only wants to PRESENT a build includes one header of it. A loadable weave
# there would put an `if(NOT TARGET loom::kernel) return()` in the middle of the tool's
# package, so a Loom that cannot host weaves would stop exporting the vocabulary Workshop
# COMPILES AGAINST — a kernel-less build breaking for the sake of a pane that build cannot
# load anyway. The presentation is a separate package for the same reason it is a separate
# participant (BLD-0): it owns none of the tool's facts.
#
# IT LINKS NO loom::kernel, like the Files weave and unlike introspection: it asks offices
# (the tool, the project, the plan, the Editor) and hears answers, and never touches the
# control door or the Manager. What it needs is the weave-authoring surface (loom::core +
# loom::switchboard, through zengine_weave), the pane protocol and the seam vocabulary
# (workshop-vocabulary), the builder's shapes, the editable line (component), the key numbers
# (input-vocabulary) and the activation cursor.

# The weave needs a Loom that can HOST loadable weaves — the same honest question every
# package here asks.
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: builder pane weave skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(zengine-builder-pane pane.cpp)
target_link_libraries(zengine-builder-pane PRIVATE zengine-workshop-vocabulary
                                                   zengine-builder-vocabulary
                                                   zengine-component zengine-input-vocabulary
                                                   zengine-activation)
