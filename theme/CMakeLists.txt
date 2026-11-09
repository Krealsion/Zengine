# The theme package: a theme's two files, how a theme resolves with its includes and how a stack is
# laid, and the `zengine.theme` role's shapes -- header-only, over the graphics vocabulary's pool and
# terminal inks and the persist library's files -- and the `zengine-theme` weave that holds the role.
add_library(zengine-theme-vocabulary INTERFACE)
target_include_directories(zengine-theme-vocabulary INTERFACE ${PROJECT_SOURCE_DIR})
target_link_libraries(zengine-theme-vocabulary INTERFACE loom::core zengine-graphics-vocabulary
                                                         zengine-persist)

if(NOT TARGET loom::kernel)
    message(STATUS "zengine: theme weave skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

# The weave holding `zengine.theme`: it reads themes, lays each stack a host's areas wear and offers
# its palette, and keeps each role's own values; its manual is compiled in.
zengine_weave(zengine-theme weave.cpp MANUAL docs/zengine.theme.md)
target_link_libraries(zengine-theme PRIVATE zengine-theme-vocabulary zengine-activation)
