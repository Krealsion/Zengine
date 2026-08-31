// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop persistence suite — what survives a process, and what deliberately does
// not.
//
// TWO OF THESE TIERS TOUCH A DISK. The document and its format are pure, and asserted
// mostly as bytes and values; save and load are then driven through the real weave on a
// real bus, because the interesting half of persistence is not the codec but what the
// SESSION does when the document under it is replaced. Every case that needs a file uses
// a temporary directory of its own and removes it; nothing here writes into the source
// tree, and no case shares a path with another — see `TempDir` in
// `workshop_support.hpp`, whose root belongs to the one suite that made it. This suite
// owns the case that says so, because it is where sixty-eight of the eighty-nine
// temporary directories in Workshop's tests are made.
//
// What it holds: the document format and its refusals; persistence through the weave; the
// SETUP a maker names the arrangement they are working in; the desk that comes back on
// its own; and the installed application — per-user roots, explicit isolation, the
// one-time legacy import, the prefs file, and the window's desktop placement remembered,
// offered back, and judged by the medium that can see displays.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

// The historical session shapes and their conversions (MIG-0) -- the conversion artifact's
// material, named here because this suite owns what a durable session file means.
#include "workshop/session_history.hpp"
#include "operator/migration.hpp"

// ============================================================================
// Tier 5 — PERSISTENCE: what survives a process, and what deliberately does not
// ============================================================================
//
// Three questions, and every case below answers one of them:
//
//   1. WHAT IS THE DOCUMENT?  Everything a maker authored — identity, name,
//      place, both halves of each extent, the ORDER, and the mint — comes back
//      exactly. Nothing else is in the file, and the strongest proof of that is
//      the workspace: save under one and load under another, and the authored
//      share is identical while the resolved cells are not.
//   2. WHAT IS "THE SAME OBJECT" ACROSS PROCESS DEATH?  The identity, and only
//      the identity. A loaded #2 IS the saved #2 — the same number, findable,
//      selectable, editable — and the mint does not rewind, so an identity that
//      died before the save cannot come back after the load.
//   3. WHAT HAPPENS WHEN THE FILE IS WRONG?  Nothing. Every refusal below
//      asserts the live document is untouched, because "a malformed file must
//      never leave Workshop halfway loaded" is the claim, and "the parser
//      returned an error" is not that claim.

TEST_CASE("a temporary directory belongs to the suite that made it") {
    // THE PROPERTY THE DECOMPOSITION MADE LOAD-BEARING. While Workshop's cases were one
    // binary, two `TempDir`s could only ever meet inside one process, where the counter
    // tells them apart. Six binaries run them now and CTest runs the six at once -- every
    // counter starts at zero -- so what keeps two suites out of one directory is that the
    // ROOT is named for the suite. Asserted here rather than trusted, because the failure
    // it prevents is not a red: `TempDir` clears what it finds, so a shared path would be
    // one suite deleting another's file mid-case and the two would disagree about why.
    const TempDir a("owner");
    const TempDir b("owner");
    CHECK(a.path() != b.path()); // the counter, within one process
    CHECK(a.path().parent_path() == b.path().parent_path());

    // The root is this binary's, and the name in it is the CTest entry's. A second suite
    // asking for the same tag resolves under a different parent, whatever it calls it.
    CHECK(a.path().parent_path() == workshop_temp_root());
    CHECK(workshop_temp_root().filename().string() ==
          std::string("zengine-workshop-") + ZENGINE_WORKSHOP_SUITE);

    // ...and it sits directly in the system's temporary directory. Asked by IDENTITY rather
    // than by spelling: `temp_directory_path()` ends in a separator on Windows and does not
    // on Linux, so `parent_path() == temp_directory_path()` is a claim about how a platform
    // writes a path rather than about where the directory is -- it passed on Linux and
    // failed on MSVC saying `C:\...\Temp` != `C:\...\Temp\`. `equivalent` asks the
    // filesystem, and both paths exist here because `a` created them.
    CHECK(std::filesystem::equivalent(workshop_temp_root().parent_path(),
                                      std::filesystem::temp_directory_path()));
}

TEST_CASE("an empty document survives a round trip as an empty document") {
    // The boring case first, because it is the one a format is most likely to
    // get wrong: an empty list and a mint that has never minted are still facts.
    WorkshopDoc empty;
    const std::string text = persist::to_text(empty);

    WorkshopDoc live = two_panels();
    REQUIRE(persist::load_into(live, text).accepted);
    CHECK(live.elements.empty());
    CHECK(live.next_id == 1);
    CHECK(live == empty);
}

TEST_CASE("every authored fact survives, and the identities are the SAME identities") {
    const WorkshopDoc original = rich_document();
    WorkshopDoc live;
    REQUIRE(persist::load_into(live, persist::to_text(original)).accepted);

    // Not "equivalent". Equal.
    CHECK(live == original);

    // And the identities are usable AS identities on the other side, which is
    // the operational form of the claim: #2 can be found, hit, and edited.
    const std::int64_t id = original.elements[1].id;
    REQUIRE(doc::find(live, id) != nullptr);
    CHECK(doc::find(live, id)->label == "panel");
    CHECK(doc::set_x(live, id, 9).accepted);
    CHECK(doc::find(live, id)->x == 9);

    Session s;
    const ui::Scene scene = workspace_scene(live, s);
    const ui::Placed* placed = ui::placed_for(scene, id);
    REQUIRE(placed != nullptr);
    const ui::Placed* under = ui::hit(scene, placed->rect.x, placed->rect.y);
    REQUIRE(under != nullptr);
    CHECK(under->id == id);
}

TEST_CASE("two objects called `panel` come back as two objects called `panel`") {
    // The two-panels fixture, asked across a process boundary: a duplicate name is legal
    // and stays legal, because the identity is the id. A format that keyed on
    // the name would silently merge these two.
    const WorkshopDoc original = two_panels();
    REQUIRE(original.elements[0].label == original.elements[1].label);

    WorkshopDoc live;
    REQUIRE(persist::load_into(live, persist::to_text(original)).accepted);
    REQUIRE(live.elements.size() == 2);
    CHECK(live.elements[0].label == live.elements[1].label);
    CHECK(live.elements[0].id != live.elements[1].id);
}

TEST_CASE("object ORDER round-trips, and it is order a maker can see") {
    // Ordering is semantic in this application in four ways -- paint order,
    // which object a click finds where two overlap, the object list, and where
    // the selection lands after a delete. So the file writes document order and
    // never sorts. The proof is the one a maker would notice: two rectangles on
    // top of each other answer a click with the SAME id after a reload.
    WorkshopDoc original;
    const std::int64_t under = doc::add(original, "under", 1, 1, ui::Extent{ui::kExtentCells, 10},
                                        ui::Extent{ui::kExtentCells, 5});
    const std::int64_t over = doc::add(original, "over", 1, 1, ui::Extent{ui::kExtentCells, 10},
                                       ui::Extent{ui::kExtentCells, 5});
    Session s;
    REQUIRE(ui::hit(workspace_scene(original, s), 3, 3)->id == over);

    WorkshopDoc live;
    REQUIRE(persist::load_into(live, persist::to_text(original)).accepted);
    CHECK(live.elements[0].id == under);
    CHECK(live.elements[1].id == over);
    CHECK(ui::hit(workspace_scene(live, s), 3, 3)->id == over);

    // And the negative control: a file whose objects are in the OTHER order is
    // a DIFFERENT document, and the click says so. This is what makes the claim
    // "order is meaning" a measurement rather than an assertion.
    WorkshopDoc swapped = original;
    std::swap(swapped.elements[0], swapped.elements[1]);
    WorkshopDoc other;
    REQUIRE(persist::load_into(other, persist::to_text(swapped)).accepted);
    CHECK(ui::hit(workspace_scene(other, s), 3, 3)->id == under);
}

TEST_CASE("a cells extent stays cells and a share stays a share") {
    // The semantic distinction, preserved even where both happen to resolve to
    // the same number of cells in the workspace that saved them.
    WorkshopDoc original;
    const std::int64_t cells = doc::add(original, "cells", 0, 0, ui::Extent{ui::kExtentCells, 28},
                                        ui::Extent{ui::kExtentCells, 3});
    const std::int64_t share = doc::add(original, "share", 0, 5, ui::Extent{ui::kExtentPercent, 60},
                                        ui::Extent{ui::kExtentCells, 3});
    Session s; // the default 48-cell workspace: 60% of 48 IS 28
    const ui::Scene scene = workspace_scene(original, s);
    REQUIRE(ui::placed_for(scene, cells)->rect.w == ui::placed_for(scene, share)->rect.w);

    WorkshopDoc live;
    REQUIRE(persist::load_into(live, persist::to_text(original)).accepted);
    CHECK(doc::find(live, cells)->width == ui::Extent{ui::kExtentCells, 28});
    CHECK(doc::find(live, share)->width == ui::Extent{ui::kExtentPercent, 60});

    // The file says which is which in words, so a person reading it does not
    // have to know what 0 and 1 mean this week.
    const std::string text = persist::to_text(original);
    CHECK(text.find("\"mode\":\"cells\"") != std::string::npos);
    CHECK(text.find("\"mode\":\"percent\"") != std::string::npos);
}

TEST_CASE("the mint survives, so an identity that died before the save stays dead") {
    // THE PROMPT'S HARD CASE, and the reason `next_id` is in the file at all.
    //
    //   create #1, #2, #3 -> delete #3 -> save -> load -> create
    //
    // must produce #4. A loader that reconstructed the mint the only way it
    // could without this field -- one past the largest surviving id -- would
    // produce #3 again, and a notice, a selection or a half-finished thought
    // still saying "#3" would quietly come to mean a different object.
    WorkshopDoc original;
    CHECK(doc::add_default(original) == 1);
    CHECK(doc::add_default(original) == 2);
    CHECK(doc::add_default(original) == 3);
    REQUIRE(doc::remove(original, 3).accepted);
    REQUIRE(original.next_id == 4);

    WorkshopDoc live;
    REQUIRE(persist::load_into(live, persist::to_text(original)).accepted);
    CHECK(live.next_id == 4);

    // Stated the way a defect would take, BEFORE creating anything: the largest
    // id that survived the save is 2, and the mint is NOT 3. `max(live)+1` is
    // the only reconstruction available to a loader without this field, and it
    // is exactly the one that recycles a dead identity.
    std::int64_t largest = 0;
    for (const ui::Element& e : live.elements) {
        largest = e.id > largest ? e.id : largest;
    }
    CHECK(largest == 2);
    CHECK(live.next_id != largest + 1);

    const std::int64_t next = doc::add_default(live);
    CHECK(next == 4);
    CHECK(next != 3);
}

TEST_CASE("save -> load -> save is byte-identical") {
    // Canonical serialization, without a canonicalization framework: the writer
    // emits fields in declared schema order and the document's own object order,
    // so the same document is always the same bytes. That is what makes a saved
    // document diffable and archivable.
    const WorkshopDoc original = rich_document();
    const std::string first = persist::to_text(original);

    WorkshopDoc live;
    REQUIRE(persist::load_into(live, first).accepted);
    const std::string second = persist::to_text(live);
    CHECK(first == second);

    // And again, so "stable" means stable rather than "the same twice".
    WorkshopDoc again;
    REQUIRE(persist::load_into(again, second).accepted);
    CHECK(persist::to_text(again) == first);
}

TEST_CASE("saving does not touch the document") {
    // Serialization is OBSERVATION. Nothing is renumbered, re-ordered, rounded,
    // clamped or tidied on the way out -- including a value some later rule
    // might not like, because a save that edits the work it was asked to
    // preserve is worse than one that refuses.
    WorkshopDoc d = rich_document();
    const WorkshopDoc before = d;
    const std::string text = persist::to_text(d);
    CHECK(d == before);
    CHECK(d.next_id == before.next_id);
    CHECK(text.size() > 0);
}

TEST_CASE("the file a person can read: identity, version, objects, and the mint") {
    // Legibility is a requirement, not a nicety: a maker owns this file. A
    // technically literate person should recognise every fact in it without
    // reverse-engineering Workshop's memory layout.
    WorkshopDoc d;
    doc::add(d, "sidebar", 3, 2, ui::Extent{ui::kExtentPercent, 60},
             ui::Extent{ui::kExtentCells, 6});
    const std::string text = persist::to_text(d);

    CHECK(text.find("\"format\":\"zengine-workshop\"") != std::string::npos);
    CHECK(text.find("\"format_version\":\"1\"") != std::string::npos);
    CHECK(text.find("\"next_id\":\"2\"") != std::string::npos);
    CHECK(text.find("\"name\":\"sidebar\"") != std::string::npos);
    CHECK(text.find("\"id\":\"1\"") != std::string::npos);
    CHECK(text.find("\"x\":\"3\"") != std::string::npos);
    CHECK(text.find("\"y\":\"2\"") != std::string::npos);
    CHECK(text.find("\"mode\":\"percent\",\"amount\":\"60\"") != std::string::npos);
    // The relationship, written as the identity it is. An ordinary root-context
    // object says `0`, which is not an identity any object can carry.
    CHECK(text.find("\"context\":\"0\"") != std::string::npos);

    // It is real JSON, and it says whose value it is -- the Loom's envelope,
    // which is the claim `admit()` checks. That is a DIFFERENT claim from the
    // `format` field above: one is about the shape of the bytes, the other is
    // about what the document means -- and here the two visibly diverge.
    // The SHAPE is at version 2 (an object grew a `context`); `format_version`
    // stayed 1, because the meaning of every field is what it was and there is
    // still exactly one Workshop format in the world.
    CHECK(text.rfind("{\"zen\":1,\"schema\":\"WorkshopDocument\",\"version\":2,", 0) == 0);
}

TEST_CASE("the file carries no resolved geometry, and the scene is rebuilt from what it does") {
    // The claim, in the only form that can actually be checked: none of the
    // resolved vocabulary appears anywhere in the file, and deleting the whole
    // scene and resolving again from the loaded document gives the same live
    // answer -- the same picture, the same hit test.
    const WorkshopDoc original = rich_document();
    const std::string text = persist::to_text(original);

    for (const char* resolved : {"\"rect\"", "\"w\":", "\"h\":", "\"scene\"", "\"resolved\"",
                                 "\"handle\"", "\"viewport\"", "\"workspace\"", "\"selected\"",
                                 "\"pixels\"", "\"cursor\"", "\"drag\""}) {
        CAPTURE(resolved);
        CHECK(text.find(resolved) == std::string::npos);
    }

    Session s;
    const ui::Scene from_original = workspace_scene(original, s);
    WorkshopDoc live;
    REQUIRE(persist::load_into(live, text).accepted);
    const ui::Scene rebuilt = workspace_scene(live, s);
    CHECK(rebuilt == from_original);

    // And the PICTURE, which is the form a maker would notice: every rectangle
    // the original screen showed is on the loaded one, in the same place, at
    // the same size, in the same ink.
    const surface::SurfaceCanvas was = paint(original, s);
    const surface::SurfaceCanvas now = paint(live, s);
    REQUIRE(all_rects(was).size() == all_rects(now).size());
    for (const surface::SurfaceRect& r : all_rects(was)) {
        CHECK(has_rect(now, r.x, r.y, r.w, r.h, r.role));
    }
}

TEST_CASE("the same share, loaded into a different workspace, resolves differently") {
    // THE PROOF THAT THE FILE KEPT INTENT AND NOT CELLS. Save under workspace
    // A; load under workspace B. The authored numbers are identical and the
    // resolved ones are not, and that is not a bug -- it is the whole reason
    // authored and resolved are two facts.
    WorkshopDoc original;
    const std::int64_t share = doc::add(original, "share", 0, 0,
                                        ui::Extent{ui::kExtentPercent, 60},
                                        ui::Extent{ui::kExtentCells, 4});
    const std::int64_t fixed = doc::add(original, "fixed", 0, 6, ui::Extent{ui::kExtentCells, 20},
                                        ui::Extent{ui::kExtentCells, 4});

    Session wide;   // 48 cells, the default
    Session narrow; // half of it
    narrow.workspace_w = 24;

    const std::int64_t share_wide = ui::placed_for(workspace_scene(original, wide), share)->rect.w;
    const std::string text = persist::to_text(original);

    WorkshopDoc live;
    REQUIRE(persist::load_into(live, text).accepted);

    // AUTHORED: identical, both of them.
    CHECK(doc::find(live, share)->width == ui::Extent{ui::kExtentPercent, 60});
    CHECK(doc::find(live, fixed)->width == ui::Extent{ui::kExtentCells, 20});

    // RESOLVED: the share moved with the workspace, the cells did not.
    const std::int64_t share_narrow = ui::placed_for(workspace_scene(live, narrow), share)->rect.w;
    CHECK(share_wide == 28);
    CHECK(share_narrow == 14);
    CHECK(share_narrow != share_wide);
    CHECK(ui::placed_for(workspace_scene(live, narrow), fixed)->rect.w == 20);

    // And the file is the same file either way: the workspace is nowhere in it.
    CHECK(persist::to_text(live) == text);
}

TEST_CASE("a save normalizes nothing: 60% is not written as the cells it happens to be") {
    // The no-op stability rule, in its persistence form. At a 48-cell
    // workspace both 59% and 60% resolve to 28 cells, so a loader that
    // "helpfully" canonicalised would be free to pick either. Each is written
    // and read as itself.
    for (const std::int64_t pct : {59, 60}) {
        CAPTURE(pct);
        WorkshopDoc d;
        const std::int64_t id = doc::add(d, "p", 0, 0, ui::Extent{ui::kExtentPercent, pct},
                                         ui::Extent{ui::kExtentCells, 3});
        Session s;
        REQUIRE(ui::placed_for(workspace_scene(d, s), id)->rect.w == 28);

        WorkshopDoc live;
        REQUIRE(persist::load_into(live, persist::to_text(d)).accepted);
        CHECK(doc::find(live, id)->width.mode == ui::kExtentPercent);
        CHECK(doc::find(live, id)->width.amount == pct);
    }
}

// ---- Refusal: every one of these leaves the live document untouched ---------

TEST_CASE("a malformed document never leaves Workshop halfway loaded") {
    // The claim is NOT "the parser returned an error". It is that the document
    // a maker is working on is exactly what it was -- which is the persistence
    // form of the rule the property editor keeps.
    const WorkshopDoc good = rich_document();
    const std::string valid = persist::to_text(good);

    struct Case {
        const char* what;
        std::string text;
    };
    std::vector<Case> cases;
    cases.push_back({"not JSON at all", "{ this is not a document"});
    cases.push_back({"empty file", ""});
    cases.push_back({"a JSON array", "[1,2,3]"});
    cases.push_back({"someone else's value",
                     loom::compat::serialize(loom::to_value(ui::Extent{0, 4}))});
    cases.push_back({"wrong format identity",
                     forged(good, "\"zengine-workshop\"", "\"someone-elses-editor\"")});
    cases.push_back({"unsupported format version",
                     forged(good, "\"format_version\":\"1\"", "\"format_version\":\"2\"")});
    cases.push_back({"a missing required field", forged(good, "\"next_id\":\"4\",", "")});
    cases.push_back({"a field of the wrong kind",
                     forged(good, "\"next_id\":\"4\"", "\"next_id\":4")});
    cases.push_back({"a field the document does not declare",
                     forged(good, "\"next_id\":", "\"colour\":\"red\",\"next_id\":")});
    cases.push_back({"an integer larger than an integer",
                     forged(good, "\"next_id\":\"4\"", "\"next_id\":\"99999999999999999999\"")});
    cases.push_back({"two objects with one identity",
                     forged(good, "\"id\":\"2\"", "\"id\":\"1\"")});
    cases.push_back({"a mint that has already been spent",
                     forged(good, "\"next_id\":\"4\"", "\"next_id\":\"2\"")});
    cases.push_back({"a mint below the first identity",
                     forged(good, "\"next_id\":\"4\"", "\"next_id\":\"0\"")});
    cases.push_back({"a mint at the bottom of the number line",
                     forged(good, "\"next_id\":\"4\"", "\"next_id\":\"-9223372036854775808\"")});
    cases.push_back({"an identity of zero", forged(good, "\"id\":\"1\"", "\"id\":\"0\"")});
    cases.push_back({"a negative identity", forged(good, "\"id\":\"1\"", "\"id\":\"-1\"")});
    cases.push_back({"an extent mode with no meaning",
                     forged(good, "\"mode\":\"percent\"", "\"mode\":\"pixels\"")});
    cases.push_back({"an empty extent mode", forged(good, "\"mode\":\"cells\"", "\"mode\":\"\"")});
    cases.push_back({"a share of more than everything",
                     forged(good, "\"mode\":\"percent\",\"amount\":\"60\"",
                            "\"mode\":\"percent\",\"amount\":\"500\"")});
    cases.push_back({"a share of nothing",
                     forged(good, "\"mode\":\"percent\",\"amount\":\"60\"",
                            "\"mode\":\"percent\",\"amount\":\"0\"")});
    cases.push_back({"a size larger than the document allows",
                     forged(good, "\"mode\":\"cells\",\"amount\":\"6\"",
                            "\"mode\":\"cells\",\"amount\":\"999999\"")});
    cases.push_back({"a size at the top of the number line",
                     forged(good, "\"mode\":\"cells\",\"amount\":\"6\"",
                            "\"mode\":\"cells\",\"amount\":\"9223372036854775807\"")});
    cases.push_back({"a position that does not exist",
                     forged(good, "\"x\":\"3\"", "\"x\":\"-1\"")});
    cases.push_back({"a name that is not a name",
                     forged(good, "\"name\":\"panel\"", "\"name\":\"\"")});
    cases.push_back({"a name longer than a name",
                     forged(good, "\"name\":\"panel\"",
                            "\"name\":\"" + std::string(doc::kMaxNameLen + 1, 'x') + "\"")});
    cases.push_back({"an integer at the bottom of the number line",
                     forged(good, "\"y\":\"2\"", "\"y\":\"-9223372036854775808\"")});

    for (const Case& c : cases) {
        CAPTURE(c.what);
        WorkshopDoc live = good;
        const Written refused = persist::load_into(live, c.text);
        CHECK_FALSE(refused.accepted);
        CHECK_FALSE(refused.refusal.empty()); // a refusal without a reason is not one
        CHECK(live == good);                  // THE claim
    }

    // The control: the unforged text is accepted, so the loop above is not
    // measuring a loader that refuses everything.
    WorkshopDoc live = two_panels();
    CHECK(persist::load_into(live, valid).accepted);
    CHECK(live == good);
}

TEST_CASE("a refusal says which fact was wrong, in words a maker can act on") {
    // Diagnostics matter more once state survives a process: the file is a
    // thing a maker owns and can open, so a refusal that names the field and
    // the object is a refusal they can fix.
    const WorkshopDoc good = rich_document();

    const Written unknown =
        persist::from_text(forged(good, "\"next_id\":", "\"colour\":\"red\",\"next_id\":"))
            .outcome;
    CHECK_FALSE(unknown.accepted);
    CHECK(unknown.refusal.find("colour") != std::string::npos);

    const Written version =
        persist::from_text(forged(good, "\"format_version\":\"1\"", "\"format_version\":\"7\""))
            .outcome;
    CHECK_FALSE(version.accepted);
    CHECK(version.refusal.find("7") != std::string::npos);
    CHECK(version.refusal.find("version 1") != std::string::npos);

    const Written mode =
        persist::from_text(forged(good, "\"mode\":\"percent\"", "\"mode\":\"furlongs\"")).outcome;
    CHECK_FALSE(mode.accepted);
    CHECK(mode.refusal.find("furlongs") != std::string::npos);
    CHECK(mode.refusal.find("percent") != std::string::npos); // and what would have worked

    const Written foreign =
        persist::from_text(forged(good, "\"zengine-workshop\"", "\"blender\"")).outcome;
    CHECK_FALSE(foreign.accepted);
    CHECK(foreign.refusal.find("blender") != std::string::npos);

    // A document-law refusal names the object it is about.
    WorkshopDoc live;
    const Written duplicate =
        persist::load_into(live, forged(good, "\"id\":\"2\"", "\"id\":\"1\""));
    CHECK_FALSE(duplicate.accepted);
    CHECK(duplicate.refusal.find("#1") != std::string::npos);
}

