"""
Fixes "In use pin X no longer exists on node Y": refreshes all nodes, moves links of orphaned
pins to the live pin with the same name, reports links the schema refuses and compiles.
  -bps="/Game/A;/Game/B"   required
  -save=1                  optional: write the changed assets to disk (default: leave them
                           dirty and undoable; the bridge lists what was saved)
"""
import unreal
from editorbridge import args, arg_list, flag, log, load_blueprint, compile_and_report

a = args()
T = unreal.EditorBridgeBlueprintTools
for path in arg_list(a["bps"]):
    bp = load_blueprint(path)
    T.refresh_all_nodes(bp)
    removed, unfixable = T.rewire_orphaned_pins(bp)
    log("%s: %d orphaned pin(s) removed" % (bp.get_name(), removed))
    for u in unfixable: log("    NOT re-connected: " + u)
    compile_and_report(bp, save=flag(a.get("save")))
