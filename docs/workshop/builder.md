# Workshop's Builder

**Reference, current state.** What the Builder pane can actually do today, said without
aspiration. The package underneath it — which is more capable than Workshop's use of it — is
[the Builder package](../reference/builder.md).

## What it does today

Press **`b`** in command mode, or **`p`** → `Builder` → `Enter` to open the pane and watch.

That starts **one target**, and the target is fixed when Zengine is configured:

```cmake
# workshop/CMakeLists.txt
"ZENGINE_BUILDER_TARGET=\"zengine-snake\""
"ZENGINE_BUILDER_CMAKE=\"${CMAKE_COMMAND}\""
"ZENGINE_BUILDER_BUILD_DIR=\"${CMAKE_BINARY_DIR}\""
```

So the shipped Workshop builds `zengine-snake`, in Zengine's own build tree, with the CMake
that configured it. The catalog the runner holds contains exactly that one recipe. A `RunBuild`
naming anything else is refused by name and nothing runs.

Workshop prints the actual command on the way up, in plain scrollback, because what a key in
this program will run is a fact you are entitled to before you press it — and the pane cannot
show it until the runner has started it, since the tool holds no command.

## The answers to the obvious questions

| question | answer today |
|---|---|
| What can Builder build? | one target, chosen at configure time. As shipped: `zengine-snake` |
| How does a user select a target? | **they cannot.** There is no target picker and no way to add a recipe at run time |
| Where does output go? | into the Builder pane, as `BuildOutput` lines the runner drained from the child's pipe |
| How does the user know progress? | the pane shows the target, whether the tool has answered yet, and what it last heard: started / output / finished with a status / not started |
| Can output automatically become loadable? | **no.** Nothing connects a finished build to the [load plan](load-plans.md) or to the Kernel |
| Can a successful artifact be mounted or loaded? | **not from Workshop.** There is no run-time load path at all; see [lifecycle](limitations.md#lifecycle) |
| Can failure be retried? | **yes** — press `b` again. There is no retry button and no backoff; it is the same gesture |
| What is its relationship to the load plan? | **none.** They are unconnected: the plan decides what this run is made of at startup, the Builder starts a process. Nothing passes between them |

## So what is it, honestly

**It is a proof that starting and following an external process works, not a project builder.**

The proof is real and worth having. Three things it establishes:

- **The build outlives the handler that started it.** The runner takes move-only custody of the
  child process and the read end of its pipe, and looks at what it holds on an ordinary
  repeating beat. Every line of it runs inside an ordinary handler on the ordinary execution
  thread — there is no thread, no queue, no scheduler and no async runtime. A build that takes
  a minute does not stop the application for a minute. The regression canary measures the old
  blocking runner carrying **zero** unrelated deliveries between a build's start and its end.
- **Process authority lives in exactly one place.** One weave in the program starts processes,
  holds a catalog written by the host, and may report four observations to whoever holds the
  Builder office. It cannot paint, publish, load a weave, or reach the Manager. Workshop itself
  gains two sentences — *ask the Builder what it is*, *ask it to build the name it told me* —
  and no powers.
- **The wire cannot spell a command.** No shape in this vocabulary has a field that is a
  program, an argument vector, a directory or a shell line. "The panel sent a command" is not
  a sentence these types can express.

That last point is why this is not one flag away from being a project builder. Making Builder
useful to a maker means deciding **who may name a recipe, and how a maker's project gets one**
— which is an authority question, not a UI question. A text field that reached a process would
undo the property above.

## What a user would reasonably expect next

Ranked by how often the absence is felt, not by size:

1. **More than one target**, and a way to choose. Even a fixed list from the host would end the
   "it builds something I did not ask for" experience.
2. **A recipe that names the maker's project**, not Zengine's build tree — which needs a place
   for a maker to say where their project is, and a decision about whether Workshop may run a
   program a maker named.
3. **A finished artifact becoming loadable**, which is the join to `edit → build → load →
   observe` and is blocked on there being any run-time load path at all.

None of the three is designed here. See [limitations](limitations.md).
