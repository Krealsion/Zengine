# The mutants `tests/test_workshop_probe.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_probe.cpp`](../test_workshop_probe.cpp) claims its cases catch, the comment
naming the entry by its path and heading. [The corpus's front page](README.md) says how an entry
reads and how `tests/mutants/run.py` runs it.

## capture-of-the-os-window-alone

The probe asks the OS window alone for a picture.

In `examples/workshop-probe/probe.hpp`:

```diff
-            if (capture_office_at_ + 1 < kCaptureOffices.size()) {
+            if (false) {
```

Red in `zengine-workshop-probe-tests`:

- probe: a picture is asked of the OS window, then the terminal window, and fetched from the office
  that described it

## chunks-of-the-os-window

The probe asks the OS window for each chunk, whichever office described the picture.

In `examples/workshop-probe/probe.hpp`:

```diff
-        ask(mail, capture_office(),
-            zengine::graphics::SurfaceCaptureChunkRequested{capture_, fetched_});
+        ask(mail, kCaptureOffices[0],
+            zengine::graphics::SurfaceCaptureChunkRequested{capture_, fetched_});
```

Red in `zengine-workshop-probe-tests`:

- probe: a picture is asked of the OS window, then the terminal window, and fetched from the office
  that described it

## stopped-on-the-last-refusal

The run stops on the last office's refusal rather than the first.

In `examples/workshop-probe/probe.hpp`:

```diff
-            fail(mail, first_capture_refusal_);
+            fail(mail, said);
```

Red in `zengine-workshop-probe-tests`:

- probe: when no office holds the picture the run stops on the first refusal, and a refusal while
  fetching asks no other office

## falls-back-mid-fetch

A refusal while fetching sends the run on to the next office, as one while capturing does.

In `examples/workshop-probe/probe.hpp`:

```diff
-        if (step_ == Step::Capture && o.state == loom::link::kOutcomeDispatchRefused) {
+        if ((step_ == Step::Capture || step_ == Step::Fetch) &&
+            o.state == loom::link::kOutcomeDispatchRefused) {
```

Red in `zengine-workshop-probe-tests`:

- probe: when no office holds the picture the run stops on the first refusal, and a refusal while
  fetching asks no other office