TEST_CASE("the document law is the maker's law, and it is stated in one place") {
    // A loaded document meets the SAME rules a maker's edits meet. The proof is
    // that each per-value refusal below is worded by the very function the
    // interactive path calls -- so the two cannot come to disagree about what a
    // legal extent, coordinate or name is.
    WorkshopDoc d;
    doc::add(d, "p", 0, 0, ui::Extent{ui::kExtentCells, 4}, ui::Extent{ui::kExtentCells, 4});

    WorkshopDoc bad_extent = d;
    bad_extent.elements[0].width = ui::Extent{ui::kExtentPercent, 500};
    CHECK(doc::check_document(bad_extent).refusal ==
          "#1: width: " + doc::check_extent(ui::Extent{ui::kExtentPercent, 500}).refusal);

    WorkshopDoc bad_coord = d;
    bad_coord.elements[0].x = -1;
    CHECK(doc::check_document(bad_coord).refusal ==
          "#1: " + doc::check_coord(-1, ui::kRootContext).refusal);

    WorkshopDoc bad_name = d;
    bad_name.elements[0].label.clear();
    CHECK(doc::check_document(bad_name).refusal == "#1: " + doc::check_name("").refusal);

    // And the two laws that are new -- the ones a maker's path holds by
    // construction and therefore never had to state.
    WorkshopDoc twice = d;
    twice.elements.push_back(twice.elements[0]);
    twice.next_id = 9;
    CHECK_FALSE(doc::check_document(twice).accepted);

    WorkshopDoc behind = d;
    behind.next_id = 1;
    CHECK_FALSE(doc::check_document(behind).accepted);

    CHECK(doc::check_document(d).accepted);
}

TEST_CASE("restore keeps the candidate's identities rather than minting new ones") {
    // The difference between "the same document came back" and "a lookalike was
    // built from it". A loader written on `create` would produce the second and
    // display the first.
    WorkshopDoc live = two_panels(); // ids 1, 2, mint 3
    WorkshopDoc candidate;
    candidate.next_id = 41;
    candidate.elements.push_back(ui::Element{7, "seven", ui::kRootContext, 1, 1,
                                             ui::Extent{ui::kExtentCells, 5},
                                             ui::Extent{ui::kExtentCells, 5}});
    candidate.elements.push_back(ui::Element{40, "forty", ui::kRootContext, 2, 2,
                                             ui::Extent{ui::kExtentCells, 5},
                                             ui::Extent{ui::kExtentCells, 5}});

    REQUIRE(doc::restore(live, candidate).accepted);
    CHECK(live.elements[0].id == 7);
    CHECK(live.elements[1].id == 40);
    CHECK(live.next_id == 41);
    CHECK(doc::add_default(live) == 41);
}

TEST_CASE("the mint can be spent, and creating says so rather than overflowing") {
    // A document arriving from a file can say its mint is at the end of the
    // number line. `next_id++` there is signed overflow -- undefined behaviour
    // produced by data, reachable through an ordinary maker gesture -- so the
    // exhausted mint is an ANSWER. (The sanitizer lane is what would catch the
    // version of this that merely looked fine.)
    WorkshopDoc spent;
    spent.next_id = doc::kMaxIdentity;
    CHECK(doc::check_document(spent).accepted); // it is a legal document
    CHECK_FALSE(doc::can_mint(spent));

    Session s;
    CHECK(create(spent, s) == 0);
    CHECK(spent.elements.empty());
    CHECK(spent.next_id == doc::kMaxIdentity);
    CHECK(s.selected == 0); // a gesture that could not happen moved nothing

    // One below the end still works, and lands exactly on the last identity.
    WorkshopDoc last;
    last.next_id = doc::kMaxIdentity - 1;
    CHECK(doc::add_default(last) == doc::kMaxIdentity - 1);
    CHECK(last.next_id == doc::kMaxIdentity);
    CHECK(doc::add_default(last) == 0);

    // And it survives the file: a spent mint is written and read as a spent mint.
    WorkshopDoc live;
    REQUIRE(persist::load_into(live, persist::to_text(last)).accepted);
    CHECK(live.next_id == doc::kMaxIdentity);
    CHECK_FALSE(doc::can_mint(live));
}

// ---- The file on disk -------------------------------------------------------

TEST_CASE("a document written to a file is the document read back from it") {
    TempDir dir("roundtrip");
    const WorkshopDoc original = rich_document();
    REQUIRE(persist::save_file(dir.document(), original).accepted);

    WorkshopDoc live = two_panels();
    REQUIRE(persist::load_file(dir.document(), live).accepted);
    CHECK(live == original);

    // The bytes on disk are the bytes the writer produced -- no wrapper, no
    // trailer, nothing added by the file layer.
    CHECK(slurp(dir.document()) == persist::to_text(original));

    // And the sibling it was written through is gone.
    CHECK_FALSE(std::filesystem::exists(persist::pending_path(dir.document())));

    // Saving again over an existing document replaces it. Stated as a case
    // because the replace is the platform's `rename` and a platform that
    // refused an existing destination would otherwise fail only on the SECOND
    // save a maker ever performs.
    WorkshopDoc second = original;
    REQUIRE(doc::rename(second, second.elements[0].id, "renamed").accepted);
    REQUIRE(persist::save_file(dir.document(), second).accepted);
    WorkshopDoc reread;
    REQUIRE(persist::load_file(dir.document(), reread).accepted);
    CHECK(reread == second);
}

TEST_CASE("a missing file is an ordinary refusal, not a crash and not an empty document") {
    TempDir dir("missing");
    WorkshopDoc live = two_panels();
    const WorkshopDoc before = live;
    const Written refused = persist::load_file(dir.file("never-written.json"), live);
    CHECK_FALSE(refused.accepted);
    CHECK(refused.refusal.find("never-written.json") != std::string::npos);
    CHECK(live == before);
}

TEST_CASE("a detected write failure leaves the last good save readable and unchanged") {
    // The reason the writer never opens the destination: a save that fails must
    // not be able to turn a maker's document into an empty or half-written file.
    TempDir dir("failsave");
    const WorkshopDoc first = two_panels();
    REQUIRE(persist::save_file(dir.document(), first).accepted);
    const std::string good_bytes = slurp(dir.document());
    REQUIRE_FALSE(good_bytes.empty());

    // A controlled, deterministic, non-destructive failure: the sibling path the
    // writer must use is occupied by a DIRECTORY, so the write cannot open. No
    // permission games, and the same result on every supported platform.
    std::filesystem::create_directories(persist::pending_path(dir.document()));

    WorkshopDoc second = first;
    REQUIRE(doc::rename(second, second.elements[0].id, "renamed").accepted);
    REQUIRE(doc::add_default(second) != 0);
    REQUIRE_FALSE(second == first);

    const Written refused = persist::save_file(dir.document(), second);
    CHECK_FALSE(refused.accepted);
    CHECK_FALSE(refused.refusal.empty());

    // The last good save is intact, byte for byte, and still loads.
    CHECK(slurp(dir.document()) == good_bytes);
    WorkshopDoc reloaded;
    REQUIRE(persist::load_file(dir.document(), reloaded).accepted);
    CHECK(reloaded == first);

    // And the document in memory is still the one the maker is working on.
    CHECK_FALSE(second == first);
    CHECK(second.elements[0].label == "renamed");

    std::error_code ec;
    std::filesystem::remove_all(persist::pending_path(dir.document()), ec);
    CHECK(persist::save_file(dir.document(), second).accepted); // and it works again
    WorkshopDoc now;
    REQUIRE(persist::load_file(dir.document(), now).accepted);
    CHECK(now == second);
}

TEST_CASE("a save into a place that does not exist refuses before it writes anything") {
    TempDir dir("nowhere");
    const std::string path = (dir.path() / "no-such-directory" / "document.json").string();
    const Written refused = persist::save_file(path, two_panels());
    CHECK_FALSE(refused.accepted);
    CHECK_FALSE(std::filesystem::exists(path));
    CHECK_FALSE(std::filesystem::exists(persist::pending_path(path)));
}

TEST_CASE("a file too large to be a document is refused before it is read") {
    TempDir dir("huge");
    const std::string path = dir.document();
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        const std::string chunk(1u << 16, 'x');
        for (int i = 0; i < 80; ++i) { // 5 MiB, past the 4 MiB ceiling
            out.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        }
    }
    REQUIRE(std::filesystem::file_size(path) > persist::kMaxDocumentBytes);

    WorkshopDoc live = two_panels();
    const WorkshopDoc before = live;
    const Written refused = persist::load_file(path, live);
    CHECK_FALSE(refused.accepted);
    CHECK(refused.refusal.find("larger") != std::string::npos);
    CHECK(live == before);
}

// ============================================================================
// Tier 6 — persistence THROUGH THE WEAVE, on a real bus
// ============================================================================
//
// Everything above is about the document and the file. These are about the
// APPLICATION: a maker presses ^s, and what the session does about it.

TEST_CASE("^s saves and ^o loads, through the real message path") {
    TempDir dir("live");
    Live t;
    t.host.document_path = dir.document();

    // Nothing has been saved yet, and the status line says so.
    t.key(input::scan::kN); // republish, so the note reflects the path
    REQUIRE_FALSE(t.notes.empty());
    CHECK(t.status_note().find(dir.document()) != std::string::npos);
    CHECK(t.status_note().find("UNSAVED") != std::string::npos);

    t.key(input::scan::kS, input::mod::kCtrl);
    CHECK(t.notice() == "saved " + dir.document());
    CHECK(std::filesystem::exists(dir.document()));
    CHECK(t.status_note().find(dir.document() + " saved") != std::string::npos);
    const WorkshopDoc as_saved = t.doc();

    // Change it. The status line notices without anyone setting a flag.
    t.key(input::scan::kL);
    CHECK(t.status_note().find("UNSAVED") != std::string::npos);
    CHECK_FALSE(t.doc() == as_saved);

    // And ^o brings the saved one back.
    t.key(input::scan::kO, input::mod::kCtrl);
    CHECK(t.doc() == as_saved);
    CHECK(t.notice() == "loaded " + dir.document() + " -- 3 objects");
    CHECK(t.status_note().find(dir.document() + " saved") != std::string::npos);

    // Editing back to what was saved says `saved` again -- a comparison cannot
    // drift from the thing it describes, and a dirty flag would have said
    // otherwise here.
    t.key(input::scan::kL);
    CHECK(t.status_note().find("UNSAVED") != std::string::npos);
    t.key(input::scan::kH);
    CHECK(t.doc() == as_saved);
    CHECK(t.status_note().find(dir.document() + " saved") != std::string::npos);
}

TEST_CASE("^s refuses while a row is being edited, and writes nothing") {
    // The draft policy. A save that quietly wrote the OLD width while a NEW one
    // is on the screen with a cursor after it would put the file and the
    // maker's eyes in disagreement, with nothing to say so.
    TempDir dir("draft");
    Live t;
    t.host.document_path = dir.document();

    t.begin_editing("Width");
    REQUIRE(t.row("Width")->editing());
    for (int i = 0; i < 8; ++i) {
        t.key(input::scan::kBackspace);
    }
    t.text("7");

    t.key(input::scan::kS, input::mod::kCtrl);
    CHECK(t.notice() == "Width is still being edited -- enter commits, esc cancels; "
                        "nothing was saved");
    CHECK(t.session().notice_is_bad);
    CHECK_FALSE(std::filesystem::exists(dir.document())); // nothing was written
    CHECK(t.row("Width")->editing());                     // and the draft is intact
    CHECK(t.row("Width")->draft() == "7");

    // Cancel and it saves. (Commit would too; the point is that the maker says
    // which, rather than the save deciding for them.)
    t.key(input::scan::kEscape);
    t.key(input::scan::kS, input::mod::kCtrl);
    CHECK(t.notice() == "saved " + dir.document());
    CHECK(std::filesystem::exists(dir.document()));
}

TEST_CASE("a successful load cancels a drag and cannot continue an old resize") {
    // No dangling reference may survive a document replacement. A pointer
    // already down held an identity and an offset from an object that is gone.
    TempDir dir("drag");
    Live t;
    t.host.document_path = dir.document();
    t.key(input::scan::kS, input::mod::kCtrl);
    REQUIRE(t.notice().rfind("saved", 0) == 0);

    // Take hold of the second object's body and start moving it.
    //
    // The coordinates are copied out as NUMBERS and not held as a pointer into
    // the document, and that is this phase's own hazard rather than style: a
    // load REPLACES the element vector, so every pointer into it dangles the
    // instant the load succeeds. The first draft of this case kept
    // `const ui::Element*` across the load and the sanitizer lane called it a
    // heap-use-after-free while the ordinary lane passed -- the third time that
    // pairing has paid for itself in this file.
    const std::int64_t held_id = t.second()->id;
    const std::int64_t held_x = t.second()->x;
    const std::int64_t held_y = t.second()->y;
    t.press(held_x + 1, held_y + 1);
    REQUIRE(t.session().drag.active);
    REQUIRE(t.session().drag.id == held_id);
    REQUIRE_FALSE(t.session().drag.resizing);

    t.key(input::scan::kO, input::mod::kCtrl);
    CHECK_FALSE(t.session().drag.active);
    CHECK(t.session().drag.id == 0);
    CHECK(t.notice().rfind("loaded", 0) == 0);

    // The pointer is still down; moving it now must author nothing.
    const WorkshopDoc after_load = t.doc();
    t.motion(held_x + 20, held_y + 20);
    CHECK(t.doc() == after_load);

    // The same, for a resize: take the handle, then load.
    const Handle handle = size_handle(t.doc(), t.session());
    REQUIRE(handle.shown);
    t.press(handle.x, handle.y);
    REQUIRE(t.session().drag.resizing);
    t.key(input::scan::kO, input::mod::kCtrl);
    CHECK_FALSE(t.session().drag.active);
    CHECK_FALSE(t.session().drag.resizing);
    const WorkshopDoc after_second_load = t.doc();
    t.motion(handle.x + 6, handle.y + 6);
    CHECK(t.doc() == after_second_load);
}

TEST_CASE("selection after a load is re-established, never inherited") {
    // A loaded document is a DIFFERENT document. Keeping the old selected id
    // would silently alias whatever new object happened to carry that number --
    // the identity confusion the whole arc is arranged to prevent, arriving
    // through the back door. So the selection is set by the rule that opens a
    // fresh Workshop: the first object.
    TempDir dir("selection");
    Live t;
    t.host.document_path = dir.document();
    t.key(input::scan::kS, input::mod::kCtrl);

    // Select the SECOND object, so "kept" and "re-established" differ.
    t.key(input::scan::kTab);
    const std::int64_t was = t.session().selected;
    REQUIRE(was == t.second()->id);
    REQUIRE(was != t.first()->id);

    t.key(input::scan::kO, input::mod::kCtrl);
    CHECK(t.session().selected == t.first()->id);
    CHECK(t.session().selected != was);
    CHECK(t.row("Identity")->value() == "#" + std::to_string(t.first()->id));

    // A load into an empty document selects nothing, and the screen says so
    // rather than going blank.
    WorkshopDoc empty;
    spillout(dir.document(), persist::to_text(empty));
    t.key(input::scan::kO, input::mod::kCtrl);
    CHECK(t.doc().elements.empty());
    CHECK(t.session().selected == 0);
    CHECK(t.session().rows.empty());
    CHECK(object_row(t.canvases.back(), t.doc(), t.session(), 0) == "(none) -- n makes one");
}

TEST_CASE("a failed load costs a maker nothing but the notice") {
    // Failure must not destroy valid session state. The document, the
    // selection, the cursor and any draft are exactly what they were.
    TempDir dir("failload");
    Live t;
    t.host.document_path = dir.document();
    t.key(input::scan::kN); // make a third object, so the document is the maker's
    t.key(input::scan::kTab);

    const WorkshopDoc before_doc = t.doc();
    const std::int64_t before_selected = t.session().selected;
    const std::size_t before_cursor = t.session().cursor;

    // No file at all.
    t.key(input::scan::kO, input::mod::kCtrl);
    CHECK(t.session().notice_is_bad);
    CHECK(t.doc() == before_doc);
    CHECK(t.session().selected == before_selected);
    CHECK(t.session().cursor == before_cursor);

    // A file that is not a document.
    spillout(dir.document(), "{\"zen\":1,\"schema\":\"Nope\"");
    t.key(input::scan::kO, input::mod::kCtrl);
    CHECK(t.session().notice_is_bad);
    CHECK(t.doc() == before_doc);
    CHECK(t.session().selected == before_selected);

    // A real Workshop document with one illegal fact in it.
    WorkshopDoc bad = before_doc;
    bad.elements[0].x = -1;
    spillout(dir.document(), persist::to_text(bad));
    t.key(input::scan::kO, input::mod::kCtrl);
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("#" + std::to_string(before_doc.elements[0].id)) != std::string::npos);
    CHECK(t.doc() == before_doc);
    CHECK(t.session().selected == before_selected);

    // And the good one still loads, so the three refusals above are not a
    // Workshop that had stopped loading anything.
    spillout(dir.document(), persist::to_text(before_doc));
    t.key(input::scan::kO, input::mod::kCtrl);
    CHECK_FALSE(t.session().notice_is_bad);
    CHECK(t.doc() == before_doc);
}

TEST_CASE("with no document file, save and open say so instead of guessing one") {
    Live t; // host.document_path left empty, as a suite-mounted weave has it
    const WorkshopDoc before = t.doc();

    t.key(input::scan::kS, input::mod::kCtrl);
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("--document") != std::string::npos);

    t.key(input::scan::kO, input::mod::kCtrl);
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("--document") != std::string::npos);
    CHECK(t.doc() == before);

    // And the status line does not claim a file it does not have.
    CHECK(t.status_note().find("saved") == std::string::npos);
    CHECK(t.status_note().find("UNSAVED") == std::string::npos);
}

TEST_CASE("a bare s and a bare o are not commands, and Ctrl is what makes them one") {
    // The same shape as the Ctrl+C case: the modifier is READ, not implied by
    // the key. A bare `o` and a bare `s` do nothing at all, so nothing can come
    // to depend on them.
    TempDir dir("modifier");
    Live t;
    t.host.document_path = dir.document();
    const WorkshopDoc before = t.doc();

    t.key(input::scan::kS);
    CHECK_FALSE(std::filesystem::exists(dir.document()));
    t.key(input::scan::kO);
    CHECK(t.doc() == before);

    t.key(input::scan::kS, input::mod::kCtrl);
    CHECK(std::filesystem::exists(dir.document()));
}

TEST_CASE("the whole cross-process story, in one session") {
    // The headless twin of the live witness in the report: author, save, lose
    // the process, come back, and get the work -- identities and all -- while
    // the mint refuses to rewind.
    TempDir dir("crossprocess");

    std::string on_disk;
    std::int64_t doomed = 0;
    {
        Live run_a;
        run_a.host.document_path = dir.document();
        run_a.key(input::scan::kN); // #3
        doomed = run_a.session().selected;
        run_a.key(input::scan::kL); // move it
        run_a.key(input::scan::kL, input::mod::kShift);
        run_a.key(input::scan::kD); // and delete it again
        REQUIRE(run_a.doc().elements.size() == 2);
        REQUIRE(run_a.doc().next_id == doomed + 1);
        run_a.key(input::scan::kS, input::mod::kCtrl);
        REQUIRE(run_a.notice().rfind("saved", 0) == 0);
        on_disk = slurp(dir.document());
    } // run A is gone, with its bus, its weave and its whole session

    REQUIRE_FALSE(on_disk.empty());

    Live run_b; // a fresh process: its own opening document, its own session
    run_b.host.document_path = dir.document();
    REQUIRE_FALSE(run_b.doc().next_id == doomed + 1);

    run_b.key(input::scan::kO, input::mod::kCtrl);
    CHECK(run_b.doc().elements.size() == 2);
    CHECK(run_b.doc().next_id == doomed + 1);
    CHECK(run_b.doc().elements[0].width == ui::Extent{ui::kExtentPercent, 60});
    CHECK(run_b.doc().elements[1].width == ui::Extent{ui::kExtentCells, 14});

    // The identity that died before the save stays dead.
    run_b.key(input::scan::kN);
    CHECK(run_b.session().selected == doomed + 1);
    CHECK(run_b.session().selected != doomed);

    // And what run B saves is what run A saved, plus exactly the new object.
    run_b.key(input::scan::kD);
    run_b.key(input::scan::kS, input::mod::kCtrl);
    CHECK(slurp(dir.document()) != on_disk); // the mint moved, and that is a fact
    WorkshopDoc back;
    REQUIRE(persist::load_file(dir.document(), back).accepted);
    CHECK(back.elements.size() == 2);
    CHECK(back.next_id == doomed + 2);
}

// ============================================================================
// Tier 13 — the SETUP: a maker names the arrangement they are working in (WS-0)
// ============================================================================
//
// The question this tier answers is the one the phase set out to ask:
//
//     what is the smallest thing a maker can NAME, LEAVE, and COME BACK TO,
//     and how does it stay honest when one of the panes it names is one this
//     build has never heard of?
//
// The answer is a human name and an ordered list of two-string references, and
// the cases below are arranged so that every way of getting it wrong is visible:
//
//   PURE       the value, its law, its bounds, and the fallible resolution that
//              is the whole reason a reference is text rather than an ordinal.
//   RECONCILED authored intent onto resolved presentations, with the Builder and
//              Info as deliberately opposite witnesses -- one has a provider to
//              ask and a copy to forget, the other has neither.
//   PERSISTED  the file's own identity, its refusals, and the round trip.
//   LIVE       through the real weave on a real bus, driven by published input,
//              including two processes' worth of it.
//
// WHAT IS DELIBERATELY ABSENT from every case here: a rectangle, a placement, a
// screen extent, a WeaveId, a picker cursor, an integer panel kind, and any
// assertion that a provider exists.

// ---- The value, its law and its bounds ---------------------------------------

TEST_CASE("a pane reference is two strings, and the pair is the identity") {
    const PaneRef info = ref_of(panel::kInfo);
    CHECK(info.provider == std::string(kWorkshopProvider));
    CHECK(info.pane == std::string(pane_key::kInfo));
    CHECK(info == PaneRef{kWorkshopProvider, pane_key::kInfo});

    // NEITHER HALF ALONE IS THE IDENTITY. The same pane key under another
    // provider is a different pane, and that is the property that lets a later
    // provider have an `info` of its own without colliding with this one.
    CHECK_FALSE(info == PaneRef{"third.party.tools", pane_key::kInfo});
    CHECK_FALSE(info == PaneRef{kWorkshopProvider, "builder"});

    // And nothing is normalised: a reference comes back exactly as it went in.
    const PaneRef odd{"Third.Party.Tools", "History"};
    CHECK_FALSE(odd == stranger());
    CHECK(ref_text(odd) == "Third.Party.Tools/History");
}

TEST_CASE("two setups are the same setup when they name the same panes in the same order") {
    const Setup a = setup_of("Build", {panel::kInfo, panel::kBuilder});
    Setup b = setup_of("Build", {panel::kInfo, panel::kBuilder});
    CHECK(a == b);

    // THE NAME IS PART OF IT: renaming a setup makes it a different setup, which
    // is what makes `saved()` say UNSAVED after a rename that moved no pane.
    b.name = "Analysis";
    CHECK_FALSE(a == b);

    // ...AND SO IS THE ORDER. Today's two built-ins occupy different places so
    // the order changes no rectangle; the value still distinguishes them,
    // because the day a second kind is placed in the stack the order IS which
    // slot each one takes (`bounds_of`).
    const Setup other_way = setup_of("Build", {panel::kBuilder, panel::kInfo});
    CHECK_FALSE(a == other_way);
}

TEST_CASE("a fresh Workshop's setup and its open panels are ONE decision") {
    // The drift this case exists to prevent is silent: `default_setup()` saying
    // Info while `default_panels()` said something else would give a maker a
    // screen that disagreed with the setup line above it from the first frame.
    // Both are derived from `kDefaultPanels`, and this is the proof.
    const Setup fresh = default_setup();
    CHECK(fresh.name == std::string(kDefaultSetupName));
    REQUIRE(fresh.panes.size() == kDefaultPanelCount);

    std::vector<std::int64_t> from_setup;
    for (const SetupPane& p : fresh.panes) {
        const std::optional<std::int64_t> kind = resolve_pane(p.ref, no_providers());
        REQUIRE(kind.has_value());
        from_setup.push_back(*kind);
    }
    CHECK(from_setup == open_kinds(Panels{}));

    // And the product default itself, named rather than merely derived: Info.
    CHECK(from_setup == std::vector<std::int64_t>{panel::kInfo});

    // A default session agrees with both.
    const Session s;
    CHECK(s.setup.active == fresh);
    CHECK(open_kinds(s.panels) == from_setup);
}

