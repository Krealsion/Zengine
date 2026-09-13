// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_DOCUMENT_SEAM_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_DOCUMENT_SEAM_VOCABULARY_HPP

// THE OBJECT DOCUMENT, ACROSS THE PANE SEAM -- what the Info pane is shown, and the four
// things it may ask for.
//
// ---- WHO OWNS THE DOCUMENT, AND WHY IT IS THIS HOST ---------------------------
//
// ⭐ THE HOST KEEPS `WorkshopDoc`, and the code decides it rather than a preference. The
// document is `WorkshopWeave`'s own `ZEN_SHAPE`, and it is read by nearly everything this
// host is: `workspace_scene` paints its objects on the workspace plane; `object_at`,
// `begin_drag` and `size_handle` are the pointer's answers about it; `doc::move` and
// `doc::resize` are what a drag writes; `check_document`, `save_file` and `restore` are its
// durable file's; the status slot counts its objects and compares it with its saved copy;
// `^s` writes it; the contextual surface acts on it; `[` and `]` refit it. Info is ONE
// READER of it, and the one that shows it as a list.
//
// So moving the document into the pane would have moved the canvas, the pointer, the file,
// the status line and the session with it -- which is not a migration, it is a different
// application. The prompt's other branch ("the document moves with the pane and its durable
// file's ownership changes by one explicit conversion") is measured and refused here, in
// writing, because a decision that is obvious after the fact is the one worth recording.
//
// ---- SO WHAT CROSSES IS A PICTURE, NOT THE DOCUMENT ---------------------------
//
// `DocumentShown` is the host's DERIVED reading -- the object rows, which one is selected,
// and the inspector rows of that one -- in exactly the form the built-in's painter already
// had them (`screen_info.cpp`, `Session::rows`). No `WorkshopDoc`, no `ui::Element`, no
// `Property<T>`, no `Row`: those hold `std::function`s bound to the live document and could
// not cross if anybody wanted them to. What a reader gets is a list of strings and a bool
// per row, and can do nothing to the document but ask.
//
// ---- SAID WHEN IT CHANGES, WHICH IS THE MIGRATION'S ONE NEW SENTENCE ----------
//
// ⚠ AND IT IS A PUBLICATION RATHER THAN AN ANSWER, WHICH THE PROMPT DID NOT PLAN FOR. Every
// other door in this seam ANSWERS: a pane asks where the project began, or what the tool
// built, and hears back. That is enough when the answer only changes because the pane asked.
// It is not enough here. WL-DOC-14 is a law -- "canvas, object list and inspector agree
// after every gesture" -- and the document changes under Info constantly with no gesture
// into Info at all: a drag on the workspace moves an object, `h`/`j`/`k`/`l` nudge it, `n`
// and `d` create and delete, `Tab` moves the selection, a restore replaces the whole thing,
// `[` and `]` refit every share. A pane that could only ask would be a list that is wrong
// most of the time, and a maker cannot tell a stale list from a broken one.
//
// So the host says it, on the same discipline `StandingConditions` is under one register
// over: derived per repaint, compared against the last utterance, published only when it
// changed -- because a pane answers a publication by publishing its rows, and a repaint
// follows that, so an unconditional publication would not terminate.
//
// ---- ...AND THE FOUR ACTS ARE ORDINARY ASKS -----------------------------------
//
//     zengine.workshop   ACTS   DocumentActRequested -> DocumentActed
//
// Answered at the office this host already holds, for the reason every act door has: the
// party that owns the document is the party that answers for it, and a second office would
// be a second answer to "who may write this". (The Editor's own act door moved with the
// Editor for the same reason -- `OpenSourceRequested` is answered at `zengine.editor`.) The four acts are the four the built-in
// offered -- select, create, delete, and commit one property -- and each is the SAME call
// the host's own key is bound to, so both gestures converge on one write, one selection rule
// and one sentence (WL-CTRL-05, kept).

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// ONE OBJECT, AS THE LIST SHOWS IT.
///
/// `identity` is the document's own id and is what an act names -- never an index, because
/// an index is a fact about a list the pane was shown and the document may have moved on.
struct ShownObject {
    std::int64_t identity = 0;
    std::string name;
    ZEN_SHAPE(ShownObject, 1, ZEN_FIELD(identity), ZEN_FIELD(name));
};

