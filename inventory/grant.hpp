// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INVENTORY_GRANT_HPP
#define ZENGINE_INVENTORY_GRANT_HPP
#include "inventory/vocabulary.hpp"
#include <zen/weave/describe.hpp>
#include <zen/weave/poke.hpp>
#include <zen/weave/standard_shapes.hpp>

namespace zengine::inventory {
// A host may choose this bounded grant for the inventory role. The artifact's
// Emit declaration describes speech; it never grants itself authority.
inline loom::Grant inventory_grant() {
    loom::Grant g;
    g.allow_to_any(loom::PokeDescribe::zen_name, loom::PokeDescribe::zen_version);
    g.allow_to_any(InventoryState::zen_name, InventoryState::zen_version);
    g.allow_to_any(InventoryCaptured::zen_name, InventoryCaptured::zen_version);
    g.allow_to_any(loom::Ack::zen_name, loom::Ack::zen_version);
    g.allow_to_any(loom::Refused::zen_name, loom::Refused::zen_version);
    loom::allow_poke_answers(g);
    loom::allow_describe_answers(g);
    return g;
}

} // namespace zengine::inventory
#endif
