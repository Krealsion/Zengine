// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PERSIST_HPP
#define ZENGINE_WORKSHOP_PERSIST_HPP

// What a maker keeps when they close Workshop, and what they get back.
//
// THE QUESTION THIS FILE ANSWERS is not "how do we write a struct to disk". It
// is: **exactly which facts are the document?** Everything else follows from
// the answer, and the answer is short enough to state here in full:
//
//   DOCUMENT   an identity, a name, the identity this object's values are
//              measured against, an authored place, two authored extents (mode
//              AND amount), the ORDER the objects are in, and the next identity
//              to mint. That is WorkshopDoc; W-5 found it needed no new field
//              and W-6 added exactly one, to the element rather than to the
//              document.
//
//              The relationship is persisted; its RESULT is not. Nowhere in
//              this file is a source's resolved rectangle, a dependent's global
//              position, the cells a share came to, or the order the loader
//              would have to resolve things in. All four are rebuilt, and the
//              proof is the same one W-5 used for a share: save under one
//              context and load under another, and the authored relationship is
//              byte-identical while the resolved geometry is not.
//
//   SESSION    the selection, the workspace extent, an editor draft, a drag in
//              flight, the last notice, the path being saved to. Belongs to the
//              run, not to the work.
//
//   DERIVED    the resolved Scene, every Rect, the resolved size the inspector
//              shows, the size handle, what a click hits, the whole painted
//              canvas. Rebuilt from DOCUMENT + the current SESSION, so writing
//              any of it down would create a second truth that goes stale.
//
// The strongest test of that boundary is the one the file makes possible: save
// under one workspace, load under another. The authored `60%` comes back
// identical and resolves to a different number of cells, because the number of
// cells was never in the file.
//
// ---- The format ----------------------------------------------------------
//
// A Workshop document is one JSON object, written and read through the LOOM's
// OWN compat codec (<zen/serialize.hpp>) -- not a parser written here. That is
// a source-traced choice rather than a convenience:
//
//   - it is already an ordinary dependency. Zengine links loom::core; W-5 adds
//     no third-party library and no new build edge.
//   - it is the SAME GATE every value crossing the bus goes through. A document
//     read from a file and a message read from the wire are refused by one
//     validator, so Workshop cannot come to trust a file more than it trusts a
//     stranger's message.
//   - it already refuses what a persistence format must refuse: a field the
//     door does not declare (UnknownField -- see the policy note below), a
//     value of the wrong kind, an integer outside int64, invalid UTF-8, and a
//     payload that would materialise more than the decoder's budget allows. Not
//     one of those is a check this phase had to write, and every one of them is
//     a check this phase would have got wrong at least once.
//   - its output is DETERMINISTIC: fields go out in declared schema order, so
//     the same document serialises to the same bytes, and save -> load -> save
//     is byte-identical. That makes a document diffable and archivable without
//     a canonicalisation framework.
//
// What it costs, stated because a reader will notice it: an integer is written
// as a QUOTED string (`"x":"3"`). That is the Loom codec's choice, and its
// reason is that JSON numbers cannot carry the whole of int64 losslessly
// through every reader. It is legible and it is honest; it is not pretty.
//
// ---- Why the file has its own shapes -------------------------------------
//
// WorkshopDoc is already a ZEN_SHAPE, so the shortest possible implementation
// would serialise it directly. This file deliberately does not, and the reason
// is the difference between an implementation and a format:
//
//   the weave's state shape  says how Workshop HOLDS a document right now. It
//                            is poke-manipulable, it is what a message carries,
//                            and it is free to change when the program changes.
//   the file's shape         says what a SAVED DOCUMENT IS. It is a promise to
//                            a file a maker owns, and it must not change
//                            because an implementation did.
//
// Serialising the state shape would weld the two together: renaming `label` to
// `name` in the struct would silently change every maker's file. So the file
// has three small shapes of its own, and `to_text`/`from_text` are the one
// translation between them -- explicit, in one place, and readable.
//
// The clearest thing the separation buys is the EXTENT MODE. In memory it is an
// integer (`ui::kExtentCells` is 0, `ui::kExtentPercent` is 1). In the file it
// is the WORD `"cells"` or `"percent"`, so a document says what it means, a
// person reading it needs no header to decode it, and the day someone reorders
// two constants no saved document silently changes size.
//
// ---- Two identities, at two layers, both checked -------------------------
//
// A saved file carries two claims and they are not redundant:
//
//   the Loom envelope   {"zen":1,"schema":"WorkshopDocument","version":1,...}
//                       "these bytes are a value of THIS SHAPE". admit()
//                       enforces it: wrong shape, wrong kinds, unknown field.
//   the document itself {"format":"zengine-workshop","format_version":"1",...}
//                       "this value MEANS a Workshop document of THIS FORMAT
//                       VERSION". Workshop enforces it, below.
//
// They can disagree, which is exactly why both exist: a future format could
// keep the same fields and change what one of them MEANS (say, x/y become a
// share of the workspace). The shape would still admit; the format version is
// what refuses. `format` is checked too, so a value that happens to have this
// structure and is not a Workshop document is named as such rather than loaded.
//
// ---- What version 1 promises, and what it does not -----------------------
//
//   PROMISED   Workshop reads and writes format version 1. A document written
//              by this build is read by this build, identically, and the second
//              save is byte-identical to the first.
//   REFUSED    any other `format_version`, closed, with the number named. There
//              is one version and there is no migration machinery, no version
//              graph, no legacy reader and no upgrade path -- because there is
//              nothing yet to migrate FROM. The trigger for that work is the
//              first real format change that must read a real v1 document.
//   REFUSED    a field the shape does not declare, including a typo and
//              including a field a NEWER Workshop might write. That is a
//              decision, not the parser's default leaking through: silently
//              dropping an unknown field turns "I read your document" into "I
//              read the part of your document I recognised", and a maker cannot
//              see the difference. Fail closed, say which field.
//   NOT SAID   nothing about field ORDER (the reader is name-keyed and accepts
//              any order; the writer emits one canonical order), nothing about
//              whitespace (the reader accepts JSON whitespace; the writer emits
//              none), and nothing at all about compatibility beyond this build.
//              OBJECT order, by contrast, IS meaning -- see to_text.
//
// ---- What this file is not -----------------------------------------------
//
// Not a Zen document format, not a Zengine serialization service, not a
// reflection system, not a schema registry, not a save-any-weave framework, not
// autosave, not undo, not a migration layer and not a file browser. It is one
// application's format for one application's document. If a second Zengine
// application ever needs to persist authored elements, THAT is when a shared
// concept is earned -- a helper that happens to be reusable is not yet a
// package concept.

