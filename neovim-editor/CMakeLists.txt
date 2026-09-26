# The Neovim-backed Editor: the Editor's office held with a Neovim this weave runs, a load plan
# choice beside the standard Editor (`editor-pane/`), reaching Workshop through the external pane
# protocol and speaking both halves of the editor switch's handoff. One translation unit over
# `neovim/` (`zengine-neovim`), which holds a Neovim; this weave decides what its facts mean for
# the office. It does not link the standard Editor, nor the standard Editor it.

# THE ASKS A MAKER SENDS IT, as this package's one public header: exported as `zengine::neovim`
# (cmake/ZengineInstall.cmake), so a host or a weave outside this tree can compose
# `NeovimStartRequested`, `NeovimStatusRequested` and `NeovimStopRequested` the way a console
# does. The hosting library (`neovim/`) is not exported: holding a Neovim is this weave's work.
add_library(zengine-neovim-editor-vocabulary INTERFACE)
target_include_directories(zengine-neovim-editor-vocabulary INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
add_library(zengine::neovim ALIAS zengine-neovim-editor-vocabulary)
target_link_libraries(zengine-neovim-editor-vocabulary INTERFACE loom::core)

if(NOT TARGET loom::kernel)
    message(STATUS "zengine: Neovim editor weave skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(zengine-neovim-editor pane.cpp)
target_link_libraries(zengine-neovim-editor PRIVATE zengine-neovim zengine-neovim-editor-vocabulary
                                                    zengine-workshop-vocabulary
                                                    zengine-input-vocabulary zengine-activation
                                                    zengine-timer-vocabulary zengine-source-transfer)
