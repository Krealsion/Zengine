# Describe a small panel in the View Builder

The **View Builder** is a pane beside [Flow](flow.md) where you describe a small panel, a
**view**, as data: number fields you type into, buttons that say something, and labels that show
what they are told. **Run** asks the view host to make the view a participant of its own, and it
opens in a pane of its own. Flow composes what happens when the view speaks; the two meet only at
message shapes, which you carry from one pane to the other. The contract beneath this page is the
[view reference](../reference/view.md).

Open the Pane Manager and choose **View Builder**. The shipped plans include the optional
`zengine-view-builder` artifact in the `zengine.view.builder` office.

## Make the tally panel

The panel counts from a start toward a limit by a step, and shows the total a Flow definition
named `tally` works out.

1. Choose **New** and name the view `tally.panel`. The name is the view's office, the name its
   pane is listed under, and the namespace of what it says.
2. Under **Add**, choose **Number** three times, **Button** once and **Label** once. Each lands
   below the last. Select one in the list and choose **Edit** to set its id, its label, and its
   place and size in **whole pixels, 12 to a cell**: `start`, `limit` and `step` (give `step`
   the starting text `1`), a button `count` labelled `Count`, and a label `total` labelled
   `Total`. An id names a field an intent will carry, so it is letters, digits and `_`.
3. Select `count` and choose **Make intent from fields**. Name it `Count`: the builder makes
   `tally.panel.Count {start: Int, limit: Int, step: Int}`, one required `Int` for each number
   field, through the same shape rules Flow declares messages with, and `count` says it.
4. Choose **Run**. The panel opens in its own pane; `Total` says `waiting`, and so does its last
   row, until something tells it `tally.Total`.

## Carry shapes between the builder and Flow

Shapes cross by carry, both ways. A canvas pane carries out by its menu: right-press the line, choose
**Carry**, then click where it goes.

- **The intent into Flow.** In the builder, select `count` and right-press its `says` line;
  choose **Carry tally.panel.Count** and click Flow. Flow offers **Declare tally.panel.Count v1
  as an accepted message**. Make the `tally` definition there with a fold of `math.add`
  ([finding the fold](flow.md#find-what-to-compose)) and an emitted `tally.Total {total}`
  ([emits](flow.md#say-what-changed-emits)).
- **What Flow says, onto a label.** In Flow's **Messages**, right-press `tally.Total` under
  **Emitted**, choose **Carry tally.Total**, and click the `total` row in the builder. A shape
  with one field a label can show is bound at once (`shows tally.Total.total`); with several,
  the builder asks which. A value from Inventory binds by its shape, and a field carried from Info
  binds that field.

Binding `Total` changes what the view is told, so **Apply** registers the view afresh and says
so; its field text starts again from the description. A later change to a label, a place or a
size keeps the same shapes, and **Apply** takes it in place: what you typed stays.

## Use it

With `tally` running in Flow, type into the panel's fields (press a field, then type; `Tab`
moves between fields) and press **Count**, or `Return` in a field:

| start, limit, step | the panel shows |
|---|---|
| `0, 10, 1` | `Total: 45` |
| `10, 0, -2` | `Total: 30` |
| `0, 10, 3` | `Total: 18` |
| `0, 10, 0` | `refused: ... a step of 0 never moves the count from 0 toward 10`, and `Total` stays |
| `0, 2000000, 1` | refused by the fold's count bound, and `Total` stays |

The panel publishes `tally.panel.Count` as its own participant; whoever accepts that shape hears
it. It does not know `tally`, and `tally` does not know the panel: the panel is told
`tally.Total` because it accepts that shape. A refusal answered to the panel is shown on its last
rows. A field that holds no whole number is said there, and nothing is published.

**Save** the view (`Ctrl`+`s`) to a `.view` file and the Flow workspace beside it. Quit, start
Workshop again, **Open** both and **Run** both: the panel works as before and waits until told.
Nothing that was running is in either file. **Stop** leaves the panel's pane saying it stopped.

In a terminal Workshop the same panel works: its picture is the window's, floored to cells, and
each line is fitted to the cell.

## Views here, in Info and in Inventory

Three panes show something called a view, and they are different things:

- **A described view** is a participant of its own, made from a description: it is told values by
  their shape and says intents by publication. You build it here and it runs in its own pane.
- **An [Info view](info-views.md)** is one of Info's own windows onto a typed value you are
  inspecting or editing; it belongs to Info and speaks for nothing.
- **A [portable Inventory view](inventory-slots.md)** is a box, row or column of stored entries;
  it belongs to Inventory and holds entries, not a panel.

## The View Builder or the Pane Creator

Reach for the [Pane Creator](panes.md#the-pane-creator--a-pane-made-of-data) for a pane of static
text that Workshop paints and you place in Info. Reach for the View Builder when the pane must be
told values, take typing, or say something when a control is used. The two stay side by side.

## Limits

Three element kinds: a label, a number field that holds a whole number, and a button. A button
says at most one intent, made from all the view's number fields. A label shows one top-level
field. Elements are placed by their numbers, not dragged. A shape leaves a canvas pane by the
right-press menu and a click, not by dragging.
