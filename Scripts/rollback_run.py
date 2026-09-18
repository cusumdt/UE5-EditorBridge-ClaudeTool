"""
Restores the files backed up by one bridge run (see "rollback:" at the end of a run's log) and
reloads the affected packages in the editor. Use through the runner:
  bridge_run.ps1 -Project <uproject> -Rollback <run-id>
or directly:
  -run=<run-id>            required: folder name under <Project>/Saved/EditorBridge/backup/
  -dir=<absolute path>     alternative: full path of the backup folder
"""
import os
import unreal
from editorbridge import args, log

a = args()
folder = a.get("dir") or os.path.join(unreal.Paths.project_dir(), "Saved", "EditorBridge", "backup", a["run"])
log("restoring from %s" % folder)
for line in unreal.EditorBridgeSaveGuard.restore_backups(folder):
    log("  " + line)
