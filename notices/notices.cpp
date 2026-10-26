// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The notices record: a loadable weave that keeps what Workshop said, as Workshop said it
// (`NoticeSaid`), up to its office-wide `keep` setting, the oldest let go past it; says when the
// record moved (`NoticesMoved`); and answers a page of it to whoever asks (`NoticesRequested`).
// It shows nothing: the Notices pane walks it. It lasts the run, and a reload in place keeps it.
// Workshop law: agents/workshop/notices.md

#include "notices/vocabulary.hpp"

#include "workshop/notice_seam_vocabulary.hpp"
#include "workshop/pane_settings.hpp"
#include "workshop/settings_seam_vocabulary.hpp"

#include "activation/activation.hpp"
#include "manual/vocabulary.hpp"

#include "manuals/zengine.notices.hpp" // this office's manual, compiled in from its page

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

/// WHO SAYS THE NOTICES: Workshop's office, spelled as a stranger spells it.
constexpr const char* kWorkshopRole = "zengine.workshop";

/// THE WORDS AS KEPT: cut at `kMaxNoticeBytes`, never inside a character, ending `...`.
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
          loom::Accept<loom::Activated, ws::NoticeSaid, nt::NoticesRequested, ws::OfficeSettings>,
          loom::Emit<nt::NoticesMoved, nt::NoticesShown, ws::OfficeSettingsDeclared>> {
    using Base = zengine::With<Manual>::Weave<
        NoticesWeave, nt::NoticesState,
        loom::Accept<loom::Activated, ws::NoticeSaid, nt::NoticesRequested, ws::OfficeSettings>,
        loom::Emit<nt::NoticesMoved, nt::NoticesShown, ws::OfficeSettingsDeclared>>;

public:
    using Base::on;

    /// EVERY ACTIVATION: the setting declared to the office that keeps it, which hands it back, and
    /// the record said to have moved, so a reader that asked of an earlier life asks again.
    void on(const loom::Activated& activated, loom::Mail& mail) {
        if (!activation_.accept(mail, activated)) {
            return;
        }
        ws::v2::PaneSettingRow keep;
        keep.key = nt::kSettingKeep;
        keep.number = nt::kDefaultKept;
        keep.low = nt::kDefaultKept;
        keep.high = nt::kMostKept;
        keep.tags = {"history"};
        keep.summary = "how many of Workshop's notices are kept";
        (void)mail.as_role(nt::kNoticesRole)
            .send_to_role(ws::kLayoutsRole, ws::OfficeSettingsDeclared{{keep}});
        moved(mail);
    }

    /// ONE THING WORKSHOP SAID, kept as heard, after the newest: from Workshop alone, which says each
    /// notice once.
    // WL-NOTE-02 -- agents/workshop/notices.md
    void on(const ws::NoticeSaid& said, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || said.number <= 0) {
            return;
        }
        std::vector<nt::KeptNotice>& kept = state_.kept;
        kept.push_back(nt::KeptNotice{said.number, kept_text(said.text), said.refusal});
        let_go_past_keep();
        moved(mail);
    }

    /// THE SETTING AS THE LAYOUTS' OWNER KEEPS IT: a default is kept as its absence, so none is
    /// `kDefaultKept`. Fewer than the record holds lets the oldest go now.
    void on(const ws::OfficeSettings& kept, loom::Mail& mail) {
        if (!mail.authored_from_role(ws::kLayoutsRole)) {
            return;
        }
        std::int64_t keep = nt::kDefaultKept;
        for (const ws::PaneSetting& s : kept.settings) {
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

    /// A PAGE OF THE RECORD, to the asker alone: at most `kMaxNoticesPage` notices ending at the
    /// newest kept at or before `newest`, oldest first, and where they stand in what is kept.
    void on(const nt::NoticesRequested& asked, loom::Mail& mail) {
        const std::vector<nt::KeptNotice>& kept = state_.kept;
        nt::NoticesShown page;
        page.first = kept.empty() ? 0 : kept.front().number;
        page.last = kept.empty() ? 0 : kept.back().number;
        page.keep = keep_;
        page.let_go = state_.let_go;
        // THE END OF THE PAGE: one past the newest kept at or before `newest`.
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
    /// THE OLDEST LET GO PAST `keep_`, counted -- past `kMostKept` while the setting has not been
    /// handed in this life, so a reload keeps what the setting it held kept.
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

    /// THE RECORD MOVED, said to any.
    void moved(loom::Mail& mail) {
        const std::vector<nt::KeptNotice>& kept = state_.kept;
        (void)mail.as_role(nt::kNoticesRole)
            .publish(nt::NoticesMoved{kept.empty() ? 0 : kept.front().number,
                                      kept.empty() ? 0 : kept.back().number, keep_,
                                      state_.let_go});
    }

    /// HOW MANY THE RECORD KEEPS, as its setting was last handed: a plain member, handed again
    /// after a reload.
    std::int64_t keep_ = nt::kDefaultKept;
    bool heard_ = false;
    zengine::ActivationCursor activation_;
};

} // namespace

ZEN_EXPORT_WEAVE(NoticesWeave)
