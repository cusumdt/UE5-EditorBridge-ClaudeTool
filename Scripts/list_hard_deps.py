"""
Hard package dependencies of one or more assets according to the Asset Registry, i.e. what
gets loaded together with them.
  -assets="/Game/A;/Game/B"   required, package paths ';'-separated
  -all=1                      optional: include engine/plugin packages too
"""
import unreal
from editorbridge import args, arg_list, log

a = args()
reg = unreal.AssetRegistryHelpers.get_asset_registry()
opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=False, include_hard_package_references=True,
                                             include_searchable_names=False, include_soft_management_references=False,
                                             include_hard_management_references=False)
for pkg in arg_list(a["assets"]):
    deps = sorted(str(x) for x in (reg.get_dependencies(unreal.Name(pkg), opts) or []))
    if not a.get("all"):
        deps = [d for d in deps if d.startswith("/Game/")]
    log("== %s (%d hard deps)" % (pkg, len(deps)))
    for d in deps:
        log("   " + d)
