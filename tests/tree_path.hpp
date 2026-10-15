// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_TREE_PATH_HPP
#define ZENGINE_TESTS_TREE_PATH_HPP

// The paths of the tree under test, reached by name: a test target's path definition is a call to
// `tree_path`, answered by the unit tests/CMakeLists.txt generated for that target and that tree
// (`zengine_test_definitions`), so no compile carries a path of this checkout or build tree.

namespace zengine::testing {

/// The path the target was given under `name`; a name it was not given ends the program, saying so.
const char* tree_path(const char* name);

} // namespace zengine::testing

#endif // ZENGINE_TESTS_TREE_PATH_HPP
