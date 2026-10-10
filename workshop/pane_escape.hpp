// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANE_ESCAPE_HPP
#define ZENGINE_WORKSHOP_PANE_ESCAPE_HPP

// Escape's default in a pane that holds the keys, installed beside the protocol
// (`workshop/pane_vocabulary.hpp`): a bare Escape drops what the pane has selected, and with
// nothing selected goes back to Workshop unspent, so Workshop puts the pane down. A holder that
// sends the word lists `PaneEscapeUnspent` in its `Emit<...>`; Workshop sends a bare Escape to no
// other holder, so a pane that never mentions Escape is put down by it without asking.
// Reference: workshop/docs/workshop-panes.md#escape-in-a-pane-that-holds-the-keys
//
//     void on(const PaneKey& key, loom::Mail& mail) {
//         if (pane_escape::answer(key, mail, kOffice, [&] { return drop_selection(mail); }))
//             return;                                   // dropped, or handed back unspent
//         ...                                           // the pane's own keys
//     }

#include "input/vocabulary.hpp"
#include "workshop/pane_vocabulary.hpp"

#include <zen/weave.hpp>

#include <string>
#include <string_view>

namespace zengine::workshop::pane_escape {

/// Escape with no modifier: the only key this default answers.
inline bool bare(const PaneKey& key) noexcept {
    return key.scancode == input::scan::kEscape && key.modifiers == input::mod::kNone;
}

/// Hand the Escape this delivery carried back to Workshop unspent, echoing its number, from
/// `office`, which offered `pane` (WL-ARR-15).
inline void unspent(loom::Mail& mail, std::string_view office, const std::string& pane) {
    (void)mail.as_role(office).send_to_role("zengine.workshop", PaneEscapeUnspent{pane},
                                            mail.correlation());
}

/// The default answer to `key`. False for any key but a bare Escape, which the caller handles as
/// before; true once a bare Escape is answered: `drop` lets go of what the pane has selected and
/// says whether there was anything, and when there was not the Escape goes back unspent.
template <class Drop>
bool answer(const PaneKey& key, loom::Mail& mail, std::string_view office, Drop&& drop) {
    if (!bare(key)) {
        return false;
    }
    if (!drop()) {
        unspent(mail, office, key.pane);
    }
    return true;
}

} // namespace zengine::workshop::pane_escape

#endif // ZENGINE_WORKSHOP_PANE_ESCAPE_HPP
