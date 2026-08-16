// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SETUP_PERSIST_HPP
#define ZENGINE_WORKSHOP_SETUP_PERSIST_HPP

// THE SETUP'S OWN FILE -- a second artifact, separate from the document's, and
// separate on purpose.
//
// ---- Why it is not in the document's file ---------------------------------
//
// A document is what a maker MADE; a setup is what they were LOOKING AT while
// they made it. Two facts, two lifetimes, two audiences:
//
//   the same document is worth opening in two different arrangements
//   the same arrangement is worth using over two different documents
//
// A single project/workspace container would make each of those impossible to
// say, and it would make every save of one an edit to the other's bytes. So the
// separation is enforced by the strongest thing available -- they are different
// files, with different format identities, written and read by different
// functions -- and the two commands stay apart: `^s`/`^o` are the DOCUMENT's,
// and they save and load nothing else.
//
// ---- What it shares with the document's file, and what it does not ---------
//
// It rides the same LOOM COMPAT CODEC (<zen/serialize.hpp>) for every reason
// persist.hpp gives for the document: an already-linked dependency, the same
// gate the live bus uses, unknown-field rejection, kind validation, UTF-8
// validation, a materialisation budget, and deterministic output that makes
// save -> load -> save byte-identical with no canonicalisation framework.
//
// It shares the SAFE-WRITE mechanism too (`persist::write_file`), because that
// is one genuinely identical invariant -- write a complete candidate to a
// sibling and rename over the destination, so a detected failure cannot destroy
// the last good file -- and reimplementing it here would be a second copy of a
// promise. It shares `persist::read_file` for the same reason, with its own
// ceiling and its own word for what it is reading.
//
// What it does NOT share is a shape, a version, a format word, a path, a
// command, or a validity law. Nothing in this file can make the document
// refuse, and nothing about the document can make a setup refuse.
//
// ---- What version 1 promises ----------------------------------------------
//
//   PROMISED   Workshop reads and writes setup format version 1, and a second
//              save of a loaded setup is byte-identical to the first.
//   REFUSED    any other `format_version`, with the number named; a `format`
//              that is not this one; a field the shape does not declare; a field
//              of the wrong kind; a reference or a name the setup law refuses; a
//              file larger than a setup can be.
//   ACCEPTED   a well-formed reference this build cannot resolve. That is not an
//              error and must never become one -- it is the case the two-string
//              reference exists for, and the branch is written here while it is
//              still unreachable by any file this build can produce.
//   NOT DONE   migration, a legacy reader, a version graph, an upgrade path.
//              There is one version and nothing in the world to migrate from.
//
// NO INTEGER PANEL KIND APPEARS ANYWHERE IN THE PERSISTED REPRESENTATION, which
// is the invariant a reader should check this file against first: search it for
// `panel::` and find nothing.

#include "persist.hpp"
#include "setup.hpp"

#include <zen/admission.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop::setup_persist {

/// What a Workshop setup file says it is. Its own word, beside and not equal to
/// the document's `zengine-workshop`, so that handing Workshop the wrong one of
/// its own two files is named rather than half-read.
inline constexpr const char* kFormat = "zengine-workshop-setup";

/// The only setup format version this build reads or writes.
inline constexpr std::int64_t kFormatVersion = 1;

/// A setup is a smaller thing than a document, and its ceiling says so. Sixty-four
/// kibibytes is more than an order of magnitude above the largest legal setup --
/// `kMaxSetupPanes` references of `kMaxPaneKeyLen` each, plus a name, is a few
/// kilobytes with the envelope -- and it is the read side of the same law the
/// Loom's decoder applies to materialisation: a hostile file does not get to
/// choose the cost of refusing it.
inline constexpr std::uintmax_t kMaxSetupBytes = 1u << 16;

// ---- The file's own shapes ---------------------------------------------------

/// ONE PANE REFERENCE AS WRITTEN. The same two strings the value carries, and
/// deliberately its own shape rather than the value's: `PaneRef` is how this
/// build HOLDS a reference and is free to change when the program does; this is
/// what a saved setup IS, and it must not change because an implementation did.
/// The same argument persist.hpp makes about `WorkshopObject`.
struct WorkshopSetupPane {
    std::string provider;
    std::string pane;

    ZEN_SHAPE(WorkshopSetupPane, 1, ZEN_FIELD(provider), ZEN_FIELD(pane));
};

/// A WHOLE SAVED SETUP: what it is, which version of that it is, what a maker
/// calls it, and the panes it means to have open IN AUTHORED ORDER.
struct WorkshopSetup {
    std::string format;
    std::int64_t format_version = 0;
    std::string name;
    std::vector<WorkshopSetupPane> panes;

    ZEN_SHAPE(WorkshopSetup, 1, ZEN_FIELD(format), ZEN_FIELD(format_version), ZEN_FIELD(name),
              ZEN_FIELD(panes));
};

// ---- Writing -------------------------------------------------------------------

