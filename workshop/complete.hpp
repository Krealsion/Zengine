// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_COMPLETE_HPP
#define ZENGINE_WORKSHOP_COMPLETE_HPP

// WHAT THIS TERMINAL CAN SAY NEXT — the pane's discovery model, and the whole of
// it that is not a picture.
// Workshop law: agents/workshop/terminal.md

#include <zen/terminal/input_lex.hpp>
#include <zen/terminal/session.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::workshop {

// ---- The verbs, written down once ------------------------------------------------------

/// ONE VERB THIS PANE SPEAKS — its spelling, whether it remembers a conversation,
/// and what it does, for a maker to read.
// WL-TERM-04 -- agents/workshop/terminal.md
struct TerminalVerb {
    const char* name;
    bool ask;             ///< does the participant remember this as a conversation?
    const char* meaning;  ///< one line, for the maker
};

/// The whole of what a Workshop Terminal pane says.
// WL-TERM-04 -- agents/workshop/terminal.md
inline constexpr TerminalVerb kTerminalVerbs[] = {
    {"send", false, "author one message; a sender is not told its fate"},
    {"ask", true, "author one message and remember it until Loom's answer arrives"},
};
inline constexpr std::size_t kTerminalVerbCount =
    sizeof(kTerminalVerbs) / sizeof(kTerminalVerbs[0]);

/// The verb with this exact spelling, or nullptr. Exact, never a prefix: an
/// abbreviation that ran a different verb than the maker typed is the one
/// convenience this pane must not have.
inline constexpr const TerminalVerb* terminal_verb(std::string_view name) noexcept {
    for (const TerminalVerb& v : kTerminalVerbs) {
        if (name == v.name) {
            return &v;
        }
    }
    return nullptr;
}

// ---- Where the caret is ----------------------------------------------------------------

/// WHICH PART OF A COMMAND LINE A POSITION IS IN — the same five parts
/// `submit_terminal_line` reads out of the same token positions.
enum class LineSlot : std::uint8_t {
    Verb = 0,      ///< token 0
    Address = 1,   ///< token 1 -- #12, @office, *
    Shape = 2,     ///< token 2
    Version = 3,   ///< token 3
    Arguments = 4, ///< token 4 and everything after it
};

/// A COMMAND LINE, DECOMPOSED — what has been said, and where the maker is.
///
/// `partial` is the token currently being typed and is empty whenever the line
/// ends in a separator, which is the difference between "I am part-way through
/// the shape" and "I have finished the shape and am about to start the version".
/// The tokens themselves are `loom::tokenize`'s, unmodified.
struct CommandLine {
    std::vector<loom::Token> tokens;
    bool open = false;   ///< the last token is still being typed
    bool quoted = false; ///< ...and it carries a double quote
    /// How many tokens are FINISHED — `tokens.size()` less the one still being typed.
    // WL-TERM-04 -- agents/workshop/terminal.md
    std::size_t said = 0;
    LineSlot slot = LineSlot::Verb;
    std::string partial;
    /// The slots already SAID, as far as they parse. Each is only meaningful when
    /// the caret is past it, which is what `slot` says.
    std::string verb;
    std::string shape;
    std::uint32_t version = 0;
    bool has_version = false;
};

/// The largest shape version this pane will read out of a line — the same bound
/// `submit_terminal_line` applies, because a version is a `std::uint32_t` on the
/// wire and a wider number is not a version at all.
inline constexpr std::uint64_t kMaxShapeVersion = 0xFFFFFFFFull;

/// DECOMPOSE ONE (POSSIBLY HALF-TYPED) COMMAND LINE.
// WL-TERM-02, WL-TERM-04 -- agents/workshop/terminal.md
inline CommandLine read_command_line(const std::string& line) {
    CommandLine cl;
    cl.tokens = loom::tokenize(line);
    cl.open = !line.empty() && line.back() != ' ' && line.back() != '\t';
    cl.said = cl.tokens.size() - (cl.open && !cl.tokens.empty() ? 1 : 0);
    if (cl.open && !cl.tokens.empty()) {
        cl.partial = cl.tokens.back().text;
        cl.quoted = cl.tokens.back().quoted;
    }
    cl.slot = cl.said >= 4 ? LineSlot::Arguments : static_cast<LineSlot>(cl.said);
    const std::size_t said = cl.said;
    if (said > 0) {
        cl.verb = cl.tokens[0].text;
    }
    if (said > 2) {
        cl.shape = cl.tokens[2].text;
    }
    if (said > 3) {
        std::uint64_t v = 0;
        if (loom::parse_u64(cl.tokens[3].text, v) && v <= kMaxShapeVersion) {
            cl.version = static_cast<std::uint32_t>(v);
            cl.has_version = true;
        }
    }
    return cl;
}