TEST_CASE("every catalog row carries a durable reference that resolves back to it") {
    // Over the POPULATION, not over its size: a third kind is proved by this
    // case without being mentioned in it.
    for (std::size_t i = 0; i < kPanelKinds; ++i) {
        const PanelKind& row = kPanelCatalog[i];
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

    // The built-ins are spelled the way the phase promised a maker could read.
    CHECK(ref_text(ref_of(panel::kInfo)) == "zengine.workshop/info");
    CHECK(ref_text(ref_of(panel::kBuilder)) == "zengine.workshop/builder");
}

TEST_CASE("an unknown reference resolves to NOTHING, and never to the Builder") {
    // THE WHOLE REASON THE FALLIBLE DOOR EXISTS. `panel_kind` answers with the
    // catalog's first row for an unknown kind, which is right for its callers
    // and would be a lie here: an unknown reference routed through it would
    // paint a maker's third-party pane as Workshop's build tool.
    CHECK_FALSE(resolve_pane(stranger(), no_providers()).has_value());
    CHECK_FALSE(resolve_pane(PaneRef{"third.party.tools", pane_key::kInfo}, no_providers()).has_value());
    CHECK_FALSE(resolve_pane(PaneRef{kWorkshopProvider, "history"}, no_providers()).has_value());
    CHECK_FALSE(resolve_pane(PaneRef{"", ""}, no_providers()).has_value());
    CHECK_FALSE(resolvable(stranger(), no_providers()));

    // The negative control, stated as its own claim: the total lookup DOES
    // answer Builder for an unknown kind, and it is still allowed to, because
    // nothing that meets a file goes through it.
    CHECK(panel_kind(9999).kind == panel::kBuilder);
    CHECK(resolve_pane(stranger(), no_providers()).value_or(panel::kInfo) != panel::kBuilder);
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
    // fields a maker looking at their own file has to tell apart.
    CHECK(check_pane_ref(PaneRef{"", "info"}).refusal.find("provider") != std::string::npos);
    CHECK(check_pane_ref(PaneRef{"zengine.workshop", ""}).refusal.find("pane key") !=
          std::string::npos);

    // AND IT JUDGES A `std::string_view` (WP-0a), which is the whole of what that
    // phase changed about this law. The two boundaries are asserted through a view
    // to say so: exactly `kMaxPaneKeyLen` bytes accepted, one more refused, in the
    // wording WS-0a fixed.
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
    // the picker already, so refusing to save it would make one arrangement a
    // maker can produce impossible to name.
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
    Setup twice = setup_of("Twice", {panel::kInfo});
    // FORGED PAST THE DOOR ON PURPOSE: `add_pane` refuses a duplicate, so a case
    // about what `check_setup` says of one has to build it by hand -- with a rank
    // that is otherwise legal, so the refusal below is about the DUPLICATE and not
    // about the permutation.
    twice.panes.push_back(SetupPane{ref_of(panel::kInfo), {}, {}, {}, 1});
    CHECK_FALSE(check_setup(twice).accepted);
    CHECK(check_setup(twice).refusal.find("twice") != std::string::npos);
    CHECK(check_setup(twice).refusal.find("zengine.workshop/info") != std::string::npos);

    // ...and neither is more than a setup may hold. THE BOUND IS NOT THE
    // CATALOG'S POPULATION, deliberately: a setup must be able to retain
    // references to panes this build has never heard of, so a limit cut to
    // `kPanelKinds` would refuse the one case the design exists to allow.
    CHECK(kMaxSetupPanes > kPanelKinds);
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
    const Setup unnamed = setup_of("", {panel::kInfo});
    CHECK_FALSE(check_setup(unnamed).accepted);
    Setup bad_ref = setup_of("Bad", {});
    REQUIRE(add_pane(bad_ref, PaneRef{"has space", "info"}));
    CHECK_FALSE(check_setup(bad_ref).accepted);
}

TEST_CASE("adding and removing a pane preserves order and never duplicates") {
    Setup s;
    s.name = "Work";
    CHECK(add_pane(s, ref_of(panel::kInfo)));
    CHECK(add_pane(s, ref_of(panel::kBuilder)));
    CHECK_FALSE(add_pane(s, ref_of(panel::kInfo))); // already there, and it says so
    REQUIRE(s.panes.size() == 2);
    CHECK(s.panes[0].ref == ref_of(panel::kInfo));
    CHECK(s.panes[1].ref == ref_of(panel::kBuilder));
    CHECK(check_setup(s).accepted);

    // ADDED AT THE END, which is where `open_panel` has always put a newly
    // opened panel -- so the authored order agrees with the resolved order a
    // maker was already watching.
    CHECK(remove_pane(s, ref_of(panel::kInfo)));
    REQUIRE(s.panes.size() == 1);
    CHECK(s.panes[0].ref == ref_of(panel::kBuilder));
    CHECK_FALSE(remove_pane(s, ref_of(panel::kInfo)));

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
    REQUIRE(add_pane(s, ref_of(panel::kInfo)));
    REQUIRE(add_pane(s, stranger()));
    REQUIRE(add_pane(s, PaneRef{"other.tools", "graph"}));
    REQUIRE(add_pane(s, ref_of(panel::kBuilder)));

    const std::vector<PaneRef> waiting = unresolved_panes(s, no_providers());
    REQUIRE(waiting.size() == 2);
    CHECK(waiting[0] == stranger());
    CHECK(waiting[1] == PaneRef{"other.tools", "graph"});
    CHECK(unresolved_panes(default_setup(), no_providers()).empty());
}

// ---- Authored intent, reconciled onto resolved presentations ------------------

TEST_CASE("reconciling opens what the setup names, in the setup's order") {
    Panels panels;
    REQUIRE(open_kinds(panels) == std::vector<std::int64_t>{panel::kInfo});

    const Reconciled done = reconcile(panels, setup_of("Both", {panel::kBuilder, panel::kInfo}), min_room());
    CHECK(done.opened == std::vector<std::int64_t>{panel::kBuilder});
    CHECK(done.closed.empty());
    CHECK(done.unresolved == 0);
    // THE ORDER IS THE SETUP'S, not the order things happened to be opened in.
    CHECK(open_kinds(panels) == std::vector<std::int64_t>{panel::kBuilder, panel::kInfo});

    // The other way round, from the same starting point, produces the other order.
    Panels again;
    (void)reconcile(again, setup_of("Both", {panel::kInfo, panel::kBuilder}), min_room());
    CHECK(open_kinds(again) == std::vector<std::int64_t>{panel::kInfo, panel::kBuilder});
}

TEST_CASE("reconciling closes what the setup does not name, through the existing door") {
    Panels panels;
    REQUIRE(open_panel(panels, panel::kBuilder));
    panels.builder.heard = true;
    panels.builder.shown.recipe = "zengine-snake";

    const Reconciled done = reconcile(panels, setup_of("Info only", {panel::kInfo}), min_room());
    CHECK(done.closed == std::vector<std::int64_t>{panel::kBuilder});
    CHECK(done.opened.empty());
    CHECK(open_kinds(panels) == std::vector<std::int64_t>{panel::kInfo});

    // THE PANEL'S COPY IS FORGOTTEN BY THE SAME ACT, because the close goes
    // through `close_panel` rather than through a loop that rebuilt the vector.
    // A removed Builder whose copied status outlived it would make a reopened
    // panel look instantly informed about something minutes stale.
    CHECK_FALSE(panels.builder.heard);
    CHECK(panels.builder.shown.recipe.empty());
}

TEST_CASE("a panel open on both sides of a reconcile keeps what it was showing") {
    // OPEN BEFORE, OPEN AFTER -- the case that says a reconcile is not a rebuild.
    // Restoring the setup you are already in must not be a visible event.
    Panels panels;
    REQUIRE(open_panel(panels, panel::kBuilder));
    panels.builder.heard = true;
    panels.builder.shown.recipe = "zengine-snake";
    panels.builder.shown.builds = 3;

    const Setup same = setup_of("Both", {panel::kInfo, panel::kBuilder});
    const Reconciled done = reconcile(panels, same, min_room());
    CHECK(done.opened.empty());
    CHECK(done.closed.empty());
    CHECK(panels.builder.heard);
    CHECK(panels.builder.shown.recipe == "zengine-snake");
    CHECK(panels.builder.shown.builds == 3);

    // ...and doing it a second time changes nothing at all.
    const Reconciled twice = reconcile(panels, same, min_room());
    CHECK(twice.opened.empty());
    CHECK(twice.closed.empty());
    CHECK(panels.builder.shown.builds == 3);
}

TEST_CASE("an unresolved reference is counted, and produces no panel of any kind") {
    Panels panels;
    Setup s = setup_of("Mixed", {panel::kInfo});
    REQUIRE(add_pane(s, stranger()));
    REQUIRE(add_pane(s, PaneRef{"other.tools", "graph"}));

    const Reconciled done = reconcile(panels, s, min_room());
    CHECK(done.unresolved == 2);
    // NO PLACEHOLDER, NO SLOT, NO FALL-THROUGH TO THE BUILDER. The only kind
    // available to paint an unknown pane with is the Builder, which is exactly
    // why the resolution had to be fallible before this line could be written.
    CHECK(open_kinds(panels) == std::vector<std::int64_t>{panel::kInfo});
    CHECK_FALSE(panels.has(panel::kBuilder));
    // And the setup still holds all three: reconciling takes it by const
    // reference and could not drop one if it wanted to.
    CHECK(s.panes.size() == 3);
    CHECK(s.panes[1].ref == stranger());
}

TEST_CASE("an empty setup closes everything, and is a legal thing to be in") {
    Panels panels;
    REQUIRE(open_panel(panels, panel::kBuilder));
    Setup nothing;
    nothing.name = "Nothing";

    const Reconciled done = reconcile(panels, nothing, min_room());
    CHECK(done.closed.size() == 2);
    CHECK(panels.open.empty());
    CHECK(done.unresolved == 0);

    // And back again from empty, which is the case that proves `opened` names
    // every kind rather than only the ones that were never open.
    const Reconciled back = reconcile(panels, setup_of("Both", {panel::kInfo, panel::kBuilder}), min_room());
    CHECK(back.opened.size() == 2);
    CHECK(open_kinds(panels) == std::vector<std::int64_t>{panel::kInfo, panel::kBuilder});
}

TEST_CASE("reconciling touches the picker, the document and the screen not at all") {
    Panels panels;
    panels.picker.open = true;
    panels.picker.cursor = 1;
    (void)reconcile(panels, setup_of("Builder", {panel::kBuilder}), min_room());
    // The picker is INTERACTION STATE. It is not setup intent, it is not in the
    // file, and reconciling is not a reason to open or close it.
    CHECK(panels.picker.open);
    CHECK(panels.picker.cursor == 1);
}

// ---- The setup's own file ------------------------------------------------------

TEST_CASE("a setup file says what it is, in words a maker can read") {
    const std::string text = setup_persist::to_text(default_setup());
    INFO(text);

    // ITS OWN FORMAT IDENTITY, beside the document's and not equal to it.
    CHECK(text.find("\"format\":\"zengine-workshop-setup\"") != std::string::npos);
    CHECK(text.find("\"format_version\":\"3\"") != std::string::npos);
    CHECK(text.find("\"name\":\"Default\"") != std::string::npos);
    CHECK(text.find("\"provider\":\"zengine.workshop\"") != std::string::npos);
    CHECK(text.find("\"pane\":\"info\"") != std::string::npos);

    // NO INTEGER PANEL KIND ANYWHERE IN IT. This is the assertion to check this
    // format against first: the file names panes the way a person and a future
    // provider would, and never the way this build's own vocabulary does.
    CHECK(text.find("\"kind\"") == std::string::npos);
    CHECK(text.find("placement") == std::string::npos);
    CHECK(text.find("rect") == std::string::npos);
    CHECK(text.find("weave") == std::string::npos);

    // ...and it is not the document's format, which is the whole point of it
    // having one of its own: handing Workshop the wrong one of its own two
    // files is named rather than half-read.
    CHECK(std::string(setup_persist::kFormat) != std::string(persist::kFormat));
}

TEST_CASE("every shape of setup survives a round trip through its file") {
    struct Case {
        const char* what;
        Setup setup;
    };
    std::vector<Case> cases;
    cases.push_back({"the default, Info only", default_setup()});
    cases.push_back({"Builder only", setup_of("Build", {panel::kBuilder})});
    cases.push_back({"both, in a deliberate order",
                     setup_of("Everything", {panel::kBuilder, panel::kInfo})});
    cases.push_back({"both, in the other order",
                     setup_of("Everything", {panel::kInfo, panel::kBuilder})});
    Setup nothing;
    nothing.name = "Nothing at all";
    cases.push_back({"an empty pane list", nothing});
    cases.push_back({"a human name with spaces in it", setup_of("Morning build", {panel::kInfo})});
    cases.push_back({"a name at the bound",
                     setup_of(std::string(kMaxSetupNameLen, 'n'), {panel::kInfo})});
    Setup mixed = setup_of("Mixed", {panel::kInfo});
    REQUIRE(add_pane(mixed, stranger()));
    REQUIRE(add_pane(mixed, ref_of(panel::kBuilder)));
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
    REQUIRE(add_pane(s, ref_of(panel::kBuilder)));
    REQUIRE(add_pane(s, stranger()));
    REQUIRE(add_pane(s, ref_of(panel::kInfo)));

    const setup_persist::LoadedSetup read = setup_persist::from_text(setup_persist::to_text(s));
    REQUIRE(read.outcome.accepted);
    REQUIRE(read.setup.panes.size() == 3);
    CHECK(read.setup.panes[0].ref == ref_of(panel::kBuilder));
    CHECK(read.setup.panes[1].ref == stranger());
    CHECK(read.setup.panes[2].ref == ref_of(panel::kInfo));

    // The unresolved entry's bytes are exactly the bytes that went in.
    const std::string text = setup_persist::to_text(read.setup);
    CHECK(text.find("\"provider\":\"third.party.tools\"") != std::string::npos);
    CHECK(text.find("\"pane\":\"history\"") != std::string::npos);
}

TEST_CASE("a malformed setup file is refused, and the live setup is untouched") {
    // The claim is not "the parser returned an error". It is that the setup a
    // maker is in is exactly what it was.
    const Setup good = setup_of("Everything", {panel::kInfo, panel::kBuilder});
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
                     loom::compat::serialize(loom::to_value(ui::Extent{0, 4}))});
    cases.push_back({"a Workshop DOCUMENT handed to the setup reader",
                     persist::to_text(two_panels())});
    cases.push_back({"a valid setup with a tail after it", valid + "x"});
    cases.push_back({"the wrong format identity",
                     forged_setup(good, "\"zengine-workshop-setup\"", "\"someone-elses-tool\"")});
    cases.push_back({"an unsupported format version",
                     forged_setup(good, "\"format_version\":\"3\"", "\"format_version\":\"9\"")});
    cases.push_back({"a missing required field",
                     forged_setup(good, "\"name\":\"Everything\",", "")});
    cases.push_back({"a field of the wrong kind",
                     forged_setup(good, "\"format_version\":\"3\"", "\"format_version\":3")});
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
                     forged_setup(good, "\"pane\":\"info\"", "\"pane\":\"\"")});
    cases.push_back({"a provider key longer than a key",
                     forged_setup(good, "\"provider\":\"zengine.workshop\"",
                                  "\"provider\":\"" + std::string(kMaxPaneKeyLen + 1, 'p') +
                                      "\"")});
    cases.push_back({"a pane key with a space in it",
                     forged_setup(good, "\"pane\":\"info\"", "\"pane\":\"two words\"")});
    cases.push_back({"the same pane named twice",
                     forged_setup(good, "\"pane\":\"builder\"", "\"pane\":\"info\"")});

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
    const Setup original = setup_of("Everything", {panel::kBuilder, panel::kInfo});
    REQUIRE(setup_persist::save_file(path, original).accepted);

    const setup_persist::LoadedSetup read = setup_persist::load_file(path);
    REQUIRE(read.outcome.accepted);
    CHECK(read.setup == original);

    // The bytes on disk are the bytes the writer produced -- no wrapper, no
    // trailer, nothing added by the file layer -- and the sibling is gone.
    CHECK(slurp(path) == setup_persist::to_text(original));
    CHECK_FALSE(std::filesystem::exists(persist::pending_path(path)));

    // Saving again over an existing setup replaces it.
    const Setup second = setup_of("Info only", {panel::kInfo});
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
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        const std::string chunk(1u << 12, 'x');
        for (int i = 0; i < 32; ++i) { // 128 KiB, past the 64 KiB ceiling
            out.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        }
    }
    REQUIRE(std::filesystem::file_size(path) > setup_persist::kMaxSetupBytes);

    const setup_persist::LoadedSetup refused = setup_persist::load_file(path);
    CHECK_FALSE(refused.outcome.accepted);
    CHECK(refused.outcome.refusal.find("larger") != std::string::npos);
    // It names WHICH of the two artifacts it was measuring against, because a
    // maker with two files needs to know which ceiling they met.
    CHECK(refused.outcome.refusal.find("setup") != std::string::npos);
}

TEST_CASE("a detected setup write failure leaves the last good setup file untouched") {
    // The reason the writer never opens the destination, asked about the second
    // artifact: a save that fails must not be able to turn a maker's saved
    // arrangement into an empty or half-written file.
    TempDir dir("setup-failsave");
    const std::string path = dir.file("setup.json");
    const Setup first = setup_of("Good", {panel::kInfo});
    REQUIRE(setup_persist::save_file(path, first).accepted);
    const std::string good_bytes = slurp(path);
    REQUIRE_FALSE(good_bytes.empty());

    // A controlled, deterministic, non-destructive failure: the sibling path the
    // writer must use is occupied by a DIRECTORY, so the write cannot open.
    std::filesystem::create_directories(persist::pending_path(path));

    const Setup second = setup_of("Better", {panel::kBuilder, panel::kInfo});
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

TEST_CASE("the document's file and the setup's file cannot be mistaken for each other") {
    TempDir dir("two-files");
    const std::string doc_path = dir.document();
    const std::string setup_path = dir.file("setup.json");
    REQUIRE(persist::save_file(doc_path, rich_document()).accepted);
    REQUIRE(setup_persist::save_file(setup_path, default_setup()).accepted);

    // Each reader refuses the other's file, by name, rather than half-reading it.
    CHECK_FALSE(setup_persist::load_file(doc_path).outcome.accepted);
    WorkshopDoc live = two_panels();
    const WorkshopDoc before = live;
    CHECK_FALSE(persist::load_file(setup_path, live).accepted);
    CHECK(live == before);

    // And writing one leaves the other's bytes alone -- which is what "two
    // artifacts" is worth, said in the only way that could fail.
    const std::string doc_bytes = slurp(doc_path);
    REQUIRE(setup_persist::save_file(setup_path, setup_of("Other", {panel::kBuilder})).accepted);
    CHECK(slurp(doc_path) == doc_bytes);
}

// ---- Through the real weave: the picker moves the intent -------------------------

TEST_CASE("a fresh Workshop's active setup and its open panels agree from the first frame") {
    Live t;
    CHECK(t.session().setup.active == default_setup());
    CHECK(open_kinds(t.session().panels) == std::vector<std::int64_t>{panel::kInfo});
    // UNSAVED, and structurally so: nothing has been written, and the copy the
    // comparison is made against has an empty name no legal setup can equal.
    CHECK_FALSE(t.session().setup.saved());
}

TEST_CASE("opening a panel through the picker moves the setup's intent, not only the screen") {
    // THE COHERENCE CLAIM. Before WS-0 the picker called `open_panel` directly;
    // if it still did, the active setup would go on describing an arrangement
    // the screen had stopped showing the moment a maker pressed `p`.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    open_builder(t);

    REQUIRE(t.session().panels.has(panel::kBuilder));
    CHECK(has_pane(t.session().setup.active, ref_of(panel::kBuilder)));
    // At the END of the authored order, which is where the screen put it.
    REQUIRE(t.session().setup.active.panes.size() == 2);
    CHECK(t.session().setup.active.panes[1].ref == ref_of(panel::kBuilder));
    // ...and the resolved open order agrees with the resolved order of the refs.
    CHECK(open_kinds(t.session().panels) ==
          std::vector<std::int64_t>{panel::kInfo, panel::kBuilder});

    // Removing it takes the reference back out.
    pick(t, panel::kBuilder);
    CHECK_FALSE(t.session().panels.has(panel::kBuilder));
    CHECK_FALSE(has_pane(t.session().setup.active, ref_of(panel::kBuilder)));

    // And so does removing Info, which has no provider to make it a special case.
    pick(t, panel::kInfo);
    CHECK_FALSE(t.session().panels.has(panel::kInfo));
    CHECK(t.session().setup.active.panes.empty());
}

TEST_CASE("a panel change makes the setup UNSAVED, and changing it back makes it saved again") {
    TempDir dir("setup-saved-truth");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    (void)mount_tool(t, "zengine-snake");

    name_setup(t, "Working");
    REQUIRE(t.session().setup.saved());
    CHECK(t.session().setup.active.name == "Working");
    CHECK(t.notice().find("saved setup \"Working\"") == 0);

    open_builder(t);
    CHECK_FALSE(t.session().setup.saved());

    // OPENING THEN CLOSING BACK TO THE SAVED INTENT SAYS SAVED AGAIN, which is
    // what a comparison buys and a dirty flag could not: there is no hand to
    // forget to unset.
    pick(t, panel::kBuilder);
    CHECK(t.session().setup.saved());

    // A rename with no pane moved is still a change, because the name is part of
    // the value.
    Setup renamed = t.session().setup.active;
    renamed.name = "Other";
    CHECK_FALSE(renamed == t.session().setup.on_file);
}

TEST_CASE("the `s` that opens the name editor does not type itself into the name") {
    // The trap WG-0 measured and named: a key transition and the character it
    // produced are two facts that were simultaneously true, and both arrive.
    TempDir dir("setup-swallow");
    Live t;
    t.host.setup_path = dir.file("setup.json");

    t.key(input::scan::kS);
    t.text("s");
    REQUIRE(t.session().setup.naming.open);
    // IT OPENED ON THE NAME THE SETUP ALREADY HAS, with no `s` appended.
    CHECK(t.session().setup.naming.line.text() == "Default");
    CHECK(t.session().setup.naming.line.caret() == std::string("Default").size());

    // A real keystroke arriving straight after IS taken -- the swallow belongs
    // to one moment and cannot eat the next character a maker meant.
    t.text("s");
    CHECK(t.session().setup.naming.line.text() == "Defaults");

    // AND THE SHIFTED KEY IS A DIFFERENT GESTURE SINCE KEY-0 (exact matching): shift+s
    // is not `s`, so it opens nothing -- the accidental subset alias this clause used to
    // pin is gone on purpose -- and the capital it produced lands nowhere, because
    // command mode takes no text. (The capital-owed half of the old claim lives on where
    // it is still true: a maker who AUTHORS a shift+letter binding gets its capital
    // swallowed by the same case-folding match; see the keymap cases.)
    Live shifted;
    shifted.host.setup_path = dir.file("other.json");
    shifted.key(input::scan::kS, input::mod::kShift);
    shifted.text("S");
    CHECK_FALSE(shifted.session().setup.naming.open);
}

TEST_CASE("escape leaves the setup name exactly as it was, and saves nothing") {
    TempDir dir("setup-cancel");
    Live t;
    t.host.setup_path = dir.file("setup.json");

    t.key(input::scan::kS);
    t.text("s");
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

    t.key(input::scan::kS);
    t.text("s");
    for (int i = 0; i < 8; ++i) {
        t.key(input::scan::kBackspace);
    }
    REQUIRE(t.session().setup.naming.line.empty());
    t.key(input::scan::kReturn);

    // STILL OPEN, so a maker fixes what they typed rather than retyping it, and
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
    CHECK(t.session().setup.saved());
}

TEST_CASE("with no setup file, naming and restoring say so and change nothing") {
    Live t; // no --setup was given
    t.key(input::scan::kS);
    CHECK_FALSE(t.session().setup.naming.open);
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("--setup") != std::string::npos);

    const Setup before = t.session().setup.active;
    t.key(input::scan::kR);
    CHECK(t.session().setup.active == before);
    CHECK(t.notice().find("--setup") != std::string::npos);
    // ...and it is a DIFFERENT sentence from the document's, because a maker
    // with one file and not the other has to know which one they are missing.
    CHECK(t.notice().find("--document") == std::string::npos);
}

