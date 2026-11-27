// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_OWN_SHOWING_HPP
#define ZENGINE_WORKSHOP_OWN_SHOWING_HPP

// The boundary Workshop's own showing runs inside: what the desk does with a presentation it was
// published answers Applied or Declined, and a throw from it is the desk's own failure, said in
// its own words where the process says its own and answered as Loom's Failed. Loom then holds the
// desk and keeps the record; nothing is left for the host's turn to explain.
// Workshop law: agents/workshop/opening.md

#include <zen/weave.hpp>

#include <cstdio>
#include <exception>
#include <string>
#include <utility>

namespace zengine::workshop {

/// A SHOWING THIS DESK COULD NOT COMPLETE, said in its own words on `said`: the desk is held from
/// here, so nothing it could say later would be heard.
inline void say_showing_failed(loom::WeaveId self, const std::string& words, std::FILE* said) {
    std::fprintf(said,
                 "zengine-workshop: weave #%llu could not show the presentation it was published, "
                 "and Loom holds it -- %s\n",
                 static_cast<unsigned long long>(self.value), words.c_str());
    std::fflush(said);
}

/// RUN `show`, the desk's showing of what it was published: true is Applied, false Declined, and
/// a throw is said on `said` and answered Failed.
// WL-OPEN-02 -- agents/workshop/opening.md
template <class Show>
loom::Weave::PublishedClaim show_own(loom::WeaveId self, Show&& show,
                                     std::FILE* said = stderr) {
    try {
        return std::forward<Show>(show)() ? loom::Weave::PublishedClaim::Applied
                                          : loom::Weave::PublishedClaim::Declined;
    } catch (const std::exception& e) {
        say_showing_failed(self, e.what(), said);
    } catch (...) {
        say_showing_failed(self, "(not a std::exception)", said);
    }
    return loom::Weave::PublishedClaim::Failed;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_OWN_SHOWING_HPP
