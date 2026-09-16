# Runs a Python script inside the OPEN Unreal Editor through EditorBridge and prints its output.
#
#   .\Scripts\bridge_run.ps1 -Project "D:\Proj\MyGame.uproject" -Script .\Scripts\dump_graph.py -Args "-bp=/Game/BP_X -graph=EventGraph"
#
# -Project     path to the .uproject (or set $env:UE_PROJECT). Used to locate <Project>/Saved/EditorBridge.
# -Script      the .py to run. Copied into the inbox; the editor executes it within ~1 s.
# -Args        optional "-key=value ..." string, available to the script through editorbridge.args().
# -TimeoutSec  how long to wait for the result (long mesh builds can take many minutes).
# -BridgeDir   override the exchange folder (must match EDITOR_BRIDGE_DIR used by the editor).
param(
    [Parameter(Mandatory = $true)][string]$Script,
    [string]$Project = $env:UE_PROJECT,
    [string]$Args = "",
    [int]$TimeoutSec = 300,
    [string]$BridgeDir = ""
)

if (-not $BridgeDir) {
    if (-not $Project) { Write-Output "ERROR: pass -Project <path to .uproject> or set UE_PROJECT"; exit 1 }
    $BridgeDir = Join-Path (Split-Path -Parent (Resolve-Path $Project)) "Saved\EditorBridge"
}
$inbox  = Join-Path $BridgeDir "inbox"
$outbox = Join-Path $BridgeDir "outbox"
New-Item -ItemType Directory -Force $inbox, $outbox | Out-Null

if (-not (Get-Process UnrealEditor -ErrorAction SilentlyContinue)) {
    Write-Output "EDITOR NOT RUNNING - run the script headless instead:"
    Write-Output "  UnrealEditor-Cmd.exe <project> -run=pythonscript -script=<file.py> $Args -unattended -nopause -nosplash -stdout -FullStdOutLogOutput"
    exit 2
}

$name     = [IO.Path]::GetFileName($Script)
$stem     = [IO.Path]::GetFileNameWithoutExtension($name)
$log      = Join-Path $outbox "$stem.log"
$progress = Join-Path $outbox "$stem.progress"
if (Test-Path $log) { Remove-Item $log }

# args sidecar first, then the script (the bridge reads the sidecar when it picks up the script)
if ($Args) { Set-Content -Path (Join-Path $inbox "$stem.args") -Value $Args -Encoding UTF8 -NoNewline }
Copy-Item $Script (Join-Path $inbox $name)

$deadline = (Get-Date).AddSeconds($TimeoutSec)
$shown = 0
while (-not (Test-Path $log)) {
    if ((Get-Date) -gt $deadline) {
        Write-Output "TIMEOUT after ${TimeoutSec}s waiting for $log (the editor may still be running the script; check Output Log for [bridge])"
        exit 3
    }
    if (Test-Path $progress) {   # stream progress lines while waiting
        $lines = Get-Content $progress -ErrorAction SilentlyContinue
        if ($lines.Count -gt $shown) { $lines[$shown..($lines.Count - 1)]; $shown = $lines.Count }
    }
    Start-Sleep -Milliseconds 500
}
Start-Sleep -Milliseconds 200
$final = Get-Content $log
$final | Select-Object -First 1                     # STATUS line
$final | Select-Object -Skip (1 + $shown)           # whatever was not streamed yet
