"""
Small helper used by EditorBridge scripts so the same file runs in both modes:

  * inside the running editor, dropped in the bridge inbox (arguments come from the
    <script>.args sidecar written by bridge_run.ps1, output goes to the outbox log), or
  * headless with the commandlet:
        UnrealEditor-Cmd.exe <project> -run=pythonscript -script=<file.py> -key=value ...

Usage in a script:
    from editorbridge import args, log
    a = args({"graph": "EventGraph"})      # dict of -key=value arguments with defaults
    log("hello")                           # goes to the outbox log (bridge) or the Output Log
"""
import re

import unreal

_ARGS = None    # set by the bridge for the current script, None in commandlet mode
_LOG = None     # callable that receives each output line in bridge mode

_ARG_RE = re.compile(r'-(\w+)=(?:"([^"]*)"|(\S+))')


def parse_args(text):
    """Parses '-key=value -other="with spaces"' into a dict."""
    return {m.group(1): (m.group(2) if m.group(2) is not None else m.group(3)) for m in _ARG_RE.finditer(text or "")}


def _set_context(arg_dict, log_fn):
    global _ARGS, _LOG
    _ARGS, _LOG = arg_dict, log_fn


def args(defaults=None):
    """Arguments for the current script: bridge sidecar in bridge mode, command line otherwise."""
    result = dict(defaults or {})
    result.update(_ARGS if _ARGS is not None else parse_args(unreal.SystemLibrary.get_command_line()))
    return result


def arg_list(value, sep=";"):
    """Splits a list argument ('a;b;c') into a clean list."""
    return [v.strip() for v in (value or "").split(sep) if v.strip()]


def flag(value):
    """True for '1', 'true', 'yes', 'on' (case-insensitive); False for anything else, including '0'."""
    return str(value).strip().lower() in ("1", "true", "yes", "on")


def log(msg):
    """Script output: outbox log + Output Log in bridge mode, Output Log in commandlet mode."""
    (_LOG or unreal.log)(str(msg))


def in_bridge():
    return _ARGS is not None


def is_dirty(obj):
    """Whether the package of an asset has unsaved changes (UPackage.IsDirty is not exposed to Python)."""
    pkg = obj.get_outermost()
    utils = unreal.EditorLoadingAndSavingUtils
    return pkg in utils.get_dirty_content_packages() or pkg in utils.get_dirty_map_packages()


def load_blueprint(path):
    """Loads a Blueprint asset or raises with a clear message (also fails while in PIE)."""
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not isinstance(asset, unreal.Blueprint):
        raise RuntimeError("Blueprint not found (or editor is in PIE): %s" % path)
    return asset


def compile_and_report(bp, save=False):
    """Compiles a Blueprint, logs errors/warnings, optionally saves. Returns True if it has errors."""
    messages, has_errors = unreal.EditorBridgeBlueprintTools.compile_blueprint_with_log(bp)
    log("compile %s -> %s" % (bp.get_name(), "ERRORS" if has_errors else "ok"))
    for m in messages:
        if m.startswith("[Error]") or m.startswith("[Warning]"):
            log("    " + m)
    if save and not has_errors:
        log("    saved: %s" % unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False))
    elif not save and is_dirty(bp):
        log("    not saved (pass -save=1, or Ctrl+S in the editor)")
    return has_errors


def load_class(path):
    """Loads a UClass from '/Script/Module.Class', '/Game/X/BP_Y.BP_Y_C' or a Blueprint asset path."""
    if path.endswith("_C") or path.startswith("/Script/"):
        cls = unreal.load_class(None, path)
        if cls:
            return cls
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if isinstance(asset, unreal.Blueprint):
        return unreal.BlueprintEditorLibrary.generated_class(asset)
    raise RuntimeError("class not found: %s" % path)


class track_saves(object):
    """
    Context manager around unreal.EditorBridgeSaveGuard: records every package saved inside the
    block, backs up the file on disk first (when backup_dir is given) and can block saves
    altogether. The bridge wraps every script in it; use it yourself in commandlet mode:

        with track_saves(backup_dir="D:/Proj/Saved/EditorBridge/backup/run1") as saves:
            ...
        for line in saves.events: log(line)     # "saved: ..." / "blocked: ..."
    """
    def __init__(self, backup_dir="", block=False):
        self.backup_dir, self.block, self.events = backup_dir or "", bool(block), []

    def __enter__(self):
        unreal.EditorBridgeSaveGuard.begin(self.backup_dir, self.block)
        return self

    def __exit__(self, *exc):
        self.events = list(unreal.EditorBridgeSaveGuard.end())
        return False
