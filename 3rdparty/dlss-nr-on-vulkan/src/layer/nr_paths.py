"""Where the layer, the daemon and the tools meet: three paths and the settings file.

Two tools and a daemon have to agree on these. They were written out separately in
`nr-ctl` first, and `nr-toggle` would have been a third copy — this project's own notes
say a fact written down twice drifts, so it is written down once.

Nothing here imports anything but the standard library, so a tool can be a single file
with a shebang and still share this.
"""
import json
import os
import pathlib
import socket
import subprocess
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "src"))
import nr_build  # noqa: E402

SETTINGS = pathlib.Path(os.environ.get("NR_SETTINGS", "/tmp/nr_settings.json"))
TRIGGER = pathlib.Path(os.environ.get("NR_LAYER_TRIGGER", "/tmp/nr_trigger"))
SOCKET = pathlib.Path(os.environ.get("NR_LAYER_SOCKET", "/tmp/nr_layer.sock"))
LOG = pathlib.Path(os.environ.get("NR_LAYER_LOG", "/tmp/nr_daemon.log"))
DAEMON = pathlib.Path(__file__).resolve().parent / "nr_daemon.py"

# The compromise this project measured: below it the picture is not worth the frame, above
# it the frame is not worth the picture, and the sign of the trade depends on how dark the
# scene is rather than on the number (`notes/phase51`, `phase52`). Only used when there is
# no settings file at all — anything already chosen wins.
FIRST_SCALE = 0.55

# On macOS the Vulkan loader finds no driver on its own: MoltenVK is discovered through an
# ICD manifest, and the build (`make` into work/, CMake into its build directory) writes
# one beside the libraries pointing at the MoltenVK it linked. The
# compute runtime does not need this — it links MoltenVK directly — but everything that
# goes through the loader does: the layer, its tests, and any game the layer attaches to.
ICD_MANIFEST = nr_build.BUILD_DIR / "MoltenVK_icd.json"


def loader_environment(environment=None):
    """The environment for a process that reaches Vulkan through the loader.

    On macOS, points the loader at MoltenVK when nothing else does, under both the
    current name (VK_DRIVER_FILES) and the one older loaders read (VK_ICD_FILENAMES).
    Anything already set wins; elsewhere the environment comes back unchanged.
    """
    environment = dict(os.environ if environment is None else environment)
    if sys.platform == "darwin" and ICD_MANIFEST.exists() \
            and not (environment.get("VK_DRIVER_FILES") or environment.get("VK_ICD_FILENAMES")):
        environment["VK_DRIVER_FILES"] = str(ICD_MANIFEST)
        environment["VK_ICD_FILENAMES"] = str(ICD_MANIFEST)
    return environment


def read():
    try:
        with SETTINGS.open() as handle:
            return json.load(handle)
    except (OSError, ValueError):
        return {}


def write(values):
    SETTINGS.parent.mkdir(parents=True, exist_ok=True)
    temporary = SETTINGS.with_suffix(SETTINGS.suffix + ".new")
    # written whole and renamed, so the daemon never reads a half-written file
    with temporary.open("w") as handle:
        json.dump(values, handle, indent=2, sort_keys=True)
        handle.write("\n")
    temporary.replace(SETTINGS)


def alive():
    """Whether a daemon is listening. A stale socket file is not a daemon."""
    if not SOCKET.exists():
        return False
    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as probe:
            probe.settimeout(0.5)
            probe.connect(str(SOCKET))
        return True
    except OSError:
        return False


def start_daemon(wait=10.0):
    """Bring the model up, detached. Returns None, or why it did not.

    Shared because both the toggle and the panel want it: a control that answers "no
    daemon, go and start one" sends the user to a terminal, which is the thing they exist
    to avoid. Turning the effect *off* deliberately leaves the daemon running — the
    trigger is separate from the model precisely so the picture can come and go without
    paying the load again.
    """
    if not DAEMON.exists():
        return f"no daemon at {DAEMON}"
    values = read()
    if "render_scale" not in values:
        values["render_scale"] = FIRST_SCALE
        write(values)
    try:
        with LOG.open("a") as log:
            subprocess.Popen(
                # the socket too, not just the settings: with `NR_LAYER_SOCKET` set, a
                # daemon started on the default path is one nothing else is talking to
                [sys.executable, str(DAEMON), "--settings", str(SETTINGS),
                 "--socket", str(SOCKET)],
                stdout=log, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL,
                start_new_session=True, cwd=str(DAEMON.parent))
    except OSError as error:
        return f"could not start it: {error}"
    # It refuses to start if one is already listening, so a second attempt is harmless.
    deadline = time.monotonic() + wait
    while time.monotonic() < deadline:
        if alive():
            return None
        time.sleep(0.25)
    return f"started, still loading - {LOG}"
