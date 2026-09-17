# The Neovim hosting library -- one Neovim, held and spoken to, with nothing of Workshop in it.
#
#   msgpack.hpp     the bounded codec (a resumable scan; nothing allocated for what a bound refuses)
#   rpc.hpp         msgpack-RPC as one conversation: ids, the books, a bounded outbound queue
#   child.hpp       custody of the process: Windows overlapped named pipes in a kill-on-close job,
#                   POSIX a socketpair in the child's own process group; never blocks
#   grid.hpp        the screen Neovim describes (`ext_linegrid` + `ext_hlstate`), by meaning
#   projection.hpp  that screen in the pane protocol's words: ASCII rows, one caret, one range
#   keys.hpp        a maker's keys and text in Neovim's notation
#   document.hpp    the exact arithmetic between the standard Editor's model and Neovim's
#   lua.hpp         the one module Zengine installs in a Neovim, as text
#   launch.hpp      which Neovim, which profile, embedded or listening
#   host.hpp        the conversation owner: start, pump, call, finish, failures in words
#
# HEADER-ONLY AND INTERFACE, like every package vocabulary here, and deliberately a LIBRARY rather
# than a weave: the weave that puts Neovim in a pane (`neovim-editor/`) and a host that holds a
# Neovim for any other reason both include these headers, and neither needs the other.
#
# IT LINKS THE INPUT VOCABULARY for `keys.hpp`'s scancode and modifier names, and nothing else of
# Zengine's: no Workshop header, no pane shape, no bus. What a Neovim's facts MEAN in a Workshop is
# the weave's to decide.
add_library(zengine-neovim INTERFACE)
target_include_directories(zengine-neovim INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
target_link_libraries(zengine-neovim INTERFACE zengine-input-vocabulary)
