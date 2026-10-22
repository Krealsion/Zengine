# The Command Palette: every action and pane a hand can reach, found by name, as a loadable weave
# holding `zengine.palette`, replaced as any weave is. It reads the keymap and the inventory
# Workshop says to any presenter and asks Workshop to run a choice; it draws in its own area of a
# graphics window (the pane protocol, the graphics canvas, the text box for its filter).

if(NOT TARGET loom::kernel)
    message(STATUS "zengine: palette skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(zengine-palette palette.cpp MANUAL ../docs/zengine.palette.md)
target_link_libraries(zengine-palette PRIVATE zengine-workshop-vocabulary
                                              zengine-graphics-vocabulary
                                              zengine-input-vocabulary
                                              zengine-activation
                                              zengine-component)
