"""
Compiles Blueprints and reports errors/warnings; optionally saves the clean ones.
  -bps="/Game/A;/Game/B"   required
  -save=1                  optional
"""
from editorbridge import args, arg_list, log, load_blueprint, compile_and_report

a = args()
any_errors = False
for path in arg_list(a["bps"]):
    any_errors |= compile_and_report(load_blueprint(path), save=bool(a.get("save")))
log("DONE - %s" % ("some Blueprints have errors" if any_errors else "all compile clean"))
