// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_THEME_STACK_NAME_HPP
#define ZENGINE_THEME_STACK_NAME_HPP

// A stack's name and bounds: the name of the palette a stack of themes is laid into, and a pane's
// theme laid over its layout's stack. A host names the palettes its areas wear by these; nothing
// here reads a file. Reference: theme/README.md, "A stack".

#include <cstddef>
#include <string>
#include <vector>

namespace zengine::theme {

/// What joins a stack's themes in the name of the palette it is laid into.
inline constexpr char kStackJoin = '+';

/// The most themes one stack lays, and the most stacks one host's areas wear.
inline constexpr std::size_t kMaxStackThemes = 8;
inline constexpr std::size_t kMaxStacksWorn = 48;

/// The name of the palette a stack is laid into: its themes, bottom first, joined by `+`
/// (`night+block`). A stack of none is named "", and wears the window's own palette.
inline std::string stack_palette(const std::vector<std::string>& themes) {
    std::string out;
    for (const std::string& t : themes) {
        if (!out.empty()) {
            out += kStackJoin;
        }
        out += t;
    }
    return out;
}

/// The stack an area wears when its layout wears `stack` and `over` is laid on top for that area
/// alone ("" for none): `over` is added, unless the stack already ends with it.
inline std::vector<std::string> stack_over(std::vector<std::string> stack,
                                           const std::string& over) {
    if (!over.empty() && (stack.empty() || stack.back() != over)) {
        stack.push_back(over);
    }
    return stack;
}

} // namespace zengine::theme

#endif // ZENGINE_THEME_STACK_NAME_HPP
