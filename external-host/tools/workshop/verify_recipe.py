# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""workshop/verify-recipe -- read one recipe back from the project's own build-recipes.json and
check the fields a caller names against it (loom-tool.json describes each input; this file is
the read and the checks).

WHAT THIS IS: a LOCAL FILESYSTEM READ of the file `zengine-workshop`'s own recipe writer saves
to (`workshop/recipe_persist.hpp`), opened directly by this tool -- never through `zengine.input`,
`zengine.skin` or any other guest door, and never an ask Workshop answers. It exists because
"the picture changed" and "an input session admitted N moments" are not the same claim as "the
SPECIFIC recipe a case meant to author was written with the SPECIFIC fields that case meant" --
a chooser presenting the wrong candidate, a field's suggested default surviving uncleared, or a
menu choice landing on a neighbour field would all still let a picture change and an injection
settle.

WHAT THIS IS NOT: authority. A file already on disk before this tool ran, an unrelated writer,
or a stale copy from an earlier run would read back identically to a fresh write from the
believed session -- this tool cannot and does not distinguish them. It answers "does the file
on disk now hold these fields", not "did Workshop just write them because of what I sent it".
A caller that needs the second claim pairs this with the surrounding tool's own settlement
(`inspect_capture.py`'s `InjectInput` answer, which names moments admitted and dispatched) and,
where the run's own directory can hold one, a fact this tool cannot manufacture -- a before/after
comparison, or an mtime newer than the batch that is believed to have caused the write.
"""

import json
import os

kFormat = "zengine-build-recipes"


def run(ctx):
    project = ctx.inputs["project"]
    recipes_file = ctx.inputs.get("recipes_file") or "build-recipes.json"
    path = os.path.join(project, recipes_file)
    ctx.step("read %s" % path)
    ctx.check(os.path.isfile(path),
              "no recipes file at %s -- this tool reads a file already on disk; it does not "
              "ask Workshop for one and cannot wait for a write that has not happened" % path)
    with open(path, "r", encoding="utf-8") as fh:
        raw = fh.read()
    try:
        envelope = json.loads(raw)
    except ValueError as err:
        ctx.fail("%s is not valid JSON: %s" % (path, err))
    fields = envelope.get("fields") if isinstance(envelope, dict) else None
    ctx.check(isinstance(fields, dict) and fields.get("format") == kFormat,
              "%s does not read as a `%s` file -- its own `fields.format` says `%s`" %
              (path, kFormat, None if not isinstance(fields, dict) else fields.get("format")))
    recipes = fields.get("recipes")
    ctx.check(isinstance(recipes, list), "%s's `fields.recipes` is not a list" % path)

    recipe_id = ctx.inputs["recipe"]
    matches = [r for r in recipes if isinstance(r, dict) and r.get("recipe") == recipe_id]
    ctx.check(len(matches) == 1,
              "%d recipe(s) named `%s` in %s, not exactly one (%s)" %
              (len(matches), recipe_id, path,
               ", ".join(sorted(r.get("recipe", "?") for r in recipes if isinstance(r, dict)))))
    row = matches[0]
    ctx.produce("recipe.json", json.dumps(row, indent=1, sort_keys=True).encode("utf-8"))

    checked = []
    mismatches = []

    def want(field, actual, expected):
        checked.append(field)
        if actual != expected:
            mismatches.append("%s: file has `%s`, expected `%s`" % (field, actual, expected))

    for top_field in ("artifact", "artifact_dir"):
        expected = ctx.inputs.get(top_field)
        if expected or ctx.inputs.get(top_field + "_blank"):
            want(top_field, row.get(top_field, ""), expected or "")

    cmake_wanted = {k: ctx.inputs.get("cmake_" + k) for k in
                    ("target", "config", "build_dir", "entry")}
    cmake_blank = {k: ctx.inputs.get("cmake_" + k + "_blank") for k in cmake_wanted}
    if any(cmake_wanted.values()) or any(cmake_blank.values()):
        cmake_target = row.get("cmake_target") or []
        ctx.check(len(cmake_target) == 1,
                  "recipe `%s` has %d `cmake_target` row(s) in %s, not exactly one" %
                  (recipe_id, len(cmake_target), path))
        ct = cmake_target[0]
        for key, expected in cmake_wanted.items():
            if expected or cmake_blank[key]:
                want("cmake_target.%s" % key, ct.get(key, ""), expected or "")

    ctx.check(checked, "asked to verify `%s` in %s but named no field to check it against -- "
                       "every `verify-recipe` run must name at least one" % (recipe_id, path))
    ctx.check(not mismatches,
              "recipe `%s` in %s does not match: %s" %
              (recipe_id, path, "; ".join(mismatches)))
    return ("recipe `%s` in %s matches every field asked for (%s) -- a LOCAL FILE OBSERVATION, "
            "not Workshop's own word that it was the one writing" %
            (recipe_id, path, ", ".join(checked)))