// ---- What may be offered ---------------------------------------------------------------

/// WHAT KIND OF THING A CANDIDATE IS. It is the slot it completes, and it exists
/// so a presentation can tell a shape from a field without parsing the row it is
/// about to draw.
enum class CandidateKind : std::uint8_t {
    Verb,
    AddressForm, ///< `*`, or the SIGIL of a form whose values cannot be listed
    Shape,
    Version,
    Field,
};

/// ONE THING THE MAKER MAY SAY NEXT.
// WL-TERM-04 -- agents/workshop/terminal.md
struct Candidate {
    std::string insert;
    std::string display;
    std::string detail;
    CandidateKind kind = CandidateKind::Verb;
    std::string shape;         ///< Shape/Version/Field: the schema this is about
    std::uint32_t version = 0; ///< Shape/Version: its version
    bool door = false;         ///< Shape: this participant also ACCEPTS it
};

/// EVERYTHING THE PANE KNOWS ABOUT THE LINE BEING TYPED.
// WL-TERM-04 -- agents/workshop/terminal.md
struct Completion {
    bool open = false;             ///< is there anything to show at all?
    LineSlot slot = LineSlot::Verb;
    std::string partial;
    std::string heading;           ///< what this list IS, and what it does not promise
    std::vector<Candidate> candidates;
    std::size_t selected = 0;
};

