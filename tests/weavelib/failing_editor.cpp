// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// A loaded stand-in for the Editor that cannot apply a published claim, labeled test
// instrumentation: it holds `zengine.editor`, offers the pane, claims a document identity and
// prepares and offers like the real image, then, armed through its exposed state, fails the
// showing once, throwing before it applies anything. The arming clears before the throw, so a
// reload's bytes revive a successor that applies: the repair a real reload witnesses. A fixture.

#include "workshop/open_seam_vocabulary.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "activation/activation.hpp"
#include "surface/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

namespace surface = zengine::surface;
using zengine::workshop::EditorDocument;
using zengine::workshop::ManagedOpenProgress;
using zengine::workshop::ManagedOpenSettled;
using zengine::workshop::PaneCatalogRequested;
using zengine::workshop::PaneOffered;
using zengine::workshop::PaneRoom;
using zengine::workshop::PrepareSourceRequested;
using zengine::workshop::SourcePrepared;

/// The office and pane the real Editor holds, spelled as strings: a stand-in is a stranger
/// to the Editor's own headers, exactly as any other image would be.
constexpr const char* kOffice = "zengine.editor";
constexpr const char* kPane = "editor";
constexpr const char* kWorkshopRole = "zengine.workshop";
constexpr const char* kOpeningRole = "zengine.opening";

/// EXPOSED WHOLE, so a case arms the failure through the substrate's own write door
/// (`zen.PokeWrite fail_next 1`) and reads every counter back through its read door.
struct FailingEditorState {
    std::int64_t fail_next = 0;      ///< nonzero: the next showing counts its attempt and fails
    std::int64_t published_seen = 0; ///< showings attempted (successes and the failure)
    std::int64_t applied = 0;        ///< showings completed
    std::int64_t prepared = 0;       ///< preparations answered
    std::string path;                ///< the document this stand-in stands behind
    std::int64_t doc_epoch = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(FailingEditorState, 1, ZEN_FIELD(fail_next), ZEN_FIELD(published_seen),
              ZEN_FIELD(applied), ZEN_FIELD(prepared), ZEN_FIELD(path), ZEN_FIELD(doc_epoch));
};

class FailingEditor
    : public loom::WeaveBase<FailingEditor, FailingEditorState,
                             loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom,
                                          PrepareSourceRequested, ManagedOpenProgress,
                                          ManagedOpenSettled>,
                             loom::Emit<PaneOffered, SourcePrepared>,
                             loom::Claims<EditorDocument>> {
public:
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return;
        }
        offer(mail);
        (void)mail.claim(EditorDocument{}); // an identity to bind: no document yet
    }

    void on(const PaneCatalogRequested&, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        offer(mail);
    }

    void on(const PaneRoom&, loom::Mail&) {}

    /// PREPARE AND OFFER, as the real Editor does: an identity for the operation, one row
    /// for the trial room, and the answer. No file is read; a stand-in has no document.
    void on(const PrepareSourceRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kOpeningRole)) {
            return;
        }
        ++state_.prepared;
        EditorDocument offered;
        offered.path = asked.path;
        offered.doc_epoch = state_.doc_epoch + 1;
        offered.opened_by = asked.op;
        const loom::JointResult r = mail.offer(static_cast<std::uint64_t>(asked.op), offered);
        SourcePrepared said;
        said.op = asked.op;
        said.ok = r.ok;
        if (!r.ok) {
            said.refusal = "the stand-in's offer was refused (" +
                           std::string(loom::name_of(r.why)) + ")";
        }
        said.generation = offered.doc_epoch;
        surface::SurfaceTextRow row;
        row.text = "stand-in: " + asked.path;
        if (static_cast<std::int64_t>(row.text.size()) > asked.columns && asked.columns > 0) {
            row.text.resize(static_cast<std::size_t>(asked.columns));
        }
        said.rows.push_back(row);
        said.caret_row = 0;
        said.caret_col = 0;
        (void)mail.answer(said);
    }

    void on(const ManagedOpenProgress&, loom::Mail&) {}
    void on(const ManagedOpenSettled&, loom::Mail&) {}

    /// THE SHOWING: counted, then applied -- or, once, failed before applying anything.
    void on_claim_published(const EditorDocument& published) {
        ++state_.published_seen;
        if (state_.fail_next != 0) {
            state_.fail_next = 0; // cleared first: the successor of a reload is not armed
            throw std::runtime_error("the stand-in Editor could not apply the published document");
        }
        state_.path = published.path;
        state_.doc_epoch = published.doc_epoch;
        ++state_.applied;
    }

private:
    void offer(loom::Mail& mail) {
        (void)mail.as_role(kOffice).send_to_role(
            kWorkshopRole, PaneOffered{kPane, "Editor", "a stand-in that cannot apply"});
    }

    zengine::ActivationCursor activation_;
};

} // namespace

ZEN_EXPORT_WEAVE(FailingEditor)
