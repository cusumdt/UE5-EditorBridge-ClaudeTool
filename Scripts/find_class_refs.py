"""
Every node, pin, variable, local variable or component of a Blueprint whose type, default or
name mentions a text (e.g. a class you want to stop referencing).
  -bp=/Game/Path/BP_X                          required
  -text="Old Class Name;BP_OldClass"           required, ';'-separated needles
"""
import re
import unreal
from editorbridge import args, arg_list, log, load_blueprint

a = args()
bp = load_blueprint(a["bp"])
needles = arg_list(a["text"])
hit = lambda s: any(t in s for t in needles)
T, G = unreal.EditorBridgeBlueprintTools, unreal.EditorBridgeGraphTools

for line in T.list_local_variables(bp):
    if hit(line): log("LOCAL VAR | " + line)
for line in T.list_member_variable_types(bp):
    if hit(line): log("MEMBER VAR | " + line)
for line in T.list_components(bp):
    if hit(line): log("COMPONENT | " + line)
for gline in G.list_graphs(bp):
    gname = gline.split(" | ")[0]
    node = None
    for line in G.list_nodes(bp, unreal.Name(gname)):
        if re.match(r"^[0-9A-F]{32} \|", line):
            node = line
            if hit(line): log("%s | %s" % (gname, line))
        elif hit(line):
            log("%s | %s |%s" % (gname, node.split(" | ")[2] if node else "?", line))
