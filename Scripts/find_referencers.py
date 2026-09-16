"""
Which objects inside a Blueprint's package reference a given asset/class/object (with the
property holding the reference when known). The definitive way to find the last hard reference.
  -bp=/Game/Path/BP_X                 required
  -target=/Game/Path/BP_Y.BP_Y        required (asset path; a Blueprint also checks its class + CDO)
"""
import unreal
from editorbridge import args, log, load_blueprint

a = args()
bp = load_blueprint(a["bp"])
lines = unreal.EditorBridgeBlueprintTools.find_referencers(bp, a["target"])
log("%d referencer(s) of %s in %s" % (len(lines), a["target"], bp.get_name()))
for line in lines:
    log("   " + line)