#include "document.hpp"
#include "vocabulary.hpp"

#include "ui/vocabulary.hpp"

#include <zen/admission.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace zengine::workshop::persist {

/// What a Workshop document says it is. A word a person recognises, not a
/// C++ type name and not a magic number.
inline constexpr const char* kFormat = "zengine-workshop";

/// The only format version this build reads or writes.
///
/// IT IS NOT YET A PUBLIC COMPATIBILITY BOUNDARY, and W-6 is the phase that had
/// to say so out loud. This phase changed the written shape -- an object now
/// carries a `context` -- so a document saved by W-5 no longer admits: the
/// Loom's gate refuses it for a missing member, before this number is ever
/// looked at. That is the honest outcome and it was chosen rather than papered
/// over. There is one implementation, one consumer, no released promise, and no
/// artifact in the world worth a compatibility layer, so:
///
///   NOT DONE   a v1 -> v2 migration, a legacy reader, an upgrade path, a
///              version graph. Every one of them would be machinery built to
///              preserve files that only this developer's own experiments
///              produced.
///   NOT DONE   bumping this number. It states what a document MEANS, and the
///              meaning of every field is unchanged; the SHAPE changing is a
///              different claim, carried by the Loom envelope's own version
///              (WorkshopDocument v2), which is what actually refuses a W-5
///              file. Incrementing it here as well would be discipline theatre:
///              a second refusal for a document the first one already stopped.
///
/// Real file-format compatibility policy is required before an official release,
/// when an external artifact or consumer actually deserves preservation. The
/// standing trigger is unchanged and it is not this: it is the first format
/// change that must READ a real document written by a released build.
inline constexpr std::int64_t kFormatVersion = 1;

