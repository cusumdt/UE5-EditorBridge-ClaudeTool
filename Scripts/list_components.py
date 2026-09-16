"""
Components of one or more Blueprints (own + inherited, with static mesh assignments).
  -bps="/Game/A;/Game/B"   required
  -own=1                   optional: hide inherited components that are not overridden
"""
import unreal
from editorbridge import args, arg_list, log, load_blueprint

a = args()
for path in arg_list(a["bps"]):
    bp = load_blueprint(path)
    log("==== " + bp.get_name())
    for line in unreal.EditorBridgeBlueprintTools.list_components(bp):
        if a.get("own") and "(inherited)" in line: continue
        log("  " + line)
