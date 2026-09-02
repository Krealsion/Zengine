// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PERSIST_HPP
#define ZENGINE_WORKSHOP_PERSIST_HPP

// What a maker keeps when they close Workshop, and what they get back.
// Workshop law: agents/workshop/document.md (+2 registers; agents/workshop.md routes)

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
// WL-DOC-13 -- agents/workshop/document.md
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

/// One authored object AS WRITTEN.
// WL-DOC-01, WL-DOC-13 -- agents/workshop/document.md
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
// WL-DOC-01, WL-DOC-13 -- agents/workshop/document.md
struct WorkshopDocument {
    std::string format;
    std::int64_t format_version = 0;
    std::int64_t next_id = 0;
    std::vector<WorkshopObject> objects;

    /// Version 2, because the objects it holds grew a field and a published
    /// shape is immutable. `format_version` below is a DIFFERENT
    /// claim and did NOT move.
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
// WL-DOC-12, WL-DOC-13 -- agents/workshop/document.md
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

/// A document as text.
// WL-DOC-13 -- agents/workshop/document.md
inline std::string to_text(const WorkshopDoc& d) {
    return loom::compat::serialize(loom::to_value(to_document(d)));
}

// ---- Reading ---------------------------------------------------------------

/// The authored extent a written one means. False for a mode this format does
/// not have a word for.
// WL-DOC-13 -- agents/workshop/document.md
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
// WL-DOC-14 -- agents/workshop/document.md
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
// WL-DOC-14 -- agents/workshop/document.md
inline Written load_into(WorkshopDoc& live, std::string_view bytes) {
    Loaded loaded = from_text(bytes);
    if (!loaded.outcome.accepted) {
        return loaded.outcome;
    }
    return doc::restore(live, std::move(loaded.document));
}

// ---- The file itself -------------------------------------------------------

/// ONE SPELLING OF A PATH, RESOLVED AGAINST THE PLACE IT WAS MEANT RELATIVE TO.
// WL-EDIT-06 -- agents/workshop/editor.md; WL-PROJ-02 -- agents/workshop/project.md
inline std::string resolved_against(const std::string& base, const std::string& spelling) {
    if (spelling.empty()) {
        return spelling;
    }
    std::filesystem::path p(spelling);
    if (p.is_absolute()) {
        return p.lexically_normal().generic_string();
    }
    if (base.empty()) {
        return spelling;
    }
    return (std::filesystem::path(base) / p).lexically_normal().generic_string();
}

/// What reading a file produced.
struct FileText {
    Written outcome;
    std::string text;
};

/// The whole file, or why not. Refusals are the ordinary ones a maker meets:
/// the file is not there, it cannot be opened, it is too big to be what it
/// claims to be.
// WL-DOC-15 -- agents/workshop/document.md
inline FileText read_file(const std::string& path,
                          std::uintmax_t most = kMaxDocumentBytes,
                          const char* what = "a Workshop document") {
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec) {
        return FileText{Written::no("cannot read " + path + ": " + ec.message()), {}};
    }
    if (size > most) {
        return FileText{Written::no("cannot read " + path + ": " + std::to_string(size) +
                                    " bytes is larger than " + what + " can be"),
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
// WL-DOC-15 -- agents/workshop/document.md; WL-SESSION-13 -- agents/workshop/session.md
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

/// The same safe write, into a directory that may not exist yet.
///
/// The per-user roots are created ON FIRST WRITE -- a read never creates a directory, and
/// a run that persists nothing leaves no trace -- so the writes that land under them (the
/// session on an orderly close, the prefs on a toggle, the one-time legacy import) go
/// through this door. Project files deliberately do not: a `--document` path into a
/// directory that is not there is a maker's typo, and inventing the directory would turn
/// a loud refusal into a file somewhere nobody meant.
inline Written write_file_making_room(const std::string& path, const std::string& text) {
    const std::filesystem::path parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            return Written::no("cannot create " + parent.string() + ": " + ec.message());
        }
    }
    return write_file(path, text);
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