/// The two spellings of an authored extent, in the file. Words, not the
/// in-memory integers -- see the header note.
inline constexpr const char* kModeCells = "cells";
inline constexpr const char* kModePercent = "percent";

/// A Workshop document is a small thing. Refusing a large file before reading
/// it into memory is the read side of the same law the Loom's decoder applies
/// to materialisation: a hostile document does not get to choose the cost.
inline constexpr std::uintmax_t kMaxDocumentBytes = 1u << 22; // 4 MiB

/// The file's suggested name. Workshop has no file browser and no project
/// concept; it has one path, given on the command line or defaulted to this.
inline constexpr const char* kDefaultDocumentName = "workshop.json";

// ---- The file's own shapes -------------------------------------------------

/// An authored extent AS WRITTEN: a mode a person can read, and an amount.
struct WorkshopExtent {
    std::string mode;
    std::int64_t amount = 0;

    ZEN_SHAPE(WorkshopExtent, 1, ZEN_FIELD(mode), ZEN_FIELD(amount));
};

/// One authored object AS WRITTEN. `name` is the display label and `id` is the
/// identity -- separate fields here for exactly the reason they are separate in
/// the vocabulary: two objects may be called `panel` and they are still two
/// objects, and a format that keyed on the name would make that unsayable.
///
/// `context` IS THE RELATIONSHIP, and it is written as the identity it is:
/// another object's `id`, or 0 for the root. Three things about that, because it
/// is the one field where the format had a real choice:
///
///   it is an IDENTITY and never a position. A relationship written as "the
///   fourth object" would survive a save and break on the first reorder --
///   which is the same reason `id` is in this file at all, and the reason the
///   loader keeps the file's identities rather than minting fresh ones.
///
///   it is a NUMBER and not a word, unlike the extent mode next to it, and the
///   asymmetry is deliberate rather than an inconsistency. `mode` is written as
///   `"cells"`/`"percent"` because its in-memory 0/1 are ARBITRARY -- renumber
///   the two constants and every saved document silently changes size. Zero
///   cannot be renumbered: it is not an identity because the mint starts at 1,
///   and a document whose objects included a #0 would already be illegal. The
///   static_assert below is that argument, made into a compile error rather than
///   left as a paragraph.
///
///   it is NOT VALIDATED HERE. Whether #4 exists, and whether following it comes
///   back around, is the document's law and is asked once, in
///   `doc::check_document`. This file's job is the same as it is for an extent:
///   say what the shape of the written thing is, and let the one law judge what
///   it means.
struct WorkshopObject {
    std::int64_t id = 0;
    std::string name;
    std::int64_t context = 0;
    std::int64_t x = 0;
    std::int64_t y = 0;
    WorkshopExtent width;
    WorkshopExtent height;

    ZEN_SHAPE(WorkshopObject, 2, ZEN_FIELD(id), ZEN_FIELD(name), ZEN_FIELD(context),
              ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(width), ZEN_FIELD(height));
};

/// The one thing that would make `context: 0` mean something else. If the mint
/// ever handed out 0, every saved document's root-context objects would silently
/// become dependents of an object numbered zero.
static_assert(ui::kRootContext < doc::kFirstIdentity,
              "A saved document spells the root context as the identity 0, which is safe only "
              "while no object can carry it. If the mint's first identity moves, the file "
              "format needs a word for the root instead of a reserved number.");

