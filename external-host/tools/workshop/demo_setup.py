# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Prepare a described setup and keep it ready: the service behind `demo.py start` and the
visible Reset button. Product state changes always go through their owners; what a setup
prepares is read from its description (setups.py, docs/workshop/demo-setups.md)."""
import base64
from collections import Counter
import json
from pathlib import Path
import time

import setups as described

ROLE = "zengine.demo"
PANE = "zengine.inventory-pane"
WORKSHOP = "zengine.workshop"
NO_ENTRY = {"owner": "", "entry": ""}
CANNOT_PRESENT = "setup names a pane this Workshop cannot present: "
DRAWS_PICTURE = "the pane draws a picture"  # Workshop's refusal of a text view of a canvas pane
READY_SECONDS = 15                           # how long a desk pane has to show its first picture


def layout(name, views=()):
    """A named setup's desk from this package's collection, as Workshop's WorkshopSetup file,
    with each portable view in `views` seated in the setup's declared view slots."""
    return described.load(name).desk(views)


def save_json(path, value):
    path = Path(path)
    temporary = path.with_suffix(path.suffix + ".new")
    temporary.write_text(json.dumps(value, indent=2), encoding="utf-8")
    temporary.replace(path)


class Measured:
    def __init__(self, ctx):
        self.ctx, self.calls, self.outcomes = ctx, Counter(), Counter()

    def __getattr__(self, name):
        return getattr(self.ctx, name)

    def ask(self, role, shape, fields, **kwargs):
        self.calls[shape] += 1
        try:
            answer = self.ctx.ask(role, shape, fields, **kwargs)
            self.outcomes["answered"] += 1
            return answer
        except Exception:
            self.outcomes["failed_or_unanswered"] += 1
            raise


class Owners:
    """Owner requests over the link, without an input session."""
    def __init__(self, ctx, link):
        self.ctx, self.link = ctx, link

    def ask(self, role, shape, fields, **kwargs):
        return self.ctx.ask(role, shape, fields, via=self.link, **kwargs)

    def view(self, provider, pane):
        return self.ask("zengine.workshop", "PaneViewRequested", {"provider": provider, "pane": pane})

    def edit(self, **fields):
        # Inventory's configuration door: every declared field is written.
        whole = dict(operation="", view="", text="", entry=NO_ENTRY, before=NO_ENTRY, scancode=0,
                     modifiers=0, enabled=False)
        whole.update(fields)
        return self.ask(PANE, "InventoryViewEdit", whole, settle=True)

    def views(self):
        return self.ask(PANE, "InventoryViewsRequested", {})


def stage(state, name):
    """Record which step preparation reached, so a refusal says what was already applied."""
    state["reached"] = name


# ---- material: the entries a setup owns, restored by identity ----------------------------------
def capture_material(hand, material, state):
    labels = material["labels"]
    for label in labels[len(state["fixtures"]):]:
        entry = hand.ask("zengine.inventory", "InventoryCaptureAdd",
                         {"target_role": material["target_role"], "label": label}, settle=True)
        state["fixtures"].append({"reference": entry["reference"], "revision": entry["revision"],
                                  "label": label, "folder": "",
                                  "pair": base64.b64encode(entry["pair"]).decode()})


def toolbox_material(hand, setup, state):
    """The toolbox once, into an empty collection; its entries become the setup's fixtures."""
    listed = hand.ask("zengine.inventory", "InventoryList", {}, version=2)
    hand.ctx.check(not listed["entries"], "Inventory already holds %d entries: a toolbox setup "
                   "starts from an empty collection (use a new root)" % len(listed["entries"]))
    path = str(setup.asset(setup.material()["toolbox"]))
    hand.ask(PANE, "InventoryToolboxRestore", {"path": path, "replace": False}, settle=True)
    listed = hand.ask("zengine.inventory", "InventoryList", {}, version=2)
    for e in listed["entries"]:
        pair = hand.ask("zengine.inventory", "InventoryRead", {"reference": e["reference"]})["pair"]
        state["fixtures"].append({"reference": e["reference"], "revision": e["revision"],
                                  "label": e["label"], "folder": e["folder"],
                                  "pair": base64.b64encode(pair).decode()})
    state["toolbox"] = path