TEST_CASE("restoring a setup returns the intent that was saved") {
    TempDir dir("setup-restore");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    (void)mount_tool(t, "zengine-snake");

    open_builder(t);
    pick(t, panel::kInfo); // Builder open, Info removed
    name_setup(t, "Build only");
    REQUIRE(t.session().setup.saved());

    // Wander away from it.
    pick(t, panel::kInfo);
    pick(t, panel::kBuilder);
    REQUIRE_FALSE(t.session().setup.saved());
    REQUIRE(open_kinds(t.session().panels) == std::vector<std::int64_t>{panel::kInfo});

    t.key(input::scan::kR);
    CHECK(t.session().setup.active.name == "Build only");
    CHECK(open_kinds(t.session().panels) == std::vector<std::int64_t>{panel::kBuilder});
    CHECK(t.session().setup.saved());
    CHECK(t.notice().find("restored setup \"Build only\"") == 0);
    CHECK(t.notice().find(t.host.setup_path) != std::string::npos);
}

TEST_CASE("a malformed setup file is refused without closing a single panel") {
    TempDir dir("setup-refuse-live");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    ToolSeat* tool = mount_tool(t, "zengine-snake");

    open_builder(t);
    name_setup(t, "Good");
    const Setup saved = t.session().setup.active;
    const std::vector<std::int64_t> panels_before = open_kinds(t.session().panels);
    const BuilderPane pane_before = t.session().panels.builder;
    const WorkshopDoc doc_before = t.doc();
    const std::int64_t asked_before = tool->described;

    // A file whose LAST field is the broken one, so a loader that acted as it
    // read would already have closed something by the time it noticed.
    spillout(t.host.setup_path,
             forged_setup(saved, "\"pane\":\"builder\"", "\"pane\":\"two words\""));
    t.key(input::scan::kR);

    CHECK(t.session().notice_is_bad);
    CHECK(t.session().setup.active == saved);
    CHECK(open_kinds(t.session().panels) == panels_before);
    CHECK(t.session().panels.builder.heard == pane_before.heard);
    CHECK(t.session().panels.builder.shown.recipe == pane_before.shown.recipe);
    CHECK(t.doc() == doc_before);
    // NOTHING WAS ASKED OF ANYBODY either: a refused restore is not a reason to
    // send a message on behalf of a panel that did not open.
    CHECK(tool->described == asked_before);
}

// ---- Lifecycle: the Builder and Info as opposite witnesses ------------------------

TEST_CASE("a restore that OPENS the Builder asks the tool what it is") {
    TempDir dir("setup-life-open");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    ToolSeat* tool = mount_tool(t, "zengine-snake");

    REQUIRE(setup_persist::save_file(t.host.setup_path,
                                     setup_of("Build", {panel::kBuilder}))
                .accepted);
    REQUIRE_FALSE(t.session().panels.has(panel::kBuilder));
    const std::int64_t before = tool->described;

    t.key(input::scan::kR);
    CHECK(t.session().panels.has(panel::kBuilder));
    // CLOSED BEFORE, OPEN AFTER: it performs the same asking opening it through
    // the picker has always performed, through the same one path.
    CHECK(tool->described == before + 1);
    CHECK(t.session().panels.builder.heard);
    CHECK(t.session().panels.builder.shown.recipe == "zengine-snake");
}

TEST_CASE("a restore that leaves the Builder open does not ask again or lose its view") {
    TempDir dir("setup-life-same");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    ToolSeat* tool = mount_tool(t, "zengine-snake");

    open_builder(t);
    REQUIRE(t.session().panels.builder.heard);
    const std::int64_t asked = tool->described;
    const zengine::builder::BuildStatus shown = t.session().panels.builder.shown;

    REQUIRE(setup_persist::save_file(t.host.setup_path,
                                     setup_of("Same", {panel::kInfo, panel::kBuilder}))
                .accepted);
    t.key(input::scan::kR);

    // OPEN BEFORE, OPEN AFTER: the presentation is left alone. No duplicate
    // refresh ceremony, and the copy it was showing is the copy it is showing.
    CHECK(tool->described == asked);
    CHECK(t.session().panels.builder.heard);
    CHECK(t.session().panels.builder.shown.recipe == shown.recipe);
    CHECK(t.session().panels.builder.shown.builds == shown.builds);
}

TEST_CASE("a restore that CLOSES the Builder forgets its copy and leaves the tool alive") {
    TempDir dir("setup-life-close");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    ToolSeat* tool = mount_tool(t, "zengine-snake");

    open_builder(t);
    REQUIRE(t.session().panels.builder.heard);

    REQUIRE(setup_persist::save_file(t.host.setup_path, setup_of("Info", {panel::kInfo}))
                .accepted);
    t.key(input::scan::kR);

    CHECK_FALSE(t.session().panels.has(panel::kBuilder));
    // OPEN BEFORE, CLOSED AFTER: the per-kind view is forgotten by the same act,
    // exactly as the picker's removal forgets it.
    CHECK_FALSE(t.session().panels.builder.heard);
    CHECK(t.session().panels.builder.shown.recipe.empty());

    // THE TOOL IS UNTOUCHED. Nothing was unloaded, nothing was told, and it
    // answers the very next ask with its own running total.
    const std::int64_t asked = tool->described;
    tool->next.builds = 7;
    open_builder(t);
    CHECK(tool->described == asked + 1);
    CHECK(t.session().panels.builder.shown.builds == 7);
}

TEST_CASE("Info opens and closes through a restore with no message and no document change") {
    TempDir dir("setup-life-info");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    ToolSeat* tool = mount_tool(t, "zengine-snake");

    const WorkshopDoc doc_before = t.doc();
    const std::int64_t selected = t.session().selected;
    REQUIRE(t.session().panels.has(panel::kInfo));

    // OPEN BEFORE, CLOSED AFTER: the document and the selection are untouched,
    // because Info holds no copy of anything -- what it presents outlives it and
    // belongs to somebody else.
    Setup nothing;
    nothing.name = "Nothing";
    REQUIRE(setup_persist::save_file(t.host.setup_path, nothing).accepted);
    t.key(input::scan::kR);
    CHECK(t.session().panels.open.empty());
    CHECK(t.doc() == doc_before);
    CHECK(t.session().selected == selected);
    CHECK(tool->described == 0);

    // CLOSED BEFORE, OPEN AFTER: it opens, and it asks nobody. A Workshop
    // hosting no tools at all does this and it works.
    REQUIRE(setup_persist::save_file(t.host.setup_path, default_setup()).accepted);
    t.key(input::scan::kR);
    CHECK(t.session().panels.has(panel::kInfo));
    CHECK(tool->described == 0);
    CHECK(t.doc() == doc_before);
    CHECK(t.session().selected == selected);
}

// ---- The unresolved reference, end to end ------------------------------------------

TEST_CASE("a setup naming a pane this build has never heard of loads, keeps it, and says so") {
    // ONE OF THE PHASE'S PRIMARY ACCEPTANCE PROOFS, and not a future-only unit
    // test: the branch is written and exercised while it is still unreachable by
    // any file this build can produce.
    TempDir dir("setup-unresolved");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    ToolSeat* tool = mount_tool(t, "zengine-snake");

    Setup authored;
    authored.name = "Future";
    REQUIRE(add_pane(authored, ref_of(panel::kInfo)));
    REQUIRE(add_pane(authored, stranger()));
    REQUIRE(add_pane(authored, ref_of(panel::kBuilder)));
    const std::string bytes = setup_persist::to_text(authored);
    spillout(t.host.setup_path, bytes);

    t.key(input::scan::kR);

    // IT LOADED. An unresolved reference is not a load failure.
    CHECK_FALSE(t.session().notice_is_bad);
    CHECK(t.session().setup.active == authored);
    // THE UNKNOWN REFERENCE IS STILL THERE, still between the two that resolved,
    // still byte-for-byte what it was.
    REQUIRE(t.session().setup.active.panes.size() == 3);
    CHECK(t.session().setup.active.panes[1].ref == stranger());

    // The two that resolve are open, in file order relative to each other.
    CHECK(open_kinds(t.session().panels) ==
          std::vector<std::int64_t>{panel::kInfo, panel::kBuilder});

    // AND NOTHING WAS PAINTED ON THE UNKNOWN REFERENCE'S BEHALF. The only kind
    // available to paint an unknown pane with is the Builder, and there is
    // exactly one Builder, in the stack's FIRST slot -- the second slot, where a
    // placeholder would have gone, is empty.
    const Screen sc = screen_of(t.session());
    const PanelBounds builder_at = bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder, sc);
    REQUIRE(builder_at.open);
    CHECK(builder_at.rect == fine_of_cells(placement_bounds(placement::kOverlayStack, 0, sc)));
    CHECK(t.session().panels.open.size() == 2);
    // ...and the slot a placeholder would have taken is not occupied by anything:
    // a hand reaching into it meets the workspace, not a pane painted on behalf
    // of a reference nothing could resolve.
    const ui::Rect second = placement_bounds(placement::kOverlayStack, 1, sc);
    CHECK_FALSE(occupied_at(t.session().panels, t.session().setup.active, sc, second.x + 1, second.y + 1).occupied);

    // The notice says UNRESOLVED and names the reference, and never says
    // unavailable -- Workshop knows it has no catalog row for this, and knows
    // nothing whatever about whoever could present it.
    CHECK(t.notice().find("unresolved") != std::string::npos);
    CHECK(t.notice().find("third.party.tools/history") != std::string::npos);
    CHECK(t.notice().find("unavailable") == std::string::npos);

    // The setup LINE says it too, as a count beside the name.
    CHECK(setup_row(t.canvases.back(), sc).find("1 unresolved") != std::string::npos);

    // NO PROVIDER TRAFFIC WAS CREATED. The one ask that happened is the Builder
    // panel's own, and nothing was sent on the unknown reference's behalf.
    CHECK(tool->described == 1);

    // AND RE-SAVING RETAINS IT EXACTLY. The maker renames the setup and saves;
    // the stranger's entry comes through untouched.
    name_setup(t, "Future kept");
    Setup expected = authored;
    expected.name = "Future kept";
    CHECK(t.session().setup.active == expected);
    CHECK(slurp(t.host.setup_path) == setup_persist::to_text(expected));
    CHECK(slurp(t.host.setup_path).find("third.party.tools") != std::string::npos);

    // ...and it survives a picker gesture on either built-in.
    pick(t, panel::kBuilder);
    CHECK(has_pane(t.session().setup.active, stranger()));
    pick(t, panel::kInfo);
    CHECK(has_pane(t.session().setup.active, stranger()));
    CHECK(t.session().setup.active.panes.front().ref == stranger());
}

// ---- Authored versus resolved -------------------------------------------------------

TEST_CASE("the same setup resolves to different bounds under a different extent") {
    // THE AUTHORED/RESOLVED PROOF FOR WS-0, and its GREEN CONTROL in one case:
    // change only the surface extent and the setup stays SAVED while every
    // rectangle it resolves to moves.
    TempDir dir("setup-extent");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    (void)mount_tool(t, "zengine-snake");

    open_builder(t);
    name_setup(t, "Both");
    REQUIRE(t.session().setup.saved());
    const std::string bytes = slurp(t.host.setup_path);
    const Setup authored = t.session().setup.active;

    const Screen small = screen_of(t.session());
    const ui::Rect info_small =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kInfo, small).rect);
    const ui::Rect builder_small =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder, small).rect);

    t.publish(loom::to_value(surface::SurfaceExtent{140, 44, 0, 0}));

    const Screen large = screen_of(t.session());
    const ui::Rect info_large =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kInfo, large).rect);
    const ui::Rect builder_large =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder, large).rect);

    // THE RESOLVED GEOMETRY MOVED...
    CHECK(large.w != small.w);
    CHECK_FALSE(info_large == info_small);
    CHECK(info_large.x != info_small.x);
    CHECK(builder_large.h == builder_small.h); // the stack's slot is a fixed size...
    CHECK(large.room_w != small.room_w);       // ...and the room around it is not

    // ...AND NOTHING AUTHORED DID. Same value, same references, same bytes, and
    // still saved -- which is the control that makes this a claim about
    // persisted geometry rather than about recomposition.
    CHECK(t.session().setup.active == authored);
    CHECK(t.session().setup.saved());
    CHECK(slurp(t.host.setup_path) == bytes);

    // Restoring under the new extent produces the same intent and the current
    // rectangles, never the ones the file was written under.
    t.key(input::scan::kR);
    CHECK(t.session().setup.active == authored);
    CHECK(cells_covered(
              bounds_of(t.session().panels, t.session().setup.active, panel::kInfo, large)
                  .rect) == info_large);

    // A text metric moves the same picture again, and the setup is untouched.
    t.publish(loom::to_value(surface::SurfaceExtent{140, 44, 8, 18}));
    CHECK(t.session().setup.active == authored);
    CHECK(t.session().setup.saved());
    CHECK(slurp(t.host.setup_path) == bytes);

    // And no RESOLVED rectangle, placement, column, row or metric is in the file at all.
    for (const char* forbidden : {"\"w\"", "\"h\"", "rect", "columns", "rows",
                                  "advance", "extent", "placement", "slot"}) {
        CAPTURE(forbidden);
        CHECK(bytes.find(forbidden) == std::string::npos);
    }
    // THE `x` AND `y` THE FILE DOES CARRY ARE A PLACE NOBODY AUTHORED (WIND-2), and the
    // difference between an authored coordinate and a resolved one is the whole of what
    // this case is about. Every row of this setup is `default` in all three geometry
    // fields, which is the smallest canonical spelling of "no override" -- so the numbers
    // beside those modes are required zeros and could not be a rectangle if they tried.
    CHECK(bytes.find("\"place\":{\"mode\":\"default\",\"x\":\"0\",\"y\":\"0\"}") !=
          std::string::npos);
    CHECK(bytes.find("\"width\":{\"mode\":\"default\",\"amount\":\"0\"}") !=
          std::string::npos);
    // ...and not one of the rectangles this case just measured is anywhere in it.
    for (const std::int64_t n : {info_small.w, info_small.h, info_large.w, info_large.h}) {
        CAPTURE(n);
        CHECK(bytes.find("\"amount\":\"" + std::to_string(n) + "\"") == std::string::npos);
    }
}

// ---- Two processes ------------------------------------------------------------------

TEST_CASE("a maker names a setup, leaves, and gets it back in a fresh Workshop") {
    // THE PRODUCT OUTCOME, deterministically: two independent Workshops, two
    // independent buses, one file between them.
    TempDir dir("setup-two-runs");
    const std::string path = dir.file("setup.json");

    std::string bytes;
    {
        // RUN A: a fresh Workshop opens with Info; the maker opens Builder,
        // removes Info, names the setup and saves.
        Live a;
        a.host.setup_path = path;
        (void)mount_tool(a, "zengine-snake");
        REQUIRE(open_kinds(a.session().panels) == std::vector<std::int64_t>{panel::kInfo});

        open_builder(a);
        pick(a, panel::kInfo);
        name_setup(a, "Morning build");

        REQUIRE(a.session().setup.saved());
        REQUIRE(open_kinds(a.session().panels) == std::vector<std::int64_t>{panel::kBuilder});
        bytes = slurp(path);
        REQUIRE_FALSE(bytes.empty());
    }

    {
        // RUN B: a fresh Workshop begins from its ordinary default, and the
        // maker restores.
        Live b;
        b.host.setup_path = path;
        ToolSeat* tool = mount_tool(b, "zengine-snake");
        REQUIRE(b.session().setup.active == default_setup());
        REQUIRE(open_kinds(b.session().panels) == std::vector<std::int64_t>{panel::kInfo});
        const WorkshopDoc opening_document = b.doc();
        const std::int64_t opening_workspace = b.session().workspace_w;

        b.key(input::scan::kR);

        CHECK(b.session().setup.active.name == "Morning build");
        CHECK(b.session().panels.has(panel::kBuilder));
        CHECK_FALSE(b.session().panels.has(panel::kInfo));
        CHECK(b.session().setup.saved());

        // BUILDER ASKS ITS STILL-INDEPENDENT TOOL FOR CURRENT STATUS -- the
        // tool's own answer, not a copy that rode the file.
        CHECK(tool->described == 1);
        CHECK(b.session().panels.builder.shown.recipe == "zengine-snake");

        // AND NEITHER DOCUMENT CONTENT NOR THE CURRENT SCREEN EXTENT CAME OUT OF
        // THE SETUP. Both are what this run had before the restore.
        CHECK(b.doc() == opening_document);
        CHECK(b.session().workspace_w == opening_workspace);

        // The file is unchanged by having been read.
        CHECK(slurp(path) == bytes);
    }
}

TEST_CASE("saving and restoring a setup does not touch the document or its saved status") {
    TempDir dir("setup-doc-separation");
    Live t;
    t.host.document_path = dir.document();
    t.host.setup_path = dir.file("setup.json");
    (void)mount_tool(t, "zengine-snake");

    // Save the document, then do a whole setup round trip over the top of it.
    t.key(input::scan::kS, input::mod::kCtrl);
    REQUIRE(t.notice().find("saved " + t.host.document_path) == 0);
    const std::string document_bytes = slurp(t.host.document_path);
    const WorkshopDoc document = t.doc();

    open_builder(t);
    name_setup(t, "Build");
    t.key(input::scan::kR);

    CHECK(t.doc() == document);
    CHECK(slurp(t.host.document_path) == document_bytes);
    // The document status line still says the document is saved -- a setup round
    // trip is not a document edit.
    CHECK(t.status_note().find(t.host.document_path + " saved") != std::string::npos);

    // And the two commands stayed apart: `^o` loads the document and leaves the
    // setup exactly where it was.
    const Setup setup_before = t.session().setup.active;
    t.key(input::scan::kO, input::mod::kCtrl);
    CHECK(t.notice().find("loaded " + t.host.document_path) == 0);
    CHECK(t.session().setup.active == setup_before);
    CHECK(t.session().setup.saved());
}

// ---- What a maker reads --------------------------------------------------------------

TEST_CASE("the setup line names the setup, its file, whether it is saved, and its gestures") {
    TempDir dir("setup-line");
    Live t;
    t.host.setup_path = dir.file("s.json");

    const Screen sc = screen_of(t.session());
    const std::string fresh = setup_row(first_frame(t), sc);
    INFO(fresh);
    CHECK(fresh.find(">Default<") == 0); // the live layout tab leads the row (WUX-9, QR-15)
    CHECK(fresh.find("UNSAVED") != std::string::npos);

    name_setup(t, "Named");
    const std::string saved = setup_row(t.canvases.back(), sc);
    CHECK(saved.find(">Named< | saved") == 0);

    // AT THE MINIMUM COMPOSITION WITH THE DEFAULT FILE NAME THE WHOLE LINE FITS,
    // and that is the measurement the ORDER of that line was chosen against: the
    // name, the marker, the file and both gestures, in 78 cells with room over.
    Live plain;
    plain.host.setup_path = kDefaultSetupFileName;
    const std::string minimal = setup_row(first_frame(plain), screen_of(plain.session()));
    INFO(minimal);
    CHECK(minimal.find(">Default< | UNSAVED") == 0);
    CHECK(minimal.find(kDefaultSetupFileName) != std::string::npos);
    CHECK(minimal.find("s name/save") != std::string::npos);
    CHECK(minimal.find("r restore") != std::string::npos);
    CHECK(static_cast<std::int64_t>(minimal.size()) <= kMinScreen.w);
    CHECK(minimal.find("...") == std::string::npos);

    // AND A LINE TOO LONG FOR THE ROOM IS CUT WITH A MARK rather than silently --
    // which is what a canvas label buys that the status slot could not, and the
    // reason the identity is at the front: it is never what elides.
    Live wordy;
    wordy.host.setup_path = std::string(90, 'p');
    const std::string cut = setup_row(first_frame(wordy), screen_of(wordy.session()));
    CHECK(static_cast<std::int64_t>(cut.size()) == kMinScreen.w);
    CHECK(cut.substr(cut.size() - 3) == "...");
    CHECK(cut.find(">Default< | UNSAVED") == 0);

    // A wider surface spends the room it gained on the rest of the sentence,
    // which needs nothing from anybody but room.
    wordy.publish(loom::to_value(surface::SurfaceExtent{160, 40, 0, 0}));
    const std::string roomy = setup_row(wordy.canvases.back(), screen_of(wordy.session()));
    CHECK(roomy.find("...") == std::string::npos);
    CHECK(roomy.find(std::string(90, 'p')) != std::string::npos);
    CHECK(roomy.find("r restore") != std::string::npos);
}

TEST_CASE("the setup line becomes the name editor while a maker is typing") {
    TempDir dir("setup-editor-line");
    Live t;
    t.host.setup_path = dir.file("s.json");

    t.key(input::scan::kS);
    t.text("s");
    const Screen sc = screen_of(t.session());
    const std::string row = setup_row(t.canvases.back(), sc);
    INFO(row);
    CHECK(row.find("setup name> Default") == 0);
    // THE CARET IS IN THE TEXT, at the end where `s` left it. A one-cell-tall
    // bounded region would hold zero rows of a real face (HD-6), so this editor
    // says its caret the way the cell projection says a region's.
    CHECK(row.find(std::string("Default") + surface::kCaretGlyph) != std::string::npos);
    CHECK(row.find("enter saves") != std::string::npos);
    CHECK(row.find("esc cancels") != std::string::npos);
    CHECK(static_cast<std::int64_t>(row.size()) <= sc.w);

    // The caret follows the maker's hand, and a character typed at it lands there.
    t.key(input::scan::kLeft);
    t.key(input::scan::kLeft);
    t.text("X");
    CHECK(t.session().setup.naming.line.text() == "DefauXlt");
    CHECK(setup_row(t.canvases.back(), sc).find(std::string("DefauX") + surface::kCaretGlyph) !=
          std::string::npos);
}

TEST_CASE("the name editor takes the keys, and the picker and the document keep theirs") {
    TempDir dir("setup-modes");
    Live t;
    t.host.setup_path = dir.file("s.json");

    t.key(input::scan::kS);
    t.text("s");
    REQUIRE(t.session().setup.naming.open);

    // `p` DOES NOT OPEN THE PICKER while the editor has the keys: it is a
    // character, and the editor is the mode above command mode.
    t.text("p");
    CHECK_FALSE(t.session().panels.picker.open);
    CHECK(t.session().setup.naming.line.text() == "Defaultp");

    // ...and `q` does not quit.
    t.text("q");
    CHECK_FALSE(t.host.quit);
    CHECK(t.session().setup.naming.line.text() == "Defaultpq");

    // The two commands that mean the same thing in every mode still do: `^s`
    // saves the DOCUMENT, from inside the setup-name editor, and says so.
    t.key(input::scan::kS, input::mod::kCtrl);
    CHECK(t.notice().find("no document file") == 0);

    // The picker and the name editor cannot both be open: `s` is a command, and
    // the picker takes the keys before command mode is reached.
    t.key(input::scan::kEscape);
    REQUIRE_FALSE(t.session().setup.naming.open);
    t.key(input::scan::kP);
    REQUIRE(t.session().panels.picker.open);
    t.key(input::scan::kS);
    t.text("s");
    CHECK_FALSE(t.session().setup.naming.open);
    CHECK(t.session().panels.picker.open);
}

TEST_CASE("the picker's own state never reaches the setup file") {
    TempDir dir("setup-no-picker");
    Live t;
    t.host.setup_path = dir.file("s.json");

    // Leave the picker's cursor somewhere deliberate, then save.
    t.key(input::scan::kP);
    t.key(input::scan::kDown);
    REQUIRE(t.session().panels.picker.cursor == 1);
    t.key(input::scan::kEscape);
    name_setup(t, "Clean");

    const std::string bytes = slurp(t.host.setup_path);
    CHECK(bytes.find("cursor") == std::string::npos);
    CHECK(bytes.find("picker") == std::string::npos);
    CHECK(bytes.find("terminal") == std::string::npos);
    CHECK(bytes.find("selected") == std::string::npos);
    CHECK(bytes.find("notice") == std::string::npos);

    // And restoring opens no picker: `r` is a command, so the picker was closed
    // before it could run, and nothing in a restore opens one.
    t.key(input::scan::kR);
    CHECK_FALSE(t.session().panels.picker.open);
}

// ---- WS-0a: the sentence that quotes a name owns the escaping ------------------
//
// WS-0 accepts `"` and `\` in a setup name and persists those bytes exactly, which is
// the right law and stays the law. What it did not own was the PROSE: three callers
// spelled the quoted token by hand, so a legal name could manufacture the delimiter a
// maker uses to tell an identity from the status beside it. These cases pin the
// spelling, all three callers, the authored bytes on both sides of it, and the unit the
// two existing bounds have always been counted in.

