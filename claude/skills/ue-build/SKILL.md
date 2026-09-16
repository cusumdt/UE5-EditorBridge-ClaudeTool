---
name: ue-build
description: Compiles the project's editor target (C++) and reports the result. Use after modifying Source/. Requires the editor to be closed.
---

1. `Get-Process UnrealEditor -ErrorAction SilentlyContinue`: if running, do not compile - ask the
   user to close it (Live Coding blocks UBT) and stop.
2. Run (paths from CLAUDE.md), redirecting to `$env:TEMP\ue_build.log`, timeout up to 10 min:
   `& "<ENGINE_ROOT>\Engine\Build\BatchFiles\Build.bat" <Project>Editor Win64 Development -Project="<uproject>" -WaitMutex -NoHotReload 2>&1 | Out-File -Encoding utf8 "$env:TEMP\ue_build.log"`
3. Filter by `error`, `warning`, `Result`, `Total execution`; report `Result: Succeeded|Failed`,
   time, each error as `File:line - message`, project warnings only.
4. Do not modify code from this command; propose the fix and wait.
