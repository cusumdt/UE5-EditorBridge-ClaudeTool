"""
Sets an editor property on named components of one or more Blueprints (the template each
Blueprint owns: own SCS node or inherited-component override, same object the Details panel
edits). Compiles and saves the Blueprints that changed.
  -bps="/Game/A;/Game/B"                  required
  -components="Engine;Suspension"         required (component variable names)
  -property=visible_in_ray_tracing        required (python property name)
  -value=false                            required (true/false, number or string)
"""
import unreal
from editorbridge import args, arg_list, log, load_blueprint, compile_and_report

a = args()
value = {"true": True, "false": False}.get(a["value"].lower(), a["value"])
if isinstance(value, str):
    try: value = float(value) if "." in value else int(value)
    except ValueError: pass
comps = set(arg_list(a["components"]))
sds, lib = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem), unreal.SubobjectDataBlueprintFunctionLibrary

for path in arg_list(a["bps"]):
    bp = load_blueprint(path)
    done, changed = set(), 0
    for h in sds.k2_gather_subobject_data_for_blueprint(bp):
        data = sds.k2_find_subobject_data_from_handle(h)
        name = str(lib.get_variable_name(data))
        if name not in comps or name in done: continue
        obj = lib.get_object_for_blueprint(data, bp)
        if not obj: continue
        done.add(name)
        if obj.get_editor_property(a["property"]) == value: continue
        obj.modify(); obj.set_editor_property(a["property"], value); changed += 1
    log("%s: %d component(s) changed (%s)" % (bp.get_name(), changed, ", ".join(sorted(done)) or "none found"))
    if changed:
        compile_and_report(bp, save=True)
