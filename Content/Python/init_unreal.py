"""
EditorBridge — runs Python scripts inside the open Unreal Editor from the outside.

PythonScriptPlugin executes this file when the editor starts. Every second it looks for *.py
files in  <Project>/Saved/EditorBridge/inbox/ , executes the first one INSIDE the editor
(full `unreal` API, same as the Python console) and writes the captured output to
<Project>/Saved/EditorBridge/outbox/<name>.log . Each script runs in one editor transaction,
so Ctrl+Z undoes it as a whole.

  * Arguments: an optional  <name>.args  sidecar next to the script ("-bp=/Game/X -graph=EventGraph"),
    exposed to the script through  editorbridge.args() .
  * Progress: lines are mirrored to the Output Log ([bridge] prefix) and to
    outbox/<name>.progress while the script runs; the final .log is written at the end.
  * Disable: create  <Project>/Saved/EditorBridge/bridge.disabled .
  * Override the exchange folder with the EDITOR_BRIDGE_DIR environment variable.

Submit scripts with  Scripts/bridge_run.ps1  (see README) or by copying files into the inbox.
"""
import io
import os
import sys
import time
import traceback

import unreal

import editorbridge

PROJECT_DIR  = os.path.normpath(unreal.Paths.project_dir())
BASE         = os.path.normpath(os.environ.get("EDITOR_BRIDGE_DIR") or os.path.join(PROJECT_DIR, "Saved", "EditorBridge"))
INBOX        = os.path.join(BASE, "inbox")
OUTBOX       = os.path.join(BASE, "outbox")
DONE         = os.path.join(BASE, "done")
DISABLED     = os.path.join(BASE, "bridge.disabled")
POLL_SECONDS = 1.0

for d in (INBOX, OUTBOX, DONE):
    os.makedirs(d, exist_ok=True)

_last_poll = 0.0
_running = False   # re-entrancy guard: long editor operations (mesh builds, saves) pump Slate, which ticks us again


class _Tee(io.StringIO):
    """Collects the script output, mirrors it to the Output Log and to a progress file."""
    def __init__(self, progress_path):
        super().__init__()
        self._progress = open(progress_path, "w", encoding="utf-8")

    def write(self, s):
        if s and s.strip():
            unreal.log("[bridge] " + s.rstrip("\n"))
            self._progress.write(s if s.endswith("\n") else s + "\n")
            self._progress.flush()
        return super().write(s)

    def close(self):
        self._progress.close()
        super().close()


def _read_args(script_path):
    sidecar = os.path.splitext(script_path)[0] + ".args"
    if not os.path.exists(sidecar):
        return {}
    with open(sidecar, "r", encoding="utf-8") as f:
        text = f.read()
    os.remove(sidecar)
    return editorbridge.parse_args(text)


def _run_script(path):
    name = os.path.basename(path)
    stem = os.path.splitext(name)[0]
    status = "ok"
    started = time.time()

    # Take the script (and its args) out of the inbox BEFORE running, so a nested tick can
    # never pick it up a second time.
    arg_dict = _read_args(path)
    done_path = os.path.join(DONE, name)
    if os.path.exists(done_path):
        os.remove(done_path)
    os.replace(path, done_path)
    path = done_path

    with open(path, "r", encoding="utf-8") as f:
        code = f.read()

    out = _Tee(os.path.join(OUTBOX, stem + ".progress"))
    env = {
        "__name__": "__main__",
        "__file__": path,
        "unreal": unreal,
        "log": lambda m: out.write(str(m) + "\n"),
        "ARGS": arg_dict,
    }
    editorbridge._set_context(arg_dict, env["log"])

    old_stdout, old_stderr = sys.stdout, sys.stderr
    sys.stdout = sys.stderr = out
    try:
        with unreal.ScopedEditorTransaction("EditorBridge: " + name):
            exec(compile(code, path, "exec"), env)
    except SystemExit as e:
        status = "exit %s" % e.code if e.code not in (None, 0) else "ok"
    except Exception:
        status = "error"
        out.write(traceback.format_exc())
    finally:
        sys.stdout, sys.stderr = old_stdout, old_stderr
        editorbridge._set_context(None, None)

    elapsed = time.time() - started
    result = "STATUS: %s (%.1fs)\n%s" % (status, elapsed, out.getvalue())
    out.close()
    log_path = os.path.join(OUTBOX, stem + ".log")
    with open(log_path, "w", encoding="utf-8") as f:
        f.write(result)
    try:
        os.remove(os.path.join(OUTBOX, stem + ".progress"))
    except OSError:
        pass
    unreal.log("[bridge] %s -> %s (%s)" % (name, status, os.path.relpath(log_path, PROJECT_DIR)))


def _tick(delta_seconds):
    global _last_poll, _running
    if _running:
        return
    now = time.time()
    if now - _last_poll < POLL_SECONDS:
        return
    _last_poll = now
    if os.path.exists(DISABLED):
        return
    _running = True
    try:
        scripts = sorted(f for f in os.listdir(INBOX) if f.lower().endswith(".py"))
        if scripts:
            _run_script(os.path.join(INBOX, scripts[0]))   # one script per tick, in name order
    except Exception:
        unreal.log_error("[bridge] " + traceback.format_exc())
    finally:
        _running = False


unreal.register_slate_post_tick_callback(_tick)
unreal.log("[bridge] EditorBridge active - inbox: %s" % INBOX)
