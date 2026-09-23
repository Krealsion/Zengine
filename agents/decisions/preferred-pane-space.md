# Providers request comfortable body space

**Decision.** A versioned offer carries preferred body rows and columns. Workshop converts them
with the current text metric and reserves title/chrome space. The first admitted preference is
stable for the runtime pane identity; authored sizes win per axis. Stack seating and bounds use
the same preferred height. Version 1 keeps its fallback.

**Why.** Uniform canvas rectangles can leave a list with fewer useful items than its omission
markers. The provider knows its content needs; the host knows how text fits the current medium.
Neither needs to know the other's screen coordinates. Saved layouts remain maker intent.

**Alternatives considered.**
- *Changing the existing offer's fields* — argued against: that changes a published schema
  identity. Version 2 makes the extension explicit and retains small version-1 providers.
- *A host table of pane-specific sizes* — argued against: every new pane would need a host edit.
- *Reapplying preferences on each offer* — argued against: a refresh or content update could
  move a maker's working desk. The refresh test pins retention of the initial preference.
- *Enforcing a minimum size* — argued against: the maker may deliberately choose a smaller
  pane. The two-medium geometry test pins the override and reports less room honestly.

**Limits.** Preferences fit the available workspace; they are not a tiling or overlap-avoidance
system. Reset-to-default returns to this preference. Large defaults can wait for reactive room;
an authored place remains independent of stack capacity.

**Laws supported.** [WL-PANE-17](../workshop/panes-and-windows.md).
