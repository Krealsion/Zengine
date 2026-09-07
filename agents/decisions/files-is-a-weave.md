# Files is a weave

The project browser was a built-in pane of the Workshop host. It is `Zengine/files/` now: a
shared library, loaded from a row in an editable plan, holding the office `zengine.files` and
offering the pane `project-files` across the external pane protocol. Workshop compiles nothing
for it, mints no kind for it, and learns its row from a live offer.

## What forced it

Nothing about the browser was broken. What was wrong was the SHAPE of the claim this
application makes: that a pane arrives by a plan row, that a host discovers panes rather than
being taught them, and that a maker can replace one. Six panes were exceptions to that, and
"except the ones we wrote" is not a rule. This is the first of them to stop being an exception,
and it was chosen first because it is the one that reads the most from its host and therefore
the one whose seam is worth designing carefully.

## What crosses, and what does not

Four facts the browser read straight off `HostContext` cannot travel into a loaded image as
anything but values, so each became an ask to an office and an answer back:

    zengine.project    read-only    ProjectRootRequested  -> ProjectRoot
    zengine.recipes    ACTS         RecipeUseRequested     -> RecipeOutcome
                                    RecipeAuthorRequested  -> RecipeOutcome
    zengine.workshop   ACTS         OpenSourceRequested    -> SourceOpened

THE SPLIT IS BY WHETHER ANSWERING ACTS, which is the observation door's own division: a
question whose answer runs nobody's code lives apart from one whose whole purpose is to change
a maker's files, so "which office can write a recipe catalog" keeps a one-word answer. The
Editor's door is addressed at `zengine.workshop` because Workshop still holds the Editor; when
the Editor migrates, the same sentence goes to `zengine.editor` and only the address moves.

No `Session&`, no `HostContext&`, no `CurrentRecipes&` and no host address crosses. Every field
is Text, Int, Bool or a List of those. And knowledge is not authority: a weave that hears where
the project is has been permitted to read nothing under it.

## Three things the code decided differently from the plan

A PANE IS ONE KEYBOARD CONTEXT. The built-in bound Return three ways, in three contexts
(`kFiles`, `kRecipeChooser`, `kAuthoring`). A pane's declared rows join into ONE map under its
runtime handle, and the collision law refuses a second row on a gesture already taken — so
`files.choose` could not exist beside `files.open`, and `files.parent` on Backspace would have
made the authoring line unable to delete a character. The answer is that a pane DECLARES WHAT
IS TRUE NOW and re-declares when its mode changes: while the line has the keyboard, two rows
are declared and every other key arrives as an ordinary key. The ids never move, so a maker's
authored keymap keeps working, which was the promise.

THE CONVERSION IS NOT A FORMAT VERSION. A saved setup naming `zengine.workshop/project-files`
is a legal file in the version it was written at; what changed is what that reference RESOLVES
to, which the setup format has always said is somebody else's question. Bumping the format
would have converted version-3 files and refused version-2 ones outright — and the browser is
older than either number. So the rewrite runs inside `setup_in`, the one function that turns
written rows into a live `Setup`, and reaches the setup file, every desk in a session and every
remembered value on a link, over every version the reader admits.

THE BUILDER PANEL LOST ITS CATALOG ROW. It named the recipe catalog in force while that had
moved from the launch default, out of a projection the browser wrote onto the session as it
made the swap. The browser says the same sentence in its own row now, at the moment of the
change; nothing left in the host can keep a projection honest, and `RecipeCatalog` — the
message the panel's rows really come from — carries the recipes and not the file they came
from. A panel showing a stale path would be worse than one showing none.

## What it costs

The image carries the document/UI closure that `persist.hpp` drags in behind the marks file,
which is more than the browser needs. The pane protocol carries no caret, so the authoring
line shows its text and no caret — the same documented loss the Powers query keeps, and the
contract the Editor's own migration would have to move. Hover reveal did not come across
(WL-PTR-09): a pane's rows are values, and reveal reads a region the host laid out.

## What it buys

One fewer exception. A maker can remove the row and have no Files pane, or write their own and
have a different one, and neither is a change to Workshop. Every remaining built-in has a
worked example of what its migration costs — and the three doors here are the shape the next
one will take, because what a pane needs from a host is nearly always a fact it used to reach
through a reference.

**Laws supported.** [WL-FILES-16](../workshop/files.md), [WL-FILES-17](../workshop/files.md).
