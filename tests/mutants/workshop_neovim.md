# The mutants `tests/test_workshop_neovim.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_neovim.cpp`](../test_workshop_neovim.cpp) claims its cases catch, the comment
naming the entry by its path and heading. [The corpus's front page](README.md) says how an entry
reads and how `tests/mutants/run.py` runs it.

## status-row-always-drawn

The row above Neovim is drawn whatever `status` says.

In `editor/neovim-editor/pane.cpp`:

```diff
-        return status_shown_ || !notice_.empty() || !running() || !host_->ready() ? kChromeRows : 0;
+        return true || !notice_.empty() || !running() || !host_->ready() ? kChromeRows : 0;
```

Red in `zengine-workshop-neovim-tests`:

- the Neovim editor declares `profile` and `status`: `status` off gives Neovim the row above it, and
  a layout's `user` is the profile its next start runs where the variable names none

## profile-from-the-variable-alone

A start reads the profile from the variable alone, whatever the layout keeps.

In `editor/neovim-editor/pane.cpp`:

```diff
-        choice_ = nv::choice_from_environment(profile_);
+        choice_ = nv::choice_from_environment();
```

Red in `zengine-workshop-neovim-tests`:

- the Neovim editor declares `profile` and `status`: `status` off gives Neovim the row above it, and
  a layout's `user` is the profile its next start runs where the variable names none
