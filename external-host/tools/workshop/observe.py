# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""workshop/observe -- follow what one office of a running Workshop publishes, for a while, and
keep every word: what an agent reads to learn what an owner says before writing a policy about it,
and the plainest proof of what a subscription promises.

It subscribes through the link to the office's shapes (the guest's row must list them under
`observe`), takes words until `seconds` pass, `count` observations arrived or the subscription
ends, releases it, and keeps them all in observed.json with the subscription's own account. It
presses nothing and holds no input session, so it may run beside a run that does.

`read_pause_ms` makes it a deliberately SLOW reader, and `window` a small one: the relay then
holds back, drops and counts, and says each loss as a Gap -- which this tool counts, never hides.
A `latest` shape is state, and while the window is shut only its newest is kept (`coalesced`)."""
import json
import time

from loom_session.tool import Refused


def run(ctx):
    link = ctx.inputs.get("link", "workshop")
    producer = ctx.inputs["producer"]
    ctx.check(producer, "producer names the office whose publications to follow")
    shapes = [s.strip() for s in ctx.inputs.get("shapes", "").split(",") if s.strip()]
    ctx.check(shapes, "shapes names one or more Name@version, comma-separated")
    latest = [s for s in ctx.inputs.get("latest", "").split(",") if s]
    seconds = float(ctx.inputs.get("seconds", 30))
    count = int(ctx.inputs.get("count", 0) or 0)
    pause = max(0, int(ctx.inputs.get("read_pause_ms", 0) or 0)) / 1000.0
    try:
        sub = ctx.observe(producer, shapes, via=link, latest=latest,
                          window=int(ctx.inputs.get("window", 0) or 0),
                          label="workshop/observe (run %s)" % ctx.name)
    except Refused as err:
        ctx.fail("Workshop refused to let this session observe %s: %s" % (producer, err))
    began = time.monotonic()
    items, observed, gaps, lost = [], 0, 0, 0
    ended = None
    while time.monotonic() - began < seconds and not (count and observed >= count):
        item = sub.next(min(1.0, max(0.0, seconds - (time.monotonic() - began))))
        if item is None:
            if sub.ended is not None:
                break
            continue
        items.append(dict(item.record(), at=round(time.monotonic() - began, 3)))
        if item.kind == "observed":
            observed += 1
        elif item.kind == "gap":
            gaps += 1
            lost += item.lost
        else:
            ended = item.record()
            break
        if pause:
            time.sleep(pause)  # a slow reader, by request: the relay's bound is the subject
    if ended is None and sub.ended is None:
        ended = sub.release().record()
    summary = sub.summary()
    ctx.produce("observed.json", json.dumps({"producer": producer, "shapes": shapes,
                                             "latest": latest, "items": items,
                                             "subscription": summary, "ended": ended},
                                            indent=1).encode())
    return ("%d observation(s) of %s, %d gap(s) losing %d, %d coalesced into newer ones; ended %s"
            % (observed, producer, gaps, lost, sum(i.get("coalesced", 0) for i in items),
               (ended or {}).get("how", "?")))
