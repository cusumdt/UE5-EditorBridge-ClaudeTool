"""
Generates LOD chains for static meshes (auto reduction, auto screen sizes).
Skips meshes that already have that many LODs. Heavy meshes are slow and blocking (a 4 M
triangle mesh takes ~35 min and ~9 GB of RAM): run ONE per invocation for those.
  -meshes="/Game/A;/Game/B"           required
  -percents="1.0,0.5,0.25,0.10"       optional: triangle percentage per LOD (LOD0 must be 1.0)
  -save=1                             optional: write the meshes to disk. The LOD build itself is
                                      NOT undoable (it rebuilds render data), so for a long run pass
                                      -save=1 or Ctrl+S afterwards; the bridge lists what was saved.
"""
import time
import unreal
from editorbridge import args, arg_list, flag, log

a = args({"percents": "1.0,0.5,0.25,0.10", "save": "0"})
percents = [float(x) for x in a["percents"].split(",")]
sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
options = unreal.StaticMeshReductionOptions()
options.auto_compute_lod_screen_size = True
settings = []
for pct in percents:
    s = unreal.StaticMeshReductionSettings(); s.percent_triangles = pct; s.screen_size = pct
    settings.append(s)
options.reduction_settings = settings
tris = lambda m: [m.get_num_triangles(i) for i in range(m.get_num_lods())]

for path in arg_list(a["meshes"]):
    mesh = unreal.EditorAssetLibrary.load_asset(path)
    if not mesh:
        log("NOT FOUND: " + path); continue
    before = tris(mesh)
    if len(before) >= len(percents):
        log("%s: already has %d LODs %s, skipped" % (mesh.get_name(), len(before), before)); continue
    t0 = time.time()
    n = sub.set_lods(mesh, options)
    log("%s: LODs %d -> %d in %.1fs | before %s | after %s" % (mesh.get_name(), len(before), n, time.time() - t0, before, tris(mesh)))
    if flag(a.get("save")):
        log("   saved: %s" % unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False))
    else:
        log("   not saved (pass -save=1, or Ctrl+S in the editor)")
