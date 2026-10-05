# Providers request comfortable body space

**Decision.** A versioned offer carries preferred body rows and columns. Workshop converts them
with the current text metric and reserves title/chrome space. The first admitted preference is
stable for the runtime pane identity; authored sizes win per axis. Stack seating and bounds use
the same preferred height. Version 1 keeps its fallback. A canvas pane asks at version 3 for a
body of whole pixels and rows of text beneath it, and is granted exactly that body, rounded up to
the cells that hold it where a medium cannot say a pixel ([the-whole-pixel](the-whole-pixel.md)).

**Why.** Uniform canvas rectangles can leave a list with fewer useful items than its omission
markers. The provider knows its content needs; the host knows how text fits the current medium.
Neither needs to know the other's screen coordinates. Saved layouts remain weaver intent.

**Alternatives considered.**
- *Changing the existing offer's fields* — argued against: that changes a published schema
  identity. Version 2 makes the extension explicit and retains small version-1 providers.
- *A host table of pane-specific sizes* — argued against: every new pane would need a host edit.
- *Reapplying preferences on each offer* — argued against: a refresh or content update could
  move a weaver's working desk. The refresh test pins retention of the initial preference.
- *Enforcing a minimum size* — argued against: the weaver may deliberately choose a smaller
  pane. The two-medium geometry test pins the override and reports less room honestly.

**Limits.** Preferences fit the available workspace; they are not a tiling or overlap-avoidance
system. Reset-to-default returns to this preference. Large defaults can wait for reactive room;
an authored place remains independent of stack capacity.

**Since.** No room rations the stack: the column spends the preferred height and begins again
at its top when spent, so a large default never waits
([the stack begins again](the-stack-begins-again.md)).

**Laws supported.** [WL-PANE-17](../workshop/panes-and-windows.md).
