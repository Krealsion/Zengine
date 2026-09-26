# The Operator package: a named, typed, composable semantic truth, and the one place that
# evaluates one. An ordinary helper stays a C++ function until two or more surfaces need the
# same rule and would each hold a copy. Not the Loom's (it owns Schema, Value and loaded-code
# lifetime, and this spends them without adding to any), and not timer/'s (a rule the Timer
# owned only the Timer could share).

# Header-only; it links loom::core because a signature is a pair of `loom::Schema`s and a pack
# goes through `loom::admit`, which is what keeps a second type system out. Portable: no
# kernel, no weave, no bus.
add_library(zengine-operator INTERFACE)
target_include_directories(zengine-operator INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
add_library(zengine::operator ALIAS zengine-operator)
target_link_libraries(zengine-operator INTERFACE loom::core)

# The consumer half of the operator-host seam, its own target because the shape is the claim: a
# loaded consumer linking this and nothing else holds a C table and a codec, no `op::Catalog`,
# `OperatorDef` or primitive, so it can reach the host's operator truth only by asking. It links
# loom::core, not loom::kernel: `zen/kernel/abi.h` and `zen/kernel/schema_codec.hpp` are headers
# on the exported include path, and the kernel library is the host's dependency, never a loaded
# artifact's.
add_library(zengine-operator-consumer INTERFACE)
target_include_directories(zengine-operator-consumer INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>)
add_library(zengine::operator-consumer ALIAS zengine-operator-consumer)
target_link_libraries(zengine-operator-consumer INTERFACE loom::core)

# The host side of the seam rides zengine-operator, since only an image that has a catalog can
# dress one in a C table; the host knows the consumer's descriptor codec, never the reverse.
target_link_libraries(zengine-operator INTERFACE zengine-operator-consumer)
if(UNIX)
    # `OperatorOffer` resolves the optional symbol from the same image the Kernel
    # opens, and `mount_provider` opens a provider artifact outright; both need the
    # dynamic loader. On glibc 2.34+ dl* lives in libc and this is a no-op; on older
    # ones it is not, and CMake's own answer is the one that knows which.
    target_link_libraries(zengine-operator INTERFACE ${CMAKE_DL_LIBS})
endif()

# The basic operator provider: an ordinary shared library supplying the scalar primitives to
# whoever mounts it. `zengine_provider`, not `zengine_weave`: no `loom::switchboard` on the line,
# because no participant lives in this image (no WeaveId, role, grant, manifest or bus); a host
# opens the file itself and resolves one optional symbol. It links the operator package because
# a provider authors; it holds nobody's resolution.
if(TARGET loom::kernel)
    zengine_provider(zengine-operators-basic basic_provider.cpp)
    target_link_libraries(zengine-operators-basic PRIVATE zengine-operator)
else()
    message(STATUS "zengine: basic operator provider skipped -- this Loom cannot host loadable images (no loom::kernel)")
endif()
