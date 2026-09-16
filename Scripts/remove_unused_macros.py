"""
Removes macro graphs from a Blueprint if (and only if) no graph instantiates them. Compiles + saves.
  -bp=/Game/Path/BP_X          required
  -macros="Macro A;Macro B"    required
"""
import re
import unreal
from editorbridge import args, arg_list, log, load_blueprint, compile_and_report

a = args()
bp = load_blueprint(a["bp"])
G, BEL = unreal.EditorBridgeGraphTools, unreal.BlueprintEditorLibrary
graphs = [l.split(" | ")[0] for l in G.list_graphs(bp)]
changed = False
for macro in arg_list(a["macros"]):
    pattern = re.compile(r"^[0-9A-F]{32} \| K2Node_MacroInstance \| %s \|" % re.escape(macro))
    users = sorted({g for g in graphs if g != macro
                    for line in G.list_nodes(bp, unreal.Name(g)) if pattern.match(line)})
    if users:
        log("keep '%s': used in %s" % (macro, users)); continue
    graph = BEL.find_graph(bp, unreal.Name(macro))
    if not graph:
        log("'%s' not found" % macro); continue
    BEL.remove_graph(bp, graph); changed = True
    log("removed '%s'" % macro)
if changed:
    compile_and_report(bp, save=True)