namespace {

/// READ ONE QUOTED TOKEN BACK THE WAY A MAKER'S EYE DOES -- and deliberately written
/// against the RULE rather than against `quoted_setup_name`, so that this is a second,
/// independent implementation and not a mirror of the first. An opening quote, then
/// bytes in which a backslash escapes whatever follows it, and the first UNESCAPED
/// quote ends the name.
///
/// It is what makes "the name cannot terminate its own token" a measurable claim
/// instead of a hopeful one: with the token built by raw interpolation this recovers
/// the wrong name, which is exactly the ambiguity WS-0a exists to close.
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
/// one code point, one character a maker would count.
constexpr const char* kFourByteChar = "\xF0\x9F\x9A\x80";

std::string four_byte_chars(std::size_t count) {
    std::string out;
    for (std::size_t i = 0; i < count; ++i) {
        out += kFourByteChar;
    }
    return out;
}

} // namespace

TEST_CASE("WS-0a: a setup name is spelled into prose as one unambiguous quoted token") {
    // ORDINARY NAMES ARE UNCHANGED, exactly, and this is the control the whole cleanup
    // is measured against: the sentence a maker has been reading since WS-0 must come
    // back byte-for-byte.
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
    // quote as an apostrophe) would give up: two names a maker can tell apart must not
    // present as one.
    CHECK(quoted_setup_name("A\"B") != quoted_setup_name("A\\\"B"));

    // ...and the whole of it, stated once: applying the escape rule in reverse recovers
    // the authored bytes, for every shape this phase is about.
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

        // The raw interpolation this replaced, asked the same question: it is a token a
        // reader recovers the WRONG name from the moment the name carries a quote.
        const QuotedToken naive = read_quoted("\"" + authored + "\"", 0);
        if (authored.find('"') != std::string::npos ||
            authored.find('\\') != std::string::npos) {
            CHECK((!naive.well_formed || naive.name != authored));
        }
    }
}

TEST_CASE("QR-15: a name that could impersonate the setup line is one SPAN on it") {
    TempDir dir("ws0a-status");
    Live t;
    t.host.setup_path = dir.file("s.json");

    // A NAME BUILT TO LIE. Every byte of it is legal under WS-0's law and stays legal:
    // this case is about the sentence, not about the name.
    const std::string authored = "Ops\" UNSAVED | decoy";
    REQUIRE(check_setup_name(authored).accepted);
    name_setup(t, authored);
    REQUIRE(t.session().setup.active.name == authored);
    REQUIRE(t.session().setup.saved());

    const Screen sc = screen_of(t.session());
    const std::string row = setup_row(t.canvases.back(), sc);
    INFO(row);

    // WS-0a ANSWERED THIS WITH A QUOTED TOKEN AND QR-15 SPENT THAT ANSWER. The layout
    // tabs paint the authored bytes bare, so the identity's boundary is no longer a
    // delimiter IN the text -- it is the tab's own recorded extent, written as the row
    // was composed (`LayoutTab::column`/`columns`, HD-3's one geometry).
    const BandStatus band = band_status(t.session(), t.host.setup_path, sc);
    REQUIRE(band.tabs.size() == 1);
    const LayoutTab live = band.tabs.front();
    const std::int64_t ends = live.column + live.columns;

    // THE IDENTITY IS ONE SPAN, and the maker's own bytes are exactly what is inside it,
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

    // ...AND EXACTLY ONE SAVED MARKER, the real one, OUTSIDE the span. The decoy word
    // inside the name is not it, and the proof is positional exactly as it was before --
    // the status begins where the identity's extent ends.
    CHECK(band.text.compare(static_cast<std::size_t>(ends), 9, " | saved ") == 0);
    CHECK(band.text.find("UNSAVED", static_cast<std::size_t>(ends)) == std::string::npos);

    // ⚠ AND THE HALF QR-15 GAVE BACK, PINNED RATHER THAN LEFT TO BE DISCOVERED. To a
    // reader scanning the BYTES alone the decoy is now indistinguishable from a status
    // word: it is on the row, ahead of the real one, and only the span says it is part of
    // a name. That is the price of a bare run and it is the maker's decision, not a
    // defect -- what may never become true again is the MACHINE losing the boundary.
    CHECK(band.text.find("UNSAVED") < static_cast<std::size_t>(ends));
    CHECK(band.text.find(" | ") < static_cast<std::size_t>(ends));

    // The row is still one bounded row of the band, and the file is still named on it.
    CHECK(static_cast<std::int64_t>(row.size()) <= sc.w);
    CHECK(row.compare(0, band.text.size(), band.text) == 0);

    // AND THE AUTHORED BYTES NEVER MOVED. The presentation is prose and reaches neither
    // the live setup nor the copy `saved()` compares against; the FILE still escapes.
    CHECK(t.session().setup.active.name == authored);
    CHECK(t.session().setup.on_file.name == authored);
    CHECK(slurp(t.host.setup_path).find("\\\" UNSAVED") != std::string::npos);
}

TEST_CASE("WS-0a: the save notice and the restore notice spell the name the same way") {
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
    // The PATH is named the ordinary way -- it is not a setup name and gains no
    // escaping from this phase.
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

    // ⚠ AND THE TAB RUN IS NOT ONE OF THEM SINCE QR-15. It paints the AUTHORED bytes, so
    // the escaped spelling is nowhere on the row -- which is a second consumer LEAVING
    // this owner, deliberately, and not a caller improvising a second spelling of it.
    const std::string row = setup_row(t.canvases.back(), screen_of(t.session()));
    INFO(row);
    CHECK(row.find(token) == std::string::npos);
    CHECK(row.compare(0, authored.size() + 2, ">" + authored + "<") == 0);
}

TEST_CASE("WS-0a: the name editor edits the authored bytes, never the escaped spelling") {
    TempDir dir("ws0a-editor");
    Live t;
    t.host.setup_path = dir.file("s.json");

    // TYPED FROM SCRATCH, character by character, exactly as a maker produces it: the
    // quote and the backslash arrive as ordinary text and are stored as themselves.
    const std::string authored = "Ops \"A\\B\"";
    name_setup(t, authored);
    REQUIRE(t.session().setup.active.name == authored);

    // REOPENED ON THE NAME IT ALREADY HAS -- and what the editor holds is the ORIGINAL
    // bytes. A maker does not have to type `\"` to mean `"` in their own name.
    t.key(input::scan::kS);
    t.text("s");
    REQUIRE(t.session().setup.naming.open);
    CHECK(t.session().setup.naming.line.text() == authored);
    CHECK(t.session().setup.naming.line.text()[6] == '\\'); // the raw backslash, stored as one

    const Screen sc = screen_of(t.session());
    const std::string row = setup_row(t.canvases.back(), sc);
    INFO(row);
    // The editing row is not a quoted sentence, so it is not an escaped one either: the
    // prompt, the raw name, the caret where the maker's hand left it, and the hint.
    CHECK(row.find(std::string("setup name> ") + authored + surface::kCaretGlyph) == 0);
    CHECK(row.find(quoted_setup_name(authored)) == std::string::npos);

    // ESCAPE CHANGES NOTHING, and the name is still the authored one.
    t.key(input::scan::kEscape);
    CHECK_FALSE(t.session().setup.naming.open);
    CHECK(t.session().setup.active.name == authored);
    CHECK(t.session().setup.saved());

    // AND ENTER SAVES THE AUTHORED BYTES, not the spelling they are presented with.
    t.key(input::scan::kS);
    t.text("s");
    t.text("!");
    t.key(input::scan::kReturn);
    const std::string grown = authored + "!";
    CHECK(t.session().setup.active.name == grown);
    const setup_persist::LoadedSetup back = setup_persist::load_file(t.host.setup_path);
    REQUIRE(back.outcome.accepted);
    CHECK(back.setup.name == grown);
    CHECK(back.setup.name.find("\\\"") == std::string::npos); // no escape was stored
}

TEST_CASE("WS-0a: an ordinary setup name presents exactly as it did before") {
    // THE GREEN CONTROL FOR THE WHOLE CLEANUP. Not one byte of the sentence a maker
    // without a quote in their name reads may have moved, and every one of these
    // strings is spelled out here rather than composed, so that a change to the
    // presentation owner cannot quietly agree with itself.
    TempDir dir("ws0a-ordinary");
    Live t;
    t.host.setup_path = dir.file("s.json");

    const Screen sc = screen_of(t.session());
    const std::string fresh = setup_row(first_frame(t), sc);
    CHECK(fresh.find(">Default< | UNSAVED") == 0);

    name_setup(t, "Morning build");
    CHECK(t.notice().find("saved setup \"Morning build\" to ") == 0);
    CHECK(setup_row(t.canvases.back(), sc).find(">Morning build< | saved") == 0);

    t.key(input::scan::kR);
    CHECK(t.notice().find("restored setup \"Morning build\" from ") == 0);

    // ...and the name a fresh Workshop carries is spelled the way the constant is.
    CHECK(quoted_setup_name(kDefaultSetupName) == "\"Default\"");
}

TEST_CASE("QR-15: a bare name at the bound is its own length, and the row is still cut") {
    TempDir dir("ws0a-fit");

    // WHAT QR-15 CHANGED HERE. WS-0a's escaping doubled every quote on the way to the
    // row; the bare run spends the authored bytes and nothing else, so twelve quotes are
    // twelve cells rather than twenty-four -- the control that keeps the case below about
    // the BOUND rather than about escaping, now for the opposite reason.
    Live modest;
    modest.host.setup_path = dir.file("m.json");
    name_setup(modest, repeated(12, '"'));
    const std::string easy = setup_row(modest.canvases.back(), screen_of(modest.session()));
    INFO(easy);
    CHECK(easy.compare(0, 14, ">" + repeated(12, '"') + "<") == 0);
    CHECK(easy.find("\\\"") == std::string::npos); // not one escape on the row
    CHECK(easy.find(" saved") != std::string::npos);

    // THE PATHOLOGICAL LEGAL NAME: thirty-two bytes at the bound, every one of them a
    // quote -- thirty-four cells of a seventy-eight cell row now, where the escaped
    // token was sixty-six. It is the file path that carries the row past its extent.
    Live t;
    t.host.setup_path = dir.file("p.json");
    const std::string authored = repeated(kMaxSetupNameLen, '"');
    REQUIRE(check_setup_name(authored).accepted);
    name_setup(t, authored);
    REQUIRE(t.session().setup.active.name == authored);
    REQUIRE(t.session().setup.saved());

    const std::string cut = setup_row(t.canvases.back(), screen_of(t.session()));
    INFO(cut);
    CHECK(static_cast<std::int64_t>(cut.size()) == kMinScreen.w);
    CHECK(cut.substr(cut.size() - 3) == "...");
    // IT CANNOT RUN UNMARKED INTO WHAT COMES AFTER IT. The existing `detail::fit` is
    // the whole of the answer -- the sentence is fitted once, at the presentation
    // boundary, after the token is formed -- so nothing downstream of the identity is
    // shown as though it were complete.
    CHECK(cut.find("s name/save") == std::string::npos);
    CHECK(cut.find(t.host.setup_path) == std::string::npos);

    // THE CUT IS PRESENTATION AND NOTHING ELSE: the name, the saved comparison and the
    // file are all untouched by it, and a wider surface spends the room on the rest of
    // the same sentence.
    CHECK(t.session().setup.active.name == authored);
    CHECK(t.session().setup.saved());
    CHECK(setup_persist::load_file(t.host.setup_path).setup.name == authored);

    t.publish(loom::to_value(surface::SurfaceExtent{240, 40, 0, 0}));
    const std::string roomy = setup_row(t.canvases.back(), screen_of(t.session()));
    INFO(roomy);
    CHECK(roomy.find("...") == std::string::npos);
    CHECK(roomy.compare(0, authored.size() + 2, ">" + authored + "<") == 0);
    CHECK(roomy.find("s name/save") != std::string::npos);
    // ...and the extent changed what fits, never the setup or its saved status.
    CHECK(t.session().setup.active.name == authored);
    CHECK(t.session().setup.saved());
}

