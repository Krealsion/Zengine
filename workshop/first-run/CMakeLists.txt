# The quick setup: a weaver's first start asks how they edit text and shows where the setups are,
# once, as a loadable weave holding `zengine.first-run`, replaced as any weave is. It keeps one
# office-wide setting in Workshop's settings file and draws in its own area of a graphics window
# (the pane protocol, the graphics canvas); its answers go to the editor switch and Workshop.

if(NOT TARGET loom::kernel)
    message(STATUS "zengine: first-run skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(zengine-first-run first_run.cpp MANUAL ../docs/zengine.first-run.md)
target_link_libraries(zengine-first-run PRIVATE zengine-workshop-vocabulary
                                                zengine-graphics-vocabulary
                                                zengine-input-vocabulary
                                                zengine-activation)