def restore_fixtures(hand, state, known):
    """Return each owned entry to its baseline; entries not owned by the setup are never touched."""
    listed = hand.ask("zengine.inventory", "InventoryList", {}, version=2)
    owner = listed["owner"]
    for fixture in known:
        current = next((e for e in listed["entries"] if e["reference"] == fixture["reference"]), None)
        if current is None:
            entry = hand.ask("zengine.inventory", "InventoryAdd", {
                "pair": base64.b64decode(fixture["pair"]), "label": fixture["label"],
                "folder": {"owner": owner, "folder": fixture.get("folder", "")}}, settle=True, version=2)
            fixture["reference"], fixture["revision"] = entry["reference"], entry["revision"]
            continue
        # Revisions fence value and label; an entry at the revision restored last time is baseline.
        if current["revision"] != fixture.get("revision") or current["label"] != fixture["label"]:
            result = hand.ask("zengine.inventory", "InventoryWrite", {
                "reference": current["reference"], "revision": current["revision"],
                "pair": base64.b64decode(fixture["pair"])}, settle=True)
            if current["label"] != fixture["label"]:
                result = hand.ask("zengine.inventory", "InventoryRename", {
                    "reference": current["reference"], "revision": result["revision"],
                    "label": fixture["label"]}, settle=True)
            fixture["revision"] = result["revision"]
        if current["folder"] != fixture.get("folder", ""):
            hand.ask("zengine.inventory", "InventoryFile", {
                "reference": current["reference"], "from": {"owner": owner, "folder": current["folder"]},
                "into": {"owner": owner, "folder": fixture.get("folder", "")}}, settle=True)


# ---- hotkeys: each declared view restored, its entries bound as declared, then switched on --------
def hotkey_entries(hand, setup, state):
    """(declaration, reference) per declared hotkey, found among the setup's own entries."""
    from workshop_steps import chord
    out = []
    for hk in setup.hotkeys():
        owned = [f for f in state["fixtures"] if f["label"] == hk["entry"]]
        hand.ctx.check(len(owned) == 1, "hotkey %s names %r, which is not exactly one of this "
                       "setup's entries" % (hk["key"], hk["entry"]))
        out.append((hk, owned[0]["reference"], chord(hk["key"])))
    return out


def hold_off(hand, pairs, owned, current, state):
    """Switch off each of the setup's items that is enabled but not yet in its own view with its
    declared chord and target, before anything moves or is rebound. Inventory judges every edit by
    the keys it would leave live, so carrying a live item into an ON view, or rebinding it where it
    is live, could meet a key only this halfway arrangement has (a command rebound to Alt+2, moved
    back into the Alt+1 row beside someone else's live Alt+2). Switching off only removes keys, so
    Inventory admits it; activate_hotkeys switches each one back on. state["held_off"] names the
    keys still off, so a refusal after this point says what it left OFF."""
    held = state.setdefault("held_off", [])
    for hk, ref, (code, mods) in pairs:
        bound = next((b for b in current["bindings"] if b["reference"] == ref), None)
        if not bound or not bound["enabled"]:
            continue
        home = next((v for v in current["views"] if v["id"] == owned.get(hk.get("view", "row"))), None)
        declared = (bound["target"], bound["scancode"], bound["modifiers"]) == (hk["target"], code, mods)
        if home and ref in home["entries"] and declared:
            continue
        hand.edit(operation="enable", entry=ref, enabled=False)
        if hk["key"] not in held:
            held.append(hk["key"])


