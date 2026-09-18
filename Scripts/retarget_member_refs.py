"""
Re-points variable / function-call nodes whose member parent is one class to another class
that declares the same member (typical after moving members to a C++ base). Compiles.
  -bps="/Game/A;/Game/B"                       required
  -from=/Game/Path/BP_X.BP_X_C                 required (class path)
  -to=/Script/MyModule.MyBaseActor             required (class path)
  -save=1                  optional: write the changed assets to disk (default: leave them
                           dirty and undoable; the bridge lists what was saved)
"""
import unreal
from editorbridge import args, arg_list, flag, log, load_blueprint, compile_and_report, load_class

a = args()
T = unreal.EditorBridgeBlueprintTools
src, dst = load_class(a["from"]), load_class(a["to"])
for path in arg_list(a["bps"]):
    bp = load_blueprint(path)
    n = T.retarget_member_references(bp, src, dst)
    log("%s: %d node(s) retargeted" % (bp.get_name(), n))
    if n:
        compile_and_report(bp, save=flag(a.get("save")))
