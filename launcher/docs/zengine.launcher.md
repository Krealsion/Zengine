# zengine.launcher -- the launcher

**Manual.** The weave that starts an app made in Workshop, or a demo, as a process of its own that
goes on after Workshop quits. It answers to the office `zengine.launcher` (an office is the name a
weave answers to) and is built as `zengine-launcher`, a row of Workshop's load plans. What it may
start is a catalog fixed when Zengine was configured: no message names a program, a path or an
argument.

## About

The catalog holds two kinds:

- **an app**: a folder directly in the project's `apps/`, holding a plan Workshop's load-plan codec
  reads, as the View Builder's [Export app](../../view/docs/zengine.view.builder.md#viewexport)
  writes one. It is started by the `zengine-app` beside Workshop's program, on that folder
  ([running an app on its own](../../app/docs/app.md)).
- **a demo**: a setup the demo service lists, by its name. It is started by the interpreter and
  the `external-host/demo.py` this build was configured with, on a root of its own,
  `<project>/demo-runs/<name>`, against this build and its Loom. A build configured with no Python
  or no Loom session tools has no demo service, and starts no demo.

A start is detached: on Windows the process has a console and a process group of its own, inherits
no handle, and leaves Workshop's job where the job allows it; on POSIX it leads a session of its
own, reads nothing, appends what it writes to `app.log` in an app's folder, or
`demo-runs/<name>.log` for a demo, and holds no descriptor of Workshop's but those three. The
launcher keeps nothing of it: its answer is the process's id, which says it started, not that it
runs. Each start is also said to any office as it is answered, so a pane that did not ask -- a
checklist, say -- hears that something started.

**Whether an app runs** is asked of the system each time a listing is asked, never remembered: an
app's host writes its process id in the app's folder while it runs, `app-session.json`, and removes
it when it ends ([running an app on its own](../../app/docs/app.md#while-it-runs)). The app runs
while that file stands and the process it names still runs the `zengine-app` beside Workshop; a
host that was killed leaves its file, naming a process that is gone.

At its start the launcher asks `zengine.project` where the project is; a start asked before that
answer is refused.

## Use

The View Builder's **Run app**, Files' **start** and the palette's setups and apps
([`zengine.palette`](../../workshop/docs/zengine.palette.md)) ask it, each after Workshop has allowed
the hand that pressed, as a Save and a Build are judged: classes `build` and `write`, so on a
weaver's host a guest starts nothing. It takes a start only from those three offices and from the
quick setup (`zengine.first-run`), admitted to ask it the same way.
Any office may ask what it can start.

## Commands

### `Launch`

`Launch` v1 asks for one start from the catalog.

- **Takes:** `kind`, `app` or `demo`; `name`, the app's folder or the setup's name, at most 64
  bytes.
- **Answers:** `Launched` v1: `ok`, the `kind` and `name` asked, the process's `pid`, and `reason`,
  `started app 'tally', process 4242`.
- **Refuses:** in `Launched`, with `ok` false and nothing started: `only the View Builder, Files, the
  palette and the quick setup ask the launcher for a start`; `the launcher does not know the project yet; ask again`; `the launcher starts
  an app or a demo, not '<kind>'`; for an app, a name that is not a plain folder name, `no app
  'tally' in <project>/apps`, `'tally' is not a folder directly in <project>/apps` (a link to
  elsewhere), `the app 'tally' has no plan this Workshop reads: <why>`, `no zengine-app beside this
  Workshop`; for a demo, `this Workshop starts no demo: it was configured with no demo service`,
  `no demo '<name>' among the setups the demo service lists`, `the demo service this Workshop was
  configured with is not there`; and the system's reason when the process could not start.
- **Example:** after Export app wrote `apps/tally/`, the View Builder asks `Launch{"app", "tally"}`.

### `LaunchCatalogRequested`

`LaunchCatalogRequested` v1 asks what the launcher can start now, from any office.

- **Takes:** nothing.
- **Answers:** `LaunchCatalog` v1: `entries`, each a `LaunchEntry` v1 -- `kind` and `name` as
  `Launch` takes them, `title` and `line` (a setup's own, from its `setup.json`; an app's title is
  its name), and `running`, for an app its host runs now -- the setups the demo service lists
  first, then each app in the project's `apps/` that `Launch` would start, by name; `left_out`, how
  many past the <!-- value kMaxListed -->128<!-- /value --> a listing holds; and `note`, why no app is listed while the launcher does not
  know the project yet.
- **Refuses:** nothing: a folder in `apps/` that `Launch` would refuse is not listed.
- **Example:** in a project holding `apps/tally/`, the answer lists the setups, then `app`
  `tally`, `running` while its `zengine-app` runs.

## Hears

### `ProjectRoot`

`ProjectRoot` v1, the project office's answer to the launcher's own ask at its start: where the
project is. Apps and demo roots are found there.

## Follow

Each start, as it is answered, is said to any as the launcher's office: the same `Launched` v1,
`ok` true. A refused start is answered to its asker alone. What it started says for itself after
that. On Windows an app opens its console and its window. On POSIX it has no console: a row
of its plan no console has decided ends it at once, exit 5, said in its `app.log`, so an app's
first start is from a terminal, its `run.sh`, where its console decides its rows; a start from
Workshop after that runs. Each start's notice says which of these follows.

## See also

- [Running an app on its own](../../app/docs/app.md): the app it starts, and its console.
- [Demo setups](../../workshop/docs/demo-setups.md): the demo service and its setups.
- The shapes: [launcher/vocabulary.hpp](../vocabulary.hpp).
