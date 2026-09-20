# The Menu Presenter -- the participant that presents a pane's offered menu, as a loadable weave
# rather than a surface compiled into the Workshop host. It holds `zengine.presenter`, which a load
# plan grants it like any other office, and a maker replaces it the way any weave is replaced: a
# plan row naming another artifact, or a reload of another image keeping the same state.
#
# WHAT IT NEEDS is what a stranger's presenter would: the pane protocol and the presenter seam
# (`zengine-pane-vocabulary`, installed as `zengine::pane`), the activation cursor and the list
# window. It links no Workshop vocabulary and no kernel: it draws nothing itself and can reach
# nobody's authority -- the host draws its lines and the requester acts on its answer.

if(NOT TARGET loom::kernel)
    message(STATUS "zengine: menu presenter skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(zengine-menu-presenter presenter.cpp)
target_link_libraries(zengine-menu-presenter PRIVATE zengine-pane-vocabulary
                                                     zengine-surface-vocabulary
                                                     zengine-activation
                                                     zengine-component)