/// A whole saved document: what it is, which version of that it is, the next
/// identity to mint, and the objects in AUTHORED ORDER.
///
/// `next_id` is here and it is the whole answer to "what does the same object
/// mean across process death". Without it a loader has to guess the mint, and
/// the only available guess -- one past the largest surviving id -- RECYCLES the
/// identity of an object that was deleted before the save. A maker who made #3,
/// deleted it, saved, and came back would find their next object silently
/// wearing a dead object's name. The counter is four bytes and it is the
/// cheapest thing in this file that could not be reconstructed.
struct WorkshopDocument {
    std::string format;
    std::int64_t format_version = 0;
    std::int64_t next_id = 0;
    std::vector<WorkshopObject> objects;

    /// Version 2 since W-6, because the objects it holds grew a field and a
    /// published shape is immutable. `format_version` below is a DIFFERENT
    /// claim and did NOT move -- see the note on it.
    ZEN_SHAPE(WorkshopDocument, 2, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(next_id), ZEN_FIELD(objects));
};

// ---- Writing ---------------------------------------------------------------

/// The word for an authored extent's mode. An unknown mode -- which only a poke
/// can produce, since every setter goes through `doc::check_extent` -- is
/// written as cells, because that is what `ui::resolve_extent` already makes of
/// it, and a file that said something else would disagree with the picture.
inline const char* mode_word(const ui::Extent& e) {
    return e.mode == ui::kExtentPercent ? kModePercent : kModeCells;
}

inline WorkshopExtent extent_out(const ui::Extent& e) {
    return WorkshopExtent{mode_word(e), e.amount};
}

/// The document, as the value that gets written.
///
/// ORDER IS PRESERVED AND THAT IS MEANING, not tidiness. In this application
/// the order of the objects decides four things a maker can see: which
/// rectangle is painted over which, which object a click finds where two
/// overlap (`ui::hit` answers with the LAST one containing the cell), the order
/// of the object list, and where the selection lands after a delete. So the
/// file writes the document's order and never sorts -- sorting by id or by name
/// would make a saved document look tidier and reload with a different object
/// under the maker's pointer.
inline WorkshopDocument to_document(const WorkshopDoc& d) {
    WorkshopDocument out;
    out.format = kFormat;
    out.format_version = kFormatVersion;
    out.next_id = d.next_id;
    out.objects.reserve(d.elements.size());
    for (const ui::Element& e : d.elements) {
        out.objects.push_back(WorkshopObject{e.id, e.label, e.context, e.x, e.y,
                                             extent_out(e.width), extent_out(e.height)});
    }
    return out;
}

/// A document as text. SERIALIZATION IS OBSERVATION: the argument is const and
/// nothing here normalises, rounds, re-orders, renumbers or tidies a value on
/// its way out. What a maker authored is what gets written, including a value
/// some later rule might not accept -- because the alternative is a save that
/// silently edits the work it was asked to preserve.
inline std::string to_text(const WorkshopDoc& d) {
    return loom::compat::serialize(loom::to_value(to_document(d)));
}

// ---- Reading ---------------------------------------------------------------

/// The authored extent a written one means. False for a mode this format does
/// not have a word for.
///
/// It is a CLOSED set, deliberately. The Loom's gate proves the field is text;
/// only Workshop knows which text is a mode, so an unrecognised word is refused
/// here rather than defaulted to cells. Defaulting would silently turn a
/// document Workshop does not understand into one it does -- and a share read as
/// a cell count is exactly the authored/resolved collapse this whole arc exists
/// to prevent.
inline bool mode_in(const WorkshopExtent& w, ui::Extent& out) {
    if (w.mode == kModeCells) {
        out = ui::Extent{ui::kExtentCells, w.amount};
        return true;
    }
    if (w.mode == kModePercent) {
        out = ui::Extent{ui::kExtentPercent, w.amount};
        return true;
    }
    return false;
}

