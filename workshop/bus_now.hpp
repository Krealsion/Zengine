// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_BUS_NOW_HPP
#define ZENGINE_WORKSHOP_BUS_NOW_HPP

// Who is on the bus now, as one delivery of Workshop's weave reads it: Loom's participants claim
// (`loom::host::Participants`), read from the delivery's own Mail at its first need and forgotten
// at its end, so no holder's fact is carried from one delivery to the next. What a weave asks of
// the bus -- who holds a role, at which incarnation, what that holder accepts and declares it
// emits -- is answered from the one table the delivery read.
// Workshop law: agents/workshop/desk-read.md

#include <zen/host/participants.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace zengine::workshop {

/// The participants one delivery read. Null from `read` when the claim cannot be read -- no
/// claimant is mounted, the reader is not granted to observe it, or the table is too large to be
/// given whole -- and then nobody holds a role, as far as the reader can tell.
// WL-READ-02 -- agents/workshop/desk-read.md
class BusNow {
public:
    /// The table, read from `mail` at the first need of its delivery; a second need in the same
    /// delivery reads what the first read. A Mail the weave was not handed (a case driving it from
    /// another weave's delivery) is read for itself.
    const loom::host::Participants* read(loom::Mail& mail) {
        if (mail_ != &mail) {
            mail_ = &mail;
            table_ = loom::host::read_participants(mail);
        }
        return table_ ? &*table_ : nullptr;
    }

    /// The delivery ended: the next one reads the claim afresh.
    void forget() {
        table_.reset();
        mail_ = nullptr;
    }

    /// Who holds `role` now; invalid when nobody does, or when the claim cannot be read.
    loom::WeaveId holder_of(std::string_view role, loom::Mail& mail) {
        const loom::host::Participants* now = read(mail);
        const loom::host::Participant* row = now != nullptr ? now->holder_of(role) : nullptr;
        return row != nullptr ? loom::WeaveId{static_cast<std::uint64_t>(row->id)}
                              : loom::WeaveId{};
    }

    /// The incarnation `who` is at now, which a reload in place moves; 0 for one not on the bus.
    std::int64_t incarnation_of(loom::WeaveId who, loom::Mail& mail) {
        const loom::host::Participants* now = read(mail);
        const loom::host::Participant* row = now != nullptr ? now->find(who) : nullptr;
        return row != nullptr ? row->incarnation : 0;
    }

    /// Does whoever holds `role` now accept `shape`? It picks which published version to send and
    /// promises nothing about delivery; nobody holding it answers no.
    bool holder_accepts(std::string_view role, const loom::Schema& shape, loom::Mail& mail) {
        const loom::host::Participants* now = read(mail);
        return now != nullptr && now->holder_accepts(role, shape);
    }

    /// Does whoever holds `role` now declare it emits `shape`? Nobody holding it answers no.
    // WL-ARR-16 -- agents/workshop/arrangement.md
    bool holder_emits(std::string_view role, const loom::Schema& shape, loom::Mail& mail) {
        const loom::host::Participants* now = read(mail);
        return now != nullptr && now->holder_emits(role, shape);
    }

private:
    std::optional<loom::host::Participants> table_;
    const loom::Mail* mail_ = nullptr;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_BUS_NOW_HPP
