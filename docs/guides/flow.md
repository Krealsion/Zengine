# Make and run a Flow definition

Flow is a standalone terminal workbench for message-driven maker definitions. You can compose
existing operators, run a definition, change its behavior while keeping state, and generate
loadable C++. Workshop is not required. The graph view is textual, with an SVG export.

## Start the workbench

Build Zengine against an installed Loom with kernel support, then build `zengine-flow-tool`:

```sh
cmake --build build --target zengine-flow-tool
```

Run `build/flow/zengine-flow` on Linux or `build/flow/zengine-flow.exe` on Windows. Multi-config
generators put the executable in the selected configuration directory. Installing Zengine puts
`zengine-flow` in the install prefix's `bin` directory.

The interactive prompt accepts one command per line. `help` lists the commands. To run a file of
the same commands, use `zengine-flow --script commands.txt`; the first refused command ends a
script with a nonzero exit. Interactive errors leave the prompt available. An accepted `send`
can still produce an application-level `zen.Refused` message; scripts must inspect these printed
outcomes, since message refusal itself does not change the process exit code.

## Try a stateful application

Enter:

```text
example
graph
send Reading degrees=17
send Reading degrees=20
send Reading degrees=23
send Reading degrees=20
```

This thermostat starts with lower and upper thresholds of 18 and 22. The four readings produce
`heating=true`, `true`, `false`, `false`. Between the thresholds it keeps its prior state.
It publishes a `thermostat.Status` after each successful input. It is a rule simulation; no
temperature sensor or heater is connected.

`send Lower lower=16` changes the lower threshold, constrained by the upper one.
`send Upper upper=24` changes the upper threshold, constrained by the lower one.
Threshold changes take effect on the next reading.

## Change behavior while it runs

This edit deliberately replaces the reading rule with one that always turns heating off:

```text
edit
on Reading heating
node logic.select_bool true false $heating
result 0
emit Status heating=$heating on_below=$on_below off_above=$off_above
end
apply
state
send Reading degrees=10
```

`apply` keeps the current state, including changes made by messages while the draft was open.
The next reading uses the edited rule. `cancel` discards a draft. A live behavior edit keeps the
state schema and the accepted/emitted message contracts; a schema change needs replacement.

The definition revision increases when `edit` starts. A trigger is replaced only when its
`end` command completes; other triggers remain. The graph's nodes run in declaration order,
including nodes after the selected result. `%0` names an earlier node's answer; `$heating`
names a field in the state-and-message pack.

## Author something else

The example is made through the same commands. Here is a complete new high-water definition:

```text
new readings
state high Int 0
accept Sample value:Int
publish HighWater high:Int
on Sample high
node math.max $high $value
result 0
emit HighWater high=$high
end
run
send Sample value=7
send Sample value=3
```

`operators` shows the four basic operators and their input/output kinds. Loading additional
operator providers is currently an embedding-host capability; this workbench has no provider-load
command. The scalar commands
support `Int`, `Bool`, `Float`, and `Text` fields. A `?` after a message field kind makes it
optional. Node constants currently support Int and Bool, as the maker composition format does.
Quote text and paths containing spaces. Within quoted text, backslash escapes the next character;
forward slashes are convenient in Windows paths.

For full maker schemas, including Bytes, nested Message/List fields and conversion metadata,
use `export-json directory`, edit the definition/state JSON projections, then use
`import-json directory/definition.json directory/state.json`. These projections use Loom's
existing debug codec and pass through the same definition and value admission. They are not a
second type system or the durable save format. Project saves and JSON imports have a 1 MiB file
limit. Generated C++ has a separate 64 MiB workbench limit, checked before writing and used again
when reading it for recovery or regeneration. The C++ API also accepts ordinary maker definitions.

## Save, generate and return

```text
save thermostat.flow
graph thermostat.svg
generate generated-thermostat
```

`save` writes the definition and current state together. `open thermostat.flow` restores both
in a fresh interpreted session. It does not restore queued messages. Finish pending work with
`pump` before switching forms; the workbench refuses a switch while work remains queued.

The generated directory contains `generated.cpp`, `definition.bin`, `graph.svg` and a CMake
project. Build it against installed Zengine and Loom packages:

```sh
cmake -S generated-thermostat -B generated-thermostat/build -DCMAKE_PREFIX_PATH="<zengine-prefix>;<loom-prefix>"
cmake --build generated-thermostat/build
```

Back in the workbench, `native generated-thermostat/build/flow-generated.so` switches to the
matching compiled artifact while retaining current state. On Windows use the `.dll` path,
including a configuration subdirectory when applicable. The switch prepares a fresh session
first; if preparation fails, the current session remains. `interpret` switches back with state
preserved. These session switches are local workbench operations, not a general replacement
manager for a multi-participant application.

The compiler is only needed for native generation. Interpreted authoring and behavior edits do
not compile anything. Native code loading executes the artifact you selected; the workbench is
a development host, not a security sandbox. It grants the weave its declared emissions and
standard maker replies, rather than granting unrestricted message authority.

To recover the definition from untouched generated C++:

```sh
zengine-flow --recover generated-thermostat/generated.cpp recovered.definition
```

Recovery regenerates the entire source and compares it byte for byte. If you edit the C++,
recovery refuses to claim those edits are represented by the embedded definition. Handwritten
C++ remains yours to maintain. Generation also refuses to overwrite edited generated source or
an edited generated `CMakeLists.txt`; choose a new output directory to keep both.

See the [Flow reference](../reference/flow.md) for the host API and exact native boundary.
