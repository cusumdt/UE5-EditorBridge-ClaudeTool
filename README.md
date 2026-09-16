# EditorBridge: script the Unreal Editor from an AI coding agent

An Unreal Engine 5 editor plugin that lets a coding agent (built for [Claude Code](https://claude.com/claude-code), usable by anything that can write files and run PowerShell) **inspect and modify Blueprints, components, materials and meshes by script**, either inside the running editor or headless.

It exists because the stock scripting surface has gaps: `BlueprintEditorLibrary` cannot create nodes, re-point variable references across classes, fix orphaned pins, tell you *who* still references an asset inside a package, or measure ray tracing geometry memory. This plugin exposes those operations to Python and adds a small file-based bridge so scripts run inside the open editor without any network socket.

Born from a production VR project (UE 5.7) where the agent refactored a 30-graph controller Blueprint, moved a Blueprint hierarchy onto a C++ base, generated LODs for 4M-triangle CAD meshes and audited ray tracing memory, all by script and with every step verifiable.

## What you get

| Piece | Purpose |
|---|---|
| `Source/EditorBridge` (editor module) | Three Python-visible function libraries: **BlueprintTools** (variables, references, components, compile), **GraphTools** (read graphs, add/connect/delete nodes, retype macro pins), **AssetTools** (loaded static meshes with LOD/triangle layout and resident ray tracing BLAS memory). |
| `Content/Python/init_unreal.py` | **The bridge.** Polls `<Project>/Saved/EditorBridge/inbox/` once per second and executes each `.py` inside the editor, in one undoable transaction, writing the output to `outbox/<name>.log`. |
| `Content/Python/editorbridge/` | Tiny helper so the same script runs through the bridge or the commandlet (`args()`, `log()`, `load_blueprint()`, `compile_and_report()`). |
| `Scripts/` | Ready-made, argument-driven scripts (dump graphs, find references, compile, rename/retarget variables, fix orphaned pins, LODs, component properties, material parameters, ray tracing audit). |
| `Scripts/bridge_run.ps1` | Submits a script + args to the bridge and prints the result (streams progress). |
| `claude/` | Drop-in templates for Claude Code: a `CLAUDE.md` section, five agents (`bp-inspector`, `bp-migrator`, `ue-builder`, `asset-auditor`, `ue-code-reviewer`) and two skills (`/ue-build`, `/bp-check`). |

## Requirements

- Unreal Engine **5.7** (tested). 5.5/5.6 will need small API adjustments (`FStaticMeshRayTracingProxy`, `FStreamableHandle`); PRs welcome.
- A C++ project (the plugin has a C++ module, so the project must build code).
- Plugins `PythonScriptPlugin` and `EditorScriptingUtilities` (enabled automatically as dependencies).
- Windows for `bridge_run.ps1` (the bridge itself is plain Python and works anywhere).

## Install

```bash
# as a submodule (recommended: you can pull updates)
git submodule add https://github.com/cusumdt/UE5-EditorBridge-ClaudeTool.git Plugins/EditorBridge
# or just copy the repository into <Project>/Plugins/EditorBridge
```

Add to your `.uproject`:

```json
"Plugins": [
  { "Name": "EditorBridge", "Enabled": true }
]
```

Build the editor target once (editor closed):

```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" MyProjectEditor Win64 Development -Project="D:\Proj\MyProject.uproject" -WaitMutex -NoHotReload
```

Open the editor. The Output Log shows `[bridge] EditorBridge active - inbox: ...`.

For Claude Code: copy `claude/agents/*` and `claude/skills/*` into your project's `.claude/`, paste `claude/CLAUDE.snippet.md` into your `CLAUDE.md` and fill in the engine/project paths.

## Quick start

```powershell
# with the editor OPEN: list the graphs of a Blueprint
.\Plugins\EditorBridge\Scripts\bridge_run.ps1 -Project "D:\Proj\MyProject.uproject" `
    -Script .\Plugins\EditorBridge\Scripts\dump_graph.py -Args "-bp=/Game/Blueprints/BP_Controller"

# dump one graph (nodes, pins, links)
... -Script .\Plugins\EditorBridge\Scripts\dump_graph.py -Args "-bp=/Game/Blueprints/BP_Controller -graph=EventGraph"

# who still hard-references BP_OldTruck inside BP_Controller?
... -Script .\Plugins\EditorBridge\Scripts\find_referencers.py -Args "-bp=/Game/Blueprints/BP_Controller -target=/Game/Trucks/BP_OldTruck.BP_OldTruck"

# with the editor CLOSED: same script, args on the command line
& "C:\...\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\Proj\MyProject.uproject" -run=pythonscript `
    -script="D:\Proj\Plugins\EditorBridge\Scripts\dump_graph.py" -bp=/Game/Blueprints/BP_Controller -unattended -nopause -nosplash -stdout -FullStdOutLogOutput
```

Writing your own script:

```python
import unreal
from editorbridge import args, log, load_blueprint, compile_and_report

a = args({"graph": "EventGraph"})              # -bp=... -graph=... from bridge sidecar or command line
bp = load_blueprint(a["bp"])
G = unreal.EditorBridgeGraphTools
guid = G.add_call_function_node(bp, unreal.Name(a["graph"]), unreal.KismetSystemLibrary.static_class(),
                                unreal.Name("PrintString"), 0, 0)
G.set_pin_default(bp, unreal.Name(a["graph"]), guid, unreal.Name("InString"), "hello from the bridge")
compile_and_report(bp, save=False)            # look at it in the editor first; Ctrl+Z undoes the whole script
```

## Python API

All functions are `static`, exposed as `unreal.EditorBridgeBlueprintTools.*`, `unreal.EditorBridgeGraphTools.*`, `unreal.EditorBridgeAssetTools.*` (snake_case in Python). Nodes are addressed by their `NodeGuid` string, pins by name.

### EditorBridgeBlueprintTools

| Function | What it does |
|---|---|
| `rename_variable_references(bp, variable_class, old, new)` | Re-points every variable node referencing `variable_class::old` (or a subclass) to `new`. Unlike the stock `ReplaceVariableReferences`, it does not depend on the dependency cache and works on external references. |
| `retarget_member_references(bp, from_class, to_class)` | Variable/function nodes whose member parent is `from_class` now point at `to_class` (when it declares the member). Needed after moving members to a C++ parent: nodes keep hard-referencing the old Blueprint class otherwise. |
| `remove_member_variable`, `list_member_variables`, `list_member_variable_types`, `list_local_variables`, `change_member_variable_type` | Variable management. |
| `list_variable_references(bp, name)` | Every variable node: graph, title, `Owner::Name`, `(UNRESOLVED)` marker. |
| `rewire_orphaned_pins(bp) -> (removed, unfixable)` | Fixes "In use pin X no longer exists on node": moves links from orphaned pins to the live pin of the same name; reports links the schema refuses. |
| `refresh_all_nodes(bp)` | File → Refresh All Nodes. |
| `compile_blueprint_with_log(bp) -> (messages, has_errors)` | Compile with captured compiler messages. |
| `get_parent_class(bp)`, `list_components(bp)`, `set_component_static_mesh(bp, component, mesh)` | Class / SCS helpers (inherited components and overrides included). |
| `find_referencers(bp, target_path)` | Objects inside the Blueprint's package that reference the target (asset, its class, its CDO), with the property when known. The definitive tool for hunting the last hard reference. |

### EditorBridgeGraphTools

| Function | What it does |
|---|---|
| `list_graphs(bp)`, `list_nodes(bp, graph)` | Text dump: `guid \| class \| title \| (x,y)` and pins `in\|out name : type [= default] [ORPHAN] -> guid.pin`. |
| `add_call_function_node`, `add_variable_get_node`, `add_variable_set_node`, `add_cast_node`, `add_event_node`, `add_custom_event_from_delegate`, `add_bind_delegate_node` | Create nodes; return the new guid. |
| `connect_pins`, `disconnect_pin`, `delete_node`, `move_node`, `set_pin_default` | Edit (connections go through the schema and are type-checked). |
| `set_graph_pin_type(bp, graph, pin, category, sub_object)` | Change the type of a macro/function input or output; refreshes macro instances. |

### EditorBridgeAssetTools

| Function | What it does |
|---|---|
| `list_loaded_static_meshes(only_game_content)` | Every static mesh loaded in the session: LOD count, triangles per LOD, RT support, Nanite, BLAS memory resident now and always resident (last LOD). Sorted by always-resident; `TOTAL` line at the end. |

## Scripts

| Script | Arguments | Modifies assets |
|---|---|---|
| `dump_graph.py` | `-bp`, `-graph="A;B"` | no |
| `inspect_blueprint.py` | `-bp`, `-var` | no |
| `list_components.py` | `-bps`, `-own=1` | no |
| `find_class_refs.py` | `-bp`, `-text="a;b"` | no |
| `find_referencers.py` | `-bp`, `-target` | no |
| `list_hard_deps.py` | `-assets="a;b"`, `-all=1` | no |
| `audit_rt_meshes.py` | `-top`, `-all=1` | no |
| `compile_blueprints.py` | `-bps`, `-save=1` | saves if asked |
| `rename_variable.py` | `-bp`, `-old`, `-new`, `-dependents`, `-remove_old=1`, `-retarget=<class>` | yes |
| `retarget_member_refs.py` | `-bps`, `-from`, `-to` | yes |
| `fix_orphaned_pins.py` | `-bps` | yes |
| `remove_unused_macros.py` | `-bp`, `-macros` | yes (only unused macros) |
| `set_component_property.py` | `-bps`, `-components`, `-property`, `-value` | yes |
| `set_material_params.py` | `-material`, `-scalars`, `-vectors` | yes |
| `generate_lods.py` | `-meshes`, `-percents`, `-save=0` | yes (slow on big meshes) |

## How the bridge works, and its limits

- **File based, local only.** No TCP port, no external process inside the editor. Scripts are picked up from `<Project>/Saved/EditorBridge/inbox/` (override with `EDITOR_BRIDGE_DIR`), moved to `done/` before running, executed with `exec` in the editor's Python, output goes to `outbox/<name>.log` (`.progress` while running). Anyone who can write to that folder can run code in your editor, which is the same trust level as writing to your project folder.
- **One transaction per script**: Ctrl+Z in the editor reverts everything the script did (unless the script saved assets).
- **One script per tick, re-entrancy guarded.** Long editor operations (mesh builds, saves) pump the UI loop; the guard prevents the same script from being picked up twice.
- **Blocking**: the editor is unresponsive while a script runs. Mesh reduction on multi-million-triangle assets takes tens of minutes and gigabytes of RAM, so run one such asset per script.
- **PIE**: `EditorAssetLibrary` returns nothing while Play In Editor is active; stop Play first.
- **C++ still needs the editor closed** (Live Coding blocks UBT). The bridge is for content.
- Disable at any time by creating `<Project>/Saved/EditorBridge/bridge.disabled`.

## Recommended workflow for agents

1. **Look**: `dump_graph.py` / `inspect_blueprint.py`. Decide on the real graph, not on asset names. Follow reroute knots to the real source.
2. **Plan**: scripts that modify graphs should support `-dry=1` and print nodes to create/delete and connections.
3. **Apply**: create nodes first, delete old ones after, wire last; log connections the schema refuses.
4. **Verify**: `compile_blueprint_with_log`; when cutting a dependency, `find_referencers` must return nothing and `list_hard_deps.py` must not list the package.
5. **Save** only when clean; otherwise leave it undoable.

## Roadmap

- Copy/paste nodes between graphs and Blueprints (`FEdGraphUtilities::ExportNodesToText`).
- `Branch` / `Sequence` / `Switch` node creation helpers.
- Engine version guards for 5.5/5.6.
- Non-Windows runner (bash) for `bridge_run`.

## License

MIT, see [LICENSE](LICENSE).
