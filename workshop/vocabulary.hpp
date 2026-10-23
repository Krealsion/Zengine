// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_VOCABULARY_HPP

// The Workshop host weave's state shape, empty: nothing the host holds is weave state. The desk,
// keymap, preferences and session are files; the rest is `Session`.

#include <zen/weave/shape.hpp>

namespace zengine::workshop {

/// The host weave's state: nothing.
struct WorkshopState {
    ZEN_SHAPE(WorkshopState, 1);
};

/// The host's fence behind a menu it withdrew, authored as its office. Its second hop comes round
/// behind every refusal the menu's earlier sentences produced, so the record of who is owed an
/// answer is forgotten only then. It orders the bus; it proves no answer.
struct WithdrawalFence {
    std::int64_t menu = 0; ///< the withdrawn menu's number
    std::int64_t hop = 0;  ///< 1 on its way round the first time, 2 the second
    ZEN_SHAPE(WithdrawalFence, 1, ZEN_FIELD(menu), ZEN_FIELD(hop));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_VOCABULARY_HPP
