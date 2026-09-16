"""
Ray tracing memory audit of the static meshes LOADED in the current editor session: top meshes
by always-resident BLAS (last LOD), LOD/triangle layout and totals. Read-only.
  -top=40   optional
  -all=1    optional: include engine/plugin meshes
"""
import unreal
from editorbridge import args, log

a = args({"top": "40"})
lines = unreal.EditorBridgeAssetTools.list_loaded_static_meshes(not a.get("all"))
meshes = [l for l in lines if not l.startswith("TOTAL")]
log([l for l in lines if l.startswith("TOTAL")][0])
log("%-70s %5s %3s %4s %7s %8s  %s" % ("mesh", "LODs", "RT", "Nan", "resid", "always", "tris"))
for l in meshes[:int(a["top"])]:
    parts = [p.strip() for p in l.split(" | ")]
    kv = dict(p.split("=", 1) for p in parts[1:])
    log("%-70s %5s %3s %4s %7s %8s  %s" % (parts[0].split(".")[0][-70:], kv["LODs"], kv["RT"], kv["Nanite"], kv["resident"], kv["alwaysResident"], kv["tris"]))
log("meshes with a single LOD and RT support: %d of %d" % (sum(1 for l in meshes if "LODs=1 |" in l and "RT=1" in l), len(meshes)))
