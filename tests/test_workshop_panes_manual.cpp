// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite -- a pane's manual across the seam: the document a pane declares beside
// its offer, judged whole under its office's stamp, held while the weave that sent it holds the
// office, and dropped when the pane is offered again. A native seat stands in for a pane, so a case
// can say the exact sentence at the exact moment; the Builder's own page, compiled into its image,
// is proven through that image in the Builder's cases.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

#include <string>
#include <vector>

namespace {

constexpr const char* kGaugeOffice = "test.gauge";
constexpr const char* kGauge = "gauge";

PaneOffered gauge_offer() { return PaneOffered{kGauge, "Gauge", "a pane that carries its manual"}; }

const std::string kPageA(64, 'a');
const std::string kPageB(64, 'b');

/// A small manual: its about, its use, and one command.
PaneDocumentDeclared gauge_document(const std::string& content_id, const std::string& words) {
    PaneDocumentDeclared d;
    d.pane = kGauge;
    d.content_id = content_id;
    d.sections = {PaneDocumentSection{"about", "About", "The gauge " + words + "."},
                  PaneDocumentSection{"use", "Use", "Read it."},
                  PaneDocumentSection{"gauge.reset", "Commands",
                                      "Sets it to zero.\n\n- **Takes:** nothing."}};
    return d;
}

/// A LIVE WORKSHOP AND THE SEAT THAT WILL OFFER THE GAUGE.
struct GaugeRig {
    PaneRig r;
    SettingsSeat* seat = nullptr;

    GaugeRig() {
        r.mount_workshop();
        r.ready();
        r.extent(160, 48);
        seat = r.mount_settings_seat(kGaugeOffice);
    }
    void offer() {
        r.drive(seat, [](SettingsSeat& s, loom::Mail& m) { s.offer(m, gauge_offer()); });
    }
    void declare(const PaneDocumentDeclared& d) {
        r.drive(seat, [d](SettingsSeat& s, loom::Mail& m) { s.declare_document(m, d); });
    }
    const RuntimePane* row() { return r.session().panes.runtime.find(kGaugeOffice, kGauge); }
    const PaneDocumentDeclared* held() { return r.w->counted_document(*row()); }
};

} // namespace

