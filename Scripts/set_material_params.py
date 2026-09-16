"""
Sets scalar/vector parameters on a Material Instance (parameter overrides) or on a base
Material (default value of the parameter node wired to each material input), then saves.
  -material=/Game/Path/M_X              required
  -scalars="Metallic=0;Specular=0.5"    optional (instance: any parameter name; base: Metallic, Specular, Roughness, Opacity)
  -vectors="Base Color=0,0,0,1"         optional (instance only)
"""
import unreal
from editorbridge import args, arg_list, log

a = args()
MEL, EAL = unreal.MaterialEditingLibrary, unreal.EditorAssetLibrary
mat = EAL.load_asset(a["material"])
scalars = dict(kv.split("=", 1) for kv in arg_list(a.get("scalars")))
vectors = dict(kv.split("=", 1) for kv in arg_list(a.get("vectors")))
mat.modify()

if isinstance(mat, unreal.MaterialInstanceConstant):
    for n, v in scalars.items():
        old = MEL.get_material_instance_scalar_parameter_value(mat, n)
        MEL.set_material_instance_scalar_parameter_value(mat, n, float(v)); log("%s: %s -> %s" % (n, old, v))
    for n, v in vectors.items():
        c = [float(x) for x in v.split(",")]
        MEL.set_material_instance_vector_parameter_value(mat, n, unreal.LinearColor(*c)); log("%s -> %s" % (n, c))
    MEL.update_material_instance(mat)
else:
    PROPS = {"metallic": unreal.MaterialProperty.MP_METALLIC, "specular": unreal.MaterialProperty.MP_SPECULAR,
             "roughness": unreal.MaterialProperty.MP_ROUGHNESS, "opacity": unreal.MaterialProperty.MP_OPACITY}
    for n, v in scalars.items():
        prop = PROPS.get(n.replace(" ", "").lower())
        node = MEL.get_material_property_input_node(mat, prop) if prop else None
        if node is None: log("%s: no material input / node, skipped" % n); continue
        if isinstance(node, unreal.MaterialExpressionScalarParameter):
            old = node.get_editor_property("default_value"); node.modify(); node.set_editor_property("default_value", float(v))
        elif isinstance(node, unreal.MaterialExpressionConstant):
            old = node.get_editor_property("r"); node.modify(); node.set_editor_property("r", float(v))
        else:
            log("%s: input node is %s, not edited" % (n, node.get_class().get_name())); continue
        log("%s: %s -> %s" % (n, old, v))
    MEL.recompile_material(mat)
log("saved: %s" % EAL.save_loaded_asset(mat, only_if_is_dirty=False))