/// ONE INSPECTOR ROW, AS THE PANEL SHOWED IT.
///
/// `value` is a FRESH READ through the property at the moment the host derived this picture
/// -- there is no cached copy on either side to go stale, which is the same property
/// `Row::value()` has always had, said one seam out.
struct ShownProperty {
    std::string label;
    std::string value;
    bool editable = false; ///< false for a derived row (a resolved size, a count)
    bool section = false;  ///< a heading inside the list: a label, no value, never editable
    ZEN_SHAPE(ShownProperty, 1, ZEN_FIELD(label), ZEN_FIELD(value), ZEN_FIELD(editable),
              ZEN_FIELD(section));
};

/// WHAT THE OBJECT DOCUMENT CURRENTLY LOOKS LIKE, in the host's own order.
///
/// PUBLISHED `to_any` AND NOT ADDRESSED, for `StandingConditions`' reason: which weave
/// presents this is the load plan's business, and a host that addressed one would be a host
/// with a pane compiled into it again.
///
/// ⚠ `properties` IS THE SELECTED OBJECT'S AND IS EMPTY WHEN NOTHING IS SELECTED, which is
/// the state the built-in wrote `(nothing selected)` for. An empty `objects` with a spent
/// selection is the empty document, which says `(none) -- n makes one`. Both sentences are
/// the pane's now and both are still said.
struct DocumentShown {
    std::vector<ShownObject> objects;
    std::int64_t selected = 0; ///< the selected identity; 0 is "none", as `ui::kNoIdentity` is
    std::vector<ShownProperty> properties;
    ZEN_SHAPE(DocumentShown, 1, ZEN_FIELD(objects), ZEN_FIELD(selected),
              ZEN_FIELD(properties));
};

// ---- The four acts ------------------------------------------------------------

/// WHICH ACT. Spelled as strings on the wire rather than as an enumerator's number, for the
/// pane-action ids' reason: a number is a fact about one build's ordering, and a name is what
/// survives a shape being read by an image compiled at a different time.
inline constexpr const char* kDocumentSelect = "select";  ///< make `identity` the selection
inline constexpr const char* kDocumentCreate = "create";  ///< mint a new object
inline constexpr const char* kDocumentDelete = "delete";  ///< remove the selected object
inline constexpr const char* kDocumentCommit = "commit";  ///< write `text` into row `row`

/// ASK THE DOCUMENT'S OWNER TO DO ONE THING.
///
/// ⚠ `row` IS AN INDEX INTO THE PICTURE THE PANE WAS SHOWN, and it is the one place an index
/// crosses. It is safe for the same reason the built-in's `s.cursor` was: the rows are
/// derived from the selection, and a commit that named a row the current derivation does not
/// have is refused by name rather than applied to a neighbour. `identity` is what SELECT
/// names, because a selection outlives a list.
struct DocumentActRequested {
    std::string act;
    std::int64_t identity = 0; ///< `select` only
    std::int64_t row = 0;      ///< `commit` only: the index in `DocumentShown::properties`
    std::string text;          ///< `commit` only: what the maker typed
    ZEN_SHAPE(DocumentActRequested, 1, ZEN_FIELD(act), ZEN_FIELD(identity), ZEN_FIELD(row),
              ZEN_FIELD(text));
};

/// WHAT THE ACT CAME TO. `accepted` with an empty `refusal`, or the document's own words --
/// a draft that is not a value of this type at all, a value the property refused and why, a
/// delete with nothing selected, a spent mint. The pane says the refusal in its own row and
/// keeps the draft open, which is what the built-in did with the same sentence.
///
/// AND A REFUSED ACT IS STILL FOLLOWED BY THE PICTURE. `DocumentShown` is compared and
/// published on the same repaint, so an accepted act reaches the pane as new rows and a
/// refused one leaves the rows exactly as they were -- with no second sentence saying which,
/// because the refusal already said it.
struct DocumentActed {
    bool accepted = false;
    std::string refusal;
    ZEN_SHAPE(DocumentActed, 1, ZEN_FIELD(accepted), ZEN_FIELD(refusal));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_DOCUMENT_SEAM_VOCABULARY_HPP
