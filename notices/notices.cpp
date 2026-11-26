// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The notices record, a loadable weave answering as `zengine.notices`, keeps what Workshop says
// (`NoticeSaid`) as Workshop said it, up to its role-wide `keep` setting, and lets the oldest go
// past it. It says when the record moved (`NoticesMoved`) and answers a page of it to whoever asks
// (`NoticesRequested`). It shows nothing: the Notices pane walks it. The record lasts the run, and
// a reload in place keeps it. Its laws are notices/docs/notices-laws.md.

#include "notices/vocabulary.hpp"

#include "workshop/notice_seam_vocabulary.hpp"
#include "pane/settings.hpp"
#include "workshop/settings_seam_vocabulary.hpp"

#include "activation/activation.hpp"
#include "manual/vocabulary.hpp"

#include "manuals/zengine.notices.hpp" // this role's manual, compiled in from its page
#include "pane/menu.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace ws = zengine::workshop;
namespace nt = zengine::notices;

/// Workshop's role, as the pane protocol names it: the one sender whose notices are kept.
using zengine::pane::pane_menu::kWorkshopRole;

/// The words as kept. A notice longer than `kMaxNoticeBytes` is cut to fit with the `...` that
/// ends it, and never inside a character.
std::string kept_text(std::string text) {
    if (text.size() <= nt::kMaxNoticeBytes) {
        return text;
    }
    std::size_t cut = nt::kMaxNoticeBytes - 3;
    while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0U) == 0x80U) {
        --cut;
    }
    text.resize(cut);
    return text + "...";
}

using Manual = zengine::manual::ManualFacet<zengine::manual::pages::zengine_notices::kManual>;

class NoticesWeave
    : public zengine::With<Manual>::Weave<
          NoticesWeave, nt::NoticesState,
          loom::Accept<loom::Activated, ws::NoticeSaid, nt::NoticesRequested, ws::RoleSettings>,
          loom::Emit<nt::NoticesMoved, nt::NoticesShown, ws::RoleSettingsDeclared>> {
    using Base = zengine::With<Manual>::Weave<
        NoticesWeave, nt::NoticesState,
        loom::Accept<loom::Activated, ws::NoticeSaid, nt::NoticesRequested, ws::RoleSettings>,
        loom::Emit<nt::NoticesMoved, nt::NoticesShown, ws::RoleSettingsDeclared>>;

public:
    using Base::on;

    /// On every activation the record declares its setting to the role that keeps it, which hands
    /// it back, and says it moved, so a reader that asked an earlier life of it asks again.
    void on(const loom::Activated& activated, loom::Mail& mail) {
        if (!activation_.accept(mail, activated)) {
            return;
        }
        zengine::pane::v2::PaneSettingRow keep;
        keep.key = nt::kSettingKeep;
        keep.number = nt::kDefaultKept;
        keep.low = nt::kDefaultKept;
        keep.high = nt::kMostKept;
        keep.tags = {"history"};
        keep.summary = "how many of Workshop's notices are kept";
        (void)mail.as_role(nt::kNoticesRole)
            .send_to_role(zengine::pane::kLayoutsRole, ws::RoleSettingsDeclared{{keep}});
        moved(mail);
    }

    /// One thing Workshop said, kept as it was heard, after the newest. Only what Workshop says is
    /// kept, never what another weave says, and Workshop says each notice once, so none is kept
    /// twice (WL-NOTE-02). A notice numbered below 1 is none.
    void on(const ws::NoticeSaid& said, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || said.number <= 0) {
            return;
        }
        std::vector<nt::KeptNotice>& kept = state_.kept;
        kept.push_back(nt::KeptNotice{said.number, kept_text(said.text), said.refusal});
        let_go_past_keep();
        moved(mail);
    }

    /// The setting as the layouts' owner keeps it. A default is kept as its absence, so no setting
    /// means `kDefaultKept`, and any other is held to `kDefaultKept` to `kMostKept`. A `keep` below
    /// what the record holds lets the oldest go now; the same `keep` again changes nothing.
    void on(const ws::RoleSettings& kept, loom::Mail& mail) {
        if (!mail.authored_from_role(zengine::pane::kLayoutsRole)) {
            return;
        }
        std::int64_t keep = nt::kDefaultKept;
        for (const zengine::pane::PaneSetting& s : kept.settings) {
            if (s.key == nt::kSettingKeep && s.number) {
                keep = std::clamp(*s.number, nt::kDefaultKept, nt::kMostKept);
            }
        }
        if (heard_ && keep == keep_) {
            return;
        }
        heard_ = true;
        keep_ = keep;
        let_go_past_keep();
        moved(mail);
    }

    /// Answers a page of the record to the asker alone: at most `kMaxNoticesPage` notices, oldest
    /// first, ending at the newest kept at or before `newest`, and where they stand among those
    /// kept.
    void on(const nt::NoticesRequested& asked, loom::Mail& mail) {
        const std::vector<nt::KeptNotice>& kept = state_.kept;
        nt::NoticesShown page;
        page.first = kept.empty() ? 0 : kept.front().number;
        page.last = kept.empty() ? 0 : kept.back().number;
        page.keep = keep_;
        page.let_go = state_.let_go;
        // The page ends one past the newest notice kept at or before `newest`.
        std::size_t end = kept.size();
        if (asked.newest > 0) {
            end = 0;
            while (end < kept.size() && kept[end].number <= asked.newest) {
                ++end;
            }
        }
        const std::size_t count = static_cast<std::size_t>(
            std::clamp<std::int64_t>(asked.count, 0, nt::kMaxNoticesPage));
        const std::size_t begin = end > count ? end - count : 0;
        page.rows.assign(kept.begin() + static_cast<std::ptrdiff_t>(begin),
                         kept.begin() + static_cast<std::ptrdiff_t>(end));
        page.before = static_cast<std::int64_t>(begin);
        page.after = static_cast<std::int64_t>(kept.size() - end);
        page.next = end < kept.size() ? kept[end].number : 0;
        (void)mail.answer(page);
    }

private:
    /// Lets the oldest go past `keep_`, and counts them. While the setting has not been handed in
    /// this life the bound is `kMostKept`, so a reload keeps what the setting it held kept.
    void let_go_past_keep() {
        std::vector<nt::KeptNotice>& kept = state_.kept;
        const std::size_t keep = static_cast<std::size_t>(heard_ ? keep_ : nt::kMostKept);
        if (kept.size() <= keep) {
            return;
        }
        const std::size_t gone = kept.size() - keep;
        kept.erase(kept.begin(), kept.begin() + static_cast<std::ptrdiff_t>(gone));
        state_.let_go += static_cast<std::int64_t>(gone);
    }

    /// Says to any that the record moved.
    void moved(loom::Mail& mail) {
        const std::vector<nt::KeptNotice>& kept = state_.kept;
        (void)mail.as_role(nt::kNoticesRole)
            .publish(nt::NoticesMoved{kept.empty() ? 0 : kept.front().number,
                                      kept.empty() ? 0 : kept.back().number, keep_,
                                      state_.let_go});
    }

    /// How many the record keeps, as its setting was last handed. It is a plain member, not state,
    /// because the setting is handed again after a reload.
    std::int64_t keep_ = nt::kDefaultKept;
    bool heard_ = false;
    zengine::ActivationCursor activation_;
};

} // namespace

ZEN_EXPORT_WEAVE(NoticesWeave)
