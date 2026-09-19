# Author a running graph in Workshop

Flow is a replaceable pane for creating message-driven weaves from the operators available in
the current host. The graph edits a draft. **Run** creates an interpreted participant; **Apply**
changes its behavior while keeping compatible live state. A rejected edit leaves it running.

Build and launch [development Workshop](develop-workshop.md), open Pane Manager, and choose
**Flow**. The shipped plans include the optional `zengine-flow-pane` artifact in the `zengine.flow`
office. Enlarge its pane for the graph; a width near 100 canvas columns is comfortable. A host
without the optional canvas protocol receives a textual summary. The independent
[Flow workbench](../guides/flow.md) remains available without Workshop.

## Create a high-water tracker

1. Choose **New**, name the project `water`, and confirm. The dialog states that it replaces the
   current draft. Save existing work first. Stop an existing run before opening another project.
2. Open **State**, choose **Add state field**, and declare `value`, `Int`, `required`. Its initial
   value is zero. Click a value to edit it; **Keep state** installs the completed form as initial
   or saved state. Editing an initial-state form does not change a running participant.
3. Open **Messages**, choose **New message**, and name it `Reading`. Choose **Add field** for
   message index `0`, name `input`, kind `Int`, presence `required`.
4. Open **Graph**, choose **Add trigger**, select message index `0` and state output `value`.
   Click `math.max` in the operator list to add it. The list describes the host's actual catalog.
5. Click `input.input` and then the node's `lhs` port. Click `state.value` and then its `rhs`
   port. Clicking a port without a selected source opens a binding editor: a constant, `$field`,
   or `%earlier-node`. The graph executes in order; forward or cyclic connections are refused.
6. Select the node header and choose **Use as result**. Drag its header to move it. Drag blank
   graph room or use the middle button to pan; the wheel pans vertically over the graph.
   The **-** and **+** controls change node positions and widths from 50% to 200%, in 25% steps.
   Text and port-row height stay fixed. Layout changes do not change executable behavior.
7. Choose **Run**. An unwired port or incompatible type names the place to repair. **Run queued**
   means a request was queued; the host's later answer establishes whether the participant exists.

In a dialog, **Tab** selects the next field, **Ctrl+A** selects its text, **Enter** confirms,
and **Escape** cancels. A refused confirmation keeps the entered text for correction. Confirm or
cancel before changing pages, starting another edit, or quitting.

## Keep examples and exercise the weave

In **Messages**, open `water.Reading`, click `input`, enter `3`, and confirm. **Save example**
stores this as `low`. Change the field to `7` and save `high`. These values are reusable data;
they carry their schemas and no sender identity, destination, grants or reply provenance.

Choose **Send**, then **Events**. The current exposed state and observed outputs/refusals appear
with send correlations. Dispatch settlement is distinct from an application result; a quiet weave
does not promise a reply. **Inspect** refreshes the owner's bounded record. An omission count says
when older observations were dropped. Loom's host Recorder/Logger remain the broader history tools.

Open `low` in **Library** and send it. The high-water state remains `7`. Return to Graph, click
the `rhs` port and bind the constant `10`. Choose **Apply**, then send `low` again: the state now
becomes `10`. Apply preserves live state and requires the existing accepted/emitted/state contract.
A schema change needs an explicit stop and fresh run; this pane does not invent a migration policy.

## Save unfinished work and return to it

**Save** writes one workspace containing the definition draft, node positions, view, named examples,
retained forms and saved state. Empty triggers and unwired ports can be saved. Forms preserve
absence, empty text and `false` separately. Missing required fields prevent sending, not saving.
Opening a changed schema retains the previous form under its previous identity. **Library** also
shows retained forms, including those historical shapes.

Click an absent Message or List field to create it. Clicking a present list appends an item;
its nested fields become editable rows. A present Message keeps its existing contents. **unset**
removes a value deliberately. The message-draft C++ API supports structured paths for list
removal; `FlowEdit` exposes `list-erase(row,index)` for the open form.
Scalar forms use Loom's existing schema-directed composer. Bytes currently require a typed API
value; the text editor does not invent a hexadecimal or base64 spelling.

**Library / Export** and **Import** share named values independently of a Flow project. Reopening
and sending checks schema agreement; an older example is not automatically migrated. The workspace
and library each have a 1 MiB file limit. Saves use the existing atomic file replacement helper.

After execution, newly observed state becomes the saved state unless an explicit initial-state
edit is awaiting a fresh run. The Events page continues to show live state independently. Saving
does not save process identity, pending queues, grants or resumable native continuations.

**Export** writes a completed executable Flow project for the standalone workbench and its C++
generation path. **Import** brings such a project into this editor. Workspace files additionally
contain unfinished authoring work and presentation; they are not executable projects.

An orderly Workshop quit waits for outstanding host work and fresh state inspection, then refuses unsaved Flow work. Saving an older snapshot while a send is pending does not bypass that wait. Save it, or choose **Discard** and type
`discard` to allow closing without saving. Pane reload restores the workspace and reconnects to
the session through its office. The selected page, unconfirmed dialog text and selection, and
the distinction between a deliberate initial-state edit and observed live state also survive
reload. Unconfirmed dialog text is separate from a saved workspace. Drawing grants and hit maps
are reacquired; a held drag ends rather than resuming in the replacement pane.

## Other interfaces

`FlowEdit` addresses `zengine.flow` with an action and a list of separate string arguments. The
`describe` action summarizes the editing grammar; `FlowEdited` returns acceptance, the workspace
and diagnostics. While a dialog is open, all `FlowEdit` requests are refused, including
`describe`; confirm or cancel the dialog through the pane first. A successful runtime command
means it was queued, with its later outcome shown in the pane. The
[Flow reference](../reference/flow.md#workshop-control-and-reload) describes this boundary.
This is the same semantic editing model the controls use. It is not a shell command.
Runtime requests have their own [Flow host protocol](../reference/flow-runtime.md), with explicit
ownership and authority. [Reusable message drafts](../reference/message-drafts.md) are available
as a package independently of this pane. The [canvas protocol](../reference/workshop-panes.md)
belongs to any pane that needs bounded local drawing.

Graphical editing currently uses rectangular nodes, orthogonal wires and fixed-size ASCII labels.
Bytes outside printable ASCII appear as `?`; this display substitution does not rewrite stored
values. An ellipsis marks a label shortened to fit. Large offscreen graphs do not consume the
visible canvas budget. If the visible drawing itself exceeds that budget, Flow shows **View
limit reached** with save/export and zoom controls. Pan or zoom in to reduce visible content;
the underlying draft remains intact.

The pane exposes input/state schemas and existing trigger graphs; arbitrary C++ deconstruction, authored
schema migration, general replay, and a visual emission-mapping editor remain separate capabilities.

The complete workspace is bounded to 1 MiB, including its retained forms and library.
An edit that would exceed this bound is refused before replacing the current draft, so
reload and save retain the same guarantee. Export reusable examples to a separate library
and remove unneeded presets before growing a workspace further.
