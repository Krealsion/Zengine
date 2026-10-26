# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""The demo owner's asks, `zengine.demo`'s reset and status, and the counted ask its journeys keep:
what a tool asks of a prepared demo, by message alone. Preparing a setup, which reads its files,
is demo_setup.py's, in the tools that reach the machine."""
from collections import Counter
import json
import time

ROLE = "zengine.demo"


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


def reset(ctx):
    started = time.monotonic()
    ctx.ask(ROLE, "DemoResetRequested", {}, via=ctx.inputs["link"], timeout=float(ctx.inputs.get("seconds", 600)))
    status = ctx.ask(ROLE, "DemoStatusRequested", {}, via=ctx.inputs["link"])
    ctx.check(status["state"] == "ready", "reset did not produce a ready demo")
    ctx.produce("reset.json", json.dumps({"status": status.fields,
                "elapsed_ms": (time.monotonic() - started) * 1000}, indent=2).encode())
    return "Demo reset completed; unrelated entries and run evidence were retained."


def status(ctx):
    from workshop_steps import plan_at_rest
    # Asked before the plan is at rest, the demo's control may not be declared yet.
    plan_at_rest(ctx, ctx.inputs["link"])
    if ctx.inputs.get("wait", False):
        result = ctx.ask(ROLE, "DemoReadyRequested", {"generation": 1}, via=ctx.inputs["link"],
                         timeout=float(ctx.inputs.get("seconds", 600)))
    else:
        result = ctx.ask(ROLE, "DemoStatusRequested", {}, via=ctx.inputs["link"])
    ctx.check(result["state"] == "ready" or not ctx.inputs.get("wait", False), result["note"])
    ctx.produce("status.json", json.dumps(result.fields, indent=2).encode())
    return result["state"] + ": " + result["note"]