/// What to say about a mode with no word. It names both what was found and what
/// would have worked, because a maker looking at their own file can fix that.
inline std::string unknown_mode(const WorkshopExtent& w) {
    return "`" + w.mode + "` is not an extent mode (" + kModeCells + " or " + kModePercent + ")";
}

/// What reading produced: whether it worked, and the document if it did.
///
/// The document is a CANDIDATE. It has been through the format's own checks and
/// through the Loom's gate; it has NOT been through the document law, because
/// that law belongs to `doc::` and there is one copy of it. `load_into` below is
/// the composition, and it is what every caller should use.
struct Loaded {
    Written outcome;
    WorkshopDoc document;

    static Loaded no(std::string why) { return Loaded{Written::no(std::move(why)), {}}; }
};

/// Text to a candidate document. Total: every input is either a candidate or a
/// refusal with a reason, and nothing here throws.
inline Loaded from_text(std::string_view bytes) {
    const loom::Unverified claim = loom::compat::parse(bytes);
    if (!claim.well_formed()) {
        // The envelope did not even parse. admit() words this precisely (which
        // byte, which member), so ask it rather than inventing a second
        // sentence for the same fact.
        const loom::Admission refused =
            loom::admit(claim, loom::schema_of<WorkshopDocument>(), loom::Report::FirstError);
        return Loaded::no("not a Workshop document: " + refused.first_error().message());
    }
    const loom::Admission admitted =
        loom::admit(claim, loom::schema_of<WorkshopDocument>(), loom::Report::FirstError);
    if (!admitted.ok()) {
        return Loaded::no(admitted.first_error().message());
    }

    const WorkshopDocument file = loom::from_value<WorkshopDocument>(admitted.value());
    if (file.format != kFormat) {
        return Loaded::no("not a Workshop document: it says it is `" + file.format + "`");
    }
    if (file.format_version != kFormatVersion) {
        // Fail closed, and NAME the number. "Unsupported version" leaves a
        // maker guessing whether their file is older or newer than the program.
        return Loaded::no("document version " + std::to_string(file.format_version) +
                          " -- this Workshop reads version " + std::to_string(kFormatVersion));
    }

    Loaded loaded;
    loaded.document.next_id = file.next_id;
    loaded.document.elements.reserve(file.objects.size());
    for (const WorkshopObject& o : file.objects) {
        ui::Extent width;
        ui::Extent height;
        const std::string who = "#" + std::to_string(o.id) + ": ";
        if (!mode_in(o.width, width)) {
            return Loaded::no(who + "width: " + unknown_mode(o.width));
        }
        if (!mode_in(o.height, height)) {
            return Loaded::no(who + "height: " + unknown_mode(o.height));
        }
        ui::Element e;
        e.id = o.id;
        e.label = o.name;
        // Copied, never interpreted. Whether it names an object, and whether
        // following it comes back here, is `doc::check_document`'s to say --
        // asked once, on the whole candidate, by `load_into` below.
        e.context = o.context;
        e.x = o.x;
        e.y = o.y;
        e.width = width;
        e.height = height;
        loaded.document.elements.push_back(std::move(e));
    }
    loaded.outcome = Written::ok();
    return loaded;
}

/// Read a document from text INTO a live one -- the whole transaction, and the
/// only composition any caller needs.
///
/// Two refusals, in two layers, and the live document is untouched by both:
///
///   the FORMAT refuses    the bytes are not a version-1 Workshop document
///                         (from_text, above -- it never sees `live`).
///   the DOCUMENT refuses  they are, and they do not describe a legal document
///                         (doc::restore -- it validates before it assigns).
///
/// So "a malformed file never leaves Workshop half loaded" is structural rather
/// than careful: there is no path here that writes a field into `live` before
/// the whole candidate has been judged.
inline Written load_into(WorkshopDoc& live, std::string_view bytes) {
    Loaded loaded = from_text(bytes);
    if (!loaded.outcome.accepted) {
        return loaded.outcome;
    }
    return doc::restore(live, std::move(loaded.document));
}