/// The setup, as the value that gets written.
///
/// NOTHING IS SORTED, NORMALISED, RESOLVED OR DROPPED ON THE WAY OUT. The
/// argument is const and this function reads it once: the order is the setup's,
/// the keys are byte-for-byte what came in, and a reference this build cannot
/// resolve is written exactly as it was read. A save that tidied would be a save
/// that edited the work it was asked to preserve -- and here it would do it to a
/// reference belonging to somebody who is not in the room.
inline WorkshopSetup to_setup(const Setup& s) {
    WorkshopSetup out;
    out.format = kFormat;
    out.format_version = kFormatVersion;
    out.name = s.name;
    out.panes.reserve(s.panes.size());
    for (const PaneRef& p : s.panes) {
        out.panes.push_back(WorkshopSetupPane{p.provider, p.pane});
    }
    return out;
}

inline std::string to_text(const Setup& s) {
    return loom::compat::serialize(loom::to_value(to_setup(s)));
}

// ---- Reading -------------------------------------------------------------------

/// What reading produced: whether it worked, and the setup if it did.
///
/// THE SETUP IS RETURNED RATHER THAN WRITTEN THROUGH A REFERENCE, and that is
/// how "a malformed file never leaves Workshop halfway restored" is structural
/// rather than careful: there is no live value in scope here for a half-built
/// candidate to be written into. The caller reconciles only what it was handed,
/// and it is only handed a setup that passed every layer.
struct LoadedSetup {
    Written outcome;
    Setup setup;

    static LoadedSetup no(std::string why) {
        return LoadedSetup{Written::no(std::move(why)), {}};
    }
};

/// Text to a setup. Total: every input is either a setup or a refusal with a
/// reason, and nothing here throws.
///
/// FOUR LAYERS, IN ORDER, AND THE LAST ONE IS THE SETUP'S OWN LAW: the envelope
/// must parse; it must admit against this shape (which is where an unknown
/// field, a wrong kind, a bad integer or invalid UTF-8 is refused, by the same
/// gate the bus uses); it must say it is this format at this version; and the
/// value it describes must be a legal setup (`check_setup` -- the SAME function
/// the one-line name editor calls, so a typed name and a loaded one cannot come
/// to disagree about what is legal).
inline LoadedSetup from_text(std::string_view bytes) {
    const loom::Unverified claim = loom::compat::parse(bytes);
    if (!claim.well_formed()) {
        const loom::Admission refused =
            loom::admit(claim, loom::schema_of<WorkshopSetup>(), loom::Report::FirstError);
        return LoadedSetup::no("not a Workshop setup: " + refused.first_error().message());
    }
    const loom::Admission admitted =
        loom::admit(claim, loom::schema_of<WorkshopSetup>(), loom::Report::FirstError);
    if (!admitted.ok()) {
        return LoadedSetup::no(admitted.first_error().message());
    }

    const WorkshopSetup file = loom::from_value<WorkshopSetup>(admitted.value());
    if (file.format != kFormat) {
        return LoadedSetup::no("not a Workshop setup: it says it is `" + file.format + "`");
    }
    if (file.format_version != kFormatVersion) {
        return LoadedSetup::no("setup version " + std::to_string(file.format_version) +
                               " -- this Workshop reads version " +
                               std::to_string(kFormatVersion));
    }

    Setup candidate;
    candidate.name = file.name;
    candidate.panes.reserve(file.panes.size());
    for (const WorkshopSetupPane& p : file.panes) {
        // COPIED, NEVER RESOLVED. Whether this build can present the pane is a
        // question for `resolve_pane`, asked later and by somebody who has
        // somewhere to put the answer; a reference that resolves to nothing is
        // still a reference this setup holds.
        candidate.panes.push_back(PaneRef{p.provider, p.pane});
    }
    const Written legal = check_setup(candidate);
    if (!legal.accepted) {
        return LoadedSetup::no(legal.refusal);
    }

    LoadedSetup loaded;
    loaded.outcome = Written::ok();
    loaded.setup = std::move(candidate);
    return loaded;
}

// ---- The file itself -------------------------------------------------------------

/// Save a setup to a file, through the document's own safe write: a complete
/// candidate to a sibling, then a rename over the destination.
///
/// THE PROMISE IS THE ONE `persist::write_file` MAKES and it is not restated
/// here as though it were a second mechanism: an ordinary detected write failure
/// -- no space, no permission, an unwritable path -- does not destroy the
/// previously valid setup file, because nothing touches the destination until a
/// complete file exists beside it. Crash durability is not claimed here either.
inline Written save_file(const std::string& path, const Setup& s) {
    return persist::write_file(path, to_text(s));
}

/// Read a setup from a file. The composition of every layer: the file, the
/// format, and the setup law.
inline LoadedSetup load_file(const std::string& path) {
    const persist::FileText read = persist::read_file(path, kMaxSetupBytes, "a Workshop setup");
    if (!read.outcome.accepted) {
        return LoadedSetup{read.outcome, {}};
    }
    return from_text(read.text);
}

} // namespace zengine::workshop::setup_persist

#endif // ZENGINE_WORKSHOP_SETUP_PERSIST_HPP