TEST_CASE("WS-0a: a name carrying a quote and a backslash survives its file exactly") {
    TempDir dir("ws0a-persist");
    const std::string authored = "Ops \"A\\B\"";
    const Setup s = setup_of(authored, {panel::kInfo, panel::kBuilder});
    REQUIRE(check_setup(s).accepted);

    // THE FORMAT WORD IS UNCHANGED AND THE VERSION IS NOT (WIND-2, then WUX-2). WS-0a's
    // claim was about the NAME's bytes surviving the file, and that claim is
    // version-independent: what this case witnesses is that an authored name carrying the
    // two bytes the quoting owner escapes comes back exactly as it went in, which is as
    // true of version 3 as it was of version 1. The format WORD did not move, because
    // handing Workshop the wrong one of its own two files is still named rather than
    // half-read.
    CHECK(setup_persist::kFormatVersion == 3);
    CHECK(std::string(setup_persist::kFormat) == "zengine-workshop-setup");

    const std::string path = dir.file("q.json");
    REQUIRE(setup_persist::save_file(path, s).accepted);
    const std::string a = slurp(path);
    INFO(a);
    CHECK(a.find("\"format\":\"zengine-workshop-setup\"") != std::string::npos);
    CHECK(a.find("\"format_version\":\"3\"") != std::string::npos);

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

TEST_CASE("WS-0a: the name and key bounds are BYTES, and the refusal says bytes") {
    // THE BOUNDS THEMSELVES ARE UNCHANGED. WS-0a corrects a false unit in a sentence
    // and nothing about what is accepted.
    CHECK(kMaxSetupNameLen == 32);
    CHECK(kMaxPaneKeyLen == 64);

    // EIGHT FOUR-BYTE CHARACTERS ARE THIRTY-TWO BYTES -- eight characters a maker
    // counts, and exactly the bound `std::string::size()` measures.
    const std::string at_bound = four_byte_chars(8);
    REQUIRE(at_bound.size() == kMaxSetupNameLen);
    CHECK(check_setup_name(at_bound).accepted);

    // ...AND ONE MORE BYTE IS REFUSED, saying the unit it was refused in.
    const Written over = check_setup_name(at_bound + "x");
    CHECK_FALSE(over.accepted);
    CHECK(over.refusal == "a setup name is at most 32 bytes");

    // THE SHARPEST ILLUSTRATION OF THE UNIT: nine characters, thirty-six bytes, refused
    // -- so a refusal saying "at most 32 characters" would be a false sentence about a
    // true refusal. That is the whole of what this correction is.
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
// WUX-0 — GIVE ME MY DESK BACK
// =============================================================================
//
// ONE PRODUCT SENTENCE, PINNED FROM BOTH ENDS: close Workshop after arranging it
// into a useful desk, reopen it, and get that desk back -- the panes, where they
// were put, how big they were made, which was in front, and how much room the
// surface had -- with no gesture in between.
//
// WHAT THESE CASES DO NOT RE-PROVE. Pane presence, ordering, authored place and
// authored size already persisted before this phase and are pinned above, at
// length, against the SETUP file. Nothing here re-tests that machinery; what it
// tests is the two things that did not exist -- something that READS it without
// being asked, and anything at all that remembers the room.

namespace {

/// A desk worth wanting back: two panes, one of them moved and resized by hand.
Setup arranged_desk(const char* name) {
    Setup s = setup_of(name, {panel::kInfo, panel::kBuilder});
    REQUIRE(author_pane_place(s, ref_of(panel::kBuilder), subs(6), subs(5)).accepted);
    REQUIRE(author_pane_size(s, ref_of(panel::kBuilder), PaneSize{pane_unit::kSubcells, subs(40)},
                             PaneSize{pane_unit::kSubcells, subs(12)})
                .accepted);
    return s;
}

/// A Workshop that has been arranged and closed the way a maker closes one: its
/// surface says hello, the medium reports its room, the maker restores a desk
/// from their named setup, and then they quit.
///
/// IT GOES THROUGH THE PRODUCTION DOORS AND NOWHERE ELSE -- `r` to restore, `q`
/// to leave -- so what lands in the session file is what a maker's own session
/// would leave there, not what a fixture reached in and assigned.
void arrange_and_close(const std::string& session_path, const std::string& setup_path,
                       const Setup& desk, std::int64_t width, std::int64_t height) {
    Live t;
    t.host.session_path = session_path;
    t.host.setup_path = setup_path;
    REQUIRE(setup_persist::save_file(setup_path, desk).accepted);
    t.publish(loom::to_value(surface::SurfaceReady{}));
    t.publish(loom::to_value(surface::SurfaceExtent{width, height}));
    t.key(input::scan::kR);
    REQUIRE(t.session().setup.active == desk);
    REQUIRE(t.session().screen_w == width);
    REQUIRE(t.session().screen_h == height);
    t.key(input::scan::kQ);
    REQUIRE(t.host.quit);
}

} // namespace

// ---- Witness A: the primary one ---------------------------------------------

TEST_CASE("WUX-0 A: the desk and the room come back, with no gesture at all") {
    TempDir dir("wux0-a");
    const std::string session = dir.file("session.json");
    const Setup desk = arranged_desk("Debugging");
    arrange_and_close(session, dir.file("setup.json"), desk, 120, 44);
    REQUIRE(std::filesystem::exists(session));

    // ---- and the maker opens Workshop again ------------------------------
    //
    // A DIFFERENT SETUP PATH ON PURPOSE. Nothing about taking the last session back
    // may depend on the named-setup file still being where it was, or on it being
    // readable, or on it existing at all.
    Live back;
    back.host.session_path = session;
    back.host.setup_path = dir.file("somewhere-else.json");
    REQUIRE(back.canvases.empty());
    back.publish(loom::to_value(surface::SurfaceReady{}));

    // THE DESK, whole: the same panes, the same authored geometry, the same order.
    CHECK(back.session().setup.active == desk);
    CHECK(back.session().panels.has(panel::kInfo));
    CHECK(back.session().panels.has(panel::kBuilder));
    // THE ROOM.
    CHECK(back.session().screen_w == 120);
    CHECK(back.session().screen_h == 44);
    // AND NOT ONE KEY WAS PRESSED. `r` is still there and still does what it did;
    // this is the run in which nobody had to know that.
    CHECK_FALSE(back.session().notice_is_bad);
    CHECK(back.notice().find("reopened your last desk") == 0);
    CHECK(back.notice().find("\"Debugging\"") != std::string::npos);
    CHECK(back.notice().find("120x44") != std::string::npos);
}

TEST_CASE("WUX-0: the FIRST picture of a run is the floor, and the room is the second") {
    // ⭐ THE INVARIANT THE WINDOW'S MINIMUM RESTS ON, and the reason the restore does
    // not simply seed the extent before the first paint. A medium that has been told
    // nothing has only a run's first picture to size itself from, and a graphical one
    // makes that picture's size the smallest the window may ever be dragged to. Ask
    // for the remembered room FIRST and a maker can never shrink their Workshop
    // again; ask for the floor first and the remembered room is an ordinary later
    // picture, which a medium is free to grow to and free to be dragged back from.
    TempDir dir("wux0-floor");
    const std::string session = dir.file("session.json");
    arrange_and_close(session, dir.file("setup.json"), arranged_desk("Wide"), 132, 48);

    Live back;
    back.host.session_path = session;
    back.publish(loom::to_value(surface::SurfaceReady{}));

    REQUIRE(back.canvases.size() >= 2);
    CHECK(back.canvases.front().width == kScreenMinW);
    CHECK(back.canvases.front().height == kScreenMinH);
    CHECK(back.canvases.back().width == 132);
    CHECK(back.canvases.back().height == 48);
}

TEST_CASE("WUX-0: the room is taken back only ONCE, however often a surface says hello") {
    // A Skin replacement announces itself again, and an afternoon of arranging must
    // not be thrown back to a file written last night.
    TempDir dir("wux0-once");
    const std::string session = dir.file("session.json");
    arrange_and_close(session, dir.file("setup.json"), arranged_desk("First"), 110, 40);

    Live back;
    back.host.session_path = session;
    back.publish(loom::to_value(surface::SurfaceReady{}));
    REQUIRE(back.session().setup.active.name == "First");

    // the maker changes their mind about the desk, and a Skin is replaced under them
    pick(back, panel::kBuilder); // remove the Builder the restored desk brought
    const Setup after = back.session().setup.active;
    REQUIRE_FALSE(after == arranged_desk("First"));
    back.publish(loom::to_value(surface::SurfaceReady{}));
    CHECK(back.session().setup.active == after);
}

// ---- Witness B: the second generation replaces the first ---------------------

TEST_CASE("WUX-0 B: the second session replaces the first, room and desk both") {
    TempDir dir("wux0-b");
    const std::string session = dir.file("session.json");
    arrange_and_close(session, dir.file("first-setup.json"), arranged_desk("First"), 100, 36);
    const std::string first_bytes = slurp(session);

    // ---- reopen, change both, close again --------------------------------
    Setup second = setup_of("Second", {panel::kInfo});
    REQUIRE(author_pane_place(second, ref_of(panel::kInfo), 2, 3).accepted);
    {
        Live t;
        t.host.session_path = session;
        t.host.setup_path = dir.file("second-setup.json");
        t.publish(loom::to_value(surface::SurfaceReady{}));
        REQUIRE(t.session().setup.active.name == "First");
        REQUIRE(t.session().screen_w == 100);
        REQUIRE(setup_persist::save_file(t.host.setup_path, second).accepted);
        t.publish(loom::to_value(surface::SurfaceExtent{140, 50}));
        t.key(input::scan::kR);
        REQUIRE(t.session().setup.active == second);
        t.close_requested(); // the close BOX, and it is the same door `q` is
        REQUIRE(t.host.quit);
    }
    // ⭐ THE DEFECT THIS PREVENTS: a startup that reads the file correctly while
    // shutdown keeps rewriting an old cached representation of it.
    CHECK(slurp(session) != first_bytes);

    Live back;
    back.host.session_path = session;
    back.publish(loom::to_value(surface::SurfaceReady{}));
    CHECK(back.session().setup.active == second);
    CHECK(back.session().screen_w == 140);
    CHECK(back.session().screen_h == 50);
    CHECK_FALSE(back.session().panels.has(panel::kBuilder));
}

// ---- Witness C: there is no previous session ---------------------------------

TEST_CASE("WUX-0 C: a first launch is not an error, and needs no file to exist") {
    TempDir dir("wux0-c");
    Live t;
    t.host.session_path = dir.file("never-written.json");
    t.publish(loom::to_value(surface::SurfaceReady{}));

    CHECK(t.session().setup.active == default_setup());
    CHECK(t.session().screen_w == kScreenMinW);
    CHECK(t.session().screen_h == kScreenMinH);
    // AND NOTHING WAS SAID ABOUT IT. The most common way startup ends is the one a
    // maker must never see a complaint about.
    CHECK_FALSE(t.session().notice_is_bad);
    CHECK(t.notice().find("session") == std::string::npos);
    // Nor was a file conjured to fill the absence.
    CHECK_FALSE(std::filesystem::exists(t.host.session_path));
}

TEST_CASE("WUX-0 C: a host that chose no session file restores nothing and writes nothing") {
    Live t;
    REQUIRE(t.host.session_path.empty());
    t.publish(loom::to_value(surface::SurfaceReady{}));
    CHECK(t.session().setup.active == default_setup());
    CHECK_FALSE(t.session().notice_is_bad);
    t.key(input::scan::kQ);
    CHECK(t.host.quit);
    CHECK_FALSE(t.session().notice_is_bad); // and quitting complained about nothing
}

// ---- Witness D: the file is there and cannot be understood -------------------

TEST_CASE("WUX-0 D: a malformed session costs the desk and nothing else") {
    const std::vector<std::pair<const char*, std::string>> cases = {
        {"not a document at all", "{"},
        {"a document that is not a session", persist::to_text(WorkshopDoc{})},
        {"a SETUP file handed to the session reader",
         setup_persist::to_text(arranged_desk("Debugging"))},
        {"a session of another version",
         [] {
             std::string text = session_persist::to_text(arranged_desk("D"), 100, 30,
                                                         session_persist::Placement{});
             const std::size_t at = text.find("\"version\":3");
             REQUIRE(at != std::string::npos);
             text.replace(at, std::string("\"version\":3").size(), "\"version\":7");
             return text;
         }()},
        {"a session whose desk is not a legal setup",
         [] {
             std::string text = session_persist::to_text(arranged_desk("D"), 100, 30,
                                                         session_persist::Placement{});
             const std::size_t at = text.find("\"pane\":\"builder\"");
             REQUIRE(at != std::string::npos);
             text.replace(at, std::string("\"pane\":\"builder\"").size(),
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
        t.publish(loom::to_value(surface::SurfaceReady{}));

        // IT DID NOT CRASH, IT SAID SO, AND IT OPENED.
        CHECK(t.session().notice_is_bad);
        CHECK_FALSE(t.notice().empty());
        CHECK(t.notice().find("opening with the default setup") != std::string::npos);
        CHECK(t.session().setup.active == default_setup());
        CHECK(t.session().screen_w == kScreenMinW);
        CHECK(t.session().screen_h == kScreenMinH);
        // AND THE MAKER'S FILE IS EXACTLY AS THEY LEFT IT. Workshop does not rewrite
        // a file it could not read.
        CHECK(slurp(t.host.session_path) == bytes);
    }
}

TEST_CASE("WUX-0 D/MIG-0: an unreadable session names its version by NUMBER") {
    TempDir dir("wux0-d-version");
    std::string text = session_persist::to_text(arranged_desk("D"), 100, 30,
                                                session_persist::Placement{});
    const std::size_t at = text.find("\"version\":3");
    REQUIRE(at != std::string::npos);
    text.replace(at, std::string("\"version\":3").size(), "\"version\":7");
    const session_persist::LoadedSession refused = session_persist::from_text(text);
    CHECK(refused.present);
    CHECK_FALSE(refused.outcome.accepted);
    // ⭐ THE NUMBER IS STILL THE FIRST THING SAID, and what follows it is now the honest
    // reason rather than a list this file would have to keep in step with history: no
    // conversion from that version to this one is live. The identity of the missing power is
    // named, because it is a fact this host knows and a maker can look for.
    CHECK(refused.outcome.refusal ==
          "session version 7 cannot be read: no live conversion from `WorkshopSession` v7 to "
          "v3 (`zengine.migrate.WorkshopSession.v7-to-v3`)");
    // AND IT CLAIMS NOTHING IT CANNOT KNOW: not that a converter exists on disk, not that
    // one should be installed. There is no unloaded discovery in this system to be honest
    // about, so the sentence does not pretend there is.
    CHECK(refused.outcome.refusal.find("install") == std::string::npos);
    CHECK(refused.outcome.refusal.find("disk") == std::string::npos);
}

TEST_CASE("MIG-0: a version-3 file whose own field says otherwise is a forgery, not history") {
    // TWO DIFFERENT FACTS, TWO DIFFERENT SENTENCES. A file whose ENVELOPE claims another
    // version is old and is answered by the conversion seam; a file whose envelope claims
    // THIS version over a body that says another is inconsistent with itself, and only a
    // forgery produces one. Before MIG-0 both said the same thing.
    std::string text = session_persist::to_text(arranged_desk("D"), 100, 30,
                                                session_persist::Placement{});
    const std::size_t at = text.find("\"format_version\":\"7\"");
    REQUIRE(at == std::string::npos);
    const std::size_t field = text.find("\"format_version\":\"3\"");
    REQUIRE(field != std::string::npos);
    text.replace(field, std::string("\"format_version\":\"3\"").size(),
                 "\"format_version\":\"7\"");

    op::Catalog conversions;
    REQUIRE(conversions.mount("suite", session_history::conversions()));
    const session_persist::LoadedSession refused =
        session_persist::from_text(text, &conversions);
    CHECK_FALSE(refused.outcome.accepted);
    CHECK(refused.outcome.refusal ==
          "this session claims version 3 and its own format_version field says 7");
    // ...and it did not become a conversion request on the way past.
    CHECK(refused.outcome.refusal.find("conversion") == std::string::npos);
}

// ---- Witness E: a viewport this Workshop will not open at --------------------

TEST_CASE("WUX-0 E: a hostile room is declined, and the desk still comes back") {
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
        {"a room larger than this Workshop is honest at", 120, kScreenMaxH + 1},
        {"an enormous room", 100000, 100000},
    };
    const Setup desk = arranged_desk("Debugging");

    for (const Case& c : cases) {
        CAPTURE(c.what);
        TempDir dir("wux0-e");
        Live t;
        t.host.session_path = dir.file("session.json");
        REQUIRE(session_persist::save_file(t.host.session_path, desk, c.w, c.h,
                                           session_persist::Placement{})
                    .accepted);
        t.publish(loom::to_value(surface::SurfaceReady{}));

        // THE DESK CAME BACK. Throwing away a good desk over a bad number would be
        // the corrupt-save-makes-Workshop-useless failure, committed by the code
        // meant to prevent it.
        CHECK(t.session().setup.active == desk);
        // THE ROOM DID NOT, AND IT WAS NOT CLAMPED INTO ONE EITHER: Workshop opens at
        // its floor, exactly as a first launch does.
        CHECK(t.session().screen_w == kScreenMinW);
        CHECK(t.session().screen_h == kScreenMinH);
        CHECK(t.canvases.back().width == kScreenMinW);
        // AND IT NEVER CLAIMS THE SIZE CAME BACK. The value that was declined is
        // named, because a maker looking at their own file can act on it.
        CHECK(t.session().notice_is_bad);
        CHECK(t.notice().find("is not one this Workshop opens at") != std::string::npos);
        CHECK(t.notice().find(std::to_string(c.w) + "x" + std::to_string(c.h)) !=
              std::string::npos);
    }
}

TEST_CASE("WUX-0 E: the band a room is honoured in is the one the screen is honest at") {
    CHECK(session_persist::viewport_honoured(kScreenMinW, kScreenMinH));
    CHECK(session_persist::viewport_honoured(kScreenMaxW, kScreenMaxH));
    CHECK_FALSE(session_persist::viewport_honoured(kScreenMinW - 1, kScreenMinH));
    CHECK_FALSE(session_persist::viewport_honoured(kScreenMinW, kScreenMinH - 1));
    CHECK_FALSE(session_persist::viewport_honoured(kScreenMaxW + 1, kScreenMaxH));
    CHECK_FALSE(session_persist::viewport_honoured(kScreenMinW, kScreenMaxH + 1));
    CHECK_FALSE(session_persist::viewport_honoured(0, 0));
}

// ---- Witness F: named setups are a different promise and stay one ------------

TEST_CASE("WUX-0 F: an automatic save never touches the file a maker named") {
    TempDir dir("wux0-f-save");
    Live t;
    t.host.session_path = dir.file("session.json");
    t.host.setup_path = dir.file("setup.json");
    const Setup named = arranged_desk("Debugging");
    REQUIRE(setup_persist::save_file(t.host.setup_path, named).accepted);
    const std::string setup_bytes = slurp(t.host.setup_path);

    t.publish(loom::to_value(surface::SurfaceReady{}));
    t.publish(loom::to_value(surface::SurfaceExtent{120, 44}));
    pick(t, panel::kBuilder); // arrange something the named setup does not have
    t.key(input::scan::kQ);

    REQUIRE(std::filesystem::exists(t.host.session_path));
    // ⭐ THE PROPERTY THE TWO FILES EXIST FOR: quitting wrote a session and left the
    // maker's named desk byte-for-byte alone.
    CHECK(slurp(t.host.setup_path) == setup_bytes);
    CHECK(setup_persist::load_file(t.host.setup_path).setup == named);
}

TEST_CASE("WUX-0 F: a restored session never touches the file a maker named, either") {
    TempDir dir("wux0-f-restore");
    const std::string session = dir.file("session.json");
    const std::string setup = dir.file("setup.json");
    const Setup named = arranged_desk("Debugging");
    REQUIRE(setup_persist::save_file(setup, named).accepted);
    const std::string setup_bytes = slurp(setup);
    REQUIRE(session_persist::save_file(session, setup_of("Loose", {panel::kInfo}), 110, 38,
                                       session_persist::Placement{})
                .accepted);

    Live t;
    t.host.session_path = session;
    t.host.setup_path = setup;
    t.publish(loom::to_value(surface::SurfaceReady{}));
    REQUIRE(t.session().setup.active.name == "Loose");
    CHECK(slurp(setup) == setup_bytes);

    // ...AND BOTH SETUP GESTURES STILL DO EXACTLY WHAT THEY DID. `r` reads the named
    // file over the restored session; `s` writes the named file and nothing else.
    t.key(input::scan::kR);
    CHECK(t.session().setup.active == named);
    CHECK(t.session().setup.saved());
    const std::string session_bytes = slurp(session);
    name_setup(t, "Renamed");
    CHECK(setup_persist::load_file(setup).setup.name == "Renamed");
    CHECK(slurp(session) == session_bytes); // naming a setup wrote no session
}

TEST_CASE("WUX-0 F: the three files are three formats, and each refuses the others") {
    const Setup desk = arranged_desk("Debugging");
    const std::string doc_text = persist::to_text(WorkshopDoc{});
    const std::string setup_text = setup_persist::to_text(desk);
    const std::string session_text = session_persist::to_text(desk, 120, 44, session_persist::Placement{});

    CHECK(std::string(session_persist::kFormat) == "zengine-workshop-session");
    CHECK(std::string(session_persist::kFormat) != std::string(setup_persist::kFormat));
    CHECK(std::string(session_persist::kFormat) != std::string(persist::kFormat));

    CHECK_FALSE(session_persist::from_text(doc_text).outcome.accepted);
    CHECK_FALSE(session_persist::from_text(setup_text).outcome.accepted);
    CHECK_FALSE(setup_persist::from_text(session_text).outcome.accepted);
    // A session handed to the setup reader is refused, and NOT half-read.
    CHECK(setup_persist::from_text(session_text).setup.panes.empty());
}

// ---- The format itself -------------------------------------------------------

TEST_CASE("WUX-0: a session round-trips, and a second save is byte-identical") {
    const Setup desk = arranged_desk("Debugging");
    const std::string first = session_persist::to_text(desk, 120, 44, session_persist::Placement{});
    const session_persist::LoadedSession read = session_persist::from_text(first);
    REQUIRE(read.outcome.accepted);
    CHECK(read.present);
    CHECK(read.honoured);
    CHECK(read.declined.empty());
    CHECK(read.desk == desk);
    CHECK(read.viewport_w == 120);
    CHECK(read.viewport_h == 44);
    CHECK(session_persist::to_text(read.desk, read.viewport_w, read.viewport_h,
                                   read.placement) == first);
}

TEST_CASE("WUX-0: a session file holds the desk and the room, and nothing runtime") {
    const std::string text = session_persist::to_text(arranged_desk("Debugging"), 120, 44,
                                                      session_persist::Placement{});
    // THE DESK IS THE SETUP'S OWN REPRESENTATION, not a paraphrase of it: every pane
    // row a setup file would have written is in here, spelled the same way.
    for (const char* fragment : {"\"provider\":\"zengine.workshop\"", "\"pane\":\"builder\"",
                                 "\"pane\":\"info\"", "\"mode\":\"subcells\"", "\"front\":",
                                 "\"format\":\"zengine-workshop-setup\""}) {
        CHECK_MESSAGE(text.find(fragment) != std::string::npos, fragment);
    }
    CHECK(text.find("\"viewport\"") != std::string::npos);
    // AND NOTHING THAT BELONGS TO A RUNNING PROCESS. No WeaveId, no runtime pane
    // handle, no catalog row, no loaded artifact, no selection, no drag.
    for (const char* forbidden : {"weave", "WeaveId", "kind", "runtime", "catalog", "selected",
                                  "cursor", "drag", "notice", "document", "text_advance",
                                  "text_line"}) {
        CHECK_MESSAGE(text.find(forbidden) == std::string::npos, forbidden);
    }
}

TEST_CASE("WUX-0: a session too large to be one is refused before it is read") {
    TempDir dir("wux0-big");
    const std::string path = dir.file("session.json");
    spillout(path, std::string(session_persist::kMaxSessionBytes + 1, 'x'));
    const session_persist::LoadedSession refused = session_persist::load_file(path);
    CHECK(refused.present);
    CHECK_FALSE(refused.outcome.accepted);
    CHECK(refused.outcome.refusal.find("a Workshop session can be") != std::string::npos);
}

TEST_CASE("WUX-0: a write that fails leaves the last good session where it was") {
    TempDir dir("wux0-write");
    const std::string path = dir.file("session.json");
    const Setup first = arranged_desk("First");
    REQUIRE(session_persist::save_file(path, first, 120, 44, session_persist::Placement{})
                .accepted);
    const std::string good = slurp(path);

    // The sibling the writer needs is occupied by a DIRECTORY, so the candidate
    // cannot be written -- and the destination is never opened.
    std::filesystem::create_directories(persist::pending_path(path));
    const Written refused = session_persist::save_file(path, setup_of("Second", {panel::kInfo}),
                                                       90, 30, session_persist::Placement{});
    CHECK_FALSE(refused.accepted);
    CHECK(slurp(path) == good);
    std::filesystem::remove_all(persist::pending_path(path));
    CHECK(session_persist::save_file(path, setup_of("Second", {panel::kInfo}), 90, 30,
                                     session_persist::Placement{})
              .accepted);
}

// ---- The ORDER: the room, and then the desk into it --------------------------

TEST_CASE("WUX-0: the desk is seated against the RESTORED room, not the default one") {
    // ⭐ THE ORDERING WITNESS, and the canary for it. Seating spends overlay slots,
    // and how many there are is a fact about the screen: the floor composition has
    // exactly one, and a restored room has more. Reconcile first and resize after,
    // and a maker's second pane is left waiting for room that was in fact already
    // theirs.
    TempDir dir("wux0-order");
    const std::string session = dir.file("session.json");
    Setup two = setup_of("Two", {panel::kBuilder});
    REQUIRE(add_pane(two, hello_ref()));
    REQUIRE(session_persist::save_file(session, two, 120, 60, session_persist::Placement{})
                .accepted);

    REQUIRE(stack_slots_that_fit(kMinScreen) == 1);
    REQUIRE(stack_slots_that_fit(screen_of(120, 60)) >= 2);

    PaneRig r;
    r.host.session_path = session;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    // The provider announces itself before the surface does, so the reference in the
    // session resolves at the moment the desk is applied -- the load order WP-0 says
    // must work either way round.
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });

    r.ready();
    CHECK(r.w->session().screen_w == 120);
    CHECK(r.w->session().screen_h == 60);
    CHECK(r.w->session().panels.open.size() == 2);
    CHECK(r.w->session().panels.has(panel::kBuilder));
    CHECK(unresolved_panes(r.w->session().setup.active, r.w->session().panels.runtime).empty());
}

TEST_CASE("WUX-0: the startup notice counts no pane nobody has had a turn to offer") {
    // MEASURED ON A REAL WINDOW FIRST, and the notice was misleading: at the instant a
    // restored desk is applied, Workshop has published `PaneCatalogRequested` and the answers
    // are still in the queue, so EVERY external reference in it is unresolved right now and
    // resolved a moment later. A count here is a fact about the clock.
    TempDir dir("wux0-unresolved");
    const std::string session = dir.file("session.json");
    Setup mixed = setup_of("Mixed", {panel::kInfo});
    REQUIRE(add_pane(mixed, hello_ref()));
    REQUIRE(session_persist::save_file(session, mixed, 110, 40, session_persist::Placement{})
                .accepted);

    Live t;
    t.host.session_path = session;
    t.publish(loom::to_value(surface::SurfaceReady{}));

    REQUIRE(t.session().setup.active == mixed);
    CHECK(t.notice().find("reopened your last desk") == 0);
    CHECK(t.notice().find("unresolved") == std::string::npos);
    CHECK_FALSE(t.session().notice_is_bad);

    // ...AND THE COUNT IS NOT LOST, it is simply owned by the surface that recomputes it.
    // The setup line carries it live, off the same `unresolved_panes` call, every paint.
    REQUIRE_FALSE(t.canvases.empty());
    CHECK(setup_row(t.canvases.back(), screen_of(t.session())).find("1 unresolved") !=
          std::string::npos);
}

TEST_CASE("WUX-0: `r` keeps its unresolved note -- a maker asking is asking later") {
    TempDir dir("wux0-r-note");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    Setup mixed = setup_of("Mixed", {panel::kInfo});
    REQUIRE(add_pane(mixed, hello_ref()));
    REQUIRE(setup_persist::save_file(t.host.setup_path, mixed).accepted);
    t.publish(loom::to_value(surface::SurfaceReady{}));
    t.key(input::scan::kR);
    CHECK(t.notice().find("1 pane unresolved") != std::string::npos);
}

// ========================================================================================
// WUX-3 — the installed application: per-user roots, explicit isolation, the one-time
// legacy import, the prefs file, and the window's desktop placement remembered, offered
// back, and judged by the medium that can see displays.
// ========================================================================================

// ---- The roots and the precedence (user_paths.hpp) -----------------------------------

TEST_CASE("WUX-3: the two Windows roots are the platform's own conventions") {
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

TEST_CASE("WUX-3: the two XDG roots, and their home fallbacks") {
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

TEST_CASE("WUX-3: one precedence -- explicit path, then isolation, then the default") {
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

TEST_CASE("WUX-3: a legacy-only file is imported once, and the original is left in place") {
    TempDir dir("wux3-import");
    const std::string legacy = dir.file("workshop-session.json");
    const std::string dest = dir.file("root/workshop-session.json");
    spillout(legacy, "the maker's bytes");

    // The destination's parent does not exist yet: the import creates it (first write).
    const user_paths::LegacyImport did =
        user_paths::import_legacy_file(dest, legacy, "session");
    CHECK(did.imported);
    CHECK_FALSE(did.shadowed);
    CHECK(slurp(dest) == "the maker's bytes");
    // NEVER DELETED, NEVER REWRITTEN: the original stands byte-for-byte.
    CHECK(slurp(legacy) == "the maker's bytes");
    // The note says what happened, naming both paths.
    CHECK(did.note.find("imported") != std::string::npos);
    CHECK(did.note.find(legacy) != std::string::npos);
    CHECK(did.note.find(dest) != std::string::npos);
    CHECK(did.note.find("left in place") != std::string::npos);
}

TEST_CASE("WUX-3: an existing user-root file always wins over a legacy file") {
    TempDir dir("wux3-conflict");
    const std::string legacy = dir.file("workshop-keymap.json");
    const std::string dest = dir.file("root/workshop-keymap.json");
    std::filesystem::create_directories(dir.file("root"));
    spillout(dest, "the user root's newer truth");
    spillout(legacy, "an older local file");

    const user_paths::LegacyImport did =
        user_paths::import_legacy_file(dest, legacy, "keymap");
    CHECK_FALSE(did.imported);
    CHECK(did.shadowed);
    // NOTHING MOVED, in either direction.
    CHECK(slurp(dest) == "the user root's newer truth");
    CHECK(slurp(legacy) == "an older local file");
    // ...and the maker is told which file is being read and how to end the note.
    CHECK(did.note.find(dest) != std::string::npos);
    CHECK(did.note.find("not read") != std::string::npos);
    CHECK(did.note.find("delete it") != std::string::npos);
}

TEST_CASE("WUX-3: repeated launches converge -- the import can never fire twice") {
    TempDir dir("wux3-repeat");
    const std::string legacy = dir.file("workshop-session.json");
    const std::string dest = dir.file("root/workshop-session.json");
    spillout(legacy, "first bytes");
    REQUIRE(user_paths::import_legacy_file(dest, legacy, "session").imported);

    // The legacy file CHANGES afterwards -- a maker still running an old build from this
    // directory -- and the user root must not be overwritten by it on any later launch.
    spillout(legacy, "second bytes the root must never take");
    const user_paths::LegacyImport again =
        user_paths::import_legacy_file(dest, legacy, "session");
    CHECK_FALSE(again.imported);
    CHECK(again.shadowed);
    CHECK(slurp(dest) == "first bytes");
    const user_paths::LegacyImport third =
        user_paths::import_legacy_file(dest, legacy, "session");
    CHECK_FALSE(third.imported);
    CHECK(slurp(dest) == "first bytes");
}

TEST_CASE("WUX-3: no legacy file, no destination -- the import does nothing, silently") {
    TempDir dir("wux3-nothing");
    const user_paths::LegacyImport did = user_paths::import_legacy_file(
        dir.file("root/workshop-session.json"), dir.file("workshop-session.json"), "session");
    CHECK_FALSE(did.imported);
    CHECK_FALSE(did.shadowed);
    CHECK(did.note.empty());
    CHECK_FALSE(std::filesystem::exists(dir.file("root")));
}

TEST_CASE("WUX-3: the host resolves the maker's files through the one precedence") {
    // A SOURCE TRIPWIRE, the host tier's own instrument: main() must reach every per-user
    // default through `user_paths::resolve_durable_path` -- one spelling of the precedence,
    // pinned above -- and must spell the isolation affordance. A host that reverted to a
    // bare CWD name would pass every weave case and silently re-scatter the maker's files.
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
    // The two project files stay project files: their defaults are still the bare names.
    CHECK(host.find("persist::kDefaultDocumentName") != std::string::npos);
    CHECK(host.find("kDefaultSetupFileName") != std::string::npos);
}

// ---- The prefs file ------------------------------------------------------------------

TEST_CASE("WUX-3: prefs round-trip, and the words are a closed set") {
    const std::string hidden = prefs_persist::to_text(false);
    CHECK(hidden.find("\"format\":\"zengine-workshop-prefs\"") != std::string::npos);
    CHECK(hidden.find("\"titles\":\"hidden\"") != std::string::npos);
    prefs_persist::LoadedPrefs read = prefs_persist::from_text(hidden);
    REQUIRE(read.outcome.accepted);
    CHECK_FALSE(read.titles_shown);
    read = prefs_persist::from_text(prefs_persist::to_text(true));
    REQUIRE(read.outcome.accepted);
    CHECK(read.titles_shown);

    // `default` is the hand-author's word for the code's answer.
    std::string authored = prefs_persist::to_text(true);
    const std::size_t at = authored.find("\"titles\":\"shown\"");
    REQUIRE(at != std::string::npos);
    authored.replace(at, std::string("\"titles\":\"shown\"").size(), "\"titles\":\"default\"");
    read = prefs_persist::from_text(authored);
    REQUIRE(read.outcome.accepted);
    CHECK(read.titles_shown == prefs_persist::kTitlesDefaultValue);

    // A word outside the closed set is refused naming what was found and what works.
    authored = prefs_persist::to_text(true);
    authored.replace(authored.find("\"titles\":\"shown\""),
                     std::string("\"titles\":\"shown\"").size(), "\"titles\":\"visible\"");
    read = prefs_persist::from_text(authored);
    CHECK_FALSE(read.outcome.accepted);
    CHECK(read.outcome.refusal.find("`visible`") != std::string::npos);
    CHECK(read.outcome.refusal.find("default, shown or hidden") != std::string::npos);

    // A foreign version is refused by ITS number, on the claim.
    std::string future = prefs_persist::to_text(true);
    future.replace(future.find("\"version\":1"), std::string("\"version\":1").size(),
                   "\"version\":9");
    read = prefs_persist::from_text(future);
    CHECK_FALSE(read.outcome.accepted);
    CHECK(read.outcome.refusal == "prefs version 9 -- this Workshop reads version 1");

    // The family's format identity: the other files' bytes are refused by name.
    read = prefs_persist::from_text(keymap_persist::to_text(Keymap{}));
    CHECK_FALSE(read.outcome.accepted);
}

TEST_CASE("WUX-3: a toggle writes the preference, and a reopened Workshop wears it") {
    TempDir dir("wux3-prefs");
    const std::string prefs = dir.file("root/workshop-prefs.json");
    {
        Live t;
        t.host.prefs_path = prefs;
        t.publish(loom::to_value(surface::SurfaceReady{}));
        REQUIRE(t.session().pane_titles);
        t.key(input::scan::kT);
        t.text("t");
        REQUIRE_FALSE(t.session().pane_titles);
        // The toggle is the maker stating the preference, so the toggle is the write --
        // and the write created the configuration root it landed in.
        REQUIRE(std::filesystem::exists(prefs));
        CHECK(slurp(prefs).find("\"titles\":\"hidden\"") != std::string::npos);
        // The notice still says what the toggle did, with no complaint added.
        CHECK(t.notice().rfind("pane titles hidden", 0) == 0);
        CHECK_FALSE(t.session().notice_is_bad);
    }
    // ANOTHER PROCESS, ANOTHER DAY: the preference is applied before the first paint.
    Live t;
    t.host.prefs_path = prefs;
    t.publish(loom::to_value(surface::SurfaceReady{}));
    CHECK_FALSE(t.session().pane_titles);
    // ...and toggling back rewrites the same file.
    t.key(input::scan::kT);
    t.text("t");
    CHECK(t.session().pane_titles);
    CHECK(slurp(prefs).find("\"titles\":\"shown\"") != std::string::npos);
}

TEST_CASE("WUX-3: a restored hidden-titles preference keeps the WUX-1 focus law whole") {
    // SC-7's second half: the preference comes back from CONFIGURATION, and the pane
    // holding the keyboard still shows its title and its mark -- restoring a preference
    // must not be a way to recreate the MSG-0 lie.
    TempDir dir("wux3-prefs-focus");
    const std::string prefs = dir.file("workshop-prefs.json");
    REQUIRE(prefs_persist::save_file(prefs, false).accepted);

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
        return external_region_rows(r.last_canvas(), external_body_rect(r.session(), k));
    };
    // Hidden titles: the pane is bare...
    CHECK(shown_rows(kind).at(0).find("Seat @") == std::string::npos);
    // ...until it takes the keyboard, when its identity auto-shows, mark and all.
    press_body(r, kind);
    REQUIRE(keyboard_pane(r.session().panels) == kind);
    CHECK(shown_rows(kind).at(0).rfind(std::string(kTypingHere) + "Seat @", 0) == 0);
}

TEST_CASE("WUX-3: a refused prefs file is spoken, stands, and is never overwritten") {
    TempDir dir("wux3-prefs-bad");
    const std::string prefs = dir.file("workshop-prefs.json");
    spillout(prefs, "{ not a prefs file");
    Live t;
    t.host.prefs_path = prefs;
    t.publish(loom::to_value(surface::SurfaceReady{}));
    // THE REFUSAL IS A STANDING CONDITION, and the defaults stand. It is not
    // on the notice line because the notice line is for things that HAPPENED, and this is
    // a wall that is still there at the next launch.
    {
        const std::vector<Condition> now = attention_conditions(t.session());
        const Condition* wall = condition_by_key(now, kPrefsWallKey);
        REQUIRE(wall != nullptr);
        CHECK(wall->compact.find("defaults stand") != std::string::npos);
        CHECK(wall->role == surface::role::kAlert);
    }
    CHECK(t.attention_note().find("preferences refused") != std::string::npos);
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

TEST_CASE("WUX-3: no prefs path means the preference lives exactly as long as the run") {
    // `--isolated`'s promise, at the weave: an empty path reads nothing and writes
    // nothing, and the toggle still works -- silently local, complaint-free.
    Live t;
    t.publish(loom::to_value(surface::SurfaceReady{}));
    t.key(input::scan::kT);
    t.text("t");
    CHECK_FALSE(t.session().pane_titles);
    CHECK(t.notice() ==
          "pane titles hidden -- a pane holding the keyboard still shows its own");
    CHECK_FALSE(t.session().notice_is_bad);
}

// ---- Session format v3: the placement -------------------------------------------------

TEST_CASE("WUX-3: a session with a placement round-trips byte-identically") {
    const Setup desk = arranged_desk("Debugging");
    session_persist::Placement place;
    place.known = true;
    place.x = -1200; // a monitor left of the primary is negative territory, legitimately
    place.y = 340;
    place.maximized = true;
    const std::string first = session_persist::to_text(desk, 120, 44, place);
    for (const char* fragment :
         {"\"placement\":", "\"mode\":\"desktop\"", "\"x\":\"-1200\"", "\"y\":\"340\"",
          "\"window\":\"maximized\""}) {
        CHECK_MESSAGE(first.find(fragment) != std::string::npos, fragment);
    }
    const session_persist::LoadedSession read = session_persist::from_text(first);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.placement.known);
    CHECK(read.placement.x == -1200);
    CHECK(read.placement.y == 340);
    CHECK(read.placement.maximized);
    CHECK(session_persist::to_text(read.desk, read.viewport_w, read.viewport_h,
                                   read.placement) == first);

    // THE ABSENCE HAS ONE SPELLING: no placement writes `none` over zeros and `normal`.
    const std::string none =
        session_persist::to_text(desk, 120, 44, session_persist::Placement{});
    CHECK(none.find("\"mode\":\"none\"") != std::string::npos);
    CHECK(none.find("\"x\":\"0\"") != std::string::npos);
    CHECK(none.find("\"window\":\"normal\"") != std::string::npos);
    CHECK_FALSE(session_persist::from_text(none).placement.known);
}

TEST_CASE("WUX-3: the placement's words are judged; its coordinates are not") {
    const Setup desk = arranged_desk("D");
    session_persist::Placement place;
    place.known = true;
    place.x = 100;
    place.y = 60;
    const std::string good = session_persist::to_text(desk, 120, 44, place);

    // A mode word outside the closed set refuses the file, naming both sets.
    std::string bad = good;
    bad.replace(bad.find("\"mode\":\"desktop\""), std::string("\"mode\":\"desktop\"").size(),
                "\"mode\":\"monitor\"");
    session_persist::LoadedSession read = session_persist::from_text(bad);
    CHECK_FALSE(read.outcome.accepted);
    CHECK(read.outcome.refusal.find("`monitor`") != std::string::npos);
    CHECK(read.outcome.refusal.find("none or desktop") != std::string::npos);

    // A window word outside its set, the same.
    bad = good;
    bad.replace(bad.find("\"window\":\"normal\""),
                std::string("\"window\":\"normal\"").size(), "\"window\":\"fullscreen\"");
    read = session_persist::from_text(bad);
    CHECK_FALSE(read.outcome.accepted);
    CHECK(read.outcome.refusal.find("`fullscreen`") != std::string::npos);
    CHECK(read.outcome.refusal.find("normal or maximized") != std::string::npos);

    // An absent placement carrying a coordinate is a spelling nobody means: refused,
    // and the refusal says both ways to fix it.
    bad = session_persist::to_text(desk, 120, 44, session_persist::Placement{});
    const std::string none_x = "\"mode\":\"none\",\"x\":\"0\"";
    bad.replace(bad.find(none_x), none_x.size(), "\"mode\":\"none\",\"x\":\"7\"");
    read = session_persist::from_text(bad);
    CHECK_FALSE(read.outcome.accepted);
    CHECK(read.outcome.refusal.find("carries no coordinates") != std::string::npos);

    // A COORDINATE IS ANOTHER MACHINE'S DESKTOP TRUTH, accepted unjudged: only the
    // medium at restore time can judge one, and refusing here would cost the desk.
    place.x = 1000000;
    place.y = -1000000;
    read = session_persist::from_text(session_persist::to_text(desk, 120, 44, place));
    REQUIRE(read.outcome.accepted);
    CHECK(read.placement.x == 1000000);
    CHECK(read.placement.y == -1000000);
}

TEST_CASE("WUX-3/MIG-0: a version-2 session still loads, its placement reading as absence") {
    session_history::v2::WorkshopSession old;
    old.format = session_persist::kFormat;
    old.format_version = 2;
    old.viewport = session_persist::WorkshopViewport{110, 38};
    old.desk = setup_persist::to_setup(arranged_desk("Yesterday"));
    const std::string bytes = loom::compat::serialize(loom::to_value(old));

    op::Catalog conversions;
    REQUIRE(conversions.mount("suite", session_history::conversions()));
    const session_persist::LoadedSession read =
        session_persist::from_text(bytes, &conversions);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.present);
    CHECK(read.honoured);
    CHECK(read.viewport_w == 110);
    CHECK(read.viewport_h == 38);
    CHECK(read.desk == arranged_desk("Yesterday"));
    CHECK_FALSE(read.placement.known);

    // The next close writes version 3, byte-stable thereafter.
    const std::string saved = session_persist::to_text(read.desk, read.viewport_w,
                                                       read.viewport_h, read.placement);
    CHECK(saved.find("\"format_version\":\"3\"") != std::string::npos);
    CHECK(session_persist::from_text(saved).outcome.accepted);
}

// ---- The weave: remember, offer back, and keep the normal room honest -----------------

TEST_CASE("WUX-3: the desk remembers where its window sat, and offers it back") {
    TempDir dir("wux3-place");
    const std::string session = dir.file("session.json");
    {
        Live t;
        t.host.session_path = session;
        t.publish(loom::to_value(surface::SurfaceReady{}));
        // The medium notices its window and says so; Workshop remembers, opaquely.
        t.publish(loom::to_value(surface::SurfacePlacement{240, 180, false}));
        CHECK(t.session().placement_known);
        CHECK(t.session().place_x == 240);
        CHECK(t.session().place_y == 180);
        t.key(input::scan::kQ);
    }
    CHECK(slurp(session).find("\"mode\":\"desktop\"") != std::string::npos);
    CHECK(slurp(session).find("\"x\":\"240\"") != std::string::npos);

    // ANOTHER PROCESS: the remembered placement is offered to whoever holds the skin
    // role, exactly once, before any medium has said anything.
    Live t;
    t.host.session_path = session;
    SkinSeat* skin = t.mount_skin_seat();
    t.publish(loom::to_value(surface::SurfaceReady{}));
    REQUIRE(skin->offered.size() == 1);
    CHECK(skin->offered[0].x == 240);
    CHECK(skin->offered[0].y == 180);
    CHECK_FALSE(skin->offered[0].maximized);
    // ...and the session remembers it even if no medium ever answers.
    CHECK(t.session().placement_known);
}

TEST_CASE("WUX-3: a session with no placement offers nothing") {
    TempDir dir("wux3-place-none");
    const std::string session = dir.file("session.json");
    REQUIRE(session_persist::save_file(session, arranged_desk("D"), 110, 38,
                                       session_persist::Placement{})
                .accepted);
    Live t;
    t.host.session_path = session;
    SkinSeat* skin = t.mount_skin_seat();
    t.publish(loom::to_value(surface::SurfaceReady{}));
    CHECK(skin->offered.empty());
    CHECK_FALSE(t.session().placement_known);
}

TEST_CASE("WUX-3: a maximized close remembers the NORMAL room beside the maximized state") {
    TempDir dir("wux3-max");
    const std::string session = dir.file("session.json");
    {
        Live t;
        t.host.session_path = session;
        t.publish(loom::to_value(surface::SurfaceReady{}));
        // The maker sizes their normal window...
        t.publish(loom::to_value(surface::SurfaceExtent{120, 40, 0, 0}));
        CHECK(t.session().normal_w == 120);
        CHECK(t.session().normal_h == 40);
        // ...then maximizes. The medium reports placement BEFORE the grown extent
        // (skin.hpp's pinned order), so the gate is closed when the big room arrives.
        t.publish(loom::to_value(surface::SurfacePlacement{300, 200, true}));
        t.publish(loom::to_value(surface::SurfaceExtent{200, 80, 0, 0}));
        CHECK(t.session().screen_w == 200); // the live screen follows the window
        CHECK(t.session().normal_w == 120); // the remembered normal room does not
        CHECK(t.session().normal_h == 40);
        t.key(input::scan::kQ);
    }
    const session_persist::LoadedSession read = session_persist::load_file(session);
    REQUIRE(read.outcome.accepted);
    CHECK(read.viewport_w == 120);
    CHECK(read.viewport_h == 40);
    CHECK(read.placement.known);
    CHECK(read.placement.maximized);
    CHECK(read.placement.x == 300);
    CHECK(read.placement.y == 200);
}

TEST_CASE("WUX-3: unmaximizing reopens the gate, and the normal room tracks again") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceReady{}));
    t.publish(loom::to_value(surface::SurfaceExtent{120, 40, 0, 0}));
    t.publish(loom::to_value(surface::SurfacePlacement{300, 200, true}));
    t.publish(loom::to_value(surface::SurfaceExtent{200, 80, 0, 0}));
    REQUIRE(t.session().normal_w == 120);
    // The maker unmaximizes: placement first, then the shrunken extent.
    t.publish(loom::to_value(surface::SurfacePlacement{300, 200, false}));
    t.publish(loom::to_value(surface::SurfaceExtent{130, 44, 0, 0}));
    CHECK(t.session().normal_w == 130);
    CHECK(t.session().normal_h == 44);
}

