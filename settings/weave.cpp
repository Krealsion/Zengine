// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The loadable weave `zengine-settings`, holding the office `zengine.settings`: the one place a
// theme file is read. It offers each theme it finds as a palette, applies a theme when asked --
// judged whole, then written through the layouts' owner and the keymap's door, its look offered as
// the palette the layouts wear -- and keeps each office's slots, applying entries that waited.
// Reference: settings/docs/zengine.settings.md, its manual, compiled in and answered.

#include "settings/apply.hpp"
#include "settings/theme_persist.hpp"
#include "settings/vocabulary.hpp"

#include "activation/activation.hpp"
#include "graphics/vocabulary.hpp"
#include "manual/vocabulary.hpp"
#include "manuals/zengine.settings.hpp" // this office's manual, compiled in from its page
#include "workshop/desktop_seam_vocabulary.hpp"
#include "workshop/pane_settings.hpp"
#include "workshop/settings_seam_vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

namespace st = zengine::settings;
namespace ws = zengine::workshop;
namespace gfx = zengine::graphics;

/// The host's office, spelled as a stranger spells it.
constexpr const char* kWorkshopRole = "zengine.workshop";

/// How many themes the office offers as palettes at most, short of the directory's bound so the
/// host's own fit beside them.
constexpr std::size_t kMaxOfferedPalettes = gfx::kMaxPalettes - 8;

/// ONE OFFICE'S SLOT ENTRIES THAT WAIT FOR IT TO DECLARE, and the theme that gave them.
struct WaitingSlots {
    std::string theme;
    st::ThemeSlots slots;
    ZEN_SHAPE(WaitingSlots, 1, ZEN_FIELD(theme), ZEN_FIELD(slots));
};

/// ONE PALETTE THE OFFICE OFFERS: the theme it is named for, the palette as last offered, and the
/// slot entries of offices the plan names that wait for them to declare.
struct HeldPalette {
    std::string theme;
    gfx::PaletteOffered palette;
    std::vector<WaitingSlots> waiting;
    ZEN_SHAPE(HeldPalette, 1, ZEN_FIELD(theme), ZEN_FIELD(palette), ZEN_FIELD(waiting));
};

/// ONE OFFICE'S DECLARED SLOTS.
struct HeldSlots {
    std::string office;
    st::SlotsDeclared slots;
    ZEN_SHAPE(HeldSlots, 1, ZEN_FIELD(office), ZEN_FIELD(slots));
};

/// WHAT A RELOAD KEEPS: where the themes are, the last revision offered, every palette offered, and
/// each office's slots.
struct SettingsState {
    st::ThemesPlaced placed;
    bool placed_heard = false;
    std::int64_t revision = 1;
    std::vector<HeldPalette> palettes;
    std::vector<HeldSlots> declared;
    ZEN_SHAPE(SettingsState, 1, ZEN_FIELD(placed), ZEN_FIELD(placed_heard), ZEN_FIELD(revision),
              ZEN_FIELD(palettes), ZEN_FIELD(declared));
};

using Manual = zengine::manual::ManualFacet<zengine::manual::pages::zengine_settings::kManual>;

