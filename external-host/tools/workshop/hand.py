# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Small orchestration helpers. Zengine owns geometry, motion, drops and permissions."""
import json
import time
from workshop_steps import moment, chord_moments, clear_moments


class Hand:
    def __init__(self, ctx, link):
        self.ctx, self.link = ctx, link
        self.session = self.ask("zengine.input", "InputSessionRequested",
                                {"purpose": "inventory composition demo"})["session"]
        self.open = True
        ctx.on_cleanup(self.close, "release the demo input session")

    def ask(self, role, shape, fields, **kw):
        return self.ctx.ask(role, shape, fields, via=self.link, **kw)

    def close(self):
        if self.open:
            self.ask("zengine.input", "InputSessionClosed", {"session": self.session, "holder": 0})
            self.open = False

    def inject(self, events):
        answer = self.ask("zengine.input", "InjectInput",
                          {"session": self.session, "events": events}, settle=True)
        self.ctx.check(answer["session"] == self.session and answer["admitted"] == len(events),
                       "input did not admit the requested moments")

    def view(self, provider, pane):
        return self.ask("zengine.workshop", "PaneViewRequested", {"provider": provider, "pane": pane})

    def row(self, provider, pane, contains, scroll=False):
        # Read the current view, then search toward each edge with ordinary wheel input.
        # A stable view is the edge, not a reason to retry it.
        for direction in ((1, -1) if scroll else (0,)):
            previous = None
            for _ in range(256):
                view = self.view(provider, pane)
                rows = [r for r in view["rows"] if contains in r["text"]]
                if len(rows) == 1:
                    return rows[0]
                self.ctx.check(not rows, "ambiguous visible row: " + contains)
                visible = [(r["row"], r["text"]) for r in view["rows"]]
                if not direction or not visible or visible == previous:
                    break
                previous = visible
                at = view["rows"][0]
                self.inject([moment(self.ctx, "PointerWheel", wheel_dy=direction,
                                    x=at["x"], y=at["y"], space=at["space"])])
        self.ctx.produce("last-view.json", json.dumps({"picture": view["picture"], "rows": view["rows"]}, indent=2).encode())
        raise ValueError("visible row not found: " + contains)

    def click(self, row):
        self.inject([moment(self.ctx, "PointerButton", button=1, pressed=p,
                            x=row["x"], y=row["y"], space=row["space"]) for p in (True, False)])

    def key(self, chord):
        self.inject(chord_moments(self.ctx, chord))

    def text(self, text):
        self.inject(clear_moments(self.ctx) + [moment(self.ctx, "TextEntered", text=text)])

    def drag(self, start, end, duration_ms=900, bend=0, during=None):
        self.ctx.check(start["space"] == end["space"], "drag endpoints use different spaces")
        self.inject([moment(self.ctx, "PointerButton", button=1, pressed=True,
                            x=start["x"], y=start["y"], space=start["space"])])
        fields = {"session": self.session, "x": end["x"], "y": end["y"],
                  "duration_ms": duration_ms, "bend": float(bend)}
        if during is None:
            moved = self.ask("zengine.input", "PointerMotionRequested", fields,
                             settle=True, timeout=duration_ms / 1000 + 10)
        else:
            pending = self.ctx.ask_async("zengine.input", "PointerMotionRequested", fields,
                                         via=self.link, settle=True)
            time.sleep(duration_ms / 2000)  # observation only; Zengine generates all samples
            during()
            moved = pending.wait(duration_ms / 1000 + 10)
        self.ctx.check(moved["session"] == self.session and moved["admitted"] > 0,
                       "pointer motion did not reach its endpoint")
        self.inject([moment(self.ctx, "PointerButton", button=1, pressed=False,
                            x=end["x"], y=end["y"], space=end["space"])])
        return moved