namespace detail {

/// Case-sensitive prefix.
// WL-TERM-04 -- agents/workshop/terminal.md
inline bool starts_with(std::string_view s, std::string_view prefix) noexcept {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

/// A shape's fields as one short line: `slot:Text text:Text`. The types are
/// `describe()`'s own spellings, so nothing here decides what a kind is called.
inline std::string field_summary(const loom::ShapeDesc& d) {
    if (d.fields.empty()) {
        return "no fields";
    }
    std::string out;
    for (const loom::FieldDesc& f : d.fields) {
        if (!out.empty()) {
            out += ' ';
        }
        out += f.name;
        out += ':';
        out += f.type;
        if (!f.required) {
            out += '?'; // optional, in the one character a narrow row can spare
        }
    }
    return out;
}

/// The still-unfilled fields of a partly-authored command, as
/// `compose()` named them.
inline std::string missing_summary(const std::vector<loom::FieldDesc>& open) {
    std::string out;
    for (const loom::FieldDesc& f : open) {
        if (!f.required) {
            continue; // an optional field is not missing; it is simply not said
        }
        if (!out.empty()) {
            out += ", ";
        }
        out += f.name;
    }
    return out;
}

/// The field names already assigned BY NAME earlier on this line.
// WL-TERM-04 -- agents/workshop/terminal.md
inline bool named_already(const CommandLine& cl, std::string_view field) {
    const std::size_t said = cl.said;
    for (std::size_t i = 4; i < said; ++i) {
        const std::string& t = cl.tokens[i].text;
        const std::size_t eq = t.find('=');
        if (eq != std::string::npos && eq > 0 && std::string_view(t).substr(0, eq) == field) {
            return true;
        }
    }
    return false;
}

} // namespace detail

// ---- The completer ---------------------------------------------------------------------

/// WHAT THIS PARTICIPANT CAN SAY NEXT, given what has been typed so far.
// WL-TERM-04 -- agents/workshop/terminal.md
inline Completion complete_line(const loom::TerminalSession& me, const std::string& line) {
    const CommandLine cl = read_command_line(line);
    Completion out;
    out.slot = cl.slot;
    out.partial = cl.partial;

    // A QUOTED TOKEN IS NOT COMPLETED. `loom::tokenize` drops the quote characters,
    // so the partial this function can see (`Sur`) is not the text on the line
    // (`"Sur`), and replacing one with the other would leave a dangling quote in a
    // line the maker can no longer see the whole of. A quote is how a maker says
    // "this is a literal"; taking them at their word is the only honest reading.
    if (cl.quoted) {
        return out;
    }

    const std::vector<loom::VocabularyEntry> catalog = me.vocabulary().catalog();

    switch (cl.slot) {
    case LineSlot::Verb: {
        for (const TerminalVerb& v : kTerminalVerbs) {
            if (!detail::starts_with(v.name, cl.partial)) {
                continue;
            }
            Candidate c;
            c.kind = CandidateKind::Verb;
            c.insert = std::string(v.name) + " ";
            c.display = v.name;
            c.detail = v.meaning;
            out.candidates.push_back(std::move(c));
        }
        out.heading = out.candidates.empty()
                          ? "no verb begins with '" + cl.partial + "'; this pane speaks two"
                          : "verbs";
        break;
    }
    case LineSlot::Address: {
        // A SIGIL ALREADY CHOSEN IS A QUESTION THIS PARTICIPANT CANNOT ANSWER, and
        // saying nothing is the honest response -- not "no match", which would
        // claim `#1` is wrong when it is a perfectly good address. What the
        // heading offers instead is Loom's OWN grammar answering whether what has
        // been typed is an address yet (`parse_address`), which is a fact rather
        // than a guess.
        if (!cl.partial.empty() && (cl.partial[0] == '#' || cl.partial[0] == '@')) {
            loom::Address parsed;
            out.heading = loom::parse_address(cl.partial, parsed)
                              ? "'" + cl.partial + "' is an address"
                              : "keep typing -- '" + cl.partial + "' is not an address yet";
            break;
        }
        struct Form {
            const char* insert;
            const char* display;
            const char* detail;
        };
        static constexpr Form kForms[] = {
            {"* ", "*", "everyone that accepts the shape"},
            {"#", "#<id>", "one weave, by id -- this terminal cannot list them"},
            {"@", "@<office>", "whoever holds a role -- nor these"},
        };
        for (const Form& f : kForms) {
            if (!detail::starts_with(f.display, cl.partial)) {
                continue;
            }
            Candidate c;
            c.kind = CandidateKind::AddressForm;
            c.insert = f.insert;
            c.display = f.display;
            c.detail = f.detail;
            out.candidates.push_back(std::move(c));
        }
        out.heading = out.candidates.empty()
                          ? "an address is #12, @office or * -- '" + cl.partial + "' is none"
                          : "where it goes";
        break;
    }
    case LineSlot::Shape: {
        std::size_t known = 0;
        for (const loom::VocabularyEntry& e : catalog) {
            ++known;
            if (!detail::starts_with(e.name, cl.partial)) {
                continue;
            }
            Candidate c;
            c.kind = CandidateKind::Shape;
            // NAME AND VERSION TOGETHER, because a shape without a version is
            // never a command this pane can run -- the grammar wants four tokens
            // and the version is the fourth. It is also what keeps a shape known
            // at two versions two ANSWERS: `Ping 1 ` and `Ping 2 ` are different
            // completions, where a bare `Ping ` would have been one row standing
            // for two shapes with nothing to choose between them.
            c.insert = e.name + " " + std::to_string(e.version) + " ";
            c.display = e.name + " v" + std::to_string(e.version);
            c.shape = e.name;
            c.version = e.version;
            c.door = e.accepted;
            const std::optional<loom::ShapeDesc> d = me.describe(e.name, e.version);
            c.detail = d ? detail::field_summary(*d) : std::string("(no description)");
            if (e.accepted) {
                c.detail += "  [door]";
            }
            out.candidates.push_back(std::move(c));
        }
        // THE ONE SENTENCE THIS LIST MUST CARRY. Knowing a shape is type knowledge;
        // being allowed to say one is authority, and they are separate in every
        // direction (vocabulary.hpp). This participant holds no way to ask the
        // second question, so the list says what it is a list OF.
        out.heading = out.candidates.empty()
                          ? "no shape here begins with '" + cl.partial + "' (" +
                                std::to_string(known) + " known)"
                          : "shapes this terminal KNOWS -- knowing one is not authority to "
                            "send it";
        break;
    }
    case LineSlot::Version: {
        for (const loom::VocabularyEntry& e : catalog) {
            if (e.name != cl.shape) {
                continue;
            }
            const std::string spelling = std::to_string(e.version);
            if (!detail::starts_with(spelling, cl.partial)) {
                continue;
            }
            Candidate c;
            c.kind = CandidateKind::Version;
            c.insert = spelling + " ";
            c.display = "v" + spelling;
            c.shape = e.name;
            c.version = e.version;
            c.door = e.accepted;
            const std::optional<loom::ShapeDesc> d = me.describe(e.name, e.version);
            c.detail = d ? detail::field_summary(*d) : std::string("(no description)");
            out.candidates.push_back(std::move(c));
        }
        out.heading = out.candidates.empty()
                          ? "this terminal knows no " + cl.shape + " at a version starting '" +
                                cl.partial + "'"
                          : "versions of " + cl.shape + " this terminal knows";
        break;
    }
    case LineSlot::Arguments: {
        const std::optional<loom::ShapeDesc> d =
            cl.has_version ? me.describe(cl.shape, cl.version) : std::nullopt;
        if (!d) {
            out.heading = cl.has_version
                              ? "this terminal does not know " + cl.shape + " v" +
                                    std::to_string(cl.version)
                              : "a version is the fourth word, and it must be a whole number";
            break;
        }
        // ONCE THERE IS AN `=` THE MAKER IS TYPING A VALUE, and this file has nothing
        // to say about values: a field's value is a runtime datum, and the one thing
        // worse than no suggestion is a suggested one that was invented. So the field
        // list stops at the separator and the heading -- `compose()`'s own verdict --
        // is what stays on screen while the value is typed.
        const bool naming = cl.partial.find('=') == std::string::npos;
        for (const loom::FieldDesc& f : d->fields) {
            const std::string spelling = f.name + "=";
            if (!naming || !detail::starts_with(spelling, cl.partial) ||
                detail::named_already(cl, f.name)) {
                continue;
            }
            Candidate c;
            c.kind = CandidateKind::Field;
            // NO TRAILING SPACE, and this is the one candidate where that matters:
            // `count=` is finished only when a VALUE follows it, and a separator
            // would put the caret in the next argument with an empty field behind
            // it. The grammar's own separator rule, honoured per slot rather than
            // applied to all of them.
            c.insert = spelling;
            c.display = spelling;
            c.detail = f.type + (f.required ? "  required" : "  optional");
            c.shape = d->name;
            c.version = d->version;
            out.candidates.push_back(std::move(c));
        }
        // THE HEADING IS `compose()`'s OWN VERDICT, which is why this slot has one
        // worth reading even when the field list is exhausted. The ladder is run
        // over the arguments already FINISHED -- never the one being typed, which
        // is half a word -- and it authors nothing: `compose` is const, returns a
        // Composition, and stops one step before anything is sent. It is the same
        // ladder submission runs, so "ready" here means the same thing it will
        // mean then.
        std::vector<loom::Arg> args;
        const std::size_t said = cl.said;
        for (std::size_t i = 4; i < said; ++i) {
            args.push_back(loom::lex_arg(cl.tokens[i]));
        }
        const loom::Composition c = me.compose(cl.shape, cl.version, args);
        const std::string title = d->name + " v" + std::to_string(d->version) + " -- ";
        switch (c.status) {
        case loom::Composition::Status::Ready:
            out.heading = title + "ready; Return submits it";
            break;
        case loom::Composition::Status::NeedsInput: {
            const std::string missing = detail::missing_summary(c.open_fields);
            out.heading = missing.empty() ? title + "the ladder needs one of these named"
                                          : title + "missing: " + missing;
            break;
        }
        case loom::Composition::Status::Error:
            out.heading = title + c.error;
            break;
        }
        break;
    }
    }

    // OPEN WHENEVER THERE IS SOMETHING TRUE TO SAY. A heading with no candidates
    // is a real answer -- "nothing here begins with that", "this terminal cannot
    // list weaves" -- and is the answer the maker most needs, because it is the
    // one that says the vocabulary does not hold what they were reaching for.
    out.open = !out.heading.empty() || !out.candidates.empty();
    return out;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_COMPLETE_HPP
