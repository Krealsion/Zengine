// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SNAKE_BOARD_HPP
#define ZENGINE_SNAKE_BOARD_HPP

// The snake's board as the game's own element: the world hands the board's data to the function
// the board's weave provides (`snake-board`, board.cpp, the holder of `kBoardRole`), and draws what
// it answers in its own picture. The board draws what it is told; the rules stay the world's. Cells
// are numbered row by row: y * cols + x.
// Reference: examples/snake/README.md.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <vector>

namespace zengine::snake {

/// The board, said whole: its size, the pixels of one cell, the snake head first, the food's cell
/// (-1 for none), and whether the snake died.
struct SnakeBoardData {
    std::int64_t cols = 0, rows = 0;
    std::int64_t cell = 12;
    std::vector<std::int64_t> body;
    std::int64_t food = -1;
    bool dead = false;
    /// Version 2: the whole board in one value, its death included, since it is drawn whole each
    /// time it is asked.
    ZEN_SHAPE(SnakeBoardData, 2, ZEN_FIELD(cols), ZEN_FIELD(rows), ZEN_FIELD(cell),
              ZEN_FIELD(body), ZEN_FIELD(food), ZEN_FIELD(dead));
    friend bool operator==(const SnakeBoardData&, const SnakeBoardData&) = default;
};

/// What the board's weave keeps: nothing. Its drawing is a function of the data it is handed.
struct SnakeBoardState {
    /// Version 2: the board's weave keeps nothing; no window keeps a board for it.
    ZEN_SHAPE(SnakeBoardState, 2);
};

} // namespace zengine::snake

#endif // ZENGINE_SNAKE_BOARD_HPP