def place_hotkeys(hand, setup, state):
    """Each view the hotkeys declare holds exactly its declared entries, in order, each bound to its
    declared chord and target. The setup owns the views it made or first adopted (state["views"]);
    another entry found in one goes back to main Inventory, unchanged. Inventory keeps a removed
    entry's place and binding and its door cannot touch them, so an owned view holding an entry
    Inventory no longer names is retired -- turned OFF, left in Inventory -- and replaced. Views the
    setup does not own, and their switches, are never changed. Nothing runs, and no key is made
    live here: the setup's own items that will move or be rebound are switched off first
    (hold_off), so each edit leaves live only keys that were live before it or will be after
    activation, and Inventory refuses only a conflict the declared arrangement itself has."""
    pairs = hotkey_entries(hand, setup, state)
    owned = state.setdefault("views", {})
    retired = state.setdefault("retired_views", [])
    named = [e["reference"] for e in hand.ask("zengine.inventory", "InventoryList", {}, version=2)["entries"]]
    current = hand.views()
    hold_off(hand, pairs, owned, current, state)
    views = current["views"]
    for kind, _ in setup.views():
        refs = [ref for hk, ref, _ in pairs if hk.get("view", "row") == kind]
        if kind in owned:
            view = next((v for v in views if v["id"] == owned[kind]), None)
        else:
            # The first preparation adopts the view its material already put them in (a toolbox's row).
            view = next((v for v in views if v["kind"] == kind and refs[0] in v["entries"]), None)
        if view and any(r not in named for r in view["entries"]):
            if view["active"]:
                hand.edit(operation="context", view=view["id"], enabled=False)
            retired.append(view["id"])
            view = None
        if view is None:
            hand.edit(operation="create", text=kind, entry=refs[0])
            views = hand.views()["views"]
            view = next(v for v in views if refs[0] in v["entries"])
        owned[kind] = view["id"]
        if view["entries"] != refs:
            for other in view["entries"]:
                if other not in refs:
                    hand.edit(operation="move", view="inventory", entry=other)
            for ref in refs:  # each move appends, so the view ends in declared order
                hand.edit(operation="move", view=view["id"], entry=ref)
            views = hand.views()["views"]
    bindings = hand.views()["bindings"]
    for hk, ref, (code, mods) in pairs:
        bound = next((b for b in bindings if b["reference"] == ref), None)
        if not bound or (bound["target"], bound["scancode"], bound["modifiers"]) != (hk["target"], code, mods):
            hand.edit(operation="bind", entry=ref, text=hk["target"], scancode=code, modifiers=mods)
    return hand.views()


def holders(hand, ref, code, mods):
    """Where else an enabled item holds this chord: the view and whether it is ON."""
    views = hand.views()
    out = []
    for b in views["bindings"]:
        if b["reference"] != ref and b["enabled"] and (b["scancode"], b["modifiers"]) == (code, mods):
            view = next((v for v in views["views"] if b["reference"] in v["entries"]), None)
            out.append("%s (%s)" % (view["id"], "ON" if view["active"] else "OFF") if view else "main Inventory")
    return "; the chord is also enabled in %s" % ", ".join(out) if out else ""


def activate_hotkeys(hand, setup, state):
    """The explicit activation the description declares: each item, then each view the setup owns."""
    pairs = hotkey_entries(hand, setup, state)
    held = state.setdefault("held_off", [])
    for hk, ref, (code, mods) in pairs:
        try:
            hand.edit(operation="enable", entry=ref, enabled=True)
        except Exception as refused:
            hand.ctx.check(False, "hotkey %s (%s) not switched on%s. Inventory: %s"
                           % (hk["key"], hk["entry"], holders(hand, ref, code, mods), refused))
        if hk["key"] in held:
            held.remove(hk["key"])
    views = hand.views()
    for kind, group in setup.views():
        view = next(v for v in views["views"] if v["id"] == state["views"][kind])
        if not view["active"]:
            try:
                hand.edit(operation="context", view=view["id"], enabled=True)
            except Exception as refused:
                hk, ref, (code, mods) = next(p for p in pairs if p[0] is group[0])
                hand.ctx.check(False, "hotkeys of %s: the view not switched ON%s. Inventory: %s"
                               % (view["id"], holders(hand, ref, code, mods), refused))
    views = hand.views()
    for hk, ref, (code, mods) in pairs:
        bound = next((b for b in views["bindings"] if b["reference"] == ref), None)
        view = next(v for v in views["views"] if v["id"] == state["views"][hk.get("view", "row")])
        hand.ctx.check(bound and bound["enabled"] and (bound["target"], bound["scancode"], bound["modifiers"]) ==
                       (hk["target"], code, mods) and ref in view["entries"] and view["active"],
                       "hotkey %s did not read back ON in %s" % (hk["key"], view["id"]))
    state.pop("held_off", None)
    return views


# ---- the desk, and the providers it needs ---------------------------------------------------------
def seated_views(hand, setup, state):
    """The portable views the desk seats, slot by slot: each declared view in its own slot, then
    (for a setup whose story makes them) other views holding entries, in Inventory's order."""
    if not setup.get("view_slots"):
        return []
    owned = [state["views"][kind] for kind, _ in setup.views()]
    skip = set(owned) | set(state.get("retired_views", []))
    return owned + [v["id"] for v in hand.views()["views"] if v["entries"] and v["id"] not in skip]


