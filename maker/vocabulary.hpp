// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_MAKER_VOCABULARY_HPP
#define ZENGINE_MAKER_VOCABULARY_HPP

// The five ceremony shapes a schema edit is spoken in, between a maker weave and its coordinator:
// permanent once shipped and generic across definitions. A weave's state travels in them as native
// bytes, admitted by the receiver at its own door, so they name no field of any definition. Their
// wire names carry the package's `zengine.maker.` prefix, which `ZEN_SHAPE` cannot spell, so they
// are registered by hand, as Loom's poke and standard shapes are. Reference:
// maker/docs/maker-weave.md.

#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <tuple>

namespace zengine::maker {

/// "Stop changing. Say what you are." An ordinary domain message at an exact FIFO position;
/// Loom gives it no standing -- what makes it a boundary is that the incumbent's handler
/// enters a quiescing state and answers its final value there.
struct Quiesce {
    std::int64_t token = 0;
    using ZenSelf = Quiesce;
    static constexpr const char* zen_name = "zengine.maker.Quiesce";
    static constexpr std::uint32_t zen_version = 1;
    static auto zen_fields() { return std::make_tuple(ZEN_FIELD(token)); }
};

/// The incumbent's final authored value, as the native bytes of its state at its own schema.
struct Quiesced {
    std::int64_t token = 0;
    loom::Bytes state;
    using ZenSelf = Quiesced;
    static constexpr const char* zen_name = "zengine.maker.Quiesced";
    static constexpr std::uint32_t zen_version = 1;
    static auto zen_fields() { return std::make_tuple(ZEN_FIELD(token), ZEN_FIELD(state)); }
};

/// "The edit is off; you are the service again." Sent by the coordinator to an incumbent it
/// quiesced, when the succession aborts.
struct Resume {
    std::int64_t token = 0;
    using ZenSelf = Resume;
    static constexpr const char* zen_name = "zengine.maker.Resume";
    static constexpr std::uint32_t zen_version = 1;
    static auto zen_fields() { return std::make_tuple(ZEN_FIELD(token)); }
};

/// The preparation ask: "here is your starting state, converted; take it." The candidate
/// admits the bytes at its OWN state schema -- the one gate -- and answers for itself.
struct Adopt {
    loom::Bytes state;
    using ZenSelf = Adopt;
    static constexpr const char* zen_name = "zengine.maker.Adopt";
    static constexpr std::uint32_t zen_version = 1;
    static auto zen_fields() { return std::make_tuple(ZEN_FIELD(state)); }
};

/// The candidate's answer to `Adopt`, spent through `Bus::answer` so Loom attests it as THE
/// answer to that ask. `ready` false carries the gate's own words in `why`.
struct Adopted {
    bool ready = false;
    std::string why;
    using ZenSelf = Adopted;
    static constexpr const char* zen_name = "zengine.maker.Adopted";
    static constexpr std::uint32_t zen_version = 1;
    static auto zen_fields() { return std::make_tuple(ZEN_FIELD(ready), ZEN_FIELD(why)); }
};

} // namespace zengine::maker

#endif // ZENGINE_MAKER_VOCABULARY_HPP
