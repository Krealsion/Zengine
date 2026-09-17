# The Neovim-backed Editor -- the Editor's office held with a Neovim this weave runs, as a load plan
# choice beside the standard Editor (`editor-pane/`). It arrives through the external pane protocol
# as every pane does, and speaks the editor switch's handoff as both halves.
#
# ONE TRANSLATION UNIT OVER A HEADER LIBRARY. Everything that holds a Neovim -- the process, the
# conversation, the screen, the keys, the document arithmetic, the module it installs -- is
# `neovim/`'s (`zengine-neovim`); this weave decides what those facts mean for the Editor's office.
# It links the Workshop vocabulary for the pane, open and switch shapes, and the activation cursor
# for its attested first breath. It does not link the standard Editor, and the standard Editor does
# not link it.

if(NOT TARGET loom::kernel)
    message(STATUS "zengine: Neovim editor weave skipped — this Loom cannot host loadable weaves (no loom::kernel)")
    return()
endif()

zengine_weave(zengine-neovim-editor pane.cpp)
target_link_libraries(zengine-neovim-editor PRIVATE zengine-neovim zengine-workshop-vocabulary
                                                    zengine-input-vocabulary zengine-activation
                                                    zengine-timer-vocabulary)
