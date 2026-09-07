# The Files package — the project browser, its marks and the two files it writes, as a
# loadable weave rather than C++ inside the Workshop host. It arrives through the external
# pane protocol exactly as introspection/ and composer/ do, and Workshop compiles nothing
# for it.
#
# Two pieces: a small static library of the two PLATFORM bodies the pure half needs, and
# the weave itself.
#
#   zengine-files-os   `leaves_the_tree` and `host_filesystem_roots` -- the only two
#                      out-of-line functions the browser and its marks spend. Everything
#                      else (the listing, the marks, their file, path admission) is
#                      header-only under workshop/, included here the same way
#                      introspection includes workshop/pane_vocabulary.hpp. A tiny library
#                      so the weave and the suite share one compilation of the two bodies
#                      rather than each carrying <windows.h> twice.
#   zengine-files      the weave: it offers `project-files`, declares its actions, browses,
#                      marks, authors a recipe row in-pane, and asks the host's four doors
#                      for what it can no longer read.
#
# IT LINKS NO loom::kernel, unlike introspection: it asks offices (the project, the
# recipes, the Editor) and hears answers, and never touches the control door or the
# Manager. What it needs is the weave-authoring surface (loom::core + loom::switchboard,
# through zengine_weave), the pane protocol and the seam vocabulary (workshop-vocabulary),
# the editable line (component), the key numbers (input-vocabulary) and the activation
# cursor.

add_library(zengine-files-os STATIC os.cpp)
set_target_properties(zengine-files-os PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(zengine-files-os PUBLIC zengine-workshop-vocabulary
                                       PRIVATE zengine-warnings zengine-sanitize)

# The weave needs a Loom that can HOST loadable weaves — the same honest question every
# package here asks.
if(NOT TARGET loom::kernel)
    message(STATUS "zengine: files weave skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(zengine-files files.cpp)
target_link_libraries(zengine-files PRIVATE zengine-files-os zengine-workshop-vocabulary
                                            zengine-component zengine-input-vocabulary
                                            zengine-activation)
