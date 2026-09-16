"""
Parent class, member variables (with types), local variables, variable-node references and
components of a Blueprint.
  -bp=/Game/Path/BP_X     required
  -var=Name               optional: only variable nodes for this variable
"""
import unreal
from editorbridge import args, log, load_blueprint

a = args()
bp = load_blueprint(a["bp"])
T = unreal.EditorBridgeBlueprintTools
parent = T.get_parent_class(bp)
log("%s (parent: %s)" % (bp.get_name(), parent.get_name() if parent else "None"))
log("-- member variables --")
for line in T.list_member_variable_types(bp): log("   " + line)
log("-- local variables --")
for line in T.list_local_variables(bp): log("   " + line)
log("-- variable nodes%s --" % (" for '%s'" % a["var"] if a.get("var") else ""))
for line in T.list_variable_references(bp, unreal.Name(a.get("var", "None"))): log("   " + line)
log("-- components --")
for line in T.list_components(bp): log("   " + line)
