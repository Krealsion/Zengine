# Best practices

**Contributing.** What good work looks like in this repository, one practice to a bullet. Each
practice links its owner: the law, method or page that states the rule and the reasons for it,
which this page never restates. The [cheat sheet](../../cheat_sheet.md) is the operational
companion, the commands and the API at a glance, and [taking an issue](taking-an-issue.md) is the
path from an issue to a pull request.

Each section belongs to one surface and stands alone, naming its own owners, so that a section
can move into its surface's own document.

## Every change

### Scope and design

- **Read the owner before the code.** [AGENTS.md](../../AGENTS.md#read-this-then-route) routes
  each surface to the document that owns its law.
- **Intent decides what is right; a passing test shows what happens.**
  [Intent, evidence, and architectural fit](../../AGENTS.md#intent-evidence-and-architectural-fit)
- **Judge the whole result, and what it leaves the next consumer.**
  [Intent, evidence, and architectural fit](../../AGENTS.md#intent-evidence-and-architectural-fit)
- **Stay inside the issue's fences.** [Taking an issue](taking-an-issue.md#1-choose-a-ready-issue)
- **Write Loom, not host code.**
  [Intent, evidence, and architectural fit](../../AGENTS.md#intent-evidence-and-architectural-fit)
- **Preserve coherent ownership, and know who owns a behaviour before moving it.**
  [Intent, evidence, and architectural fit](../../AGENTS.md#intent-evidence-and-architectural-fit)
  and [the ownership map](../architecture/README.md#ownership-map)
- **Address a slot, not an instance; refuse rather than clamp; say absence, never guess it.**
  [Recurring principles](../architecture/README.md#recurring-principles)

### The witness

- **The case comes first, red on the unchanged code for the issue's reason.**
  [Taking an issue](taking-an-issue.md#6-a-test-that-fails-before-the-change)
- **A red for an incidental reason is not evidence.**
  [VM-MUT-19](../../agents/verification/mutation.md#vm-mut-19--a-red-for-an-incidental-reason-is-not-evidence)
- **Assert that the work happened before asserting what it said.**
  [VM-FIX-17](../../agents/verification/fixtures.md#vm-fix-17--assert-that-the-work-happened-before-asserting-what-it-said)
- **All caught means the cases bind the code, not that the code is right.**
  [VM-FIX-04](../../agents/verification/fixtures.md#vm-fix-04--a-matrix-cannot-find-what-the-code-and-the-cases-agree-on)
- **A mutation matrix starts with a canary proven red by hand.**
  [VM-MUT-01](../../agents/verification/mutation.md#vm-mut-01--canary-first)
- **A late answer is staged by enqueueing, never by sleeping.**
  [VM-FIX-24](../../agents/verification/fixtures.md#vm-fix-24--stage-a-late-answer-by-enqueueing-a-batch-never-by-sleeping)
- **A comma splits a doctest filter**: list the selection before trusting a run.
  [VM-POP-20](../../agents/verification/population.md#vm-pop-20--a-doctest-filter-splits-on-commas)
- **An assertion total is evidence, never a population.**
  [VM-POP-07](../../agents/verification/population.md#vm-pop-07--an-assertion-total-is-evidence-never-a-population)

### Verification

- **Run the lane the change can fail, and say which did not run.**
  [Which lane a change obliges](build-and-test.md#the-sanitizer-lane)
- **Quote the lane, never a bare `ctest`**: `cmake -DZEN_BUILD_DIR=build -P tests/verify.cmake`.
  [VM-LANE-01](../../agents/verification/lanes.md#vm-lane-01--quote-the-lane-never-a-bare-ctest)
- **A green names its repository, configuration and compiler.**
  [VM-LANE-03](../../agents/verification/lanes.md#vm-lane-03--a-green-names-its-repository-configuration-and-compiler)
- **A verdict is the check's own exit code**, written to a file, never read through a pipe.
  [VM-LANE-16](../../agents/verification/lanes.md#vm-lane-16--a-verdict-is-the-checks-own-exit-code)
- **A hosted run is read job by job.**
  [VM-LANE-05](../../agents/verification/lanes.md#vm-lane-05--a-job-under-continue-on-error-is-red-only-in-the-jobs-list)
- **A new CTest entry is a line in the population file, a case added raises its suite's floor, and
  a floor is never lowered to pass a deletion.** [The population contract](../../AGENTS.md#the-population-contract)
  and [VM-POP-05](../../agents/verification/population.md#vm-pop-05--floors-are-minimums-anchored-to-a-measured-baseline)

### Writing

- **A document states the present**, to a reader with the repository and nothing else.
  [Documentation conventions](repository-conventions.md#documentation-conventions)
- **A value the code owns is named by its owner, or held in a marker**:
  `cmake -DZEN_VALUES_WRITE=ON -P tests/check_code_values.cmake` rewrites the markers.
  [Values the code owns](repository-conventions.md#values-the-code-owns)
- **A comment stays only where a reader would otherwise make a mistake.**
  [Source comment conventions](repository-conventions.md#source-comment-conventions)
- **A case's name says what it proves.**
  [Source comment conventions](repository-conventions.md#source-comment-conventions)
- **A name carries its terminating condition.** [Naming](repository-conventions.md#naming)

### Delivery

- **An issue's comments are its record**: the taker claims it there, says the approach before
  building, and links the pull request on pushing.
  [Taking an issue](taking-an-issue.md#1-choose-a-ready-issue)
- **One issue is one branch and one pull request, which closes it with `Fixes #n`.**
  [Taking an issue](taking-an-issue.md#1-choose-a-ready-issue) and
  [the pull request](taking-an-issue.md#9-the-pull-request)
- **A commit is authored by the person who makes it, never the AI agent they use, with no
  co-author line and no AI credit**: `cmake -P tests/check_commit_attribution.cmake`.
  [Attribution](repository-conventions.md#attribution) and
  [the attribution guard](../../tests/check_commit_attribution.cmake)
- **The whole diff is read before the push.**
  [Taking an issue](taking-an-issue.md#8-the-checks-whose-green-counts)
- **A visible change shows its screenshots in the pull request.**
  [Taking an issue](taking-an-issue.md#9-the-pull-request)
- **One push, and its run read to its end before the next.**
  [Taking an issue](taking-an-issue.md#10-reading-the-hosted-run)

## Workshop and its panes

The host, `workshop/`, and the panes that reach it. Workshop's law is in the registers
[its router](../../agents/workshop.md#where-the-law-is) lists, what crosses the pane seam is
[the pane protocol's](../../agents/panes.md), a register's PROVEN BY names the cases that pin its
law, and the router of each folder, its `AGENTS.md`, names the suites. The Workshop entry
`workshop_<subject>` runs `build/tests/zengine-workshop-<subject>-tests`.

### Every pane

- **A new law is a new entry under its owner register, and the source keeps only its pointer.**
  [Ongoing rules](../../agents/workshop.md#ongoing-rules)
- **Changing a case a law cites re-verifies every law that cites it, in the same commit, and lists
  their ids in the commit message; renaming a case moves its citations with it.**
  [Ongoing rules](../../agents/workshop.md#ongoing-rules) and
  [Do not assume](../../agents/workshop.md#do-not-assume)
- **A published shape is frozen**: a new version goes beside it, never a field added in place.
  [A pane declares its actions](../../agents/panes.md#a-pane-declares-its-actions-and-the-host-dispatches-the-resolved-id)
- **A changed shape is a new version, and so is every shape that encloses it**; `tests/shapes.txt`
  pins them. [Do not assume](../../AGENTS.md#do-not-assume)
- **A press crosses as a place**, read against the picture the pane is showing.
  [A press crosses the seam as a place](../../agents/panes.md#a-press-crosses-the-seam-as-a-place-never-as-a-meaning)
- **Grants, picture maps and gestures stay out of reload state.**
  [A pane may draw locally](../../agents/panes.md#a-pane-may-draw-locally-with-an-explicit-room-and-gesture-identity)
- **A pane whose messages or state change cannot be reloaded in place.**
  [When a pane's messages change](../../workshop/docs/develop-workshop.md#when-a-panes-messages-change-a-new-runtime-too)
- **Routing and arbitration are Workshop's**; a pane never decides where input goes.
  [Where a cross-pane drag crosses a boundary](../architecture/README.md#where-a-cross-pane-drag-crosses-a-boundary)
- **A claim about routing, permission, layout or a reply is tested through the real pane.**
  [Testing a loaded pane](testing-workshop-panes.md)
- **Arrange exactly the boundary under proof.**
  [Arrange the same boundary](testing-workshop-panes.md#arrange-the-same-boundary-you-intend-to-prove)
- **A race is ordered by turns of the bus, never by sleeping.**
  [Put a message between a request and its answer](testing-workshop-panes.md#put-a-message-between-a-request-and-its-answer)
- **A test follows a delay or a refusal with the weaver's next act.**
  [Test what happens after the first outcome](testing-workshop-panes.md#test-what-happens-after-the-first-outcome)
- **Build the pane's own target with its suite; a focused run is not a green.**
  [Build and check only the relevant work](../../workshop/docs/make-a-workshop-tool.md#build-and-check-only-the-relevant-work-while-iterating)

### The host (`workshop/`)

Its law, pages and suites: [`workshop/AGENTS.md`](../../workshop/AGENTS.md).

- **A header declares**; the bodies are in the subject files, each `// WL-` pointer beside its
  body. [Where the law is](../../agents/workshop.md#where-the-law-is)
- **Read the ownership note before moving anything in `screen.hpp`; `weave.hpp`'s split by mode is
  not earned yet.** [Large source units](../architecture/README.md#large-source-units)
- **One geometry draws a thing and hits it.**
  [WL-GEO-01](../../agents/workshop/geometry.md#wl-geo-01--one-geometry-draws-a-thing-and-hits-it)
- **A layout test uses the ordinary setup operation**; a fixture that edits the setup directly
  reports a changed extent and restores it, then checks the rectangle.
  [Separate authored placement](testing-workshop-panes.md#separate-authored-placement-from-the-last-arranged-picture)

### Components (`component/`)

Its law, pages and suites: [`component/AGENTS.md`](../../component/AGENTS.md).

- **A piece joins the package when extracting it deletes copies that working tools carry.**
  [Recurring principles](../architecture/README.md#recurring-principles)
- **An unavailable control is still drawn and still a target**, and the operation asks again when
  pressed. [WL-HAND-01](../../agents/workshop/pane-controls.md#wl-hand-01--a-control-is-a-face-a-place-and-an-operation-and-availability-is-drawn)
- **A paste is a conversation**, and its answer belongs to the draft that asked.
  [WL-TEXT-09](../../agents/workshop/text-box.md#wl-text-09--a-paste-is-a-conversation-and-the-answer-belongs-to-the-draft-that-asked)

### Attention (`attention/`)

Its law, pages and suites: [`attention/AGENTS.md`](../../attention/AGENTS.md).

- **A condition leaves only when it stops being true.**
  [What makes a condition go away](../../attention/docs/attention.md#what-makes-a-condition-go-away)
- **Hiding is not fixing**: a condition that materially changes comes back, and the glance still
  counts what is hidden. [Hiding is not fixing](../../attention/docs/attention.md#hiding-is-not-fixing)
- **Nothing Workshop discovers opens the pane or takes the keys.**
  [The Attention pane](../../attention/docs/attention.md#the-attention-pane)

### The Builder (`builder/builder-pane/`, `builder/`)

Its law, pages and suites: [`builder/AGENTS.md`](../../builder/AGENTS.md).

- **A build succeeded when it exited zero and its named artifact exists**, judged in that order.
  [A process exiting zero is not an artifact](../../builder/docs/recipes.md#where-the-artifact-lands-and-how-a-build-is-judged)
- **A recipe names inputs, never a program.**
  [A recipe can name no program](../../builder/docs/recipes.md#what-a-recipe-cannot-do)
- **A reload in place keeps the running weave's shapes**; anything else is refused before the
  weave is touched. [Load after build](../../workshop/docs/load-plans.md#when-a-built-artifact-is-loaded-again)
- **An observer follows a press by the request it caused**, never by the first status to arrive.
  [What an observer is told](../../builder/docs/zengine.builder.md#follow)
- **Two builds at once in one case take two build trees.**
  [VM-LANE-21](../../agents/verification/lanes.md#vm-lane-21--one-owner-per-build-tree-holds-inside-one-process-too)

### Connections (`external-host/connections-pane/`)

Its law, pages and suites: [`external-host/AGENTS.md`](../../external-host/AGENTS.md).

- **A row's losses are one function's**, said at launch, in Attention and here.
  [WL-GUEST-08](../../agents/workshop/guests.md#wl-guest-08--what-a-rows-powers-do-not-reach-is-said-beside-it)
- **A guest's hand is narrowed only where the guests law names the act.**
  [Do not assume](../../agents/workshop/guests.md#do-not-assume)
- **A guest in a test is arranged with its admitted row and its host.**
  [Permissions belong to the arranged actor](testing-workshop-panes.md#permissions-belong-to-the-arranged-actor)

### The Pane Manager and Hotkeys (`workshop/desktop-pane/`)

Its law, pages and suites: [`workshop/AGENTS.md`](../../workshop/AGENTS.md).

- **The application's defaults are declared once, by the desktop.**
  [Keys the application supplies](../../workshop/docs/hotkeys.md#keys-the-application-supplies--and-how-to-take-them-away)
- **A pane takes an application row only by naming its id.**
  [Do not assume](../../agents/workshop/desktop.md#do-not-assume)
- **A launch opens or focuses a pane; it never toggles and never loads.**
  [WL-DESK-03](../../agents/workshop/desktop.md#wl-desk-03--a-launch-opens-or-focuses-it-never-toggles-and-never-loads)

### The Editor (`editor/editor-pane/`, `editor/source-transfer/`)

Its law, pages and suites: [`editor/AGENTS.md`](../../editor/AGENTS.md).

- **Every opener goes through the one managed opening.** [Opening a source](../../editor/docs/editor.md#opening-a-source)
- **A held Editor is repaired by reloading its image**; closing its pane repairs nothing.
  [While an open is on its way](../../editor/docs/editor.md#while-an-open-is-on-its-way-and-if-it-stalls)
- **No ordinary act drops dirty source.** [Save, dirty, and never losing work](../../editor/docs/editor.md#save-dirty-and-never-losing-work)
- **What the Editors carry is refused whole past its limit, never cut.** [Limits](../../editor/docs/source-transfer.md#limits)
- **The opening's witnesses are model rigs; none drives a live medium.**
  [Do not assume](../../agents/workshop/opening.md#do-not-assume)

### Files (`files/`)

Its law, pages and suites: [`files/AGENTS.md`](../../files/AGENTS.md).

- **The host holds three doors and the plan row that loads the browser, and knows nothing else of
  it.** [Do not assume](../../agents/workshop/files.md#do-not-assume)
- **A pane's rows are its mode's, and an id is one operation.**
  [WL-FILES-16](../../agents/workshop/files.md#wl-files-16--a-panes-rows-are-its-modes-and-an-id-is-one-operation)
- **Files judges no file's contents and no recipe**; it hands over a path or a place.
  [What it is not](../../files/docs/files.md#what-it-is-not)

### Flow (`flow/flow-pane/`, `flow/`, `flow/flow-host/`)

Its law, pages and suites: [`flow/AGENTS.md`](../../flow/AGENTS.md).

- **The authoring package stays independent of Workshop and of the external host.**
  [Flow authoring](../../agents/flow.md)
- **Powers are found only through the discovery door**, and shown as it answered them.
  [Flow authoring](../../agents/flow.md)
- **An open dialog refuses every `FlowEdit`.** [Other interfaces](../../flow/docs/flow.md#other-interfaces)
- **A new semantic boundary joins the named witnesses**, not only a canned example.
  [Flow authoring](../../agents/flow.md)
- **A Flow session pumps and waits nothing of its own**, and `FlowAnswer.ok` on a send proves
  only queueing. [Flow on a host](../../agents/flow-host.md)

### Info (`info/`)

Its law, pages and suites: [`info/AGENTS.md`](../../info/AGENTS.md).

- **While a view waits, its draft is frozen and nothing times out.**
  [While a view waits](../../info/docs/info-views.md#while-a-view-waits)
- **Only an inspector's own ask names its subject**; selection, keys and Escape never move it.
  [WL-INFO-14](../../agents/workshop/info-body.md#wl-info-14--the-subject-is-a-pane-info-names-and-nothing-else-moves-it)
- **An ask Loom says never arrived is released.**
  [WL-INFO-13](../../agents/workshop/info-body.md#wl-info-13--an-ask-loom-says-never-arrived-is-released-and-silence-still-waits)
- **A slow source in a test defers its answer**; it never forges a correlation.
  [Delay a reply](testing-workshop-panes.md#delay-a-reply-without-changing-what-a-reply-means)

### Inventory (`inventory/inventory-pane/`, `inventory/`)

Its law, pages and suites: [`inventory/AGENTS.md`](../../inventory/AGENTS.md).

- **One gesture changes one fact at one owner.** [Inventory](../../agents/inventory.md)
- **A refusal leaves storage unchanged**, and a reference never rebinds by label or row.
  [Inventory](../../agents/inventory.md)
- **The actor in a test is granted exactly the operation**, and denied it for the negative case.
  [Permissions belong to the arranged actor](testing-workshop-panes.md#permissions-belong-to-the-arranged-actor)

### The Terminal (`terminal/`)

Its law, pages and suites: [`terminal/AGENTS.md`](../../terminal/AGENTS.md).

- **The image that presents the Terminal's participant cannot reach it.**
  [WL-TERM-08](../../agents/workshop/terminal.md#wl-term-08--the-image-that-presents-a-participant-cannot-reach-one)
- **Escape sheds one layer a press**, and is given back only when the pane holds nothing.
  [Leaving the pane](../../terminal/docs/terminal.md#leaving-the-pane)
- **A case moved to another consumer is coverage lost.**
  [VM-POP-21](../../agents/verification/population.md#vm-pop-21--a-test-repointed-to-another-consumer-is-coverage-lost-at-the-first)

### The View Builder (`view/view-builder/`, `view/`)

Its law, pages and suites: [`view/AGENTS.md`](../../view/AGENTS.md).

- **`view::picture` is the one drawing of a view**; the builder draws only marks over it.
  [Described views](../../agents/view.md)
- **Pan, grid, gestures and marks are presentation**, never saved into a description.
  [Described views](../../agents/view.md)
- **A described view, an Info view and an Inventory view are three different things.**
  [Views here, in Info and in Inventory](../../view/docs/view-builder.md#views-here-in-info-and-in-inventory)

### Neovim (`editor/neovim-editor/`, `editor/neovim/`)

Its law, pages and suites: [`editor/AGENTS.md`](../../editor/AGENTS.md).

- **The ungated cases use a Neovim that fails at start on purpose**; the gated ones need a real one.
  [Do not assume](../../agents/workshop/neovim.md#do-not-assume)
- **A timeout is not a refusal**: a request already sent still runs.
  [Do not assume](../../agents/workshop/neovim-transfers.md#do-not-assume)
- **A bounded wait is tested past the moment Neovim resumes.**
  [VM-PROBE-13](../../agents/verification/probes.md#vm-probe-13--a-bounded-wait-on-a-change-is-tested-past-the-callees-resumption)

### Loaded, Project and Powers (`introspection/`)

Its law, pages and suites: [`introspection/AGENTS.md`](../../introspection/AGENTS.md).

- **Loaded and Project disagree on purpose.**
  [The system can show what it is](../../agents/panes.md#the-system-can-show-what-it-is)
- **Each ask derives its answer anew, and nothing is kept between asks.**
  [The system can show what it is](../../agents/panes.md#the-system-can-show-what-it-is)
- **One office offers three panes, so every key is judged by its pane before any state moves.**
  [The Powers pane](../../agents/panes.md#the-powers-pane-became-a-browser-and-the-seam-did-not-move)

### Compose (`composer/`, `message-draft/`)

Its law, pages and suites: [`composer/AGENTS.md`](../../composer/AGENTS.md) and
[`message-draft/AGENTS.md`](../../message-draft/AGENTS.md).

- **A drop never submits**, and submitting authorizes the exact destination and versioned shape.
  [The Composer](../../agents/panes.md#the-composer-is-a-schema-directed-message-form)
- **`SUBMITTED` means queued**, not applied. [Reuse a stored command](../../inventory/docs/inventory-compose.md)
- **Drafts spend Loom's lexer and composer**, and absence, false and empty stay distinct.
  [Message drafts](../../agents/message-drafts.md)

### The menu presenter (`workshop/menu-presenter/`)

Its law, pages and suites: [`workshop/AGENTS.md`](../../workshop/AGENTS.md).

- **The presenter owns a menu's showing and lifetime; replacing it is ordinary.**
  [WL-CTX-10](../../agents/workshop/pane-menu.md#wl-ctx-10--the-presenter-owns-a-menus-showing-and-lifetime-replacing-it-is-ordinary)
- **A presenter that keeps `HeldMenu` as its reload state takes over an open menu when reloaded.**
  [Replacing the menu presenter](../../workshop/docs/panes.md#replacing-the-menu-presenter) and
  [the menu presenter, and replacing it](../../workshop/docs/workshop-panes.md#the-menu-presenter-and-replacing-it)

## The external host (`external-host/`, `external-host/demo-control/`)

Its law, pages and suites: [`external-host/AGENTS.md`](../../external-host/AGENTS.md).

- **A tool's capability and its help change together.**
  [Journeys as Python tools](../../external-host/docs/external-host.md#3-from-a-loom-session-journeys-as-python-tools)
- **Settlement orders the bus's work; success is each owner's own answer.**
  [Run the journey](../../external-host/docs/external-host.md#5-run-the-journey)
- **A tool gives Workshop's input session back in its cleanup**; a forced cancel or a dead worker
  leaves it held.
  [Giving Workshop's input session back](../../external-host/docs/external-host.md#giving-workshops-input-session-back)
- **The desk is written outside every source checkout.**
  [The desk written as text for an agent](../../external-host/docs/external-host.md#the-desk-written-as-text-for-an-agent)
- **Reset is no transaction**: a pending owner operation refuses it.
  [Reset and its boundary](../../workshop/docs/demo-setups.md#reset-and-its-boundary)

## Surface (`surface/`)

Its law, pages and suites: [`surface/AGENTS.md`](../../surface/AGENTS.md).

- **Choose a text primitive by who owns the room.**
  [Which text primitive](../../agents/surface.md#which-text-primitive-who-owns-the-room)
- **Paint and hit share one arithmetic**, floored at each medium's grain.
  [The canvas is pixels](../../agents/surface.md#the-canvas-is-pixels-and-each-medium-floors-at-its-own-grain)
- **A terminal it cannot measure publishes nothing**, never a size of zero.
  [The terminal is a medium with a size](../../agents/surface.md#the-terminal-is-a-medium-with-a-size-and-the-sink-is-what-holds-it)
- **Read the traps before changing a medium.** [Do not assume](../../agents/surface.md#do-not-assume)

## Input (`input/`)

Its law, pages and suites: [`input/AGENTS.md`](../../input/AGENTS.md).

- **An SDL scancode is the key's identity**; its name is a convenience.
  [The Input package](../../input/docs/input.md)
- **A scan name names a value that already arrives.**
  [Input scan names](../../agents/surface.md#input-scan-names-are-names-for-values-that-already-arrived)
- **A suspect translation is bracketed by two probes.**
  [VM-PROBE-03](../../agents/verification/probes.md#vm-probe-03--bracket-a-suspect-translation-edge-with-two-probes)
- **One poll can publish several events.**
  [VM-FIX-25](../../agents/verification/fixtures.md#vm-fix-25--a-loops-shape-is-not-a-bound-on-what-a-weaver-can-produce)

## UI (`ui/`)

Its law, pages and suites: [`ui/AGENTS.md`](../../ui/AGENTS.md).

- **The fence keeps resolved numbers out of an authored type, by type and by name**, each half a
  compile entry beside its control, and the name half is not airtight. [The fence](../../ui/docs/ui.md#the-fence)
- **Resolution is total over every value.** [Resolution is total](../../ui/docs/ui.md#resolution-is-total)
- **A broken chain is absent**, never guessed. [A broken chain is absent](../../ui/docs/ui.md#a-broken-chain-is-absent)
- **Extent arithmetic runs the sanitizer lane too.** [The sanitizer lane](build-and-test.md#the-sanitizer-lane)

## The Timer and activation (`timer/`, `activation/`)

Its law, pages and suites: [`timer/AGENTS.md`](../../timer/AGENTS.md) and
[`activation/AGENTS.md`](../../activation/AGENTS.md).

- **A process with the Timer loaded is never idle**: its host loop turns with `pump_pending()`.
  [Do not assume](../../AGENTS.md#do-not-assume)
- **Orders are placed again on `TimerReady`.**
  [The one rule](../../timer/docs/timers.md#the-one-rule-that-keeps-schedules-honest)
- **A compile wall is judged on its diagnostic, beside a control that must compile.**
  [VM-WALL-04](../../agents/verification/walls.md#vm-wall-04--one-compile-negative-case-per-reachable-path-with-its-control)
- **An activation's provenance is asked before its lineage.**
  [Two questions, in this order](../../activation/docs/activation.md#two-questions-in-this-order)
- **The lane stays serial on Windows.**
  [Build / test](../../AGENTS.md#build--test-canonical-wsl-consumes-an-installed-loom)

## Operators (`operator/`)

Its law, pages and suites: [`operator/AGENTS.md`](../../operator/AGENTS.md).

- **A loadable consumer never links the operator library**, which would give it a catalog of its
  own. [A loaded weave can spend the host's operators](../../agents/operators.md#a-loaded-weave-can-spend-the-hosts-operators)
- **No descriptor is written by hand.**
  [A loaded weave can spend the host's operators](../../agents/operators.md#a-loaded-weave-can-spend-the-hosts-operators)
- **Read the traps before changing the catalog.** [Do not assume](../../agents/operators.md#do-not-assume)

## The maker weave (`maker/`)

Its law, pages and suites: [`maker/AGENTS.md`](../../maker/AGENTS.md).

- **Cases go in the one `maker` suite**, authoring their definitions as data.
  [Where the law is](../../agents/maker.md#where-the-law-is)
- **A behaviour edit and a schema edit are distinct paths.** [Do not assume](../../agents/maker.md#do-not-assume)
- **A source tripwire is a pure string check**: reword the prose, never weaken the check.
  [VM-WALL-10](../../agents/verification/walls.md#vm-wall-10--a-source-tripwire-is-a-pure-string-check)

## Snake and smoke (`examples/snake/`, `tests/smoke/`)

Snake's law, pages and suites: [`examples/AGENTS.md`](../../examples/AGENTS.md); `smoke` is the
root [AGENTS.md](../../AGENTS.md#read-this-then-route)'s.

- **The smoke test shows the gate refusing**, which is what makes it a proof.
  [What is in the lane](build-and-test.md#what-is-in-the-lane)
- **Every suite but `smoke` needs a Loom with its kernel.**
  [VM-POP-10](../../agents/verification/population.md#vm-pop-10--every-suite-but-smoke-needs-a-loom-that-can-host-weaves)

## Examples (`examples/`)

Its law, pages and suites: [`examples/AGENTS.md`](../../examples/AGENTS.md).

- **An example that claims to compile has been compiled.**
  [Documentation conventions](repository-conventions.md#documentation-conventions)
- **An action keeps its id while its meaning holds**, and a new meaning is a new action.
  [Start with the running example](../../workshop/docs/make-a-workshop-tool.md#start-with-the-running-example)

## The installed package (`cmake/ZengineInstall.cmake`, public headers)

[Packaging](../../agents/packaging.md); [using Zengine from another project](../getting-started.md#using-zengine-from-another-project);
witnessed by the installed-package witness and `package_vocabulary`.

- **The package's boundary changes in `cmake/ZengineInstall.cmake`**, and nowhere else.
  [Zengine is a package a stranger installs](../../agents/packaging.md#zengine-is-a-package-a-stranger-installs)
- **A header is public only when installed**, with everything it includes.
  [Do not assume](../../agents/packaging.md#do-not-assume)
- **An exported target links what its public headers use, on its own line.**
  [Packages](repository-conventions.md#packages)
- **A public header, an exported link line or the install file runs the package witness**:
  `cmake -DZEN_BUILD_DIR=build -DZEN_WORK=<outside the repository> -P tests/package/run.cmake`.
  [The lanes in order](../../agents/verification.md#the-lanes-in-order)

## Documentation and its checks

[Documentation conventions](repository-conventions.md#documentation-conventions) and
[the population contract](../../AGENTS.md#the-population-contract); witnessed by `doc_links`,
`package_vocabulary`, `law_register`, `source_comments`, `code_values` and `folder_map`, which the
documentation lane runs together.

- **One page has one reader purpose.**
  [Documentation conventions](repository-conventions.md#documentation-conventions)
- **A page cites a test case only by a name a test declares, and a law only by an id a register
  declares.** [Read this, then route](../../AGENTS.md#read-this-then-route)
- **A page a compiled test reads is the official lane's**, and `tests/text_checks.cmake` names it.
  [Build / test](../../AGENTS.md#build--test-canonical-wsl-consumes-an-installed-loom)
- **A check that reads the tree self-tests before it answers.**
  [VM-CHECK-01](../../agents/verification/checks.md#vm-check-01--a-tree-reading-check-self-tests-before-it-answers)
