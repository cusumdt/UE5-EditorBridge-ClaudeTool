<!-- Paste this section into your project's CLAUDE.md and adjust the paths. -->

## Unreal Editor scripting (EditorBridge plugin)

Engine: `<ENGINE_ROOT>` (e.g. `C:\Program Files\Epic Games\UE_5.7`). Project: `<PROJECT>.uproject`.
Plugin: `Plugins/EditorBridge` (Python API `unreal.EditorBridgeBlueprintTools`,
`unreal.EditorBridgeGraphTools`, `unreal.EditorBridgeAssetTools`; scripts in `Plugins/EditorBridge/Scripts`).

```powershell
# C++ build — the editor MUST be closed (Live Coding blocks UBT). Check with Get-Process UnrealEditor
& "<ENGINE_ROOT>\Engine\Build\BatchFiles\Build.bat" <PROJECT>Editor Win64 Development -Project="<PROJECT_DIR>\<PROJECT>.uproject" -WaitMutex -NoHotReload

# Python inside the OPEN editor (bridge): result in <PROJECT_DIR>/Saved/EditorBridge/outbox/<name>.log
.\Plugins\EditorBridge\Scripts\bridge_run.ps1 -Project "<PROJECT_DIR>\<PROJECT>.uproject" -Script <file.py> -Args "-bp=/Game/X -graph=EventGraph"

# Python with the editor CLOSED (commandlet); same scripts, args on the command line
& "<ENGINE_ROOT>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<PROJECT_DIR>\<PROJECT>.uproject" -run=pythonscript -script="<file.py>" -bp=/Game/X -unattended -nopause -nosplash -stdout -FullStdOutLogOutput
```

Rules:
- Blueprints are edited **by script, never by hand**. Flow: `dump_graph.py` to see the real graph →
  dry-run plan → apply → `compile_blueprint_with_log` → `find_referencers` / `list_hard_deps.py`
  when the goal was to cut a dependency → save. With the bridge each script is one editor
  transaction (Ctrl+Z undoes it).
- Before editing a Blueprint through the bridge, make sure the user does not have it open with
  unsaved changes. `EditorAssetLibrary` does not work while the editor is in PIE: stop Play first.
- Long operations (LOD generation, mesh builds) block the editor; run one heavy asset per script.
- Nodes are addressed by `NodeGuid` (stable across saves), pins by name (`execute`/`then`/`self`).
- Never add AI attribution (Co-Authored-By etc.) to commits.
