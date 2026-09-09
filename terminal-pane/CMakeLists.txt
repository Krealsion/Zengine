# The Terminal pane package — the transcript a maker reads and the line they compose messages
# on, as a loadable weave rather than a modal overlay inside the Workshop host. It arrives
# through the external pane protocol exactly as introspection/, composer/, files/,
# builder-pane/, attention-pane/ and info-pane/ do, and Workshop compiles nothing for it.
#
# ⚠ THE PARTICIPANT IS NOT IN THIS PACKAGE AND THAT IS THE MIGRATION'S MEASUREMENT.
# `loom::TerminalSession` is mounted by the host and reached by a pointer the host holds. No
# message in the interface it has TODAY drives it, and this work adds no Loom sentence, so
# under that constraint it could not have come here -- which is a bound, not an impossibility.
# This package links `zengine-workshop-vocabulary` for the pane protocol and the terminal
# SEAM's shapes, and nothing that could construct, read or author through a participant.
# What it can do to one is ask.
#
# ONE TARGET, AND NO PLATFORM HALF. Everything this pane does is compose text from values it
# was told; the weave is one translation unit. It links `zengine-component` for the editable
# line — the same one the Builder's role line, the browser's authoring field and Info's
# property draft spend.

# The weave needs a Loom that can HOST loadable weaves — the same honest question every
# package here asks.
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: terminal pane weave skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(zengine-terminal-pane pane.cpp)
target_link_libraries(zengine-terminal-pane PRIVATE zengine-workshop-vocabulary
                                                    zengine-component zengine-input-vocabulary
                                                    zengine-activation)
