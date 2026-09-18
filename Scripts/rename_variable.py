"""
Renames/moves a Blueprint variable: every node in the owner Blueprint AND in the listed
dependents that references <owner class>::<old> is pointed to <new> (e.g. after moving the
variable to a C++ parent). Optionally removes the old Blueprint variable and re-points the
member parent to the new owner class. Then refreshes nodes, rewires orphaned pins and compiles.
  -bp=/Game/Path/BP_Owner          required (Blueprint that declared the variable)
  -old=OldName -new=NewName        required
  -dependents="/Game/A;/Game/B"    optional Blueprints whose nodes reference the variable
  -remove_old=1                    optional: delete the old variable from the owner
  -retarget=/Script/Module.Class   optional: class that now declares the variable
  -save=1                  optional: write the changed assets to disk (default: leave them
                           dirty and undoable; the bridge lists what was saved)
"""
import unreal
from editorbridge import args, arg_list, flag, log, load_blueprint, compile_and_report, load_class

a = args()
T = unreal.EditorBridgeBlueprintTools
owner = load_blueprint(a["bp"])
owner_class = unreal.BlueprintEditorLibrary.generated_class(owner)
deps = [load_blueprint(p) for p in arg_list(a.get("dependents"))]
old_n, new_n = unreal.Name(a["old"]), unreal.Name(a["new"])

for bp in [owner] + deps:
    n = T.rename_variable_references(bp, owner_class, old_n, new_n)
    log("%s: %d node(s) renamed" % (bp.get_name(), n))
    if a.get("retarget"):
        m = T.retarget_member_references(bp, owner_class, load_class(a["retarget"]))
        log("%s: %d node(s) retargeted to %s" % (bp.get_name(), m, a["retarget"]))

if a.get("remove_old") and old_n in T.list_member_variables(owner):
    T.remove_member_variable(owner, old_n)
    log("removed variable '%s' from %s" % (a["old"], owner.get_name()))

any_errors = False
for bp in [owner] + deps:
    T.refresh_all_nodes(bp)
    removed, unfixable = T.rewire_orphaned_pins(bp)
    if removed: log("%s: %d orphaned pin(s) rewired" % (bp.get_name(), removed))
    for u in unfixable: log("    NOT re-connected: " + u)
    any_errors |= compile_and_report(bp, save=flag(a.get("save")))
log("DONE - %s" % ("errors remain, see above" if any_errors else "all compile clean"))