// ---- The file itself -------------------------------------------------------

/// What reading a file produced.
struct FileText {
    Written outcome;
    std::string text;
};

/// The whole file, or why not. Refusals are the ordinary ones a maker meets:
/// the file is not there, it cannot be opened, it is too big to be a document.
inline FileText read_file(const std::string& path) {
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec) {
        return FileText{Written::no("cannot read " + path + ": " + ec.message()), {}};
    }
    if (size > kMaxDocumentBytes) {
        return FileText{Written::no("cannot read " + path + ": " + std::to_string(size) +
                                    " bytes is larger than a Workshop document can be"),
                        {}};
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return FileText{Written::no("cannot read " + path), {}};
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (in.bad()) {
        return FileText{Written::no("cannot read " + path + ": the read failed part way"), {}};
    }
    return FileText{Written::ok(), std::move(text)};
}

/// Where the complete candidate is written before it becomes the document.
inline std::string pending_path(const std::string& path) { return path + ".saving"; }

/// Write text to a file WITHOUT putting the last good save at risk.
///
/// The whole of the mechanism, and the whole of the promise:
///
///     serialize the complete candidate   (the caller did; this takes text)
///     write it to a SIBLING file         so the destination is never opened
///     close it and check                 a write that failed is a refusal
///     rename the sibling over the        the destination stops being the old
///       destination                      document only when a complete new one
///                                        is ready to take its place
///
/// WHAT IS PROMISED: an ordinary detected write failure -- no space, no
/// permission, a path that is not writable -- does not destroy the previously
/// valid save, because nothing touches the destination until a complete file
/// exists beside it. Measured on both supported lanes: `std::filesystem::rename`
/// replaces an existing destination on POSIX (rename(2)) and on Windows
/// (libstdc++ uses MoveFileExW with MOVEFILE_REPLACE_EXISTING).
///
/// WHAT IS NOT PROMISED, and is not claimed anywhere else either: durability
/// across a crash or a power cut. Nothing here calls fsync, so a rename may be
/// visible before the bytes it renamed are on the platter. That is a different
/// guarantee, it costs a platform-specific syscall on every save, and no
/// evidence in this project asks for it yet.
inline Written write_file(const std::string& path, const std::string& text) {
    const std::string pending = pending_path(path);
    {
        std::ofstream out(pending, std::ios::binary | std::ios::trunc);
        if (!out) {
            return Written::no("cannot write " + pending);
        }
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        out.close();
        if (!out) {
            std::error_code drop;
            std::filesystem::remove(pending, drop);
            return Written::no("could not finish writing " + pending +
                               " -- " + path + " is unchanged");
        }
    }
    std::error_code ec;
    std::filesystem::rename(pending, path, ec);
    if (ec) {
        std::error_code drop;
        std::filesystem::remove(pending, drop);
        return Written::no("cannot replace " + path + ": " + ec.message() +
                           " -- it is unchanged");
    }
    return Written::ok();
}

/// Save a document to a file. The composition, so no caller has to remember to
/// serialize the whole thing before opening anything.
inline Written save_file(const std::string& path, const WorkshopDoc& d) {
    return write_file(path, to_text(d));
}

/// Load a document from a file into a live one. The composition of every layer:
/// the file, the format, and the document law.
inline Written load_file(const std::string& path, WorkshopDoc& live) {
    const FileText read = read_file(path);
    if (!read.outcome.accepted) {
        return read.outcome;
    }
    return load_into(live, read.text);
}

} // namespace zengine::workshop::persist

#endif // ZENGINE_WORKSHOP_PERSIST_HPP
