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

- **One issue is one branch and one pull request, which closes it with `Fixes #n`.**
  [Taking an issue](taking-an-issue.md#1-choose-a-ready-issue) and
  [the pull request](taking-an-issue.md#9-the-pull-request)
- **No co-author trailer of any kind, and no assistant credit line**:
  `cmake -P tests/check_commit_attribution.cmake`. [Attribution](repository-conventions.md#attribution)
  and [the attribution guard](../../tests/check_commit_attribution.cmake)
- **The whole diff is read before the push.**
  [Taking an issue](taking-an-issue.md#8-the-checks-whose-green-counts)
- **A visible change shows its screenshots in the pull request.**
  [Taking an issue](taking-an-issue.md#9-the-pull-request)
- **One push, and its run read to its end before the next.**
  [Taking an issue](taking-an-issue.md#10-reading-the-hosted-run)

## Workshop and its panes

The host, `workshop/`, and the panes that reach it. Workshop's law is in the registers
[its router](../../agents/workshop.md#where-the-law-is) lists, what crosses the pane seam is
[the pane protocol's](../../agents/panes.md), and
[where a case goes](../../agents/verification/population.md#where-a-case-goes) says how to find
the suite that pins a register's law. The Workshop entry `workshop_<subject>` runs
`build/tests/zengine-workshop-<subject>-tests`.

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
  [When a pane's messages change](../workshop/develop-workshop.md#when-a-panes-messages-change-a-new-runtime-too)
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
  [Build and check only the relevant work](../guides/make-a-workshop-tool.md#build-and-check-only-the-relevant-work-while-iterating)

### The host (`workshop/`)

Workshop's own registers, which [the router](../../agents/workshop.md#where-the-law-is) lists by
subject; [panes](../workshop/panes.md), [setups](../workshop/setups.md) and
[the pane contracts](../reference/workshop-panes.md); witnessed by `workshop_panes`,
`workshop_screen`, `workshop_host`, `workshop_document`, `workshop_desk`, `workshop_persistence`,
`workshop_load` and `workshop_shapes`.

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

The [text box](../../agents/workshop/text-box.md) and [pane controls](../../agents/workshop/pane-controls.md)
registers; [the component package](../reference/component.md); witnessed by `component`,
`workshop_panes` and `workshop_document`.

- **A piece joins the package when extracting it deletes copies that working tools carry.**
  [Recurring principles](../architecture/README.md#recurring-principles)
- **An unavailable control is still drawn and still a target**, and the operation asks again when
  pressed. [WL-HAND-01](../../agents/workshop/pane-controls.md#wl-hand-01--a-control-is-a-face-a-place-and-an-operation-and-availability-is-drawn)
- **A paste is a conversation**, and its answer belongs to the draft that asked.
  [WL-TEXT-09](../../agents/workshop/text-box.md#wl-text-09--a-paste-is-a-conversation-and-the-answer-belongs-to-the-draft-that-asked)

### Attention (`attention-pane/`)

The [attention](../../agents/workshop/attention.md) register;
[what needs your attention](../workshop/attention.md); witnessed by `workshop_panes` and
`workshop_host`.

- **A condition leaves only when it stops being true.**
  [What makes a condition go away](../workshop/attention.md#what-makes-a-condition-go-away)
- **Hiding is not fixing**: a condition that materially changes comes back, and the glance still
  counts what is hidden. [Hiding is not fixing](../workshop/attention.md#hiding-is-not-fixing)
- **Nothing Workshop discovers opens the pane or takes the keys.**
  [The Attention pane](../workshop/attention.md#the-attention-pane)

### The Builder (`builder-pane/`, `builder/`)

[Realization](../../agents/realization.md), and the [project](../../agents/workshop/project.md),
[build output](../../agents/workshop/build-output.md), [code](../../agents/workshop/code.md) and
[authoring](../../agents/workshop/authoring.md) registers; [Workshop's Builder](../workshop/builder.md)
and [the Builder package](../reference/builder.md); witnessed by `workshop_panes`, `workshop_files`,
`workshop_load` and `builder`.

- **A build succeeded when it exited zero and its named artifact exists**, judged in that order.
  [A process exiting zero is not an artifact](../reference/builder.md#a-process-exiting-zero-is-not-an-artifact)
- **A recipe names inputs, never a program.**
  [A recipe can name no program](../reference/builder.md#a-recipe-is-authored-knowledge-and-it-can-name-no-program)
- **A reload in place keeps the running weave's shapes**; anything else is refused before the
  weave is touched. [Load after build](../workshop/builder.md#load-after-build-and-reload-in-place)
- **An observer follows a press by the request it caused**, never by the first status to arrive.
  [What an observer is told](../workshop/builder.md#what-an-observer-is-told)
- **Two builds at once in one case take two build trees.**
  [VM-LANE-21](../../agents/verification/lanes.md#vm-lane-21--one-owner-per-build-tree-holds-inside-one-process-too)

### Connections (`connections-pane/`)

The [guests](../../agents/workshop/guests.md) register and [the pane protocol](../../agents/panes.md);
[the external host](../workshop/external-host.md#whose-host-this-is-and-what-each-power-reaches-there);
witnessed by `workshop_panes`, `workshop_guests` and `guest_journey`.

- **A row's losses are one function's**, said at launch, in Attention and here.
  [WL-GUEST-08](../../agents/workshop/guests.md#wl-guest-08--what-a-rows-powers-do-not-reach-is-said-beside-it)
- **A guest's hand is narrowed only where the guests law names the act.**
  [Do not assume](../../agents/workshop/guests.md#do-not-assume)
- **A guest in a test is arranged with its admitted row and its host.**
  [Permissions belong to the arranged actor](testing-workshop-panes.md#permissions-belong-to-the-arranged-actor)

### The Pane Manager and Hotkeys (`desktop-pane/`)

The [desktop](../../agents/workshop/desktop.md), [desktop presenting](../../agents/workshop/desktop-presenting.md),
[keyboard](../../agents/workshop/keyboard.md) and [keymap edit](../../agents/workshop/keymap-edit.md)
registers; [the Pane Manager](../workshop/panes.md#showing-going-to-and-hiding--the-pane-manager)
and [hotkeys](../workshop/hotkeys.md); witnessed by `workshop_panes` and `workshop_document`.

- **The application's defaults are declared once, by the desktop.**
  [Keys the application supplies](../workshop/hotkeys.md#keys-the-application-supplies--and-how-to-take-them-away)
- **A pane takes an application row only by naming its id.**
  [Do not assume](../../agents/workshop/desktop.md#do-not-assume)
- **A launch opens or focuses a pane; it never toggles and never loads.**
  [WL-DESK-03](../../agents/workshop/desktop.md#wl-desk-03--a-launch-opens-or-focuses-it-never-toggles-and-never-loads)

### The Editor (`editor-pane/`, `source-transfer/`)

The [editor](../../agents/workshop/editor.md), [editor transfers](../../agents/workshop/editor-transfers.md),
[opening](../../agents/workshop/opening.md) and [editor switch](../../agents/workshop/editor-switch.md)
registers; [the source editor](../workshop/editor.md) and
[source transfer](../reference/source-transfer.md); witnessed by `workshop_panes`, `editor`,
`workshop_editor_switch`, `workshop_load`, `source_transfer` and `source_transfer_cpp`.

- **Every opener goes through the one managed opening.** [Opening a source](../workshop/editor.md#opening-a-source)
- **A held Editor is repaired by reloading its image**; closing its pane repairs nothing.
  [While an open is on its way](../workshop/editor.md#while-an-open-is-on-its-way-and-if-it-stalls)
- **No ordinary act drops dirty source.** [Save, dirty, and never losing work](../workshop/editor.md#save-dirty-and-never-losing-work)
- **What the Editors carry is refused whole past its limit, never cut.** [Limits](../reference/source-transfer.md#limits)
- **The opening's witnesses are model rigs; none drives a live medium.**
  [Do not assume](../../agents/workshop/opening.md#do-not-assume)

### Files (`files/`)

The [files](../../agents/workshop/files.md) register; [Files](../workshop/files.md); witnessed by
`files_weave` for its values, and `workshop_panes` and `workshop_files` for its gestures.

- **The host holds three doors and the plan row that loads the browser, and knows nothing else of
  it.** [Do not assume](../../agents/workshop/files.md#do-not-assume)
- **A pane's rows are its mode's, and an id is one operation.**
  [WL-FILES-16](../../agents/workshop/files.md#wl-files-16--a-panes-rows-are-its-modes-and-an-id-is-one-operation)
- **Files judges no file's contents and no recipe**; it hands over a path or a place.
  [What it is not](../workshop/files.md#what-it-is-not)

### Flow (`flow-pane/`, `flow/`, `flow-host/`)

[Flow authoring](../../agents/flow.md) and [Flow on a host](../../agents/flow-host.md);
[Flow](../workshop/flow.md) and [its reference](../reference/flow.md); witnessed by `flow` and
`flow_pane`, and at the installed boundary by the package witness.

- **The authoring package stays independent of Workshop and of the external host.**
  [Flow authoring](../../agents/flow.md)
- **Powers are found only through the discovery door**, and shown as it answered them.
  [Flow authoring](../../agents/flow.md)
- **An open dialog refuses every `FlowEdit`.** [Other interfaces](../workshop/flow.md#other-interfaces)
- **A new semantic boundary joins the named witnesses**, not only a canned example.
  [Flow authoring](../../agents/flow.md)
- **A Flow session pumps and waits nothing of its own**, and `FlowAnswer.ok` on a send proves
  only queueing. [Flow on a host](../../agents/flow-host.md)

### Info (`info-pane/`)

The [Info body](../../agents/workshop/info-body.md), [Info controls](../../agents/workshop/info-controls.md)
and [settings](../../agents/workshop/settings.md) registers, the [pane manager](../../agents/workshop/pane-manager.md)
register for a pane's subject rows, and [Inventory](../../agents/inventory.md) for the value views;
[Info views](../workshop/info-views.md); witnessed by `workshop_panes` and `workshop_persistence`,
and the subject rows by `workshop_host`.

- **While a view waits, its draft is frozen and nothing times out.**
  [While a view waits](../workshop/info-views.md#while-a-view-waits)
- **Only an inspector's own ask names its subject**; selection, keys and Escape never move it.
  [WL-INFO-14](../../agents/workshop/info-body.md#wl-info-14--the-subject-is-a-pane-info-names-and-nothing-else-moves-it)
- **An ask Loom says never arrived is released.**
  [WL-INFO-13](../../agents/workshop/info-body.md#wl-info-13--an-ask-loom-says-never-arrived-is-released-and-silence-still-waits)
- **A slow source in a test defers its answer**; it never forges a correlation.
  [Delay a reply](testing-workshop-panes.md#delay-a-reply-without-changing-what-a-reply-means)

### Inventory (`inventory-pane/`, `inventory/`)

[Inventory](../../agents/inventory.md); [slots](../workshop/inventory-slots.md),
[folders](../workshop/inventory-folders.md), [toolboxes](../workshop/toolboxes.md) and
[the reference](../reference/inventory.md); witnessed by `inventory` and `workshop_panes`.

- **One gesture changes one fact at one owner.** [Inventory](../../agents/inventory.md)
- **A refusal leaves storage unchanged**, and a reference never rebinds by label or row.
  [Inventory](../../agents/inventory.md)
- **The actor in a test is granted exactly the operation**, and denied it for the negative case.
  [Permissions belong to the arranged actor](testing-workshop-panes.md#permissions-belong-to-the-arranged-actor)

### The Terminal (`terminal-pane/`)

The [terminal](../../agents/workshop/terminal.md) and [Terminal pane](../../agents/workshop/terminal-pane.md)
registers; [Terminal](../workshop/terminal.md); witnessed by `workshop_panes` and `workshop_host`.

- **The image that presents the Terminal's participant cannot reach it.**
  [WL-TERM-08](../../agents/workshop/terminal.md#wl-term-08--the-image-that-presents-a-participant-cannot-reach-one)
- **Escape sheds one layer a press**, and is given back only when the pane holds nothing.
  [Leaving the pane](../workshop/terminal.md#leaving-the-pane)
- **A case moved to another consumer is coverage lost.**
  [VM-POP-21](../../agents/verification/population.md#vm-pop-21--a-test-repointed-to-another-consumer-is-coverage-lost-at-the-first)

### The View Builder (`view-builder/`, `view/`)

[Described views](../../agents/view.md) and the [weaver's pane](../../agents/workshop/maker-pane.md)
register; [the View Builder](../workshop/view-builder.md) and [views](../reference/view.md);
witnessed by `view`, `view_builder` and `workshop_panes`.

- **`view::picture` is the one drawing of a view**; the builder draws only marks over it.
  [Described views](../../agents/view.md)
- **Pan, grid, gestures and marks are presentation**, never saved into a description.
  [Described views](../../agents/view.md)
- **A described view, an Info view and an Inventory view are three different things.**
  [Views here, in Info and in Inventory](../workshop/view-builder.md#views-here-in-info-and-in-inventory)

### Neovim (`neovim-editor/`, `neovim/`)

The [Neovim](../../agents/workshop/neovim.md) and [Neovim transfers](../../agents/workshop/neovim-transfers.md)
registers; [Neovim in Workshop](../workshop/neovim.md); witnessed by `neovim` and
`workshop_neovim`, and with a Neovim named at configure, `neovim_live` too.

- **The ungated cases use a Neovim that fails at start on purpose**; the gated ones need a real one.
  [Do not assume](../../agents/workshop/neovim.md#do-not-assume)
- **A timeout is not a refusal**: a request already sent still runs.
  [Do not assume](../../agents/workshop/neovim-transfers.md#do-not-assume)
- **A bounded wait is tested past the moment Neovim resumes.**
  [VM-PROBE-13](../../agents/verification/probes.md#vm-probe-13--a-bounded-wait-on-a-change-is-tested-past-the-callees-resumption)

### Loaded, Project and Powers (`introspection/`)

The pane protocol's [introspection sections](../../agents/panes.md#the-system-can-show-what-it-is);
[introspection](../reference/introspection.md); witnessed by `workshop_panes` and `workshop_load`.

- **Loaded and Project disagree on purpose.**
  [The system can show what it is](../../agents/panes.md#the-system-can-show-what-it-is)
- **Each ask derives its answer anew, and nothing is kept between asks.**
  [The system can show what it is](../../agents/panes.md#the-system-can-show-what-it-is)
- **One office offers three panes, so every key is judged by its pane before any state moves.**
  [The Powers pane](../../agents/panes.md#the-powers-pane-became-a-browser-and-the-seam-did-not-move)

### Compose (`composer/`, `message-draft/`)

[The Composer](../../agents/panes.md#the-composer-is-a-schema-directed-message-form) and
[message drafts](../../agents/message-drafts.md); [reuse a stored command](../workshop/inventory-compose.md)
and [message drafts](../reference/message-drafts.md); witnessed by `composer`, `message_draft`
and `workshop_panes`.

- **A drop never submits**, and submitting authorizes the exact destination and versioned shape.
  [The Composer](../../agents/panes.md#the-composer-is-a-schema-directed-message-form)
- **`SUBMITTED` means queued**, not applied. [Reuse a stored command](../workshop/inventory-compose.md)
- **Drafts spend Loom's lexer and composer**, and absence, false and empty stay distinct.
  [Message drafts](../../agents/message-drafts.md)

### The menu presenter (`menu-presenter/`)

The [pane menu](../../agents/workshop/pane-menu.md) register and
[the second button](../../agents/panes.md#the-second-button-crosses-as-one-shape-a-menu-is-presented-by-a-participant-and-a-press-names-its-picture);
[replacing the menu presenter](../workshop/panes.md#replacing-the-menu-presenter); witnessed by
`workshop_panes`.

- **The presenter owns a menu's showing and lifetime; replacing it is ordinary.**
  [WL-CTX-10](../../agents/workshop/pane-menu.md#wl-ctx-10--the-presenter-owns-a-menus-showing-and-lifetime-replacing-it-is-ordinary)
- **A presenter that keeps `HeldMenu` as its reload state takes over an open menu when reloaded.**
  [Replacing the menu presenter](../workshop/panes.md#replacing-the-menu-presenter) and
  [the menu presenter, and replacing it](../reference/workshop-panes.md#the-menu-presenter-and-replacing-it)

## The external host (`external-host/`, `demo-control/`)

[The external host](../workshop/external-host.md) and [demo setups](../workshop/demo-setups.md);
the [guests](../../agents/workshop/guests.md) and [desk read](../../agents/workshop/desk-read.md)
registers; witnessed by `guest_vocabulary`, `workshop_guests`, `workshop_probe`, `workshop_desk`
and `guest_journey`, and with Python, `demo_recipes` and `workshop_journey`, which first runs the
tool packages' checks under `tests/session/`.

- **A tool's capability and its help change together.**
  [Journeys as Python tools](../workshop/external-host.md#3-from-a-loom-session-journeys-as-python-tools)
- **Settlement orders the bus's work; success is each owner's own answer.**
  [Run the journey](../workshop/external-host.md#5-run-the-journey)
- **A tool gives Workshop's input session back in its cleanup**; a forced cancel or a dead worker
  leaves it held.
  [Giving Workshop's input session back](../workshop/external-host.md#giving-workshops-input-session-back)
- **The desk is written outside every source checkout.**
  [The desk written as text for an agent](../workshop/external-host.md#the-desk-written-as-text-for-an-agent)
- **Reset is no transaction**: a pending owner operation refuses it.
  [Reset and its boundary](../workshop/demo-setups.md#reset-and-its-boundary)

## Surface (`surface/`)

[Surface](../../agents/surface.md); [the surface reference](../reference/surface.md); witnessed by
`surface`, its SDL half under the `sdl` gate.

- **Choose a text primitive by who owns the room.**
  [Which text primitive](../../agents/surface.md#which-text-primitive-who-owns-the-room)
- **Paint and hit share one arithmetic**, floored at each medium's grain.
  [The canvas is pixels](../../agents/surface.md#the-canvas-is-pixels-and-each-medium-floors-at-its-own-grain)
- **A terminal it cannot measure publishes nothing**, never a size of zero.
  [The terminal is a medium with a size](../../agents/surface.md#the-terminal-is-a-medium-with-a-size-and-the-sink-is-what-holds-it)
- **Read the traps before changing a medium.** [Do not assume](../../agents/surface.md#do-not-assume)

## Input (`input/`)

[The Input package](../reference/input.md) and its backends in [surface](../../agents/surface.md);
witnessed by `input`, its SDL half under the `sdl` gate.

- **An SDL scancode is the key's identity**; its name is a convenience.
  [The Input package](../reference/input.md)
- **A scan name names a value that already arrives.**
  [Input scan names](../../agents/surface.md#input-scan-names-are-names-for-values-that-already-arrived)
- **A suspect translation is bracketed by two probes.**
  [VM-PROBE-03](../../agents/verification/probes.md#vm-probe-03--bracket-a-suspect-translation-edge-with-two-probes)
- **One poll can publish several events.**
  [VM-FIX-25](../../agents/verification/fixtures.md#vm-fix-25--a-loops-shape-is-not-a-bound-on-what-a-weaver-can-produce)

## UI (`ui/`)

[The UI package](../reference/ui.md); witnessed by `ui` and its three compile entries.

- **The fence keeps resolved numbers out of an authored type, by type and by name**, each half a
  compile entry beside its control, and the name half is not airtight. [The fence](../reference/ui.md#the-fence)
- **Resolution is total over every value.** [Resolution is total](../reference/ui.md#resolution-is-total)
- **A broken chain is absent**, never guessed. [A broken chain is absent](../reference/ui.md#a-broken-chain-is-absent)
- **Extent arithmetic runs the sanitizer lane too.** [The sanitizer lane](build-and-test.md#the-sanitizer-lane)

## The Timer and activation (`timer/`, `activation/`)

[TIMER-01 to TIMER-05](../laws/timer-laws.md), [the timer guide](../guides/timers.md) and
[activation](../reference/activation.md); witnessed by `timer`, its compile entries and
`audit_probes`.

- **A process with the Timer loaded is never idle**: its host loop turns with `pump_pending()`.
  [Do not assume](../../AGENTS.md#do-not-assume)
- **Orders are placed again on `TimerReady`.**
  [The one rule](../guides/timers.md#the-one-rule-that-keeps-schedules-honest)
- **A compile wall is judged on its diagnostic, beside a control that must compile.**
  [VM-WALL-04](../../agents/verification/walls.md#vm-wall-04--one-compile-negative-case-per-reachable-path-with-its-control)
- **An activation's provenance is asked before its lineage.**
  [Two questions, in this order](../reference/activation.md#two-questions-in-this-order)
- **The lane stays serial on Windows.**
  [Build / test](../../AGENTS.md#build--test-canonical-wsl-consumes-an-installed-loom)

## Operators (`operator/`)

[Operators](../../agents/operators.md); the [host](../reference/operator-host.md),
[providers](../reference/operator-providers.md) and [sources](../reference/operator-sources.md)
references; witnessed by `operator`.

- **A loadable consumer never links the operator library**, which would give it a catalog of its
  own. [A loaded weave can spend the host's operators](../../agents/operators.md#a-loaded-weave-can-spend-the-hosts-operators)
- **No descriptor is written by hand.**
  [A loaded weave can spend the host's operators](../../agents/operators.md#a-loaded-weave-can-spend-the-hosts-operators)
- **Read the traps before changing the catalog.** [Do not assume](../../agents/operators.md#do-not-assume)

## The maker weave (`maker/`)

[The maker router](../../agents/maker.md) and its registers; [the maker weave](../reference/maker-weave.md);
witnessed by `maker`.

- **Cases go in the one `maker` suite**, authoring their definitions as data.
  [Where the law is](../../agents/maker.md#where-the-law-is)
- **A behaviour edit and a schema edit are distinct paths.** [Do not assume](../../agents/maker.md#do-not-assume)
- **A source tripwire is a pure string check**: reword the prose, never weaken the check.
  [VM-WALL-10](../../agents/verification/walls.md#vm-wall-10--a-source-tripwire-is-a-pure-string-check)

## Snake and smoke (`snake/`, `smoke/`)

[Snake](../reference/snake.md) and [the lane's table](build-and-test.md#what-is-in-the-lane);
witnessed by `snake` and `smoke`.

- **The smoke test shows the gate refusing**, which is what makes it a proof.
  [What is in the lane](build-and-test.md#what-is-in-the-lane)
- **Every suite but `smoke` needs a Loom with its kernel.**
  [VM-POP-10](../../agents/verification/population.md#vm-pop-10--every-suite-but-smoke-needs-a-loom-that-can-host-weaves)

## Examples (`examples/`)

[Documentation conventions](repository-conventions.md#documentation-conventions) and
[making a Workshop tool](../guides/make-a-workshop-tool.md); compiled by `tests/` and loaded by the
Workshop suites.

- **An example that claims to compile has been compiled.**
  [Documentation conventions](repository-conventions.md#documentation-conventions)
- **An action keeps its id while its meaning holds**, and a new meaning is a new action.
  [Start with the running example](../guides/make-a-workshop-tool.md#start-with-the-running-example)

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
`package_vocabulary`, `law_register`, `source_comments` and `code_values`, which the documentation
lane runs together.

- **One page has one reader purpose.**
  [Documentation conventions](repository-conventions.md#documentation-conventions)
- **A page cites a test case only by a name a test declares, and a law only by an id a register
  declares.** [Read this, then route](../../AGENTS.md#read-this-then-route)
- **A page a compiled test reads is the official lane's**, and `tests/text_checks.cmake` names it.
  [Build / test](../../AGENTS.md#build--test-canonical-wsl-consumes-an-installed-loom)
- **A check that reads the tree self-tests before it answers.**
  [VM-CHECK-01](../../agents/verification/checks.md#vm-check-01--a-tree-reading-check-self-tests-before-it-answers)
