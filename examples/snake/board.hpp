// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SNAKE_BOARD_HPP
#define ZENGINE_SNAKE_BOARD_HPP

// The snake's board as the game's own element: the world says it once, and then each step's
// facts, and a window presents it from the code in `snake-board` (board.cpp). The board draws what
// it is told; the rules stay the world's. Cells are numbered row by row: y * cols + x.
// Reference: examples/snake/README.md.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <tuple>
#include <vector>

namespace zengine::snake {

/// The board, said whole: its size, the pixels of one cell, the snake head first, and the food's
/// cell (-1 for none).
struct SnakeBoardData {
    std::int64_t cols = 0, rows = 0;
    std::int64_t cell = 12;
    std::vector<std::int64_t> body;
    std::int64_t food = -1;
    ZEN_SHAPE(SnakeBoardData, 1, ZEN_FIELD(cols), ZEN_FIELD(rows), ZEN_FIELD(cell),
              ZEN_FIELD(body), ZEN_FIELD(food));
};

/// One step: the head's new cell (-1 when it did not move); the cell the tail left (-1 when the
/// snake grew or did not move); the food's new cell (-1 when it stayed); whether the snake died.
struct SnakeStep {
    std::int64_t head = -1;
    std::int64_t freed = -1;
    std::int64_t food = -1;
    bool dead = false;
    ZEN_SHAPE(SnakeStep, 1, ZEN_FIELD(head), ZEN_FIELD(freed), ZEN_FIELD(food), ZEN_FIELD(dead));
};

/// A new game on the same board: the snake and the food laid afresh, alive.
struct SnakeNewGame {
    std::vector<std::int64_t> body;
    std::int64_t food = -1;
    ZEN_SHAPE(SnakeNewGame, 1, ZEN_FIELD(body), ZEN_FIELD(food));
};

/// The changes the board's code takes. A change travels as its index here, so the order is a wire
/// contract: the list only grows at its end.
using SnakeBoardChanges = std::tuple<SnakeStep, SnakeNewGame>;

/// What a window keeps of the board for its code.
struct SnakeBoardState {
    SnakeBoardData board;
    bool dead = false;
    ZEN_SHAPE(SnakeBoardState, 1, ZEN_FIELD(board), ZEN_FIELD(dead));
};

} // namespace zengine::snake

#endif // ZENGINE_SNAKE_BOARD_HPP
