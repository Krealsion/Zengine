// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_THEME_VOCABULARY_HPP
#define ZENGINE_THEME_VOCABULARY_HPP

// THE THEME ROLE'S DOORS, `zengine.theme`, held by the `zengine-theme` weave: where the themes are
// and which stacks a host's areas wear, said by the host; the palette each stack is laid into,
// offered to graphics under the stack's name; and the reading doors -- every theme, one resolved,
// each stack as it was laid, each link's medium and warning. The theme files are
// `theme_persist.hpp`; how a stack is laid, `stack.hpp`. Reference: theme/docs/zengine.theme.md.

#include "theme/stack_name.hpp"
#include "theme/theme_persist.hpp"

#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::theme {

/// The role every door here is addressed to.
inline constexpr const char* kThemeRole = "zengine.theme";

// ---- What the host says -------------------------------------------------------------------

/// THE HOST -> the theme role, each time any of it changes and whenever another weave comes to
/// hold the role: the folder of the weaver's own themes ("" for none, as an isolated launch keeps
/// none) and the folder of the themes shipped beside the host. A theme of the weaver's wins over a
/// shipped one of its name.
struct ThemesPlaced {
    std::string themes;
    std::string shipped;

    ZEN_SHAPE(ThemesPlaced, 1, ZEN_FIELD(themes), ZEN_FIELD(shipped));
    friend bool operator==(const ThemesPlaced&, const ThemesPlaced&) = default;
};

/// THE THEME ROLE -> the host, on each of its activations: say where the themes are and what your
/// areas wear, so a role held after the host last said it hears it.
struct ThemesPlacedRequested {
    ZEN_SHAPE(ThemesPlacedRequested, 1);
    friend bool operator==(const ThemesPlacedRequested&, const ThemesPlacedRequested&) = default;
};

/// ONE STACK A HOST'S AREAS WEAR: its themes, bottom first.
struct ThemeStack {
    std::vector<std::string> themes;

    ZEN_SHAPE(ThemeStack, 1, ZEN_FIELD(themes));
    friend bool operator==(const ThemeStack&, const ThemeStack&) = default;
};

/// THE HOST -> the theme role, whole, each time it changes: every stack its areas wear. Each is
/// laid from the files as they stand and offered to graphics under its name (`stack_palette`).
struct ThemesWorn {
    std::vector<ThemeStack> stacks;

    ZEN_SHAPE(ThemesWorn, 1, ZEN_FIELD(stacks));
    friend bool operator==(const ThemesWorn&, const ThemesWorn&) = default;
};

// ---- What the role says -------------------------------------------------------------------

/// ONE LINK OF A STACK AS IT WAS LAID: its theme, which of its files it has (a window file, a
/// terminal file), the warning said when it was put in -- a theme of the other medium alone,
/// converted -- and why it was left out, where it was.
struct ThemeLink {
    std::string theme;
    bool window = false;
    bool terminal = false;
    std::string warning;
    std::string refusal;

    ZEN_SHAPE(ThemeLink, 1, ZEN_FIELD(theme), ZEN_FIELD(window), ZEN_FIELD(terminal),
              ZEN_FIELD(warning), ZEN_FIELD(refusal));
    friend bool operator==(const ThemeLink&, const ThemeLink&) = default;
};

/// One own value a stack's theme set that its role does not declare, left out, and why.
struct OwnSkipped {
    std::string theme;
    std::string role;
    std::string name;
    std::string why;

    ZEN_SHAPE(OwnSkipped, 1, ZEN_FIELD(theme), ZEN_FIELD(role), ZEN_FIELD(name), ZEN_FIELD(why));
    friend bool operator==(const OwnSkipped&, const OwnSkipped&) = default;
};

/// ONE STACK AS IT WAS LAID: the palette it was offered as, at which revision, each link, and the
/// own values left out.
struct StackLaid {
    std::string palette;
    std::int64_t revision = 0;
    std::vector<ThemeLink> links;
    std::vector<OwnSkipped> skipped;

    ZEN_SHAPE(StackLaid, 1, ZEN_FIELD(palette), ZEN_FIELD(revision), ZEN_FIELD(links),
              ZEN_FIELD(skipped));
    friend bool operator==(const StackLaid&, const StackLaid&) = default;
};

/// THE THEME ROLE -> any, published when a stack is laid with something to say: a link of the
/// other medium alone, converted, said once as it is put in; a link left out; an own value left
/// out.
struct StackSaid {
    StackLaid laid;

    ZEN_SHAPE(StackSaid, 1, ZEN_FIELD(laid));
    friend bool operator==(const StackSaid&, const StackSaid&) = default;
};

/// Ask for every stack as it was last laid; answered by a `StacksShown`.
struct StacksRequested {
    ZEN_SHAPE(StacksRequested, 1);
    friend bool operator==(const StacksRequested&, const StacksRequested&) = default;
};

/// The answer to `StacksRequested`: every stack the host's areas wear, as it was laid.
struct StacksShown {
    std::vector<StackLaid> stacks;

    ZEN_SHAPE(StacksShown, 1, ZEN_FIELD(stacks));
    friend bool operator==(const StacksShown&, const StacksShown&) = default;
};

/// Ask for every theme in the folders; answered by a `ThemesShown`.
struct ThemesRequested {
    ZEN_SHAPE(ThemesRequested, 1);
    friend bool operator==(const ThemesRequested&, const ThemesRequested&) = default;
};

/// The answer to `ThemesRequested`: each theme by name, a weaver's own over a shipped one, a
/// broken one with its refusal.
struct ThemesShown {
    std::vector<ThemeSummary> themes;

    ZEN_SHAPE(ThemesShown, 2, ZEN_FIELD(themes));
    friend bool operator==(const ThemesShown&, const ThemesShown&) = default;
};

/// Resolve theme `theme` with its includes, laying nothing; answered by a `ThemeResolved`.
struct ThemeResolveRequested {
    std::string theme;

    ZEN_SHAPE(ThemeResolveRequested, 1, ZEN_FIELD(theme));
    friend bool operator==(const ThemeResolveRequested&, const ThemeResolveRequested&) = default;
};

/// SAVE A THEME among the weaver's own: its window file, its terminal file, or both (`window`,
/// `terminal` say which are given), over a theme of that name only with `replace`.
struct ThemeSaveRequested {
    bool window = false;
    ThemeFile theme;
    bool terminal = false;
    TerminalFile terminal_file;
    bool replace = false;

    ZEN_SHAPE(ThemeSaveRequested, 2, ZEN_FIELD(window), ZEN_FIELD(theme), ZEN_FIELD(terminal),
              ZEN_FIELD(terminal_file), ZEN_FIELD(replace));
    friend bool operator==(const ThemeSaveRequested&, const ThemeSaveRequested&) = default;
};

/// The answer to a save: the theme's name and each file written, or the refusal; nothing is
/// written where either file is refused.
struct ThemeSaved {
    bool accepted = false;
    std::string refusal;
    std::string name;
    std::vector<std::string> paths;

    ZEN_SHAPE(ThemeSaved, 2, ZEN_FIELD(accepted), ZEN_FIELD(refusal), ZEN_FIELD(name),
              ZEN_FIELD(paths));
    friend bool operator==(const ThemeSaved&, const ThemeSaved&) = default;
};

} // namespace zengine::theme

#endif // ZENGINE_THEME_VOCABULARY_HPP