class SettingsWeave
    : public zengine::With<Manual>::Weave<
          SettingsWeave, SettingsState,
          loom::Accept<loom::Activated, st::ThemesPlaced, gfx::GraphicsReady, st::SlotsDeclared,
                       st::ThemesRequested, st::ThemeResolveRequested, st::ThemeApplyRequested,
                       st::ThemeSaveRequested, st::ThemeSnapshotRequested,
                       st::ThemePromoteRequested, st::ThemeProvenanceRequested,
                       ws::LayoutSettingsAnswered, ws::SettingsWriteAnswered, ws::KeymapShown,
                       ws::KeymapEditAnswered, loom::Refused>,
          loom::Emit<gfx::PaletteOffered, st::ThemesShown, st::ThemeResolved, st::ThemeActed,
                     st::ThemeSaved, st::ThemeProvenanceShown, st::ThemeSkipsSaid, st::SlotsAsked,
                   st::ThemesPlacedRequested,
                     ws::LayoutSettingsRequested, ws::SettingsWriteRequested,
                     ws::KeymapRequested, ws::KeymapEditRequested>> {
    using Base = zengine::With<Manual>::Weave<
        SettingsWeave, SettingsState,
        loom::Accept<loom::Activated, st::ThemesPlaced, gfx::GraphicsReady, st::SlotsDeclared,
                     st::ThemesRequested, st::ThemeResolveRequested, st::ThemeApplyRequested,
                     st::ThemeSaveRequested, st::ThemeSnapshotRequested, st::ThemePromoteRequested,
                     st::ThemeProvenanceRequested, ws::LayoutSettingsAnswered,
                     ws::SettingsWriteAnswered, ws::KeymapShown, ws::KeymapEditAnswered,
                     loom::Refused>,
        loom::Emit<gfx::PaletteOffered, st::ThemesShown, st::ThemeResolved, st::ThemeActed,
                   st::ThemeSaved, st::ThemeProvenanceShown, st::ThemeSkipsSaid, st::SlotsAsked,
                   st::ThemesPlacedRequested,
                   ws::LayoutSettingsRequested, ws::SettingsWriteRequested, ws::KeymapRequested,
                   ws::KeymapEditRequested>>;

public:
    using Base::on;

    /// EVERY ACTIVATION: what a reload kept is offered again, so a directory that heard nothing
    /// since paints from it.
    void on(const loom::Activated& activated, loom::Mail& mail) {
        if (!activation_.accept(mail, activated)) {
            return;
        }
        offer_held(mail);
        (void)mail.as_role(st::kSettingsRole).send_to_role(kWorkshopRole, st::ThemesPlacedRequested{});
        (void)mail.as_role(st::kSettingsRole).publish(st::SlotsAsked{});
    }

    /// THE HOST: where the themes are, the offices its plan names, and what its own areas wear.
    /// Each theme found not yet offered is offered, its look over the pool's defaults.
    void on(const st::ThemesPlaced& placed, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        state_.placed = placed;
        state_.placed_heard = true;
        for (const std::string& name : theme_names()) {
            if (held(name) == nullptr && state_.palettes.size() < kMaxOfferedPalettes) {
                const st::ThemeResolved r = resolve(name);
                if (r.accepted) {
                    keep(mail, name, whole(r), {});
                }
            }
        }
    }

    /// A DIRECTORY STARTED AFRESH: every palette offered again.
    void on(const gfx::GraphicsReady&, loom::Mail& mail) {
        if (!mail.authored_from_role(gfx::kGraphicsRole)) {
            return;
        }
        offer_held(mail);
    }

    /// AN OFFICE'S SLOTS, as that office: kept, and every theme's entries that waited for it
    /// judged now -- what it holds applied and offered again, the rest skipped and said.
    void on(const st::SlotsDeclared& declared, loom::Mail& mail) {
        const std::string office(mail.authored_role());
        if (office.empty() ||
            declared.colors.size() + declared.sizes.size() > st::kMaxDeclaredSlots) {
            return;
        }
        auto at = std::find_if(state_.declared.begin(), state_.declared.end(),
                               [&](const HeldSlots& h) { return h.office == office; });
        if (at == state_.declared.end()) {
            if (state_.declared.size() >= st::kMaxSlotOffices) {
                return; // past the offices one office keeps the slots of
            }
            state_.declared.push_back(HeldSlots{office, declared});
        } else {
            at->slots = declared;
        }
        const std::map<std::string, st::SlotsDeclared> slots = declared_slots();
        for (HeldPalette& p : state_.palettes) {
            const auto waited = std::find_if(p.waiting.begin(), p.waiting.end(),
                                             [&](const WaitingSlots& w) { return w.slots.office == office; });
            if (waited == p.waiting.end()) {
                continue;
            }
            auto [ready, skipped] = st::judge_waiting(waited->slots, declared);
            const std::string from = waited->theme;
            p.waiting.erase(waited);
            p.palette = st::look_over(std::move(p.palette), {}, {}, {ready}, slots);
            p.palette.revision = ++state_.revision;
            (void)mail.as_role(st::kSettingsRole).send_to_role(gfx::kGraphicsRole, p.palette);
            if (!skipped.empty()) {
                (void)mail.as_role(st::kSettingsRole)
                    .publish(st::ThemeSkipsSaid{from, p.palette.palette, std::move(skipped)});
            }
        }
    }

    /// EVERY THEME IN THE FOLDERS, a weaver's own over a shipped one of its name, to the asker.
    void on(const st::ThemesRequested&, loom::Mail& mail) {
        std::map<std::string, st::ThemeSummary> found;
        for (const std::string& dir : {state_.placed.shipped, state_.placed.themes}) {
            if (dir.empty()) {
                continue;
            }
            for (st::ThemeSummary& s : st::list_themes(dir)) {
                found[s.name] = std::move(s);
            }
        }
        st::ThemesShown shown;
        for (auto& [name, s] : found) {
            shown.themes.push_back(std::move(s));
        }
        (void)mail.answer(shown);
    }

    /// ONE THEME RESOLVED WITH ITS INCLUDES, applying nothing.
    void on(const st::ThemeResolveRequested& asked, loom::Mail& mail) {
        (void)mail.answer(resolve(asked.theme));
    }

    /// A THEME SAVED AMONG THE WEAVER'S OWN, by the safe write; refused where the host names no
    /// folder of the weaver's.
    void on(const st::ThemeSaveRequested& asked, loom::Mail& mail) {
        st::ThemeSaved saved;
        saved.name = asked.theme.name;
        if (state_.placed.themes.empty()) {
            saved.refusal = "this Workshop keeps no themes of the weaver's (an isolated launch keeps "
                            "none) -- nothing was written";
            (void)mail.answer(saved);
            return;
        }
        const st::Written w = st::save_file(state_.placed.themes, asked.theme, asked.replace);
        saved.accepted = w.accepted;
        saved.refusal = w.refusal;
        saved.path = w.accepted ? st::theme_path(state_.placed.themes, asked.theme.name) : "";
        (void)mail.answer(saved);
    }

    /// The doors this office does not answer yet, refused out loud.
    void on(const st::ThemeSnapshotRequested&, loom::Mail& mail) {
        (void)mail.answer(st::ThemeSaved{false, kNotYet, {}, {}});
    }
    void on(const st::ThemePromoteRequested&, loom::Mail& mail) {
        (void)mail.answer(st::ThemeSaved{false, kNotYet, {}, {}});
    }
    void on(const st::ThemeProvenanceRequested& asked, loom::Mail& mail) {
        st::ThemeProvenanceShown shown;
        shown.refusal = kNotYet;
        shown.layout = asked.layout;
        (void)mail.answer(shown);
    }

    /// APPLY A THEME: resolved now, from the file as it stands; then the layouts and the keymap are
    /// read, and the whole theme judged against them before anything is written.
    void on(const st::ThemeApplyRequested& asked, loom::Mail& mail) {
        st::ThemeActed acted;
        acted.theme = asked.theme;
        if (!asked.mode.empty() && asked.mode != st::kApplyMerge && asked.mode != st::kApplyReset) {
            acted.refusal = "a theme applies by `merge` or `reset`, not `" + asked.mode + "`";
            (void)mail.answer(acted);
            return;
        }
        if (!state_.placed_heard) {
            acted.refusal = "no host has said where its themes are yet";
            (void)mail.answer(acted);
            return;
        }
        if (applies_.size() >= st::kMaxAppliesInFlight) {
            acted.refusal = "already applying " + std::to_string(applies_.size()) +
                            " themes -- ask again once one has answered";
            (void)mail.answer(acted);
            return;
        }
        st::ThemeResolved resolved = resolve(asked.theme);
        if (!resolved.accepted) {
            acted.refusal = resolved.refusal;
            (void)mail.answer(acted);
            return;
        }
        Apply a;
        a.asked = asked;
        if (a.asked.mode.empty()) {
            a.asked.mode = st::kApplyMerge;
        }
        a.resolved = std::move(resolved);
        a.acted = std::move(acted);
        a.answer = mail.defer_answer();
        applies_.push_back(std::move(a));
        read_world(mail, applies_.back());
    }

    /// THE LAYOUTS AS THEY STAND, for the apply that asked.
    void on(const ws::LayoutSettingsAnswered& shown, loom::Mail& mail) {
        Apply* a = asking(mail, &Apply::layouts_question);
        if (a == nullptr) {
            return;
        }
        a->layouts_question = 0;
        a->world.layouts = shown;
        a->layouts_heard = true;
        judged(mail, *a);
    }

    /// THE KEYMAP AS IT STANDS, for the apply that asked; one said to all is no answer.
    void on(const ws::KeymapShown& shown, loom::Mail& mail) {
        Apply* a = asking(mail, &Apply::keymap_question);
        if (a == nullptr) {
            return;
        }
        a->keymap_question = 0;
        a->world.keymap = shown;
        a->keymap_heard = true;
        judged(mail, *a);
    }

    /// WHAT THE BATCH CAME TO: a stale reading is read again once, then refused aloud.
    void on(const ws::SettingsWriteAnswered& written, loom::Mail& mail) {
        Apply* a = asking(mail, &Apply::write_question);
        if (a == nullptr) {
            return;
        }
        a->write_question = 0;
        if (!written.accepted) {
            if (written.refusal.find("stale revision") != std::string::npos && !a->read_again) {
                a->read_again = true;
                read_world(mail, *a);
                return;
            }
            refuse(mail, *a, "theme `" + a->asked.theme + "`: the layouts refused its settings -- " +
                                 written.refusal);
            return;
        }
        for (const ws::SettingWriteResult& r : written.results) {
            const auto i = static_cast<std::size_t>(r.index);
            if (i >= a->plan.writes.size()) {
                continue;
            }
            const ws::SettingWrite& w = a->plan.writes[i];
            if (r.outcome == ws::kWriteWritten) {
                ++a->acted.written;
            } else if (r.outcome == ws::kWriteUnchanged) {
                ++a->acted.unchanged;
            } else if (r.outcome == ws::kWriteLocked) {
                a->acted.locked.push_back(st::ThemeSkip{w.layout, w.target, w.key, r.said});
            } else {
                a->acted.skipped.push_back(st::ThemeSkip{w.layout, w.target, w.key, r.said});
            }
        }
        a->acted.baseline = !a->plan.baselines.empty();
        send_keys(mail, *a);
    }

    /// WHAT ONE KEY EDIT CAME TO.
    void on(const ws::KeymapEditAnswered& edited, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return;
        }
        const auto at = std::find_if(applies_.begin(), applies_.end(), [&](const Apply& a) {
            return std::find(a.key_questions.begin(), a.key_questions.end(), mail.correlation()) !=
                   a.key_questions.end();
        });
        if (at == applies_.end()) {
            return;
        }
        Apply& a = *at;
        a.key_questions.erase(
            std::find(a.key_questions.begin(), a.key_questions.end(), mail.correlation()));
        if (edited.accepted) {
            a.keyed.insert(edited.action);
        } else {
            a.acted.skipped.push_back(st::ThemeSkip{0, "keys", edited.action, edited.refusal});
        }
        if (a.key_questions.empty()) {
            finish(mail, a);
        }
    }

    /// A QUESTION REFUSED OUT LOUD: the apply that asked it is refused, saying so.
    void on(const loom::Refused& refused, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return;
        }
        for (Apply& a : applies_) {
            if (a.layouts_question == mail.correlation() || a.keymap_question == mail.correlation() ||
                a.write_question == mail.correlation() ||
                std::find(a.key_questions.begin(), a.key_questions.end(), mail.correlation()) !=
                    a.key_questions.end()) {
                refuse(mail, a, "theme `" + a.asked.theme + "`: " + refused.reason);
                return;
            }
        }
    }

