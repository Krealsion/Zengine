# The settings package: the theme file, how a theme resolves with its includes, and the
# `zengine.settings` office's shapes -- header-only, reading the pane protocol's settings, the
# input library's key spelling and the persist library's files -- and the `zengine-settings` weave
# that holds the office.
add_library(zengine-settings-vocabulary INTERFACE)
target_include_directories(zengine-settings-vocabulary INTERFACE ${PROJECT_SOURCE_DIR})
target_link_libraries(zengine-settings-vocabulary INTERFACE loom::core zengine-pane-vocabulary
                                                            zengine-input-vocabulary
                                                            zengine-persist)

if(NOT TARGET loom::kernel)
    message(STATUS "zengine: settings weave skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

# The weave holding `zengine.settings`: it reads and applies themes, offers their palettes and
# keeps each office's declared slots. It speaks to the layouts' owner and the keymap by their seam's
# words, and its manual is compiled in.
zengine_weave(zengine-settings weave.cpp MANUAL docs/zengine.settings.md)
target_link_libraries(zengine-settings PRIVATE zengine-settings-vocabulary
                                               zengine-graphics-vocabulary
                                               zengine-workshop-vocabulary zengine-activation)
