// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_FILESYSTEM_ROOTS_HPP
#define ZENGINE_WORKSHOP_FILESYSTEM_ROOTS_HPP

// THE ONE PLACE THIS REPOSITORY ASKS AN OPERATING SYSTEM WHICH FILESYSTEM ROOTS IT HAS.
// Workshop law: agents/workshop/files.md

#include <string>
#include <vector>


namespace zengine::workshop {

/// THE FILESYSTEM ROOTS THIS HOST REPORTS, ascending, spelled the way every path in this
/// application is spelled: absolute, with forward separators.
std::vector<std::string> host_filesystem_roots();

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_FILESYSTEM_ROOTS_HPP