TEST_CASE("WUX-3: a run whose medium reports no placement RETAINS the remembered one") {
    // A terminal run between two graphical runs must not cost the maker their window
    // position: the TUI has no desktop fact, makes no claim, and carries the memory.
    TempDir dir("wux3-retain");
    const std::string session = dir.file("session.json");
    session_persist::Placement place;
    place.known = true;
    place.x = 640;
    place.y = 220;
    REQUIRE(
        session_persist::save_file(session, arranged_desk("D"), 110, 38, place).accepted);
    {
        Live t;
        t.host.session_path = session;
        t.publish(loom::to_value(surface::SurfaceReady{}));
        // A terminal-shaped run: extents arrive, placements never do.
        t.publish(loom::to_value(surface::SurfaceExtent{100, 33, 0, 0}));
        t.key(input::scan::kQ);
    }
    const session_persist::LoadedSession read = session_persist::load_file(session);
    REQUIRE(read.outcome.accepted);
    // The new room was remembered -- the restored maximized flag from ANOTHER run's
    // window must not stop this run's viewport tracking...
    CHECK(read.viewport_w == 100);
    CHECK(read.viewport_h == 33);
    // ...and the placement crossed unchanged.
    CHECK(read.placement.known);
    CHECK(read.placement.x == 640);
    CHECK(read.placement.y == 220);
}

// ============================================================================
// WUX-6 -- looking at a projection is not authoring one
// ============================================================================

namespace {

/// A GEOMETRY NO MEDIUM HERE CAN SAY THE SAME WAY TWICE. Each number is a whole
/// number of the shipped window's pixels (four sub-units) and none of them is a
/// whole number of cells, so a green produced by values that happen to divide
/// evenly is impossible here -- the phase's own requirement about this falsifier.
inline constexpr std::int64_t kHostilePlaceX = 4 * 77;  //  77 px,  6 cells + 20/48
inline constexpr std::int64_t kHostilePlaceY = 4 * 53;  //  53 px,  4 cells + 20/48
inline constexpr std::int64_t kHostileWidth = 4 * 417;  // 417 px, 34 cells + 36/48
inline constexpr std::int64_t kHostileHeight = 4 * 233; // 233 px, 19 cells + 20/48

static_assert(kHostilePlaceX % surface::kCellSubs != 0, "must not divide evenly");
static_assert(kHostilePlaceY % surface::kCellSubs != 0, "must not divide evenly");
static_assert(kHostileWidth % surface::kCellSubs != 0, "must not divide evenly");
static_assert(kHostileHeight % surface::kCellSubs != 0, "must not divide evenly");

/// A desk holding exactly that, authored through the ordinary value doors.
inline Setup hostile_desk() {
    Setup s;
    s.name = "Hostile";
    REQUIRE(add_pane(s, ref_of(panel::kBuilder)));
    REQUIRE(
        author_pane_place(s, ref_of(panel::kBuilder), kHostilePlaceX, kHostilePlaceY).accepted);
    REQUIRE(author_pane_size(s, ref_of(panel::kBuilder),
                             PaneSize{pane_unit::kSubcells, kHostileWidth},
                             PaneSize{pane_unit::kSubcells, kHostileHeight})
                .accepted);
    return s;
}

} // namespace

TEST_CASE("WUX-6/SC-4: a read-only visit through the other medium writes the SAME BYTES") {
    // THE PHASE'S FALSIFIER. A geometry that cannot round-trip through a terminal without
    // information loss is authored; a whole session is then spent LOOKING at it through
    // both media -- opening the arrangement, stepping to the pane, reading its geometry in
    // one unit and then the other -- and the file that closing writes is compared BYTE FOR
    // BYTE against the file the same session writes having never crossed a medium at all.
    //
    // The two runs differ in exactly one thing: what the medium said about its own device
    // unit. Same room, same desk, same gestures. If a projection could author, these bytes
    // would differ, and the difference would be the projected answer.
    TempDir dir("wux6-project");
    const std::string never = dir.file("never-crossed.json");
    const std::string crossed = dir.file("crossed.json");

    {
        Live t;
        t.host.session_path = never;
        t.publish(loom::to_value(surface::SurfaceReady{}));
        t.publish(loom::to_value(surface::SurfaceExtent{140, 44, 0, 0, 0}));
        live(t).setup.active = hostile_desk();
        t.key(input::scan::kQ);
    }
    {
        Live t;
        t.host.session_path = crossed;
        t.publish(loom::to_value(surface::SurfaceReady{}));
        t.publish(loom::to_value(surface::SurfaceExtent{140, 44, 0, 0, 0}));
        live(t).setup.active = hostile_desk();

        // LOOK AT IT IN CELLS. Every number is a projection this medium cannot say.
        enter_arrange_desk(t);
        for (int i = 0; i < 32 && t.session().arrange.pane != ref_of(panel::kBuilder); ++i) {
            t.key(input::scan::kTab);
        }
        REQUIRE(t.session().arrange.pane == ref_of(panel::kBuilder));
        INFO(t.notice());
        CHECK(t.notice().find("~34x~19 cells") != std::string::npos);
        CHECK(t.notice().find("(~ projected)") != std::string::npos);

        // NOW THE SAME DESK ON THE SHIPPED WINDOW, at the same room -- so the ONLY thing
        // that changed about this run is which unit the maker is reading in.
        t.publish(loom::to_value(
            surface::SurfaceExtent{140, 44, 8, 18, surface::kCanvasCellPx}));
        t.key(input::scan::kTab);
        for (int i = 0; i < 32 && t.session().arrange.pane != ref_of(panel::kBuilder); ++i) {
            t.key(input::scan::kTab);
        }
        INFO(t.notice());
        CHECK(t.notice().find("@77,53 417x233 px") != std::string::npos);
        CHECK(t.notice().find("(~ projected)") == std::string::npos);

        // ...AND BACK, which is the direction that would show a write having happened.
        t.publish(loom::to_value(surface::SurfaceExtent{140, 44, 0, 0, 0}));
        t.key(input::scan::kTab);
        for (int i = 0; i < 32 && t.session().arrange.pane != ref_of(panel::kBuilder); ++i) {
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
    const SetupPane* row = pane_of(read.desk, ref_of(panel::kBuilder));
    REQUIRE(row != nullptr);
    CHECK(row->place.x == kHostilePlaceX);
    CHECK(row->place.y == kHostilePlaceY);
    CHECK(row->width.amount == kHostileWidth);
    CHECK(row->height.amount == kHostileHeight);
    // NOT the projected answers, which is what a medium writing back would have left.
    CHECK(row->width.amount != subs(34));
    CHECK(row->height.amount != subs(19));
}

TEST_CASE("WUX-6/SC-9: the medium's device unit reaches no durable file") {
    // It is the text metric's own rule, for the text metric's own reason: how big a cell
    // is belongs to whichever medium opens the face, is republished every run, and would
    // be a stale claim about somebody else's monitor the moment it was written down.
    TempDir dir("wux6-nofile");
    const std::string path = dir.file("session.json");
    {
        Live t;
        t.host.session_path = path;
        t.publish(loom::to_value(surface::SurfaceReady{}));
        t.publish(loom::to_value(
            surface::SurfaceExtent{140, 44, 8, 18, surface::kCanvasCellPx}));
        REQUIRE(t.session().cell_px == surface::kCanvasCellPx);
        live(t).setup.active = hostile_desk();
        t.key(input::scan::kQ);
    }
    const std::string text = slurp(path);
    INFO(text);
    CHECK(text.find("cell_px") == std::string::npos);
    CHECK(text.find("\"12\"") == std::string::npos);

    // AND A RESTORE HANDS THIS RUN'S UNIT STRAIGHT BACK. A file remembers the ROOM; what
    // the medium said about its own units is THIS run's, and the restore -- which adopts a
    // remembered viewport through the same door -- must not reset it to the character
    // reading and leave a maker on a window reading cells.
    Live t;
    t.host.session_path = path;
    t.publish(loom::to_value(surface::SurfaceExtent{200, 60, 8, 18, surface::kCanvasCellPx}));
    REQUIRE(t.session().cell_px == surface::kCanvasCellPx);
    t.publish(loom::to_value(surface::SurfaceReady{}));
    CHECK(t.session().screen_w == 140);                      // the restore DID land...
    CHECK(t.session().cell_px == surface::kCanvasCellPx);     // ...and cost nothing
    CHECK(std::string(geometry_unit(t.session().cell_px)) == "px");
}

TEST_CASE("WUX-3: a restored maximized flag alone does not gate this run's viewport") {
    TempDir dir("wux3-stale-max");
    const std::string session = dir.file("session.json");
    session_persist::Placement place;
    place.known = true;
    place.x = 10;
    place.y = 10;
    place.maximized = true; // last run closed maximized...
    REQUIRE(
        session_persist::save_file(session, arranged_desk("D"), 110, 38, place).accepted);
    Live t;
    t.host.session_path = session;
    t.publish(loom::to_value(surface::SurfaceReady{}));
    // ...but THIS run's medium never says so (a terminal), so resizes track normally.
    t.publish(loom::to_value(surface::SurfaceExtent{100, 33, 0, 0}));
    CHECK(t.session().normal_w == 100);
    CHECK(t.session().normal_h == 33);
}


// ============================================================================
// ---- WUX-9: what the layout shelf means for the two files ------------------
//
// A Setup file is still ONE named desk arrangement, and the session still carries ONE
// desk. The shelf is live composition and reaches neither format: what these cases pin is
// that `s` and `r` act on the LIVE layout alone, that neither touches the shelf, and that
// the session's promise is exactly the one it always made.

namespace {

/// Step the live layout one along the run, as a maker does.
void layout_next(Live& t) {
    const Gesture g = t.session().keymap.gesture_of(Act::kLayoutNext);
    t.key(g.scancode, g.modifiers);
}

/// Copy the live layout and stand on the copy, as a maker does.
void layout_new(Live& t) {
    const Gesture g = t.session().keymap.gesture_of(Act::kLayoutNew);
    t.key(g.scancode, g.modifiers);
}

/// Every layout this Workshop holds, in the maker's order.
std::vector<Setup> shelf_of(const Live& t) {
    std::vector<Setup> out;
    for (std::size_t i = 0; i < layout_count(t.session().setup); ++i) {
        out.push_back(layout_at(t.session().setup, i));
    }
    return out;
}

} // namespace

TEST_CASE("WUX-9/SC-12: `s` writes the live layout and leaves the shelf alone") {
    TempDir dir("wux9-save");
    Live t;
    t.host.setup_path = dir.file("s.json");

    layout_new(t);
    REQUIRE(add_pane(live(t).setup.active, ref_of(panel::kBuilder)));
    REQUIRE(layout_count(t.session().setup) == 2);
    const std::vector<Setup> before = shelf_of(t);

    name_setup(t, "Second");
    CHECK(t.notice().find("saved setup \"Second\"") == 0);

    // THE FILE HOLDS ONE DESK -- the live one -- and its meaning is unchanged: the
    // format's own reader gets exactly the arrangement a maker was standing in.
    const setup_persist::LoadedSetup written = setup_persist::load_file(t.host.setup_path);
    REQUIRE(written.outcome.accepted);
    CHECK(written.setup == t.session().setup.active);
    CHECK(written.setup.name == "Second");
    CHECK(has_pane(written.setup, ref_of(panel::kBuilder)));
    // ...AND THE SHELF IS BYTE-FOR-BYTE WHAT IT WAS, name included.
    const std::vector<Setup> after = shelf_of(t);
    REQUIRE(after.size() == before.size());
    CHECK(after[0] == before[0]);
    // The saved marker is about the live layout against the file, and says so.
    CHECK(t.session().setup.saved());
    CHECK(setup_row(t.canvases.back(), screen_of(t.session())).find("| saved") !=
          std::string::npos);
}

TEST_CASE("WUX-9/SC-12: `r` restores into the live layout and clears no shelf") {
    TempDir dir("wux9-restore");
    const Setup named = setup_of("From file", {panel::kInfo, panel::kBuilder});
    REQUIRE(setup_persist::save_file(dir.file("s.json"), named).accepted);

    Live t;
    t.host.setup_path = dir.file("s.json");
    layout_new(t);
    live(t).setup.active.name = "Scratch";
    REQUIRE(layout_count(t.session().setup) == 2);
    const Setup shelved = layout_at(t.session().setup, 0);
    const std::size_t at = t.session().setup.active_at;

    t.key(input::scan::kR);
    CHECK(t.notice().find("restored setup \"From file\"") == 0);

    // THE LIVE LAYOUT BECAME THE FILE'S DESK, IN ITS OWN POSITION IN THE RUN...
    CHECK(t.session().setup.active == named);
    CHECK(t.session().setup.active_at == at);
    CHECK(layout_count(t.session().setup) == 2);
    // ...AND NOTHING ELSE ON THE SHELF MOVED. A restore that replaced the run would be
    // one file quietly deciding how many desks a maker has.
    CHECK(layout_at(t.session().setup, 0) == shelved);

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

TEST_CASE("WUX-9/SC-13: the live layout still rides the session, and the shelf does not") {
    TempDir dir("wux9-session");
    const std::string session = dir.file("session.json");

    {
        Live t;
        t.host.session_path = session;
        t.host.setup_path = dir.file("s.json");
        t.publish(loom::to_value(surface::SurfaceReady{}));
        t.publish(loom::to_value(surface::SurfaceExtent{120, 44}));

        layout_new(t);
        live(t).setup.active.name = "Second";
        REQUIRE(add_pane(live(t).setup.active, ref_of(panel::kBuilder)));
        layout_new(t);
        live(t).setup.active.name = "Third";
        REQUIRE(layout_count(t.session().setup) == 3);
        // Stand on the middle one, so what returns is neither the first nor the last.
        layout_next(t);
        layout_next(t);
        REQUIRE(t.session().setup.active.name == "Second");

        t.key(input::scan::kQ);
        REQUIRE(t.host.quit);
    }

    // THE FORMAT DID NOT MOVE. WUX-9 bumps no schema, and the file still carries ONE
    // desk under the field it always did.
    const session_persist::LoadedSession read = session_persist::load_file(session);
    REQUIRE(read.outcome.accepted);
    CHECK(read.desk.name == "Second");
    CHECK(has_pane(read.desk, ref_of(panel::kBuilder)));
    CHECK(slurp(session).find("\"format_version\":\"3\"") != std::string::npos);
    CHECK(slurp(session).find("layout") == std::string::npos);
    CHECK(slurp(session).find("shelved") == std::string::npos);

    // AND THE NEXT RUN COMES BACK ON THAT DESK, alone -- the temporary limitation this
    // phase states plainly rather than dressing up as a decision.
    Live back;
    back.host.session_path = session;
    back.host.setup_path = dir.file("elsewhere.json");
    back.publish(loom::to_value(surface::SurfaceReady{}));
    CHECK(back.session().setup.active.name == "Second");
    CHECK(has_pane(back.session().setup.active, ref_of(panel::kBuilder)));
    CHECK(layout_count(back.session().setup) == 1);
    CHECK(back.session().setup.active_at == 0);
}

TEST_CASE("WUX-9/SC-14: crossing media never writes a device value into any layout") {
    TempDir dir("wux9-media");
    Live t;
    t.host.setup_path = dir.file("s.json");
    t.publish(loom::to_value(surface::SurfaceReady{}));
    t.publish(loom::to_value(surface::SurfaceExtent{120, 44}));

    // TWO LAYOUTS WITH DISTINCT FINE-LATTICE GEOMETRY -- one of them deliberately NOT on
    // a cell boundary, which is the value a character medium cannot say and must not
    // round on its way through.
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(panel::kInfo),
                              surface::subs_of_cells(3) + 17, surface::subs_of_cells(4) + 5)
                .accepted);
    REQUIRE(author_pane_size(live(t).setup.active, ref_of(panel::kInfo),
                             PaneSize{pane_unit::kSubcells, surface::subs_of_cells(20) + 11},
                             PaneSize{pane_unit::kSubcells, surface::subs_of_cells(9) + 23})
                .accepted);
    live(t).setup.active.name = "Fine";
    const Setup fine = t.session().setup.active;

    layout_new(t);
    live(t).setup.active.name = "Whole";
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(panel::kInfo),
                              surface::subs_of_cells(6), surface::subs_of_cells(2))
                .accepted);
    const Setup whole = t.session().setup.active;
    REQUIRE(fine != whole);

    // VIEW BOTH ON A WINDOW WITH A REAL FACE AND ITS OWN PIXEL, then on a terminal, then
    // back -- switching, repainting and reading all the way.
    for (int round = 0; round < 2; ++round) {
        CAPTURE(round);
        t.publish(loom::to_value(surface::SurfaceExtent{120, 44, 8, 18, 12}));
        layout_next(t);
        (void)paint(t.doc(), t.session(), t.host.setup_path);
        layout_next(t);
        (void)paint(t.doc(), t.session(), t.host.setup_path);
        t.publish(loom::to_value(surface::SurfaceExtent{120, 44, 0, 0, 0}));
        layout_next(t);
        (void)paint(t.doc(), t.session(), t.host.setup_path);
        layout_next(t);
        (void)paint(t.doc(), t.session(), t.host.setup_path);
    }

    // EVERY LAYOUT'S AUTHORED NUMBERS ARE THE ONES THAT WENT IN. Looking is not
    // authoring, and a switch is not a geometry conversion.
    CHECK(layout_at(t.session().setup, 0) == fine);
    CHECK(layout_at(t.session().setup, 1) == whole);

    // ...AND SAVING EACH ONE WRITES THE SAME BYTES AS A RUN THAT NEVER CROSSED.
    CHECK(setup_persist::to_text(layout_at(t.session().setup, 0)) ==
          setup_persist::to_text(fine));
    CHECK(setup_persist::to_text(layout_at(t.session().setup, 1)) ==
          setup_persist::to_text(whole));
    // No projected spelling reached the bytes: a device unit has no word in this format.
    CHECK(setup_persist::to_text(fine).find("px") == std::string::npos);
    CHECK(setup_persist::to_text(fine).find("pixels") == std::string::npos);
}