def apply(hand, desk):
    hand.ask("zengine.workshop", "SetupApplyRequested", {"setup": json.dumps(desk)}, settle=True)


def without_pane(desk, provider, pane):
    """`desk` less one pane, its front ranks renumbered."""
    desk = json.loads(json.dumps(desk))
    desk["fields"]["panes"] = [p for p in desk["fields"]["panes"]
                               if (p["provider"], p["pane"]) != (provider, pane)]
    for i, p in enumerate(desk["fields"]["panes"]):
        p["front"] = str(i)
    return desk


def seat_desk(ctx, hand, setup, state, link):
    """The whole desk. A pane Workshop cannot present is prepared when the setup says how; one the
    setup does not prepare (a described view nobody has run yet, say) is left off the desk and
    named in state["unseated"], since the rest of the desk is usable without it."""
    desk = setup.desk(seated_views(hand, setup, state))
    roles = [p["role"] for p in setup.providers()]
    state["unseated"] = []
    for _ in range(len(desk["fields"]["panes"]) + 1):
        try:
            apply(hand, desk)
            return desk
        except Exception as refused:
            said = str(refused)
            if CANNOT_PRESENT not in said:
                raise
            provider, pane = (said.split(CANNOT_PRESENT, 1)[1].split() + [""])[:2]
            if provider in roles:
                break
            state["unseated"].append("%s %s" % (provider, pane))
            desk = without_pane(desk, provider, pane)
    stage(state, "desk without " + ", ".join(roles))
    apply(hand, setup.without(desk, [p["role"] for p in setup.providers()]))
    for provider in setup.providers():
        stage(state, "build " + provider["role"])
        import builder
        ctx.step("build the project frontier for " + provider["role"])
        summary = builder.perform(ctx, {"act": "frontier", "link": link, "realize": "realized",
                                        "seconds": provider.get("seconds", 900)}, name="setup-build")
        state["built"] = summary
    stage(state, "desk")
    for attempt in range(40):
        try:
            apply(hand, desk)
            state["desk_attempts"] = attempt + 1
            return desk
        except Exception as refused:
            if CANNOT_PRESENT not in str(refused) or attempt == 39:
                raise
            time.sleep(0.25)  # the realized weave offers its pane on its own turn


def prepare(ctx, setup, state, link):
    """One preparation: first start or Reset. Restores only what the setup owns; other entries,
    including the weaver's copies, survive. Raises with the owner's words when an owner refuses."""
    hand = Owners(ctx, link)
    state.setdefault("fixtures", [])
    material = setup.material()
    stage(state, "revision")
    # The description and desk are the revision loaded; a file still to be read must be too.
    if "toolbox" in material and not state.get("toolbox"):
        ctx.check(setup.unchanged(material["toolbox"]), "%s changed since this setup was loaded; "
                  "nothing was changed. Preparation uses the revision it loaded: start a new root "
                  "for the change" % material["toolbox"])
    stage(state, "pane resets")
    # Refuse a pending owner operation before changing any stored value.
    for provider, pane in setup.resets():
        hand.ask(provider, "PaneResetRequested", {"pane": pane}, settle=True)
    stage(state, "material")
    known = list(state["fixtures"])
    if "capture" in material:
        capture_material(hand, material["capture"], state)
    elif "toolbox" in material and not state.get("toolbox"):
        toolbox_material(hand, setup, state)
        known = []
    if known:
        restore_fixtures(hand, state, known)
    if setup.hotkeys():
        stage(state, "hotkey placement")
        place_hotkeys(hand, setup, state)
    stage(state, "desk")
    desk = seat_desk(ctx, hand, setup, state, link)
    if setup.hotkeys():
        stage(state, "hotkey activation")
        activate_hotkeys(hand, setup, state)
    if setup.get("starting"):
        stage(state, "starting steps")
        from act import act, VERBS
        from hand import Hand
        weaver = Hand(ctx, link)
        try:
            for step in setup.get("starting"):
                act(ctx, weaver, next(v for v in VERBS if v in step), step)
        finally:
            weaver.close()
    stage(state, "readiness")
    # The visible reading comes from Workshop, not a private model of its typography. Workshop's
    # own panes (Layouts) are painted by Workshop itself and have no provider to wait for.
    for row in desk["fields"]["panes"]:
        if row["provider"] != WORKSHOP:
            shown(ctx, hand, row["provider"], row["pane"])
    stage(state, "ready")


