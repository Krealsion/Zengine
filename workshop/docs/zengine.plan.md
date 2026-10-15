# zengine.plan -- the plan office

**Manual.** Workshop's office that changes which artifacts this project runs: it adds one row to the
project's load plan, loads it into the running project, and writes it to the plan file. It is a
door inside Workshop, answering to the office `zengine.plan`; it is not a file you load. The
Builder pane reaches it from its role line.

## About

The *load plan* says which artifacts a project runs and as what
([choosing what a run is made of](load-plans.md)). A row added here goes first to the running
project, whose realization owner, [`zengine.realization`](zengine.realization.md), takes it or
refuses it; only a row it took is written to the file. How an artifact is built is not the plan's
but the recipes' ([`zengine.recipes`](zengine.recipes.md)).

## Use

Ask as an office: an ask sent as nobody is counted and answered by no one. The answer is to the
asker alone.

## Commands

### `PlanRowRequested`

`PlanRowRequested` v1 adds the row `{ artifact, role }` to the running project, then to its plan
file.

- **Takes:** `stem`, the artifact; `role`, the office it is loaded as; `recipe`, the recipe that
  makes it, so the answer can say whether that is built.
- **Answers:** `PlanRowWritten` v1: `accepted`; `detail`, what the running project made of the row;
  `frontier`, true when the new row is what the project now waits on; `product`, where the recipe's
  file is when the row is the frontier and the file is already built; `path`, the plan file written.
- **Refuses:** in `refusal`: `this host cannot author plan rows`; the plan law's words for the row or
  the plan; `this run began nowhere -- a plan row needs a project to be written into`; `this host
  has no realization owner to append to`; the running project's refusal, as `realization is not
  between rows: it is still performing the authored plan`. A refused row is not written.
- **Example:** the Builder pane's role line, `{stem: "tally", role: "example.tally", recipe:
  "tally"}`.

## Files

Writes `workshop-plan.json` in the directory Workshop was launched in; its format is
[the authored load plan](load-plan.md)'s.

## See also

- [`zengine.realization`](zengine.realization.md), which performs the plan.
- [`zengine.project`](zengine.project.md), which answers whether the plan names an artifact.