// =============================================================================
// MIG-0 — YESTERDAY'S SESSION, THROUGH A CONVERSION THIS RUN HAPPENS TO HAVE
// =============================================================================
//
// ONE PRODUCT SENTENCE, PINNED FROM BOTH ENDS: a Workshop whose session reader knows only
// the shape it admits still opens a session file written months ago -- because an artifact
// the arrangement mounted contributes the conversion, and the reader spends it through the
// same operator catalog every other power in this process comes from.
//
// ...AND THE SENTENCE THAT MAKES IT SAFE: if that conversion is not already live, old bytes
// can ask for nothing more powerful than an honest refusal. No load, no mount, no plan row,
// no rewrite.
//
// WHAT THESE CASES DO NOT RE-PROVE. How an edge is spelled, how one is found, what a
// hostile contribution is refused for, and what a chain is refused for are the operator
// suite's (`test_operator_migration.cpp`), asked over an invented history so the real
// session converters never have to be dishonest. What is asked HERE is what a durable
// OWNER does with all of that.

namespace {

/// A version-1 session file, exactly as WUX-0's Workshop wrote one: whole-cell geometry, a
/// nested version-2 desk, and no placement because nothing could say one.
session_history::v1::WorkshopSession old_v1_session(const char* name, std::int64_t w,
                                                    std::int64_t h) {
    session_history::v1::WorkshopSession old;
    old.format = session_persist::kFormat;
    old.format_version = 1;
    old.viewport = session_persist::WorkshopViewport{w, h};
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

/// The same vintage's WUX-2 successor: the current desk shape, still no placement.
session_history::v2::WorkshopSession old_v2_session(const char* name, std::int64_t w,
                                                    std::int64_t h) {
    session_history::v2::WorkshopSession old;
    old.format = session_persist::kFormat;
    old.format_version = 2;
    old.viewport = session_persist::WorkshopViewport{w, h};
    old.desk = setup_persist::to_setup(arranged_desk(name));
    return old;
}

std::string as_text(const session_history::v1::WorkshopSession& old) {
    return loom::compat::serialize(loom::to_value(old));
}
std::string as_text(const session_history::v2::WorkshopSession& old) {
    return loom::compat::serialize(loom::to_value(old));
}

/// A catalog holding the real shipped conversion artifact -- the file a maker's Workshop
/// mounts, not a stand-in for it.
struct MountedHistory {
    op::Catalog catalog;
    op::MountResult mounted;

    MountedHistory() { mounted = op::mount_provider(catalog, SESSION_HISTORY_SO); }
};

} // namespace

TEST_CASE("MIG-0/SC-7: the shipped artifact supplies exactly the two conventional edges") {
    MountedHistory history;
    REQUIRE_MESSAGE(history.mounted.ok, history.mounted.reason);
    CHECK(history.mounted.provider == "zengine.workshop.session_history");
    // THE IDENTITIES ARE DERIVED FROM THE EDGES, so this list is a reading of the
    // convention rather than a list somebody typed twice.
    const std::vector<std::string> supplied = history.catalog.identities();
    CHECK(supplied == std::vector<std::string>{"zengine.migrate.WorkshopSession.v1-to-v3",
                                               "zengine.migrate.WorkshopSession.v2-to-v3"});
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

TEST_CASE("MIG-0/SC-7: a version-1 session means EXACTLY what its own reader meant") {
    // ⭐ THE EQUIVALENCE PIN, AND IT IS NOT A COPIED NUMBER. The desk the predecessor's road
    // produced is still computable -- `setup_persist::setup_in_v2` is the standalone setup
    // file's own legacy reader and is untouched by this phase -- so the two answers are
    // compared as VALUES rather than against a transcription of what one of them once said.
    const session_history::v1::WorkshopSession old = old_v1_session("Yesterday", 120, 44);
    Setup predecessor;
    REQUIRE(setup_persist::setup_in_v2(old.desk, predecessor).accepted);

    MountedHistory history;
    REQUIRE(history.mounted.ok);
    const session_persist::LoadedSession read =
        session_persist::from_text(as_text(old), &history.catalog);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);

    CHECK(read.desk == predecessor);
    // ...and every session fact of that vintage, by hand, so the comparison above cannot
    // pass by both sides being empty.
    CHECK(read.present);
    CHECK(read.honoured);
    CHECK(read.viewport_w == 120);
    CHECK(read.viewport_h == 44);
    CHECK(read.desk.name == "Yesterday");
    REQUIRE(read.desk.panes.size() == 2);
    CHECK(read.desk.panes[0].ref == PaneRef{"zengine.workshop", "info"});
    CHECK(read.desk.panes[0].place.mode == pane_unit::kSubcells);
    CHECK(read.desk.panes[0].place.x == subs(3));
    CHECK(read.desk.panes[0].place.y == subs(2));
    CHECK(read.desk.panes[0].width.amount == subs(28));
    CHECK(read.desk.panes[0].width.mode == pane_unit::kSubcells);
    CHECK(read.desk.panes[0].height.mode == pane_unit::kDefault);
    CHECK(read.desk.panes[1].ref == PaneRef{"zengine.workshop", "builder"});
    CHECK(read.desk.panes[1].place.mode == pane_unit::kDefault);
    CHECK(read.desk.panes[1].width.amount == subs(40));
    // A PIXEL AXIS IS DEVICE PIXELS IN BOTH VERSIONS AND CROSSES UNSCALED.
    CHECK(read.desk.panes[1].height.mode == pane_unit::kPixels);
    CHECK(read.desk.panes[1].height.amount == 220);
    CHECK(read.desk.panes[1].front == 1);
    // A LEGACY ROAD CARRIES NO PLACEMENT: nothing in a v1 file could have said one, and the
    // absence has exactly one spelling.
    CHECK_FALSE(read.placement.known);
    CHECK(read.placement.x == 0);
    CHECK(read.placement.y == 0);
    CHECK_FALSE(read.placement.maximized);
}

TEST_CASE("MIG-0/SC-7: a version-2 session means exactly what its own reader meant") {
    const session_history::v2::WorkshopSession old = old_v2_session("Yesterday", 110, 38);
    Setup predecessor;
    REQUIRE(setup_persist::setup_in(old.desk, predecessor).accepted);

    MountedHistory history;
    REQUIRE(history.mounted.ok);
    const session_persist::LoadedSession read =
        session_persist::from_text(as_text(old), &history.catalog);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.desk == predecessor);
    CHECK(read.desk == arranged_desk("Yesterday"));
    CHECK(read.viewport_w == 110);
    CHECK(read.viewport_h == 38);
    CHECK(read.honoured);
    CHECK_FALSE(read.placement.known);
}

TEST_CASE("MIG-0: an old session's OWN law still runs -- the conversion skips no check") {
    MountedHistory history;
    REQUIRE(history.mounted.ok);

    SUBCASE("a desk that is not a legal setup is refused in the setup owner's own words") {
        session_history::v1::WorkshopSession old = old_v1_session("Bad", 100, 30);
        old.desk.panes[1].pane = "two words";
        const session_persist::LoadedSession no =
            session_persist::from_text(as_text(old), &history.catalog);
        CHECK_FALSE(no.outcome.accepted);
        CHECK(no.outcome.refusal ==
              "a pane reference's pane key cannot contain spaces or control characters");
    }
    SUBCASE("a unit word version 2 never had is refused in VERSION 2's vocabulary") {
        session_history::v1::WorkshopSession old = old_v1_session("Bad", 100, 30);
        old.desk.panes[0].place.mode = "barns";
        const session_persist::LoadedSession no =
            session_persist::from_text(as_text(old), &history.catalog);
        CHECK_FALSE(no.outcome.accepted);
        CHECK(no.outcome.refusal.find("`barns`") != std::string::npos);
        CHECK(no.outcome.refusal.find("default or cells") != std::string::npos);
        // The conversion that refused is named, because a maker who has one converter
        // mounted and another missing needs to know which spoke.
        CHECK(no.outcome.refusal.find("zengine.migrate.WorkshopSession.v1-to-v3") !=
              std::string::npos);
    }
    SUBCASE("a viewport this build will not open at is declined, and the desk still comes") {
        const session_history::v1::WorkshopSession old = old_v1_session("Huge", 100000, 44);
        const session_persist::LoadedSession read =
            session_persist::from_text(as_text(old), &history.catalog);
        REQUIRE(read.outcome.accepted);
        CHECK_FALSE(read.honoured);
        CHECK_FALSE(read.declined.empty());
        CHECK(read.desk.name == "Huge");
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

TEST_CASE("MIG-0/SC-5: an old session with no conversion live refuses and changes nothing") {
    TempDir dir("mig0-absent");
    const std::string path = dir.file("session.json");
    const std::string bytes = as_text(old_v1_session("Yesterday", 120, 44));
    spillout(path, bytes);

    // THE AUTHORITY MEASUREMENT. The artifact that would supply the conversion is on disk,
    // named by this very build -- and a run that did not mount it does not open it.
    const op::ImageCounts before = op::image_counts();
    Live t;
    t.host.session_path = path;
    REQUIRE(t.host.conversions == nullptr);
    t.publish(loom::to_value(surface::SurfaceReady{}));

    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("session version 1 cannot be read") != std::string::npos);
    CHECK(t.notice().find("no live conversion") != std::string::npos);
    CHECK(t.notice().find("opening with the default setup") != std::string::npos);
    // NOTHING WAS INSTALLED and nothing was written.
    CHECK(t.session().setup.active == default_setup());
    CHECK(t.session().screen_w == kScreenMinW);
    CHECK(slurp(path) == bytes);
    // NO IMAGE WAS OPENED. A version claim is a lookup key; it reaches no load door.
    const op::ImageCounts after = op::image_counts();
    CHECK(after.opens == before.opens);
    CHECK(after.closes == before.closes);
    CHECK_FALSE(slurp(SESSION_HISTORY_SO).empty()); // ...and it was sitting right there
}

TEST_CASE("MIG-0/SC-6: with the conversion mounted, the desk comes back through the weave") {
    TempDir dir("mig0-live");
    const std::string path = dir.file("session.json");
    spillout(path, as_text(old_v1_session("Yesterday", 120, 44)));

    MountedHistory history;
    REQUIRE(history.mounted.ok);
    Live t;
    t.host.session_path = path;
    t.host.conversions = &history.catalog;
    t.publish(loom::to_value(surface::SurfaceReady{}));

    // ...and it is an ORDINARY restore: the same notice, the same room, the same desk.
    CHECK_FALSE(t.session().notice_is_bad);
    CHECK(t.notice().find("reopened your last desk") == 0);
    CHECK(t.notice().find("\"Yesterday\"") != std::string::npos);
    CHECK(t.session().screen_w == 120);
    CHECK(t.session().screen_h == 44);
    CHECK(t.session().setup.active.name == "Yesterday");
    CHECK(t.session().panels.has(panel::kInfo));
    CHECK(t.session().panels.has(panel::kBuilder));
}

TEST_CASE("MIG-0/SC-13: reading an old session does not rewrite it; the next close does") {
    // THE PAYOFF, IN FIVE STEPS. A converter is needed only while yesterday's bytes still
    // exist -- and the moment a maker closes normally, they do not.
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
        t.publish(loom::to_value(surface::SurfaceReady{}));
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
    CHECK(now.find("\"version\":3") != std::string::npos);
    CHECK(now.find("\"format_version\":\"3\"") != std::string::npos);

    // 5. ...SO THE NEXT RUN NEEDS NO CONVERTER AT ALL.
    Live back;
    back.host.session_path = path;
    REQUIRE(back.host.conversions == nullptr);
    back.publish(loom::to_value(surface::SurfaceReady{}));
    CHECK_FALSE(back.session().notice_is_bad);
    CHECK(back.session().setup.active.name == "Yesterday");
    CHECK(back.session().screen_w == 120);
}

TEST_CASE("MIG-0/SC-11: unmounting the artifact takes the conversion with it") {
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
    const std::string current = session_persist::to_text(arranged_desk("Now"), 100, 30,
                                                         session_persist::Placement{});
    CHECK(session_persist::from_text(current, &history.catalog).outcome.accepted);
}

TEST_CASE("MIG-0/SC-14: a current session bypasses conversion entirely") {
    // Measured on the invocation counter rather than asserted: the conversions are mounted
    // IN PROCESS here (`op::invocations()` cannot see a body in another image), so a spend
    // would move the number.
    op::Catalog conversions;
    REQUIRE(conversions.mount("suite", session_history::conversions()));
    const std::string current = session_persist::to_text(arranged_desk("Now"), 132, 48,
                                                         session_persist::Placement{});

    const std::uint64_t before = op::invocations();
    const session_persist::LoadedSession read =
        session_persist::from_text(current, &conversions);
    REQUIRE(read.outcome.accepted);
    CHECK(read.desk == arranged_desk("Now"));
    CHECK(op::invocations() == before);
}

TEST_CASE("MIG-0/SC-5: nothing but a historical claim of THIS shape asks for a conversion") {
    // THE NARROWNESS THAT KEEPS THIS FROM BEING A FALLBACK. A seam that answered "try a
    // conversion" to any admission failure would turn every corrupt file, every wrong file
    // and every hostile file into a search for something willing to eat it.
    op::Catalog conversions;
    REQUIRE(conversions.mount("suite", session_history::conversions()));

    const std::vector<std::pair<const char*, std::string>> never = {
        {"not a Zen value at all", "{"},
        {"a DOCUMENT handed to the session reader", persist::to_text(WorkshopDoc{})},
        {"a SETUP file handed to the session reader",
         setup_persist::to_text(arranged_desk("Debugging"))},
        {"a current-version session with a malformed desk",
         [] {
             std::string text = session_persist::to_text(arranged_desk("D"), 100, 30,
                                                         session_persist::Placement{});
             const std::size_t at = text.find("\"pane\":\"builder\"");
             REQUIRE(at != std::string::npos);
             text.replace(at, std::string("\"pane\":\"builder\"").size(),
                          "\"pane\":\"two words\"");
             return text;
         }()},
        {"a current-version session with a hostile placement word",
         [] {
             std::string text = session_persist::to_text(arranged_desk("D"), 100, 30,
                                                         session_persist::Placement{});
             const std::size_t at = text.find("\"window\":\"normal\"");
             REQUIRE(at != std::string::npos);
             text.replace(at, std::string("\"window\":\"normal\"").size(),
                          "\"window\":\"iconified\"");
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

TEST_CASE("MIG-0/SC-8: the session reader owns no historical shape and no conversion") {
    // Defence in depth, the shape this repository's other source tripwires use: what a
    // translation unit can NAME is a fact only reading the file can carry, and the whole
    // value of the move is that the current owner stops growing a rung per vintage.
    const std::string source = slurp(WORKSHOP_SESSION_PERSIST_HPP);
    REQUIRE_FALSE(source.empty());
    for (const char* forbidden : {"namespace v1", "namespace v2", "setup_in_v2",
                                  "kV1FormatVersion", "kV2FormatVersion",
                                  "session_history::", "WorkshopSession, 1", "WorkshopSession, 2"}) {
        CAPTURE(forbidden);
        CHECK(source.find(forbidden) == std::string::npos);
    }
    // ...and the one number it does carry is the one it writes.
    CHECK(session_persist::kFormatVersion == 3);
    CHECK(session_persist::WorkshopSession::zen_version == 3u);
}

TEST_CASE("MIG-0/SC-13: a session this run could not read is never written over") {
    // ⭐ THE FILE IS WORTH MORE THAN IT USED TO BE. Before MIG-0 a refused session was one
    // this build would never read; now the likeliest reason is that the conversion for it is
    // not mounted in THIS arrangement -- which a maker fixes by adding a row to a plan, on a
    // file that has to still be there when they do. So an orderly close writes nothing.
    struct Case {
        const char* what;
        std::string bytes;
        bool refused;
    };
    const std::vector<Case> cases = {
        {"an older session with no conversion live", as_text(old_v1_session("Old", 120, 44)),
         true},
        {"a version this build has never written", [] {
             std::string text = session_persist::to_text(arranged_desk("D"), 100, 30,
                                                         session_persist::Placement{});
             const std::size_t at = text.find("\"version\":3");
             REQUIRE(at != std::string::npos);
             text.replace(at, std::string("\"version\":3").size(), "\"version\":9");
             return text;
         }(), true},
        {"bytes that are not a session at all", std::string("{"), true},
        // ...AND THE CONTROL, WHICH IS THE HALF THAT MAKES THE FLAG A JUDGEMENT RATHER THAN A
        // BLANKET. A file whose VIEWPORT was declined was READ -- its desk came back -- so
        // the run keeps its session exactly as it always did.
        {"a session whose viewport this build declines",
         session_persist::to_text(arranged_desk("Wide"), 100000, 44,
                                  session_persist::Placement{}),
         false},
    };
    for (const Case& c : cases) {
        CAPTURE(c.what);
        TempDir dir("mig0-refused");
        const std::string path = dir.file("session.json");
        spillout(path, c.bytes);
        {
            Live t;
            t.host.session_path = path;
            t.publish(loom::to_value(surface::SurfaceReady{}));
            // Every case here says SOMETHING bad -- a declined viewport is a complaint too --
            // so the notice cannot be the discriminator, and the condition is.
            CHECK(t.session().notice_is_bad);
            // THE STANDING CONSEQUENCE IS A CONDITION, not the notice: that this run keeps no
            // session is still true an hour later and has a maker action, which is exactly
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

TEST_CASE("MIG-0/SC-13: the file survives the run that could not read it, and opens later") {
    // ⭐ THE WHOLE POINT OF THE PREVIOUS CASE, IN ONE STORY. A maker launches an arrangement
    // whose plan does not carry the conversion, is told so, works, closes -- and then adds
    // the row and gets their desk back. Nothing about the second run is special.
    TempDir dir("mig0-recovered");
    const std::string path = dir.file("session.json");
    const std::string original = as_text(old_v1_session("Yesterday", 120, 44));
    spillout(path, original);
    {
        Live without;
        without.host.session_path = path;
        without.publish(loom::to_value(surface::SurfaceReady{}));
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
    with.publish(loom::to_value(surface::SurfaceReady{}));
    CHECK_FALSE(with.session().notice_is_bad);
    CHECK(with.session().setup.active.name == "Yesterday");
    CHECK(with.session().conditions.find(kSessionWallKey) == nullptr);
}

TEST_CASE("MIG-0/SC-7: a conversion owns yesterday's semantics and does not rewrite history") {
    // THE VINTAGE A CONVERSION CONVERTS IS THE ONE IT CHECKS. An envelope claiming version 1
    // is what SELECTED this edge; a body that then says it is a different vintage is a file
    // asking to be translated by a road it does not belong to, and stamping the current
    // number onto it would admit a file the predecessor refused.
    MountedHistory history;
    REQUIRE(history.mounted.ok);

    SUBCASE("the session's own format_version must be the vintage the edge converts") {
        session_history::v1::WorkshopSession old = old_v1_session("Forged", 100, 30);
        old.format_version = 7; // the envelope still claims v1
        const session_persist::LoadedSession no =
            session_persist::from_text(as_text(old), &history.catalog);
        CHECK_FALSE(no.outcome.accepted);
        CHECK(no.outcome.refusal.find("this session claims version 1 and its own "
                                      "format_version field says 7") != std::string::npos);
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
    SUBCASE("a version-2 session's desk is judged by the CURRENT setup reader, unchanged") {
        // The v2 edge passes the desk through whole, because a version-2 session already
        // nests the shape `setup_in` reads -- so a wrong number there is that reader's
        // sentence, in that reader's own words, exactly as it was before this phase.
        session_history::v2::WorkshopSession old = old_v2_session("Forged", 100, 30);
        old.desk.format_version = 2;
        const session_persist::LoadedSession no =
            session_persist::from_text(as_text(old), &history.catalog);
        CHECK_FALSE(no.outcome.accepted);
        CHECK(no.outcome.refusal == setup_persist::wrong_version(2));
    }
}