TEST_CASE("a pane's manual is judged whole under the office stamp, refused aloud, and replaced") {
    GaugeRig t;
    t.offer();
    REQUIRE(t.row() != nullptr);
    CHECK(t.held() == nullptr); // offered, and no words yet

    t.declare(gauge_document(kPageA, "reads one"));
    REQUIRE(t.held() != nullptr);
    CHECK(t.held()->content_id == kPageA);
    REQUIRE(find_pane_document_section(*t.held(), "gauge.reset") != nullptr);
    CHECK(t.row()->document_from == t.r.settings_seat_id(t.seat));

    struct Case {
        const char* what;
        PaneDocumentDeclared declared;
        const char* says;
    };
    PaneDocumentDeclared no_id = gauge_document("", "x");
    PaneDocumentDeclared upper = gauge_document(std::string(64, 'A'), "x");
    PaneDocumentDeclared twice = gauge_document(kPageB, "x");
    twice.sections.push_back(PaneDocumentSection{"use", "Use", "again"});
    PaneDocumentDeclared bad_key = gauge_document(kPageB, "x");
    bad_key.sections[2].key = "gauge reset";
    PaneDocumentDeclared long_key = gauge_document(kPageB, "x");
    long_key.sections[2].key = std::string(zengine::manual::kMaxManualKeyBytes + 1, 'k');
    PaneDocumentDeclared long_words = gauge_document(kPageB, "x");
    long_words.sections[2].text = std::string(zengine::manual::kMaxManualSectionBytes + 1, 'w');
    PaneDocumentDeclared long_follow = gauge_document(kPageB, "x");
    long_follow.sections.push_back(PaneDocumentSection{
        "follow", "Follow", std::string(zengine::manual::kMaxManualFollowBytes + 1, 'f')});
    PaneDocumentDeclared control = gauge_document(kPageB, "x");
    control.sections[0].text = "a\tb";
    PaneDocumentDeclared crowded = gauge_document(kPageB, "x");
    for (std::size_t i = 0; i < zengine::manual::kMaxManualSections; ++i) {
        crowded.sections.push_back(PaneDocumentSection{"k" + std::to_string(i), "Commands", "w"});
    }
    const std::vector<Case> cases = {
        {"no content id", no_id, "names its page by the page's SHA-256"},
        {"a content id in capitals", upper, "sixty-four lowercase hex digits"},
        {"a key twice", twice, "section `use` is declared twice"},
        {"a key a weaver cannot spell", bad_key, "section 3's key is not a key"},
        {"a key past its bound", long_key, "section 3's key is not a key"},
        {"a command past its bound", long_words, "section `gauge.reset` holds 2049 bytes, past 2048"},
        {"a follow past its bound", long_follow, "section `follow` holds 1025 bytes, past 1024"},
        {"a control character", control, "section `about` holds a control character"},
        {"more sections than a page holds", crowded, "a document holds at most"},
    };
    for (const Case& c : cases) {
        CAPTURE(c.what);
        t.declare(c.declared);
        // REFUSED ALOUD, NAMING THE PANE, and the document in force stands.
        CHECK(t.r.session().notice_is_bad);
        CHECK(t.r.last_notice().find("Gauge @test.gauge: its document was not taken") !=
              std::string::npos);
        CHECK(t.r.last_notice().find(c.says) != std::string::npos);
        REQUIRE(t.held() != nullptr);
        CHECK(t.held()->content_id == kPageA);
    }

    // A LATER ACCEPTED DECLARATION REPLACES THE WORDS WHOLE.
    t.declare(gauge_document(kPageB, "reads two"));
    REQUIRE(t.held() != nullptr);
    CHECK(t.held()->content_id == kPageB);
    CHECK(find_pane_document_section(*t.held(), "about")->text == "The gauge reads two.");

    // ...A PANE THE OFFICE NEVER OFFERED IS REFUSED BY NAME.
    PaneDocumentDeclared stranger = gauge_document(kPageA, "x");
    stranger.pane = "other";
    t.declare(stranger);
    CHECK(t.r.last_notice().find("`test.gauge/other` is not a pane that office has offered -- its "
                                 "document was not taken") != std::string::npos);
    CHECK(t.held()->content_id == kPageB);
}

TEST_CASE("a pane's manual counts only while the weave that sent it holds the office, and an "
          "offer made again drops it") {
    GaugeRig t;
    t.offer();
    t.declare(gauge_document(kPageA, "reads one"));
    REQUIRE(t.held() != nullptr);

    // OFFERED AGAIN -- HOW A RELOADED IMAGE ARRIVES, AT THE SAME WEAVE ID: the words go, and an
    // image that says none is held with none, never with its predecessor's.
    t.offer();
    CHECK(t.held() == nullptr);
    CHECK(t.row()->document.sections.empty());
    t.declare(gauge_document(kPageB, "reads two"));
    REQUIRE(t.held() != nullptr);
    CHECK(t.held()->content_id == kPageB);

    // ANOTHER WEAVE TAKES THE OFFICE: the document counts for nothing until it declares its own.
    t.r.unmount_settings_seat(t.seat);
    SettingsSeat* successor = t.r.mount_settings_seat(kGaugeOffice);
    CHECK(t.held() == nullptr);
    const PaneDocumentDeclared own = gauge_document(kPageA, "reads three");
    t.r.drive(successor, [own](SettingsSeat& s, loom::Mail& m) { s.declare_document(m, own); });
    REQUIRE(t.held() != nullptr);
    CHECK(find_pane_document_section(*t.held(), "about")->text == "The gauge reads three.");
}
