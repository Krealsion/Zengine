// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `weave.hpp`'s sections -- save and open -- compiled once into
// `zengine-workshop-logic` and linked by the host and every suite; the declarations, the
// constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/document-file.md (+1 register; agents/workshop.md routes)

#include "weave.hpp"

namespace zengine::workshop {

// ---- Save and open -------------------------------------------------------

// WL-DOC-16 -- agents/workshop/document-file.md
void WorkshopWeave::save_document() {
    if (host_->document_path.empty()) {
        say(kNoDocumentFile, true);
        return;
    }
    const Row* draft = editing_row();
    if (draft != nullptr) {
        say(draft->label() + " is still being edited -- " + hotkey(Act::kDraftCommit) +
                " commits, " + hotkey(Act::kDraftCancel) + " cancels; nothing was saved",
            true);
        return;
    }
    const Written written = persist::save_file(host_->document_path, state_);
    if (!written.accepted) {
        say(written.refusal, true);
        return;
    }
    // What is on disk is now what is in memory. Recorded as a COPY of the
    // document rather than as a flag, so "saved" cannot drift from the truth
    // it describes -- see WorkshopDoc's operator==.
    saved_ = state_;
    say("saved " + host_->document_path, false);
}

// WL-CTX-01 -- agents/workshop/contextual.md; WL-DOC-16 -- agents/workshop/document-file.md
void WorkshopWeave::load_document() {
    if (host_->document_path.empty()) {
        say(kNoDocumentFile, true);
        return;
    }
    const Written read = persist::load_file(host_->document_path, state_);
    if (!read.accepted) {
        // The document, the selection, the drag and any draft are all
        // exactly as they were. A failed load costs a maker nothing but the
        // notice.
        say(read.refusal, true);
        return;
    }
    end_drag(session_);
    open_on_first();
    // A DOCUMENT REPLACEMENT IS THE ONE PATH an old object identity can come to
    // alias a different object -- the file restores the mint -- so a captured
    // contextual subject from the old document is dropped at this door, exactly as
    // the selection is re-established rather than preserved. A room or pane
    // subject names nothing the replacement touched and stands.
    if (session_.context.subject == context_subject::kObject) {
        session_.context = ContextMenu{};
    }
    saved_ = state_;
    say("loaded " + host_->document_path + " -- " + std::to_string(state_.elements.size()) +
            " objects",
        false);
}

void WorkshopWeave::open_on_first() {
    session_.selected = state_.elements.empty() ? 0 : state_.elements.front().id;
    rebuild_rows();
}

} // namespace zengine::workshop
