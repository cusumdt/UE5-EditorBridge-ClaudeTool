# Runs a Python script inside the OPEN Unreal Editor through EditorBridge and prints its output.
#
#   .\Scripts\bridge_run.ps1 -Project "D:\Proj\MyGame.uproject" -Script .\Scripts\dump_graph.py -Args "-bp=/Game/BP_X -graph=EventGraph"
#
# -Project     path to the .uproject (or set $env:UE_PROJECT). Used to locate <Project>/Saved/EditorBridge.
# -Script      the .py to run. Copied into the inbox; the editor executes it within ~1 s.
# -Args        optional "-key=value ..." string, available to the script through editorbridge.args().
# -TimeoutSec  how long to wait for the result (long mesh builds can take many minutes).
# -BridgeDir   override the exchange folder (must match EDITOR_BRIDGE_DIR used by the editor).
# -NoSave      block every asset save during this run (the log lists what the script tried to save).
# -Rollback    <run-id> from the "rollback:" line of a previous log: puts the backed-up files back.
#              With the editor open the packages are reloaded; with it closed the files are just copied.
param(
    [string]$Script = "",
    [string]$Project = $env:UE_PROJECT,
    [Alias("Args")][string]$ScriptArgs = "",   # not "Args": that name is PowerShell's automatic variable
    [int]$TimeoutSec = 300,
    [string]$BridgeDir = "",
    [switch]$NoSave,
    [string]$Rollback = ""
)

if (-not $BridgeDir) {
    if (-not $Project) { Write-Output "ERROR: pass -Project <path to .uproject> or set UE_PROJECT"; exit 1 }
    $BridgeDir = Join-Path (Split-Path -Parent (Resolve-Path $Project)) "Saved\EditorBridge"
}
$inbox  = Join-Path $BridgeDir "inbox"
$outbox = Join-Path $BridgeDir "outbox"
New-Item -ItemType Directory -Force $inbox, $outbox | Out-Null
$editorRunning = [bool](Get-Process UnrealEditor -ErrorAction SilentlyContinue)

if ($Rollback) {
    $backup = Join-Path (Join-Path $BridgeDir "backup") $Rollback
    if (-not (Test-Path $backup)) { Write-Output "ERROR: no backup folder $backup"; exit 1 }
    if ($editorRunning) {
        # let the editor copy and reload, so open assets pick up the disk state
        $Script = Join-Path $PSScriptRoot "rollback_run.py"
        $ScriptArgs = "-dir=`"$backup`""
    } else {
        $projectDir = Split-Path -Parent (Resolve-Path $Project)
        Get-ChildItem $backup -Recurse -File | ForEach-Object {
            $rel = $_.FullName.Substring($backup.Length).TrimStart('\', '/')
            $dest = Join-Path $projectDir $rel
            Copy-Item $_.FullName $dest -Force
            Write-Output "restored: $dest"
        }
        Write-Output "STATUS: ok (editor closed, files copied)"
        exit 0
    }
}
if (-not $Script) { Write-Output "ERROR: pass -Script <file.py> or -Rollback <run-id>"; exit 1 }
if ($NoSave) { $ScriptArgs = ($ScriptArgs + " -nosave=1").Trim() }

if (-not $editorRunning) {
    Write-Output "EDITOR NOT RUNNING - run the script headless instead:"
    Write-Output "  UnrealEditor-Cmd.exe <project> -run=pythonscript -script=<file.py> $ScriptArgs -unattended -nopause -nosplash -stdout -FullStdOutLogOutput"
    exit 2
}

$name     = [IO.Path]::GetFileName($Script)
$stem     = [IO.Path]::GetFileNameWithoutExtension($name)
$log      = Join-Path $outbox "$stem.log"
$progress = Join-Path $outbox "$stem.progress"
if (Test-Path $log) { Remove-Item $log }

# args sidecar first, then the script (the bridge reads the sidecar when it picks up the script)
if ($ScriptArgs) { [IO.File]::WriteAllText((Join-Path $inbox "$stem.args"), $ScriptArgs, (New-Object Text.UTF8Encoding $false)) }   # no BOM
Copy-Item $Script (Join-Path $inbox $name)

$deadline = (Get-Date).AddSeconds($TimeoutSec)
$shown = 0
function Get-CompleteLines($path) {
    # only lines terminated by a newline: the editor may be mid-write on the last one
    $raw = Get-Content $path -Raw -ErrorAction SilentlyContinue
    if (-not $raw) { return @() }
    $lines = @($raw -split "`r?`n")
    if (-not $raw.EndsWith("`n")) {
        if ($lines.Count -le 1) { return @() }          # 0..-1 would wrap around in PowerShell
        $lines = $lines[0..($lines.Count - 2)]
    }
    return @($lines | Where-Object { $_ -ne "" })
}
while (-not (Test-Path $log)) {
    if ((Get-Date) -gt $deadline) {
        Write-Output "TIMEOUT after ${TimeoutSec}s waiting for $log (the editor may still be running the script; check Output Log for [bridge])"
        exit 3
    }
    if (Test-Path $progress) {   # stream finished progress lines while waiting
        $lines = @(Get-CompleteLines $progress)   # @(): a one-line result would unroll to a string
        if ($lines.Count -gt $shown) { $lines[$shown..($lines.Count - 1)]; $shown = $lines.Count }
    }
    Start-Sleep -Milliseconds 500
}
Start-Sleep -Milliseconds 200
$final = @(Get-Content $log)
$body = @($final | Select-Object -Skip 1 | Where-Object { $_ -ne "" })
if ($body.Count -gt $shown) { $body[$shown..($body.Count - 1)] }   # whatever was not streamed yet
$final[0]                                                          # STATUS line, last
