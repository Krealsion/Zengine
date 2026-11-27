// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The loadable weave `zengine-theme` holds the role `zengine.theme`, and is the one place a theme
// file is read. It lays each stack a host's areas wear from the files as they stand -- its themes,
// bottom first, a link of the other medium converted and said -- and offers each to graphics as the
// palette named for the stack; it keeps each role's own values, and answers what the folders hold.
// It names no host, and a host where it runs offers no palette of its own, so each palette name
// has one owner (THEME-01). Its manual, theme/docs/zengine.theme.md, is compiled in and answered.

#include "theme/stack.hpp"
#include "theme/theme_persist.hpp"
#include "theme/vocabulary.hpp"

#include "activation/activation.hpp"
#include "graphics/vocabulary.hpp"
#include "manual/vocabulary.hpp"
#include "manuals/zengine.theme.hpp" // this role's manual, compiled in from its page

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

namespace th = zengine::theme;
namespace gfx = zengine::graphics;

/// How many roles' own values the weave keeps.
constexpr std::size_t kMaxOwnRoles = 64;

/// One role's own values, as it last declared them.
struct HeldOwn {
    std::string role;
    gfx::OwnValuesDeclared values;
    ZEN_SHAPE(HeldOwn, 1, ZEN_FIELD(role), ZEN_FIELD(values));
};

/// One stack as last laid: what laying said and the palette offered.
struct HeldStack {
    th::StackLaid laid;
    gfx::PaletteOffered palette;
    ZEN_SHAPE(HeldStack, 1, ZEN_FIELD(laid), ZEN_FIELD(palette));
};

/// What a reload keeps: where the themes are, what is worn, the last revision offered, each stack
/// as laid, and each role's own values.
struct ThemeState {
    th::ThemesPlaced placed;
    bool placed_heard = false;
    std::vector<th::ThemeStack> worn;
    std::int64_t revision = 0;
    std::vector<HeldStack> stacks;
    std::vector<HeldOwn> own;
    ZEN_SHAPE(ThemeState, 1, ZEN_FIELD(placed), ZEN_FIELD(placed_heard), ZEN_FIELD(worn),
              ZEN_FIELD(revision), ZEN_FIELD(stacks), ZEN_FIELD(own));
};

using Manual = zengine::manual::ManualFacet<zengine::manual::pages::zengine_theme::kManual>;

