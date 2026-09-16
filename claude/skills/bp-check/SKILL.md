---
name: bp-check
description: Compiles a list of Blueprints (from CLAUDE.md or given as argument) and reports errors and warnings. Use after C++ changes to Blueprint parents or after scripted Blueprint migrations.
---

1. Blueprints to cover: the list in CLAUDE.md ("Blueprint chain to verify") or the paths given as
   argument, `;`-separated.
2. Editor open (`Get-Process UnrealEditor`): run through the bridge without saving:
   `.\Plugins\EditorBridge\Scripts\bridge_run.ps1 -Project <uproject> -Script .\Plugins\EditorBridge\Scripts\compile_blueprints.py -Args "-bps=<list>"`
3. Editor closed: same script through the commandlet
   (`UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<compile_blueprints.py> -bps=<list> -unattended -nopause -nosplash -stdout -FullStdOutLogOutput`), filter by `LogPython`.
4. Report one line per Blueprint (`ok` / `N errors`) and the literal messages of the failing ones.
