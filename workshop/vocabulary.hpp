// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_VOCABULARY_HPP

// The Workshop host weave's state shape, empty: nothing the host holds is weave state. The desk,
// keymap, preferences, session and a maker-made pane are files; the rest is `Session`.

#include <zen/weave/shape.hpp>

namespace zengine::workshop {

/// The host weave's state: nothing.
struct WorkshopState {
    ZEN_SHAPE(WorkshopState, 1);
};

/// The host's own fence behind a picture it handed the medium, authored as its office: a numbered
/// picture becomes the press stamp only once this has come round twice behind the canvas that
/// first showed it (`PictureStamp`). Not seen: the medium's latency after that, or a press the
/// platform buffered before the input beat read it.
struct PictureFence {
    std::int64_t number = 0; ///< which fence: every picture handed out before it is covered
    std::int64_t hop = 0;    ///< 1 on its way round the first time, 2 the second
    ZEN_SHAPE(PictureFence, 1, ZEN_FIELD(number), ZEN_FIELD(hop));
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