class ThemeWeave
    : public zengine::With<Manual>::Weave<
          ThemeWeave, ThemeState,
          loom::Accept<loom::Activated, th::ThemesPlaced, th::ThemesWorn, gfx::GraphicsReady,
                       gfx::OwnValuesDeclared, th::StacksRequested, th::ThemesRequested,
                       th::ThemeResolveRequested, th::ThemeSaveRequested>,
          loom::Emit<gfx::PaletteOffered, th::StackSaid, th::StacksShown, th::ThemesShown,
                     th::ThemeResolved, th::ThemeSaved, th::ThemesPlacedRequested,
                     gfx::OwnValuesAsked>> {
    using Base = zengine::With<Manual>::Weave<
        ThemeWeave, ThemeState,
        loom::Accept<loom::Activated, th::ThemesPlaced, th::ThemesWorn, gfx::GraphicsReady,
                     gfx::OwnValuesDeclared, th::StacksRequested, th::ThemesRequested,
                     th::ThemeResolveRequested, th::ThemeSaveRequested>,
        loom::Emit<gfx::PaletteOffered, th::StackSaid, th::StacksShown, th::ThemesShown,
                   th::ThemeResolved, th::ThemeSaved, th::ThemesPlacedRequested,
                   gfx::OwnValuesAsked>>;

public:
    using Base::on;

    /// On every activation, what a reload kept is offered again, and whoever hosts the role and
    /// every pane with own values is asked to say again what they said before it was held.
    void on(const loom::Activated& activated, loom::Mail& mail) {
        if (!activation_.accept(mail, activated)) {
            return;
        }
        offer_held(mail);
        (void)mail.as_role(th::kThemeRole).publish(th::ThemesPlacedRequested{});
        (void)mail.as_role(th::kThemeRole).publish(gfx::OwnValuesAsked{});
    }

    /// Where the themes are: every stack worn is laid again from the files there.
    void on(const th::ThemesPlaced& placed, loom::Mail& mail) {
        state_.placed = placed;
        state_.placed_heard = true;
        lay_all(mail);
    }

    /// What the host's areas wear, whole: each stack is laid from the files as they stand, and
    /// offered where its palette changed. Past the bounds, the rest is not laid.
    void on(const th::ThemesWorn& worn, loom::Mail& mail) {
        state_.worn.clear();
        std::set<std::string> seen;
        for (const th::ThemeStack& s : worn.stacks) {
            if (state_.worn.size() >= th::kMaxStacksWorn) {
                break;
            }
            th::ThemeStack bounded = s;
            if (bounded.themes.size() > th::kMaxStackThemes) {
                bounded.themes.resize(th::kMaxStackThemes);
            }
            if (bounded.themes.empty() || !seen.insert(th::stack_palette(bounded.themes)).second) {
                continue;
            }
            state_.worn.push_back(std::move(bounded));
        }
        lay_all(mail);
    }

    /// A directory started afresh is offered every palette again.
    void on(const gfx::GraphicsReady&, loom::Mail& mail) {
        if (!mail.authored_from_role(gfx::kGraphicsRole)) {
            return;
        }
        offer_held(mail);
    }

    /// A role's own values, said as that role, are kept, and every stack is laid again with its
    /// entries for that role judged against them.
    void on(const gfx::OwnValuesDeclared& declared, loom::Mail& mail) {
        const std::string role(mail.authored_role());
        if (role.empty() || declared.values.size() > gfx::kMaxOwnValues) {
            return;
        }
        auto at = std::find_if(state_.own.begin(), state_.own.end(),
                               [&](const HeldOwn& h) { return h.role == role; });
        if (at == state_.own.end()) {
            if (state_.own.size() >= kMaxOwnRoles) {
                return;
            }
            state_.own.push_back(HeldOwn{role, declared});
        } else if (at->values == declared) {
            return;
        } else {
            at->values = declared;
        }
        lay_all(mail);
    }

    /// Answers every stack as it was last laid, to the asker.
    void on(const th::StacksRequested&, loom::Mail& mail) {
        th::StacksShown shown;
        for (const HeldStack& h : state_.stacks) {
            shown.stacks.push_back(h.laid);
        }
        (void)mail.answer(shown);
    }

    /// Answers every theme in the folders, a weaver's own over a shipped one of its name, to the
    /// asker.
    void on(const th::ThemesRequested&, loom::Mail& mail) {
        std::map<std::string, th::ThemeSummary> found;
        for (const std::string& dir : {state_.placed.shipped, state_.placed.themes}) {
            if (dir.empty()) {
                continue;
            }
            for (th::ThemeSummary& s : th::list_themes(dir)) {
                found[s.name] = std::move(s);
            }
        }
        th::ThemesShown shown;
        for (auto& [name, s] : found) {
            shown.themes.push_back(std::move(s));
        }
        (void)mail.answer(shown);
    }

    /// Answers one theme resolved with its includes, and lays nothing.
    void on(const th::ThemeResolveRequested& asked, loom::Mail& mail) {
        (void)mail.answer(resolve(asked.theme));
    }

    /// Saves a theme among the weaver's own, by the safe write, and refuses where the host names no
    /// folder of the weaver's. Every stack wearing it is laid again.
    void on(const th::ThemeSaveRequested& asked, loom::Mail& mail) {
        th::ThemeSaved saved;
        saved.name = asked.window ? asked.theme.name : asked.terminal_file.name;
        if (state_.placed.themes.empty()) {
            saved.refusal = "no folder of the weaver's own themes is named here (an isolated launch "
                            "keeps none) -- nothing was written";
            (void)mail.answer(saved);
            return;
        }
        const th::Written w = th::save_theme(state_.placed.themes,
                                             asked.window ? &asked.theme : nullptr,
                                             asked.terminal ? &asked.terminal_file : nullptr,
                                             asked.replace, &saved.paths);
        saved.accepted = w.accepted;
        saved.refusal = w.refusal;
        (void)mail.answer(saved);
        if (w.accepted) {
            lay_all(mail);
        }
    }

private:
    /// Theme `name` resolved from the folders, the weaver's own first.
    th::ThemeResolved resolve(const std::string& name) const {
        const std::string user = state_.placed.themes;
        const std::string shipped = state_.placed.shipped;
        return th::resolve_theme_with(name, [&](std::string_view n) {
            if (!user.empty()) {
                th::ThemeFiles mine = th::load_theme(user, n);
                if (mine.any()) {
                    return mine;
                }
            }
            return shipped.empty() ? th::ThemeFiles{} : th::load_theme(shipped, n);
        });
    }

    th::Declared declared() const {
        th::Declared out;
        for (const HeldOwn& h : state_.own) {
            out[h.role] = h.values;
        }
        return out;
    }

    /// Lays every stack worn again: a palette that changed is offered at the next revision, and a
    /// laying with something to say is said once; a stack no longer worn is let go.
    void lay_all(loom::Mail& mail) {
        if (!state_.placed_heard) {
            return;
        }
        const th::Declared own = declared();
        std::vector<HeldStack> next;
        for (const th::ThemeStack& s : state_.worn) {
            th::Laid laid = th::lay_stack(s.themes, [&](const std::string& n) { return resolve(n); }, own);
            const auto was = std::find_if(state_.stacks.begin(), state_.stacks.end(), [&](const HeldStack& h) {
                return h.palette.palette == laid.palette.palette;
            });
            HeldStack held{std::move(laid.laid), std::move(laid.palette)};
            bool offer = true;
            bool say = th::worth_saying(held.laid);
            if (was != state_.stacks.end()) {
                gfx::PaletteOffered before = was->palette;
                before.revision = 0;
                offer = !(before == held.palette);
                th::StackLaid said_before = was->laid;
                said_before.revision = 0;
                say = say && !(said_before == held.laid);
                if (!offer) {
                    held.palette.revision = was->palette.revision;
                }
            }
            if (offer) {
                held.palette.revision = ++state_.revision;
            }
            held.laid.revision = held.palette.revision;
            if (offer) {
                (void)mail.as_role(th::kThemeRole).send_to_role(gfx::kGraphicsRole, held.palette);
            }
            if (say) {
                (void)mail.as_role(th::kThemeRole).publish(th::StackSaid{held.laid});
            }
            next.push_back(std::move(held));
        }
        state_.stacks = std::move(next);
    }

    void offer_held(loom::Mail& mail) {
        for (const HeldStack& h : state_.stacks) {
            (void)mail.as_role(th::kThemeRole).send_to_role(gfx::kGraphicsRole, h.palette);
        }
    }

    zengine::ActivationCursor activation_;
};

} // namespace

ZEN_EXPORT_WEAVE(ThemeWeave)
