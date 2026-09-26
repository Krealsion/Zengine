# The Menu Presenter: the participant that presents a pane's offered menu, as a loadable weave
# holding `zengine.presenter`, replaced as any weave is (another plan row, or a reload keeping
# the same state). It needs what a stranger's presenter would -- the pane protocol and presenter
# seam (`zengine::pane`), the activation cursor and the list window -- and no Workshop vocabulary
# and no kernel: the host draws its lines and the requester acts on its answer.

if(NOT TARGET loom::kernel)
    message(STATUS "zengine: menu presenter skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(zengine-menu-presenter presenter.cpp)
target_link_libraries(zengine-menu-presenter PRIVATE zengine-pane-vocabulary
                                                     zengine-surface-vocabulary
                                                     zengine-activation
                                                     zengine-component)
