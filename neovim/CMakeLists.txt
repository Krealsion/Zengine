# The Neovim hosting library: one Neovim, held and spoken to, with nothing of Workshop in it --
# the bounded codec (msgpack.hpp), the conversation (rpc.hpp), custody of the process
# (child.hpp), the screen (grid.hpp) and its pane projection, keys, the document arithmetic,
# the installed Lua module, the launch choice, and the owner (host.hpp).

# Header-only and a library, not a weave: the weave that puts Neovim in a pane (`neovim-editor/`)
# and a host that holds a Neovim for another reason both include it. It links only the input
# vocabulary (for `keys.hpp`); what a Neovim's facts mean in a Workshop is the weave's.
add_library(zengine-neovim INTERFACE)
target_include_directories(zengine-neovim INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
target_link_libraries(zengine-neovim INTERFACE zengine-input-vocabulary)
