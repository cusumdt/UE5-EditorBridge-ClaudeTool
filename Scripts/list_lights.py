"""
Lights of the level currently open in the editor (persistent level + loaded sublevels),
including light components inside Blueprint actors. Read-only.
  -mobility=Static;Stationary   optional: only these mobilities
  -sort=intensity               optional: sort by intensity (default: level, then name)

Bridge (editor open):
  bridge_run.ps1 -Project <uproject> -Script list_lights.py
Commandlet (editor closed, needs a map):
  UnrealEditor-Cmd.exe <uproject> <map> -run=pythonscript -script=list_lights.py -unattended -nopause -nosplash -stdout -FullStdOutLogOutput
"""
import unreal
from editorbridge import args, arg_list, log

a = args({"mobility": "", "sort": ""})
want = set(m.lower() for m in arg_list(a["mobility"]))

MOBILITY = {unreal.ComponentMobility.STATIC: "Static",
            unreal.ComponentMobility.STATIONARY: "Stationary",
            unreal.ComponentMobility.MOVABLE: "Movable"}


def prop(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def describe(actor, comp):
    kind = comp.get_class().get_name().replace("Component", "")
    mob = MOBILITY.get(prop(comp, "mobility"), "?")
    intensity = prop(comp, "intensity", 0.0)
    units = prop(comp, "intensity_units")
    units = " " + units.name.title() if units is not None and kind != "DirectionalLight" else ""
    color = prop(comp, "light_color")
    color_s = "(%d,%d,%d)" % (color.r, color.g, color.b) if color else ""
    temp = prop(comp, "temperature")
    use_temp = prop(comp, "use_temperature", False)
    shadows = prop(comp, "cast_shadows", False)
    parts = [
        "%-11s" % mob,
        "%-16s" % kind,
        "I=%-10.2f%s" % (intensity, units),
    ]
    radius = prop(comp, "attenuation_radius")
    if radius is not None:
        parts.append("radius=%.0f" % radius)
    if kind == "SpotLight":
        parts.append("cone=%.0f/%.0f" % (prop(comp, "inner_cone_angle", 0), prop(comp, "outer_cone_angle", 0)))
    if kind == "RectLight":
        parts.append("size=%.0fx%.0f" % (prop(comp, "source_width", 0), prop(comp, "source_height", 0)))
    parts.append("shadows=%s" % ("ON" if shadows else "off"))
    if shadows and kind != "DirectionalLight":
        rt = prop(comp, "cast_raytraced_shadow")
        if rt is not None:
            parts.append("rt_shadow=%s" % rt.name.lower())
    if color_s:
        parts.append("color=%s" % color_s)
    if use_temp:
        parts.append("temp=%.0fK" % temp)
    if prop(comp, "affects_world", True) is False:
        parts.append("AFFECTS_WORLD=OFF")
    if prop(comp, "visible", True) is False or prop(actor, "hidden", False):
        parts.append("HIDDEN")
    return " ".join(parts)


world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
log("world: %s" % world.get_path_name())
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()

rows = []
for actor in actors:
    for comp in actor.get_components_by_class(unreal.LightComponent):
        mob = MOBILITY.get(prop(comp, "mobility"), "?")
        if want and mob.lower() not in want:
            continue
        level = actor.get_outer().get_outer().get_name() if actor.get_outer() else "?"
        bp = "" if actor.get_class().get_name() in (
            "PointLight", "SpotLight", "RectLight", "DirectionalLight", "SkyLight") else " [%s]" % actor.get_class().get_name()
        rows.append((level, actor.get_actor_label(), prop(comp, "intensity", 0.0),
                     "%s%s.%s  %s" % (actor.get_actor_label(), bp, comp.get_name(), describe(actor, comp))))

if a["sort"] == "intensity":
    rows.sort(key=lambda r: -r[2])
else:
    rows.sort(key=lambda r: (r[0], r[1]))

current = None
for level, _, _, line in rows:
    if level != current:
        current = level
        log("==== level: %s" % level)
    log("  " + line)

summary = {}
for actor in actors:
    for comp in actor.get_components_by_class(unreal.LightComponent):
        mob = MOBILITY.get(prop(comp, "mobility"), "?")
        key = (mob, "shadows" if prop(comp, "cast_shadows", False) else "no shadows")
        summary[key] = summary.get(key, 0) + 1
log("---- totals: %d light components" % sum(summary.values()))
for (mob, sh), n in sorted(summary.items()):
    log("  %-11s %-10s %d" % (mob, sh, n))

sky = [c for act in actors for c in act.get_components_by_class(unreal.SkyLightComponent)]
for c in sky:
    log("skylight: %s mobility=%s intensity=%.2f real_time_capture=%s" % (
        c.get_owner().get_actor_label(), MOBILITY.get(prop(c, "mobility"), "?"),
        prop(c, "intensity", 0.0), prop(c, "real_time_capture", False)))
