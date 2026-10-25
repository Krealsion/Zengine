# The settings package: the theme file, how a theme resolves with its includes, and the
# `zengine.settings` office's shapes, which no weave holds. Header-only; a theme's settings and
# keys are judged by Workshop's own rules, so it links Workshop's vocabulary.
add_library(zengine-settings-vocabulary INTERFACE)
target_include_directories(zengine-settings-vocabulary INTERFACE ${PROJECT_SOURCE_DIR})
target_link_libraries(zengine-settings-vocabulary INTERFACE loom::core zengine-workshop-vocabulary)
