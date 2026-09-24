# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""One owner operation saves or restores a durable Workshop toolbox."""
import json
import time


def run(ctx):
    operation = ctx.inputs["operation"]
    ctx.check(operation in ("save", "restore"), "operation must be save or restore")
    path = ctx.inputs["path"]
    ctx.check(bool(path), "choose a file path on the Workshop host")
    fields = {"path": path}
    if operation == "restore":
        fields["replace"] = ctx.inputs["replace"]
    ctx.step(operation + " toolbox")
    started = time.monotonic()
    answer = ctx.ask("zengine.inventory-pane", "InventoryToolbox" + operation.title(), fields,
                     via=ctx.inputs["link"], settle=True)
    ctx.check(answer["operation"] == operation and answer["entries"] >= 0,
              "unexpected toolbox completion; inspect Workshop before repeating")
    result = dict(answer.fields, elapsed_ms=(time.monotonic() - started) * 1000, request_calls=1)
    ctx.produce("toolbox.json", json.dumps(result, indent=2).encode())
    return "%s: %s entries, one remote request (%s)" % (operation, answer["entries"], answer["path"])
