// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop persistence suite: what survives a process, and what deliberately does not --
// the file doors and their refusals, the SETUP a weaver names their arrangement by, the desk
// that comes back on its own, and the installed application's roots, isolation, prefs and
// window placement. Every case that needs a file uses a `TempDir` of its own (see
// `workshop_support.hpp`); nothing here writes into the source tree.

#include "workshop_support.hpp"

#include "workshop/desktop-pane/vocabulary.hpp"

// The historical session shapes and their conversions -- the conversion artifact's
// material, named here because this suite owns what a durable session file means.
#include "workshop/layouts/session_history.hpp"
#include "operator/migration.hpp"
#include "workshop/decoded_cells.hpp"

#include <algorithm>
#include <functional>
#include <limits>

// ============================================================================
// Tier 5 — PERSISTENCE: what survives a process, and what deliberately does not
// ============================================================================
// The file doors every durable artifact shares (WL-DOC-15), and what an old object-document
// file meets: nothing at all (WL-DOC-22). The setup, the session and the rest follow.

TEST_CASE("a temporary directory belongs to the suite that made it") {
    // THE ROOT IS NAMED FOR THE SUITE. Several Workshop binaries run at once under CTest,
    // every `TempDir` counter starting at zero, so only the root keeps two suites out of one
    // directory. Asserted rather than trusted, because the failure is not a red: `TempDir`
    // clears what it finds, so a shared path would be one suite deleting another's file.
    const TempDir a("owner");
    const TempDir b("owner");
    CHECK(a.path() != b.path()); // the counter, within one process
    CHECK(a.path().parent_path() == b.path().parent_path());

    // The root is this binary's, and the name in it is the CTest entry's and this process's.
    // A second suite asking for the same tag resolves under a different parent, whatever it
    // calls it -- and so does a second process of THIS suite, which is what an MSVC build and
    // a MinGW build of one suite, run side by side, are.
    CHECK(a.path().parent_path() == workshop_temp_root());
    CHECK(workshop_temp_root().filename().string() ==
          std::string("zengine-workshop-") + ZENGINE_WORKSHOP_SUITE + "-" + this_process_id());

    // ...and it sits directly in the system's temporary directory. Asked by IDENTITY rather
    // than by spelling: `temp_directory_path()` ends in a separator on Windows and does not
    // on Linux, so `parent_path() == temp_directory_path()` is a claim about how a platform
    // writes a path rather than about where the directory is -- it passed on Linux and
    // failed on MSVC saying `C:\...\Temp` != `C:\...\Temp\`. `equivalent` asks the
    // filesystem, and both paths exist here because `a` created them.
    CHECK(std::filesystem::equivalent(workshop_temp_root().parent_path(),
                                      std::filesystem::temp_directory_path()));
}

// ---- The file on disk -------------------------------------------------------

TEST_CASE("a missing file is an ordinary refusal, not a crash and not an empty file") {
    // THE FILE DOORS EVERY DURABLE ARTIFACT SHARES (WL-DOC-15): the setup, the keymap, the
    // prefs, the session, the plan and a pane definition read and write through them.
    TempDir dir("missing");
    const persist::FileText read =
        persist::read_file(dir.file("never-written.json"), 1024, "a test file");
    CHECK_FALSE(read.outcome.accepted);
    CHECK(read.outcome.refusal.find("never-written.json") != std::string::npos);
    CHECK(read.text.empty());
}

TEST_CASE("a detected write failure leaves the last good save readable and unchanged") {
    // The reason the writer never opens the destination: a save that fails must not be able to
    // turn a weaver's file into an empty or half-written one.
    TempDir dir("failsave");
    const std::string path = dir.file("setup.json");
    const std::string first = setup_persist::to_text(default_setup());
    REQUIRE(persist::write_file(path, first).accepted);
    REQUIRE(slurp(path) == first);

    // A controlled, deterministic, non-destructive failure: the sibling path the writer must use
    // is occupied by a DIRECTORY, so the write cannot open. No permission games, and the same
    // result on every supported platform.
    std::filesystem::create_directories(persist::pending_path(path));
    const std::string second = setup_persist::to_text(setup_of("Other", {stock::kKind}));
    REQUIRE(second != first);
    const Written refused = persist::write_file(path, second);
    CHECK_FALSE(refused.accepted);
    CHECK_FALSE(refused.refusal.empty());

    // The last good save is intact, byte for byte, and still reads.
    CHECK(slurp(path) == first);
    CHECK(setup_persist::load_file(path).outcome.accepted);

    std::error_code ec;
    std::filesystem::remove_all(persist::pending_path(path), ec);
    CHECK(persist::write_file(path, second).accepted); // and it works again
    CHECK(slurp(path) == second);
}

TEST_CASE("a save into a place that does not exist refuses before it writes anything") {
    TempDir dir("nowhere");
    const std::string path = (dir.path() / "no-such-directory" / "setup.json").string();
    const Written refused = persist::write_file(path, setup_persist::to_text(default_setup()));
    CHECK_FALSE(refused.accepted);
    CHECK_FALSE(std::filesystem::exists(path));
    CHECK_FALSE(std::filesystem::exists(persist::pending_path(path)));
}

TEST_CASE("a file too large to be what it claims is refused before it is read") {
    TempDir dir("huge");
    const std::string path = dir.file("big.json");
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        const std::string chunk(1u << 16, 'x');
        for (int i = 0; i < 2; ++i) { // 128 KiB, past the bound this read is given
            out.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        }
    }
    const persist::FileText read = persist::read_file(path, 1u << 16, "a test file");
    CHECK_FALSE(read.outcome.accepted);
    CHECK(read.outcome.refusal.find("larger than a test file can be") != std::string::npos);
    CHECK(read.text.empty());
}

TEST_CASE("an old object document is left exactly as it is: a launch names it once, and nothing reads, rewrites or deletes it") {
    // AN OLD OBJECT-DOCUMENT FILE, HANDLED OUT LOUD. A weaver may have one saved with `^s`, and
    // a launch line may still say `--document <it>`. The host says once that it is left alone
    // (`HostContext::retired_document`, WL-DOC-22), and every door of this run leaves its bytes
    // exactly as they were.
    TempDir dir("retired-document");
    const std::string path = dir.file("workshop.json");
    spillout(path, kRetiredObjectDocument);
    Live t;
    t.host.retired_document = path;
    t.host.setup_path = dir.file("setup.json");
    t.ready();
    CHECK(t.notice().find("object document " + path + " left as it is") != std::string::npos);
    CHECK(t.notice().find("nothing here reads or writes it") != std::string::npos);
    // `^s` AND `^o` ARE NOBODY'S, and a setup save beside the file writes only its own file.
    t.key(input::scan::kS, input::mod::kCtrl);
    t.key(input::scan::kO, input::mod::kCtrl);
    t.key(input::scan::kS);
    t.text("s");
    CHECK(std::filesystem::exists(t.host.setup_path));
    CHECK(slurp(path) == kRetiredObjectDocument);
    // ...AND NO READER OF THIS HOST TAKES IT FOR ONE OF ITS OWN.
    CHECK_FALSE(setup_persist::from_text(kRetiredObjectDocument).outcome.accepted);
    CHECK_FALSE(session_persist::from_text(kRetiredObjectDocument).outcome.accepted);
}

TEST_CASE("what the plan could not load and gave way from is said on the desk as it comes up, and "
          "nothing is said of it where every row loaded its own") {
    // ⚔ MUTATION: the startup notes leaving out `HostContext::passed_over` -- the desk comes up
    // in the terminal window and never says the OS window could not open.
    const std::string said = "`zengine-window-sdl` did not load, so `zengine-window-terminal` "
                             "stands in its row -- it would not start here: zengine-window-sdl: "
                             "SDL_Init(SDL_INIT_VIDEO) failed: No available video device";
    Live t;
    t.host.passed_over = [&said] { return std::vector<std::string>{said}; };
    // ...AND IT STANDS FOR THE RUN, as the host keeps it (`gave_way`), so a sentence said
    // after the note leaves it where a weaver reads it.
    t.host.standing_conditions.push_back(
        gave_way("zengine-window-sdl", "zengine-window-terminal", "it would not start"));
    t.ready();
    CHECK(t.notice().find(said) != std::string::npos);
    const Condition* stands = t.session().conditions.find("load.gave-way/zengine-window-sdl");
    REQUIRE(stands != nullptr);
    CHECK(stands->compact ==
          "zengine-window-sdl did not load; zengine-window-terminal stands in its row");

    Live quiet;
    quiet.host.passed_over = [] { return std::vector<std::string>{}; };
    quiet.ready();
    CHECK(quiet.notice().find("did not load") == std::string::npos);
}

// ============================================================================
// Tier 13 — the SETUP: a weaver names the arrangement they are working in
// ============================================================================
// The smallest thing a weaver can NAME, LEAVE and COME BACK TO: a human name and an ordered
// list of two-string references -- no rectangle, WeaveId or pane kind -- honest about panes
// this build does not know.

// ---- The value, its law and its bounds ---------------------------------------

TEST_CASE("a pane reference is two strings, and the pair is the identity") {
    const PaneRef info = info_ref();
    CHECK(info.provider == std::string(kInfoPaneProvider));
    CHECK(info.pane == std::string(kInfoPaneKey));
    CHECK(info == PaneRef{kInfoPaneProvider, kInfoPaneKey});

    // NEITHER HALF ALONE IS THE IDENTITY. The same pane key under another
    // provider is a different pane, and that is the property that let this very pane change
    // hands: `zengine.workshop/info` and `zengine.info/info` are two references, which is why
    // a saved file naming the first is CONVERTED rather than aliased (`pane_migration.hpp`).
    CHECK_FALSE(info == PaneRef{"third.party.tools", kInfoPaneKey});
    CHECK_FALSE(info == PaneRef{kWorkshopProvider, kInfoPaneKey});
    CHECK_FALSE(info == PaneRef{kInfoPaneProvider, "builder"});

    // And nothing is normalised: a reference comes back exactly as it went in.
    const PaneRef odd{"Third.Party.Tools", "History"};
    CHECK_FALSE(odd == stranger());
    CHECK(ref_text(odd) == "Third.Party.Tools/History");
}

TEST_CASE("two setups are the same setup when they name the same panes in the same order") {
    const Setup a = setup_of("Build", {second::kKind, stock::kKind});
    Setup b = setup_of("Build", {second::kKind, stock::kKind});
    CHECK(a == b);

    // THE NAME IS PART OF IT: renaming a setup makes it a different setup, which
    // is what makes `saved()` say UNSAVED after a rename that moved no pane.
    b.name = "Analysis";
    CHECK_FALSE(a == b);

    // ...AND SO IS THE ORDER. The two stand-ins here share the stack, so the
    // order IS which slot each one takes (`bounds_of`); the value distinguishes
    // them whether or not the order moves a rectangle.
    const Setup other_way = setup_of("Build", {stock::kKind, second::kKind});
    CHECK_FALSE(a == other_way);
}

TEST_CASE("a fresh Workshop's setup names one pane this build does not compile") {
    // WHAT EACH DEFAULT IS FOR. The desk is the product default, what a weaver opens with; the
    // open panes are what this host can present before any weave has spoken. Info is in the
    // first and not the second, and that is not a disagreement: it is what a loaded pane
    // looks like at boot.
    const Setup fresh = default_setup();
    CHECK(fresh.name == std::string(kDefaultSetupName));
    REQUIRE(fresh.panes.size() == kDefaultPaneCount + 1);

    // THE COMPILED HALF: every row that resolves is a kind this session already has open,
    // in the setup's order.
    std::vector<std::int64_t> resolved;
    std::vector<PaneRef> waiting;
    for (const SetupPane& p : fresh.panes) {
        const std::optional<std::int64_t> kind = resolve_pane(p.ref, no_providers());
        if (kind.has_value()) {
            resolved.push_back(*kind);
        } else {
            waiting.push_back(p.ref);
        }
    }
    CHECK(resolved == open_kinds(Panes{}));
    CHECK(resolved == std::vector<std::int64_t>{pane_kind::kLayouts});

    // AND THE OTHER HALF IS INFO, NAMED BY THE DESK AND ANSWERED BY A WEAVE. The row carries
    // the right column by NAME, and it is here because a desk is where a weaver's panes are
    // named -- not because this host knows what Info is.
    REQUIRE(waiting.size() == 1);
    CHECK(waiting[0] == info_ref());
    const SetupPane* row = pane_of(fresh, info_ref());
    REQUIRE(row != nullptr);
    CHECK(row->place.mode == pane_unit::kRightColumn);
    CHECK(row->place.x == 0);
    CHECK(row->width.mode == pane_unit::kDefault);

    // A default session agrees with both.
    const Session s;
    CHECK(s.setup.active == fresh);
    CHECK(open_kinds(s.panes) == resolved);
    CHECK(unresolved_panes(s.setup.active, no_providers()).size() == 1);
}

TEST_CASE("every catalog row carries a durable reference that resolves back to it") {
    // Over the POPULATION, not over its size: a third kind is proved by this
    // case without being mentioned in it.
    for (std::size_t i = 0; i < kBuiltinPaneCount; ++i) {
        const BuiltinPane& row = kBuiltinPanes[i];
        INFO("catalog entry ", i, ": ", std::string(row.name));
        REQUIRE(row.provider != nullptr);
        REQUIRE(row.pane != nullptr);
        CHECK_FALSE(std::string(row.provider).empty());
        CHECK_FALSE(std::string(row.pane).empty());

        // The two directions compose: a kind's reference resolves to that kind.
        const PaneRef ref = pane_ref_of(row.kind);
        CHECK(ref.provider == std::string(row.provider));
        CHECK(ref.pane == std::string(row.pane));
        const std::optional<std::int64_t> back = resolve_pane(ref, no_providers());
        REQUIRE(back.has_value());
        CHECK(*back == row.kind);

        // And it is legal as a reference, by the same law a file's is judged by.
        CHECK(check_pane_ref(ref).accepted);
    }

    // The built-ins are spelled so a weaver can read them.
    CHECK(ref_text(info_ref()) == "zengine.info/info");
    CHECK(ref_text(ref_of(stock::kKind)) == "zengine.test.stack/stack");
}

TEST_CASE("an unknown reference resolves to NOTHING, and never to the catalog's first row") {
    // THE WHOLE REASON THE FALLIBLE DOOR EXISTS. `builtin_pane` answers with the
    // catalog's first row for an unknown kind, which is right for its callers
    // and would be a lie here: an unknown reference routed through it would
    // paint a weaver's third-party pane as one of Workshop's own built-ins.
    CHECK_FALSE(resolve_pane(stranger(), no_providers()).has_value());
    CHECK_FALSE(resolve_pane(PaneRef{"third.party.tools", "info"}, no_providers()).has_value());
    CHECK_FALSE(resolve_pane(PaneRef{kWorkshopProvider, "history"}, no_providers()).has_value());
    CHECK_FALSE(resolve_pane(PaneRef{"", ""}, no_providers()).has_value());
    CHECK_FALSE(resolvable(stranger(), no_providers()));

    // The negative control, stated as its own claim: the total lookup DOES answer the
    // catalog's first row for an unknown kind, and may, because nothing that meets a file goes
    // through it. WHICH row is an accident of order, so it is read from the catalog rather
    // than named: a spelled constant would fail for the wrong reason when the catalog changes.
    CHECK(builtin_pane(9999).kind == kBuiltinPanes[0].kind);
    CHECK(resolve_pane(stranger(), no_providers()).value_or(kNoPaneKind) !=
          kBuiltinPanes[0].kind);
}

TEST_CASE("what this application accepts as a setup name") {
    CHECK(check_setup_name("Default").accepted);
    CHECK(check_setup_name("Morning build").accepted);                    // spaces allowed
    CHECK(check_setup_name(std::string(kMaxSetupNameLen, 'x')).accepted); // exactly the bound

    CHECK_FALSE(check_setup_name("").accepted);
    CHECK_FALSE(check_setup_name("   ").accepted); // more than spaces
    CHECK_FALSE(check_setup_name(std::string(kMaxSetupNameLen + 1, 'x')).accepted);
    // A control character can only arrive from a forged file, and what it would
    // do is move a terminal's cursor out of the line it was given.
    CHECK_FALSE(check_setup_name("Build\nmore").accepted);
    CHECK_FALSE(check_setup_name(std::string("Build\x1b[2J")).accepted);
    CHECK_FALSE(check_setup_name(std::string("Build\x7f")).accepted);

    // The refusals name what is wrong, because the reason IS the feature.
    CHECK(check_setup_name("").refusal.find("empty") != std::string::npos);
    CHECK(check_setup_name(std::string(kMaxSetupNameLen + 1, 'x'))
              .refusal.find(std::to_string(kMaxSetupNameLen)) != std::string::npos);
    CHECK(check_setup_name("a\tb").refusal.find("control") != std::string::npos);
}

TEST_CASE("what this application accepts as either half of a reference") {
    CHECK(check_pane_key("zengine.workshop", "provider").accepted);
    CHECK(check_pane_key(std::string(kMaxPaneKeyLen, 'a'), "pane key").accepted);

    CHECK_FALSE(check_pane_key("", "provider").accepted);
    CHECK_FALSE(check_pane_key(std::string(kMaxPaneKeyLen + 1, 'a'), "provider").accepted);
    CHECK_FALSE(check_pane_key("two words", "pane key").accepted);
    CHECK_FALSE(check_pane_key("line\nbreak", "pane key").accepted);

    // The refusal says WHICH half, because `provider` and `pane key` are two
    // fields a weaver looking at their own file has to tell apart.
    CHECK(check_pane_ref(PaneRef{"", "info"}).refusal.find("provider") != std::string::npos);
    CHECK(check_pane_ref(PaneRef{"zengine.workshop", ""}).refusal.find("pane key") !=
          std::string::npos);

    // AND IT JUDGES A `std::string_view`. The two boundaries are asserted through a view to
    // say so: exactly `kMaxPaneKeyLen` bytes accepted, one more refused.
    const std::string one_over(kMaxPaneKeyLen + 1, 'a');
    const std::string_view exactly(one_over.data(), kMaxPaneKeyLen);
    REQUIRE(exactly.size() == kMaxPaneKeyLen);
    CHECK(check_pane_key(exactly, "provider").accepted);
    CHECK_FALSE(check_pane_key(std::string_view(one_over), "provider").accepted);
    CHECK(check_pane_key(std::string_view(one_over), "provider").refusal ==
          "a pane reference's provider is at most 64 bytes");

    // A VIEW IS LENGTH-BEARING AND THIS LAW SPENDS ITS LENGTH. `exactly` is a
    // window onto the first sixty-four bytes of a sixty-five-byte string, so there
    // is no terminator where the view ends -- a checker that read to one would have
    // seen the sixty-fifth byte and refused. Accepting it is the assertion that the
    // size, and only the size, was measured.
    CHECK(exactly.data()[exactly.size()] == 'a');

    // ...AND EMPTY, SPACE AND CONTROL ARE THE ANSWERS THEY WERE, asked with a view.
    CHECK_FALSE(check_pane_key(std::string_view(""), "provider").accepted);
    CHECK(check_pane_key(std::string_view(""), "provider").refusal ==
          "a pane reference's provider cannot be empty");
    CHECK_FALSE(check_pane_key(std::string_view("two words"), "pane key").accepted);
    CHECK_FALSE(check_pane_key(std::string_view("line\nbreak"), "pane key").accepted);
    CHECK(check_pane_key(std::string_view("zengine.workshop"), "provider").accepted);
}

TEST_CASE("the whole-setup law: duplicates, the count bound, and an empty list") {
    CHECK(check_setup(default_setup()).accepted);

    // AN EMPTY PANE LIST IS LEGAL. "I want nothing open" is reachable through
    // the close door already, so refusing to save it would make one arrangement a
    // weaver can produce impossible to name.
    Setup empty;
    empty.name = "Nothing";
    CHECK(check_setup(empty).accepted);

    // AN UNRESOLVED REFERENCE IS LEGAL, and this is the case that says the law
    // and the resolution are different questions.
    Setup future;
    future.name = "Later";
    REQUIRE(add_pane(future, stranger()));
    CHECK(check_setup(future).accepted);
    CHECK_FALSE(resolvable(future.panes.front().ref, no_providers()));

    // A DUPLICATE IS NOT. A kind is open or it is not; a file naming one twice
    // was written by somebody who believed in a policy this application does not
    // have, and silently keeping one of the two would hide that.
    Setup twice = setup_of("Twice", {stock::kKind});
    // FORGED PAST THE DOOR ON PURPOSE: `add_pane` refuses a duplicate, so a case
    // about what `check_setup` says of one has to build it by hand -- with a rank
    // that is otherwise legal, so the refusal below is about the DUPLICATE and not
    // about the permutation.
    twice.panes.push_back(SetupPane{ref_of(stock::kKind), {}, {}, {}, 1, {}});
    CHECK_FALSE(check_setup(twice).accepted);
    CHECK(check_setup(twice).refusal.find("twice") != std::string::npos);
    CHECK(check_setup(twice).refusal.find("zengine.test.stack/stack") != std::string::npos);

    // ...and neither is more than a setup may hold. THE BOUND IS NOT THE
    // CATALOG'S POPULATION, deliberately: a setup must be able to retain
    // references to panes this build has never heard of, so a limit cut to
    // `kBuiltinPaneCount` would refuse the one case the design exists to allow.
    CHECK(kMaxSetupPanes > kBuiltinPaneCount);
    Setup many_panes;
    many_panes.name = "Many";
    for (std::size_t i = 0; i < kMaxSetupPanes; ++i) {
        REQUIRE(add_pane(many_panes, PaneRef{"third.party.tools", "p" + std::to_string(i)}));
    }
    CHECK(check_setup(many_panes).accepted);
    REQUIRE(add_pane(many_panes, PaneRef{"third.party.tools", "one-too-many"}));
    CHECK_FALSE(check_setup(many_panes).accepted);
    CHECK(check_setup(many_panes).refusal.find(std::to_string(kMaxSetupPanes)) !=
          std::string::npos);

    // A bad name and a bad reference are both refused by the one whole-setup
    // law, so a caller cannot check one and forget the other.
    const Setup unnamed = setup_of("", {second::kKind});
    CHECK_FALSE(check_setup(unnamed).accepted);
    Setup bad_ref = setup_of("Bad", {});
    REQUIRE(add_pane(bad_ref, PaneRef{"has space", "info"}));
    CHECK_FALSE(check_setup(bad_ref).accepted);
}

TEST_CASE("adding and removing a pane preserves order and never duplicates") {
    Setup s;
    s.name = "Work";
    CHECK(add_pane(s, info_ref()));
    CHECK(add_pane(s, ref_of(stock::kKind)));
    CHECK_FALSE(add_pane(s, info_ref())); // already there, and it says so
    REQUIRE(s.panes.size() == 2);
    CHECK(s.panes[0].ref == info_ref());
    CHECK(s.panes[1].ref == ref_of(stock::kKind));
    CHECK(check_setup(s).accepted);

    // ADDED AT THE END, which is where `open_kind` has always put a newly
    // opened pane -- so the authored order agrees with the resolved order a
    // weaver was already watching.
    CHECK(remove_pane(s, info_ref()));
    REQUIRE(s.panes.size() == 1);
    CHECK(s.panes[0].ref == ref_of(stock::kKind));
    CHECK_FALSE(remove_pane(s, info_ref()));

    // An unresolved reference is an ordinary member: it can be added, found and
    // removed by exactly the same three functions.
    CHECK(add_pane(s, stranger()));
    CHECK(has_pane(s, stranger()));
    CHECK(remove_pane(s, stranger()));
    CHECK_FALSE(has_pane(s, stranger()));
}

TEST_CASE("the unresolved panes are reported in the setup's own order") {
    Setup s;
    s.name = "Mixed";
    REQUIRE(add_pane(s, ref_of(second::kKind)));
    REQUIRE(add_pane(s, stranger()));
    REQUIRE(add_pane(s, PaneRef{"other.tools", "graph"}));
    REQUIRE(add_pane(s, ref_of(stock::kKind)));

    Panes with_stock; // the stand-in is a runtime pane: it resolves where it was offered
    admit_stock(with_stock);
    admit_second(with_stock);
    const std::vector<PaneRef> waiting = unresolved_panes(s, with_stock);
    REQUIRE(waiting.size() == 2);
    CHECK(waiting[0] == stranger());
    CHECK(waiting[1] == PaneRef{"other.tools", "graph"});
    // AND THE SHIPPED DESK HAS ONE OF ITS OWN, WHICH IS NOT A DEFECT. `default_setup` names
    // the Info pane and Info is a weave: with no provider in the room its row is unresolved,
    // exactly as a weaver's desk naming a pane whose office has not spoken yet.
    const std::vector<PaneRef> fresh = unresolved_panes(default_setup(), no_providers());
    REQUIRE(fresh.size() == 1);
    CHECK(fresh[0] == info_ref());
}

// ---- Authored intent, reconciled onto resolved presentations ------------------

TEST_CASE("reconciling opens what the setup names, in the setup's order") {
    Panes panes;
    admit_stock(panes); // the stand-in, first (stock)
    admit_second(panes); // ...and the second
    REQUIRE(open_kinds(panes) == std::vector<std::int64_t>{pane_kind::kLayouts});

    const Reconciled done = reconcile(
        panes, setup_of("Both", {stock::kKind, second::kKind, pane_kind::kLayouts}));
    CHECK(done.opened == std::vector<std::int64_t>{stock::kKind, second::kKind});
    CHECK(done.closed.empty());
    CHECK(done.unresolved == 0);
    // THE ORDER IS THE SETUP'S, not the order things happened to be opened in.
    CHECK(open_kinds(panes) ==
          std::vector<std::int64_t>{stock::kKind, second::kKind, pane_kind::kLayouts});

    // The other way round, from the same starting point, produces the other order.
    Panes again;
    admit_stock(again); // the stand-in, first (stock)
    admit_second(again); // ...and the second
    (void)reconcile(again, setup_of("Both", {second::kKind, stock::kKind}));
    CHECK(open_kinds(again) == std::vector<std::int64_t>{second::kKind, stock::kKind});
}

TEST_CASE("reconciling closes what the setup does not name, through the existing door") {
    Panes panes;
    admit_stock(panes); // the stand-in, first (stock)
    admit_second(panes); // ...and the second
    REQUIRE(open_kind(panes, stock::kKind));

    const Reconciled done = reconcile(
        panes, setup_of("Panes and layouts", {second::kKind, pane_kind::kLayouts}));
    CHECK(done.closed == std::vector<std::int64_t>{stock::kKind});
    CHECK(done.opened == std::vector<std::int64_t>{second::kKind});
    CHECK(open_kinds(panes) == std::vector<std::int64_t>{second::kKind, pane_kind::kLayouts});

    // AND THE CLOSE GOES THROUGH `close_kind` RATHER THAN A LOOP THAT REBUILDS THE VECTOR:
    // the door was spent -- the presentation is gone from `open`, and the setup that named it
    // is untouched.
    CHECK_FALSE(panes.has(stock::kKind));
}

TEST_CASE("a pane open on both sides of a reconcile keeps what it was showing") {
    // OPEN BEFORE, OPEN AFTER -- the case that says a reconcile is not a rebuild.
    // Restoring the setup you are already in must not be a visible event.
    Panes panes;
    admit_stock(panes); // the stand-in, first (stock)
    admit_second(panes); // ...and the second
    REQUIRE(open_kind(panes, stock::kKind));

    // OPENED FIRST, so the second reconcile below is the one this case is about: a fresh
    // `Panes` has only the Layouts pane on it, and the second stand-in arrives with the
    // setup rather than with the boot.
    const Setup same = setup_of("Both", {second::kKind, stock::kKind, pane_kind::kLayouts});
    (void)reconcile(panes, same);
    const Reconciled done = reconcile(panes, same);
    CHECK(done.opened.empty());
    CHECK(done.closed.empty());
    CHECK(open_kinds(panes) ==
          std::vector<std::int64_t>{second::kKind, stock::kKind, pane_kind::kLayouts});

    // ...and doing it a second time changes nothing at all.
    const Reconciled twice = reconcile(panes, same);
    CHECK(twice.opened.empty());
    CHECK(twice.closed.empty());
    CHECK(open_kinds(panes) ==
          std::vector<std::int64_t>{second::kKind, stock::kKind, pane_kind::kLayouts});
}

TEST_CASE("an unresolved reference is counted, and produces no pane of any kind") {
    Panes panes;
    admit_stock(panes); // the stand-in, first (stock)
    admit_second(panes); // ...and the second
    Setup s = setup_of("Mixed", {second::kKind});
    REQUIRE(add_pane(s, stranger()));
    REQUIRE(add_pane(s, PaneRef{"other.tools", "graph"}));

    const Reconciled done = reconcile(panes, s);
    CHECK(done.unresolved == 2);
    // NO PLACEHOLDER, NO SLOT, NO FALL-THROUGH TO THE BUILDER. The only kind
    // available to paint an unknown pane with is the Builder, which is exactly
    // why the resolution had to be fallible before this line could be written.
    CHECK(open_kinds(panes) == std::vector<std::int64_t>{second::kKind});
    CHECK_FALSE(panes.has(stock::kKind));
    // And the setup still holds all three: reconciling takes it by const
    // reference and could not drop one if it wanted to.
    CHECK(s.panes.size() == 3);
    CHECK(s.panes[1].ref == stranger());
}

TEST_CASE("an empty setup closes everything, and is a legal thing to be in") {
    Panes panes;
    admit_stock(panes); // the stand-in, first (stock)
    admit_second(panes); // ...and the second
    REQUIRE(open_kind(panes, stock::kKind));
    Setup nothing;
    nothing.name = "Nothing";

    const Reconciled done = reconcile(panes, nothing);
    // TWO: the Layouts pane a fresh Workshop opens, and the Editor this case opened over it;
    // Info arrives with a weave and is not here. An empty setup is a legal thing to be in -- a
    // Workshop with no layout surface either -- and the desktop's Pane Manager
    // (`desktop.panes`) is the way back.
    CHECK(done.closed.size() == 2);
    CHECK(panes.open.empty());
    CHECK(done.unresolved == 0);

    // And back again from empty, which is the case that proves `opened` names
    // every kind rather than only the ones that were never open.
    const Reconciled back = reconcile(panes, setup_of("Both", {second::kKind, stock::kKind}));
    CHECK(back.opened.size() == 2);
    CHECK(open_kinds(panes) == std::vector<std::int64_t>{second::kKind, stock::kKind});
}

// ---- The setup's own file ------------------------------------------------------

TEST_CASE("a setup file says what it is, in words a weaver can read") {
    const std::string text = setup_persist::to_text(default_setup());
    INFO(text);

    // ITS OWN FORMAT IDENTITY, beside the document's and not equal to it.
    CHECK(text.find("\"format\":\"zengine-workshop-setup\"") != std::string::npos);
    CHECK(text.find("\"format_version\":\"6\"") != std::string::npos);
    CHECK(text.find("\"name\":\"Default\"") != std::string::npos);
    CHECK(text.find("\"provider\":\"zengine.workshop\"") != std::string::npos);
    CHECK(text.find("\"pane\":\"info\"") != std::string::npos);

    // NO INTEGER PANE KIND ANYWHERE IN IT. This is the assertion to check this
    // format against first: the file names panes the way a person and a future
    // provider would, and never the way this build's own vocabulary does.
    CHECK(text.find("\"kind\"") == std::string::npos);
    CHECK(text.find("placement") == std::string::npos);
    CHECK(text.find("rect") == std::string::npos);
    CHECK(text.find("weave") == std::string::npos);

    // ...and it is not the retired object document's format, which is the whole point of it
    // having one of its own: handing Workshop an old file of that kind is named rather than
    // half-read.
    CHECK(std::string(kRetiredObjectDocument).find(std::string("\"") + setup_persist::kFormat +
                                                   "\"") == std::string::npos);
}

TEST_CASE("every shape of setup survives a round trip through its file") {
    struct Case {
        const char* what;
        Setup setup;
    };
    std::vector<Case> cases;
    cases.push_back({"the default, Info only", default_setup()});
    cases.push_back({"Builder only", setup_of("Build", {stock::kKind})});
    cases.push_back({"both, in a deliberate order",
                     setup_of("Everything", {stock::kKind, second::kKind})});
    cases.push_back({"both, in the other order",
                     setup_of("Everything", {second::kKind, stock::kKind})});
    Setup nothing;
    nothing.name = "Nothing at all";
    cases.push_back({"an empty pane list", nothing});
    cases.push_back({"a human name with spaces in it", setup_of("Morning build", {second::kKind})});
    cases.push_back({"a name at the bound",
                     setup_of(std::string(kMaxSetupNameLen, 'n'), {second::kKind})});
    Setup mixed = setup_of("Mixed", {second::kKind});
    REQUIRE(add_pane(mixed, stranger()));
    REQUIRE(add_pane(mixed, ref_of(stock::kKind)));
    cases.push_back({"a reference this build cannot resolve, between two it can", mixed});

    for (const Case& c : cases) {
        CAPTURE(c.what);
        const std::string a = setup_persist::to_text(c.setup);
        const setup_persist::LoadedSetup read = setup_persist::from_text(a);
        REQUIRE(read.outcome.accepted);
        CHECK(read.setup == c.setup);

        // SAVE -> LOAD -> SAVE IS BYTE-IDENTICAL. No canonicalisation framework
        // and no sorting: the codec's output is deterministic and nothing here
        // reorders, normalises or resolves a value on its way through.
        const std::string b = setup_persist::to_text(read.setup);
        CHECK(a == b);
    }
}

TEST_CASE("saving never sorts, normalises, resolves or drops a reference") {
    // The strongest version of the claim: a setup whose order is NOT the
    // catalog's, containing an entry this build cannot present, in the middle.
    Setup s;
    s.name = "Deliberate";
    REQUIRE(add_pane(s, ref_of(stock::kKind)));
    REQUIRE(add_pane(s, stranger()));
    REQUIRE(add_pane(s, info_ref()));

    const setup_persist::LoadedSetup read = setup_persist::from_text(setup_persist::to_text(s));
    REQUIRE(read.outcome.accepted);
    REQUIRE(read.setup.panes.size() == 3);
    CHECK(read.setup.panes[0].ref == ref_of(stock::kKind));
    CHECK(read.setup.panes[1].ref == stranger());
    CHECK(read.setup.panes[2].ref == info_ref());

    // The unresolved entry's bytes are exactly the bytes that went in.
    const std::string text = setup_persist::to_text(read.setup);
    CHECK(text.find("\"provider\":\"third.party.tools\"") != std::string::npos);
    CHECK(text.find("\"pane\":\"history\"") != std::string::npos);
}

TEST_CASE("a malformed setup file is refused, and the live setup is untouched") {
    // The claim is not "the parser returned an error". It is that the setup a
    // weaver is in is exactly what it was.
    const Setup good = setup_of("Everything", {pane_kind::kLayouts, stock::kKind});
    const std::string valid = setup_persist::to_text(good);

    struct Case {
        const char* what;
        std::string text;
    };
    std::vector<Case> cases;
    cases.push_back({"not JSON at all", "{ this is not a setup"});
    cases.push_back({"an empty file", ""});
    cases.push_back({"a JSON array", "[1,2,3]"});
    cases.push_back({"someone else's value",
                     loom::compat::serialize(loom::to_value(ui::Size{ui::kSizePixels, 4}))});
    cases.push_back({"a retired Workshop object DOCUMENT handed to the setup reader",
                     kRetiredObjectDocument});
    cases.push_back({"a valid setup with a tail after it", valid + "x"});
    cases.push_back({"the wrong format identity",
                     forged_setup(good, "\"zengine-workshop-setup\"", "\"someone-elses-tool\"")});
    cases.push_back({"an unsupported format version",
                     forged_setup(good, "\"format_version\":\"6\"", "\"format_version\":\"9\"")});
    cases.push_back({"a missing required field",
                     forged_setup(good, "\"name\":\"Everything\",", "")});
    cases.push_back({"a field of the wrong kind",
                     forged_setup(good, "\"format_version\":\"6\"", "\"format_version\":3")});
    cases.push_back({"a field the setup does not declare",
                     forged_setup(good, "\"name\":", "\"colour\":\"red\",\"name\":")});
    cases.push_back({"a field a pane reference does not declare",
                     forged_setup(good, "\"provider\":", "\"rect\":\"1\",\"provider\":")});
    cases.push_back({"a name that is not a name",
                     forged_setup(good, "\"name\":\"Everything\"", "\"name\":\"\"")});
    cases.push_back({"a name that is only spaces",
                     forged_setup(good, "\"name\":\"Everything\"", "\"name\":\"   \"")});
    cases.push_back({"a name longer than a name",
                     forged_setup(good, "\"name\":\"Everything\"",
                                  "\"name\":\"" + std::string(kMaxSetupNameLen + 1, 'x') +
                                      "\"")});
    cases.push_back({"a name carrying a control character",
                     forged_setup(good, "\"name\":\"Everything\"", "\"name\":\"Ever\\ttime\"")});
    cases.push_back({"an empty provider key",
                     forged_setup(good, "\"provider\":\"zengine.workshop\"", "\"provider\":\"\"")});
    cases.push_back({"an empty pane key",
                     forged_setup(good, "\"pane\":\"layouts\"", "\"pane\":\"\"")});
    cases.push_back({"a provider key longer than a key",
                     forged_setup(good, "\"provider\":\"zengine.workshop\"",
                                  "\"provider\":\"" + std::string(kMaxPaneKeyLen + 1, 'p') +
                                      "\"")});
    cases.push_back({"a pane key with a space in it",
                     forged_setup(good, "\"pane\":\"layouts\"", "\"pane\":\"two words\"")});
    cases.push_back({"the same pane named twice",
                     forged_setup(good, "\"provider\":\"zengine.test.stack\",\"pane\":\"stack\"",
                                  "\"provider\":\"zengine.workshop\",\"pane\":\"layouts\"")});

    // ...and one that has to be built rather than forged: more panes than a
    // setup may hold.
    Setup crowd;
    crowd.name = "Crowd";
    for (std::size_t i = 0; i <= kMaxSetupPanes; ++i) {
        REQUIRE(add_pane(crowd, PaneRef{"third.party.tools", "p" + std::to_string(i)}));
    }
    cases.push_back({"more panes than a setup may hold", setup_persist::to_text(crowd)});

    for (const Case& c : cases) {
        CAPTURE(c.what);
        const setup_persist::LoadedSetup refused = setup_persist::from_text(c.text);
        CHECK_FALSE(refused.outcome.accepted);
        CHECK_FALSE(refused.outcome.refusal.empty()); // a refusal without a reason is not one
        // And nothing was carried out of it: the candidate a caller would have
        // reconciled is empty, which is what makes "never halfway restored"
        // structural rather than careful.
        CHECK(refused.setup.name.empty());
        CHECK(refused.setup.panes.empty());
    }

    // The control: the file these were all forged from still loads.
    const setup_persist::LoadedSetup ok = setup_persist::from_text(valid);
    CHECK(ok.outcome.accepted);
    CHECK(ok.setup == good);
}

TEST_CASE("a setup file on disk is the setup read back from it") {
    TempDir dir("setup-roundtrip");
    const std::string path = dir.file("setup.json");
    const Setup original = setup_of("Everything", {stock::kKind, second::kKind});
    REQUIRE(setup_persist::save_file(path, original).accepted);

    const setup_persist::LoadedSetup read = setup_persist::load_file(path);
    REQUIRE(read.outcome.accepted);
    CHECK(read.setup == original);

    // The bytes on disk are the bytes the writer produced -- no wrapper, no
    // trailer, nothing added by the file layer -- and the sibling is gone.
    CHECK(slurp(path) == setup_persist::to_text(original));
    CHECK_FALSE(std::filesystem::exists(persist::pending_path(path)));

    // Saving again over an existing setup replaces it.
    const Setup second = setup_of("Info only", {second::kKind});
    REQUIRE(setup_persist::save_file(path, second).accepted);
    CHECK(setup_persist::load_file(path).setup == second);
}

TEST_CASE("a missing setup file is an ordinary refusal, not an empty setup") {
    TempDir dir("setup-missing");
    const setup_persist::LoadedSetup refused = setup_persist::load_file(dir.file("never.json"));
    CHECK_FALSE(refused.outcome.accepted);
    CHECK(refused.outcome.refusal.find("never.json") != std::string::npos);
}

TEST_CASE("a file too large to be a setup is refused before it is read") {
    TempDir dir("setup-huge");
    const std::string path = dir.file("setup.json");
    spillout(path, std::string(static_cast<std::size_t>(setup_persist::kMaxSetupBytes) + 1, 'x'));
    REQUIRE(std::filesystem::file_size(path) > setup_persist::kMaxSetupBytes);

    const setup_persist::LoadedSetup refused = setup_persist::load_file(path);
    CHECK_FALSE(refused.outcome.accepted);
    CHECK(refused.outcome.refusal.find("larger") != std::string::npos);
    // It names WHICH of the two artifacts it was measuring against, because a
    // weaver with two files needs to know which ceiling they met.
    CHECK(refused.outcome.refusal.find("setup") != std::string::npos);
}

TEST_CASE("a detected setup write failure leaves the last good setup file untouched") {
    // The reason the writer never opens the destination, asked about the second
    // artifact: a save that fails must not be able to turn a weaver's saved
    // arrangement into an empty or half-written file.
    TempDir dir("setup-failsave");
    const std::string path = dir.file("setup.json");
    const Setup first = setup_of("Good", {second::kKind});
    REQUIRE(setup_persist::save_file(path, first).accepted);
    const std::string good_bytes = slurp(path);
    REQUIRE_FALSE(good_bytes.empty());

    // A controlled, deterministic, non-destructive failure: the sibling path the
    // writer must use is occupied by a DIRECTORY, so the write cannot open.
    std::filesystem::create_directories(persist::pending_path(path));

    const Setup second = setup_of("Better", {stock::kKind, second::kKind});
    const Written refused = setup_persist::save_file(path, second);
    CHECK_FALSE(refused.accepted);
    CHECK_FALSE(refused.refusal.empty());

    // The last good save is intact, byte for byte, and still loads.
    CHECK(slurp(path) == good_bytes);
    const setup_persist::LoadedSetup reloaded = setup_persist::load_file(path);
    REQUIRE(reloaded.outcome.accepted);
    CHECK(reloaded.setup == first);

    std::error_code ec;
    std::filesystem::remove_all(persist::pending_path(path), ec);
    CHECK(setup_persist::save_file(path, second).accepted); // and it works again
    CHECK(setup_persist::load_file(path).setup == second);
}

TEST_CASE("an old object document's file and the setup's file cannot be mistaken for each other") {
    // OLD OBJECT-DOCUMENT FILES STAY ON WEAVERS' DISKS. The setup reader refuses one by name
    // rather than half-reading it, and writing a setup leaves an old document's bytes alone --
    // what "two artifacts" is worth, said in the only way that could fail.
    TempDir dir("two-files");
    const std::string doc_path = dir.file("workshop.json");
    const std::string setup_path = dir.file("setup.json");
    spillout(doc_path, kRetiredObjectDocument);
    REQUIRE(setup_persist::save_file(setup_path, default_setup()).accepted);
    CHECK_FALSE(setup_persist::load_file(doc_path).outcome.accepted);
    REQUIRE(setup_persist::save_file(setup_path, setup_of("Other", {stock::kKind})).accepted);
    CHECK(slurp(doc_path) == kRetiredObjectDocument);
}

// ---- Through the real weave: the doors move the intent ---------------------------

TEST_CASE("a fresh Workshop's active setup and its open panes agree from the first frame") {
    Live t;
    CHECK(t.session().setup.active == default_setup());
    CHECK(open_kinds(t.session().panes) ==
          std::vector<std::int64_t>{pane_kind::kLayouts});
    // UNSAVED, and structurally so: nothing has been written, and the copy the
    // comparison is made against has an empty name no legal setup can equal.
    CHECK_FALSE((live_status(t.run()) == setup_link::kCurrent));
}

TEST_CASE("opening a pane through the launch door moves the setup's intent, not only the screen") {
    // THE COHERENCE CLAIM: if the launch door called `open_kind` directly, the active setup
    // would go on describing an arrangement the screen stopped showing the moment a pane was
    // launched.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    open_stock_pane(t);

    REQUIRE(t.session().panes.has(stock::kKind));
    CHECK(has_pane(t.session().setup.active, ref_of(stock::kKind)));
    // At the END of the authored order, which is where the screen put it -- behind the
    // Layouts pane a fresh Workshop already had, because the door appends.
    REQUIRE(t.session().setup.active.panes.size() == 3);
    CHECK(t.session().setup.active.panes[2].ref == ref_of(stock::kKind));
    // ...and the resolved open order agrees with the resolved order of the refs.
    CHECK(open_kinds(t.session().panes) ==
          std::vector<std::int64_t>{pane_kind::kLayouts, stock::kKind});

    // Removing it takes the reference back out.
    pick(t, stock::kKind);
    CHECK_FALSE(t.session().panes.has(stock::kKind));
    CHECK_FALSE(has_pane(t.session().setup.active, ref_of(stock::kKind)));

    // ...AND SO DOES REMOVING LAYOUTS: the layout run is a pane a weaver may take off their
    // desk through the ordinary door, leaving an empty desk rather than a Workshop with one
    // surface it cannot lose.
    pick(t, pane_kind::kLayouts);
    CHECK_FALSE(t.session().panes.has(pane_kind::kLayouts));
    // ...AND WHAT IS LEFT IN THE DESK IS THE INFO ROW THE SHIPPED SETUP NAMED, which no
    // provider in this rig can resolve. A desk row for a pane nobody is offering is still a
    // desk row, which is the whole of what an unresolved reference is (WL-SETUP-01).
    REQUIRE(t.session().setup.active.panes.size() == 1);
    CHECK(t.session().setup.active.panes[0].ref == info_ref());
}

TEST_CASE("a pane change makes the setup UNSAVED, and changing it back makes it saved again") {
    TempDir dir("setup-saved-truth");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    (void)mount_tool(t, "zengine-snake");

    name_setup(t, "Working");
    REQUIRE((live_status(t.run()) == setup_link::kCurrent));
    CHECK(t.session().setup.active.name == "Working");
    CHECK(t.notice().find("saved setup \"Working\"") == 0);

    open_stock_pane(t);
    CHECK_FALSE((live_status(t.run()) == setup_link::kCurrent));

    // OPENING THEN CLOSING BACK TO THE SAVED INTENT SAYS SAVED AGAIN, which is
    // what a comparison buys and a dirty flag could not: there is no hand to
    // forget to unset.
    pick(t, stock::kKind);
    CHECK((live_status(t.run()) == setup_link::kCurrent));

    // A rename with no pane moved is still a change, because the name is part of
    // the value.
    Setup renamed = t.session().setup.active;
    renamed.name = "Other";
    CHECK_FALSE(renamed == t.run().active_link.known);
}

TEST_CASE("the rename editor opens on the tab's own name and writes nothing") {
    TempDir dir("layout-rename");
    Live t;
    t.host.setup_path = dir.file("setup.json");

    open_rename_on_tab(t, t.session().setup.active_at);
    REQUIRE(t.session().setup.naming.open);
    // IT OPENED ON THE NAME THE LAYOUT ALREADY HAS, with the caret at its end.
    CHECK(t.session().setup.naming.line.text() == "Default");
    CHECK(t.session().setup.naming.line.caret() == std::string("Default").size());
    CHECK(t.session().setup.naming.at == t.session().setup.active_at);

    // AND `s` DOES NOT OPEN IT: saving a Setup and naming a layout are separate gestures.
    t.key(input::scan::kEscape);
    t.key(input::scan::kS);
    t.text("s");
    CHECK_FALSE(t.session().setup.naming.open);
    CHECK(std::filesystem::exists(t.host.setup_path)); // it SAVED instead
}

TEST_CASE("escape leaves the layout name exactly as it was, and writes nothing") {
    TempDir dir("setup-cancel");
    Live t;
    t.host.setup_path = dir.file("setup.json");

    open_rename_on_tab(t, t.session().setup.active_at);
    t.text("!");
    REQUIRE(t.session().setup.naming.line.text() == "Default!");
    t.key(input::scan::kEscape);

    CHECK_FALSE(t.session().setup.naming.open);
    CHECK(t.session().setup.active.name == "Default");
    CHECK_FALSE(std::filesystem::exists(t.host.setup_path));
    CHECK(t.notice().find("unchanged") != std::string::npos);
}

TEST_CASE("a name the law refuses leaves the editor open over what was typed") {
    TempDir dir("setup-badname");
    Live t;
    t.host.setup_path = dir.file("setup.json");

    open_rename_on_tab(t, t.session().setup.active_at);
    for (int i = 0; i < 8; ++i) {
        t.key(input::scan::kBackspace);
    }
    REQUIRE(t.session().setup.naming.line.empty());
    t.key(input::scan::kReturn);

    // STILL OPEN, so a weaver fixes what they typed rather than retyping it, and
    // nothing was written.
    CHECK(t.session().setup.naming.open);
    CHECK(t.session().setup.active.name == "Default");
    CHECK_FALSE(std::filesystem::exists(t.host.setup_path));
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("empty") != std::string::npos);

    // ...and typing a legal one from there works.
    for (const char c : std::string("Fixed")) {
        t.text(std::string(1, c));
    }
    t.key(input::scan::kReturn);
    CHECK_FALSE(t.session().setup.naming.open);
    CHECK(t.session().setup.active.name == "Fixed");
    // A RENAME WRITES NO SETUP ARTIFACT. The layout is named and the file the host configured
    // was never opened, in either direction.
    CHECK_FALSE(std::filesystem::exists(t.host.setup_path));
    CHECK(live_status(t.run()) == setup_link::kNone);
}

TEST_CASE("with no setup file, saving and restoring say so and change nothing") {
    Live t; // no --setup was given
    t.key(input::scan::kS);
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("--setup") != std::string::npos);
    // ...AND RENAMING STILL WORKS, because it touches no artifact: no refusal that guards a
    // save applies to it.
    open_rename_on_tab(t, t.session().setup.active_at);
    CHECK(t.session().setup.naming.open);
    t.key(input::scan::kEscape);

    const Setup before = t.session().setup.active;
    t.key(input::scan::kR);
    CHECK(t.session().setup.active == before);
    CHECK(t.notice().find("--setup") != std::string::npos);
    // ...and it is a DIFFERENT sentence from the document's, because a weaver
    // with one file and not the other has to know which one they are missing.
    CHECK(t.notice().find("--document") == std::string::npos);
}

TEST_CASE("restoring a setup returns the intent that was saved") {
    TempDir dir("setup-restore");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    (void)mount_tool(t, "zengine-snake");

    open_stock_pane(t); // the stand-in open beside the Layouts pane
    name_setup(t, "Build only");
    REQUIRE((live_status(t.run()) == setup_link::kCurrent));

    // Wander away from it.
    pick(t, stock::kKind);
    REQUIRE_FALSE((live_status(t.run()) == setup_link::kCurrent));
    REQUIRE(open_kinds(t.session().panes) ==
            std::vector<std::int64_t>{pane_kind::kLayouts});

    t.key(input::scan::kR);
    CHECK(t.session().setup.active.name == "Build only");
    CHECK(open_kinds(t.session().panes) ==
          std::vector<std::int64_t>{pane_kind::kLayouts, stock::kKind});
    CHECK((live_status(t.run()) == setup_link::kCurrent));
    CHECK(t.notice().find("restored setup \"Build only\"") == 0);
    CHECK(t.notice().find(t.host.setup_path) != std::string::npos);
}

TEST_CASE("a malformed setup file is refused without closing a single pane") {
    TempDir dir("setup-refuse-live");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    ToolSeat* tool = mount_tool(t, "zengine-snake");

    open_stock_pane(t);
    name_setup(t, "Good");
    const Setup saved = t.session().setup.active;
    const std::vector<std::int64_t> panes_before = open_kinds(t.session().panes);
    const std::int64_t asked_before = tool->described;

    // A file whose LAST field is the broken one, so a loader that acted as it
    // read would already have closed something by the time it noticed.
    spillout(t.host.setup_path,
             forged_setup(saved, "\"pane\":\"stack\"", "\"pane\":\"two words\""));
    t.key(input::scan::kR);

    CHECK(t.session().notice_is_bad);
    CHECK(t.session().setup.active == saved);
    CHECK(open_kinds(t.session().panes) == panes_before);
    // NOTHING WAS ASKED OF ANYBODY either: a refused restore is not a reason to
    // send a message on behalf of a pane that did not open.
    CHECK(tool->described == asked_before);
}

// ---- Lifecycle: what a restore opens and closes ----------------------------------
// What a restore does to a LOADED pane that holds a copy of somebody else's facts is asked
// where that pane lives; here, a built-in that holds none.

TEST_CASE("Info opens and closes through a restore, asking nobody anything") {
    TempDir dir("setup-life-info");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    ToolSeat* tool = mount_tool(t, "zengine-snake");

    REQUIRE(t.session().panes.has(pane_kind::kLayouts));

    // OPEN BEFORE, CLOSED AFTER: a pane holds no copy of anything -- what it presents outlives
    // it and belongs to somebody else.
    Setup nothing;
    nothing.name = "Nothing";
    REQUIRE(setup_persist::save_file(t.host.setup_path, nothing).accepted);
    t.key(input::scan::kR);
    CHECK(t.session().panes.open.empty());
    CHECK(tool->described == 0);

    // CLOSED BEFORE, OPEN AFTER: it opens, and it asks nobody. A Workshop
    // hosting no tools at all does this and it works.
    REQUIRE(setup_persist::save_file(t.host.setup_path, default_setup()).accepted);
    t.key(input::scan::kR);
    CHECK(t.session().panes.has(pane_kind::kLayouts));
    CHECK(tool->described == 0);
}

// ---- The unresolved reference, end to end ------------------------------------------

TEST_CASE("a setup naming a pane this build has never heard of loads, keeps it, and says so") {
    // A PRIMARY ACCEPTANCE PROOF, not a future-only unit test: the branch is written and
    // exercised while no file this build can produce reaches it.
    TempDir dir("setup-unresolved");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    ToolSeat* tool = mount_tool(t, "zengine-snake");

    Setup authored;
    authored.name = "Future";
    REQUIRE(add_pane(authored, info_ref()));
    REQUIRE(add_pane(authored, stranger()));
    REQUIRE(add_pane(authored, ref_of(stock::kKind)));
    // ...AND THE LAYOUTS PANE, because the row this case reads the count off lives in it. A
    // desk that does not name it has no identity row to count onto -- itself the honest
    // answer, and the case below this one.
    REQUIRE(add_pane(authored, ref_of(pane_kind::kLayouts)));
    const std::string bytes = setup_persist::to_text(authored);
    spillout(t.host.setup_path, bytes);

    t.key(input::scan::kR);

    // IT LOADED. An unresolved reference is not a load failure.
    CHECK_FALSE(t.session().notice_is_bad);
    CHECK(t.session().setup.active == authored);
    // THE UNKNOWN REFERENCE IS STILL THERE, still between the two that resolved,
    // still byte-for-byte what it was.
    REQUIRE(t.session().setup.active.panes.size() == 4);
    CHECK(t.session().setup.active.panes[1].ref == stranger());

    // The three that resolve are open, in file order relative to each other.
    CHECK(open_kinds(t.session().panes) ==
          std::vector<std::int64_t>{stock::kKind, pane_kind::kLayouts});

    // AND NOTHING WAS PAINTED ON THE UNKNOWN REFERENCE'S BEHALF. The only kind
    // available to paint an unknown pane with is the Builder, and there is
    // exactly one Builder, in the stack's FIRST slot -- the second slot, where a
    // placeholder would have gone, is empty.
    const Screen sc = screen_of(t.session());
    const PaneBounds builder_at = bounds_of(t.session().panes, t.session().setup.active, stock::kKind, sc);
    REQUIRE(builder_at.open);
    CHECK(builder_at.rect == placement_bounds(placement::kOverlayStack, 0, sc));
    CHECK(t.session().panes.open.size() == 2);
    // ...and the slot a placeholder would have taken is not occupied by anything:
    // a hand reaching into it meets the workspace, not a pane painted on behalf
    // of a reference nothing could resolve.
    const CellRect second = cells_covered(placement_bounds(placement::kOverlayStack, 1, sc));
    CHECK_FALSE(occupied_at(t.session().panes, t.session().setup.active, sc, second.x + 1, second.y + 1).occupied);

    // The notice says UNRESOLVED and names the reference, and never says
    // unavailable -- Workshop knows it has no catalog row for this, and knows
    // nothing whatever about whoever could present it.
    CHECK(t.notice().find("unresolved") != std::string::npos);
    // ...AND IT NAMES THE FIRST OF THEM. The shipped desk names the Info pane, which no provider
    // in this rig has offered, so this desk holds two unresolved rows and the notice is bounded
    // -- it says the count and names as many as it has room for, in the setup's own order.
    CHECK((t.notice().find("third.party.tools/history") != std::string::npos ||
           t.notice().find("zengine.info/info") != std::string::npos));
    CHECK(t.notice().find("unavailable") == std::string::npos);

    // The setup LINE says it too, as a count beside the association: two, the stranger this
    // case authored and the Info row the shipped desk names, which no provider here offered.
    CHECK(band_status(t.session(), sc).text.find("2 unresolved") != std::string::npos);

    // NO PROVIDER TRAFFIC WAS CREATED AT ALL: no built-in asks a participant anything when it
    // opens, and nothing was sent on the unknown reference's behalf either.
    CHECK(tool->described == 0);

    // AND RE-SAVING RETAINS IT EXACTLY. The weaver renames the setup and saves;
    // the stranger's entry comes through untouched.
    name_setup(t, "Future kept");
    Setup expected = authored;
    expected.name = "Future kept";
    CHECK(t.session().setup.active == expected);
    CHECK(slurp(t.host.setup_path) == setup_persist::to_text(expected));
    CHECK(slurp(t.host.setup_path).find("third.party.tools") != std::string::npos);

    // ...and it survives a door's gesture on either pane.
    pick(t, stock::kKind);
    CHECK(has_pane(t.session().setup.active, stranger()));
    pick(t, pane_kind::kLayouts);
    CHECK(has_pane(t.session().setup.active, stranger()));
}

// ---- Authored versus resolved -------------------------------------------------------

TEST_CASE("the same setup resolves to different bounds under a different extent") {
    // THE AUTHORED/RESOLVED PROOF, and its GREEN CONTROL in one case: change only the surface
    // extent and the setup stays SAVED while every rectangle it resolves to moves.
    TempDir dir("setup-extent");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    (void)mount_tool(t, "zengine-snake");

    // TWO PANES, BOTH OPENED THE WAY A WEAVER OPENS THEM, ON A SCREEN THAT CAN SEAT BOTH.
    // A fresh desk carries one pane this rig can present, so the second is picked rather
    // than inherited -- the desk's Info row is a weave nothing has offered here, and an
    // unresolved row has no rectangle to move. Both are overlay panes, so the smaller of
    // the two extents this case compares has to hold two stack slots or the launch door
    // refuses the second for room and the case would be measuring one pane twice.
    t.extent_px(cells_px(120), cells_px(44), 0, 0);
    open_stock_pane(t);
    open_pane(t, ref_of(second::kKind));
    REQUIRE(t.session().panes.has(second::kKind));
    name_setup(t, "Both");
    REQUIRE((live_status(t.run()) == setup_link::kCurrent));
    const std::string bytes = slurp(t.host.setup_path);
    const Setup authored = t.session().setup.active;

    const Screen small = screen_of(t.session());
    const CellRect info_small =
cells_covered(bounds_of(t.session().panes, t.session().setup.active, second::kKind, small).rect);
    const CellRect builder_small =
cells_covered(bounds_of(t.session().panes, t.session().setup.active, stock::kKind, small).rect);

    t.extent_px(cells_px(140), cells_px(44), 0, 0);

    const Screen large = screen_of(t.session());
    const CellRect info_large =
cells_covered(bounds_of(t.session().panes, t.session().setup.active, second::kKind, large).rect);
    const CellRect builder_large =
cells_covered(bounds_of(t.session().panes, t.session().setup.active, stock::kKind, large).rect);

    // THE RESOLVED GEOMETRY MOVED...
    CHECK(large.w != cells_of(small).w);
    CHECK(builder_large.w != builder_small.w);
    CHECK(builder_large.h == builder_small.h); // the stack's slot is a fixed size...
    CHECK(large.room_w != cells_of(small).room_w);       // ...and the room around it is not

    // ...AND NOTHING AUTHORED DID. Same value, same references, same bytes, and
    // still saved -- which is the control that makes this a claim about
    // persisted geometry rather than about recomposition.
    CHECK(t.session().setup.active == authored);
    CHECK((live_status(t.run()) == setup_link::kCurrent));
    CHECK(slurp(t.host.setup_path) == bytes);

    // Restoring under the new extent produces the same intent and the current
    // rectangles, never the ones the file was written under.
    t.key(input::scan::kR);
    CHECK(t.session().setup.active == authored);
    CHECK(cells_covered(
              bounds_of(t.session().panes, t.session().setup.active, second::kKind, large)
                  .rect) == info_large);

    // A text metric moves the same picture again, and the setup is untouched.
    t.extent_px(cells_px(140), cells_px(44), 8, 18);
    CHECK(t.session().setup.active == authored);
    CHECK((live_status(t.run()) == setup_link::kCurrent));
    CHECK(slurp(t.host.setup_path) == bytes);

    // And no RESOLVED rectangle, placement, column, row or metric is in the file at all.
    for (const char* forbidden : {"\"w\"", "\"h\"", "rect", "columns", "rows",
                                  "advance", "extent", "placement", "slot"}) {
        CAPTURE(forbidden);
        CHECK(bytes.find(forbidden) == std::string::npos);
    }
    // THE FILE CARRIES NO PLACE NOBODY AUTHORED, and the difference between an authored
    // coordinate and a resolved one is the whole of this case. Every row here is `default` in
    // all three geometry fields, the smallest canonical spelling of "no override", and the word
    // reads no number -- so none stands beside it.
    CHECK(bytes.find("\"place\":{\"mode\":\"default\"}") != std::string::npos);
    CHECK(bytes.find("\"width\":{\"mode\":\"default\"}") != std::string::npos);
    // ...and none of the WIDTHS this case just measured is anywhere in it. (The heights are
    // not asked: both panes are overlay panes, `kStackRows` is the same at every extent, and a number that small
    // is one a sparse file legitimately holds in a rank.)
    for (const std::int64_t n : {builder_small.w, builder_large.w, info_small.w, info_large.w}) {
        CAPTURE(n);
        CHECK(bytes.find("\"amount\":\"" + std::to_string(n) + "\"") == std::string::npos);
    }
}

// ---- Two processes ------------------------------------------------------------------

TEST_CASE("a weaver names a setup, leaves, and gets it back in a fresh Workshop") {
    // THE PRODUCT OUTCOME, deterministically: two independent Workshops, two
    // independent buses, one file between them.
    TempDir dir("setup-two-runs");
    const std::string path = dir.file("setup.json");

    std::string bytes;
    {
        // RUN A: a fresh Workshop opens with the Layouts pane; the weaver opens the stand-in,
        // names the setup and saves.
        Live a;
        a.host.setup_path = path;
        (void)mount_tool(a, "zengine-snake");
        REQUIRE(open_kinds(a.session().panes) ==
                std::vector<std::int64_t>{pane_kind::kLayouts});

        open_stock_pane(a);
        name_setup(a, "Morning build");

        REQUIRE((live_status(a.run()) == setup_link::kCurrent));
        REQUIRE(open_kinds(a.session().panes) ==
                std::vector<std::int64_t>{pane_kind::kLayouts, stock::kKind});
        bytes = slurp(path);
        REQUIRE_FALSE(bytes.empty());
    }

    {
        // RUN B: a fresh Workshop begins from its ordinary default, and the
        // weaver restores.
        Live b;
        b.host.setup_path = path;
        ToolSeat* tool = mount_tool(b, "zengine-snake");
        REQUIRE(b.session().setup.active == default_setup());
        REQUIRE(open_kinds(b.session().panes) ==
                std::vector<std::int64_t>{pane_kind::kLayouts});
        const std::int64_t opening_room = screen_of(b.session()).room_w;

        b.key(input::scan::kR);

        CHECK(b.session().setup.active.name == "Morning build");
        CHECK(b.session().panes.has(stock::kKind));
        CHECK_FALSE(b.session().panes.has(second::kKind));
        CHECK((live_status(b.run()) == setup_link::kCurrent));

        // NO COPY OF ANYBODY ELSE'S FACTS RODE THE FILE, which is what the tool's own
        // counter still says: the setup carries panes and places, and a participant's state
        // is that participant's.
        CHECK(tool->described == 0);

        // AND THE CURRENT SCREEN EXTENT DID NOT COME OUT OF THE SETUP: it is what this run had
        // before the restore.
        CHECK(screen_of(b.session()).room_w == opening_room);

        // The file is unchanged by having been read.
        CHECK(slurp(path) == bytes);
    }
}

// ---- What a weaver reads --------------------------------------------------------------

TEST_CASE("the top row says the ACTIVE layout's Setup association") {
    TempDir dir("setup-line");
    Live t;
    t.host.setup_path = dir.file("s.json");

    const Screen sc = screen_of(t.session());
    const std::string fresh = setup_row(first_frame(t), sc);
    INFO(fresh);
    CHECK(fresh.find(">Default<") == 0); // the live layout tab leads the row
    // `none` MEANS "RELATED TO NO ARTIFACT", NOT "UNSAVED". The session remembers this
    // desk automatically; what it has not got is an explicit standalone Setup file.
    CHECK(fresh.find("setup: none") != std::string::npos);
    CHECK(fresh.find("UNSAVED") == std::string::npos);
    // ...AND THE HOST'S CONFIGURED PATH IS NOT SHOWN, because no layout is related to it
    // yet. It is the acquisition door, never a default association.
    CHECK(fresh.find("s.json") == std::string::npos);

    // A SUCCESSFUL SAVE ESTABLISHES THE ASSOCIATION, and the row says the artifact and
    // the verdict.
    save_setup(t);
    const std::string saved = setup_row(t.shown(), sc);
    INFO(saved);
    CHECK(saved.find(">Default<") == 0);
    CHECK(saved.find("setup: ") != std::string::npos);
    CHECK(saved.find("| current") != std::string::npos);

    // ...AND MUTATING THE DESK MAKES IT `modified`, DERIVED rather than flagged.
    pick(t, stock::kKind);
    const std::string moved = setup_row(t.shown(), sc);
    INFO(moved);
    CHECK(moved.find("| modified") != std::string::npos);
    CHECK(moved.find("| current") == std::string::npos);

    // AT THE MINIMUM COMPOSITION WITH THE DEFAULT FILE NAME THE WHOLE LINE FITS, and that
    // is the measurement the ORDER of that line was chosen against.
    Live plain;
    plain.host.setup_path = kDefaultSetupFileName;
    const std::string minimal = setup_row(first_frame(plain), screen_of(plain.session()));
    INFO(minimal);
    CHECK(minimal.find(">Default<") == 0);
    CHECK(minimal.find("setup: none") != std::string::npos);
    CHECK(minimal.find("s save") != std::string::npos);
    CHECK(minimal.find("r restore") != std::string::npos);
    CHECK(static_cast<std::int64_t>(minimal.size()) <= cells_of(kMinScreen).w);

    // AND A PATH TOO LONG FOR THE ROOM IS ELIDED BEFORE THE VERDICT IS.
    Live wordy;
    wordy.host.setup_path = std::string(90, 'p');
    (void)first_frame(wordy);
    save_setup(wordy);
    const std::string cut = setup_row(wordy.shown(), screen_of(wordy.session()));
    INFO(cut);
    CHECK(static_cast<std::int64_t>(cut.size()) <= cells_of(kMinScreen).w);
    CHECK(cut.find(">Default<") == 0);
    CHECK(cut.find("setup: ") != std::string::npos);
    CHECK(cut.find("| current") != std::string::npos);
    CHECK(cut.find("...") != std::string::npos); // the cut is marked, never silent

    // A wider surface spends the room it gained on the path itself.
    wordy.extent_px(cells_px(200), cells_px(40), 0, 0);
    const std::string roomy = setup_row(wordy.shown(), screen_of(wordy.session()));
    INFO(roomy);
    CHECK(roomy.find(std::string(90, 'p')) != std::string::npos);
    CHECK(roomy.find("r restore") != std::string::npos);
}

TEST_CASE("the setup line becomes the name editor while a weaver is typing") {
    TempDir dir("setup-editor-line");
    Live t;
    t.host.setup_path = dir.file("s.json");

    open_rename_on_tab(t, t.session().setup.active_at);
    const Screen sc = screen_of(t.session());
    const std::string row = setup_row(t.shown(), sc);
    INFO(row);
    CHECK(row.find("layout name> Default") == 0);
    // THE CARET STANDS AFTER THE NAME, where `s` left it, on the blank past its last letter. A
    // one-cell-tall bounded region would hold zero rows of a real face, so this editor shows its
    // caret the way the cell projection shows a region's, moving no character.
    CHECK(caret_at(paint(t.session()), 0, 0) ==
          static_cast<std::int64_t>(row.find("Default") + std::string("Default").size()));
    CHECK(row.find("return renames") != std::string::npos);
    CHECK(row.find("escape cancels") != std::string::npos);
    CHECK(static_cast<std::int64_t>(row.size()) <= cells_of(sc).w);

    // The caret follows the weaver's hand, and a character typed at it lands there.
    t.key(input::scan::kLeft);
    t.key(input::scan::kLeft);
    t.text("X");
    CHECK(t.session().setup.naming.line.text() == "DefauXlt");
    const std::string typed = setup_row(t.shown(), sc);
    // (The window draws the caret as a bar over the letter it stands before.)
    CHECK(caret_at(paint(t.session()), 0, 0) ==
          static_cast<std::int64_t>(typed.find("DefauX") + std::string("DefauX").size()));
}

TEST_CASE("the name editor takes the keys, and the contextual surface keeps its own") {
    TempDir dir("setup-modes");
    Live t;
    t.host.setup_path = dir.file("s.json");

    open_rename_on_tab(t, t.session().setup.active_at);
    REQUIRE(t.session().setup.naming.open);

    // `a` DOES NOT OPEN THE CONTEXTUAL SURFACE while the editor has the keys: it is a
    // character, and the editor is the mode above command mode.
    t.key(input::scan::kA);
    t.text("a");
    CHECK_FALSE(t.menu().open);
    CHECK(t.session().setup.naming.line.text() == "Defaulta");

    // ...and `q` does not quit.
    t.text("q");
    CHECK_FALSE(t.host.quit);
    CHECK(t.session().setup.naming.line.text() == "Defaultaq");

    // `^s` IS NOBODY'S: a chord no row answers is not a character either, so the line keeps
    // its text and nothing is said.
    const std::string notice_before = t.notice();
    t.key(input::scan::kS, input::mod::kCtrl);
    CHECK(t.notice() == notice_before);
    CHECK(t.session().setup.naming.line.text() == "Defaultaq");

    // The surface and the name editor cannot both be open: `s` is a command, and
    // the surface takes the keys before command mode is reached.
    t.key(input::scan::kEscape);
    REQUIRE_FALSE(t.session().setup.naming.open);
    t.key(input::scan::kA);
    REQUIRE(t.menu().open);
    t.key(input::scan::kS);
    t.text("s");
    CHECK_FALSE(t.session().setup.naming.open);
    CHECK(t.menu().open);
}

TEST_CASE("the contextual surface's own state never reaches the setup file") {
    TempDir dir("setup-no-menu");
    Live t;
    t.host.setup_path = dir.file("s.json");

    // Leave the surface's cursor somewhere deliberate, then save.
    t.key(input::scan::kA);
    t.key(input::scan::kDown);
    REQUIRE(t.menu().cursor == 1);
    t.key(input::scan::kEscape);
    name_setup(t, "Clean");

    const std::string bytes = slurp(t.host.setup_path);
    CHECK(bytes.find("cursor") == std::string::npos);
    CHECK(bytes.find("menu") == std::string::npos);
    CHECK(bytes.find("terminal") == std::string::npos);
    CHECK(bytes.find("selected") == std::string::npos);
    CHECK(bytes.find("notice") == std::string::npos);

    // And restoring opens no surface: `r` is a command, so the surface was closed
    // before it could run, and nothing in a restore opens one.
    t.key(input::scan::kR);
    CHECK_FALSE(t.menu().open);
}

// ---- the sentence that quotes a name owns the escaping ------------------------
// A setup name may hold `"` and `\`, persisted exactly; the PROSE quoting it must not let a
// legal name manufacture the delimiter a weaver tells an identity from its status by. These
// pin the spelling, its callers, the authored bytes on both sides, and the bounds' unit.

namespace {

/// READ ONE QUOTED TOKEN BACK THE WAY A WEAVER'S EYE DOES, written against the RULE rather
/// than against `quoted_setup_name`, so it is an independent second implementation: an
/// opening quote, bytes in which a backslash escapes what follows, and the first UNESCAPED
/// quote ends the name. With the token built by raw interpolation this recovers the wrong
/// name, which makes "the name cannot terminate its own token" measurable.
struct QuotedToken {
    bool well_formed = false;
    std::string name; ///< the bytes the token means
    std::size_t end = 0; ///< one past the token's closing quote
};

QuotedToken read_quoted(const std::string& line, std::size_t at) {
    QuotedToken t;
    if (at >= line.size() || line[at] != '"') {
        return t;
    }
    for (std::size_t i = at + 1; i < line.size(); ++i) {
        if (line[i] == '\\') {
            if (i + 1 >= line.size()) {
                return t; // a trailing escape is not a finished token
            }
            t.name += line[++i];
            continue;
        }
        if (line[i] == '"') {
            t.well_formed = true;
            t.end = i + 1;
            return t;
        }
        t.name += line[i];
    }
    return t;
}

/// A name of `n` repetitions of one byte -- the pathological shapes, said once.
std::string repeated(std::size_t n, char c) { return std::string(n, c); }

/// U+1F680, written as its four UTF-8 bytes rather than as a source character, so
/// nothing here depends on this file's execution encoding or on `char8_t`. Four bytes,
/// one code point, one character a weaver would count.
constexpr const char* kFourByteChar = "\xF0\x9F\x9A\x80";

std::string four_byte_chars(std::size_t count) {
    std::string out;
    for (std::size_t i = 0; i < count; ++i) {
        out += kFourByteChar;
    }
    return out;
}

} // namespace

TEST_CASE("a setup name is spelled into prose as one unambiguous quoted token") {
    // ORDINARY NAMES ARE UNCHANGED, exactly -- the control the whole escaping is measured
    // against: the sentence a weaver reads for an ordinary name comes back byte-for-byte.
    CHECK(quoted_setup_name("Default") == "\"Default\"");
    CHECK(quoted_setup_name("Morning build") == "\"Morning build\"");
    CHECK(quoted_setup_name(repeated(kMaxSetupNameLen, 'n')) ==
          "\"" + repeated(kMaxSetupNameLen, 'n') + "\"");

    // A QUOTE CANNOT TERMINATE THE TOKEN. The exact output, because a case that only
    // looked for `\"` somewhere in the string would pass with a second, unescaped quote
    // still sitting further along it.
    CHECK(quoted_setup_name("Ops\" UNSAVED | decoy") == "\"Ops\\\" UNSAVED | decoy\"");

    // A BACKSLASH CANNOT DISGUISE WHETHER THE QUOTE AFTER IT WAS AUTHORED.
    CHECK(quoted_setup_name("A\\B") == "\"A\\\\B\"");
    CHECK(quoted_setup_name("A\\\"B") == "\"A\\\\\\\"B\"");

    // TOTAL, including on input no valid `Setup` can hold: the law refuses an empty
    // name, so this is a helper's boundary rather than a reachable state, and a
    // presentation spelling that had an opinion about which inputs it would answer for
    // would be a second law in a second place.
    CHECK(quoted_setup_name("") == "\"\"");
    CHECK_FALSE(check_setup_name("").accepted);

    // THE SUBSTITUTION IS INJECTIVE, which is the property a lossy repair (rendering a
    // quote as an apostrophe) would give up: two names a weaver can tell apart must not
    // present as one.
    CHECK(quoted_setup_name("A\"B") != quoted_setup_name("A\\\"B"));

    // ...and the whole of it, stated once: applying the escape rule in reverse recovers
    // the authored bytes, for every shape here.
    const std::vector<std::string> names = {
        "Default",       "Morning build", "Ops\" UNSAVED | decoy", "A\\B",
        "A\\\"B",        "\"",            "\\",                    "\\\\\"\"",
        "quote\"in\"it", repeated(kMaxSetupNameLen, '"'),
    };
    for (const std::string& authored : names) {
        CAPTURE(authored);
        const QuotedToken read = read_quoted(quoted_setup_name(authored), 0);
        CHECK(read.well_formed);
        CHECK(read.name == authored);
        CHECK(read.end == quoted_setup_name(authored).size());

        // Raw interpolation, asked the same question: a token a reader recovers the WRONG
        // name from the moment the name carries a quote.
        const QuotedToken naive = read_quoted("\"" + authored + "\"", 0);
        if (authored.find('"') != std::string::npos ||
            authored.find('\\') != std::string::npos) {
            CHECK((!naive.well_formed || naive.name != authored));
        }
    }
}

TEST_CASE("a name that could impersonate the setup line is one SPAN on it") {
    TempDir dir("ws0a-status");
    Live t;
    t.host.setup_path = dir.file("s.json");

    // A NAME BUILT TO LIE. Every byte of it is legal under the name law and stays legal:
    // this case is about the sentence, not about the name.
    const std::string authored = "Ops\" UNSAVED | decoy";
    REQUIRE(check_setup_name(authored).accepted);
    name_setup(t, authored);
    REQUIRE(t.session().setup.active.name == authored);
    REQUIRE((live_status(t.run()) == setup_link::kCurrent));

    const Screen sc = screen_of(t.session());
    const std::string row = setup_row(t.shown(), sc);
    INFO(row);

    // THE LAYOUT TABS PAINT THE AUTHORED BYTES BARE, so the identity's boundary is not a
    // delimiter IN the text -- it is the tab's own recorded extent, written as the row was
    // composed (`LayoutTab::column`/`columns`, one geometry for paint and press).
    const BandStatus band = band_status(t.session(), sc);
    REQUIRE(band.tabs.size() == 1);
    const LayoutTab live = band.tabs.front();
    const std::int64_t ends = live.column + live.columns;

    // THE IDENTITY IS ONE SPAN, and the weaver's own bytes are exactly what is inside it,
    // one cell in from each marker. Nothing was escaped and nothing was substituted.
    CHECK(live.column == 0);
    CHECK(band.text.substr(static_cast<std::size_t>(live.column),
                           static_cast<std::size_t>(live.columns)) == ">" + authored + "<");
    CHECK(band.text.substr(static_cast<std::size_t>(live.column) + 1,
                           static_cast<std::size_t>(live.columns) - 2) == authored);
    // NO ESCAPE REACHED THE TAB. Asked of the SPAN and not of the row: the row also
    // carries the setup file's path, and on Windows its separators are backslashes.
    CHECK(band.text.substr(static_cast<std::size_t>(live.column),
                           static_cast<std::size_t>(live.columns))
              .find('\\') == std::string::npos);

    // ...AND EXACTLY ONE `setup:` SLOT, the real one, OUTSIDE the span. The decoy words
    // inside the name are not it, and the proof is positional: the status begins after the
    // identity's extent ends.
    CHECK(band.text.find("setup: ") > static_cast<std::size_t>(ends));
    CHECK(band.text.find("UNSAVED", static_cast<std::size_t>(ends)) == std::string::npos);

    // ⚠ AND THE HALF A BARE RUN GIVES UP, PINNED RATHER THAN LEFT TO BE DISCOVERED. To a
    // reader scanning the BYTES alone the decoy is indistinguishable from a status word: it
    // is on the row, ahead of the real one, and only the span says it is part of a name. That
    // is the weaver's decision, not a defect -- what may never happen is the MACHINE losing
    // the boundary.
    CHECK(band.text.find("UNSAVED") < static_cast<std::size_t>(ends));
    CHECK(band.text.find(" | ") < static_cast<std::size_t>(ends));
    CHECK(band.text.find("setup: ") > static_cast<std::size_t>(ends));

    // The row is still one bounded row of the band, and the file is still named on it.
    CHECK(static_cast<std::int64_t>(row.size()) <= cells_of(sc).w);
    CHECK(row.compare(0, band.text.size(), band.text) == 0);

    // AND THE AUTHORED BYTES NEVER MOVED. The presentation is prose and reaches neither
    // the live setup nor the copy `saved()` compares against; the FILE still escapes.
    CHECK(t.session().setup.active.name == authored);
    CHECK(t.run().active_link.known.name == authored);
    CHECK(slurp(t.host.setup_path).find("\\\" UNSAVED") != std::string::npos);
}

TEST_CASE("the save notice and the restore notice spell the name the same way") {
    TempDir dir("ws0a-notices");
    Live t;
    t.host.setup_path = dir.file("s.json");

    const std::string authored = "Ops \"A\\B\"";
    REQUIRE(check_setup_name(authored).accepted);

    name_setup(t, authored);
    const std::string saved = t.notice();
    INFO(saved);
    REQUIRE(saved.compare(0, 12, "saved setup ") == 0);
    const QuotedToken after_save = read_quoted(saved, 12);
    REQUIRE(after_save.well_formed);
    CHECK(after_save.name == authored);
    // The PATH is named the ordinary way -- it is not a setup name and gains no escaping.
    CHECK(saved.compare(after_save.end, 4, " to ") == 0);
    CHECK(saved.find(t.host.setup_path) != std::string::npos);

    t.key(input::scan::kR);
    const std::string restored = t.notice();
    INFO(restored);
    REQUIRE(restored.compare(0, 15, "restored setup ") == 0);
    const QuotedToken after_restore = read_quoted(restored, 15);
    REQUIRE(after_restore.well_formed);
    // NOT REINTERPRETED AND NOT NORMALISED on the way back: the name that comes out of
    // the file is the name that went into it, and the notice says those bytes.
    CHECK(after_restore.name == authored);
    CHECK(restored.compare(after_restore.end, 6, " from ") == 0);
    CHECK(t.session().setup.active.name == authored);

    // ONE OWNER, PROVEN BY AGREEMENT: both notices carry the identical token, so a caller
    // that resumed improvising its own would be named here rather than only in whichever
    // case happened to cover it.
    const std::string token = quoted_setup_name(authored);
    CHECK(saved.find(token) == 12);
    CHECK(restored.find(token) == 15);

    // ⚠ AND THE TAB RUN IS NOT ONE OF THEM: it paints the AUTHORED bytes, so the escaped
    // spelling is nowhere on the row -- a consumer that deliberately does not use this owner,
    // not a caller improvising a second spelling of it.
    const std::string row = setup_row(t.shown(), screen_of(t.session()));
    INFO(row);
    CHECK(row.find(token) == std::string::npos);
    CHECK(row.compare(0, authored.size() + 2, ">" + authored + "<") == 0);
}

TEST_CASE("the name editor edits the authored bytes, never the escaped spelling") {
    TempDir dir("ws0a-editor");
    Live t;
    t.host.setup_path = dir.file("s.json");

    // TYPED FROM SCRATCH, character by character, exactly as a weaver produces it: the
    // quote and the backslash arrive as ordinary text and are stored as themselves.
    const std::string authored = "Ops \"A\\B\"";
    name_setup(t, authored);
    REQUIRE(t.session().setup.active.name == authored);

    // REOPENED ON THE NAME IT ALREADY HAS -- and what the editor holds is the ORIGINAL
    // bytes. A weaver does not have to type `\"` to mean `"` in their own name.
    open_rename_on_tab(t, t.session().setup.active_at);
    REQUIRE(t.session().setup.naming.open);
    CHECK(t.session().setup.naming.line.text() == authored);
    CHECK(t.session().setup.naming.line.text()[6] == '\\'); // the raw backslash, stored as one

    const Screen sc = screen_of(t.session());
    const std::string row = setup_row(t.shown(), sc);
    INFO(row);
    // The editing row is not a quoted sentence, so it is not an escaped one either: the
    // prompt, the raw name, the caret where the weaver's hand left it, and the hint.
    CHECK(row.find(std::string("layout name> ") + authored) == 0);
    CHECK(caret_at(paint(t.session()), 0, 0) ==
          static_cast<std::int64_t>(std::string("layout name> ").size() + authored.size()));
    CHECK(row.find(quoted_setup_name(authored)) == std::string::npos);

    // ESCAPE CHANGES NOTHING, and the name is still the authored one.
    t.key(input::scan::kEscape);
    CHECK_FALSE(t.session().setup.naming.open);
    CHECK(t.session().setup.active.name == authored);
    CHECK((live_status(t.run()) == setup_link::kCurrent));

    // AND ENTER RENAMES TO THE AUTHORED BYTES, not to the spelling they are presented with
    // -- then `s` writes exactly those bytes: two gestures, one value.
    open_rename_on_tab(t, t.session().setup.active_at);
    t.text("!");
    t.key(input::scan::kReturn);
    const std::string grown = authored + "!";
    CHECK(t.session().setup.active.name == grown);
    save_setup(t);
    const setup_persist::LoadedSetup back = setup_persist::load_file(t.host.setup_path);
    REQUIRE(back.outcome.accepted);
    CHECK(back.setup.name == grown);
    CHECK(back.setup.name.find("\\\"") == std::string::npos); // no escape was stored
}

TEST_CASE("an ordinary setup name presents exactly as it did before") {
    // THE GREEN CONTROL FOR THE ESCAPING. Not one byte of the sentence a weaver without a
    // quote in their name reads may move, and every string here is spelled out rather than
    // composed, so a change to the presentation owner cannot quietly agree with itself.
    TempDir dir("ws0a-ordinary");
    Live t;
    t.host.setup_path = dir.file("s.json");

    const Screen sc = screen_of(t.session());
    const std::string fresh = setup_row(first_frame(t), sc);
    CHECK(fresh.find(">Default<") == 0);
    CHECK(fresh.find("setup: none") != std::string::npos);

    name_setup(t, "Morning build");
    CHECK(t.notice().find("saved setup \"Morning build\" to ") == 0);
    CHECK(setup_row(t.shown(), sc).find(">Morning build<") == 0);
    CHECK(setup_row(t.shown(), sc).find("| current") != std::string::npos);

    t.key(input::scan::kR);
    CHECK(t.notice().find("restored setup \"Morning build\" from ") == 0);

    // ...and the name a fresh Workshop carries is spelled the way the constant is.
    CHECK(quoted_setup_name(kDefaultSetupName) == "\"Default\"");
}

TEST_CASE("a bare name at the bound is its own length, and the row is still cut") {
    TempDir dir("ws0a-fit");

    // THE BARE RUN SPENDS THE AUTHORED BYTES AND NOTHING ELSE, so twelve quotes are twelve
    // cells, not twenty-four as escaped -- the control that keeps the case below about the
    // BOUND rather than about escaping.
    Live modest;
    modest.host.setup_path = dir.file("m.json");
    name_setup(modest, repeated(12, '"'));
    const std::string easy = setup_row(modest.shown(), screen_of(modest.session()));
    INFO(easy);
    CHECK(easy.compare(0, 14, ">" + repeated(12, '"') + "<") == 0);
    CHECK(easy.find("\\\"") == std::string::npos); // not one escape on the row
    CHECK(easy.find("| current") != std::string::npos);

    // THE PATHOLOGICAL LEGAL NAME: thirty-two bytes at the bound, every one a quote --
    // thirty-four cells of a seventy-eight cell row. It is the file path that carries the
    // row past its extent.
    Live t;
    t.host.setup_path = dir.file("p.json");
    const std::string authored = repeated(kMaxSetupNameLen, '"');
    REQUIRE(check_setup_name(authored).accepted);
    name_setup(t, authored);
    REQUIRE(t.session().setup.active.name == authored);
    REQUIRE((live_status(t.run()) == setup_link::kCurrent));

    const std::string cut = setup_row(t.shown(), screen_of(t.session()));
    INFO(cut);
    CHECK(static_cast<std::int64_t>(cut.size()) == cells_of(kMinScreen).w);
    CHECK(cut.substr(cut.size() - 3) == "...");
    // IT CANNOT RUN UNMARKED INTO WHAT COMES AFTER IT. The existing `detail::fit` is
    // the whole of the answer -- the sentence is fitted once, at the presentation
    // boundary, after the token is formed -- so nothing downstream of the identity is
    // shown as though it were complete.
    CHECK(cut.find("r restore") == std::string::npos);
    CHECK(cut.find(t.host.setup_path) == std::string::npos);

    // THE CUT IS PRESENTATION AND NOTHING ELSE: the name, the saved comparison and the
    // file are all untouched by it, and a wider surface spends the room on the rest of
    // the same sentence.
    CHECK(t.session().setup.active.name == authored);
    CHECK((live_status(t.run()) == setup_link::kCurrent));
    CHECK(setup_persist::load_file(t.host.setup_path).setup.name == authored);

    t.extent_px(cells_px(240), cells_px(40), 0, 0);
    const std::string roomy = setup_row(t.shown(), screen_of(t.session()));
    INFO(roomy);
    CHECK(roomy.find("...") == std::string::npos);
    CHECK(roomy.compare(0, authored.size() + 2, ">" + authored + "<") == 0);
    CHECK(roomy.find("s save") != std::string::npos);
    // ...and the extent changed what fits, never the setup or its saved status.
    CHECK(t.session().setup.active.name == authored);
    CHECK((live_status(t.run()) == setup_link::kCurrent));
}

TEST_CASE("a name carrying a quote and a backslash survives its file exactly") {
    TempDir dir("ws0a-persist");
    const std::string authored = "Ops \"A\\B\"";
    const Setup s = setup_of(authored, {second::kKind, stock::kKind});
    REQUIRE(check_setup(s).accepted);

    // THE FORMAT WORD IS UNCHANGED AND THE VERSION IS NOT: an authored name carrying the two
    // bytes the quoting owner escapes comes back exactly as it went in, whatever the version,
    // and handing Workshop the wrong one of its own two files is still named, not half-read.
    CHECK(setup_persist::kFormatVersion == 6);
    CHECK(std::string(setup_persist::kFormat) == "zengine-workshop-setup");

    const std::string path = dir.file("q.json");
    REQUIRE(setup_persist::save_file(path, s).accepted);
    const std::string a = slurp(path);
    INFO(a);
    CHECK(a.find("\"format\":\"zengine-workshop-setup\"") != std::string::npos);
    CHECK(a.find("\"format_version\":\"6\"") != std::string::npos);

    const setup_persist::LoadedSetup read = setup_persist::load_file(path);
    REQUIRE(read.outcome.accepted);
    // EXACT AUTHORED BYTES, and not the spelling any sentence presents them with.
    CHECK(read.setup.name == authored);
    CHECK(read.setup == s);

    REQUIRE(setup_persist::save_file(path, read.setup).accepted);
    const std::string b = slurp(path);
    CHECK(a == b); // save -> load -> save, byte-identical, with a quote in the name

    // THE COMPAT CODEC OWNS THE FILE'S OWN ESCAPING and always did; nothing here
    // hand-authored a second one, and the escaped PROSE spelling was never stored.
    CHECK(a.find("Ops \\\"A\\\\B\\\"") != std::string::npos);
    CHECK(quoted_setup_name(read.setup.name) == "\"Ops \\\"A\\\\B\\\"\"");
}

TEST_CASE("the name and key bounds are BYTES, and the refusal says bytes") {
    // THE BOUNDS THEMSELVES: the refusal's sentence names the unit, and nothing about what is
    // accepted moves.
    CHECK(kMaxSetupNameLen == 32);
    CHECK(kMaxPaneKeyLen == 64);

    // EIGHT FOUR-BYTE CHARACTERS ARE THIRTY-TWO BYTES -- eight characters a weaver
    // counts, and exactly the bound `std::string::size()` measures.
    const std::string at_bound = four_byte_chars(8);
    REQUIRE(at_bound.size() == kMaxSetupNameLen);
    CHECK(check_setup_name(at_bound).accepted);

    // ...AND ONE MORE BYTE IS REFUSED, saying the unit it was refused in.
    const Written over = check_setup_name(at_bound + "x");
    CHECK_FALSE(over.accepted);
    CHECK(over.refusal == "a setup name is at most 32 bytes");

    // THE SHARPEST ILLUSTRATION OF THE UNIT: nine characters, thirty-six bytes, refused -- so
    // a refusal saying "at most 32 characters" would be a false sentence about a true refusal.
    const std::string nine = four_byte_chars(9);
    CHECK(nine.size() == 36);
    CHECK_FALSE(check_setup_name(nine).accepted);
    CHECK(check_setup_name(nine).refusal == "a setup name is at most 32 bytes");

    // THE SAME QUESTION OF THE KEY BOUND, both halves of a reference.
    const std::string key_at_bound = four_byte_chars(16);
    REQUIRE(key_at_bound.size() == kMaxPaneKeyLen);
    CHECK(check_pane_key(key_at_bound, "provider").accepted);
    CHECK(check_pane_key(key_at_bound + "x", "provider").refusal ==
          "a pane reference's provider is at most 64 bytes");
    CHECK(check_pane_key(key_at_bound + "x", "pane key").refusal ==
          "a pane reference's pane key is at most 64 bytes");
    CHECK(check_pane_ref(PaneRef{"zengine.workshop", four_byte_chars(17)}).refusal ==
          "a pane reference's pane key is at most 64 bytes");

    // NO UNICODE POLICY WAS BOUGHT WITH THIS, and the absence is asserted rather than
    // merely intended. Workshop counts bytes and nothing else: it has no opinion about
    // how many code points, graphemes or CELLS a name occupies, and a thirty-third byte
    // is refused whatever a character count would have said about it.
    CHECK(check_setup_name(four_byte_chars(1)).accepted);   // 4 bytes, 1 character
    CHECK(check_setup_name(repeated(4, 'a')).accepted);     // 4 bytes, 4 characters
    CHECK_FALSE(check_setup_name(at_bound + " ").accepted); // 9 characters, 33 bytes
}

// =============================================================================
// GIVE ME MY DESK BACK
// =============================================================================
// Close Workshop after arranging a useful desk, reopen it, and get that desk back -- panes,
// places, sizes, front order and the surface's room -- with no gesture between. The SETUP
// file's own round trip is pinned above; this is the unasked read and the remembered room.

namespace {

/// A desk worth wanting back: two panes, one of them moved and resized by hand.
Setup arranged_desk(const char* name) {
    Setup s = setup_of(name, {second::kKind, stock::kKind});
    REQUIRE(author_pane_place(s, ref_of(stock::kKind), cells_px(6), cells_px(5)).accepted);
    REQUIRE(author_pane_size(s, ref_of(stock::kKind), PaneSize{pane_unit::kPixels, cells_px(40)},
                             PaneSize{pane_unit::kPixels, cells_px(12)})
                .accepted);
    return s;
}

/// THE SAME DESK AS A SESSION OF THIS VINTAGE HOLDS IT, with the layout surface written down
/// as the ordinary pane it is. It goes through `add_pane`, which is the point: the migration
/// must agree with the ORDINARY DOOR a weaver's launch spends -- same reference, appended
/// position, front-most rank -- and a conversion seeding anything else goes red by field.
Setup materialized(Setup desk) {
    REQUIRE(add_pane(desk, ref_of(pane_kind::kLayouts)));
    return desk;
}

/// A Workshop arranged and closed the way a weaver closes one: its surface says hello, the
/// medium reports its room, the weaver restores a desk from their named setup, and quits --
/// through the production doors alone (`r`, `q`), so the session file holds what a weaver's
/// own session would leave, not what a fixture assigned.
void arrange_and_close(const std::string& session_path, const std::string& setup_path,
                       const Setup& desk, std::int64_t width, std::int64_t height) {
    Live t;
    t.host.session_path = session_path;
    t.host.setup_path = setup_path;
    REQUIRE(setup_persist::save_file(setup_path, desk).accepted);
    t.ready();
    t.extent_px(cells_px(width), cells_px(height));
    t.key(input::scan::kR);
    REQUIRE(t.run().active == desk);
    REQUIRE(t.session().screen_w == cells_px(width));
    REQUIRE(t.session().screen_h == cells_px(height));
    t.key(input::scan::kQ);
    REQUIRE(t.host.quit);
}

/// THE IMAGES A WINDOW OPENS FOR ITSELF on a run with no session, the default desk: its painters
/// are images. Opened and closed while the run stands, as the measured run's are.
op::ImageCounts window_images() {
    const op::ImageCounts was = op::image_counts();
    Live w;
    w.ready();
    const op::ImageCounts now = op::image_counts();
    return op::ImageCounts{now.opens - was.opens, now.closes - was.closes};
}

} // namespace

// ---- Witness A: the primary one ---------------------------------------------

TEST_CASE("the desk and the room come back, with no gesture at all") {
    TempDir dir("wux0-a");
    const std::string session = dir.file("session.json");
    const Setup desk = arranged_desk("Debugging");
    arrange_and_close(session, dir.file("setup.json"), desk, 120, 44);
    REQUIRE(std::filesystem::exists(session));

    // ---- and the weaver opens Workshop again ------------------------------
    //
    // A DIFFERENT SETUP PATH ON PURPOSE. Nothing about taking the last session back
    // may depend on the named-setup file still being where it was, or on it being
    // readable, or on it existing at all.
    Live back;
    back.host.session_path = session;
    back.host.setup_path = dir.file("somewhere-else.json");
    REQUIRE((back.pictures == 0));
    back.on_os_window(); // the medium that grows to the room restored
    back.ready();
    back.window_settles();
    INFO(back.notice());

    // THE DESK, whole: the same panes, the same authored geometry, the same order.
    CHECK(back.session().setup.active == desk);
    CHECK(back.session().panes.has(second::kKind));
    CHECK(back.session().panes.has(stock::kKind));
    // THE ROOM, which the OS window grows to.
    CHECK(back.session().screen_w == cells_px(120));
    CHECK(back.session().screen_h == cells_px(44));
    // AND NOT ONE KEY WAS PRESSED. `r` is still there and still does what it did;
    // this is the run in which nobody had to know that.
    CHECK_FALSE(back.session().notice_is_bad);
    CHECK(back.notice().find("reopened your last desk") == 0);
    CHECK(back.notice().find("\"Debugging\"") != std::string::npos);
    CHECK(back.notice().find("1440x528 px") != std::string::npos);
}

TEST_CASE("the restore notice says the room in the unit the medium names: pixels in a window, cells in a terminal") {
    // ⚔ MUTATION: the notice spelled once, at the restore, before any medium has named its unit --
    // a window's launch says `120x75 cells`.
    TempDir dir("restore-unit");
    const std::string session = dir.file("session.json");
    REQUIRE(save_last_run(session, one_layout(setup_of("Default", {pane_kind::kLayouts})), 0,
                          cells_px(120), cells_px(75), window_persist::Placement{})
                .accepted);
    // A WINDOW names its device pixel when it offers itself, and the notice standing then says
    // the room in pixels.
    {
        Live w;
        w.host.session_path = session;
        w.on_os_window();
        w.ready();
        w.extent_px(cells_px(120), cells_px(75), 8, 18, gfx::kCellPx);
        CHECK(w.notice() == "reopened your last desk \"Default\" -- 1440x900 px");
    }
    // A TERMINAL's unit is the cell.
    {
        Live t;
        t.host.session_path = session;
        t.ready();
        t.extent_px(cells_px(120), cells_px(75), 0, 0, 0);
        CHECK(t.notice() == "reopened your last desk \"Default\" -- 120x75 cells");
    }
    // A NOTICE SAID SINCE STANDS: the restore's is not said again over it.
    {
        Live k;
        k.host.session_path = session;
        k.ready();
        k.key(input::scan::kT);
        k.text("t");
        const std::string said = k.notice();
        REQUIRE(said.rfind("pane titles hidden", 0) == 0);
        k.extent_px(cells_px(120), cells_px(75), 8, 18, gfx::kCellPx);
        CHECK(k.notice() == said);
    }
}

TEST_CASE("the room is taken back only ONCE, however often a window offers itself") {
    // A window started again in place offers itself again, and an afternoon of arranging must
    // not be thrown back to a file written last night.
    TempDir dir("wux0-once");
    const std::string session = dir.file("session.json");
    arrange_and_close(session, dir.file("setup.json"), arranged_desk("First"), 110, 40);

    Live back;
    back.host.session_path = session;
    back.ready();
    REQUIRE(back.session().setup.active.name == "First");

    // the weaver changes their mind about the desk, and the window is started again under them
    pick(back, stock::kKind); // remove the Builder the restored desk brought
    const Setup after = back.session().setup.active;
    REQUIRE_FALSE(after == arranged_desk("First"));
    back.windows.restart();
    back.bus.drain_until_idle();
    CHECK(back.session().setup.active == after);
}

// ---- Witness B: the second generation replaces the first ---------------------

TEST_CASE("the second session replaces the first, room and desk both") {
    TempDir dir("wux0-b");
    const std::string session = dir.file("session.json");
    arrange_and_close(session, dir.file("first-setup.json"), arranged_desk("First"), 100, 36);
    const std::string first_bytes = slurp(session);

    // ---- reopen, change both, close again --------------------------------
    Setup second = setup_of("Second", {second::kKind});
    REQUIRE(author_pane_place(second, ref_of(second::kKind), 2, 3).accepted);
    {
        Live t;
        t.host.session_path = session;
        t.host.setup_path = dir.file("second-setup.json");
        t.on_os_window(); // the medium that grows to the room restored
        t.ready();
        t.window_settles();
        REQUIRE(t.session().setup.active.name == "First");
        REQUIRE(t.session().screen_w == cells_px(100));
        // ⚠ THE RESTORED LAYOUT CAME BACK WITH ITS ASSOCIATION, and `r` acts on THAT artifact
        // rather than on whatever `--setup` this run names: an association is what a weaver
        // related this desk to, and the configured path is only the door a layout with NO
        // association acquires one through. So the file rewritten is the one the layout names.
        REQUIRE(t.run().active_link.path == dir.file("first-setup.json"));
        REQUIRE(setup_persist::save_file(t.run().active_link.path, second).accepted);
        t.extent_px(cells_px(140), cells_px(50));
        t.key(input::scan::kR);
        REQUIRE_MESSAGE(t.session().setup.active == second, t.notice());
        t.close_requested(); // the close BOX, and it is the same door `q` is
        REQUIRE(t.host.quit);
    }
    // THE DEFECT THIS PREVENTS: a startup that reads the file correctly while
    // shutdown keeps rewriting an old cached representation of it.
    CHECK(slurp(session) != first_bytes);

    Live back;
    back.host.session_path = session;
    back.on_os_window(); // the medium that grows to the room restored
    back.ready();
    back.window_settles();
    CHECK(back.session().setup.active == second);
    CHECK(back.session().screen_w == cells_px(140));
    CHECK(back.session().screen_h == cells_px(50));
    CHECK_FALSE(back.session().panes.has(stock::kKind));
}

// ---- Witness C: there is no previous session ---------------------------------

TEST_CASE("a first launch is not an error, and needs no file to exist") {
    TempDir dir("wux0-c");
    Live t;
    t.host.session_path = dir.file("never-written.json");
    t.ready();

    CHECK(t.session().setup.active == default_setup());
    CHECK(t.session().screen_w == kScreenMinW);
    CHECK(t.session().screen_h == kScreenMinH);
    // AND NOTHING WAS SAID ABOUT IT. The most common way startup ends is the one a
    // weaver must never see a complaint about.
    INFO(t.notice());
    CHECK_FALSE(t.session().notice_is_bad);
    CHECK(t.notice().find("session") == std::string::npos);
    // Nor was a file conjured to fill the absence.
    CHECK_FALSE(std::filesystem::exists(t.host.session_path));
}

TEST_CASE("a host that chose no session file restores nothing and writes nothing") {
    Live t;
    REQUIRE(t.host.session_path.empty());
    t.ready();
    CHECK(t.session().setup.active == default_setup());
    CHECK_FALSE(t.session().notice_is_bad);
    t.key(input::scan::kQ);
    CHECK(t.host.quit);
    CHECK_FALSE(t.session().notice_is_bad); // and quitting complained about nothing
}

// ---- Witness D: the file is there and cannot be understood -------------------

TEST_CASE("a malformed session costs the desk and nothing else") {
    const std::vector<std::pair<const char*, std::string>> cases = {
        {"not a document at all", "{"},
        {"a retired object document, which is not a session", kRetiredObjectDocument},
        {"a SETUP file handed to the session reader",
         setup_persist::to_text(arranged_desk("Debugging"))},
        {"a session of another version",
         [] {
             std::string text = session_text(one_layout(arranged_desk("D")), 0);
             const std::size_t at = text.find("\"version\":12");
             REQUIRE(at != std::string::npos);
             text.replace(at, std::string("\"version\":12").size(), "\"version\":13");
             return text;
         }()},
        {"a session whose desk is not a legal setup",
         [] {
             std::string text = session_text(one_layout(arranged_desk("D")), 0);
             const std::size_t at = text.find("\"pane\":\"stack\"");
             REQUIRE(at != std::string::npos);
             text.replace(at, std::string("\"pane\":\"stack\"").size(),
                          "\"pane\":\"two words\"");
             return text;
         }()},
    };

    for (const auto& [what, bytes] : cases) {
        CAPTURE(what);
        TempDir dir("wux0-d");
        Live t;
        t.host.session_path = dir.file("session.json");
        spillout(t.host.session_path, bytes);
        t.ready();

        // IT DID NOT CRASH, IT SAID SO, AND IT OPENED.
        CHECK(t.session().notice_is_bad);
        CHECK_FALSE(t.notice().empty());
        CHECK(t.notice().find("opening with the default setup") != std::string::npos);
        CHECK(t.session().setup.active == default_setup());
        CHECK(t.session().screen_w == kScreenMinW);
        CHECK(t.session().screen_h == kScreenMinH);
        // AND THE WEAVER'S FILE IS EXACTLY AS THEY LEFT IT. Workshop does not rewrite
        // a file it could not read.
        CHECK(slurp(t.host.session_path) == bytes);
    }
}

TEST_CASE("an unreadable session names its version by NUMBER") {
    TempDir dir("wux0-d-version");
    // A WHOLE SESSION OF A NUMBER THIS BUILD HAS NEVER WRITTEN, as one value.
    std::string text = loom::compat::serialize(loom::to_value(session_value(one_layout(arranged_desk("D")), 0)));
    const std::size_t at = text.find("\"version\":12");
    REQUIRE(at != std::string::npos);
    text.replace(at, std::string("\"version\":12").size(), "\"version\":13");
    const session_persist::LoadedSession refused = session_persist::from_text(text);
    CHECK(refused.present);
    CHECK_FALSE(refused.outcome.accepted);
    // THE NUMBER IS THE FIRST THING SAID, then the honest reason: no conversion from that
    // version to this one is live. The identity of the missing power is named, because it is
    // a fact this host knows and a weaver can look for.
    CHECK(refused.outcome.refusal ==
          "session version 13 cannot be read: no live conversion from `WorkshopSession` v13 to "
          "v12 (`zengine.migrate.WorkshopSession.v13-to-v12`)");
    // ...AND A HEADER LINE OF THAT NUMBER IS SAID BY ITS NUMBER TOO.
    std::string lines = session_text(one_layout(arranged_desk("D")), 0);
    const std::size_t head = lines.find("\"version\":12");
    REQUIRE(head != std::string::npos);
    lines.replace(head, std::string("\"version\":12").size(), "\"version\":13");
    const session_persist::LoadedSession newer = session_persist::from_text(lines);
    CHECK_FALSE(newer.outcome.accepted);
    CHECK(newer.outcome.refusal.rfind("session version 13 cannot be read: ", 0) == 0);
    // AND IT CLAIMS NOTHING IT CANNOT KNOW: not that a converter exists on disk, not that
    // one should be installed. There is no unloaded discovery in this system to be honest
    // about, so the sentence does not pretend there is.
    CHECK(refused.outcome.refusal.find("install") == std::string::npos);
    CHECK(refused.outcome.refusal.find("disk") == std::string::npos);
}

TEST_CASE("a current-version file whose own field says otherwise is a forgery") {
    // TWO DIFFERENT FACTS, TWO DIFFERENT SENTENCES. A file whose ENVELOPE claims another
    // version is old and is answered by the conversion seam; a file whose envelope claims
    // THIS version over a body that says another is inconsistent with itself, and only a
    // forgery produces one.
    std::string text = session_text(one_layout(arranged_desk("D")), 0);
    const std::size_t at = text.find("\"format_version\":\"13\"");
    REQUIRE(at == std::string::npos);
    // ⚠ THE SESSION'S OWN FIELD AND A LAYOUT'S ARE TWO FACTS AND TWO NUMBERS: a session nests
    // desks at their own version. The case edits the session's and asserts the session's
    // sentence; the desk's own has a separate owner, and the case below proves it.
    const std::size_t field = text.find("\"format_version\":\"12\"");
    REQUIRE(field != std::string::npos);
    text.replace(field, std::string("\"format_version\":\"12\"").size(),
                 "\"format_version\":\"13\"");

    op::Catalog conversions;
    REQUIRE(conversions.mount("suite", session_history::conversions()));
    const session_persist::LoadedSession refused =
        session_persist::from_text(text, &conversions);
    CHECK_FALSE(refused.outcome.accepted);
    CHECK(refused.outcome.refusal ==
          "this session claims version 12 and its own format_version field says 13");
    // ...and it did not become a conversion request on the way past.
    CHECK(refused.outcome.refusal.find("conversion") == std::string::npos);
}

// ---- Witness E: a viewport this Workshop will not open at --------------------

TEST_CASE("a hostile room is declined, and the desk still comes back") {
    struct Case {
        const char* what;
        std::int64_t w;
        std::int64_t h;
    };
    // WRITTEN BY THE HONEST WRITER, not forged: the writer writes what it is given
    // and the READER is where the judgement lives, so a case can spell an impossible
    // room without going behind the format's back.
    const Case cases[] = {
        {"no width at all", 0, 40},
        {"no height at all", 120, 0},
        {"a negative room", -100, -40},
        {"a room larger than this Workshop is honest at", 120, kScreenMaxRows + 1},
        {"an enormous room", 100000, 100000},
    };
    const Setup desk = arranged_desk("Debugging");

    for (const Case& c : cases) {
        CAPTURE(c.what);
        TempDir dir("wux0-e");
        Live t;
        t.host.session_path = dir.file("session.json");
        REQUIRE(save_last_run(t.host.session_path, one_layout(desk), 0, cells_px(c.w),
                              cells_px(c.h), window_persist::Placement{})
                    .accepted);
        t.ready();

        // THE DESK CAME BACK. Throwing away a good desk over a bad number would be
        // the corrupt-save-makes-Workshop-useless failure, committed by the code
        // meant to prevent it.
        CHECK(t.session().setup.active == desk);
        // THE ROOM DID NOT, AND IT WAS NOT CLAMPED INTO ONE EITHER: Workshop opens at
        // its floor, exactly as a first launch does.
        CHECK(t.session().screen_w == kScreenMinW);
        CHECK(t.session().screen_h == kScreenMinH);
        CHECK(paint(t.session()).width == kScreenMinW);
        // AND IT NEVER CLAIMS THE SIZE CAME BACK. The value that was declined is
        // named, because a weaver looking at their own file can act on it.
        CHECK(t.session().notice_is_bad);
        CHECK(t.notice().find("is not one this Workshop opens at") != std::string::npos);
        CHECK(t.notice().find(std::to_string(cells_px(c.w)) + "x" + std::to_string(cells_px(c.h))) !=
              std::string::npos);
    }
}

TEST_CASE("the band a room is honoured in is the one the screen is honest at") {
    CHECK(window_persist::viewport_honoured(kScreenMinW, kScreenMinH));
    CHECK(window_persist::viewport_honoured(kScreenMaxW, kScreenMaxH));
    CHECK_FALSE(window_persist::viewport_honoured(kScreenMinW - 1, kScreenMinH));
    CHECK_FALSE(window_persist::viewport_honoured(kScreenMinW, kScreenMinH - 1));
    CHECK_FALSE(window_persist::viewport_honoured(kScreenMaxW + 1, kScreenMaxH));
    CHECK_FALSE(window_persist::viewport_honoured(kScreenMinW, kScreenMaxH + 1));
    CHECK_FALSE(window_persist::viewport_honoured(0, 0));
}

// ---- Witness F: named setups are a different promise and stay one ------------

TEST_CASE("an automatic save never touches the file a weaver named") {
    TempDir dir("wux0-f-save");
    Live t;
    t.host.session_path = dir.file("session.json");
    t.host.setup_path = dir.file("setup.json");
    const Setup named = arranged_desk("Debugging");
    REQUIRE(setup_persist::save_file(t.host.setup_path, named).accepted);
    const std::string setup_bytes = slurp(t.host.setup_path);

    t.ready();
    t.extent_px(cells_px(120), cells_px(44));
    pick(t, stock::kKind); // arrange something the named setup does not have
    t.key(input::scan::kQ);

    REQUIRE(std::filesystem::exists(t.host.session_path));
    // THE PROPERTY THE TWO FILES EXIST FOR: quitting wrote a session and left the
    // weaver's named desk byte-for-byte alone.
    CHECK(slurp(t.host.setup_path) == setup_bytes);
    CHECK(setup_persist::load_file(t.host.setup_path).setup == named);
}

TEST_CASE("a restored session never touches the file a weaver named, either") {
    TempDir dir("wux0-f-restore");
    const std::string session = dir.file("session.json");
    const std::string setup = dir.file("setup.json");
    // A DESK THIS BUILD'S WEAVER WOULD HAVE SAVED, which names the Layouts pane -- the
    // case restores it and then goes on using the tab run, and a desk that did not name it
    // would come back with no tab run at all (which is the honest answer for such a file,
    // and is what launching the Layouts pane undoes).
    const Setup named = materialized(arranged_desk("Debugging"));
    REQUIRE(setup_persist::save_file(setup, named).accepted);
    const std::string setup_bytes = slurp(setup);
    REQUIRE(save_last_run(session,
                          one_layout(setup_of("Loose", {second::kKind, pane_kind::kLayouts})), 0,
                          cells_px(110), cells_px(38), window_persist::Placement{})
                .accepted);

    Live t;
    t.host.session_path = session;
    t.host.setup_path = setup;
    t.ready();
    REQUIRE(t.session().setup.active.name == "Loose");
    CHECK(slurp(setup) == setup_bytes);

    // ...AND BOTH SETUP GESTURES STILL DO EXACTLY WHAT THEY DID. `r` reads the named
    // file over the restored session; `s` writes the named file and nothing else.
    t.key(input::scan::kR);
    CHECK(t.session().setup.active == named);
    CHECK((live_status(t.run()) == setup_link::kCurrent));
    const std::string session_bytes = slurp(session);
    name_setup(t, "Renamed");
    CHECK(setup_persist::load_file(setup).setup.name == "Renamed");
    CHECK(slurp(session) == session_bytes); // naming a setup wrote no session
}

TEST_CASE("the three files are three formats, and each refuses the others") {
    // (The third is an old object document: a weaver may still have one on disk, and neither
    // reader here takes it for its own.)
    const Setup desk = arranged_desk("Debugging");
    const std::string doc_text = kRetiredObjectDocument;
    const std::string setup_text = setup_persist::to_text(desk);
    const std::string session_bytes = session_text(one_layout(desk), 0);

    CHECK(std::string(session_persist::kFormat) == "zengine-workshop-session");
    CHECK(std::string(session_persist::kFormat) != std::string(setup_persist::kFormat));
    CHECK(doc_text.find(std::string("\"") + session_persist::kFormat + "\"") == std::string::npos);

    CHECK_FALSE(session_persist::from_text(doc_text).outcome.accepted);
    CHECK_FALSE(session_persist::from_text(setup_text).outcome.accepted);
    CHECK_FALSE(setup_persist::from_text(doc_text).outcome.accepted);
    CHECK_FALSE(setup_persist::from_text(session_bytes).outcome.accepted);
    // A session handed to the setup reader is refused, and NOT half-read.
    CHECK(setup_persist::from_text(session_bytes).setup.panes.empty());
}

// ---- The format itself -------------------------------------------------------

TEST_CASE("a session round-trips, and a second save is byte-identical") {
    const Setup desk = arranged_desk("Debugging");
    const std::string first = session_text(one_layout(desk), 0);
    const session_persist::LoadedSession read = session_persist::from_text(first);
    REQUIRE(read.outcome.accepted);
    CHECK(read.present);
    CHECK(live_layout(read) == desk);
    CHECK(read.layouts.front().id == 1);
    CHECK(read.next_id == 2);
    CHECK(session_persist::to_text(read.layouts, read.active, read.next_id) == first);
}

namespace {

/// A desk that says every place word and every size word, each number where its word reads one.
Setup every_word_desk() {
    Setup desk;
    desk.name = "Words";
    const auto size_at = [](std::size_t i) {
        const auto mode = static_cast<std::int64_t>(i % ui::kSizeWords.size());
        const std::int64_t amount =
            mode == ui::kSizeShare ? 2500 + static_cast<std::int64_t>(i)
                                   : (mode == ui::kSizePixels ? 120 + static_cast<std::int64_t>(i) : 0);
        return PaneSize{mode, amount};
    };
    for (std::size_t i = 0; i < ui::kPlaceWords.size(); ++i) {
        const auto mode = static_cast<std::int64_t>(i);
        SetupPane row;
        row.ref = PaneRef{"zengine.words", "place-" + std::to_string(i)};
        row.place = PanePlace{mode, ui::place_reads_x(mode) ? 10 + mode : 0,
                              ui::place_reads_y(mode) ? 20 + mode : 0};
        row.width = size_at(i);
        row.height = size_at(i + 2);
        row.front = mode;
        desk.panes.push_back(std::move(row));
    }
    return desk;
}

} // namespace

TEST_CASE("every place and size word survives the setup file and the session byte for byte") {
    // ⚔ MUTATION: `place_out` writing a word's coordinates whatever the word -- a centred axis
    // comes back carrying an offset, which the file refuses, so the desk does not come back.
    const Setup desk = every_word_desk();
    REQUIRE_MESSAGE(check_setup(desk).accepted, check_setup(desk).refusal);
    const std::string text = setup_persist::to_text(desk);
    // EACH WORD IS WRITTEN AS ITS WORD, never as the number this build holds it by.
    for (const std::string_view word : ui::kPlaceWords) {
        CHECK(text.find("\"place\":{\"mode\":\"" + std::string(word) + "\"") != std::string::npos);
    }
    for (const std::string_view word : ui::kSizeWords) {
        CHECK(text.find("{\"mode\":\"" + std::string(word) + "\"") != std::string::npos);
    }
    CHECK(text.find("\"place\":{\"mode\":\"center\"}") != std::string::npos);
    CHECK(text.find("\"place\":{\"mode\":\"top\",\"y\":\"22\"}") != std::string::npos);
    CHECK(text.find("\"place\":{\"mode\":\"right\",\"x\":\"16\"}") != std::string::npos);
    const setup_persist::LoadedSetup read = setup_persist::from_text(text);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.setup == desk);
    CHECK(setup_persist::to_text(read.setup) == text);

    // ...AND INSIDE A SESSION, which holds the same rows.
    const std::string session = session_text(one_layout(desk), 0);
    const session_persist::LoadedSession held = session_persist::from_text(session);
    REQUIRE_MESSAGE(held.outcome.accepted, held.outcome.refusal);
    CHECK(live_layout(held) == desk);
    CHECK(session_persist::to_text(held.layouts, held.active, held.next_id) == session);

    // A WORD COSTS NO DECODED CELL A PIXEL WOULD NOT: the fullest desk, every row at its bounds,
    // its places and sizes cycling through every word, decodes in no more cells than the same desk
    // in pixels, and inside the budget.
    Setup full = setup_of("Full", {});
    fill_to_every_bound(full);
    const std::int64_t in_pixels = decoded_cells(loom::to_value(setup_persist::to_setup(full)));
    for (std::size_t i = 0; i < full.panes.size(); ++i) {
        const auto mode = static_cast<std::int64_t>(i % ui::kPlaceWords.size());
        full.panes[i].place = PanePlace{mode, ui::place_reads_x(mode) ? kMaxPanePixels : 0,
                                        ui::place_reads_y(mode) ? kMaxPanePixels : 0};
        const auto size = static_cast<std::int64_t>(i % ui::kSizeWords.size());
        full.panes[i].width = PaneSize{size, size == ui::kSizeShare ? ui::kShareWhole
                                             : (size == ui::kSizePixels ? kMaxPanePixels : 0)};
    }
    REQUIRE_MESSAGE(check_setup(full).accepted, check_setup(full).refusal);
    const std::int64_t in_words = decoded_cells(loom::to_value(setup_persist::to_setup(full)));
    CHECK(in_words <= in_pixels);
    CHECK(in_words <= kDecodedCellBudget);
    CHECK(decoded_cells(loom::to_value(setup_persist::to_setup(desk))) <= kDecodedCellBudget);
}

TEST_CASE("a word's numbers out of its range are refused in the weaver's words, by the file and by the owner") {
    // ⚔ MUTATION: `check_pane_size` taking any share -- a share past the whole room is stored.
    const Setup good = every_word_desk();
    const auto refused = [&good](const std::function<void(SetupPane&)>& edit) {
        Setup bad = good;
        edit(bad.panes[3]);
        return check_setup(bad).refusal;
    };
    CHECK(refused([](SetupPane& r) { r.width = PaneSize{ui::kSizeShare, 12500}; }) ==
          "a pane width's share is 1 to 10000 ten-thousandths of the room (5000 is half) -- not "
          "12500");
    CHECK(refused([](SetupPane& r) { r.height = PaneSize{ui::kSizeShare, 0}; }) ==
          "a pane height's share is 1 to 10000 ten-thousandths of the room (5000 is half) -- not 0");
    CHECK(refused([](SetupPane& r) { r.place = PanePlace{ui::kPlaceTop, 5, 0}; }) ==
          "a `top` pane place is centred across, so it carries no x");
    CHECK(refused([](SetupPane& r) { r.place = PanePlace{ui::kPlaceCenter, 0, 3}; }) ==
          "a `center` pane place is centred down, so it carries no y");
    CHECK(refused([](SetupPane& r) { r.width = PaneSize{ui::kSizeFill, 2}; }) ==
          "a fill pane width carries no amount");
    CHECK(refused([](SetupPane& r) { r.height = PaneSize{ui::kSizeFit, 1}; }) ==
          "a fit pane height carries no amount");
    CHECK(refused([](SetupPane& r) { r.place = PanePlace{ui::kPlaceBottomRight, 0, -1}; }) ==
          "a pane place cannot be negative");
    CHECK(refused([](SetupPane& r) { r.place = PanePlace{42, 0, 0}; })
              .find("a pane place is default, pixels, top") == 0);
    // THE FILE SAYS ITS OWN: a number where the word reads none, and one missing where it reads it.
    const std::string text = setup_persist::to_text(good);
    const auto forged = [&text](const std::string& from, const std::string& to) {
        std::string bad = text;
        const std::size_t at = bad.find(from);
        REQUIRE(at != std::string::npos);
        bad.replace(at, from.size(), to);
        return setup_persist::from_text(bad).outcome.refusal;
    };
    CHECK(forged("\"place\":{\"mode\":\"top\",\"y\":\"22\"}",
                 "\"place\":{\"mode\":\"top\",\"x\":\"1\",\"y\":\"22\"}")
              .find("a top pane place names its y and no x: it is centred on that axis") !=
          std::string::npos);
    CHECK(forged("\"place\":{\"mode\":\"center\"}", "\"place\":{\"mode\":\"center\",\"x\":\"0\"}")
              .find("a center pane place carries no coordinates") != std::string::npos);
    CHECK(forged("\"place\":{\"mode\":\"right\",\"x\":\"16\"}", "\"place\":{\"mode\":\"right\"}")
              .find("a right pane place names its x and no y") != std::string::npos);
}

TEST_CASE("a session file holds the run and nothing of the window, and nothing runtime") {
    const std::string text = session_text(one_layout(arranged_desk("Debugging")), 0);
    // THE DESK IS THE SETUP'S OWN REPRESENTATION, not a paraphrase of it: every pane
    // row a setup file would have written is in here, spelled the same way.
    for (const char* fragment : {"\"provider\":\"zengine.test.stack\"", "\"pane\":\"stack\"",
                                 "\"pane\":\"second\"", "\"mode\":\"pixels\"",
                                 "\"front\":", "\"format\":\"zengine-workshop-setup\""}) {
        CHECK_MESSAGE(text.find(fragment) != std::string::npos, fragment);
    }
    // THE WINDOW'S FACTS ARE THE WINDOW FILE'S: no room, no placement.
    for (const char* window : {"viewport", "placement", "window_room"}) {
        CHECK_MESSAGE(text.find(window) == std::string::npos, window);
    }
    // AND NOTHING THAT BELONGS TO A RUNNING PROCESS. No WeaveId, no runtime pane
    // handle, no catalog row, no loaded artifact, no selection, no drag.
    for (const char* forbidden : {"weave", "WeaveId", "kind", "runtime", "catalog", "selected",
                                  "cursor", "drag", "notice", "document", "text_advance",
                                  "text_line"}) {
        CHECK_MESSAGE(text.find(forbidden) == std::string::npos, forbidden);
    }
}

TEST_CASE("a session too large to be one is refused before it is read") {
    TempDir dir("wux0-big");
    const std::string path = dir.file("session.json");
    spillout(path, std::string(session_persist::kMaxSessionBytes + 1, 'x'));
    const session_persist::LoadedSession refused = session_persist::load_file(path);
    CHECK(refused.present);
    CHECK_FALSE(refused.outcome.accepted);
    CHECK(refused.outcome.refusal.find("a Workshop session can be") != std::string::npos);
}

TEST_CASE("a write that fails leaves the last good session where it was") {
    TempDir dir("wux0-write");
    const std::string path = dir.file("session.json");
    const Setup first = arranged_desk("First");
    REQUIRE(save_last_run(path, one_layout(first), 0, cells_px(120), cells_px(44),
                          window_persist::Placement{})
                .accepted);
    const std::string good = slurp(path);

    // The sibling the writer needs is occupied by a DIRECTORY, so the candidate
    // cannot be written -- and the destination is never opened.
    std::filesystem::create_directories(persist::pending_path(path));
    const Written refused = save_last_run(path, one_layout(setup_of("Second", {second::kKind})), 0,
                                          cells_px(90), cells_px(30), window_persist::Placement{});
    CHECK_FALSE(refused.accepted);
    CHECK(slurp(path) == good);
    std::filesystem::remove_all(persist::pending_path(path));
    CHECK(save_last_run(path, one_layout(setup_of("Second", {second::kKind})), 0, cells_px(90),
                        cells_px(30), window_persist::Placement{})
              .accepted);
}

// ---- The room, and the desk into it --------------------------

TEST_CASE("a restored desk opens every pane it names, in the room it restored") {
    // The floor composition's column holds one stacked pane and the restored room's holds two;
    // either way every pane the desk names is seated, the stack being rationed by no room.
    TempDir dir("wux0-order");
    const std::string session = dir.file("session.json");
    Setup two = setup_of("Two", {stock::kKind});
    REQUIRE(add_pane(two, hello_ref()));
    REQUIRE(save_last_run(session, one_layout(two), 0, cells_px(120), cells_px(60),
                          window_persist::Placement{})
                .accepted);

    REQUIRE(stack_slots_that_fit(kMinScreen) == 1);
    REQUIRE(stack_slots_that_fit(screen_of(cells_px(120), cells_px(60))) >= 2);

    PaneRig r;
    r.host.session_path = session;
    r.mount_workshop();
    admit_stock(r.session().panes); // the stand-in, first, as a `Live` would have it
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    // The provider announces itself before the surface does, so the reference in the
    // session resolves at the moment the desk is applied -- a load order that must work
    // either way round.
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });

    r.on_os_window(); // the medium that grows to the room restored
    r.ready();
    r.window_settles();
    CHECK(r.w->session().screen_w == cells_px(120));
    CHECK(r.w->session().screen_h == cells_px(60));
    CHECK(r.w->session().panes.open.size() == 2);
    CHECK(r.w->session().panes.has(stock::kKind));
    CHECK(unresolved_panes(r.w->session().setup.active, r.w->session().panes).empty());
}

TEST_CASE("the startup notice counts no pane nobody has had a turn to offer") {
    // MEASURED ON A REAL WINDOW FIRST, and the notice was misleading: at the instant a
    // restored desk is applied, Workshop has published `PaneCatalogRequested` and the answers
    // are still in the queue, so EVERY external reference in it is unresolved right now and
    // resolved a moment later. A count here is a fact about the clock.
    TempDir dir("wux0-unresolved");
    const std::string session = dir.file("session.json");
    Setup mixed = setup_of("Mixed", {second::kKind, pane_kind::kLayouts});
    REQUIRE(add_pane(mixed, hello_ref()));
    REQUIRE(save_last_run(session, one_layout(mixed), 0, cells_px(110), cells_px(40),
                          window_persist::Placement{})
                .accepted);

    Live t;
    t.host.session_path = session;
    t.ready();

    REQUIRE(t.session().setup.active == mixed);
    CHECK(t.notice().find("reopened your last desk") == 0);
    CHECK(t.notice().find("unresolved") == std::string::npos);
    CHECK_FALSE(t.session().notice_is_bad);

    // ...AND THE COUNT IS NOT LOST, it is simply owned by the surface that recomputes it.
    // The setup line carries it live, off the same `unresolved_panes` call, every paint.
    REQUIRE_FALSE((t.pictures == 0));
    CHECK(setup_row(t.shown(), screen_of(t.session())).find("1 unresolved") !=
          std::string::npos);
}

TEST_CASE("`r` keeps its unresolved note -- a weaver asking is asking later") {
    TempDir dir("wux0-r-note");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    Setup mixed = setup_of("Mixed", {second::kKind});
    REQUIRE(add_pane(mixed, hello_ref()));
    REQUIRE(setup_persist::save_file(t.host.setup_path, mixed).accepted);
    t.ready();
    t.key(input::scan::kR);
    CHECK(t.notice().find("1 pane unresolved") != std::string::npos);
}

// ========================================================================================
// The installed application: per-user roots, explicit isolation, the one-time legacy import,
// the prefs file, and the window's desktop placement remembered, offered back, and judged by
// the medium that can see displays.
// ========================================================================================

// ---- The roots and the precedence (user_paths.hpp) -----------------------------------

TEST_CASE("the two Windows roots are the platform's own conventions") {
    user_paths::Environment env;
    env.appdata = "C:/Users/riley/AppData/Roaming";
    env.local_appdata = "C:/Users/riley/AppData/Local";
    CHECK(user_paths::windows_config_root(env) ==
          "C:/Users/riley/AppData/Roaming/zengine-workshop");
    CHECK(user_paths::windows_state_root(env) ==
          "C:/Users/riley/AppData/Local/zengine-workshop");
    // A BARE ENVIRONMENT IS AN ABSENCE, NEVER A FALLBACK TO CWD.
    user_paths::Environment bare;
    CHECK(user_paths::windows_config_root(bare).empty());
    CHECK(user_paths::windows_state_root(bare).empty());
}

TEST_CASE("the two XDG roots, and their home fallbacks") {
    user_paths::Environment env;
    env.xdg_config_home = "/tmp/xdgc";
    env.xdg_state_home = "/tmp/xdgs";
    env.home = "/home/riley";
    CHECK(user_paths::xdg_config_root(env) == "/tmp/xdgc/zengine-workshop");
    CHECK(user_paths::xdg_state_root(env) == "/tmp/xdgs/zengine-workshop");
    // THE STANDARD HOME FALLBACKS, exactly the XDG base directory spec's.
    env.xdg_config_home.clear();
    env.xdg_state_home.clear();
    CHECK(user_paths::xdg_config_root(env) == "/home/riley/.config/zengine-workshop");
    CHECK(user_paths::xdg_state_root(env) == "/home/riley/.local/state/zengine-workshop");
    // No HOME either: the absence, never an invention.
    env.home.clear();
    CHECK(user_paths::xdg_config_root(env).empty());
    CHECK(user_paths::xdg_state_root(env).empty());
}

TEST_CASE("one precedence -- explicit path, then isolation, then the default") {
    const std::string root = "/tmp/root";
    // 1. An explicit path wins over everything, isolation included: an isolated witness
    //    that needs scratch persistence names its scratch files.
    CHECK(user_paths::resolve_durable_path("mine.json", false, root, "workshop-x.json") ==
          "mine.json");
    CHECK(user_paths::resolve_durable_path("mine.json", true, root, "workshop-x.json") ==
          "mine.json");
    // 2. Isolation makes the fact absent -- the weave's designed no-persistence.
    CHECK(user_paths::resolve_durable_path("", true, root, "workshop-x.json").empty());
    // 3. Otherwise the per-user default under the root.
    CHECK(user_paths::resolve_durable_path("", false, root, "workshop-x.json") ==
          "/tmp/root/workshop-x.json");
    // 4. A root this environment cannot supply is the same absence, never CWD.
    CHECK(user_paths::resolve_durable_path("", false, "", "workshop-x.json").empty());
}

// ---- The one-time legacy import ------------------------------------------------------

TEST_CASE("a legacy-only file is imported once, and the original is left in place") {
    TempDir dir("wux3-import");
    const std::string legacy = dir.file("workshop-session.json");
    const std::string dest = dir.file("root/workshop-session.json");
    spillout(legacy, "the weaver's bytes");

    // The destination's parent does not exist yet: the import creates it (first write).
    const user_paths::LegacyImport did =
        user_paths::import_legacy_file(dest, legacy, "session", session_persist::kMaxSessionBytes);
    CHECK(did.imported);
    CHECK_FALSE(did.shadowed);
    CHECK(slurp(dest) == "the weaver's bytes");
    // NEVER DELETED, NEVER REWRITTEN: the original stands byte-for-byte.
    CHECK(slurp(legacy) == "the weaver's bytes");
    // The note says what happened, naming both paths.
    CHECK(did.note.find("imported") != std::string::npos);
    CHECK(did.note.find(legacy) != std::string::npos);
    CHECK(did.note.find(dest) != std::string::npos);
    CHECK(did.note.find("left in place") != std::string::npos);
}

TEST_CASE("an existing user-root file always wins over a legacy file") {
    TempDir dir("wux3-conflict");
    const std::string legacy = dir.file("workshop-keymap.json");
    const std::string dest = dir.file("root/workshop-keymap.json");
    std::filesystem::create_directories(dir.file("root"));
    spillout(dest, "the user root's newer truth");
    spillout(legacy, "an older local file");

    const user_paths::LegacyImport did =
        user_paths::import_legacy_file(dest, legacy, "keymap", keymap_persist::kMaxKeymapBytes);
    CHECK_FALSE(did.imported);
    CHECK(did.shadowed);
    // NOTHING MOVED, in either direction.
    CHECK(slurp(dest) == "the user root's newer truth");
    CHECK(slurp(legacy) == "an older local file");
    // ...and the weaver is told which file is being read and how to end the note.
    CHECK(did.note.find(dest) != std::string::npos);
    CHECK(did.note.find("not read") != std::string::npos);
    CHECK(did.note.find("delete it") != std::string::npos);
}

TEST_CASE("repeated launches converge -- the import can never fire twice") {
    TempDir dir("wux3-repeat");
    const std::string legacy = dir.file("workshop-session.json");
    const std::string dest = dir.file("root/workshop-session.json");
    spillout(legacy, "first bytes");
    REQUIRE(user_paths::import_legacy_file(dest, legacy, "session",
        session_persist::kMaxSessionBytes).imported);

    // The legacy file CHANGES afterwards -- a weaver still running an old build from this
    // directory -- and the user root must not be overwritten by it on any later launch.
    spillout(legacy, "second bytes the root must never take");
    const user_paths::LegacyImport again =
        user_paths::import_legacy_file(dest, legacy, "session", session_persist::kMaxSessionBytes);
    CHECK_FALSE(again.imported);
    CHECK(again.shadowed);
    CHECK(slurp(dest) == "first bytes");
    const user_paths::LegacyImport third =
        user_paths::import_legacy_file(dest, legacy, "session", session_persist::kMaxSessionBytes);
    CHECK_FALSE(third.imported);
    CHECK(slurp(dest) == "first bytes");
}

TEST_CASE("no legacy file, no destination -- the import does nothing, silently") {
    TempDir dir("wux3-nothing");
    const user_paths::LegacyImport did = user_paths::import_legacy_file(
        dir.file("root/workshop-session.json"), dir.file("workshop-session.json"), "session",
        session_persist::kMaxSessionBytes);
    CHECK_FALSE(did.imported);
    CHECK_FALSE(did.shadowed);
    CHECK(did.note.empty());
    CHECK_FALSE(std::filesystem::exists(dir.file("root")));
}

TEST_CASE("a legacy session as large as the session reader takes is imported whole, and a larger "
          "one or a taken root is left alone") {
    // THE LARGEST LEGAL SESSION: every layout a maximal desk remembering a maximal desk. The
    // session reader takes it, and it is larger than a mebibyte, so an import read capped there
    // would leave it behind.
    std::vector<Layout> run;
    for (std::size_t i = 0; i < kMaxLayouts; ++i) {
        Setup s = setup_of(std::string(kMaxSetupNameLen, static_cast<char>('a' + i)), {});
        fill_to_every_bound(s);
        run.push_back(Layout{s, SetupLink{std::string(256, 'p') + ".json", s}});
    }
    const std::string text = session_text(run, kMaxLayouts - 1);
    REQUIRE(text.size() > (std::size_t{1} << 20));
    REQUIRE(text.size() <= session_persist::kMaxSessionBytes);
    TempDir dir("legacy-import-bound");
    const std::string legacy = dir.file("workshop-session.json");
    spillout(legacy, text);
    const session_persist::LoadedSession legacy_read = session_persist::load_file(legacy);
    REQUIRE_MESSAGE(legacy_read.outcome.accepted, legacy_read.outcome.refusal);

    const std::string dest = dir.file("root/workshop-session.json");
    const user_paths::LegacyImport did =
        user_paths::import_legacy_file(dest, legacy, "session", session_persist::kMaxSessionBytes);
    CHECK_MESSAGE(did.imported, did.note);
    CHECK(slurp(dest) == text);
    CHECK(slurp(legacy) == text);
    const session_persist::LoadedSession imported = session_persist::load_file(dest);
    REQUIRE(imported.present);
    CHECK_MESSAGE(imported.outcome.accepted, imported.outcome.refusal);

    // A FILE PAST THE READER'S CEILING is not imported: nothing is written, the original stays.
    const std::string over = dir.file("over/workshop-session.json");
    const std::string over_dest = dir.file("over-root/workshop-session.json");
    std::filesystem::create_directories(dir.file("over"));
    const std::string too_large(session_persist::kMaxSessionBytes + 1, 'x');
    spillout(over, too_large);
    const user_paths::LegacyImport refused =
        user_paths::import_legacy_file(over_dest, over, "session",
                                       session_persist::kMaxSessionBytes);
    CHECK_FALSE(refused.imported);
    CHECK(refused.note.find("could not read it") != std::string::npos);
    CHECK_FALSE(std::filesystem::exists(over_dest));
    CHECK(slurp(over) == too_large);

    // ...AND A ROOT THAT ALREADY HOLDS A SESSION KEEPS IT.
    spillout(legacy, "a newer local file");
    const user_paths::LegacyImport shadowed =
        user_paths::import_legacy_file(dest, legacy, "session", session_persist::kMaxSessionBytes);
    CHECK_FALSE(shadowed.imported);
    CHECK(shadowed.shadowed);
    CHECK(slurp(dest) == text);
}

// ---- A folder named beyond ASCII ---------------------------------------------------------

namespace {

/// A FOLDER A WEAVER'S MACHINE MAY HOLD: Western and Cyrillic letters and a space, as the UTF-8
/// bytes every narrow door of the product takes.
const std::string kBeyondAscii = "Zo\xC3\xAB \xD0\x96 Doe";

/// The same path as the filesystem holds it, built from UTF-8 whatever the code page.
std::filesystem::path held(const std::string& utf8) {
    return std::filesystem::path(
        std::u8string(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
}

} // namespace

TEST_CASE("a weaver's files under a folder named beyond ASCII are written, found and read back") {
    // EVERY DOOR TAKES THE NARROW UTF-8 SPELLING, as the host hands it the roots and the command
    // line; the filesystem is then asked by the name it really holds. A program in another code
    // page writes nothing there, or something under another name.
    TempDir dir("beyond-ascii");
    const std::string folder = dir.path().string() + "/" + kBeyondAscii;
    const std::string session = folder + "/state/zengine-workshop/workshop-session.json";
    const Setup desk = arranged_desk("Debugging");
    const Written saved = save_last_run(session, one_layout(desk), 0, cells_px(120), cells_px(44),
                                        window_persist::Placement{});
    REQUIRE_MESSAGE(saved.accepted, saved.refusal);
    CHECK(std::filesystem::exists(held(session)));
    CHECK(std::filesystem::exists(session));
    const session_persist::LoadedSession read = session_persist::load_file(session);
    REQUIRE(read.present);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(live_layout(read) == desk);

    // The one-time import, from a legacy file in such a folder to a root in another.
    const std::string legacy = folder + "/workshop-keymap.json";
    const std::string keymap = folder + "/config/zengine-workshop/workshop-keymap.json";
    REQUIRE(persist::write_file(legacy, "the weaver's bytes").accepted);
    const user_paths::LegacyImport did =
        user_paths::import_legacy_file(keymap, legacy, "keymap", keymap_persist::kMaxKeymapBytes);
    CHECK_MESSAGE(did.imported, did.note);
    CHECK(std::filesystem::exists(held(keymap)));
    const persist::FileText back = persist::read_file(keymap, 1024, "a keymap");
    REQUIRE_MESSAGE(back.outcome.accepted, back.outcome.refusal);
    CHECK(back.text == "the weaver's bytes");
}

TEST_CASE("the host resolves the weaver's files through the one precedence") {
    // A SOURCE TRIPWIRE, the host tier's own instrument: main() must reach every per-user
    // default through `user_paths::resolve_durable_path` -- one spelling of the precedence,
    // pinned above -- and must spell the isolation affordance. A host that reverted to a
    // bare CWD name would pass every weave case and silently re-scatter the weaver's files.
    const std::string host = file_source(WORKSHOP_HOST_CPP);
    std::size_t resolutions = 0;
    for (std::size_t at = host.find("user_paths::resolve_durable_path");
         at != std::string::npos;
         at = host.find("user_paths::resolve_durable_path", at + 1)) {
        ++resolutions;
    }
    CHECK(resolutions >= 3); // keymap, prefs, session
    CHECK(host.find("--isolated") != std::string::npos);
    CHECK(host.find("user_paths::import_legacy_file") != std::string::npos);
    // The project file stays a project file: its default is still the bare name, and an old
    // object document's bare name is only LOOKED FOR in the project, to be said and left alone.
    CHECK(host.find("kDefaultSetupFileName") != std::string::npos);
    CHECK(host.find("persist::kRetiredDocumentName") != std::string::npos);
}

// ---- The prefs file ------------------------------------------------------------------

TEST_CASE("the settings file round-trips what it keeps: Workshop's own settings, another office's, pin lists and locks") {
    using prefs_persist::Presentation;
    using prefs_persist::Prefs;
    // WORKSHOP'S OWN, KEPT AS ITS ABSENCE AT THE DEFAULT: the shown titles and the shipped
    // spellings keep nothing.
    CHECK(prefs_persist::prefs_of(Presentation{}) == Prefs{});
    const Prefs hidden = prefs_persist::prefs_of(Presentation{false});
    const std::string text = prefs_persist::to_text(hidden);
    CHECK(text.find("\"format\":\"zengine-workshop-prefs\"") != std::string::npos);
    CHECK(text.find("\"workshop.pane-titles\"") != std::string::npos);
    prefs_persist::LoadedPrefs read = prefs_persist::from_text(text);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK_FALSE(read.prefs.titles_shown);
    CHECK(read.kept == hidden);

    // THE TWO SPELLINGS, THE PIN SCOPE AND THE RING, each Workshop's own key.
    Prefs all = prefs_persist::prefs_of(Presentation{true, KeySpelling::kFull, KeySpelling::kShort});
    REQUIRE(prefs_persist::put_workshop_value(
        all, prefs_persist::kPinnedScopeKey, PaneSetting{"", {}, {}, std::string(kPinsPerLayout)}));
    REQUIRE(prefs_persist::put_workshop_value(
        all, prefs_persist::kFocusRingKey, PaneSetting{"", {}, {}, std::string("plain")}));
    // ...ANOTHER OFFICE'S, KEPT BY FORM; A PIN LIST; AN EVERYWHERE LOCK.
    REQUIRE(prefs_persist::put_office_value(all, "test.skin", "window.face",
                                            PaneSetting{"", {}, 15, {}}));
    prefs_persist::put_pin_list(all, prefs_persist::PinList{"zengine.terminal/terminal",
                                                            {"wheel-rows", "legend"}});
    all.locks.push_back(prefs_persist::SettingLock{"zengine.terminal/terminal", "legend", false,
                                                   std::nullopt, std::nullopt, false});
    read = prefs_persist::from_text(prefs_persist::to_text(all));
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.kept == all);
    CHECK(read.prefs.band_keys == KeySpelling::kFull);
    CHECK(read.prefs.hotkeys_keys == KeySpelling::kShort);
    CHECK(read.prefs.pins_per_layout);
    CHECK(read.prefs.ring_plain);
    // KEPT IN ORDER: offices by name, an office's settings by key.
    REQUIRE(all.offices.size() == 2);
    CHECK(all.offices[0].office == "test.skin");
    CHECK(all.offices[1].office == prefs_persist::kWorkshopOffice);
    // ...AND A VALUE PUT BACK AT ITS DEFAULT TAKES ITS KEY, AND AN OFFICE KEEPING NOTHING, AWAY.
    REQUIRE(prefs_persist::put_office_value(all, "test.skin", "window.face", std::nullopt));
    CHECK(prefs_persist::office_kept(all, "test.skin") == nullptr);

    // A FOREIGN VERSION IS REFUSED BY ITS NUMBER, on the claim.
    std::string future = prefs_persist::to_text(hidden);
    const std::size_t at = future.find("\"version\":4");
    REQUIRE(at != std::string::npos);
    future.replace(at, 11, "\"version\":9");
    read = prefs_persist::from_text(future);
    CHECK_FALSE(read.outcome.accepted);
    CHECK(read.outcome.refusal == "prefs version 9 -- this Workshop reads versions 1 to 4");
    // The family's format identity: the other files' bytes are refused by name.
    CHECK_FALSE(prefs_persist::from_text(keymap_persist::to_text(Keymap{})).outcome.accepted);
}

TEST_CASE("a version-3 settings file is judged whole: a value Workshop's settings do not take, a list out of order, a pin twice, a lock with two values") {
    using prefs_persist::Prefs;
    struct Case {
        const char* what;
        std::function<void(Prefs&)> make;
        const char* said;
    };
    const std::vector<Case> cases = {
        {"a value Workshop's setting does not take",
         [](Prefs& p) {
             p.offices = {prefs_persist::OfficeSettingsKept{
                 prefs_persist::kWorkshopOffice,
                 {PaneSetting{prefs_persist::kFocusRingKey, {}, {}, std::string("dotted")}}}};
         },
         "workshop.focus.ring takes one of accent, plain, not a text `dotted`"},
        {"offices out of order",
         [](Prefs& p) {
             p.offices = {prefs_persist::OfficeSettingsKept{"z.office", {PaneSetting{"a", true, {}, {}}}},
                          prefs_persist::OfficeSettingsKept{"a.office", {PaneSetting{"a", true, {}, {}}}}};
         },
         "office `a.office` is listed twice or out of order"},
        {"a setting with two values",
         [](Prefs& p) {
             p.offices = {prefs_persist::OfficeSettingsKept{"a.office", {PaneSetting{"a", true, 1, {}}}}};
         },
         "office `a.office`: setting `a` holds more than one value -- a setting holds exactly one "
         "of a flag, a number or a text"},
        {"a pin twice",
         [](Prefs& p) { p.pins = {prefs_persist::PinList{"a/b", {"x", "x"}}}; },
         "pin list `a/b` pins `x` twice"},
        {"a pin list past its bound",
         [](Prefs& p) {
             p.pins = {prefs_persist::PinList{"a/b", {"k1", "k2", "k3", "k4", "k5", "k6", "k7", "k8", "k9"}}};
         },
         "pin list `a/b` holds more than 8 keys"},
        {"a lock with two values",
         [](Prefs& p) {
             p.locks = {prefs_persist::SettingLock{"a/b", "k", true, 2, std::nullopt, false}};
         },
         "the lock on `k` of `a/b` holds more than one value -- a lock holds one, `clear`, or none"},
    };
    for (const Case& c : cases) {
        CAPTURE(c.what);
        Prefs p;
        c.make(p);
        const prefs_persist::LoadedPrefs read = prefs_persist::from_text(prefs_persist::to_text(p));
        CHECK_FALSE(read.outcome.accepted);
        CHECK(read.outcome.refusal == c.said);
    }
    // A KEY OF WORKSHOP'S OFFICE THIS BUILD DOES NOT DECLARE IS KEPT AS IT IS: a newer file's.
    Prefs newer;
    newer.offices = {prefs_persist::OfficeSettingsKept{
        prefs_persist::kWorkshopOffice, {PaneSetting{"workshop.later", true, {}, {}}}}};
    const prefs_persist::LoadedPrefs read = prefs_persist::from_text(prefs_persist::to_text(newer));
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.kept == newer);
}

TEST_CASE("a version-2 prefs file is read as Workshop's own three settings, and its words are still a closed set") {
    const auto words = [](const char* titles, const char* band, const char* hotkeys) {
        return loom::compat::serialize(loom::to_value(prefs_persist::v2::WorkshopPrefs{
            prefs_persist::kFormat, prefs_persist::kWordsVersion, titles, band, hotkeys}));
    };
    prefs_persist::LoadedPrefs read = prefs_persist::from_text(words("hidden", "full", "short"));
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK_FALSE(read.prefs.titles_shown);
    CHECK(read.prefs.band_keys == KeySpelling::kFull);
    CHECK(read.prefs.hotkeys_keys == KeySpelling::kShort);
    // ...KEPT AS WORKSHOP'S OWN KEYS, so the next write is version 4.
    CHECK(prefs_persist::office_value(read.kept, prefs_persist::kWorkshopOffice,
                                      prefs_persist::kPaneTitlesKey)
              ->flag == false);
    CHECK(prefs_persist::to_text(read.kept).find("\"version\":4") != std::string::npos);
    // `default` is the hand-author's word for the code's answer, for each word.
    read = prefs_persist::from_text(words("default", "default", "default"));
    REQUIRE(read.outcome.accepted);
    CHECK(read.prefs.titles_shown == prefs_persist::kTitlesDefaultValue);
    CHECK(read.prefs.band_keys == prefs_persist::kBandKeysDefaultValue);
    CHECK(read.prefs.hotkeys_keys == prefs_persist::kHotkeysKeysDefaultValue);
    CHECK(read.kept == prefs_persist::Prefs{});
    // A word outside the closed set is refused naming what was found and what works.
    read = prefs_persist::from_text(words("visible", "short", "full"));
    CHECK_FALSE(read.outcome.accepted);
    CHECK(read.outcome.refusal.find("`visible`") != std::string::npos);
    CHECK(read.outcome.refusal.find("default, shown or hidden") != std::string::npos);
    read = prefs_persist::from_text(words("shown", "tiny", "full"));
    CHECK_FALSE(read.outcome.accepted);
    CHECK(read.outcome.refusal.find("`tiny`") != std::string::npos);
    CHECK(read.outcome.refusal.find("default, short or full") != std::string::npos);
    // ...and its own number is still its own: a version-2 file saying it is version 3 is refused.
    prefs_persist::v2::WorkshopPrefs lying{prefs_persist::kFormat, prefs_persist::kFormatVersion,
                                           "shown", "short", "full"};
    CHECK_FALSE(prefs_persist::from_text(loom::compat::serialize(loom::to_value(lying)))
                    .outcome.accepted);
}

TEST_CASE("a prefs file of the version that held the titles alone is read, its spellings the "
          "defaults") {
    // The file a weaver's Workshop wrote before the spellings existed: read as it says, never
    // refused for the words it could not have held, and never rewritten by the reading.
    const prefs_persist::v1::WorkshopPrefs old{prefs_persist::kFormat,
                                               prefs_persist::kTitlesOnlyVersion, "hidden"};
    const std::string text = loom::compat::serialize(loom::to_value(old));
    REQUIRE(text.find("\"version\":1") != std::string::npos);
    const prefs_persist::LoadedPrefs read = prefs_persist::from_text(text);
    REQUIRE(read.outcome.accepted);
    CHECK_FALSE(read.prefs.titles_shown);
    CHECK(read.prefs.band_keys == KeySpelling::kFull);
    CHECK(read.prefs.hotkeys_keys == KeySpelling::kFull);

    // ...and its own number is still its own: a version-1 file saying it is version 3 is refused.
    prefs_persist::v1::WorkshopPrefs lying = old;
    lying.format_version = prefs_persist::kFormatVersion;
    CHECK_FALSE(prefs_persist::from_text(loom::compat::serialize(loom::to_value(lying)))
                    .outcome.accepted);
}

TEST_CASE("a toggle writes the preference, and a reopened Workshop wears it") {
    TempDir dir("wux3-prefs");
    const std::string prefs = dir.file("root/workshop-prefs.json");
    {
        Live t;
        t.host.prefs_path = prefs;
        t.ready();
        REQUIRE(t.session().pane_titles);
        t.key(input::scan::kT);
        t.text("t");
        REQUIRE_FALSE(t.session().pane_titles);
        INFO(t.notice());
        // The toggle is the weaver stating the preference, so the toggle is the write --
        // and the write created the configuration root it landed in.
        REQUIRE(std::filesystem::exists(prefs));
        CHECK(slurp(prefs).find("\"workshop.pane-titles\"") != std::string::npos);
        // The notice still says what the toggle did, with no complaint added.
        CHECK(t.notice().rfind("pane titles hidden", 0) == 0);
        CHECK_FALSE(t.session().notice_is_bad);
    }
    // ANOTHER PROCESS, ANOTHER DAY: the preference is applied before the first paint.
    Live t;
    t.host.prefs_path = prefs;
    t.ready();
    CHECK_FALSE(t.session().pane_titles);
    // ...and toggling back rewrites the same file, the default kept as its absence.
    t.key(input::scan::kT);
    t.text("t");
    CHECK(t.session().pane_titles);
    CHECK(slurp(prefs).find("\"workshop.pane-titles\"") == std::string::npos);
    CHECK(prefs_persist::from_text(slurp(prefs)).outcome.accepted);
}

TEST_CASE("a settings file another hand changed since this Workshop read it is never written over") {
    TempDir dir("prefs-two-hands");
    const std::string prefs = dir.file("workshop-prefs.json");
    REQUIRE(prefs_persist::save_file(prefs, prefs_persist::Presentation{false}).accepted);
    Live t;
    t.host.prefs_path = prefs;
    t.ready();
    REQUIRE_FALSE(t.session().pane_titles);
    // ANOTHER WORKSHOP, OR A HAND, WRITES THE FILE AFTER THIS ONE READ IT.
    const std::string theirs = prefs_persist::to_text(
        prefs_persist::Presentation{false, KeySpelling::kShort, KeySpelling::kFull});
    spillout(prefs, theirs);
    t.key(input::scan::kT);
    t.text("t");
    // THE TOGGLE STANDS FOR THIS RUN, AND THE FILE IS LEFT AS THE OTHER HAND WROTE IT.
    CHECK(t.session().pane_titles);
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("the settings file changed since this Workshop read it") !=
          std::string::npos);
    CHECK(slurp(prefs) == theirs);
}

TEST_CASE("a restored hidden-titles preference still shows which pane holds the keyboard") {
    // The preference comes back from CONFIGURATION, and the pane holding the keyboard still
    // shows its title and its mark -- restoring a preference must not be a way to hide which
    // pane has the keys.
    TempDir dir("wux3-prefs-focus");
    const std::string prefs = dir.file("workshop-prefs.json");
    REQUIRE(prefs_persist::save_file(prefs, prefs_persist::Presentation{false}).accepted);

    PaneRig r;
    r.host.prefs_path = prefs;
    r.mount_workshop();
    r.ready();
    r.extent(100, 44);
    REQUIRE_FALSE(r.session().pane_titles); // restored, not toggled
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    const std::int64_t kind = seat_pane_open(r, seat, kHelloOffice, kHelloPane);
    REQUIRE(kind != kNoPaneKind);
    const auto shown_rows = [&](std::int64_t k) {
        return external_region_rows(r.shown(), external_body_rect(r.session(), k));
    };
    // Hidden titles: the pane is bare...
    CHECK(shown_rows(kind).at(0).find("Seat @") == std::string::npos);
    // ...until it takes the keyboard, when its identity auto-shows, mark and all, once the press
    // that moved the keys ends.
    click_body(r, kind);
    REQUIRE(keyboard_pane(r.session().panes) == kind);
    CHECK(shown_rows(kind).at(0).rfind(std::string(kTypingHere) + "Seat @", 0) == 0);
}

TEST_CASE("a refused prefs file is spoken, stands, and is never overwritten") {
    TempDir dir("wux3-prefs-bad");
    const std::string prefs = dir.file("workshop-prefs.json");
    spillout(prefs, "{ not a prefs file");
    Live t;
    t.host.prefs_path = prefs;
    t.ready();
    // THE REFUSAL IS A STANDING CONDITION, and the defaults stand. It is not
    // on the notice line because the notice line is for things that HAPPENED, and this is
    // a wall that is still there at the next launch.
    {
        const std::vector<Condition> now = attention_conditions(t.session());
        const Condition* wall = condition_by_key(now, kPrefsWallKey);
        REQUIRE(wall != nullptr);
        CHECK(wall->compact.find("defaults stand") != std::string::npos);
        CHECK(wall->role == gfx::role::kAlert);
    }
    CHECK(t.glance().find("preferences refused") != std::string::npos);
    CHECK(t.session().pane_titles);

    // A toggle changes the LIVE preference and deliberately writes nothing: Workshop
    // does not rewrite a file it could not understand.
    t.key(input::scan::kT);
    t.text("t");
    CHECK_FALSE(t.session().pane_titles);
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("will not be overwritten") != std::string::npos);
    CHECK(slurp(prefs) == "{ not a prefs file");
}

TEST_CASE("a prefs file version 3 cannot read is refused whole, and its bytes stand") {
    // A FILE OF THE RIGHT FORMAT HOLDING A VALUE WORKSHOP'S SETTINGS DO NOT TAKE: a wall like
    // a malformed one, the defaults in force and the file never rewritten.
    TempDir dir("prefs-bad-value");
    const std::string prefs = dir.file("workshop-prefs.json");
    prefs_persist::Prefs bad;
    bad.offices = {prefs_persist::OfficeSettingsKept{
        prefs_persist::kWorkshopOffice,
        {PaneSetting{prefs_persist::kBandKeysKey, {}, {}, std::string("tiny")}}}};
    const std::string text = prefs_persist::to_text(bad);
    spillout(prefs, text);
    Live t;
    t.host.prefs_path = prefs;
    t.ready();
    const std::vector<Condition> now = attention_conditions(t.session());
    const Condition* wall = condition_by_key(now, kPrefsWallKey);
    REQUIRE(wall != nullptr);
    CHECK(wall->detail.find("workshop.band-keys takes one of short, full") != std::string::npos);
    CHECK(t.session().band_keys == prefs_persist::kBandKeysDefaultValue);
    t.key(input::scan::kT);
    t.text("t");
    CHECK(slurp(prefs) == text);
}

TEST_CASE("no prefs path means the preference lives exactly as long as the run") {
    // `--isolated`'s promise, at the weave: an empty path reads nothing and writes
    // nothing, and the toggle still works -- silently local, complaint-free.
    Live t;
    t.ready();
    t.key(input::scan::kT);
    t.text("t");
    CHECK_FALSE(t.session().pane_titles);
    CHECK(t.notice() ==
          "pane titles hidden -- a pane holding the keyboard still shows its own");
    CHECK_FALSE(t.session().notice_is_bad);
}

// ---- The window file: the placement ----------------------------------------------------

TEST_CASE("a window file with a placement round-trips byte-identically") {
    window_persist::Placement place;
    place.known = true;
    place.x = -1200; // a monitor left of the primary is negative territory, legitimately
    place.y = 340;
    place.maximized = true;
    const std::string first = window_persist::to_text(cells_px(120), cells_px(44), place);
    for (const char* fragment :
         {"\"placement\":", "\"mode\":\"desktop\"", "\"x\":\"-1200\"", "\"y\":\"340\"",
          "\"window\":\"maximized\""}) {
        CHECK_MESSAGE(first.find(fragment) != std::string::npos, fragment);
    }
    const window_persist::LoadedWindow read = window_persist::from_text(first);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.honoured);
    CHECK(read.viewport_w == cells_px(120));
    CHECK(read.viewport_h == cells_px(44));
    CHECK(read.placement.known);
    CHECK(read.placement.x == -1200);
    CHECK(read.placement.y == 340);
    CHECK(read.placement.maximized);
    CHECK(window_persist::to_text(read.viewport_w, read.viewport_h, read.placement) == first);

    // THE ABSENCE HAS ONE SPELLING: no placement writes `none` over zeros and `normal`.
    const std::string none =
        window_persist::to_text(cells_px(120), cells_px(44), window_persist::Placement{});
    CHECK(none.find("\"mode\":\"none\"") != std::string::npos);
    CHECK(none.find("\"x\":\"0\"") != std::string::npos);
    CHECK(none.find("\"window\":\"normal\"") != std::string::npos);
    CHECK_FALSE(window_persist::from_text(none).placement.known);
}

TEST_CASE("the placement's words are judged; its coordinates are not") {
    window_persist::Placement place;
    place.known = true;
    place.x = 100;
    place.y = 60;
    const std::string good = window_persist::to_text(cells_px(100), cells_px(30), place);

    // A mode word outside the closed set refuses the file, naming both sets.
    std::string bad = good;
    bad.replace(bad.find("\"mode\":\"desktop\""), std::string("\"mode\":\"desktop\"").size(),
                "\"mode\":\"monitor\"");
    window_persist::LoadedWindow read = window_persist::from_text(bad);
    CHECK_FALSE(read.outcome.accepted);
    CHECK(read.outcome.refusal.find("`monitor`") != std::string::npos);
    CHECK(read.outcome.refusal.find("none or desktop") != std::string::npos);

    // A window word outside its set, the same.
    bad = good;
    bad.replace(bad.find("\"window\":\"normal\""),
                std::string("\"window\":\"normal\"").size(), "\"window\":\"fullscreen\"");
    read = window_persist::from_text(bad);
    CHECK_FALSE(read.outcome.accepted);
    CHECK(read.outcome.refusal.find("`fullscreen`") != std::string::npos);
    CHECK(read.outcome.refusal.find("normal or maximized") != std::string::npos);

    // An absent placement carrying a coordinate is a spelling nobody means: refused,
    // and the refusal says both ways to fix it.
    bad = window_persist::to_text(cells_px(100), cells_px(30), window_persist::Placement{});
    const std::string none_x = "\"mode\":\"none\",\"x\":\"0\"";
    bad.replace(bad.find(none_x), none_x.size(), "\"mode\":\"none\",\"x\":\"7\"");
    read = window_persist::from_text(bad);
    CHECK_FALSE(read.outcome.accepted);
    CHECK(read.outcome.refusal.find("carries no coordinates") != std::string::npos);

    // A COORDINATE IS ANOTHER MACHINE'S DESKTOP TRUTH, accepted unjudged: only the
    // medium at restore time can judge one, and refusing here would cost the desk.
    place.x = 1000000;
    place.y = -1000000;
    read = window_persist::from_text(window_persist::to_text(cells_px(100), cells_px(30), place));
    REQUIRE(read.outcome.accepted);
    CHECK(read.placement.x == 1000000);
    CHECK(read.placement.y == -1000000);
}

TEST_CASE("a version-2 session still loads, its placement reading as absence") {
    session_history::v2::WorkshopSession old;
    old.format = session_persist::kFormat;
    old.format_version = 2;
    old.viewport = session_history::v6::WorkshopViewport{110, 38};
    old.desk = v3_desk(arranged_desk("Yesterday"));
    const std::string bytes = loom::compat::serialize(loom::to_value(old));

    op::Catalog conversions;
    REQUIRE(conversions.mount("suite", session_history::conversions()));
    const session_persist::LoadedSession read =
        session_persist::from_text(bytes, &conversions);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.present);
    CHECK(live_layout(read) == materialized(arranged_desk("Yesterday")));
    // ...AND ITS ROOM IS THE WINDOW FILE'S, imported from it once.
    const window_persist::LoadedWindow room = window_persist::import_from_session_text(bytes);
    CHECK(room.honoured);
    CHECK(room.viewport_w == cells_px(110));
    CHECK(room.viewport_h == cells_px(38));
    CHECK_FALSE(room.placement.known);

    // The next close writes the current version, byte-stable thereafter.
    const std::string saved = session_text(read.layouts, read.active);
    CHECK(saved.find("\"format_version\":\"6\"") != std::string::npos);
    CHECK(session_persist::from_text(saved).outcome.accepted);
}

// ---- The weave: remember, offer back, and keep the normal room honest -----------------

TEST_CASE("the desk remembers where its window sat, and offers it back") {
    TempDir dir("wux3-place");
    const std::string session = dir.file("session.json");
    {
        Live t;
        t.host.session_path = session;
        t.ready();
        // The medium notices its window and says so; Workshop remembers, opaquely.
        t.publish(loom::to_value(gfx::SurfacePlacement{240, 180, false}));
        CHECK(t.session().placement_known);
        CHECK(t.session().place_x == 240);
        CHECK(t.session().place_y == 180);
        t.key(input::scan::kQ);
    }
    const std::string window = slurp(window_persist::path_beside(session));
    CHECK(window.find("\"mode\":\"desktop\"") != std::string::npos);
    CHECK(window.find("\"x\":\"240\"") != std::string::npos);
    CHECK(slurp(session).find("placement") == std::string::npos);

    // ANOTHER PROCESS: the remembered placement is offered to whoever holds the skin
    // role, exactly once, before any medium has said anything.
    Live t;
    t.host.session_path = session;
    t.on_os_window();
    t.ready();
    REQUIRE(t.windows.placed().size() == 1);
    CHECK(t.windows.placed()[0].x == 240);
    CHECK(t.windows.placed()[0].y == 180);
    CHECK_FALSE(t.windows.placed()[0].maximized);
    // ...and the session remembers it even if no medium ever answers.
    CHECK(t.session().placement_known);
}

TEST_CASE("the window file keeps the OS window's own room beside its placement, and a run in the "
          "terminal window writes it back as it read it, as it does the placement") {
    // ⚔ MUTATION: the save writing this run's room as the OS window's whatever the window -- a run
    // in the terminal window leaves its own room where the OS window's was.
    TempDir dir("os-room-kept");
    const std::string session = dir.file("session.json");
    {
        Live t;
        t.host.session_path = session;
        t.on_os_window();
        t.ready();
        t.publish(loom::to_value(gfx::SurfacePlacement{240, 180, false}));
        t.extent_px(1180, 760, 8, 18);
        CHECK(t.session().normal_w == 1180);
        t.key(input::scan::kQ);
    }
    window_persist::LoadedWindow read = window_persist::load_file(window_persist::path_beside(session));
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.placement.room_w == 1180);
    CHECK(read.placement.room_h == 760);
    {
        Live t;
        t.host.session_path = session;
        t.ready(); // the terminal window
        t.extent(100, 34);
        t.key(input::scan::kQ);
    }
    read = window_persist::load_file(window_persist::path_beside(session));
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.viewport_w == cells_px(100)); // the desk's room is the terminal's...
    CHECK(read.placement.room_w == 1180);    // ...and the OS window's is its own, as it was
    CHECK(read.placement.room_h == 760);
    CHECK(read.placement.known);
    CHECK(read.placement.x == 240);
}

TEST_CASE("a restore in the OS window shapes it to its own room, a smaller one taken, then places "
          "it; a maximized one is shaped before it is maximized; the terminal window keeps its own") {
    // ⚔ MUTATION: the restore placing the window as soon as it reads the file -- a maximize lands
    // before the shaping, and restoring the window down returns the room it opened at.
    TempDir dir("os-room-shaped");
    const std::string session = dir.file("session.json");
    window_persist::Placement place;
    place.known = true;
    place.x = 64;
    place.y = 48;
    place.maximized = true;
    place.room_w = 1280; // smaller than the 1600 by 900 the OS window opens at
    place.room_h = 720;
    REQUIRE(save_last_run(session, one_layout(arranged_desk("D")), 0, cells_px(120), cells_px(40),
                          place)
                .accepted);
    {
        Live t;
        t.host.session_path = session;
        t.on_os_window();
        t.ready();
        REQUIRE(t.windows.shaped().size() == 1);
        CHECK(t.windows.shaped()[0] == std::pair<std::int64_t, std::int64_t>{1280, 720});
        REQUIRE(t.windows.placed().size() == 1);
        CHECK(t.windows.placed()[0].maximized);
        // SHAPED FIRST, then placed: the maximize waits for the room the window was given.
        CHECK(t.windows.window_acts() == std::vector<std::string>{"shape", "place"});
        // ...and the desk is laid out in that room, which the session keeps as the normal one.
        CHECK(t.session().normal_w == 1280);
        CHECK(t.session().normal_h == 720);
    }
    // THE TERMINAL WINDOW KEEPS ITS OWN: nothing is shaped, and its room is the terminal's.
    Live term;
    term.host.session_path = session;
    term.ready();
    CHECK(term.windows.shaped().empty());
    CHECK(term.session().window_room_w == 1280);
    // A ROOM OUTSIDE THE SCREEN'S BAND IS DECLINED, never clamped, and said.
    place.room_w = 4;
    place.room_h = 3;
    REQUIRE(save_last_run(session, one_layout(arranged_desk("D")), 0, cells_px(120), cells_px(40),
                          place)
                .accepted);
    Live small;
    small.host.session_path = session;
    small.on_os_window();
    small.ready();
    CHECK(small.windows.shaped().empty());
    CHECK(small.notice().find("the saved window size 4x3 pixels is not one this Workshop opens at") !=
          std::string::npos);
}

TEST_CASE("a session with no placement offers nothing") {
    TempDir dir("wux3-place-none");
    const std::string session = dir.file("session.json");
    REQUIRE(save_last_run(session, one_layout(arranged_desk("D")), 0, cells_px(110), cells_px(38),
                          window_persist::Placement{})
                .accepted);
    Live t;
    t.host.session_path = session;
    t.on_os_window();
    t.ready();
    CHECK(t.windows.placed().empty());
    CHECK_FALSE(t.session().placement_known);
}

TEST_CASE("a maximized close remembers the NORMAL room beside the maximized state") {
    TempDir dir("wux3-max");
    const std::string session = dir.file("session.json");
    {
        Live t;
        t.host.session_path = session;
        t.ready();
        // The weaver sizes their normal window...
        t.extent_px(cells_px(120), cells_px(40), 0, 0);
        CHECK(t.session().normal_w == cells_px(120));
        CHECK(t.session().normal_h == cells_px(40));
        // ...then maximizes. The medium reports placement BEFORE the grown extent
        // (skin.hpp's pinned order), so the gate is closed when the big room arrives.
        t.publish(loom::to_value(gfx::SurfacePlacement{300, 200, true}));
        t.extent_px(cells_px(200), cells_px(80), 0, 0);
        CHECK(t.session().screen_w == cells_px(200)); // the live screen follows the window
        CHECK(t.session().normal_w == cells_px(120)); // the remembered normal room does not
        CHECK(t.session().normal_h == cells_px(40));
        t.key(input::scan::kQ);
    }
    const window_persist::LoadedWindow read = window_persist::load_file(window_persist::path_beside(session));
    REQUIRE(read.outcome.accepted);
    CHECK(read.viewport_w == cells_px(120));
    CHECK(read.viewport_h == cells_px(40));
    CHECK(read.placement.known);
    CHECK(read.placement.maximized);
    CHECK(read.placement.x == 300);
    CHECK(read.placement.y == 200);
}

TEST_CASE("unmaximizing reopens the gate, and the normal room tracks again") {
    Live t;
    t.ready();
    t.extent_px(cells_px(120), cells_px(40), 0, 0);
    t.publish(loom::to_value(gfx::SurfacePlacement{300, 200, true}));
    t.extent_px(cells_px(200), cells_px(80), 0, 0);
    REQUIRE(t.session().normal_w == cells_px(120));
    // The weaver unmaximizes: placement first, then the shrunken extent.
    t.publish(loom::to_value(gfx::SurfacePlacement{300, 200, false}));
    t.extent_px(cells_px(130), cells_px(44), 0, 0);
    CHECK(t.session().normal_w == cells_px(130));
    CHECK(t.session().normal_h == cells_px(44));
}

TEST_CASE("a run whose medium reports no placement RETAINS the remembered one") {
    // A terminal run between two graphical runs must not cost the weaver their window
    // position: the TUI has no desktop fact, makes no claim, and carries the memory.
    TempDir dir("wux3-retain");
    const std::string session = dir.file("session.json");
    window_persist::Placement place;
    place.known = true;
    place.x = 640;
    place.y = 220;
    REQUIRE(
        save_last_run(session, one_layout(arranged_desk("D")), 0, cells_px(110), cells_px(38), place).accepted);
    {
        Live t;
        t.host.session_path = session;
        t.ready();
        // A terminal-shaped run: extents arrive, placements never do.
        t.extent_px(cells_px(100), cells_px(33), 0, 0);
        t.key(input::scan::kQ);
    }
    const window_persist::LoadedWindow read = window_persist::load_file(window_persist::path_beside(session));
    REQUIRE(read.outcome.accepted);
    // The new room was remembered -- the restored maximized flag from ANOTHER run's
    // window must not stop this run's viewport tracking...
    CHECK(read.viewport_w == cells_px(100));
    CHECK(read.viewport_h == cells_px(33));
    // ...and the placement crossed unchanged.
    CHECK(read.placement.known);
    CHECK(read.placement.x == 640);
    CHECK(read.placement.y == 220);
}

// ============================================================================
// Looking at a projection is not authoring one
// ============================================================================

namespace {

/// A GEOMETRY NO MEDIUM HERE CAN SAY THE SAME WAY TWICE. Each number is a whole number of
/// pixels and none is a whole number of cells, so a green produced by values that happen to
/// divide evenly is impossible here.
inline constexpr std::int64_t kHostilePlaceX = 77;  //  6 cells + 5 px
inline constexpr std::int64_t kHostilePlaceY = 53;  //  4 cells + 5 px
inline constexpr std::int64_t kHostileWidth = 417;  // 34 cells + 9 px
inline constexpr std::int64_t kHostileHeight = 233; // 19 cells + 5 px

static_assert(kHostilePlaceX % gfx::kCellPx != 0, "must not divide evenly");
static_assert(kHostilePlaceY % gfx::kCellPx != 0, "must not divide evenly");
static_assert(kHostileWidth % gfx::kCellPx != 0, "must not divide evenly");
static_assert(kHostileHeight % gfx::kCellPx != 0, "must not divide evenly");

/// A desk holding exactly that, authored through the ordinary value doors.
inline Setup hostile_desk() {
    Setup s;
    s.name = "Hostile";
    REQUIRE(add_pane(s, ref_of(stock::kKind)));
    REQUIRE(
        author_pane_place(s, ref_of(stock::kKind), kHostilePlaceX, kHostilePlaceY).accepted);
    REQUIRE(author_pane_size(s, ref_of(stock::kKind),
                             PaneSize{pane_unit::kPixels, kHostileWidth},
                             PaneSize{pane_unit::kPixels, kHostileHeight})
                .accepted);
    return s;
}

} // namespace

TEST_CASE("a read-only visit through the other medium writes the SAME BYTES") {
    // THE FALSIFIER. A geometry that cannot round-trip through a terminal is authored; a whole
    // session is spent LOOKING at it through both media -- the arrangement opened, the pane
    // stepped to, its geometry read in one unit and then the other -- and the file closing
    // writes is compared BYTE FOR BYTE with the same session's file having never crossed a
    // medium. The runs differ only in the medium's device unit; a projection that authored
    // would show as the difference.
    TempDir dir("wux6-project");
    const std::string never = dir.file("never-crossed.json");
    const std::string crossed = dir.file("crossed.json");

    {
        Live t;
        t.host.session_path = never;
        t.ready();
        t.extent_px(cells_px(140), cells_px(44), 0, 0, 0);
        stage_desk(t, [&](Setup& desk) {
            desk = hostile_desk();
        });
        t.key(input::scan::kQ);
    }
    {
        Live t;
        t.host.session_path = crossed;
        t.ready();
        t.extent_px(cells_px(140), cells_px(44), 0, 0, 0);
        stage_desk(t, [&](Setup& desk) {
            desk = hostile_desk();
        });

        // LOOK AT IT IN CELLS. Every number is a projection this medium cannot say.
        enter_arrange_desk(t);
        for (int i = 0; i < 32 && t.session().arrange.pane != ref_of(stock::kKind); ++i) {
            t.key(input::scan::kTab);
        }
        REQUIRE(t.session().arrange.pane == ref_of(stock::kKind));
        INFO(t.notice());
        CHECK(t.notice().find("~34x~19 cells") != std::string::npos);
        CHECK(t.notice().find("(~ projected)") != std::string::npos);

        // NOW THE SAME DESK ON THE SHIPPED WINDOW, at the same room -- so the ONLY thing
        // that changed about this run is which unit the weaver is reading in.
        t.extent_px(cells_px(140), cells_px(44), 8, 18, gfx::kCellPx);
        t.key(input::scan::kTab);
        for (int i = 0; i < 32 && t.session().arrange.pane != ref_of(stock::kKind); ++i) {
            t.key(input::scan::kTab);
        }
        INFO(t.notice());
        CHECK(t.notice().find("@77,53 417x233 px") != std::string::npos);
        CHECK(t.notice().find("(~ projected)") == std::string::npos);

        // ...AND BACK, which is the direction that would show a write having happened.
        t.extent_px(cells_px(140), cells_px(44), 0, 0, 0);
        t.key(input::scan::kTab);
        for (int i = 0; i < 32 && t.session().arrange.pane != ref_of(stock::kKind); ++i) {
            t.key(input::scan::kTab);
        }
        CHECK(t.notice().find("~34x~19 cells") != std::string::npos);
        t.key(input::scan::kEscape);
        t.key(input::scan::kQ);
    }

    CHECK(slurp(crossed) == slurp(never));

    // AND THE AUTHORED NUMBERS ARE THE ONES THAT WERE WRITTEN -- byte identity between two
    // files that were both wrong the same way would prove nothing.
    const session_persist::LoadedSession read = session_persist::load_file(crossed);
    REQUIRE(read.outcome.accepted);
    const SetupPane* row = pane_of(live_layout(read), ref_of(stock::kKind));
    REQUIRE(row != nullptr);
    CHECK(row->place.x == kHostilePlaceX);
    CHECK(row->place.y == kHostilePlaceY);
    CHECK(row->width.amount == kHostileWidth);
    CHECK(row->height.amount == kHostileHeight);
    // NOT the projected answers, which is what a medium writing back would have left.
    CHECK(row->width.amount != cells_px(34));
    CHECK(row->height.amount != cells_px(19));
}

TEST_CASE("the medium's device unit reaches no durable file") {
    // It is the text metric's own rule, for the text metric's own reason: how big a cell
    // is belongs to whichever medium opens the face, is republished every run, and would
    // be a stale claim about somebody else's monitor the moment it was written down.
    TempDir dir("wux6-nofile");
    const std::string path = dir.file("session.json");
    {
        Live t;
        t.host.session_path = path;
        t.ready();
        t.extent_px(cells_px(140), cells_px(44), 8, 18, gfx::kCellPx);
        REQUIRE(t.session().cell_px == gfx::kCellPx);
        stage_desk(t, [&](Setup& desk) {
            desk = hostile_desk();
        });
        t.key(input::scan::kQ);
    }
    // THE SESSION AND THE WINDOW FILE BESIDE IT. The session's own format is version 12, which
    // is no cell: its spellings go before the files are searched.
    std::string text = slurp(path) + slurp(window_persist::path_beside(path));
    for (const std::string own : {"\"version\":12", "\"format_version\":\"12\""}) {
        for (std::size_t at = text.find(own); at != std::string::npos; at = text.find(own)) {
            text.erase(at, own.size());
        }
    }
    INFO(text);
    CHECK(text.find("cell_px") == std::string::npos);
    CHECK(text.find("\"12\"") == std::string::npos);

    // AND A RESTORE HANDS THIS RUN'S UNIT STRAIGHT BACK. A file remembers the ROOM; what
    // the medium said about its own units is THIS run's, and the restore -- which adopts a
    // remembered viewport through the same door -- must not reset it to the character
    // reading and leave a weaver on a window reading cells.
    Live t;
    t.host.session_path = path;
    t.on_os_window(); // the medium that grows to the room restored
    t.ready();
    t.window_settles();
    CHECK(t.session().screen_w == cells_px(140));                      // the restore DID land...
    CHECK(t.session().cell_px == gfx::kCellPx);     // ...and cost nothing
    CHECK(std::string(geometry_unit(t.session().cell_px)) == "px");
}

TEST_CASE("a restored maximized flag alone does not gate this run's viewport") {
    TempDir dir("wux3-stale-max");
    const std::string session = dir.file("session.json");
    window_persist::Placement place;
    place.known = true;
    place.x = 10;
    place.y = 10;
    place.maximized = true; // last run closed maximized...
    REQUIRE(
        save_last_run(session, one_layout(arranged_desk("D")), 0, cells_px(110), cells_px(38), place).accepted);
    Live t;
    t.host.session_path = session;
    t.ready();
    // ...but THIS run's medium never says so (a terminal), so resizes track normally.
    t.extent_px(cells_px(100), cells_px(33), 0, 0);
    CHECK(t.session().normal_w == cells_px(100));
    CHECK(t.session().normal_h == cells_px(33));
}


// ============================================================================
// ---- what the layout shelf means for the two files -------------------------
// A Setup file is ONE named desk arrangement: `s` and `r` act on the LIVE layout alone, and
// neither touches the rest of the shelf.

namespace {

/// Step the live layout one along the run, as a weaver does.
void layout_next(Live& t) {
    const Gesture g = t.session().keymap.gesture_of(Act::kLayoutNext);
    t.key(g.scancode, g.modifiers);
}

/// Make a fresh blank layout and stand on it, as a weaver does.
void layout_new(Live& t) {
    const Gesture g = t.session().keymap.gesture_of(Act::kLayoutNew);
    t.key(g.scancode, g.modifiers);
}

/// Every layout this Workshop holds, in the weaver's order.
std::vector<Setup> shelf_of(const Live& t) {
    std::vector<Setup> out;
    for (std::size_t i = 0; i < layout_count(t.session().setup); ++i) {
        out.push_back(layout_at(t.run(), i));
    }
    return out;
}

} // namespace

TEST_CASE("`s` writes the live layout and leaves the shelf alone") {
    TempDir dir("wux9-save");
    Live t;
    t.host.setup_path = dir.file("s.json");

    layout_new(t);
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(add_pane(desk, ref_of(stock::kKind)));
    });
    REQUIRE(layout_count(t.session().setup) == 2);
    const std::vector<Setup> before = shelf_of(t);

    name_setup(t, "Second");
    CHECK(t.notice().find("saved setup \"Second\"") == 0);

    // THE FILE HOLDS ONE DESK -- the live one -- and its meaning is unchanged: the
    // format's own reader gets exactly the arrangement a weaver was standing in.
    const setup_persist::LoadedSetup written = setup_persist::load_file(t.host.setup_path);
    REQUIRE(written.outcome.accepted);
    CHECK(written.setup == t.session().setup.active);
    CHECK(written.setup.name == "Second");
    CHECK(has_pane(written.setup, ref_of(stock::kKind)));
    // ...AND THE SHELF IS BYTE-FOR-BYTE WHAT IT WAS, name included.
    const std::vector<Setup> after = shelf_of(t);
    REQUIRE(after.size() == before.size());
    CHECK(after[0] == before[0]);
    // The verdict is about the LIVE layout against its own artifact, and says so.
    CHECK((live_status(t.run()) == setup_link::kCurrent));
    CHECK(setup_row(t.shown(), screen_of(t.session())).find("| current") !=
          std::string::npos);
    // ...AND THE SHELVED LAYOUT GAINED NO ASSOCIATION: saving one desk is not a
    // reason for an unrelated one to acquire a relationship to that file.
    CHECK(link_at(t.run(), 0).path.empty());
}

TEST_CASE("`r` restores into the live layout and clears no shelf") {
    TempDir dir("wux9-restore");
    const Setup named = setup_of("From file", {second::kKind, stock::kKind});
    REQUIRE(setup_persist::save_file(dir.file("s.json"), named).accepted);

    Live t;
    t.host.setup_path = dir.file("s.json");
    layout_new(t);
    stage_desk(t, [&](Setup& desk) {
        desk.name = "Scratch";
    });
    REQUIRE(layout_count(t.session().setup) == 2);
    const Setup shelved = layout_at(t.run(), 0);
    const std::size_t at = t.session().setup.active_at;

    t.key(input::scan::kR);
    CHECK(t.notice().find("restored setup \"From file\"") == 0);

    // THE LIVE LAYOUT BECAME THE FILE'S DESK, IN ITS OWN POSITION IN THE RUN...
    CHECK(t.session().setup.active == named);
    CHECK(t.session().setup.active_at == at);
    CHECK(layout_count(t.session().setup) == 2);
    // ...AND NOTHING ELSE ON THE SHELF MOVED. A restore that replaced the run would be
    // one file quietly deciding how many desks a weaver has.
    CHECK(layout_at(t.run(), 0) == shelved);

    // A REFUSED RESTORE COSTS THE NOTICE AND NOTHING ELSE, shelf included.
    REQUIRE(persist::write_file(dir.file("s.json"), "{ not a setup").accepted);
    const std::vector<Setup> before = shelf_of(t);
    t.key(input::scan::kR);
    CHECK(t.session().notice_is_bad);
    const std::vector<Setup> after = shelf_of(t);
    REQUIRE(after.size() == before.size());
    for (std::size_t i = 0; i < after.size(); ++i) {
        CAPTURE(i);
        CHECK(after[i] == before[i]);
    }
}

TEST_CASE("the whole layout run rides the session, and comes back") {
    // THE SESSION CARRIES THE WHOLE RUN: every layout survives the close, not only the live
    // desk.
    TempDir dir("wux10-session");
    const std::string session = dir.file("session.json");
    Setup second_authored;

    {
        Live t;
        t.host.session_path = session;
        t.host.setup_path = dir.file("s.json");
        t.ready();
        t.extent_px(cells_px(120), cells_px(44));

        stage_desk(t, [&](Setup& desk) {
            desk.name = "First";
        });
        layout_new(t);
        stage_desk(t, [&](Setup& desk) {
            desk.name = "Second";
        });
        stage_desk(t, [&](Setup& desk) {
            REQUIRE(add_pane(desk, ref_of(stock::kKind)));
        });
        layout_new(t);
        stage_desk(t, [&](Setup& desk) {
            desk.name = "Third";
        });
        REQUIRE(layout_count(t.session().setup) == 3);
        // Stand on the middle one, so what returns is neither the first nor the last -- and
        // `active_at == shelved.size()` (where `layout.new` leaves a weaver) cannot pass for
        // the position that was actually saved.
        layout_next(t);
        layout_next(t);
        REQUIRE(t.session().setup.active.name == "Second");
        REQUIRE(t.session().setup.active_at == 1);
        second_authored = t.session().setup.active;

        t.key(input::scan::kQ);
        REQUIRE(t.host.quit);
    }

    // THE FILE CARRIES THE RUN AND THE POSITION.
    const session_persist::LoadedSession read = session_persist::load_file(session);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    REQUIRE(read.layouts.size() == 3);
    CHECK(read.active == 1);
    CHECK(read.layouts[0].desk.name == "First");
    CHECK(read.layouts[1].desk.name == "Second");
    CHECK(read.layouts[2].desk.name == "Third");
    CHECK(live_layout(read) == second_authored);
    CHECK(has_pane(read.layouts[1].desk, ref_of(stock::kKind)));
    CHECK_FALSE(has_pane(read.layouts[0].desk, ref_of(stock::kKind)));
    CHECK(slurp(session).find("\"version\":12") != std::string::npos);
    CHECK(slurp(session).find("\"layout_count\":") != std::string::npos);

    // AND THE NEXT RUN COMES BACK ON ALL THREE, standing on the one it left on.
    Live back;
    back.host.session_path = session;
    back.host.setup_path = dir.file("elsewhere.json");
    back.ready();
    CHECK(layout_count(back.session().setup) == 3);
    CHECK(back.session().setup.active_at == 1);
    CHECK(back.session().setup.active == second_authored);
    CHECK(layout_at(back.run(), 0).name == "First");
    CHECK(layout_at(back.run(), 2).name == "Third");
    // ...and the notice says so, counting the tabs the way a weaver counts them.
    CHECK(back.notice().find("reopened your last desk") == 0);
    CHECK(back.notice().find("(2 of 3 layouts)") != std::string::npos);
    // THE LIVE LAYOUT IS THE ONE THE PANES AGREE WITH -- `apply_setup` is still the one
    // membership door, and a dormant layout opened nothing.
    CHECK(back.session().panes.has(stock::kKind));
    CHECK(back.session().setup.active_at == 1);
}

TEST_CASE("crossing media never writes a device value into any layout") {
    TempDir dir("wux9-media");
    Live t;
    t.host.setup_path = dir.file("s.json");
    t.ready();
    t.extent_px(cells_px(120), cells_px(44));

    // TWO LAYOUTS WITH DISTINCT FINE-LATTICE GEOMETRY -- one of them deliberately NOT on
    // a cell boundary, which is the value a character medium cannot say and must not
    // round on its way through.
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(add_pane(desk, ref_of(second::kKind)));
    });
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(author_pane_place(desk, ref_of(second::kKind),
                                  gfx::px_of_cells(3) + 17, gfx::px_of_cells(4) + 5)
                    .accepted);
    });
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(author_pane_size(desk, ref_of(second::kKind),
                                 PaneSize{pane_unit::kPixels, gfx::px_of_cells(20) + 11},
                                 PaneSize{pane_unit::kPixels, gfx::px_of_cells(9) + 23})
                    .accepted);
    });
    stage_desk(t, [&](Setup& desk) {
        desk.name = "Fine";
    });
    const Setup fine = t.session().setup.active;

    layout_new(t);
    stage_desk(t, [&](Setup& desk) {
        desk.name = "Whole";
    });
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(add_pane(desk, ref_of(second::kKind)));
    });
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(author_pane_place(desk, ref_of(second::kKind),
                                  gfx::px_of_cells(6), gfx::px_of_cells(2))
                    .accepted);
    });
    const Setup whole = t.session().setup.active;
    REQUIRE(fine != whole);

    // VIEW BOTH ON A WINDOW WITH A REAL FACE AND ITS OWN PIXEL, then on a terminal, then
    // back -- switching, repainting and reading all the way.
    for (int round = 0; round < 2; ++round) {
        CAPTURE(round);
        t.extent_px(cells_px(120), cells_px(44), 8, 18, 12);
        layout_next(t);
        (void)paint(t.session());
        layout_next(t);
        (void)paint(t.session());
        t.extent_px(cells_px(120), cells_px(44), 0, 0, 0);
        layout_next(t);
        (void)paint(t.session());
        layout_next(t);
        (void)paint(t.session());
    }

    // EVERY LAYOUT'S AUTHORED NUMBERS ARE THE ONES THAT WENT IN. Looking is not
    // authoring, and a switch is not a geometry conversion.
    CHECK(layout_at(t.run(), 0) == fine);
    CHECK(layout_at(t.run(), 1) == whole);

    // ...AND SAVING EACH ONE WRITES THE SAME BYTES AS A RUN THAT NEVER CROSSED.
    CHECK(setup_persist::to_text(layout_at(t.run(), 0)) ==
          setup_persist::to_text(fine));
    CHECK(setup_persist::to_text(layout_at(t.run(), 1)) ==
          setup_persist::to_text(whole));
    // No projected spelling reached the bytes: a device unit has no field in this format.
    CHECK(setup_persist::to_text(fine).find("px") == std::string::npos);
    CHECK(setup_persist::to_text(fine).find("cells") == std::string::npos);
}

// =============================================================================
// YESTERDAY'S SESSION, THROUGH A CONVERSION THIS RUN HAPPENS TO HAVE
// =============================================================================
// A reader knowing only its own shape still opens an old session file: a mounted artifact
// contributes the conversion through the operator catalog, and without it old bytes get an
// honest refusal and nothing more. The edges themselves are `test_operator_migration.cpp`'s.

namespace {

/// A version-1 session file, exactly as a version-1 Workshop wrote one: whole-cell geometry,
/// a nested version-2 desk, and no placement because nothing could say one.
session_history::v1::WorkshopSession old_v1_session(const char* name, std::int64_t w,
                                                    std::int64_t h) {
    session_history::v1::WorkshopSession old;
    old.format = session_persist::kFormat;
    old.format_version = 1;
    old.viewport = session_history::v6::WorkshopViewport{w, h};
    old.desk.format = setup_persist::kFormat;
    old.desk.format_version = 2;
    old.desk.name = name;
    setup_persist::v2::WorkshopSetupPane info;
    info.provider = "zengine.workshop";
    info.pane = "info";
    info.place = setup_persist::v2::WorkshopPanePlace{"cells", 3, 2};
    info.width = setup_persist::v2::WorkshopPaneSize{"cells", 28};
    info.height = setup_persist::v2::WorkshopPaneSize{"default", 0};
    info.front = 0;
    old.desk.panes.push_back(info);
    setup_persist::v2::WorkshopSetupPane builder;
    builder.provider = "zengine.workshop";
    builder.pane = "builder";
    builder.place = setup_persist::v2::WorkshopPanePlace{"default", 0, 0};
    builder.width = setup_persist::v2::WorkshopPaneSize{"cells", 40};
    builder.height = setup_persist::v2::WorkshopPaneSize{"pixels", 220};
    builder.front = 1;
    old.desk.panes.push_back(builder);
    return old;
}

/// The same vintage's successor: the current desk shape, still no placement.
session_history::v2::WorkshopSession old_v2_session(const char* name, std::int64_t w,
                                                    std::int64_t h) {
    session_history::v2::WorkshopSession old;
    old.format = session_persist::kFormat;
    old.format_version = 2;
    old.viewport = session_history::v6::WorkshopViewport{w, h};
    old.desk = v3_desk(arranged_desk(name));
    return old;
}

std::string as_text(const session_history::v1::WorkshopSession& old) {
    return loom::compat::serialize(loom::to_value(old));
}
std::string as_text(const session_history::v2::WorkshopSession& old) {
    return loom::compat::serialize(loom::to_value(old));
}

/// A catalog holding the real shipped conversion artifact -- the file a weaver's Workshop
/// mounts, not a stand-in for it.
struct MountedHistory {
    op::Catalog catalog;
    op::MountResult mounted;

    MountedHistory() { mounted = op::mount_provider(catalog, SESSION_HISTORY_SO); }
};

} // namespace

TEST_CASE("the shipped artifact supplies exactly the conventional edges") {
    MountedHistory history;
    REQUIRE_MESSAGE(history.mounted.ok, history.mounted.reason);
    CHECK(history.mounted.provider == "zengine.workshop.session_history");
    // THE IDENTITIES ARE DERIVED FROM THE EDGES, so this list is a reading of the
    // convention rather than a list somebody typed twice.
    const std::vector<std::string> supplied = history.catalog.identities();
    CHECK(supplied == std::vector<std::string>{"zengine.migrate.WorkshopSession.v1-to-v12",
                                               "zengine.migrate.WorkshopSession.v10-to-v12",
                                               "zengine.migrate.WorkshopSession.v11-to-v12",
                                               "zengine.migrate.WorkshopSession.v2-to-v12",
                                               "zengine.migrate.WorkshopSession.v3-to-v12",
                                               "zengine.migrate.WorkshopSession.v4-to-v12",
                                               "zengine.migrate.WorkshopSession.v5-to-v12",
                                               "zengine.migrate.WorkshopSession.v6-to-v12",
                                               "zengine.migrate.WorkshopSession.v7-to-v12",
                                               "zengine.migrate.WorkshopSession.v8-to-v12",
                                               "zengine.migrate.WorkshopSession.v9-to-v12"});
    // ...and each of them declares the edge its name claims.
    for (const std::string& identity : supplied) {
        CAPTURE(identity);
        const op::OperatorDef* edge = history.catalog.find(identity);
        REQUIRE(edge != nullptr);
        CHECK(op::declares_migration(*edge));
        CHECK(loom::same_identity(*op::migration_target(*edge),
                                  *loom::schema_of<session_persist::WorkshopSession>()));
    }
}

TEST_CASE("a version-1 session means EXACTLY what its own reader meant") {
    // THE EQUIVALENCE PIN, AND IT IS NOT A COPIED NUMBER: `setup_persist::setup_in_v2`, the
    // setup file's own legacy reader, still computes the predecessor's desk, so the answers
    // are compared as VALUES. ⚠ Plus the surface its vintage had: a version-1 Workshop painted
    // the layout run itself, so today's equivalent desk adds the Layouts pane through the
    // ordinary door (`materialized`), and any other rank, key or geometry is named here.
    const session_history::v1::WorkshopSession old = old_v1_session("Yesterday", 120, 44);
    Setup predecessor;
    REQUIRE(setup_persist::setup_in_v2(old.desk, predecessor).accepted);

    MountedHistory history;
    REQUIRE(history.mounted.ok);
    const session_persist::LoadedSession read =
        session_persist::from_text(as_text(old), &history.catalog);
    const window_persist::LoadedWindow room_read = window_persist::import_from_session_text(as_text(old));
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);

    CHECK(live_layout(read) == materialized(predecessor));
    // EXACTLY ONE LAYOUT, LIVE AT ZERO. A vintage that could not say how many desks a weaver
    // had means the one it had -- never none, never two.
    CHECK(read.layouts.size() == 1);
    CHECK(read.active == 0);
    // ...and every session fact of that vintage, by hand, so the comparison above cannot
    // pass by both sides being empty.
    CHECK(read.present);
    CHECK(room_read.honoured);
    CHECK(room_read.viewport_w == cells_px(120));
    CHECK(room_read.viewport_h == cells_px(44));
    CHECK(live_layout(read).name == "Yesterday");
    // THREE ROWS: the two the file authored, in their own order and untouched, and the
    // Layouts pane the vintage had implicitly, appended behind them.
    REQUIRE(live_layout(read).panes.size() == 3);
    // ...AND THE INFO ROW IS CONVERTED ON THE WAY IN, like the browser's and the Builder's:
    // the file says `zengine.workshop/info` and this build reads `zengine.info/info`, because
    // that pane changed hands (`pane_migration.hpp`).
    CHECK(live_layout(read).panes[0].ref == info_ref());
    CHECK(live_layout(read).panes[0].place.mode == pane_unit::kPixels);
    CHECK(live_layout(read).panes[0].place.x == cells_px(3));
    CHECK(live_layout(read).panes[0].place.y == 0); // the canvas's row 2: the room's top
    CHECK(live_layout(read).panes[0].width.amount == cells_px(28));
    CHECK(live_layout(read).panes[0].width.mode == pane_unit::kPixels);
    CHECK(live_layout(read).panes[0].height.mode == pane_unit::kDefault);
    // AND THE BUILDER ROW COMES BACK UNDER THE WEAVE'S OFFICE. The version-1 file names
    // `zengine.workshop/builder`, and the reference is rewritten at read
    // (`pane_migration.hpp`). What moved is the OFFICE -- the pane key, the place, the two
    // sizes and the front order are the file's, untouched.
    CHECK(live_layout(read).panes[1].ref == PaneRef{"zengine.builder-pane", "builder"});
    CHECK(live_layout(read).panes[1].place.mode == pane_unit::kDefault);
    CHECK(live_layout(read).panes[1].width.amount == cells_px(40));
    // A PIXEL AXIS IS DEVICE PIXELS IN BOTH VERSIONS AND CROSSES UNSCALED.
    CHECK(live_layout(read).panes[1].height.mode == pane_unit::kPixels);
    CHECK(live_layout(read).panes[1].height.amount == 220);
    CHECK(live_layout(read).panes[1].front == 1);
    // ...AND THE MATERIALIZED ROW IS UNAUTHORED AND FRONT-MOST. Unauthored, because a weaver
    // who never chose a geometry has still not chosen one and `placement_bounds` answers the
    // historical rectangle for a defaulted row; front-most, because the surface it replaces
    // was painted after every pane.
    CHECK(live_layout(read).panes[2].ref == PaneRef{"zengine.workshop", "layouts"});
    CHECK(live_layout(read).panes[2].place.mode == pane_unit::kDefault);
    CHECK(live_layout(read).panes[2].width.mode == pane_unit::kDefault);
    CHECK(live_layout(read).panes[2].height.mode == pane_unit::kDefault);
    CHECK(live_layout(read).panes[2].front == 2);
    // A LEGACY ROAD CARRIES NO PLACEMENT: nothing in a v1 file could have said one, and the
    // absence has exactly one spelling.
    CHECK_FALSE(room_read.placement.known);
    CHECK(room_read.placement.x == 0);
    CHECK(room_read.placement.y == 0);
    CHECK_FALSE(room_read.placement.maximized);
}

TEST_CASE("a version-2 session means exactly what its own reader meant") {
    const session_history::v2::WorkshopSession old = old_v2_session("Yesterday", 110, 38);
    Setup predecessor;
    REQUIRE(v3_setup_in(old.desk, predecessor).accepted);

    MountedHistory history;
    REQUIRE(history.mounted.ok);
    const session_persist::LoadedSession read =
        session_persist::from_text(as_text(old), &history.catalog);
    const window_persist::LoadedWindow room_read = window_persist::import_from_session_text(as_text(old));
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(live_layout(read) == materialized(predecessor));
    CHECK(live_layout(read) == materialized(arranged_desk("Yesterday")));
    CHECK(read.layouts.size() == 1);
    CHECK(read.active == 0);
    CHECK(room_read.viewport_w == cells_px(110));
    CHECK(room_read.viewport_h == cells_px(38));
    CHECK(room_read.honoured);
    CHECK_FALSE(room_read.placement.known);
}

TEST_CASE("an old session's OWN law still runs -- the conversion skips no check") {
    MountedHistory history;
    REQUIRE(history.mounted.ok);

    SUBCASE("a desk that is not a legal setup is refused in the setup owner's own words") {
        session_history::v1::WorkshopSession old = old_v1_session("Bad", 100, 30);
        old.desk.panes[1].pane = "two words";
        const session_persist::LoadedSession no =
            session_persist::from_text(as_text(old), &history.catalog);
        CHECK_FALSE(no.outcome.accepted);
        // ⚠ THE SETUP OWNER'S SENTENCE, WITH THE LAYOUT'S POSITION IN FRONT OF IT: a session
        // holds a RUN, so "which desk" is a fact the reader has and a weaver needs; the
        // sentence itself is still the setup owner's, unrewritten.
        CHECK(no.outcome.refusal ==
              "layout at position 0: a pane reference's pane key cannot contain spaces or "
              "control characters");
    }
    SUBCASE("a unit word version 2 never had is refused in VERSION 2's vocabulary") {
        session_history::v1::WorkshopSession old = old_v1_session("Bad", 100, 30);
        old.desk.panes[0].place.mode = "barns";
        const session_persist::LoadedSession no =
            session_persist::from_text(as_text(old), &history.catalog);
        CHECK_FALSE(no.outcome.accepted);
        CHECK(no.outcome.refusal.find("`barns`") != std::string::npos);
        CHECK(no.outcome.refusal.find("default or cells") != std::string::npos);
        // The conversion that refused is named, because a weaver who has one converter
        // mounted and another missing needs to know which spoke.
        CHECK(no.outcome.refusal.find("zengine.migrate.WorkshopSession.v1-to-v12") !=
              std::string::npos);
    }
    SUBCASE("a viewport this build will not open at is declined, and the desk still comes") {
        const session_history::v1::WorkshopSession old = old_v1_session("Huge", 100000, 44);
        const session_persist::LoadedSession read =
            session_persist::from_text(as_text(old), &history.catalog);
    const window_persist::LoadedWindow room_read = window_persist::import_from_session_text(as_text(old));
        REQUIRE(read.outcome.accepted);
        CHECK_FALSE(room_read.honoured);
        CHECK_FALSE(room_read.declined.empty());
        CHECK(live_layout(read).name == "Huge");
    }
    SUBCASE("a converted file that is not a Workshop session at all is still refused") {
        session_history::v1::WorkshopSession old = old_v1_session("Wrong", 100, 30);
        old.format = "zengine-workshop";
        const session_persist::LoadedSession no =
            session_persist::from_text(as_text(old), &history.catalog);
        CHECK_FALSE(no.outcome.accepted);
        // The CURRENT reader's own sentence, because the word crossed untouched and this is
        // the party that has always judged it.
        CHECK(no.outcome.refusal == "not a Workshop session: it says it is `zengine-workshop`");
    }
}

TEST_CASE("an old session with no conversion live refuses and changes nothing") {
    TempDir dir("mig0-absent");
    const std::string path = dir.file("session.json");
    const std::string bytes = as_text(old_v1_session("Yesterday", 120, 44));
    spillout(path, bytes);

    // THE AUTHORITY MEASUREMENT. The artifact that would supply the conversion is on disk,
    // named by this very build -- and a run that did not mount it does not open it. A window's
    // painters are images, so a run with no session, the same desk, is the measure of its own.
    const op::ImageCounts window = window_images();
    const op::ImageCounts before = op::image_counts();
    Live t;
    t.host.session_path = path;
    REQUIRE(t.host.conversions == nullptr);
    t.ready();

    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("session version 1 cannot be read") != std::string::npos);
    CHECK(t.notice().find("no live conversion") != std::string::npos);
    CHECK(t.notice().find("opening with the default setup") != std::string::npos);
    // NOTHING WAS INSTALLED and nothing was written.
    CHECK(t.session().setup.active == default_setup());
    CHECK(t.session().screen_w == kScreenMinW);
    CHECK(slurp(path) == bytes);
    // NO IMAGE WAS OPENED but the window's own. A version claim is a lookup key; it reaches no
    // load door.
    const op::ImageCounts after = op::image_counts();
    CHECK(after.opens == before.opens + window.opens);
    CHECK(after.closes == before.closes + window.closes);
    CHECK_FALSE(slurp(SESSION_HISTORY_SO).empty()); // ...and it was sitting right there
}

TEST_CASE("with the conversion mounted, the desk comes back through the weave") {
    TempDir dir("mig0-live");
    const std::string path = dir.file("session.json");
    spillout(path, as_text(old_v1_session("Yesterday", 120, 44)));

    MountedHistory history;
    REQUIRE(history.mounted.ok);
    Live t;
    t.host.session_path = path;
    t.host.conversions = &history.catalog;
    t.on_os_window(); // the medium that grows to the room restored
    t.ready();
    t.window_settles();

    // ...and it is an ORDINARY restore: the same notice, the same room, the same desk.
    CHECK_FALSE(t.session().notice_is_bad);
    CHECK(t.notice().find("reopened your last desk") == 0);
    CHECK(t.notice().find("\"Yesterday\"") != std::string::npos);
    CHECK(t.session().screen_w == cells_px(120));
    CHECK(t.session().screen_h == cells_px(44));
    CHECK(t.session().setup.active.name == "Yesterday");
    CHECK(t.session().panes.has(pane_kind::kLayouts));
    // AND THE BUILDER ROW RESOLVES TO THE WEAVE'S PANE: the vintage file says
    // `zengine.workshop/builder`, the conversion at read makes it
    // `zengine.builder-pane/builder`, a RUNTIME kind that resolves only once that office has
    // offered. With no such office in this rig the row is retained and unresolved, as a saved
    // reference to an absent provider always is; the desk is otherwise the file's.
    CHECK_FALSE(t.session().panes.has(stock::kKind));
    CHECK(has_pane(t.session().setup.active,
                   PaneRef{"zengine.builder-pane", "builder"}));
}

TEST_CASE("reading an old session does not rewrite it; the next close does") {
    // THE PAYOFF, IN FIVE STEPS. A converter is needed only while yesterday's bytes still
    // exist -- and the moment a weaver closes normally, they do not.
    TempDir dir("mig0-rewrite");
    const std::string path = dir.file("session.json");
    const std::string original = as_text(old_v1_session("Yesterday", 120, 44));
    spillout(path, original);

    MountedHistory history;
    REQUIRE(history.mounted.ok);
    {
        Live t;
        t.host.session_path = path;
        t.host.conversions = &history.catalog;
        t.on_os_window(); // the medium that grows to the room restored
        t.ready();
        t.window_settles();
        REQUIRE(t.session().setup.active.name == "Yesterday");
        // 1-3. THE MIGRATION SUCCEEDED IN MEMORY AND THE FILE IS BYTE-IDENTICAL. Reading is
        // reading; there is no "migration complete, rewrite now" path and there must not be.
        CHECK(slurp(path) == original);
        t.key(input::scan::kQ);
        REQUIRE(t.host.quit);
    }
    // 4. AND THE ORDINARY CLOSE-TIME SAVE WROTE THE CURRENT SHAPE, on its own existing law.
    const std::string now = slurp(path);
    CHECK(now != original);
    CHECK(now.find("\"version\":12") != std::string::npos);
    CHECK(now.find("\"format_version\":\"6\"") != std::string::npos);

    // 5. ...SO THE NEXT RUN NEEDS NO CONVERTER AT ALL.
    Live back;
    back.host.session_path = path;
    REQUIRE(back.host.conversions == nullptr);
    back.on_os_window(); // the medium that grows to the room restored
    back.ready();
    back.window_settles();
    CHECK_FALSE(back.session().notice_is_bad);
    CHECK(back.session().setup.active.name == "Yesterday");
    CHECK(back.session().screen_w == cells_px(120));
}

TEST_CASE("unmounting the artifact takes the conversion with it") {
    const std::string bytes = as_text(old_v1_session("Yesterday", 120, 44));
    MountedHistory history;
    REQUIRE(history.mounted.ok);
    REQUIRE(session_persist::from_text(bytes, &history.catalog).outcome.accepted);

    REQUIRE(history.catalog.unmount(history.mounted.provider));
    const session_persist::LoadedSession no =
        session_persist::from_text(bytes, &history.catalog);
    CHECK_FALSE(no.outcome.accepted);
    CHECK(no.outcome.refusal.find("no live conversion") != std::string::npos);
    // A current-shape session is unaffected: it never needed the artifact.
    const std::string current = session_text(one_layout(arranged_desk("Now")), 0);
    CHECK(session_persist::from_text(current, &history.catalog).outcome.accepted);
}

TEST_CASE("a current session bypasses conversion entirely") {
    // Measured on the invocation counter rather than asserted: the conversions are mounted
    // IN PROCESS here (`op::invocations()` cannot see a body in another image), so a spend
    // would move the number.
    op::Catalog conversions;
    REQUIRE(conversions.mount("suite", session_history::conversions()));
    const std::string current = session_text(one_layout(arranged_desk("Now")), 0);

    const std::uint64_t before = op::invocations();
    const session_persist::LoadedSession read =
        session_persist::from_text(current, &conversions);
    REQUIRE(read.outcome.accepted);
    CHECK(live_layout(read) == arranged_desk("Now"));
    CHECK(op::invocations() == before);
}

TEST_CASE("nothing but a historical claim of THIS shape asks for a conversion") {
    // THE NARROWNESS THAT KEEPS THIS FROM BEING A FALLBACK. A seam that answered "try a
    // conversion" to any admission failure would turn every corrupt file, every wrong file
    // and every hostile file into a search for something willing to eat it.
    op::Catalog conversions;
    REQUIRE(conversions.mount("suite", session_history::conversions()));

    const std::vector<std::pair<const char*, std::string>> never = {
        {"not a Zen value at all", "{"},
        {"a retired object DOCUMENT handed to the session reader", kRetiredObjectDocument},
        {"a SETUP file handed to the session reader",
         setup_persist::to_text(arranged_desk("Debugging"))},
        {"a current-version session with a malformed desk",
         [] {
             std::string text = session_text(one_layout(arranged_desk("D")), 0);
             const std::size_t at = text.find("\"pane\":\"stack\"");
             REQUIRE(at != std::string::npos);
             text.replace(at, std::string("\"pane\":\"stack\"").size(),
                          "\"pane\":\"two words\"");
             return text;
         }()},
        {"a current-version session whose header counts a layout it has no line for",
         [] {
             std::string text = session_text(one_layout(arranged_desk("D")), 0);
             const std::size_t at = text.find("\"layout_count\":\"1\"");
             REQUIRE(at != std::string::npos);
             text.replace(at, std::string("\"layout_count\":\"1\"").size(),
                          "\"layout_count\":\"2\"");
             return text;
         }()},
    };
    for (const auto& [what, bytes] : never) {
        CAPTURE(what);
        const std::uint64_t before = op::invocations();
        const session_persist::LoadedSession no =
            session_persist::from_text(bytes, &conversions);
        CHECK_FALSE(no.outcome.accepted);
        CHECK(no.outcome.refusal.find("conversion") == std::string::npos);
        CHECK(op::invocations() == before);
    }
}

TEST_CASE("the session reader owns no historical shape and no conversion") {
    // Defence in depth, the shape this repository's other source tripwires use: what a
    // translation unit can NAME is a fact only reading the file carries, and the point is
    // that the current owner does not grow a rung per vintage. A retained shape, a
    // `kV*FormatVersion` or a `claimed_version() == 7` arm each reddens this case. ⚠ Version 7
    // matters most: its FIELDS are the current shape's, so a retained v7 branch would compile,
    // admit and behave -- nothing but reading this file catches it.
    const std::string source = slurp(WORKSHOP_SESSION_PERSIST_HPP);
    REQUIRE_FALSE(source.empty());
    for (const char* forbidden : {"namespace v1", "namespace v2", "namespace v3",
                                  "namespace v4", "namespace v5", "namespace v6",
                                  "namespace v7", "namespace v8", "namespace v9",
                                  "namespace v10", "setup_in_v2",
                                  "kV1FormatVersion", "kV2FormatVersion",
                                  "kV3FormatVersion", "kV4FormatVersion",
                                  "kV5FormatVersion", "kV6FormatVersion",
                                  "kV7FormatVersion", "kV8FormatVersion",
                                  "kV9FormatVersion", "kV10FormatVersion", "session_history::",
                                  "WorkshopSession, 1,", "WorkshopSession, 2,",
                                  "WorkshopSession, 3,", "WorkshopSession, 4,",
                                  "WorkshopSession, 5,", "WorkshopSession, 6,",
                                  "WorkshopSession, 7,", "WorkshopSession, 8,",
                                  "WorkshopSession, 9,", "WorkshopSession, 10,",
                                  "claimed_version() ==",
                                  "claimed_version()=="}) {
        CAPTURE(forbidden);
        CHECK(source.find(forbidden) == std::string::npos);
    }
    // ...and the one number it does carry is the one it writes.
    CHECK(session_persist::kFormatVersion == 12);
    CHECK(session_persist::WorkshopSession::zen_version == 12u);
    CHECK(session_persist::WorkshopSessionHeader::zen_version == 12u);
}

TEST_CASE("a session this run could not read is never written over") {
    // THE FILE IS WORTH KEEPING: the likeliest reason a session is refused is that its
    // conversion is not mounted in THIS arrangement -- which a weaver fixes by adding a plan
    // row, on a file that has to still be there when they do. So an orderly close writes
    // nothing.
    struct Case {
        const char* what;
        std::string bytes;
        bool refused;
    };
    const std::vector<Case> cases = {
        {"an older session with no conversion live", as_text(old_v1_session("Old", 120, 44)),
         true},
        {"the vintage this build most recently retired",
         v11_lines(v11_session(one_layout(arranged_desk("Retired")), 0, cells_px(120), cells_px(44))),
         true},
        {"a version this build has never written", [] {
             std::string text = session_text(one_layout(arranged_desk("D")), 0);
             const std::size_t at = text.find("\"version\":12");
             REQUIRE(at != std::string::npos);
             text.replace(at, std::string("\"version\":12").size(), "\"version\":13");
             return text;
         }(), true},
        {"bytes that are not a session at all", std::string("{"), true},
        // ...AND THE CONTROL, WHICH IS THE HALF THAT MAKES THE FLAG A JUDGEMENT RATHER THAN A
        // BLANKET. A session read beside a window file whose VIEWPORT was declined was READ -- its
        // desk came back -- so the run keeps its session exactly as it always did.
        {"a session whose window file's viewport this build declines",
         session_text(one_layout(arranged_desk("Wide")), 0),
         false},
    };
    for (const Case& c : cases) {
        CAPTURE(c.what);
        TempDir dir("mig0-refused");
        const std::string path = dir.file("session.json");
        spillout(path, c.bytes);
        if (!c.refused) {
            spillout(window_persist::path_beside(path),
                     window_persist::to_text(cells_px(100000), cells_px(44), {}));
        }
        {
            Live t;
            t.host.session_path = path;
            t.ready();
            // THIS RUN MOVES THE DESK, so a close that writes is told from one that does not.
            stage_desk(t, [](Setup& desk) { desk.name = "Moved"; });
            // Every case here says SOMETHING bad -- a declined viewport is a complaint too --
            // so the notice cannot be the discriminator, and the condition is.
            CHECK(t.session().notice_is_bad);
            // THE STANDING CONSEQUENCE IS A CONDITION, not the notice: that this run keeps no
            // session is still true an hour later and has a weaver action, which is exactly
            // the shape the keymap, prefs and marks walls already have.
            CHECK((t.session().conditions.find(kSessionWallKey) != nullptr) == c.refused);
            t.key(input::scan::kQ);
            REQUIRE(t.host.quit);
        }
        if (c.refused) {
            CHECK(slurp(path) == c.bytes);
        } else {
            CHECK(slurp(path) != c.bytes); // read, so kept: the close wrote this run's own
        }
    }
}

TEST_CASE("the file survives the run that could not read it, and opens later") {
    // THE WHOLE POINT OF THE PREVIOUS CASE, IN ONE STORY. A weaver launches an arrangement
    // whose plan does not carry the conversion, is told so, works, closes -- and then adds
    // the row and gets their desk back. Nothing about the second run is special.
    TempDir dir("mig0-recovered");
    const std::string path = dir.file("session.json");
    const std::string original = as_text(old_v1_session("Yesterday", 120, 44));
    spillout(path, original);
    {
        Live without;
        without.host.session_path = path;
        without.ready();
        REQUIRE(without.session().notice_is_bad);
        without.key(input::scan::kQ);
        REQUIRE(without.host.quit);
    }
    CHECK(slurp(path) == original);

    MountedHistory history;
    REQUIRE(history.mounted.ok);
    Live with;
    with.host.session_path = path;
    with.host.conversions = &history.catalog;
    with.ready();
    CHECK_FALSE(with.session().notice_is_bad);
    CHECK(with.session().setup.active.name == "Yesterday");
    CHECK(with.session().conditions.find(kSessionWallKey) == nullptr);
}

TEST_CASE("a conversion owns yesterday's semantics and does not rewrite history") {
    // THE VINTAGE A CONVERSION CONVERTS IS THE ONE IT CHECKS. An envelope claiming version 1
    // is what SELECTED this edge; a body that then says it is a different vintage is a file
    // asking to be translated by a road it does not belong to, and stamping the current
    // number onto it would admit a file the predecessor refused.
    MountedHistory history;
    REQUIRE(history.mounted.ok);

    SUBCASE("the session's own format_version must be the vintage the edge converts") {
        session_history::v1::WorkshopSession old = old_v1_session("Forged", 100, 30);
        old.format_version = 8; // the envelope still claims v1
        const session_persist::LoadedSession no =
            session_persist::from_text(as_text(old), &history.catalog);
        CHECK_FALSE(no.outcome.accepted);
        CHECK(no.outcome.refusal.find("this session claims version 1 and its own "
                                      "format_version field says 8") != std::string::npos);
    }
    SUBCASE("...and so must the nested desk's") {
        session_history::v1::WorkshopSession old = old_v1_session("Forged", 100, 30);
        old.desk.format_version = 9;
        const session_persist::LoadedSession no =
            session_persist::from_text(as_text(old), &history.catalog);
        CHECK_FALSE(no.outcome.accepted);
        CHECK(no.outcome.refusal.find("this desk claims version 2 and its own "
                                      "format_version field says 9") != std::string::npos);
    }
    SUBCASE("a version-2 session's desk is judged by its own vintage's setup reader") {
        // The v2 edge passes the desk through whole, because a version-2 session already
        // nests version 3's desk -- so a wrong number there is that version's reader's
        // sentence, in its own words, naming the layout it is in.
        session_history::v2::WorkshopSession old = old_v2_session("Forged", 100, 30);
        old.desk.format_version = 2;
        const session_persist::LoadedSession no =
            session_persist::from_text(as_text(old), &history.catalog);
        CHECK_FALSE(no.outcome.accepted);
        CHECK(no.outcome.refusal.find("layout at position 0: " +
                                      setup_persist::wrong_version(2)) != std::string::npos);
    }
}

// =============================================================================
// EVERY LAYOUT THE WEAVER AUTHORED COMES BACK
// =============================================================================
// Close with several authored layouts and get the same set back: values, order, names and
// the one active, every layout an ordinary `Setup`. `layout_run` / `install_layout_run` are
// the only spellings of the run; an old session becomes exactly one layout at position zero.

namespace {

/// A LAYOUT WITH GEOMETRY NOBODY ELSE HAS, deliberately OFF the cell boundary.
///
/// A whole-cell number is a value a character medium can say exactly, so a case built out
/// of whole cells cannot tell "the authored value came back" from "a projection happened to
/// land on it". Every layout below carries a remainder.
Setup layout_of(const std::string& name, std::int64_t nudge) {
    // ⚠ VINTAGE-NEUTRAL, DELIBERATELY. A case planting a HISTORICAL session wants what an
    // older Workshop wrote (two panes, the layout surface implicit); a case putting it into a
    // LIVE session says `materialized(layout_of(...))` at its own call site, so which vintage a
    // fixture means is written where it is used rather than guessed here.
    Setup s = setup_of(name, {second::kKind, stock::kKind});
    REQUIRE(author_pane_place(s, ref_of(second::kKind), cells_px(3) + nudge, cells_px(2) + nudge)
                .accepted);
    REQUIRE(author_pane_size(s, ref_of(second::kKind),
                             PaneSize{pane_unit::kPixels, cells_px(30) + nudge},
                             PaneSize{pane_unit::kPixels, cells_px(9) + nudge})
                .accepted);
    REQUIRE(author_pane_place(s, ref_of(stock::kKind), cells_px(9) + nudge, cells_px(6)).accepted);
    return s;
}

/// THE RUN THREE AUTHORED LAYOUTS MAKE, each different from the others in name, in geometry
/// and in which pane is in front.
std::vector<Setup> three_desks() {
    std::vector<Setup> run{layout_of("Home", 7), layout_of("Code", 19), layout_of("Art", 31)};
    // ...and a distinct FRONT order in the middle one, which is authored data the file
    // carries and no geometry can stand in for.
    REQUIRE(send_to_front(run[1], ref_of(second::kKind)));
    return run;
}

/// ...AS A RUN OF LAYOUTS, each related to no Setup artifact. What most cases about the
/// durable run mean: three desks, in this order, and nothing about associations.
std::vector<Layout> three_layouts() { return plain_run(three_desks()); }

/// A CURRENT SESSION VALUE BUILT BY HAND, for the cases that need one this build would
/// never write -- an empty run, a ninth layout, a position that is not a layout.
session_persist::WorkshopSession hand_built(std::size_t layouts, std::int64_t active) {
    session_persist::WorkshopSession out;
    out.format = session_persist::kFormat;
    out.format_version = session_persist::kFormatVersion;
    for (std::size_t i = 0; i < layouts; ++i) {
        out.layouts.push_back(session_persist::WorkshopLayout{
            static_cast<std::int64_t>(i) + 1,
            setup_persist::to_setup(setup_of("L" + std::to_string(i), {second::kKind})),
            session_persist::to_link(SetupLink{})});
    }
    out.active = active;
    out.next_id = static_cast<std::int64_t>(layouts) + 1;
    return out;
}

std::string as_text(const session_persist::WorkshopSession& s) {
    return session_persist::to_text(s);
}

/// A VERSION 3 SESSION, WRITTEN THE WAY A VERSION 3 WORKSHOP WROTE IT -- produced by the
/// shipped historical shape rather than by a hand-typed approximation.
session_history::v3::WorkshopSession old_v3_session(const char* name, std::int64_t w,
                                                    std::int64_t h) {
    session_history::v3::WorkshopSession old;
    old.format = session_persist::kFormat;
    old.format_version = 3;
    old.viewport = session_history::v6::WorkshopViewport{w, h};
    old.desk = v3_desk(arranged_desk(name));
    old.placement = session_history::absent_placement();
    return old;
}

std::string as_text(const session_history::v3::WorkshopSession& old) {
    return loom::compat::serialize(loom::to_value(old));
}

} // namespace

// ---- A: the durable shape -------------------------------------------------

TEST_CASE("a whole layout run round-trips exactly, active in the middle") {
    const std::vector<Layout> run = three_layouts();
    window_persist::Placement place;
    place.known = true;
    place.x = -1200;
    place.y = 340;

    const std::string text = session_text(run, 1);
    const session_persist::LoadedSession read = session_persist::from_text(text);
    const window_persist::LoadedWindow room_read =
        window_persist::from_text(window_persist::to_text(cells_px(120), cells_px(44), place));
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);

    // VALUES, ORDER, NAMES AND THE ACTIVE POSITION -- the four things the completion
    // sentence names, asserted as VALUES rather than field by field, so a layout that came
    // back subtly different cannot pass by the fields a case happened to list.
    CHECK(read.layouts == run);
    CHECK(read.active == 1);
    REQUIRE(read.layouts.size() == 3);
    CHECK(read.layouts[0].desk.name == "Home");
    CHECK(read.layouts[1].desk.name == "Code");
    CHECK(read.layouts[2].desk.name == "Art");
    CHECK(live_layout(read).name == "Code");
    // ...and the facts a value comparison could pass on by being empty on both sides.
    CHECK(read.layouts[0].desk.panes[0].place.x == cells_px(3) + 7);
    CHECK(read.layouts[2].desk.panes[0].place.x == cells_px(3) + 31);
    CHECK(read.layouts[1].desk.panes[0].front > read.layouts[1].desk.panes[1].front);
    CHECK(read.layouts[0].desk.panes[0].front < read.layouts[0].desk.panes[1].front);
    // The room and the placement are the window file's, beside the run and none of it.
    CHECK_FALSE(window_persist::import_from_session_text(text).present);
    CHECK(room_read.viewport_w == cells_px(120));
    CHECK(room_read.viewport_h == cells_px(44));
    CHECK(room_read.honoured);
    CHECK(room_read.placement.known);
    CHECK(room_read.placement.x == -1200);

    // A SECOND SAVE OF A LOADED SESSION IS BYTE-IDENTICAL, which is what makes "nothing is
    // normalised on the way through" a fact rather than a hope.
    CHECK(session_text(read.layouts, read.active) == text);
    // AND THE FILE SAYS WHAT IT IS: the current version, a run, and a position.
    CHECK(text.find("\"version\":12") != std::string::npos);
    CHECK(text.find("\"format_version\":\"12\"") != std::string::npos);
    CHECK(text.find("\"layout_count\":") != std::string::npos);
    CHECK(text.find("\"active\":\"1\"") != std::string::npos);
    // ...and every layout in it is an ordinary setup, at the setup format's own version.
    CHECK(text.find("\"format\":\"zengine-workshop-setup\"") != std::string::npos);
}

TEST_CASE("every position in the run is a position a session can be saved at") {
    // NOT ONLY THE MIDDLE. `layout.new` leaves a weaver on the LAST layout, so a save that
    // wrote `shelved.size()` instead of `active_at` passes a middle-only case and fails a
    // weaver who pressed `,` once.
    const std::vector<Layout> run = three_layouts();
    for (std::size_t at = 0; at < run.size(); ++at) {
        CAPTURE(at);
        const session_persist::LoadedSession read = session_persist::from_text(
            session_text(run, at));
        REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
        CHECK(read.layouts == run);
        CHECK(read.active == at);
    }
}

TEST_CASE("a current run this Workshop could not have made is refused as CURRENT data") {
    // THE NARROWNESS THAT MAKES MIGRATION SAFE, ASSERTED FROM THE OTHER SIDE. A malformed
    // CURRENT file is wrong, not old: it must meet this version's own law and get this
    // version's own sentence, and it must never become a search for something willing to
    // translate it. The conversions are mounted throughout, so "no conversion was asked
    // for" is measured rather than assumed.
    MountedHistory history;
    REQUIRE(history.mounted.ok);

    struct Case {
        const char* what;
        std::string bytes;
        std::string says;
    };
    session_persist::WorkshopSession illegal_desk = hand_built(2, 0);
    illegal_desk.layouts[1].desk.panes[0].pane = "two words";

    const std::vector<Case> cases = {
        {"no layouts at all", as_text(hand_built(0, 0)),
         "this session holds no layout at all, and a Workshop always has at least one desk"},
        {"an active position past the end", as_text(hand_built(3, 3)),
         "this session's active layout is position 3, and its layouts run from 0 to 2"},
        {"a negative active position", as_text(hand_built(3, -1)),
         "this session's active layout is position -1, and its layouts run from 0 to 2"},
        {"one more layout than this Workshop keeps",
         as_text(hand_built(kMaxLayouts + 1, 0)),
         "this session holds 9 layouts, and this Workshop keeps at most 8"},
        {"a layout that is not a legal setup", as_text(illegal_desk),
         "layout at position 1: a pane reference's pane key cannot contain spaces or "
         "control characters"},
    };

    for (const Case& c : cases) {
        CAPTURE(c.what);
        const std::uint64_t before = op::invocations();
        const session_persist::LoadedSession no =
            session_persist::from_text(c.bytes, &history.catalog);
        CHECK_FALSE(no.outcome.accepted);
        CHECK(no.outcome.refusal == c.says);
        // ...and it never went looking for a conversion: not in the sentence, and not in
        // the counter that records a spend.
        CHECK(no.outcome.refusal.find("conversion") == std::string::npos);
        CHECK(no.outcome.refusal.find("version") == std::string::npos);
        CHECK(op::invocations() == before);
        // AND NOTHING IS HALF-INSTALLED: a refused session has no run at all.
        CHECK(no.layouts.empty());
    }
    // THE CEILING IS THE GESTURE'S OWN NUMBER, not a second one written into the format.
    CHECK(session_persist::from_text(as_text(hand_built(kMaxLayouts, 0))).outcome.accepted);
}

TEST_CASE("a desk at its pane bound, every key at its own bound, is a setup file a launch reads") {
    // The row bound and the file's byte bound are two owners' numbers: raising the rows must
    // not let a weaver save a desk the next launch refuses as too large. Every key, place,
    // extent and setting at its own bound, the texts a writer escapes, is the largest file.
    Setup s = setup_of(std::string(kMaxSetupNameLen, 'n'), {});
    fill_to_every_bound(s);
    const std::string text = setup_persist::to_text(s);
    CHECK(text.size() <= setup_persist::kMaxSetupBytes);
    const auto read = setup_persist::from_text(text);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.setup == s);

    // ...AND THROUGH THE FILE: `s` writes it and `r`'s reader reads it back whole.
    TempDir dir("setup-every-bound");
    const std::string path = dir.file("setup.json");
    REQUIRE(setup_persist::save_file(path, s).accepted);
    const auto loaded = setup_persist::load_file(path);
    REQUIRE_MESSAGE(loaded.outcome.accepted, loaded.outcome.refusal);
    CHECK(loaded.setup == s);
}

TEST_CASE("a session may hold as much as it may hold, and be read back") {
    // THE CASE THAT MAKES THE DERIVED READ CEILING LOAD-BEARING: `kMaxLayouts` desks of
    // `kMaxSetupPanes` rows, each row up to two `kMaxPaneKeyLen` keys, is LARGER THAN ONE
    // DESK'S CEILING -- and a session ceiling left there would have `q` write a file the next
    // launch refuses. Every bound is the owner's own constant, never a literal, so raising any
    // of them re-measures this case instead of stranding it.
    std::vector<Layout> run;
    for (std::size_t i = 0; i < kMaxLayouts; ++i) {
        Setup s = layout_of(std::string(kMaxSetupNameLen - 2, 'n') + std::to_string(i),
                            static_cast<std::int64_t>(i) + 1);
        // FULL DESKS, to the setup owner's own bounds -- rows, keys, places, extents and
        // settings: a reference this build cannot resolve is legal and is written exactly as
        // authored, which is what makes it the honest way to reach a maximal file.
        fill_to_every_bound(s);
        run.push_back(Layout{std::move(s), SetupLink{}});
    }
    const std::string text =
        session_text(run, kMaxLayouts - 1);
    // ...AND IT IS BIGGER THAN A SINGLE DESK MAY BE. This is the whole argument for the
    // derived bound, asserted rather than reasoned about.
    CHECK(text.size() > setup_persist::kMaxSetupBytes);
    CHECK(text.size() <= session_persist::kMaxSessionBytes);
    // AND THE BOUND IS PROVEN FROM THE FORMAT'S OWN NUMBERS, not this measurement: with
    // `kMaxSetupBytes` above the largest legal desk, a ceiling of one
    // desk per layout would pass a size case while wrong about what a layout MAY hold. The read
    // bound must admit all the per-field bounds allow: its desk, its association's, the path.
    CHECK(session_persist::kMaxSessionBytes >=
          static_cast<std::uintmax_t>(kMaxLayouts) * 2u * setup_persist::kMaxSetupBytes);

    TempDir dir("wux10-full");
    const std::string path = dir.file("session.json");
    REQUIRE(save_last_run(path, run, kMaxLayouts - 1, cells_px(120), cells_px(44),
                          window_persist::Placement{})
                .accepted);
    const session_persist::LoadedSession read = session_persist::load_file(path);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.layouts == run);
    CHECK(read.active == kMaxLayouts - 1);
}

TEST_CASE("a retired shape's wire identity is the identity it was written at") {
    // THE ONE THING THAT WOULD SILENTLY STRAND EVERY OLD FILE: a historical shape IS the door
    // old bytes claim -- name, version AND content id. Reorder, rename or nest a field and
    // `loom::admit` stops recognising them, with no compile error. So the ids are written
    // down, each with its provenance: v3's compiled from `session_persist::WorkshopSession` at
    // Zengine `0cffe92`, the build that wrote version 3; v1's and v2's are read back above.
    struct Vintage {
        const char* what;
        std::shared_ptr<const loom::Schema> shape;
        std::uint32_t version;
        std::uint64_t id;
    };
    const std::vector<Vintage> history = {
        {"v1", loom::schema_of<session_history::v1::WorkshopSession>(), 1u,
         0xe82094b53653a1f2ull},
        {"v2", loom::schema_of<session_history::v2::WorkshopSession>(), 2u,
         0x862c718a0abf08c5ull},
        {"v3", loom::schema_of<session_history::v3::WorkshopSession>(), 3u,
         0x849fe51c31dc0cfbull},
        // v4's was measured against Zengine a39795e, the build that WROTE version 4.
        {"v4", loom::schema_of<session_history::v4::WorkshopSession>(), 4u,
         0xb621c9f3616c7bb1ull},
        // AND v5'S IS READ OFF A FILE A VERSION 5 BUILD WROTE: a live SDL witness left a
        // session file (produced by Zengine 2dc7626, kept outside this repository) whose
        // envelope says `"content_id":"0x6f5b0dfc72bfa501"` -- so this number is the door those
        // bytes claim, corroborated by bytes, not a compiler's reading of a retyped struct.
        {"v5", loom::schema_of<session_history::v5::WorkshopSession>(), 5u,
         0x6f5b0dfc72bfa501ull},
        // v6's is read off session files version 6 builds wrote (kept outside this
        // repository): every one says `"content_id":"0x65ddb2477e598d18"`.
        {"v6", loom::schema_of<session_history::v6::WorkshopSession>(), 6u,
         0x65ddb2477e598d18ull},
        // v7's is the content id the census pinned for it while version 7 was the one Zengine
        // wrote (`tests/shapes.txt` at Zengine 6205dc9), and every version 7 file names.
        {"v7", loom::schema_of<session_history::v7::WorkshopSession>(), 7u,
         0xcac415773fb5880aull},
        // v8's is the content id the census pinned for it while version 8 was the one Zengine
        // wrote (`tests/shapes.txt` at Zengine 7d42479), and every version 8 file names.
        {"v8", loom::schema_of<session_history::v8::WorkshopSession>(), 8u,
         0x520a73e48f9d3ed1ull},
        // v9's is the content id the census pinned for it while version 9 was the one Zengine
        // wrote (`tests/shapes.txt` at Zengine 2b48d263), and every version 9 file names.
        {"v9", loom::schema_of<session_history::v9::WorkshopSession>(), 9u,
         0xc0d49edb80731ff2ull},
    };
    for (const Vintage& v : history) {
        CAPTURE(v.what);
        CHECK(v.shape->name() == std::string(session_persist::WorkshopSession::zen_name));
        CHECK(v.shape->version() == v.version);
        CHECK(v.shape->content_id() == v.id);
    }
    // ...and the current shape is none of them, which is what makes them history.
    const std::shared_ptr<const loom::Schema> current =
        loom::schema_of<session_persist::WorkshopSession>();
    CHECK(current->version() == 12u);
    for (const Vintage& v : history) {
        CAPTURE(v.what);
        CHECK_FALSE(loom::same_identity(*current, *v.shape));
    }
}

// ---- B: the lowering, in both directions ----------------------------------

TEST_CASE("the run and the lifted-active representation are one fact") {
    // THE INVERSE PAIR, SWEPT OVER EVERY POSITION. `shelved` + `active_at` is the run
    // with one element taken out, so putting it back and lifting it again must be the
    // identity -- no duplication of the live value, no reorder, no drift in which one is
    // active.
    const std::vector<Layout> run = three_layouts();
    for (std::size_t at = 0; at < run.size(); ++at) {
        CAPTURE(at);
        SetupState state;
        REQUIRE(install_layout_run(state, run, at));

        // WHAT RUNTIME HOLDS: one lifted value, the rest on the shelf, in order.
        CHECK(state.active == run[at].desk);
        CHECK(state.active_link == run[at].link);
        CHECK(state.active_at == at);
        CHECK(layout_count(state) == run.size());
        CHECK(state.shelved.size() == run.size() - 1);
        // ...and the live value is NOT also on the shelf. One live desk, one copy of it.
        for (const Layout& shelved : state.shelved) {
            CHECK_FALSE(&shelved.desk == &state.active);
            CHECK(shelved.desk.name != run[at].desk.name);
        }
        // THE READ PUTS IT BACK EXACTLY WHERE IT WAS.
        CHECK(layout_run(state) == run);
        // ...and `layout_at` -- the runtime reading every consumer uses -- agrees with the
        // durable one position for position, which is what makes them one order.
        for (std::size_t i = 0; i < run.size(); ++i) {
            CAPTURE(i);
            CHECK(layout_at(state, i) == run[i].desk);
            CHECK(link_at(state, i) == run[i].link);
        }
        // AND READING IS NOT A WRITE: the same answer twice, from an untouched state.
        CHECK(layout_run(state) == run);
        CHECK(state.active_at == at);
    }
}

TEST_CASE("installing a run touches nothing else the session owns") {
    // THE NAME EDITOR IS NOT TOUCHED, and neither is anything outside the run: this is the
    // container's own operation and knows nothing about presentations. The associations ride
    // IN the run, so what is asserted is that the run's own contents arrive whole and that
    // nothing beside them was reached for.
    SetupState state;

    REQUIRE(install_layout_run(state, three_layouts(), 2));
    CHECK(live_status(state) == setup_link::kNone);

    // AND THE TYPE'S OWN FLOOR IS KEPT WHATEVER THE CALLER SAYS: a run this could not lift
    // from is refused, and nothing moved.
    const SetupState before = state;
    CHECK_FALSE(install_layout_run(state, std::vector<Layout>{}, 0));
    CHECK_FALSE(install_layout_run(state, three_layouts(), 3));
    CHECK(state.active == before.active);
    CHECK(state.shelved == before.shelved);
    CHECK(state.active_at == before.active_at);
}

// ---- C: yesterday, three direct edges -------------------------------------

TEST_CASE("a version-3 session becomes exactly one layout, live at zero") {
    // THE CANONICAL FALSIFIER. A v3 session value becomes a session whose run holds exactly
    // its old desk, active at position zero, with every non-layout fact unchanged -- and the
    // equivalence is computed from the predecessor's own reader rather than transcribed.
    session_history::v3::WorkshopSession old = old_v3_session("Yesterday", 120, 44);
    old.placement.mode = window_persist::kPlacementDesktop;
    old.placement.x = -900;
    old.placement.y = 120;
    old.placement.window = window_persist::kWindowMaximized;

    Setup predecessor;
    REQUIRE(v3_setup_in(old.desk, predecessor).accepted);

    MountedHistory history;
    REQUIRE(history.mounted.ok);
    const session_persist::LoadedSession read =
        session_persist::from_text(as_text(old), &history.catalog);
    const window_persist::LoadedWindow room_read = window_persist::import_from_session_text(as_text(old));
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);

    // EXACTLY ONE LAYOUT, AND IT IS THE DESK THAT WAS LIVE.
    REQUIRE(read.layouts.size() == 1);
    CHECK(read.active == 0);
    CHECK(read.layouts[0].desk == materialized(predecessor));
    CHECK(read.layouts[0].desk == materialized(arranged_desk("Yesterday")));
    // ...AND IT IS RELATED TO NO SETUP ARTIFACT: a version-3 file could not say that a desk
    // came from one, so inventing an association would be this reader deciding something the
    // weaver never wrote down.
    CHECK(read.layouts[0].link.path.empty());
    CHECK(link_status(read.layouts[0].desk, read.layouts[0].link) == setup_link::kNone);
    // EVERY NON-LAYOUT FACT OF THAT VINTAGE, UNCHANGED -- including a REAL placement, which
    // is the fact version 3 had and versions 1 and 2 did not.
    CHECK(room_read.viewport_w == cells_px(120));
    CHECK(room_read.viewport_h == cells_px(44));
    CHECK(room_read.honoured);
    CHECK(room_read.placement.known);
    CHECK(room_read.placement.x == -900);
    CHECK(room_read.placement.y == 120);
    CHECK(room_read.placement.maximized);
}

TEST_CASE("all three vintages arrive as one layout at position zero") {
    // THE PLURALITY IS DEFAULTED AND NOT INVENTED. None of these vintages could say how
    // many layouts a weaver had, so the only truthful reading is the one desk they meant --
    // never zero, never two, and never an active position other than the one that exists.
    MountedHistory history;
    REQUIRE(history.mounted.ok);
    const std::vector<std::pair<const char*, std::string>> vintages = {
        {"version 1", as_text(old_v1_session("One", 110, 38))},
        {"version 2", as_text(old_v2_session("Two", 110, 38))},
        {"version 3", as_text(old_v3_session("Three", 110, 38))},
    };
    for (const std::pair<const char*, std::string>& v : vintages) {
        CAPTURE(v.first);
        const session_persist::LoadedSession read =
            session_persist::from_text(v.second, &history.catalog);
        REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
        CHECK(read.layouts.size() == 1);
        CHECK(read.active == 0);
        CHECK_FALSE(live_layout(read).panes.empty());
    }
}

TEST_CASE("three DIRECT edges, and no chain to walk even if one wanted to") {
    // ONE SPEND IS ONE AUTHORED EDGE. Every old vintage has its own edge to the current shape
    // rather than routing through an intermediate rung, so the catalog this arrangement mounts
    // holds no intermediate edge, and a composed answer could not arise even from a seam that
    // tried.
    MountedHistory history;
    REQUIRE(history.mounted.ok);
    for (const char* absent : {"zengine.migrate.WorkshopSession.v1-to-v3",
                               "zengine.migrate.WorkshopSession.v2-to-v3",
                               "zengine.migrate.WorkshopSession.v1-to-v2"}) {
        CAPTURE(absent);
        CHECK(history.catalog.find(absent) == nullptr);
    }
    for (const char* live : {"zengine.migrate.WorkshopSession.v1-to-v12",
                             "zengine.migrate.WorkshopSession.v2-to-v12",
                             "zengine.migrate.WorkshopSession.v3-to-v12"}) {
        CAPTURE(live);
        REQUIRE(history.catalog.find(live) != nullptr);
    }
    // ...and the oldest vintage still opens, in one spend.
    const std::uint64_t before = op::invocations();
    const session_persist::LoadedSession read = session_persist::from_text(
        as_text(old_v1_session("Oldest", 110, 38)), &history.catalog);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(op::invocations() == before + 1);
}

TEST_CASE("a version-3 file with no conversion live refuses and is not rewritten") {
    // THE AUTHORITY STORY, FOR THE VERSION 3 VINTAGE. A file that opened fine yesterday needs
    // conversion power today, and that power comes from a row in an arrangement -- not from
    // the bytes asking for it.
    TempDir dir("wux10-absent");
    const std::string path = dir.file("session.json");
    const std::string bytes = as_text(old_v3_session("Yesterday", 120, 44));
    spillout(path, bytes);

    const op::ImageCounts window = window_images();
    const op::ImageCounts before = op::image_counts();
    {
        Live t;
        t.host.session_path = path;
        REQUIRE(t.host.conversions == nullptr);
        t.ready();

        CHECK(t.session().notice_is_bad);
        CHECK(t.notice().find("session version 3 cannot be read") != std::string::npos);
        CHECK(t.notice().find("`zengine.migrate.WorkshopSession.v3-to-v12`") !=
              std::string::npos);
        CHECK(t.session().setup.active == default_setup());
        CHECK(layout_count(t.session().setup) == 1);
        // ...AND AN ORDERLY CLOSE WRITES NOTHING OVER IT.
        t.key(input::scan::kQ);
        REQUIRE(t.host.quit);
    }
    CHECK(slurp(path) == bytes);
    // NO IMAGE WAS OPENED but the window's own: a version claim is a lookup key and reaches no
    // load door.
    const op::ImageCounts after = op::image_counts();
    CHECK(after.opens == before.opens + window.opens);
    CHECK_FALSE(slurp(SESSION_HISTORY_SO).empty()); // ...and it was sitting right there

    // PUT THE POWER BACK, AND THE SAME BYTES OPEN.
    MountedHistory history;
    REQUIRE(history.mounted.ok);
    Live back;
    back.host.session_path = path;
    back.host.conversions = &history.catalog;
    back.ready();
    CHECK_FALSE(back.session().notice_is_bad);
    CHECK(back.session().setup.active.name == "Yesterday");
    CHECK(layout_count(back.session().setup) == 1);
    CHECK(back.session().setup.active_at == 0);
}

// ---- D: the weaver's whole run, through the real weave ---------------------

TEST_CASE("three layouts, closed on the middle, come back and stay separate") {
    // THE COMPLETION SENTENCE, DRIVEN THROUGH THE PRODUCTION DOORS. Author three layouts
    // with distinct names, populations, geometry and front order; stand on the middle one;
    // quit; and come back to the same run.
    TempDir dir("wux10-run");
    const std::string session = dir.file("session.json");
    std::vector<Layout> authored;

    {
        Live t;
        t.host.session_path = session;
        t.host.setup_path = dir.file("s.json");
        t.ready();
        t.extent_px(cells_px(120), cells_px(44));

        stage_desk(t, [&](Setup& desk) {
            desk = materialized(layout_of("Home", 7));
        });
        layout_new(t);
        stage_desk(t, [&](Setup& desk) {
            desk = materialized(layout_of("Code", 19));
        });
        stage_desk(t, [&](Setup& desk) {
            REQUIRE(send_to_front(desk, ref_of(second::kKind)));
        });
        layout_new(t);
        stage_desk(t, [&](Setup& desk) {
            desk = materialized(layout_of("Art", 31));
        });
        stage_desk(t, [&](Setup& desk) {
            REQUIRE(remove_pane(desk, ref_of(stock::kKind)));
        });
        REQUIRE(layout_count(t.session().setup) == 3);
        layout_next(t);
        layout_next(t);
        REQUIRE(t.session().setup.active.name == "Code");
        authored = layout_run(t.run());

        t.key(input::scan::kQ);
        REQUIRE(t.host.quit);
    }

    Live back;
    back.host.session_path = session;
    back.host.setup_path = dir.file("elsewhere.json");
    back.ready();
    back.extent_px(cells_px(120), cells_px(44));

    // ALL THREE, IN ORDER, WITH THEIR NAMES AND THEIR EXACT AUTHORED VALUES.
    REQUIRE(layout_count(back.session().setup) == 3);
    CHECK(layout_run(back.run()) == authored);
    CHECK(back.session().setup.active_at == 1);
    CHECK(back.session().setup.active.name == "Code");
    // EACH ONE RETURNS ITS AUTHORED VALUE WHEN ACTIVATED, through the shipped gesture.
    for (std::size_t at = 0; at < 3; ++at) {
        CAPTURE(at);
        while (back.session().setup.active_at != at) {
            layout_next(back);
        }
        CHECK(back.session().setup.active == authored[at].desk);
    }
    // ...AND THE WORKSHOP-GLOBAL FACTS ARE STILL GLOBAL: one document, one project, one
    // browser location, one keymap, one window. A layout is a desk and nothing more.
    CHECK(back.session().setup.active_at == 2);
    CHECK(back.session().screen_w == cells_px(120));

    // MODIFY ONE RESTORED LAYOUT AND THE OTHERS ARE BYTE-FOR-BYTE WHAT THEY WERE.
    while (back.session().setup.active_at != 0) {
        layout_next(back);
    }
    stage_desk(back, [&](Setup& desk) {
        REQUIRE(add_pane(desk, stranger()));
    });
    CHECK(layout_at(back.run(), 1) == authored[1].desk);
    CHECK(layout_at(back.run(), 2) == authored[2].desk);
    CHECK(setup_persist::to_text(layout_at(back.run(), 1)) ==
          setup_persist::to_text(authored[1].desk));
}

TEST_CASE("the position that comes back is the one the weaver stood on") {
    // NOT THE END, AND NOT ZERO. Two closes from two different tabs, with nothing else
    // changed, must produce two different active positions -- which is what makes the
    // saved position a fact about the weaver rather than a fact about `layout.new`.
    TempDir dir("wux10-active");
    const std::string session = dir.file("session.json");
    for (const std::size_t stand_on : {std::size_t(0), std::size_t(2), std::size_t(1)}) {
        CAPTURE(stand_on);
        {
            Live t;
            t.host.session_path = session;
            t.host.setup_path = dir.file("s.json");
            t.ready();
            if (layout_count(t.session().setup) == 1) {
                stage_desk(t, [&](Setup& desk) {
                    desk.name = "Home";
                });
                layout_new(t);
                stage_desk(t, [&](Setup& desk) {
                    desk.name = "Code";
                });
                layout_new(t);
                stage_desk(t, [&](Setup& desk) {
                    desk.name = "Art";
                });
            }
            REQUIRE(layout_count(t.session().setup) == 3);
            while (t.session().setup.active_at != stand_on) {
                layout_next(t);
            }
            t.key(input::scan::kQ);
            REQUIRE(t.host.quit);
        }
        Live back;
        back.host.session_path = session;
        back.host.setup_path = dir.file("s.json");
        back.ready();
        CHECK(layout_count(back.session().setup) == 3);
        CHECK(back.session().setup.active_at == stand_on);
    }
}

// ---- E: what deliberately did not move ------------------------------------

TEST_CASE("`s` and `r` still mean the live layout, across a save and a restart") {
    // THE OWNERSHIP DISTINCTION THIS MUST NOT BLUR. A standalone setup file is ONE named desk;
    // the session is the machine-local fact of which desks a weaver was using. Persisting
    // several of the second must not turn the first into a workspace.
    TempDir dir("wux10-setupfile");
    const std::string session = dir.file("session.json");
    const std::string setup = dir.file("s.json");
    std::vector<Layout> after_save;

    {
        Live t;
        t.host.session_path = session;
        t.host.setup_path = setup;
        t.ready();
        stage_desk(t, [&](Setup& desk) {
            desk = materialized(layout_of("Home", 7));
        });
        layout_new(t);
        stage_desk(t, [&](Setup& desk) {
            desk = materialized(layout_of("Code", 19));
        });
        layout_new(t);
        stage_desk(t, [&](Setup& desk) {
            desk = materialized(layout_of("Art", 31));
        });
        layout_next(t); // wrap to Home
        REQUIRE(t.session().setup.active_at == 0);

        name_setup(t, "Kept");
        after_save = layout_run(t.run());
        t.key(input::scan::kQ);
        REQUIRE(t.host.quit);
    }

    // THE SETUP FILE HOLDS ONE DESK -- the live one -- and knows nothing about a run.
    const setup_persist::LoadedSetup file = setup_persist::load_file(setup);
    REQUIRE(file.outcome.accepted);
    CHECK(file.setup.name == "Kept");
    // ⚠ THE RUN'S OWN FIELD, NOT THE WORD: `layouts` is a pane KEY as well as the session's
    // field name, and a desk naming the Layouts pane says it honestly.
    CHECK(slurp(setup).find("\"layouts\":[") == std::string::npos);
    CHECK(slurp(setup).find("active") == std::string::npos);
    // ...and the SESSION still holds all three, with `s` having moved only the live one.
    const session_persist::LoadedSession read = session_persist::load_file(session);
    REQUIRE(read.outcome.accepted);
    CHECK(read.layouts == after_save);
    CHECK(read.layouts.size() == 3);

    // AND `r` READS THAT FILE INTO THE LIVE LAYOUT ONLY, in its own position in the run.
    Live back;
    back.host.session_path = session;
    back.host.setup_path = setup;
    back.ready();
    REQUIRE(layout_count(back.session().setup) == 3);
    layout_next(back);
    REQUIRE(back.session().setup.active_at == 1);
    const Setup untouched_before = layout_at(back.run(), 0);
    const Setup untouched_after_two = layout_at(back.run(), 2);
    back.key(input::scan::kR);
    CHECK(back.notice().find("restored setup \"Kept\"") == 0);
    CHECK(back.session().setup.active.name == "Kept");
    CHECK(back.session().setup.active_at == 1);
    CHECK(layout_count(back.session().setup) == 3);
    CHECK(layout_at(back.run(), 0) == untouched_before);
    CHECK(layout_at(back.run(), 2) == untouched_after_two);
}

TEST_CASE("crossing media never rewrites a persisted layout's geometry") {
    // CROSS-MEDIUM PRESENTATION IS PROJECTION ONLY, and the plural does not weaken it: a run
    // authored on the fine lattice, LOOKED AT through a character medium and then saved, is
    // byte-identical to a run that never crossed.
    TempDir dir("wux10-media");
    const std::string session = dir.file("session.json");
    const std::vector<Layout> authored = three_layouts();
    const std::string never_crossed =
        session_text(authored, 1);

    Live t;
    t.host.session_path = session;
    t.host.setup_path = dir.file("s.json");
    t.ready();
    t.extent_px(cells_px(120), cells_px(44));
    stage_run(t, [&](SetupState& run) { REQUIRE(install_layout_run(run, authored, 1)); });

    // LOOK AT EVERY LAYOUT, on a medium whose cells cannot say a sub-cell remainder.
    for (std::size_t i = 0; i < authored.size() * 2; ++i) {
        (void)paint(t.session());
        layout_next(t);
    }
    while (t.session().setup.active_at != 1) {
        layout_next(t);
    }
    t.key(input::scan::kQ);
    REQUIRE(t.host.quit);

    CHECK(slurp(session) == never_crossed);
    CHECK(slurp(session).find("cell_px") == std::string::npos);
}

// ---- F: the Setup ASSOCIATION, through the real weave -------------

TEST_CASE("`s` establishes the association only after a successful write") {
    TempDir dir("wux11-save");
    Live t;
    t.host.setup_path = dir.file("s.json");
    (void)first_frame(t);

    // A FRESH LAYOUT IS `none`, and that is not "unsaved": the session remembers it.
    REQUIRE(live_status(t.run()) == setup_link::kNone);
    REQUIRE(t.run().active_link.path.empty());

    // A REFUSED WRITE ADVANCES NOTHING. The destination is a DIRECTORY, so the writer
    // cannot open it -- and a failed save may not leave a layout claiming a relationship
    // to a file it was never written to.
    Live blocked;
    blocked.host.setup_path = dir.file("wall");
    std::filesystem::create_directories(blocked.host.setup_path);
    (void)first_frame(blocked);
    const Setup was = blocked.session().setup.active;
    blocked.key(input::scan::kS);
    CHECK(blocked.session().notice_is_bad);
    CHECK(live_status(blocked.run()) == setup_link::kNone);
    CHECK(blocked.run().active_link.path.empty());
    CHECK(blocked.session().setup.active == was); // and the live desk is untouched

    // A SUCCESSFUL ONE ESTABLISHES IT, at the host's configured path -- the acquisition
    // door for a layout that had none.
    save_setup(t);
    CHECK_FALSE(t.session().notice_is_bad);
    CHECK(t.run().active_link.path == t.host.setup_path);
    CHECK(live_status(t.run()) == setup_link::kCurrent);
    CHECK(t.run().active_link.known == t.session().setup.active);

    // MUTATING THE DESK MAKES IT `modified`, DERIVED and never flagged...
    pick(t, stock::kKind);
    CHECK(live_status(t.run()) == setup_link::kModified);
    // ...and saving again re-establishes `current` on the SAME artifact.
    save_setup(t);
    CHECK(t.run().active_link.path == t.host.setup_path);
    CHECK(live_status(t.run()) == setup_link::kCurrent);
}

TEST_CASE("`r` establishes on success and changes nothing on refusal") {
    TempDir dir("wux11-restore");
    Live t;
    t.host.setup_path = dir.file("s.json");
    const Setup authored = arranged_desk("Restored");
    REQUIRE(setup_persist::save_file(t.host.setup_path, authored).accepted);
    (void)first_frame(t);
    REQUIRE(live_status(t.run()) == setup_link::kNone);

    // A SUCCESSFUL RESTORE INSTALLS THE DESK AND ESTABLISHES THE ASSOCIATION, whose known
    // value is exactly what was admitted.
    t.key(input::scan::kR);
    CHECK_FALSE(t.session().notice_is_bad);
    CHECK(t.session().setup.active == authored);
    CHECK(t.run().active_link.path == t.host.setup_path);
    CHECK(t.run().active_link.known == authored);
    CHECK(live_status(t.run()) == setup_link::kCurrent);

    // A REFUSED RESTORE LEAVES THE DESK AND THE ASSOCIATION EXACTLY AS THEY WERE. The
    // loader RETURNS a candidate, so there is no path by which half a desk is installed --
    // and nothing about association truth may advance on the way past a refusal either.
    spillout(t.host.setup_path, "{ this is not a setup");
    t.key(input::scan::kR);
    CHECK(t.session().notice_is_bad);
    CHECK(t.session().setup.active == authored);
    CHECK(t.run().active_link.path == t.host.setup_path);
    CHECK(t.run().active_link.known == authored);
    CHECK(live_status(t.run()) == setup_link::kCurrent);

    // A LAYOUT WITH NO ASSOCIATION SURVIVES A REFUSAL AS `none`, which is the other half:
    // a failed acquisition is not an acquisition.
    Live bad;
    bad.host.setup_path = dir.file("broken.json");
    spillout(bad.host.setup_path, "{ nor is this");
    (void)first_frame(bad);
    const Setup before = bad.session().setup.active;
    bad.key(input::scan::kR);
    CHECK(bad.session().notice_is_bad);
    CHECK(bad.session().setup.active == before);
    CHECK(live_status(bad.run()) == setup_link::kNone);
    CHECK(bad.run().active_link.path.empty());
}

TEST_CASE("two layouts sharing one artifact cannot both claim `current`") {
    // THE SHARED-ARTIFACT LAW, THROUGH THE REAL DOORS. Both layouts refer to one file;
    // one of them overwrites it; the other must stop claiming to match it.
    TempDir dir("wux11-shared");
    Live t;
    t.host.setup_path = dir.file("shared.json");
    (void)first_frame(t);

    save_setup(t); // the first layout acquires the artifact
    REQUIRE(t.run().active_link.path == t.host.setup_path);
    // A SECOND LAYOUT ON THE SAME FILE: made blank, then saved to the same configured path.
    layout_new(t);
    REQUIRE(live_status(t.run()) == setup_link::kNone);
    pick(t, stock::kKind); // make it a genuinely different desk
    save_setup(t);
    REQUIRE(t.run().active_link.path == t.host.setup_path);
    REQUIRE(layout_count(t.session().setup) == 2);

    // THE ONE THAT JUST WROTE IT SAYS `current`; the one whose value it replaced says
    // `modified`. Updating only the saving layout's baseline is the defect this refuses.
    CHECK(live_status(t.run()) == setup_link::kCurrent);
    CHECK(link_at(t.run(), 0).path == t.host.setup_path);
    CHECK(link_status(layout_at(t.run(), 0), link_at(t.run(), 0)) ==
          setup_link::kModified);
    // ...and both baselines are the SAME value, which is what the file now holds.
    CHECK(link_at(t.run(), 0).known == t.run().active_link.known);
    CHECK(setup_persist::load_file(t.host.setup_path).setup ==
          t.run().active_link.known);

    // AND READING IT BACK FROM THE OTHER SIDE TEACHES BOTH TOO.
    layout_next(t);
    REQUIRE(t.session().setup.active_at == 0);
    t.key(input::scan::kR);
    CHECK(live_status(t.run()) == setup_link::kCurrent);
    CHECK(link_status(layout_at(t.run(), 1), link_at(t.run(), 1)) ==
          setup_link::kCurrent);
}

TEST_CASE("the standing verdict performs no filesystem read") {
    // THE SHARPEST FALSIFIER FOR "current MEANS WORKSHOP'S KNOWLEDGE". The artifact is
    // DELETED after the save; a composition that went to disk to decide what to paint
    // would have to change its answer, and this one does not -- because `current` is a
    // claim about what this run last successfully wrote, and that fact did not move.
    TempDir dir("wux11-noread");
    Live t;
    t.host.setup_path = dir.file("gone.json");
    (void)first_frame(t);
    save_setup(t);
    REQUIRE(live_status(t.run()) == setup_link::kCurrent);

    std::error_code ec;
    std::filesystem::remove(t.host.setup_path, ec);
    REQUIRE_FALSE(std::filesystem::exists(t.host.setup_path));

    const Screen sc = screen_of(t.session());
    for (int again = 0; again < 3; ++again) {
        CHECK(band_status(t.session(), sc).text.find("| current") != std::string::npos);
        CHECK(link_status(t.session().setup.active, t.run().active_link) ==
              setup_link::kCurrent);
    }
    // ...AND THE MISSING FILE IS DISCOVERED ONLY WHEN A WEAVER ASKS FOR IT, which is the
    // honest boundary: an explicit operation touches the artifact, and it says so.
    t.key(input::scan::kR);
    CHECK(t.session().notice_is_bad);
}

TEST_CASE("the whole run and every association come back after a restart") {
    // THE COMPLETION SENTENCE'S DURABLE HALF. Three layouts: one associated and matching,
    // one associated and diverged, one with no association at all -- and the run's order,
    // names, active position and every one of those three verdicts return.
    TempDir dir("wux11-durable");
    const std::string session = dir.file("session.json");
    const std::string artifact = dir.file("code.json");

    std::vector<Layout> authored;
    {
        Live t;
        t.host.session_path = session;
        t.host.setup_path = artifact;
        t.ready();
        t.extent_px(cells_px(120), cells_px(44));

        // ⚠ ONE `--setup` PATH MEANS ONE ARTIFACT PER RUN, so both saves land on the same
        // file and the shared-artifact law decides the two verdicts: the layout that wrote
        // it last matches it, and the one whose value it replaced does not.
        rename_live_layout(t, "Home");
        save_setup(t);
        REQUIRE(live_status(t.run()) == setup_link::kCurrent);

        layout_new(t);
        rename_live_layout(t, "Code");
        pick(t, stock::kKind); // a genuinely different desk
        save_setup(t); // Code: associated and current; Home: associated and MODIFIED
        REQUIRE(live_status(t.run()) == setup_link::kCurrent);
        REQUIRE(link_status(layout_at(t.run(), 0),
                            link_at(t.run(), 0)) == setup_link::kModified);

        layout_new(t);
        rename_live_layout(t, "Art"); // Art: no association at all
        REQUIRE(live_status(t.run()) == setup_link::kNone);

        layout_next(t); // wrap to Home, and close standing on it
        REQUIRE(t.session().setup.active_at == 0);
        authored = layout_run(t.run());
        t.key(input::scan::kQ);
        REQUIRE(t.host.quit);
    }

    // THE FILE SAYS IT, at the current version, with a link per layout.
    const session_persist::LoadedSession read = session_persist::load_file(session);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.layouts == authored);
    CHECK(read.active == 0);
    CHECK(slurp(session).find("\"version\":12") != std::string::npos);
    CHECK(slurp(session).find("\"link\":") != std::string::npos);

    // AND THE NEXT RUN COMES BACK ON ALL THREE, with all three verdicts.
    Live back;
    back.host.session_path = session;
    back.host.setup_path = dir.file("somewhere-else.json");
    back.ready();
    back.extent_px(cells_px(120), cells_px(44));

    REQUIRE(layout_count(back.session().setup) == 3);
    CHECK(layout_run(back.run()) == authored);
    CHECK(back.session().setup.active_at == 0);
    CHECK(back.session().setup.active.name == "Home"); // the rename survived
    CHECK(link_status(layout_at(back.run(), 0), link_at(back.run(), 0)) ==
          setup_link::kModified);
    CHECK(link_status(layout_at(back.run(), 1), link_at(back.run(), 1)) ==
          setup_link::kCurrent);
    CHECK(link_status(layout_at(back.run(), 2), link_at(back.run(), 2)) ==
          setup_link::kNone);
    CHECK(link_at(back.run(), 0).path == artifact);
    CHECK(link_at(back.run(), 2).path.empty());
    // ...and the standing row says the one a weaver is standing on, and only that one.
    CHECK(band_status(back.session(), screen_of(back.session())).text.find("| modified") !=
          std::string::npos);

    // AND THE STANDALONE SETUP FILE IS STILL EXACTLY ONE DESK, with no layout run and no
    // association in it.
    const setup_persist::LoadedSetup file = setup_persist::load_file(artifact);
    REQUIRE(file.outcome.accepted);
    const std::string bytes = slurp(artifact);
    // ⚠ THE RUN'S OWN FIELD, NOT THE WORD. `layouts` is a pane KEY as well as the session's
    // field name, and a desk that names the Layouts pane says the word legitimately; what a
    // setup file must not contain is the session's layout ARRAY.
    CHECK(bytes.find("\"layouts\":[") == std::string::npos);
    CHECK(bytes.find("\"link\"") == std::string::npos);
    CHECK(bytes.find("\"active\"") == std::string::npos);
    CHECK(bytes.find("\"known\"") == std::string::npos);
}

TEST_CASE("every position and every association combination round-trips") {
    // SWEPT, because the durable representation has two independent axes now: which
    // position is live, and which layouts are associated.
    const std::vector<Setup> desks = three_desks();
    for (std::size_t active = 0; active < desks.size(); ++active) {
        for (int mask = 0; mask < 8; ++mask) {
            CAPTURE(active);
            CAPTURE(mask);
            std::vector<Layout> run;
            for (std::size_t at = 0; at < desks.size(); ++at) {
                if ((mask & (1 << at)) != 0) {
                    // Associated, and deliberately DIVERGED for one of them, so the two
                    // verdicts are both exercised rather than only the equal one.
                    Setup known = desks[at];
                    if (at == 1) {
                        known.name = "Whatever the file held";
                    }
                    run.push_back(Layout{desks[at],
                                         SetupLink{"/w/" + desks[at].name + ".json", known}});
                } else {
                    run.push_back(Layout{desks[at], SetupLink{}});
                }
            }
            const std::string text = session_text(run, active);
            const session_persist::LoadedSession read = session_persist::from_text(text);
            REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
            CHECK(read.layouts == run);
            CHECK(read.active == active);
            // AND THE RUNTIME LIFT IS THE INVERSE, associations included.
            SetupState state;
            REQUIRE(install_layout_run(state, read.layouts, read.active));
            CHECK(layout_run(state) == run);
            for (std::size_t at = 0; at < run.size(); ++at) {
                CAPTURE(at);
                CHECK(link_at(state, at) == run[at].link);
                CHECK(link_status(layout_at(state, at), link_at(state, at)) ==
                      (run[at].link.path.empty()
                           ? setup_link::kNone
                           : (at == 1 ? setup_link::kModified : setup_link::kCurrent)));
            }
        }
    }
}

TEST_CASE("a current-version session with half an association is refused") {
    // A LAYOUT THAT NAMES NO FILE AND REMEMBERS A DESK ANYWAY says two contradictory things
    // about itself, and absence has exactly one spelling here -- the law `placement_in` keeps
    // one field over.
    MountedHistory history;
    REQUIRE(history.mounted.ok);
    session_persist::WorkshopSession forged = hand_built(2, 0);
    forged.layouts[1].link.known = setup_persist::to_setup(arranged_desk("Invented"));
    const session_persist::LoadedSession refused =
        session_persist::from_text(as_text(forged), &history.catalog);
    CHECK_FALSE(refused.outcome.accepted);
    CHECK(refused.outcome.refusal.find("layout at position 1") == 0);
    CHECK(refused.outcome.refusal.find("remembers nothing") != std::string::npos);

    // ...AND AN ASSOCIATION WHOSE REMEMBERED DESK IS NOT A LEGAL SETUP is refused too, in
    // the setup owner's own words behind this file's position.
    session_persist::WorkshopSession illegal = hand_built(2, 0);
    illegal.layouts[0].link.path = "/w/desk.json";
    illegal.layouts[0].link.known = setup_persist::to_setup(arranged_desk("Fine"));
    illegal.layouts[0].link.known.panes[0].pane = "two words";
    const session_persist::LoadedSession no =
        session_persist::from_text(as_text(illegal), &history.catalog);
    CHECK_FALSE(no.outcome.accepted);
    CHECK(no.outcome.refusal.find("layout at position 0") == 0);
    CHECK(no.outcome.refusal.find("remembered Setup value") != std::string::npos);
}

TEST_CASE("a version-4 session opens with its run whole and every link none") {
    // FIELD DEFAULTING AND NOT INFERRED INTENT. A version-4 session could not say that a
    // desk was related to a standalone artifact, so the truthful reading is *these desks,
    // in this order, standing on that one, and no artifact is known for any of them*.
    // Inventing an association out of the host's configured `--setup` path would be this
    // reader deciding something the weaver never wrote down.
    MountedHistory history;
    REQUIRE(history.mounted.ok);

    session_history::v4::WorkshopSession old;
    old.format = session_persist::kFormat;
    old.format_version = session_history::kV4FormatVersion;
    old.viewport = session_history::v6::WorkshopViewport{132, 41};
    for (const Setup& desk : three_desks()) {
        old.layouts.push_back(v3_desk(desk));
    }
    old.active = 2;
    old.placement.mode = window_persist::kPlacementDesktop;
    old.placement.x = -640;
    old.placement.y = 96;
    old.placement.window = window_persist::kWindowMaximized;

    const session_persist::LoadedSession read = session_persist::from_text(
        loom::compat::serialize(loom::to_value(old)), &history.catalog);
    const window_persist::LoadedWindow room_read = window_persist::import_from_session_text(loom::compat::serialize(loom::to_value(old)));
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);

    // THE RUN IS THE OLD RUN, EXACTLY -- order, names, authored geometry and the position
    // that was live -- each desk carrying the layout surface its vintage had.
    REQUIRE(read.layouts.size() == 3);
    CHECK(read.active == 2);
    std::vector<Setup> expected;
    for (const Setup& desk : three_desks()) {
        expected.push_back(materialized(desk));
    }
    CHECK(desks_of(read.layouts) == expected);
    // ...AND EVERY ASSOCIATION IS `none`.
    for (const Layout& layout : read.layouts) {
        CHECK(layout.link.path.empty());
        CHECK(link_status(layout.desk, layout.link) == setup_link::kNone);
    }
    // EVERY NON-LAYOUT FACT CROSSES UNCHANGED, including a REAL placement.
    CHECK(room_read.viewport_w == cells_px(132));
    CHECK(room_read.viewport_h == cells_px(41));
    CHECK(room_read.honoured);
    CHECK(room_read.placement.known);
    CHECK(room_read.placement.x == -640);
    CHECK(room_read.placement.maximized);

    // AND THE EDGE IS ONE AUTHORED CONVERSION, SPENT ONCE.
    const std::uint64_t before = op::invocations();
    (void)session_persist::from_text(loom::compat::serialize(loom::to_value(old)),
                                     &history.catalog);
    CHECK(op::invocations() == before + 1);
    REQUIRE(history.catalog.find("zengine.migrate.WorkshopSession.v4-to-v12") != nullptr);
    // ...and no intermediate rung was added for the older vintages to be routed through.
    CHECK(history.catalog.find("zengine.migrate.WorkshopSession.v1-to-v4") == nullptr);
    CHECK(history.catalog.find("zengine.migrate.WorkshopSession.v3-to-v4") == nullptr);
}

TEST_CASE("a maximal legal session is still one this build can read back") {
    // THE DERIVED CEILING, RE-MEASURED FOR THE SECOND DESK PER LAYOUT. `kMaxLayouts`
    // layouts of a maximal desk AND a maximal remembered value is the largest legal file
    // this build writes; a ceiling left at one desk per layout would let `q` write a file
    // the next launch refuses, which is the worst thing a durable owner can do. Every desk at
    // every bound is also the most decoded cells a session holds: what Loom's decode budget
    // must admit, which this read meets for real.
    std::vector<Layout> run;
    for (std::size_t i = 0; i < kMaxLayouts; ++i) {
        Setup s = setup_of(std::string(kMaxSetupNameLen, static_cast<char>('a' + i)), {});
        fill_to_every_bound(s);
        // ...AND ITS ASSOCIATION REMEMBERS A MAXIMAL DESK TOO, which is the whole point.
        run.push_back(Layout{s, SetupLink{std::string(256, 'p') + ".json", s}});
    }
    const std::string text =
        session_text(run, kMaxLayouts - 1);
    CHECK(text.size() <= session_persist::kMaxSessionBytes);
    // ...AND IT IS BIGGER THAN A SINGLE DESK MAY BE: the plural's half of the same argument,
    // and why the ceiling is not the desk's.
    CHECK(text.size() > setup_persist::kMaxSetupBytes);
    // AND THE ASSOCIATIONS ARE WHAT MADE IT GROW AGAIN, measured rather than reasoned
    // about: the same run with every link cleared is barely half the size, so a ceiling
    // derived from one desk per layout would be a bound this build can write past.
    std::vector<Layout> bare = run;
    for (Layout& layout : bare) {
        layout.link = SetupLink{};
    }
    const std::string without =
        session_text(bare, kMaxLayouts - 1);
    CHECK(text.size() > without.size() * 3 / 2);

    TempDir dir("wux11-full");
    const std::string path = dir.file("session.json");
    REQUIRE(save_last_run(path, run, kMaxLayouts - 1, cells_px(120), cells_px(44),
                          window_persist::Placement{})
                .accepted);
    const session_persist::LoadedSession read = session_persist::load_file(path);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.layouts == run);
}

// ========================================================================================
// A session read a layout at a time: its file is a header line and a line per layout, each
// line one value decoded alone at its own shape, and the file is refused whole where any line
// is refused or the lines are not the header's count.
// ========================================================================================

namespace {

/// A session file's lines, as its reader takes them: each ended by a newline.
std::vector<std::string> session_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::size_t from = 0;
    while (from < text.size()) {
        const std::size_t end = text.find('\n', from);
        lines.push_back(text.substr(from, end == std::string::npos ? std::string::npos : end - from));
        from = end == std::string::npos ? text.size() : end + 1;
    }
    return lines;
}

std::string joined_lines(const std::vector<std::string>& lines) {
    std::string out;
    for (const std::string& line : lines) {
        out += line + "\n";
    }
    return out;
}

/// Three layouts, the middle one live and linked to a Setup file whose name holds a newline.
std::vector<Layout> three_linked() {
    std::vector<Layout> run = {Layout{arranged_desk("A"), SetupLink{}},
                               Layout{arranged_desk("B"), SetupLink{}},
                               Layout{arranged_desk("C"), SetupLink{}}};
    run[1].link = SetupLink{"two\nlines.json", run[1].desk};
    return run;
}

} // namespace

TEST_CASE("a session's file is a header line and a line for each layout, each decoded alone") {
    const std::vector<Layout> run = three_linked();
    const std::vector<session_persist::WorkshopChoice> chose = {
        session_persist::WorkshopChoice{"zengine.editor", "vim"}};
    const std::string text = session_text(run, 1, chose);
    // EVERY LINE ENDS IN A NEWLINE, and a newline inside a value is the JSON's escape, never a
    // line of its own: the path above holds one.
    REQUIRE_FALSE(text.empty());
    CHECK(text.back() == '\n');
    const std::vector<std::string> lines = session_lines(text);
    REQUIRE(lines.size() == run.size() + 1);
    const loom::Unverified header = loom::compat::parse(lines[0]);
    REQUIRE(header.well_formed());
    CHECK(header.claimed_name() == "WorkshopSessionHeader");
    CHECK(header.claimed_version() == static_cast<std::uint32_t>(session_persist::kFormatVersion));
    // EACH LAYOUT IS A VALUE OF ITS OWN, admitted alone at the shape a layout has.
    for (std::size_t at = 0; at < run.size(); ++at) {
        CAPTURE(at);
        const loom::Admission one =
            loom::admit(loom::compat::parse(lines[at + 1]),
                        loom::schema_of<session_persist::WorkshopLayout>(), loom::Report::FirstError);
        REQUIRE_MESSAGE(one.ok(), one.first_error().message());
        CHECK(loom::from_value<session_persist::WorkshopLayout>(one.value()).desk.name ==
              run[at].desk.name);
    }
    // ...AND THE LINES READ BACK AS THE SESSION THEY WROTE, a Windows line end among them.
    for (const std::string& file : {text, [&] {
             std::string crlf;
             for (const std::string& line : lines) {
                 crlf += line + "\r\n";
             }
             return crlf;
         }()}) {
        const session_persist::LoadedSession read = session_persist::from_text(file);
        REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
        CHECK(read.layouts == run);
        CHECK(read.active == 1);
        REQUIRE(read.choices.size() == 1);
        CHECK(read.choices[0].choice == "vim");
    }
}

TEST_CASE("a session's file is refused whole where a line is refused or the lines are not the header's count") {
    const std::vector<Layout> run = three_linked();
    const std::vector<std::string> lines = session_lines(session_text(run, 1));
    REQUIRE(lines.size() == 4);
    struct Case {
        const char* what;
        std::vector<std::string> lines;
        const char* said;
    };
    const std::vector<Case> cases = {
        {"a layout line missing", {lines[0], lines[1], lines[2]},
         "this session's header counts 3 layouts, and 2 lines follow it"},
        {"a layout line too many", {lines[0], lines[1], lines[2], lines[3], lines[3]},
         "this session's header counts 3 layouts, and 4 lines follow it"},
        {"an empty line among them", {lines[0], lines[1], "", lines[3]}, "layout at position 1: "},
        {"a layout line that is no layout", {lines[0], lines[1], lines[0], lines[3]},
         "layout at position 1: "},
        {"a layout line that its own law refuses", [&] {
             std::vector<std::string> bad = lines;
             const std::size_t at = bad[3].find("\"name\":\"C\"");
             REQUIRE(at != std::string::npos);
             bad[3].replace(at, std::string("\"name\":\"C\"").size(), "\"name\":\"\"");
             return bad;
         }(), "layout at position 2: "},
    };
    for (const Case& c : cases) {
        CAPTURE(c.what);
        const session_persist::LoadedSession read = session_persist::from_text(joined_lines(c.lines));
        CHECK_FALSE(read.outcome.accepted);
        CHECK(read.layouts.empty());
        CHECK_MESSAGE(read.outcome.refusal.rfind(c.said, 0) == 0, read.outcome.refusal);
    }
}

TEST_CASE("a maximal session decodes a line at a time, each line with room for a field on every row") {
    // EVERY BOUND AT ONCE: `kMaxLayouts` layouts, each desk and each remembered value full, and
    // every choice a session remembers with its office's name at its bound, spelled in the
    // characters JSON spells longest.
    std::vector<Layout> run;
    for (std::size_t i = 0; i < kMaxLayouts; ++i) {
        Setup s = setup_of(std::string(kMaxSetupNameLen, static_cast<char>('a' + i)), {});
        fill_to_every_bound(s);
        run.push_back(Layout{s, SetupLink{std::string(session_persist::kMaxLinkPathBytes, 'p'), s}});
    }
    window_persist::Placement place;
    place.known = true;
    place.x = std::numeric_limits<std::int64_t>::min();
    place.y = std::numeric_limits<std::int64_t>::min();
    place.maximized = true;
    place.room_w = std::numeric_limits<std::int64_t>::min();
    place.room_h = std::numeric_limits<std::int64_t>::min();
    for (const std::string& spelled : {std::string(load::kMaxWeaveRoleLen, '"'),
                                       std::string(load::kMaxWeaveRoleLen, '\\'),
                                       [] {
                                           std::string accented;
                                           while (accented.size() < load::kMaxWeaveRoleLen) {
                                               accented += "\xc3\xa9";
                                           }
                                           return accented;
                                       }()}) {
        const std::vector<session_persist::WorkshopChoice> chose(
            session_persist::kMaxRememberedChoices,
            session_persist::WorkshopChoice{spelled, std::string(load::kMaxChoiceNameLen, 'z')});
        const std::string text = session_text(run, kMaxLayouts - 1, chose);
        CHECK(text.size() <= session_persist::kMaxSessionBytes);
        const std::vector<std::string> lines = session_lines(text);
        REQUIRE(lines.size() == kMaxLayouts + 1);
        CHECK(lines[0].size() <= session_persist::kMaxHeaderLineBytes);
        const loom::Admission header =
            loom::admit(loom::compat::parse(lines[0]),
                        loom::schema_of<session_persist::WorkshopSessionHeader>(),
                        loom::Report::FirstError);
        // A message reads the error only where there is one: doctest's `-s` says it on success too.
        REQUIRE_MESSAGE(header.ok(), (header.ok() ? std::string() : header.first_error().message()));
        const std::int64_t header_cells = decoded_cells(header.value());
        CHECK(header_cells <= kDecodedCellBudget);
        std::int64_t layout_cells = 0;
        for (std::size_t at = 1; at < lines.size(); ++at) {
            CAPTURE(at);
            CHECK(lines[at].size() <= session_persist::kMaxLayoutLineBytes);
            const loom::Admission layout =
                loom::admit(loom::compat::parse(lines[at]),
                            loom::schema_of<session_persist::WorkshopLayout>(),
                            loom::Report::FirstError);
            REQUIRE_MESSAGE(layout.ok(),
                            (layout.ok() ? std::string() : layout.first_error().message()));
            layout_cells = std::max(layout_cells, decoded_cells(layout.value()));
            // ONE FIELD MORE ON EVERY ROW of both its desks still decodes.
            CHECK(decoded_cells(layout.value()) + 2 * static_cast<std::int64_t>(kMaxSetupPanes) <=
                  kDecodedCellBudget);
        }
        // ...WHICH THE SAME SESSION AS ONE VALUE HAS NO ROOM FOR, measured.
        const std::int64_t whole = decoded_cells(loom::to_value(session_value(run, kMaxLayouts - 1, chose)));
        CHECK(whole + 2 * static_cast<std::int64_t>(kMaxSetupPanes * kMaxLayouts) >
              kDecodedCellBudget);
        MESSAGE("the header line: ", lines[0].size(), " bytes, ", header_cells,
                " cells; a layout line: ", lines[1].size(), " bytes, ", layout_cells,
                " cells; the file: ", text.size(), " bytes; as one value: ", whole, " cells");

        TempDir dir("session-lines-full");
        const std::string path = dir.file("session.json");
        REQUIRE(save_last_run(path, run, kMaxLayouts - 1, kScreenMaxW, kScreenMaxH, place, chose)
                    .accepted);
        const session_persist::LoadedSession read = session_persist::load_file(path);
        REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
        CHECK(read.layouts == run);
        CHECK(read.choices.size() == session_persist::kMaxRememberedChoices);
    }
}

TEST_CASE("a version-10 session comes forward whole and is written back in lines") {
    // ⚔ MUTATION: the v10 edge leaving out a field -- the room, the choices or the live one --
    // brings a session forward short of what its weaver left.
    MountedHistory history;
    REQUIRE_MESSAGE(history.mounted.ok, history.mounted.reason);
    window_persist::Placement place;
    place.known = true;
    place.x = 30;
    place.y = 40;
    place.room_w = 1500;
    place.room_h = 900;
    const std::vector<Layout> run = three_layouts();
    const session_history::v11::WorkshopSession now = v11_session(
        run, 2, cells_px(128), cells_px(40), place,
        {session_persist::WorkshopChoice{"zengine.editor", "neovim"}});
    session_history::v10::WorkshopSession old;
    old.format = session_persist::kFormat;
    old.format_version = session_history::kV10FormatVersion;
    old.viewport = now.viewport;
    old.layouts = now.layouts;
    old.active = now.active;
    old.placement = now.placement;
    old.choices = now.choices;
    old.window_room = now.window_room;
    const std::string text = loom::compat::serialize(loom::to_value(old));
    REQUIRE(history.catalog.find("zengine.migrate.WorkshopSession.v10-to-v12") != nullptr);
    const session_persist::LoadedSession read = session_persist::from_text(text, &history.catalog);
    const window_persist::LoadedWindow room_read = window_persist::import_from_session_text(text);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.layouts == run);
    CHECK(read.active == 2);
    CHECK(room_read.viewport_w == cells_px(128));
    CHECK(room_read.placement.known);
    CHECK(room_read.placement.x == 30);
    CHECK(room_read.placement.room_w == 1500);
    CHECK(room_read.placement.room_h == 900);
    REQUIRE(read.choices.size() == 1);
    CHECK(read.choices[0].choice == "neovim");
    // ...WITH NO CONVERSION LIVE IT IS REFUSED BY ITS NUMBER, naming the edge it lacks...
    const session_persist::LoadedSession none = session_persist::from_text(text);
    CHECK_FALSE(none.outcome.accepted);
    CHECK(none.outcome.refusal.find("zengine.migrate.WorkshopSession.v10-to-v12") !=
          std::string::npos);
    // ...AND WHAT IT BROUGHT FORWARD IS WRITTEN BACK AS THIS VERSION, a line a layout.
    TempDir dir("session-ten");
    const std::string path = dir.file("session.json");
    REQUIRE(save_last_run(path, read.layouts, read.active, room_read.viewport_w,
                          room_read.viewport_h, room_read.placement, read.choices)
                .accepted);
    const std::vector<std::string> lines = session_lines(slurp(path));
    REQUIRE(lines.size() == run.size() + 1);
    CHECK(loom::compat::parse(lines[0]).claimed_name() == "WorkshopSessionHeader");
    const session_persist::LoadedSession again = session_persist::load_file(path);
    REQUIRE_MESSAGE(again.outcome.accepted, again.outcome.refusal);
    CHECK(again.layouts == run);
    CHECK(window_persist::load_file(window_persist::path_beside(path)).placement.room_w == 1500);
}

TEST_CASE("a header counting more layouts than a Workshop keeps is refused before a line is read") {
    // EVERY LINE AFTER IT IS NO LAYOUT AT ALL: a reader that decoded them first would refuse
    // the first of them, and the bound is what must answer.
    std::vector<std::string> lines = session_lines(session_text(one_layout(arranged_desk("One")), 0));
    REQUIRE(lines.size() == 2);
    const std::string counted = "\"layout_count\":\"1\"";
    const std::size_t at = lines[0].find(counted);
    REQUIRE(at != std::string::npos);
    lines[0].replace(at, counted.size(),
                     "\"layout_count\":\"" + std::to_string(kMaxLayouts + 1) + "\"");
    lines.resize(1);
    lines.resize(kMaxLayouts + 2, "{}");
    const session_persist::LoadedSession read = session_persist::from_text(joined_lines(lines));
    CHECK_FALSE(read.outcome.accepted);
    CHECK(read.outcome.refusal == session_persist::too_many_layouts(kMaxLayouts + 1));
}

TEST_CASE("a version-11 session written as one value is refused, its file being a line a layout") {
    // THE ONE VALUE IS WHAT MEMORY HOLDS AND A CONVERSION ANSWERS WITH, never a file: read as
    // one, it would be the decode the lines exist to keep under Loom's budget.
    const std::string text = loom::compat::serialize(loom::to_value(session_value(one_layout(arranged_desk("One")), 0)));
    const session_persist::LoadedSession read = session_persist::from_text(text);
    CHECK_FALSE(read.outcome.accepted);
    CHECK(read.layouts.empty());
    CHECK(read.outcome.refusal == session_persist::one_value());
}

// ========================================================================================
// Yesterday's desks and the layout surface: a version-5 session's bytes mean the layout run
// was painted by `paint` itself, so a version-6 reading adds the ordinary Layouts pane where
// it always was. A predecessor session comes back with nothing lost, an explicit historical
// row is kept rather than duplicated, and old bytes still cannot load code or be overwritten.
// ========================================================================================

namespace {

/// A REAL VERSION 5 SESSION: three layouts that do not name the layout surface, in a weaver's
/// order, the middle one live and the first associated with a Setup artifact whose remembered
/// value it matches -- or, given an empty path, associated with nothing, in the ONE spelling
/// that means it (an empty path beside a remembered desk is `half_a_link`, refused). ⚠ Its
/// desks are `layout_of`'s: what a version-5 Workshop wrote, not what a later fixture would.
session_history::v5::WorkshopSession old_v5_session(const std::string& artifact) {
    session_history::v5::WorkshopSession old;
    old.format = session_persist::kFormat;
    old.format_version = session_history::kV5FormatVersion;
    old.viewport = session_history::v6::WorkshopViewport{132, 41};
    const std::vector<Setup> desks = three_desks();
    for (std::size_t i = 0; i < desks.size(); ++i) {
        session_history::v6::WorkshopLayout row;
        row.desk = v3_desk(desks[i]);
        // THE FIRST LAYOUT IS ASSOCIATED AND MATCHING: its remembered value is its own desk,
        // which is what `save_setup` leaves behind and what makes the row read `current`.
        row.link = i == 0 && !artifact.empty()
                       ? session_history::v6::WorkshopSetupLink{artifact, row.desk}
                       : session_history::absent_link();
        old.layouts.push_back(std::move(row));
    }
    old.active = 1;
    old.placement.mode = window_persist::kPlacementDesktop;
    old.placement.x = -640;
    old.placement.y = 96;
    old.placement.window = window_persist::kWindowMaximized;
    return old;
}

std::string as_text(const session_history::v5::WorkshopSession& old) {
    return loom::compat::serialize(loom::to_value(old));
}

} // namespace

TEST_CASE("a real version-5 session comes back with nothing lost") {
    // THE MIGRATION WITNESS. Same layouts, order, names, active position, Setup association
    // AND its verdict, same authored pane geometry -- plus the layout surface every desk of
    // that vintage had, written down as a pane at the rectangle it always occupied. ⚔ MUTATION:
    // converting v5 without adding the row; every `materialized` comparison below goes red.
    const std::string artifact = "/kept/code.json";
    const session_history::v5::WorkshopSession old = old_v5_session(artifact);

    MountedHistory history;
    REQUIRE_MESSAGE(history.mounted.ok, history.mounted.reason);
    const session_persist::LoadedSession read =
        session_persist::from_text(as_text(old), &history.catalog);
    const window_persist::LoadedWindow room_read = window_persist::import_from_session_text(as_text(old));
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);

    // THE RUN IS THE OLD RUN, EXACTLY -- with one row appended to each desk.
    REQUIRE(read.layouts.size() == 3);
    CHECK(read.active == 1);
    std::vector<Setup> expected;
    for (const Setup& desk : three_desks()) {
        expected.push_back(materialized(desk));
    }
    CHECK(desks_of(read.layouts) == expected);
    for (std::size_t i = 0; i < read.layouts.size(); ++i) {
        CAPTURE(i);
        CHECK(read.layouts[i].desk.name == three_desks()[i].name);
        CHECK(has_pane(read.layouts[i].desk, ref_of(pane_kind::kLayouts)));
        // AT THE HISTORICAL DEFAULT AND UNAUTHORED: the pane comes back where the band was,
        // and a weaver who never chose a geometry still has not chosen one.
        const SetupPane* row = pane_of(read.layouts[i].desk, ref_of(pane_kind::kLayouts));
        REQUIRE(row != nullptr);
        CHECK(row->place.mode == pane_unit::kDefault);
        CHECK(row->width.mode == pane_unit::kDefault);
        CHECK(row->height.mode == pane_unit::kDefault);
        // ...AND FRONT-MOST, because the surface it replaces was painted after every pane.
        CHECK(row->front ==
              static_cast<std::int64_t>(read.layouts[i].desk.panes.size()) - 1);
    }

    // THE ASSOCIATION SURVIVES *AND SO DOES ITS VERDICT*. The remembered value is
    // converted with the desk, so a layout that matched its artifact still matches it --
    // a weaver must not be told their desk drifted from a file by an upgrade they did not
    // make. And a layout that named no artifact remembers nothing, still.
    CHECK(read.layouts[0].link.path == artifact);
    CHECK(link_status(read.layouts[0].desk, read.layouts[0].link) == setup_link::kCurrent);
    for (std::size_t i = 1; i < read.layouts.size(); ++i) {
        CAPTURE(i);
        CHECK(read.layouts[i].link.path.empty());
        CHECK(link_status(read.layouts[i].desk, read.layouts[i].link) == setup_link::kNone);
    }

    // EVERY NON-LAYOUT FACT CROSSES UNCHANGED, including a real placement.
    CHECK(room_read.viewport_w == cells_px(132));
    CHECK(room_read.viewport_h == cells_px(41));
    CHECK(room_read.honoured);
    CHECK(room_read.placement.known);
    CHECK(room_read.placement.x == -640);
    CHECK(room_read.placement.y == 96);
    CHECK(room_read.placement.maximized);

    // AND THE EDGE IS ONE AUTHORED CONVERSION, SPENT ONCE.
    REQUIRE(history.catalog.find("zengine.migrate.WorkshopSession.v5-to-v12") != nullptr);
    const std::uint64_t before = op::invocations();
    REQUIRE(session_persist::from_text(as_text(old), &history.catalog).outcome.accepted);
    CHECK(op::invocations() == before + 1);
}

TEST_CASE("the weaver sees no loss, and the next run spends no conversion") {
    // THE RESTART WITNESS, END TO END AND THROUGH THE REAL DOORS. A weaver's own
    // predecessor session file is on disk; a Workshop of THIS build opens it, and what they
    // see is the desk they left -- tab run included, at the top of the screen, pressable.
    // They close it in the ordinary way, and the next run reads the file with no conversion
    // mounted at all.
    TempDir dir("wux12-restart");
    const std::string session = dir.file("session.json");
    spillout(session, as_text(old_v5_session(std::string())));

    std::vector<Layout> after_open;
    {
        MountedHistory history;
        REQUIRE_MESSAGE(history.mounted.ok, history.mounted.reason);
        Live t;
        t.host.session_path = session;
        t.host.conversions = &history.catalog;
        t.ready();
        t.extent_px(cells_px(132), cells_px(41));
        REQUIRE_MESSAGE(!t.session().notice_is_bad, t.notice());

        // THE RUN IS BACK, standing where the weaver left it.
        CHECK(layout_count(t.session().setup) == 3);
        CHECK(t.session().setup.active_at == 1);
        CHECK(layout_at(t.run(), 1).name == three_desks()[1].name);

        // ...AND THE TAB SURFACE IS THERE, at the rectangle it always occupied, saying what it
        // always said, and answering a press: no apparent loss of the weaver's tab surface.
        const Screen sc = screen_of(t.session());
        REQUIRE(t.session().panes.has(pane_kind::kLayouts));
        CHECK(bounds_of(t.session().panes, t.session().setup.active, pane_kind::kLayouts, sc)
                  .rect == top_band_bounds(sc));
        const BandStatus row = band_status(t.session(), sc);
        CHECK(row.text.find(">" + layout_at(t.run(), 1).name + "<") !=
              std::string::npos);
        REQUIRE(row.tabs.size() == 3);
        press_tab(t, 0);
        CHECK(t.session().setup.active_at == 0);

        after_open = layout_run(t.run());
        t.key(input::scan::kQ);
        REQUIRE(t.host.quit);
    }

    // THE FILE IS THE CURRENT SHAPE NOW, and only the current shape.
    const std::string bytes = slurp(session);
    CHECK(bytes.find("\"version\":12") != std::string::npos);
    CHECK(bytes.find("\"format_version\":\"12\"") != std::string::npos);
    CHECK(bytes.find("\"pane\":\"layouts\"") != std::string::npos);

    // ...AND THE NEXT RUN READS IT WITH NO CONVERSION IN THE ARRANGEMENT AT ALL, which is
    // what "migrated once" means: the conversion is spent on the bytes, not on every launch.
    const std::uint64_t before = op::invocations();
    const session_persist::LoadedSession again = session_persist::load_file(session);
    REQUIRE_MESSAGE(again.outcome.accepted, again.outcome.refusal);
    CHECK(op::invocations() == before);
    CHECK(again.layouts == after_open);
}

TEST_CASE("an explicit historical row is preserved, never duplicated") {
    // THE OTHER HALF OF "PRESERVE WHAT THE BYTES SAID": a version-5 file CAN already name
    // `zengine.workshop/layouts` (a pane key is an ordinary string), so a conversion appending
    // regardless would author a duplicate `check_setup` refuses, and shadow the weaver's
    // geometry. ⚔ MUTATION: dropping the `names_layouts` guard; the load below is refused.
    session_history::v5::WorkshopSession old;
    old.format = session_persist::kFormat;
    old.format_version = session_history::kV5FormatVersion;
    old.viewport = session_history::v6::WorkshopViewport{120, 40};
    Setup authored = setup_of("Deliberate", {second::kKind, pane_kind::kLayouts});
    // ...AND THE WEAVER PUT IT SOMEWHERE OF THEIR OWN, which is the fact a duplicate row
    // would hide behind a default.
    REQUIRE(author_pane_place(authored, ref_of(pane_kind::kLayouts), cells_px(4), cells_px(9)).accepted);
    REQUIRE(send_to_back(authored, ref_of(pane_kind::kLayouts)));
    old.layouts.push_back(
        session_history::v6::WorkshopLayout{v3_desk(authored), session_history::absent_link()});
    old.active = 0;
    old.placement = session_history::absent_placement();

    MountedHistory history;
    REQUIRE(history.mounted.ok);
    const session_persist::LoadedSession read =
        session_persist::from_text(as_text(old), &history.catalog);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);

    // ONE ROW, AND IT IS THE WEAVER'S OWN -- same place, same rank, unchanged.
    CHECK(live_layout(read) == authored);
    std::size_t named = 0;
    for (const SetupPane& row : live_layout(read).panes) {
        named += row.ref == ref_of(pane_kind::kLayouts) ? 1u : 0u;
    }
    CHECK(named == 1);
    const SetupPane* row = pane_of(live_layout(read), ref_of(pane_kind::kLayouts));
    REQUIRE(row != nullptr);
    CHECK(row->place.mode == pane_unit::kPixels);
    CHECK(row->place.x == cells_px(4));
    CHECK(row->place.y == cells_px(9));
    CHECK(row->front == 0);
}

TEST_CASE("a version-5 file with no conversion live refuses, and is not rewritten") {
    // THE AUTHORITY STORY FOR THE VERSION 5 VINTAGE, unchanged: conversion power comes from a
    // row in an arrangement, not from the bytes, and an unreadable session is never written
    // over by an orderly close. ⚔ MUTATION: removing the provider's v5 edge reddens the
    // restart witness above; letting the current reader keep a v5 branch reddens this one.
    TempDir dir("wux12-absent");
    const std::string path = dir.file("session.json");
    const std::string before = as_text(old_v5_session(std::string()));
    spillout(path, before);

    Live t;
    t.host.session_path = path; // no `conversions` hook: this arrangement mounts none
    t.ready();
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("`zengine.migrate.WorkshopSession.v5-to-v12`") != std::string::npos);
    // ...and it claims nothing it cannot know.
    CHECK(t.notice().find("install") == std::string::npos);

    t.extent_px(cells_px(132), cells_px(41));
    t.key(input::scan::kQ);
    REQUIRE(t.host.quit);
    CHECK(slurp(path) == before); // byte-identical after an orderly close
}

TEST_CASE("a full desk refuses the conversion rather than losing either fact") {
    // THE ONE PLACE THE MIGRATION CANNOT BE HONEST AND SILENT. A version-5 desk already
    // holding `kMaxSetupPanes` panes cannot also hold the pane its layout surface became,
    // and both quiet answers are lies: dropping the surface says the weaver removed it, and
    // dropping one of their panes says they never had it. So it refuses in words -- and
    // because nothing in the history file writes anything, the file is still there for a
    // build that can say more.
    Setup full;
    full.name = "Crowded";
    for (std::size_t i = 0; i < kMaxSetupPanes; ++i) {
        REQUIRE(add_pane(full, PaneRef{"third.party.tools", "p" + std::to_string(i)}));
    }
    REQUIRE(full.panes.size() == kMaxSetupPanes);

    session_history::v5::WorkshopSession old;
    old.format = session_persist::kFormat;
    old.format_version = session_history::kV5FormatVersion;
    old.viewport = session_history::v6::WorkshopViewport{120, 40};
    old.layouts.push_back(
        session_history::v6::WorkshopLayout{v3_desk(full), session_history::absent_link()});
    old.active = 0;
    old.placement = session_history::absent_placement();

    MountedHistory history;
    REQUIRE(history.mounted.ok);
    const session_persist::LoadedSession no =
        session_persist::from_text(as_text(old), &history.catalog);
    CHECK_FALSE(no.outcome.accepted);
    CHECK(no.outcome.refusal.find(std::to_string(kMaxSetupPanes)) != std::string::npos);
    CHECK(no.outcome.refusal.find("layout surface") != std::string::npos);
}

// ============================================================================
// A pane that changed hands, and the one rewrite that says so
// ============================================================================
// Saved files spell the browser `zengine.workshop/project-files`; this build's is
// `zengine.files/project-files`. The rewrite runs INSIDE `setup_in`, so it reaches the setup
// file, every session desk and every link's remembered value, over every admitted version.

namespace {

/// The reference a saved file wrote for the built-in browser, and the one it means now.
/// Spelled as literals on purpose: these are claims about BYTES ALREADY ON DISK, and a
/// fixture that asked the current build what the name is could not fail when the name moved.
inline PaneRef old_files_ref() { return PaneRef{"zengine.workshop", "project-files"}; }
inline PaneRef new_files_ref() { return PaneRef{"zengine.files", "project-files"}; }

/// A setup naming one built-in and the browser, in that order, with authored geometry on
/// the browser's row -- because "the row survived" has to mean the whole row.
inline Setup desk_with_the_browser() {
    Setup s;
    s.name = "Yesterday";
    REQUIRE(add_pane(s, info_ref()));
    REQUIRE(add_pane(s, old_files_ref()));
    const std::size_t row = pane_row(s, old_files_ref());
    REQUIRE(row != kNoPaneRow);
    s.panes[row].place = PanePlace{pane_unit::kPixels, 96, 32};
    s.panes[row].width = PaneSize{pane_unit::kPixels, 320};
    s.panes[row].height = PaneSize{pane_unit::kPixels, 240};
    return s;
}

} // namespace

TEST_CASE("a saved setup naming the built-in browser opens as the loaded pane") {
    // THE WHOLE CLAIM, AND IT IS ABOUT ONE ROW. The office moved; the pane key, the
    // place, both sizes and the front order did not, because none of them changed hands.
    //
    // ⚔ MUTATION: dropping the rewrite from `setup_in`. The reference comes back naming
    // `zengine.workshop`, and this build's pane catalog answers nothing for it -- the
    // weaver's Files pane is a row that resolves to nothing, silently.
    const Setup wrote = desk_with_the_browser();
    const setup_persist::LoadedSetup read =
        setup_persist::from_text(setup_persist::to_text(wrote));
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.converted.total() == 1);

    REQUIRE(read.setup.panes.size() == 2);
    CHECK(read.setup.panes[0].ref == info_ref());
    CHECK(read.setup.panes[1].ref == new_files_ref());
    // THE REST OF THE ROW IS THE WEAVER'S, UNTOUCHED.
    CHECK(read.setup.panes[1].place.mode == pane_unit::kPixels);
    CHECK(read.setup.panes[1].place.x == 96);
    CHECK(read.setup.panes[1].place.y == 32);
    CHECK(read.setup.panes[1].width.amount == 320);
    CHECK(read.setup.panes[1].height.mode == pane_unit::kPixels);
    CHECK(read.setup.panes[1].height.amount == 240);
    CHECK(read.setup.panes[1].front == wrote.panes[1].front);
    // ...AND THE NAME AND THE ORDER OF THE OTHER ROW ARE TOO.
    CHECK(read.setup.name == "Yesterday");

    // AND THE NEXT SAVE WRITES THE NEW SPELLING, which is what makes the conversion
    // something spent once on the bytes rather than at every launch (WL-MIG-10's rule,
    // reading never rewrites: the FILE changes at the weaver's next ordinary save).
    const std::string again = setup_persist::to_text(read.setup);
    CHECK(again.find("\"provider\":\"zengine.files\"") != std::string::npos);
    CHECK(again.find("\"provider\":\"zengine.workshop\",\"pane\":\"project-files\"") ==
          std::string::npos);
    const setup_persist::LoadedSetup back = setup_persist::from_text(again);
    REQUIRE(back.outcome.accepted);
    CHECK(back.converted.total() == 0);
    CHECK(back.setup == read.setup);
}

TEST_CASE("a setup that names no retired pane is not touched, and says so") {
    // THE OTHER HALF OF A MEASUREMENT. A converter that reported work it did not do would
    // make the transition note appear for every weaver in the world, forever.
    const Setup ordinary = setup_of("Ordinary", {second::kKind, stock::kKind});
    const std::string wrote = setup_persist::to_text(ordinary);
    const setup_persist::LoadedSetup read = setup_persist::from_text(wrote);
    REQUIRE(read.outcome.accepted);
    CHECK(read.converted.total() == 0);
    CHECK(read.setup == ordinary);
    CHECK(setup_persist::to_text(read.setup) == wrote);

    // ...AND NEITHER IS A THIRD PARTY'S PANE THAT MERELY SHARES ONE HALF OF THE NAME.
    // The rewrite is one named pair and not a pattern: an office match alone is not it,
    // and a key match alone is not it either.
    // ⚠ NOT `near`, which is a Win32 macro: `<windows.h>` defines it, and a case named for
    // what it is about would not compile on one of the two supported toolchains.
    Setup almost;
    almost.name = "Near misses";
    REQUIRE(add_pane(almost, PaneRef{"zengine.workshop", "project-files-2"}));
    REQUIRE(add_pane(almost, PaneRef{"third.party.tools", "project-files"}));
    REQUIRE(add_pane(almost, PaneRef{"zengine.files", "project-files"}));
    const setup_persist::LoadedSetup untouched =
        setup_persist::from_text(setup_persist::to_text(almost));
    REQUIRE(untouched.outcome.accepted);
    CHECK(untouched.converted.total() == 0);
    CHECK(untouched.setup == almost);
}

TEST_CASE("the legacy road converts too, because the browser is older than it") {
    // THE ARM A FORMAT VERSION WOULD HAVE LOST. This reader carries exactly one legacy
    // rung, so a conversion gated on a version bump would have brought version-3 files
    // forward and refused version-2 ones -- and a version-2 setup can name the browser as
    // easily as a version-3 one, because the pane predates both numbers.
    setup_persist::v2::WorkshopSetup old;
    old.format = setup_persist::kFormat;
    old.format_version = setup_persist::v2::kRetainedVersion;
    old.name = "Whole cells";
    setup_persist::v2::WorkshopSetupPane files;
    files.provider = "zengine.workshop";
    files.pane = "project-files";
    files.place = setup_persist::v2::WorkshopPanePlace{"cells", 3, 2};
    files.width = setup_persist::v2::WorkshopPaneSize{"cells", 28};
    files.height = setup_persist::v2::WorkshopPaneSize{"default", 0};
    files.front = 0;
    old.panes.push_back(files);

    const setup_persist::LoadedSetup read =
        setup_persist::from_text(loom::compat::serialize(loom::to_value(old)));
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.converted.total() == 1);
    REQUIRE(read.setup.panes.size() == 1);
    CHECK(read.setup.panes[0].ref == new_files_ref());
    // ...AND THE UNIT TRANSLATION THAT ROAD EXISTS FOR STILL HAPPENED. The two conversions
    // are independent and both are owed.
    CHECK(read.setup.panes[0].place.mode == pane_unit::kPixels);
    CHECK(read.setup.panes[0].width.amount == gfx::px_of_cells(28));
}

TEST_CASE("a file naming BOTH spellings is refused for naming one pane twice") {
    // THE ORDER IS THE CLAIM. The rewrite runs BEFORE the setup's own law, so a
    // contradictory file meets `check_setup` as what it actually is -- two rows for one
    // pane -- rather than being quietly installed as a desk holding the same pane twice.
    Setup both;
    both.name = "Contradiction";
    REQUIRE(add_pane(both, old_files_ref()));
    REQUIRE(add_pane(both, new_files_ref()));
    const setup_persist::LoadedSetup read =
        setup_persist::from_text(setup_persist::to_text(both));
    REQUIRE_FALSE(read.outcome.accepted);
    CHECK(read.outcome.refusal.find("zengine.files/project-files") != std::string::npos);
    CHECK(read.outcome.refusal.find("named twice") != std::string::npos);
    // AND NOTHING WAS HALF-INSTALLED: the whole candidate is a local of the reader.
    CHECK(read.setup.panes.empty());
}

TEST_CASE("every desk in a session is converted, and the run counts once") {
    // A WEAVER WITH EIGHT DESKS IS TOLD ONCE. The count is the run's, because "the Files
    // pane moved" is one fact about this build and not one fact per desk -- and it is a
    // COUNT rather than a flag so a case can tell "every desk converted" from "one did".
    std::vector<Layout> run;
    for (int i = 0; i < 3; ++i) {
        Setup desk = desk_with_the_browser();
        desk.name = "Desk " + std::to_string(i + 1);
        run.push_back(Layout{desk, SetupLink{}});
    }
    // ...AND THE REMEMBERED VALUE ON AN ASSOCIATION IS A DESK TOO, so it converts with the
    // rest: a weaver whose layout is associated with a Setup file wrote the browser in two
    // places and is owed both.
    run[1].link = SetupLink{"/somewhere/morning.json", desk_with_the_browser()};

    session_persist::WorkshopSession file;
    file.format = session_persist::kFormat;
    file.format_version = session_persist::kFormatVersion;
    file.next_id = 9;
    file.active = 1;
    for (const Layout& l : run) {
        session_persist::WorkshopLayout out;
        out.id = static_cast<std::int64_t>(file.layouts.size()) + 1;
        out.desk = setup_persist::to_setup(l.desk);
        out.link.path = l.link.path;
        out.link.known = setup_persist::to_setup(l.link.known);
        file.layouts.push_back(out);
    }
    const session_persist::LoadedSession read =
        session_persist::from_text(session_persist::to_text(file));
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    // THREE DESKS AND ONE REMEMBERED VALUE, each holding one browser row.
    CHECK(read.converted.total() == 4);
    REQUIRE(read.layouts.size() == 3);
    for (const Layout& l : read.layouts) {
        CHECK(pane_row(l.desk, new_files_ref()) != kNoPaneRow);
        CHECK(pane_row(l.desk, old_files_ref()) == kNoPaneRow);
    }
    CHECK(pane_row(read.layouts[1].link.known, new_files_ref()) != kNoPaneRow);
}

TEST_CASE("the weaver is told once, in the pane's own durable names") {
    // THE TRANSITION NOTE, THROUGH THE REAL RESTORE. A weaver whose session held the
    // browser on every desk reads ONE sentence about it, beside the ordinary reopening
    // sentence -- and a weaver whose session held none reads nothing at all, which is what
    // keeps the note a piece of news rather than a permanent decoration.
    TempDir dir("pane-mig-note");
    const std::string path = dir.file("session.json");

    session_persist::WorkshopSession file;
    file.format = session_persist::kFormat;
    file.format_version = session_persist::kFormatVersion;
    file.next_id = 9;
    file.active = 0;
    for (int i = 0; i < 2; ++i) {
        Setup desk = desk_with_the_browser();
        desk.name = "Desk " + std::to_string(i + 1);
        session_persist::WorkshopLayout out;
        out.id = static_cast<std::int64_t>(file.layouts.size()) + 1;
        out.desk = setup_persist::to_setup(desk);
        file.layouts.push_back(out);
    }
    spillout(path, session_persist::to_text(file));

    Live t;
    t.host.session_path = path;
    t.ready();
    t.extent_px(cells_px(132), cells_px(41));
    REQUIRE_MESSAGE(!t.session().notice_is_bad, t.notice());

    const std::string said = t.notice();
    CHECK(said.find("reopened your last desk") != std::string::npos);
    CHECK(said.find("zengine.workshop/project-files") != std::string::npos);
    CHECK(said.find("zengine.files/project-files") != std::string::npos);
    // ONCE, over two desks that both held it -- counted on the sentence's own opening, which
    // is said once per RUN however many rows carried the reference. (`is now` is one clause
    // per PAIR that moved, so counting that would count pairs rather than sentences.)
    std::size_t times = 0;
    for (std::size_t at = said.find("panes moved to their own offices"); at != std::string::npos;
         at = said.find("panes moved to their own offices", at + 1)) {
        ++times;
    }
    CHECK(times == 1u);
    // ...AND IT NAMES ONLY WHAT THIS FILE HELD. These desks hold the browser and no Builder
    // row, so the weaver is not told about a pane they would not find if they went and looked.
    CHECK(said.find("zengine.builder-pane") == std::string::npos);
    // ...AND THE DESKS THEMSELVES CAME BACK NAMING THE NEW OFFICE.
    REQUIRE(layout_count(t.session().setup) == 2);
    CHECK(pane_row(t.session().setup.active, new_files_ref()) != kNoPaneRow);
}

TEST_CASE("Info's PLACE moves with its office, and an authored one does not") {
    // THE CLAIM THE TABLE EXISTS FOR: Info's place moved with its office. A saved desk wrote
    // `zengine.workshop/info` with a `default` place, which meant the right column because the
    // catalog put it there; this build's catalog does not, so moving only the office would
    // drop a weaver's Info into the overlay stack. The safe half: a row whose place the weaver
    // AUTHORED keeps it -- the place is written only over a `kDefault` -- and both are asserted.
    Setup desk;
    desk.name = "Yesterday";
    REQUIRE(add_pane(desk, PaneRef{"zengine.workshop", "info"}));
    REQUIRE(add_pane(desk, PaneRef{"zengine.workshop", "layouts"}));
    // A SECOND INFO-ERA ROW, MOVED BY HAND. It is the Builder, because a desk cannot hold
    // two rows with one reference and the Builder's conversion is the neighbouring pair.
    REQUIRE(add_pane(desk, PaneRef{"zengine.workshop", "builder"}));
    const std::size_t built = pane_row(desk, PaneRef{"zengine.workshop", "builder"});
    REQUIRE(built != kNoPaneRow);
    desk.panes[built].place = PanePlace{pane_unit::kPixels, 96, 32};

    Setup live = desk;
    const pane_migration::Converted moved = pane_migration::convert_retired_panes(live);
    CHECK(moved.total() == 2);

    // THE OFFICE MOVED...
    const std::size_t info_at = pane_row(live, info_ref());
    REQUIRE(info_at != kNoPaneRow);
    CHECK(pane_row(live, PaneRef{"zengine.workshop", "info"}) == kNoPaneRow);
    // ...AND THE PLACE CAME WITH IT, by NAME and carrying no coordinates.
    CHECK(live.panes[info_at].place.mode == pane_unit::kRightColumn);
    CHECK(live.panes[info_at].place.x == 0);
    CHECK(live.panes[info_at].place.y == 0);
    // ...and the resulting desk is one this host would accept from a file.
    CHECK(check_setup(live).accepted);

    // THE AUTHORED ROW KEPT ITS OWN COORDINATES, office moved and place untouched.
    const std::size_t builder_at = pane_row(live, PaneRef{"zengine.builder-pane", "builder"});
    REQUIRE(builder_at != kNoPaneRow);
    CHECK(live.panes[builder_at].place.mode == pane_unit::kPixels);
    CHECK(live.panes[builder_at].place.x == 96);
    CHECK(live.panes[builder_at].place.y == 32);

    // AND AN INFO ROW THE WEAVER HAD ALREADY MOVED IS LEFT WHERE THEY PUT IT.
    Setup authored = desk;
    const std::size_t was = pane_row(authored, PaneRef{"zengine.workshop", "info"});
    REQUIRE(was != kNoPaneRow);
    authored.panes[was].place = PanePlace{pane_unit::kPixels, 12, 8};
    (void)pane_migration::convert_retired_panes(authored);
    const std::size_t now = pane_row(authored, info_ref());
    REQUIRE(now != kNoPaneRow);
    CHECK(authored.panes[now].place.mode == pane_unit::kPixels);
    CHECK(authored.panes[now].place.x == 12);
    CHECK(authored.panes[now].place.y == 8);
}

TEST_CASE("a saved setup naming the host's Pane Manager opens as the desktop's") {
    // THE FIFTH PAIR, AND THE FIRST WHOSE PANE KEY MOVED: `zengine.workshop/pane-editor` names
    // no built-in; its list is the desktop's Pane Manager, and a desk seating the old one seats
    // the new one where the weaver put it. ⚔ MUTATION: dropping the fifth row from `kRetired`;
    // the row comes back naming `zengine.workshop/pane-editor`, which nothing offers.
    Setup old;
    old.name = "Managed";
    const PaneRef was{pane_migration::kRetiredManagerProvider, pane_migration::kRetiredManagerPane};
    REQUIRE(add_pane(old, was));
    REQUIRE(author_pane_place(old, was, cells_px(4), cells_px(2)).accepted);
    REQUIRE(add_pane(old, ref_of(stock::kKind)));
    const setup_persist::LoadedSetup read = setup_persist::from_text(setup_persist::to_text(old));
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.converted.total() == 1);
    REQUIRE(read.setup.panes.size() == 2);
    const PaneRef now{pane_migration::kManagerProvider, pane_migration::kManagerPane};
    CHECK(read.setup.panes[0].ref == now);
    CHECK(read.setup.panes[0].place == old.panes[0].place); // the weaver's own place
    CHECK(read.setup.panes[0].front == old.panes[0].front);
    CHECK(read.setup.panes[1] == old.panes[1]); // and the other row is untouched
    CHECK(pane_migration::names_the_retired_manager(was));
    CHECK_FALSE(pane_migration::names_the_retired_manager(now));
    // THE WEAVER IS TOLD in the pane's durable names.
    CHECK(pane_migration::converted_note(read.converted)
              .find("zengine.workshop/pane-editor is now zengine.desktop/launcher") !=
          std::string::npos);
    // ...AND THE NEW SPELLING IS THE DESKTOP'S OWN, pinned against its vocabulary rather than
    // agreed with a copy of it.
    CHECK(std::string(pane_migration::kManagerProvider) == zengine::desktop_pane::kDesktopRole);
    CHECK(std::string(pane_migration::kManagerPane) == zengine::desktop_pane::kLauncherPane);
}

TEST_CASE("a session with nothing to convert says nothing about it") {
    TempDir dir("pane-mig-quiet");
    const std::string path = dir.file("session.json");
    session_persist::WorkshopSession file;
    file.format = session_persist::kFormat;
    file.format_version = session_persist::kFormatVersion;
    file.next_id = 9;
    file.active = 0;
    session_persist::WorkshopLayout out;
    out.id = 1;
    out.desk = setup_persist::to_setup(setup_of("Ordinary", {second::kKind}));
    file.layouts.push_back(out);
    spillout(path, session_persist::to_text(file));

    Live t;
    t.host.session_path = path;
    t.ready();
    t.extent_px(cells_px(132), cells_px(41));
    REQUIRE_MESSAGE(!t.session().notice_is_bad, t.notice());
    CHECK(t.notice().find("reopened your last desk") != std::string::npos);
    CHECK(t.notice().find("project-files") == std::string::npos);
}


// ---- A pane's settings in its setup row ---------------------------------------------------

namespace {

PaneSetting flag_setting(const std::string& key, bool on) { return PaneSetting{key, on, {}, {}}; }
PaneSetting number_setting(const std::string& key, std::int64_t n) {
    return PaneSetting{key, {}, n, {}};
}
PaneSetting text_setting(const std::string& key, const std::string& text) {
    return PaneSetting{key, {}, {}, text};
}

/// A desk whose stock row keeps one setting of each kind, and whose stranger -- a pane this
/// build cannot present -- keeps one of its own.
Setup desk_with_settings() {
    Setup desk = arranged_desk("Settings");
    pane_of(desk, ref_of(stock::kKind))->settings = {number_setting("count", 12),
                                                     flag_setting("legend", false),
                                                     text_setting("mode", "wide")};
    REQUIRE(add_pane(desk, stranger()));
    pane_of(desk, stranger())->settings = {flag_setting("kept", true)};
    REQUIRE_MESSAGE(check_setup(desk).accepted, check_setup(desk).refusal);
    return desk;
}

} // namespace

TEST_CASE("a setting rides its row through the setup file in its own kind's field") {
    const Setup desk = desk_with_settings();
    const std::string text = setup_persist::to_text(desk);
    // A NUMBER IS A NUMBER AND A FLAG A FLAG: each in the field of its kind, never spelled as
    // text, and the two fields a value does not hold are absent rather than empty.
    CHECK(text.find(R"("settings":[{"key":"count","number":"12"},{"key":"legend","flag":false},)"
                    R"({"key":"mode","text":"wide"}])") != std::string::npos);
    CHECK(text.find(R"("settings":[{"key":"kept","flag":true}])") != std::string::npos);
    // ...AND A ROW THAT KEEPS NONE SAYS SO THE ONE WAY: an empty list.
    CHECK(text.find(R"("settings":[])") != std::string::npos);

    const setup_persist::LoadedSetup read = setup_persist::from_text(text);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.setup == desk);
    // AN UNRESOLVED REFERENCE KEEPS ITS SETTINGS, and the file comes back byte for byte.
    CHECK(pane_of(read.setup, stranger())->settings ==
          std::vector<PaneSetting>{flag_setting("kept", true)});
    CHECK(setup_persist::to_text(read.setup) == text);
}

TEST_CASE("a setting holding no value, or two, is refused by every reader, naming its key") {
    struct Case {
        const char* what;
        std::vector<PaneSetting> settings;
        const char* says;
    };
    std::vector<PaneSetting> crowded;
    for (std::size_t i = 0; i <= kMaxPaneSettingsPerRow; ++i) {
        crowded.push_back(number_setting("k" + std::to_string(100 + i), 1));
    }
    const std::vector<Case> cases = {
        {"no value", {PaneSetting{"legend", {}, {}, {}}}, "setting `legend` holds no value"},
        {"two values", {PaneSetting{"legend", true, {}, std::string("on")}},
         "setting `legend` holds more than one value"},
        {"a key a weaver cannot type", {flag_setting("Legend", true)}, "`Legend` is not"},
        {"a key past its bound", {flag_setting(std::string(kMaxPaneSettingKeyLen + 1, 'k'), true)},
         "a setting's key is at most"},
        {"a text past its bound",
         {text_setting("title", std::string(kMaxPaneSettingTextLen + 1, 't'))},
         "setting `title` holds a text past"},
        {"a text a pane cannot draw", {text_setting("title", "two\nlines")},
         "setting `title` holds a text past"},
        {"two settings out of key order", {flag_setting("zoom", true), flag_setting("legend", true)},
         "`legend` follows `zoom`"},
        {"one key twice", {flag_setting("legend", true), flag_setting("legend", false)},
         "`legend` follows `legend`"},
        {"a Workshop setting this Workshop does not know", {number_setting("workshop.text", 1)},
         "`workshop.text` is a Workshop setting this Workshop does not know"},
        {"more settings than a row keeps", crowded, "settings in a layout -- this row has"},
    };
    for (const Case& c : cases) {
        CAPTURE(c.what);
        Setup desk = arranged_desk("Bad");
        pane_of(desk, ref_of(stock::kKind))->settings = c.settings;

        // THE LAW, AS A TYPED GESTURE WOULD MEET IT...
        const Written law = check_setup(desk);
        CHECK_FALSE(law.accepted);
        CHECK(law.refusal.find(c.says) != std::string::npos);

        // ...THE SETUP FILE...
        const setup_persist::LoadedSetup file =
            setup_persist::from_text(setup_persist::to_text(desk));
        CHECK_FALSE(file.outcome.accepted);
        CHECK(file.outcome.refusal.find(c.says) != std::string::npos);

        // ...AND A SESSION'S DESK, refused whole in the layout it stands in.
        const session_persist::LoadedSession session = session_persist::from_text(
            session_text(one_layout(desk), 0));
        CHECK_FALSE(session.outcome.accepted);
        CHECK(session.outcome.refusal.find(c.says) != std::string::npos);
        CHECK(session.outcome.refusal.find("layout at position 0") != std::string::npos);
    }

    // AND A DESK PAST ITS OWN BOUND, however its rows share them out.
    Setup full = setup_of("Full", {});
    fill_to_every_bound(full);
    full.panes.back().settings.push_back(number_setting("zz", 1));
    const Written past = check_setup(full);
    CHECK_FALSE(past.accepted);
    CHECK(past.refusal.find("settings, locks and pins over all its panes") != std::string::npos);
    CHECK_FALSE(setup_persist::from_text(setup_persist::to_text(full)).outcome.accepted);
}

TEST_CASE("the settings door keeps a desk inside its bound, and a value moved on a full desk is kept") {
    Setup desk = setup_of("Full", {});
    fill_to_every_bound(desk);
    const PaneRef bare = desk.panes.back().ref;
    REQUIRE(pane_of(desk, bare)->settings.empty());

    // ONE MORE KEY WOULD BE THE DESK'S FIRST PAST ITS BOUND: refused, and nothing is written.
    const Setup before = desk;
    const Written past = author_pane_setting(desk, bare, "legend", flag_setting("legend", false),
                                             flag_setting("legend", true));
    CHECK_FALSE(past.accepted);
    CHECK(past.refusal == "this layout keeps " + std::to_string(kMaxPaneSettingsPerDesk) +
                              " settings already, the most a desk keeps -- clear one first");
    CHECK(desk == before);

    // A KEPT KEY TAKES A NEW VALUE THERE, since the count does not move...
    const PaneRef crowded = desk.panes.front().ref;
    const std::string key = desk.panes.front().settings.front().key;
    REQUIRE(author_pane_setting(desk, crowded, key, text_setting(key, "moved"), std::nullopt)
                .accepted);
    CHECK(find_pane_setting(pane_of(desk, crowded)->settings, key)->text == "moved");
    CHECK(desk_setting_count(desk.panes) == kMaxPaneSettingsPerDesk);

    // ...AND ONE CLEARED MAKES ROOM FOR ONE MORE.
    REQUIRE(author_pane_setting(desk, crowded, key, std::nullopt, std::nullopt).accepted);
    CHECK(author_pane_setting(desk, bare, "legend", flag_setting("legend", false),
                              flag_setting("legend", true))
              .accepted);
    CHECK(desk_setting_count(desk.panes) == kMaxPaneSettingsPerDesk);
    CHECK(check_setup(desk).accepted);
}

TEST_CASE("a version-4 setup file reads with no settings, and the next save writes the current version") {
    const Setup desk = arranged_desk("Four");
    // A FILE A WORKSHOP OF version 4 WROTE, content id and all: what its `s` left on disk.
    const std::string four = loom::compat::serialize(loom::to_value(v4_desk(desk)));
    REQUIRE(four.find("\"version\":4") != std::string::npos);
    REQUIRE(four.find("settings") == std::string::npos);

    const setup_persist::LoadedSetup read = setup_persist::from_text(four);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.setup == desk);
    for (const SetupPane& row : read.setup.panes) {
        CHECK(row.settings.empty());
    }
    const std::string now = setup_persist::to_text(read.setup);
    CHECK(now.find("\"format_version\":\"6\"") != std::string::npos);
    CHECK(now.find("\"settings\":[]") != std::string::npos);

    // ...AND AN ENVELOPE OF version 4 OVER A BODY THAT SAYS ANOTHER IS REFUSED BY NUMBER.
    setup_persist::v4::WorkshopSetup forged = v4_desk(desk);
    forged.format_version = 5;
    const setup_persist::LoadedSetup no =
        setup_persist::from_text(loom::compat::serialize(loom::to_value(forged)));
    CHECK_FALSE(no.outcome.accepted);
    CHECK(no.outcome.refusal == "setup version 5 -- this Workshop reads versions 2, 3, 4, 5 and 6");
}

TEST_CASE("a setup past its byte ceiling is refused at the save, and nothing is written") {
    // NOT A DESK THE LAW ALLOWS -- every row at its own bound is far past the desk's -- so the
    // write side's own refusal is what stands between it and a file `r` would refuse.
    Setup huge = setup_of("Huge", {});
    fill_to_every_bound(huge);
    for (SetupPane& row : huge.panes) {
        while (row.settings.size() < kMaxPaneSettingsPerRow) {
            PaneSetting one;
            one.key = "z" + std::to_string(100 + row.settings.size());
            one.text = std::string(kMaxPaneSettingTextLen, '"');
            row.settings.push_back(std::move(one));
        }
    }
    REQUIRE(setup_persist::to_text(huge).size() > setup_persist::kMaxSetupBytes);

    TempDir dir("setup-over-ceiling");
    const std::string path = dir.file("setup.json");
    spillout(path, "the weaver's own bytes");
    const Written saved = setup_persist::save_file(path, huge);
    CHECK_FALSE(saved.accepted);
    CHECK(saved.refusal.find("larger than a Workshop setup can be") != std::string::npos);
    CHECK(saved.refusal.find("nothing was written") != std::string::npos);
    CHECK(slurp(path) == "the weaver's own bytes");
}

TEST_CASE("a setup holding a Workshop setting is refused by name, and `r` leaves file and desk") {
    TempDir dir("setup-desk-setting");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    Setup newer = arranged_desk("Newer");
    pane_of(newer, ref_of(stock::kKind))->settings = {number_setting("workshop.text", 1)};
    // WHAT A NEWER WORKSHOP COULD WRITE: sound in every other respect, so the refusal is the
    // key and nothing else.
    const std::string bytes = setup_persist::to_text(newer);
    spillout(t.host.setup_path, bytes);
    t.ready();
    const Setup before = t.session().setup.active;

    t.key(input::scan::kR);
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("`workshop.text` is a Workshop setting this Workshop does not know") !=
          std::string::npos);
    CHECK(t.session().setup.active == before);
    CHECK(slurp(t.host.setup_path) == bytes);
}

TEST_CASE("a layout's settings come back with its desk after a restart") {
    TempDir dir("settings-restart");
    const std::string session = dir.file("session.json");
    const Setup desk = desk_with_settings();
    arrange_and_close(session, dir.file("setup.json"), desk, 120, 44);

    Live back;
    back.host.session_path = session;
    back.ready();
    CHECK(back.run().active == desk);
    REQUIRE(pane_of(back.run().active, ref_of(stock::kKind)) != nullptr);
    CHECK(pane_of(back.run().active, ref_of(stock::kKind))->settings ==
          pane_of(desk, ref_of(stock::kKind))->settings);
}

TEST_CASE("a version-7 session converts with every desk and remembered value keeping no settings") {
    // THE NEWEST RETIRED VINTAGE: a layout associated with its artifact still matches it, every
    // desk and every remembered value arrives with no settings, and nothing else moves.
    MountedHistory history;
    REQUIRE_MESSAGE(history.mounted.ok, history.mounted.reason);
    const std::vector<Setup> desks = three_desks();
    session_history::v7::WorkshopSession old;
    old.format = session_persist::kFormat;
    old.format_version = session_history::kV7FormatVersion;
    old.viewport = window_persist::WorkshopViewport{cells_px(132), cells_px(41)};
    for (std::size_t i = 0; i < desks.size(); ++i) {
        session_history::v7::WorkshopLayout layout{
            v4_desk(desks[i]),
            session_history::v7::WorkshopSetupLink{std::string(), session_history::absent_v4_desk()}};
        if (i == 0) {
            layout.link = session_history::v7::WorkshopSetupLink{"/kept/home.json", v4_desk(desks[i])};
        }
        old.layouts.push_back(std::move(layout));
    }
    old.active = 1;
    old.placement = session_history::absent_placement();
    const std::string bytes = loom::compat::serialize(loom::to_value(old));

    const std::uint64_t before = op::invocations();
    const session_persist::LoadedSession read = session_persist::from_text(bytes, &history.catalog);
    const window_persist::LoadedWindow room_read = window_persist::import_from_session_text(bytes);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(op::invocations() == before + 1);
    REQUIRE(history.catalog.find("zengine.migrate.WorkshopSession.v7-to-v12") != nullptr);
    CHECK(desks_of(read.layouts) == desks);
    CHECK(read.active == 1);
    for (const Layout& layout : read.layouts) {
        for (const SetupPane& row : layout.desk.panes) {
            CHECK(row.settings.empty());
        }
    }
    // THE ASSOCIATION'S REMEMBERED VALUE IS CONVERTED WITH ITS DESK, or the layout would read
    // `modified` after an upgrade the weaver did not make.
    CHECK(read.layouts[0].link.path == "/kept/home.json");
    CHECK(link_status(read.layouts[0].desk, read.layouts[0].link) == setup_link::kCurrent);
    CHECK(link_status(read.layouts[1].desk, read.layouts[1].link) == setup_link::kNone);
    CHECK(room_read.viewport_w == cells_px(132));
    CHECK(room_read.viewport_h == cells_px(41));

    // ...AND A DESK OF version 7 WHOSE OWN FIELD SAYS ANOTHER IS REFUSED BY THE CONVERSION,
    // which skips no check its own reader made.
    session_history::v7::WorkshopSession forged = old;
    forged.layouts[2].desk.format_version = 9;
    const session_persist::LoadedSession no = session_persist::from_text(
        loom::compat::serialize(loom::to_value(forged)), &history.catalog);
    CHECK_FALSE(no.outcome.accepted);
    CHECK(no.outcome.refusal.find("layout at position 2: setup version 9") != std::string::npos);
}

// ---- WL-SWITCH-08: the choice the last session remembered ----------------------------------

TEST_CASE("a version-8 session converts with every field kept and no choice remembered") {
    MountedHistory history;
    REQUIRE_MESSAGE(history.mounted.ok, history.mounted.reason);
    const Setup desk = arranged_desk("Kept");
    session_history::v8::WorkshopSession old;
    old.format = session_persist::kFormat;
    old.format_version = session_history::kV8FormatVersion;
    const session_history::v11::WorkshopSession now =
        v11_session(one_layout(desk), 0, cells_px(128), cells_px(40));
    old.viewport = now.viewport;
    old.layouts = now.layouts;
    old.placement = now.placement;
    const session_persist::LoadedSession read =
        session_persist::from_text(loom::compat::serialize(loom::to_value(old)), &history.catalog);
    const window_persist::LoadedWindow room_read = window_persist::import_from_session_text(loom::compat::serialize(loom::to_value(old)));
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    REQUIRE(history.catalog.find("zengine.migrate.WorkshopSession.v8-to-v12") != nullptr);
    REQUIRE(read.layouts.size() == 1);
    CHECK(read.layouts[0].desk == desk);
    CHECK(room_read.viewport_w == cells_px(128));
    CHECK(room_read.viewport_h == cells_px(40));
    // A SESSION BEFORE 9 REMEMBERS NO CHOICE, so the next start loads each office's own row.
    CHECK(read.choices.empty());
}

TEST_CASE("a version-9 session converts with every field kept and no OS window's room") {
    // ⚔ MUTATION: the conversion taking the viewport as the OS window's room -- a version-9 file a
    // terminal run wrote would shape the OS window to the terminal's room.
    MountedHistory history;
    REQUIRE_MESSAGE(history.mounted.ok, history.mounted.reason);
    const Setup desk = arranged_desk("Kept");
    window_persist::Placement place;
    place.known = true;
    place.x = 30;
    place.y = 40;
    const session_history::v11::WorkshopSession now =
        v11_session(one_layout(desk), 0, cells_px(128), cells_px(40), place,
                    {session_persist::WorkshopChoice{"zengine.editor", "neovim"}});
    session_history::v9::WorkshopSession old;
    old.format = session_persist::kFormat;
    old.format_version = session_history::kV9FormatVersion;
    old.viewport = now.viewport;
    old.layouts = now.layouts;
    old.placement = now.placement;
    old.choices = now.choices;
    const session_persist::LoadedSession read =
        session_persist::from_text(loom::compat::serialize(loom::to_value(old)), &history.catalog);
    const window_persist::LoadedWindow room_read = window_persist::import_from_session_text(loom::compat::serialize(loom::to_value(old)));
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    REQUIRE(history.catalog.find("zengine.migrate.WorkshopSession.v9-to-v12") != nullptr);
    REQUIRE(read.layouts.size() == 1);
    CHECK(read.layouts[0].desk == desk);
    CHECK(room_read.viewport_w == cells_px(128));
    CHECK(room_read.placement.known);
    CHECK(room_read.placement.x == 30);
    REQUIRE(read.choices.size() == 1);
    CHECK(read.choices[0].choice == "neovim");
    // A SESSION BEFORE 10 KEEPS NO OS WINDOW'S ROOM, so its window opens and grows as it did.
    CHECK(room_read.placement.room_w == 0);
    CHECK(room_read.placement.room_h == 0);
}

TEST_CASE("a session keeps the choices it was given, and refuses more than an office's worth") {
    const std::vector<Layout> run = one_layout(arranged_desk("D"));
    const std::vector<session_persist::WorkshopChoice> chosen = {
        {"zengine.editor", "neovim"}, {"zengine.viewer", "plain"}};
    const std::string text = session_text(run, 0, chosen);
    const session_persist::LoadedSession read = session_persist::from_text(text);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    REQUIRE(read.choices.size() == 2);
    CHECK(read.choices[0].role == "zengine.editor");
    CHECK(read.choices[0].choice == "neovim");
    CHECK(read.choices[1].role == "zengine.viewer");
    // ...and one more office than the bound is a file this build will not read.
    std::vector<session_persist::WorkshopChoice> many;
    for (std::size_t i = 0; i <= session_persist::kMaxRememberedChoices; ++i) {
        many.push_back({"office." + std::to_string(i), "choice"});
    }
    const session_persist::LoadedSession refused = session_persist::from_text(
        session_text(run, 0, many));
    CHECK_FALSE(refused.outcome.accepted);
    CHECK(refused.outcome.refusal.find("at most 16 offices' choices") != std::string::npos);
}

TEST_CASE("a start loads the remembered choice in its office's row, while the plan authors it") {
    const auto plan_of = [] {
        load::LoadPlan plan;
        load::ArtifactIntent editor;
        editor.stem = "zengine-editor-pane";
        editor.weave = load::WeaveIntent{"zengine.editor"};
        plan.artifacts.push_back(editor);
        load::ArtifactIntent other;
        other.stem = "zengine-files";
        plan.artifacts.push_back(other);
        plan.choices = {load::ChoiceIntent{"zengine.editor", "standard", "zengine-editor-pane"},
                        load::ChoiceIntent{"zengine.editor", "neovim", "zengine-neovim-pane"}};
        return plan;
    };
    // THE REMEMBERED CHOICE LOADS IN THE OFFICE'S ROW, and no other row moves.
    load::LoadPlan plan = plan_of();
    CHECK(load::remember_choices(plan, {{"zengine.editor", "neovim"}}).empty());
    CHECK(plan.artifacts[0].stem == "zengine-neovim-pane");
    CHECK(plan.artifacts[1].stem == "zengine-files");
    // A CHOICE THE ROW ALREADY LOADS CHANGES NOTHING.
    load::LoadPlan same = plan_of();
    CHECK(load::remember_choices(same, {{"zengine.editor", "standard"}}).empty());
    CHECK(same.artifacts[0].stem == "zengine-editor-pane");
    // A CHOICE THE PLAN NO LONGER AUTHORS LOADS NOTHING, and is returned to be said.
    load::LoadPlan gone = plan_of();
    const auto unauthored = load::remember_choices(gone, {{"zengine.editor", "emacs"}});
    REQUIRE(unauthored.size() == 1);
    CHECK(unauthored[0].second == "emacs");
    CHECK(gone.artifacts[0].stem == "zengine-editor-pane");
}

TEST_CASE("a quit saves which choice holds each office, and a start says a choice no longer authored") {
    TempDir dir("switch-remembered");
    const std::string session = dir.file("session.json");
    {
        Live t;
        t.host.session_path = session;
        t.host.chosen = [] {
            return std::vector<session_persist::WorkshopChoice>{{"zengine.editor", "neovim"}};
        };
        t.ready();
        t.key(input::scan::kQ);
        REQUIRE(t.host.quit);
    }
    const session_persist::LoadedSession read = session_persist::load_file(session);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    REQUIRE(read.choices.size() == 1);
    CHECK(read.choices[0].role == "zengine.editor");
    CHECK(read.choices[0].choice == "neovim");

    // THE NEXT START'S NOTE, ONCE, on the restore's own line.
    Live back;
    back.host.session_path = session;
    back.host.choice_note = "your last choice `emacs` for zengine.editor is not one this plan "
                            "authors, so its own loaded";
    back.ready();
    CHECK(back.notice().find("your last choice `emacs`") != std::string::npos);
}

// ---- The window file, and what a version-3 settings file kept for layouts -------------------

TEST_CASE("an older session's room is Workshop's window file's once: read while none stands, and never again once one does") {
    // ⚔ MUTATION: the import reading the session whatever stands beside it -- the second run takes
    // the old session's room over the window file the weaver's last close wrote.
    TempDir dir("window-import");
    const std::string path = dir.file("session.json");
    const std::string old = v11_lines(
        v11_session(one_layout(arranged_desk("Before")), 0, cells_px(120), cells_px(44)));
    spillout(path, old);
    MountedHistory history;
    REQUIRE(history.mounted.ok);
    {
        Live t;
        t.host.session_path = path;
        t.host.conversions = &history.catalog;
        t.on_os_window(); // the medium that grows to the room restored
        t.ready();
        t.window_settles();
        REQUIRE(t.session().setup.active.name == "Before");
        CHECK(t.session().screen_w == cells_px(120));
        CHECK(t.session().screen_h == cells_px(44));
        t.key(input::scan::kQ);
        REQUIRE(t.host.quit);
    }
    // THE CLOSE WROTE BOTH: the window file holding the room, the session none of it.
    const window_persist::LoadedWindow wrote =
        window_persist::load_file(window_persist::path_beside(path));
    REQUIRE(wrote.present);
    REQUIRE_MESSAGE(wrote.outcome.accepted, wrote.outcome.refusal);
    CHECK(wrote.viewport_w == cells_px(120));
    CHECK(slurp(path).find("viewport") == std::string::npos);
    // THE OLD SESSION BACK, BESIDE A WINDOW FILE OF ANOTHER ROOM: the window file is the room's.
    spillout(path, old);
    spillout(window_persist::path_beside(path),
             window_persist::to_text(cells_px(90), cells_px(30), window_persist::Placement{}));
    Live back;
    back.host.session_path = path;
    back.host.conversions = &history.catalog;
    back.on_os_window();
    back.ready();
    back.window_settles();
    REQUIRE(back.session().setup.active.name == "Before");
    CHECK(back.session().screen_w == cells_px(90));
    CHECK(back.session().screen_h == cells_px(30));
}

namespace {

/// A SETTINGS FILE AT VERSION 3 KEEPING, BY LAYOUT NAME, a lock for "Shared" and pins for a layout
/// no session holds.
prefs_persist::Prefs layout_entries() {
    prefs_persist::Prefs p;
    prefs_persist::put_lock(p, SettingLock{"zengine.test.stack/stack@Shared", "legend"});
    p.pins = {prefs_persist::PinList{"zengine.test.stack/stack@Gone", {"step"}}};
    return p;
}

/// THE OLDER SESSION THEY GO WITH: two layouts named "Shared" and one "Solo".
std::string shared_run_session() {
    const std::vector<Layout> run{Layout{arranged_desk("Shared"), {}},
                                  Layout{arranged_desk("Shared"), {}},
                                  Layout{arranged_desk("Solo"), {}}};
    return v11_lines(v11_session(run, 0, cells_px(120), cells_px(44)));
}

} // namespace

TEST_CASE("what a version-3 settings file kept by layout name goes into each desk of the name, a copy each, and each entry no desk takes is named in full") {
    // ⚔ MUTATION: the fold given to the first desk of the name alone -- the second "Shared" keeps
    // no lock.
    TempDir dir("fold-in");
    const std::string path = dir.file("session.json");
    spillout(path, shared_run_session());
    REQUIRE(prefs_persist::save_file(dir.file("prefs.json"), layout_entries()).accepted);
    MountedHistory history;
    REQUIRE(history.mounted.ok);
    Live t;
    t.host.session_path = path;
    t.host.prefs_path = dir.file("prefs.json");
    t.host.conversions = &history.catalog;
    t.ready();
    const std::vector<Layout> run = layout_run(t.owner->layouts().run());
    REQUIRE(run.size() == 3);
    const std::vector<SetupLock> legend{SetupLock{ref_of(stock::kKind), "legend"}};
    CHECK(run[0].desk.locks == legend);
    CHECK(run[1].desk.locks == legend);
    CHECK(run[2].desk.locks.empty());
    // SAID ONCE, AT THE RESTORE: the shared name, and the entry no desk took, in full.
    CHECK(t.notice().find("layouts named \"Shared\" each keep their own locks, pins and baseline "
                          "now") != std::string::npos);
    CHECK(t.notice().find("the pins of `zengine.test.stack/stack@Gone`") != std::string::npos);
}

TEST_CASE("the settings file keeps its layouts' entries until a session holds them: a run that ends without its close finds them again") {
    // ⚔ MUTATION: the entries dropped from the settings file at the fold -- a run killed before
    // its close loses them, the session on disk still the older one that holds none.
    TempDir dir("fold-held");
    const std::string path = dir.file("session.json");
    const std::string prefs = dir.file("prefs.json");
    spillout(path, shared_run_session());
    REQUIRE(prefs_persist::save_file(prefs, layout_entries()).accepted);
    MountedHistory history;
    REQUIRE(history.mounted.ok);
    const auto run_once = [&](bool close) {
        Live t;
        t.host.session_path = path;
        t.host.prefs_path = prefs;
        t.host.conversions = &history.catalog;
        t.ready();
        REQUIRE(layout_run(t.owner->layouts().run()).size() == 3);
        CHECK(layout_run(t.owner->layouts().run())[1].desk.locks.size() == 1);
        // A TOGGLE WRITES THE SETTINGS FILE MID-RUN, and the entries stay in it.
        layout_key(t, Act::kPaneTitles);
        CHECK(prefs_persist::has_layout_entries(prefs_persist::load_file(prefs).kept));
        if (close) {
            t.key(input::scan::kQ);
            REQUIRE(t.host.quit);
        }
    };
    // A RUN THAT ENDS WITHOUT ITS CLOSE: the session on disk is still the older one...
    run_once(false);
    CHECK(slurp(path) == shared_run_session());
    // ...AND THE NEXT FINDS EVERY ENTRY STILL THERE, and folds it again.
    run_once(true);
    // ONCE A SESSION HOLDS THEM, the settings file goes without: version 4.
    CHECK(prefs_persist::load_file(prefs).read_version == 4);
    CHECK_FALSE(prefs_persist::has_layout_entries(prefs_persist::load_file(prefs).kept));
    const session_persist::LoadedSession held = session_persist::load_file(path);
    REQUIRE_MESSAGE(held.outcome.accepted, held.outcome.refusal);
    CHECK(held.layouts[1].desk.locks.size() == 1);
}

TEST_CASE("a place and a size carry a number exactly where the word reads one: a version-5 desk converts by its words, and a number where the word reads none is refused") {
    // ⚔ MUTATION: the version-5 lift copying each number whatever its word -- a named place comes
    // forward carrying coordinates, which this version refuses.
    Setup desk = setup_of("Words", {stock::kKind, second::kKind});
    desk.panes[0].place = PanePlace{pane_unit::kPixels, cells_px(3), cells_px(2)};
    desk.panes[0].width = PaneSize{pane_unit::kPixels, cells_px(40)};
    desk.panes[1].place = PanePlace{pane_unit::kRightColumn, 0, 0};
    REQUIRE(check_setup(desk).accepted);
    const setup_persist::LoadedSetup read =
        setup_persist::from_text(loom::compat::serialize(loom::to_value(v5_desk(desk))));
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.setup == desk);
    // WRITTEN AT THIS VERSION: a number beside each word that reads one, and none beside another.
    const std::string six = setup_persist::to_text(read.setup);
    const std::string pixels = "\"place\":{\"mode\":\"pixels\",\"x\":\"" +
                               std::to_string(cells_px(3)) + "\",\"y\":\"" +
                               std::to_string(cells_px(2)) + "\"}";
    CHECK(six.find(pixels) != std::string::npos);
    CHECK(six.find("\"width\":{\"mode\":\"pixels\",\"amount\":\"" + std::to_string(cells_px(40)) +
                   "\"}") != std::string::npos);
    const std::string named = "\"place\":{\"mode\":\"right-column\"}";
    REQUIRE(six.find(named) != std::string::npos);
    CHECK(six.find("\"height\":{\"mode\":\"default\"}") != std::string::npos);
    // A NUMBER WHERE THE WORD READS NONE, AND ONE MISSING WHERE IT READS ONE: each refused.
    const auto refusal_of = [&six](const std::string& from, const std::string& to) {
        std::string bad = six;
        const std::size_t at = bad.find(from);
        REQUIRE(at != std::string::npos);
        bad.replace(at, from.size(), to);
        return setup_persist::from_text(bad).outcome.refusal;
    };
    CHECK(refusal_of(named, "\"place\":{\"mode\":\"right-column\",\"x\":\"0\",\"y\":\"0\"}")
              .find("a named pane place carries no coordinates") != std::string::npos);
    CHECK(refusal_of("\"height\":{\"mode\":\"default\"}",
                     "\"height\":{\"mode\":\"default\",\"amount\":\"0\"}")
              .find("a default pane height carries no amount") != std::string::npos);
    CHECK(refusal_of(pixels, "\"place\":{\"mode\":\"pixels\",\"x\":\"" +
                                 std::to_string(cells_px(3)) + "\"}")
              .find("a pixels pane place names both x and y") != std::string::npos);
}

TEST_CASE("the live layout's picture, the largest edit, the window file and a full desk each decode inside Loom's budget") {
    // ⚔ MUTATION: a `LiveLayout` carrying every layout's desk rather than the live one's -- the
    // picture of a full run passes the budget.
    std::vector<Layout> run;
    for (std::size_t i = 0; i < kMaxLayouts; ++i) {
        Setup s = setup_of(std::string(kMaxSetupNameLen, static_cast<char>('a' + i)), {});
        fill_to_every_bound(s);
        run.push_back(Layout{s, SetupLink{std::string(session_persist::kMaxLinkPathBytes, 'p'), s}});
    }
    // THE PICTURE THE OWNER SENDS ITS PRESENTER, of a full run.
    LayoutsHost host;
    LayoutStore store(host);
    SetupState state;
    REQUIRE(install_layout_run(state, numbered(run), kMaxLayouts - 1));
    store.put_run(state);
    const std::int64_t live = decoded_cells(loom::to_value(store.live_layout()));
    CHECK(live <= kDecodedCellBudget);
    // THE LARGEST EDIT THE PRESENTER ASKS: every edit a window at every bound.
    LayoutEditRequested edit;
    edit.revision = std::numeric_limits<std::int64_t>::max();
    for (std::size_t i = 0; i < kMaxLayoutEdits; ++i) {
        LayoutEdit e;
        e.op = "reset-front";
        e.layout = std::numeric_limits<std::int64_t>::max();
        e.provider = std::string(kMaxPaneKeyLen, '"');
        e.pane = std::string(kMaxPaneKeyLen, '"');
        e.place = setup_persist::place_out(
            PanePlace{pane_unit::kPixels, kMaxPanePixels, kMaxPanePixels});
        e.width = setup_persist::size_out(PaneSize{pane_unit::kPixels, kMaxPanePixels});
        e.height = e.width;
        e.by = std::numeric_limits<std::int64_t>::max();
        e.name = std::string(kMaxSetupNameLen, '"');
        edit.edits.push_back(std::move(e));
    }
    const std::int64_t asked = decoded_cells(loom::to_value(edit));
    CHECK(asked <= kDecodedCellBudget);
    // THE WINDOW FILE, every field present, and A FULL DESK at this version.
    window_persist::Placement place;
    place.known = true;
    place.maximized = true;
    place.x = place.y = place.room_w = place.room_h = std::numeric_limits<std::int64_t>::min();
    const std::int64_t window = decoded_cells(loom::to_value(
        window_persist::to_window(kScreenMaxW, kScreenMaxH, place)));
    CHECK(window <= kDecodedCellBudget);
    const std::int64_t desk = decoded_cells(loom::to_value(setup_persist::to_setup(run.back().desk)));
    CHECK(desk <= kDecodedCellBudget);
    MESSAGE("LiveLayout: ", live, " cells; the largest LayoutEditRequested: ", asked,
            " cells; the window file: ", window, " cells; a full desk: ", desk, " cells");
}
