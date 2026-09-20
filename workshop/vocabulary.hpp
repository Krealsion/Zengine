// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_VOCABULARY_HPP

// The Workshop host weave's STATE SHAPE -- what a same-shape revive restores.
//
// ⭐ IT WAS THE PROTOTYPE OBJECT DOCUMENT (`WorkshopDoc` v2: the authored objects and their
// identity mint) UNTIL THAT RETIRED WITH ITS CANVAS. Nothing the host holds now is weave state:
// the desk, the keymap, the preferences, the session and a maker-made pane are FILES a maker
// owns, and everything else is `Session` -- what a maker is doing, which a revive is entitled to
// keep in place and a new process starts fresh. So the shape is empty, and says so, rather than
// carrying a field nobody reads.
//
// AN OLD OBJECT DOCUMENT ON DISK IS NOT READ AS ANYTHING ELSE. `workshop.json`, or the file a
// `--document` names, is left exactly as it is and said once at startup
// (`HostContext::retired_document`): no door of this host opens, rewrites or deletes it.

#include <zen/weave/shape.hpp>

namespace zengine::workshop {

/// THE HOST WEAVE'S STATE: nothing -- see above.
struct WorkshopState {
    ZEN_SHAPE(WorkshopState, 1);
};

/// THE HOST'S OWN FENCE BEHIND A PICTURE IT HANDED THE MEDIUM (P-WORK-25, the queued-before-
/// admission case). Workshop -> Workshop, authored as its office, and never anybody else's
/// sentence: a picture a pane numbered becomes the one a press is STAMPED with only after this
/// fence has come round twice behind the canvas that first showed it. Delivery is single-threaded
/// FIFO and input is read by a delivery (the input weave's beat), so the first hop is handled
/// right after the medium handled that canvas, and the second is queued behind every press read
/// before it -- a press read while the older picture was still what the medium had been handed is
/// stamped with that older one, and a pane refuses it as moved rather than resolving it against
/// the row that moved in. What this cannot see is the medium's own latency after it handled the
/// canvas, nor a press the platform buffered before the beat read it; those stay named residue.
struct PictureFence {
    std::int64_t number = 0; ///< which fence: every picture handed out before it is covered
    std::int64_t hop = 0;    ///< 1 on its way round the first time, 2 the second
    ZEN_SHAPE(PictureFence, 1, ZEN_FIELD(number), ZEN_FIELD(hop));
};

/// THE HOST'S FENCE BEHIND A MENU IT WITHDREW (`WithdrawnMenu`). Workshop -> Workshop, authored as
/// its office, and never anybody else's sentence. Loom appends a `zen.DispatchRefused` for a
/// refused send when that send is dispatched, and every sentence about the menu -- its grant, the
/// acts forwarded to it, the withdrawal -- was queued no later than the withdrawal, ahead of this
/// fence's first hop. The second hop is queued when the first is handled, so it comes round behind
/// every refusal those sentences produced: the record of who is owed an answer is forgotten then,
/// with nothing left to hear. Ordering on the bus is all it establishes -- that a presenter
/// received a withdrawal, not that it answered.
struct WithdrawalFence {
    std::int64_t menu = 0; ///< the withdrawn menu's number
    std::int64_t hop = 0;  ///< 1 on its way round the first time, 2 the second
    ZEN_SHAPE(WithdrawalFence, 1, ZEN_FIELD(menu), ZEN_FIELD(hop));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_VOCABULARY_HPP
