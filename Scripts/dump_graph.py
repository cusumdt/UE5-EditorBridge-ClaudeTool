"""
Lists the graphs of a Blueprint, or dumps the nodes + pins of one or more graphs.
  -bp=/Game/Path/BP_X            required
  -graph="EventGraph;My Macro"   optional, ';'-separated; omit to list graphs
Output per node: "<guid> | <class> | <title> | (x,y)" then one line per pin:
"    in|out <name> : <type> [= default] [ORPHAN] -> <guid>.<pin>, ..."
"""
import unreal
from editorbridge import args, arg_list, log, load_blueprint

a = args()
bp = load_blueprint(a["bp"])
G = unreal.EditorBridgeGraphTools
if a.get("graph"):
    for g in arg_list(a["graph"]):
        log("==== %s / %s ====" % (bp.get_name(), g))
        for line in G.list_nodes(bp, unreal.Name(g)):
            log(line)
else:
    log("==== graphs of %s ====" % bp.get_name())
    for line in G.list_graphs(bp):
        log(line)