def shown(ctx, hand, provider, pane):
    """Wait, a bounded while, until Workshop shows the pane: text rows, or a picture it draws."""
    deadline, why = time.monotonic() + READY_SECONDS, "no rows"
    while True:
        try:
            if hand.view(provider, pane)["rows"]:
                return
            why = "no rows"
        except Exception as refused:
            if DRAWS_PICTURE in str(refused):
                return
            why = str(refused)
        ctx.check(time.monotonic() < deadline, "a desk pane is not ready after %ds: %s %s (%s)"
                  % (READY_SECONDS, provider, pane, why))
        time.sleep(0.2)


def ready_note(setup, state):
    """What a ready demo says: the first task, and any pane the desk could not seat."""
    note = "Ready. %s" % setup.get("first_task")["do"]
    if state.get("unseated"):
        note += " Not on the desk: %s, which this Workshop does not offer yet." % ", ".join(state["unseated"])
    return note


def failure_note(error, state):
    """What a failed preparation says: where it stopped and which of its keys it left OFF first, then
    the owner's words, so the facts survive the controls' short note (256 bytes)."""
    held = state.get("held_off")
    return "Failed at %s%s: %s" % (state.get("reached", "start"), "; this setup's %s left OFF until a "
                                   "Reset completes" % ", ".join(held) if held else "", error)


def serve(ctx):
    link = ctx.inputs["link"]
    setup = described.load(ctx.inputs["setup"])
    state = {"setup": setup.name, "directory": str(setup.root), "digest": setup.digest(),
             "fixtures": [], "samples": []}
    ctx.ask(ROLE, "DemoServiceOpened", {"name": setup.get("title")[:64]}, via=link)
    ctx.on_cleanup(lambda: ctx.ask(ROLE, "DemoServiceClosed", {}, via=link, timeout=10),
                   "detach the reset service")
    while True:
        ctx.step("waiting for initial setup or Reset demo")
        work = ctx.ask(ROLE, "DemoWorkRequested", {}, via=link, timeout=86400)
        started = time.monotonic()
        passed = True
        ctx.step("prepare " + setup.name)
        measured = Measured(ctx)
        try:
            prepare(measured, setup, state, link)
            note = ready_note(setup, state)
        except Exception as error:
            passed, note = False, failure_note(error, state)
        state["samples"].append({"generation": work["generation"], "passed": passed,
                                 "elapsed_ms": (time.monotonic() - started) * 1000,
                                 "note": note, "reached": state.get("reached"),
                                 "unseated": list(state.get("unseated", [])),
                                 "request_calls": dict(measured.calls),
                                 "outcomes": dict(measured.outcomes)})
        state["samples"] = state["samples"][-64:]
        ctx.produce("setup.json", json.dumps(state, indent=2, default=str).encode())
        ctx.ask(ROLE, "DemoWorkFinished", {"generation": work["generation"], "passed": passed,
                                          "note": note}, via=link, settle=True)


def reset(ctx):
    started = time.monotonic()
    ctx.ask(ROLE, "DemoResetRequested", {}, via=ctx.inputs["link"], timeout=float(ctx.inputs.get("seconds", 600)))
    status = ctx.ask(ROLE, "DemoStatusRequested", {}, via=ctx.inputs["link"])
    ctx.check(status["state"] == "ready", "reset did not produce a ready demo")
    ctx.produce("reset.json", json.dumps({"status": status.fields,
                "elapsed_ms": (time.monotonic() - started) * 1000}, indent=2).encode())
    return "Demo reset completed; unrelated entries and run evidence were retained."


def status(ctx):
    if ctx.inputs.get("wait", False):
        result = ctx.ask(ROLE, "DemoReadyRequested", {"generation": 1}, via=ctx.inputs["link"],
                         timeout=float(ctx.inputs.get("seconds", 600)))
    else:
        result = ctx.ask(ROLE, "DemoStatusRequested", {}, via=ctx.inputs["link"])
    ctx.check(result["state"] == "ready" or not ctx.inputs.get("wait", False), result["note"])
    ctx.produce("status.json", json.dumps(result.fields, indent=2).encode())
    return result["state"] + ": " + result["note"]