private:
    static constexpr const char* kNotYet = "this office does not answer this door yet";

    /// ONE APPLY IN FLIGHT: what was asked and resolved, the questions it waits on, what it read,
    /// its plan, and the answer it owes.
    struct Apply {
        st::ThemeApplyRequested asked;
        st::ThemeResolved resolved;
        st::ApplyWorld world;
        bool layouts_heard = false;
        bool keymap_heard = false;
        bool read_again = false;
        std::uint64_t layouts_question = 0;
        std::uint64_t keymap_question = 0;
        std::uint64_t write_question = 0;
        std::vector<std::uint64_t> key_questions;
        std::set<std::string> keyed;
        st::ApplyPlan plan;
        st::ThemeActed acted;
        loom::DeferredAnswer answer;
    };

    /// The apply whose question `field` this answer answers, as Loom verified it.
    Apply* asking(const loom::Mail& mail, std::uint64_t Apply::*field) {
        if (!mail.answers_ask()) {
            return nullptr;
        }
        for (Apply& a : applies_) {
            if (a.*field != 0 && a.*field == mail.correlation()) {
                return &a;
            }
        }
        return nullptr;
    }

    void read_world(loom::Mail& mail, Apply& a) {
        a.layouts_heard = false;
        a.keymap_heard = false;
        a.layouts_question = ++questions_;
        a.keymap_question = ++questions_;
        const loom::Ticket layouts = mail.as_role(st::kSettingsRole)
                                         .send_to_role(ws::kLayoutsRole, ws::LayoutSettingsRequested{},
                                                       a.layouts_question);
        const loom::Ticket keymap = mail.as_role(st::kSettingsRole)
                                        .send_to_role(kWorkshopRole, ws::KeymapRequested{},
                                                      a.keymap_question);
        if (!layouts.valid() || !keymap.valid()) {
            refuse(mail, a, "theme `" + a.asked.theme +
                                "`: the layouts' owner and the keymap could not be asked");
        }
    }

    /// BOTH READINGS IN: the theme judged whole; refused, nothing is written.
    void judged(loom::Mail& mail, Apply& a) {
        if (!a.layouts_heard || !a.keymap_heard) {
            return;
        }
        if (a.world.layouts.revision == 0) {
            refuse(mail, a, "theme `" + a.asked.theme + "`: the layouts' owner withheld the layouts");
            return;
        }
        a.world.pool_colors = st::pool_color_names();
        a.world.pool_fonts = st::pool_font_names();
        a.world.pool_sizes = st::pool_size_names();
        a.world.declared = declared_slots();
        a.world.planned = state_.placed.planned;
        a.plan = st::plan_apply(a.resolved, a.asked, a.world);
        if (!a.plan.accepted) {
            refuse(mail, a, a.plan.refusal);
            return;
        }
        a.acted.skipped = a.plan.skipped;
        if (a.plan.writes.empty() && a.plan.baselines.empty()) {
            send_keys(mail, a);
            return;
        }
        a.write_question = ++questions_;
        ws::SettingsWriteRequested batch;
        batch.revision = a.world.layouts.revision;
        batch.writes = a.plan.writes;
        batch.baselines = a.plan.baselines;
        batch.cause = "theme " + a.asked.theme + " applied";
        const loom::Ticket sent =
            mail.as_role(st::kSettingsRole).send_to_role(ws::kLayoutsRole, batch, a.write_question);
        if (!sent.valid()) {
            refuse(mail, a, "theme `" + a.asked.theme + "`: the layouts' owner could not be asked");
        }
    }

    void send_keys(loom::Mail& mail, Apply& a) {
        for (const st::KeyEdit& k : a.plan.keys) {
            const std::uint64_t q = ++questions_;
            ws::KeymapEditRequested edit;
            edit.action = k.action;
            edit.op = k.op;
            edit.text = k.text;
            const loom::Ticket sent =
                mail.as_role(st::kSettingsRole).send_to_role(kWorkshopRole, edit, q);
            if (sent.valid()) {
                a.key_questions.push_back(q);
            } else {
                a.acted.skipped.push_back(
                    st::ThemeSkip{0, "keys", k.action, "the keymap could not be asked"});
            }
        }
        if (a.key_questions.empty()) {
            finish(mail, a);
        }
    }

    /// EVERYTHING WRITTEN: the look offered as the palette each layout wears, and the answer.
    void finish(loom::Mail& mail, Apply& a) {
        a.acted.keys = static_cast<std::int64_t>(a.keyed.size());
        const std::map<std::string, st::SlotsDeclared> slots = declared_slots();
        const bool reset = a.asked.mode == st::kApplyReset;
        std::set<std::string> offered;
        for (const std::int64_t id : a.plan.layouts) {
            std::string worn = state_.placed.worn;
            bool live = false;
            for (const ws::LayoutSettings& l : a.world.layouts.layouts) {
                if (l.id == id) {
                    if (!l.theme.name.empty()) worn = l.theme.name;
                    live = l.position == a.world.layouts.active;
                }
            }
            const std::string name = a.asked.baseline ? a.asked.theme : worn;
            if (live || a.acted.palette.empty()) {
                a.acted.palette = name;
            }
            if (worn.empty() || !offered.insert(name).second) {
                continue;
            }
            gfx::PaletteOffered base = st::default_palette(name);
            std::vector<WaitingSlots> waiting;
            if (!reset) {
                if (const HeldPalette* was = held(worn)) {
                    base = was->palette;
                    waiting = was->waiting;
                } else if (const st::ThemeResolved r = resolve(worn); r.accepted) {
                    base = whole(r);
                }
                base.palette = name;
            }
            for (const st::ThemeSlots& w : a.plan.waiting) {
                waiting.erase(std::remove_if(waiting.begin(), waiting.end(),
                                             [&](const WaitingSlots& s) { return s.slots.office == w.office; }),
                              waiting.end());
                waiting.push_back(WaitingSlots{a.asked.theme, w});
            }
            keep(mail, name,
                 st::look_over(std::move(base), a.plan.outline, a.plan.colors, a.plan.slots, slots,
                               a.plan.fonts, a.plan.sizes),
                 std::move(waiting));
        }
        a.acted.revision = state_.revision;
        for (const st::ThemeSlots& w : a.plan.waiting) {
            a.acted.waiting.push_back(w.office);
        }
        a.acted.accepted = true;
        (void)loom::answer_deferred(a.answer, mail, a.acted);
        drop(a);
    }

    void refuse(loom::Mail& mail, Apply& a, std::string why) {
        a.acted.accepted = false;
        a.acted.refusal = std::move(why);
        a.acted.written = 0;
        a.acted.unchanged = 0;
        (void)loom::answer_deferred(a.answer, mail, a.acted);
        drop(a);
    }

    void drop(const Apply& a) {
        applies_.erase(std::find_if(applies_.begin(), applies_.end(),
                                    [&](const Apply& k) { return &k == &a; }));
    }

    /// Theme `name` resolved from the folders, the weaver's own first.
    st::ThemeResolved resolve(const std::string& name) const {
        const std::string user = state_.placed.themes;
        const std::string shipped = state_.placed.shipped;
        return st::resolve_theme_with(name, [&](std::string_view n) {
            std::error_code ec;
            if (!user.empty() && std::filesystem::exists(st::theme_path(user, n), ec)) {
                return st::load_file(user, n);
            }
            if (shipped.empty()) {
                return st::LoadedTheme::no("no theme `" + std::string(n) + "` in " +
                                           (user.empty() ? std::string("this Workshop") : user));
            }
            return st::load_file(shipped, n);
        });
    }

    /// The name of every theme file in the folders, once each.
    std::vector<std::string> theme_names() const {
        std::set<std::string> names;
        for (const std::string& dir : {state_.placed.shipped, state_.placed.themes}) {
            if (dir.empty()) {
                continue;
            }
            for (const st::ThemeSummary& s : st::list_themes(dir)) {
                if (s.refusal.empty()) names.insert(s.name);
            }
        }
        return {names.begin(), names.end()};
    }

    /// A theme's whole look over the pool's defaults, as a launch offers it: the colours, fonts and
    /// sizes the pool names and each office's slots, judged when it is applied.
    gfx::PaletteOffered whole(const st::ThemeResolved& r) const {
        const auto named = [](const auto& items, const std::vector<std::string>& pool) {
            std::decay_t<decltype(items)> out;
            for (const auto& i : items) {
                if (std::find(pool.begin(), pool.end(), i.name) != pool.end()) out.push_back(i);
            }
            return out;
        };
        return st::look_over(st::default_palette(r.theme), r.outline,
                             named(r.colors, st::pool_color_names()), r.slots, declared_slots(),
                             named(r.fonts, st::pool_font_names()),
                             named(r.sizes, st::pool_size_names()));
    }

    std::map<std::string, st::SlotsDeclared> declared_slots() const {
        std::map<std::string, st::SlotsDeclared> out;
        for (const HeldSlots& h : state_.declared) {
            out[h.office] = h.slots;
        }
        return out;
    }

    const HeldPalette* held(const std::string& theme) const {
        for (const HeldPalette& p : state_.palettes) {
            if (p.theme == theme) return &p;
        }
        return nullptr;
    }

    /// A palette kept under `theme` at the next revision, and offered.
    void keep(loom::Mail& mail, const std::string& theme, gfx::PaletteOffered palette,
              std::vector<WaitingSlots> waiting) {
        palette.palette = theme;
        palette.revision = ++state_.revision;
        auto at = std::find_if(state_.palettes.begin(), state_.palettes.end(),
                               [&](const HeldPalette& p) { return p.theme == theme; });
        if (at == state_.palettes.end()) {
            if (state_.palettes.size() >= kMaxOfferedPalettes) {
                return;
            }
            state_.palettes.push_back(HeldPalette{theme, palette, std::move(waiting)});
        } else {
            at->palette = palette;
            at->waiting = std::move(waiting);
        }
        (void)mail.as_role(st::kSettingsRole).send_to_role(gfx::kGraphicsRole, palette);
    }

    void offer_held(loom::Mail& mail) {
        for (const HeldPalette& p : state_.palettes) {
            (void)mail.as_role(st::kSettingsRole).send_to_role(gfx::kGraphicsRole, p.palette);
        }
    }

    std::vector<Apply> applies_;
    std::uint64_t questions_ = 0;
    zengine::ActivationCursor activation_;
};

} // namespace

ZEN_EXPORT_WEAVE(SettingsWeave)
