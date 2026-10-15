// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INPUT_OFFICE_PAGE_HPP
#define ZENGINE_INPUT_OFFICE_PAGE_HPP

// The `zengine.input` office's page, as the two loadable weaves carry it: an `InputWeaveT` built
// with `OfficePage` answers `ShapeDocumentRequested` from the manual compiled into its artifact
// (`MANUAL` in input/CMakeLists.txt). Not installed: a weave built over another reader names its
// own page, or none. Reference: input/docs/zengine.input.md.

#include "manual/vocabulary.hpp"
#include "manuals/zengine.input.hpp" // this office's manual, compiled in from its page

namespace zengine::input {

struct OfficePage {
    using Asked = manual::ShapeDocumentRequested;
    using Shown = manual::ShapeDocumentShown;
    template <class Schemas>
    static Shown shown(const Asked& ask, const Schemas& accepted) {
        return manual::shape_document(ask, accepted, manual::pages::zengine_input::kManual);
    }
};

} // namespace zengine::input

#endif // ZENGINE_INPUT_OFFICE_PAGE_HPP
