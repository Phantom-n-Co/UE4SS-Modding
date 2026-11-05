#!/usr/bin/env python3
"""Run automated in-game tests with only the mods they need.

Deploys a dedicated Amethyst profile `aftest` that enables UE4SS and this repo's
`UE4SS-Modding` mod and nothing else. Inside those two it also disables UE4SS's bundled
Lua mods (and mods.txt/mods.json) and every repo mod not asked for. AFTestBridge is always
included. `restore` redeploys the `default` profile and checks that the deployed file list
(ue5_deployed.txt) matches the one saved before the first switch.

  tools/aftest_mods.py use StackAnything        # deploy aftest with AFTestBridge + StackAnything
  tools/aftest_mods.py status                   # which profile/mods are deployed, game state
  tools/aftest_mods.py restore                  # deploy default again (quits a test game first)

Test scripts call `run_isolated(["StackAnything"], main)`: it switches only when needed,
enters the AFTest world, runs the test and restores the default profile, including after
a failure. `AFTEST_KEEP_MODS=1` skips the restore (several runs in a row; then `restore`).

The game is never quit unless it was started under an `aftest` deployment made by this
tool, so a session the user started is left alone (the switch is refused instead). Not
covered by Amethyst: mod.io UGC mods (C:/users/Public/mod.io), which the game loads
itself; `check_no_ugc()` fails a test whose session loaded any.
"""
import argparse
import contextlib
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import afgame  # noqa: E402

GAME = "Abiotic_Factor"
AMETHYST = Path.home() / "Games/Amethyst/Abiotic Factor"
MODS = AMETHYST / "mods"
PROFILES = AMETHYST / "profiles"
DEFAULT, TEST = "default", "aftest"
DEPLOYED = AMETHYST / "ue5_deployed.txt"  # every file of the current deployment, relative to the game dir
REPO_MOD = "UE4SS-Modding"
ALWAYS = {"AFTestBridge"}
GAME_DIR = Path.home() / ".local/share/Steam/steamapps/common/AbioticFactor/AbioticFactor"
GAME_LOG = (Path.home() / ".local/share/Steam/steamapps/compatdata/427410/pfx/drive_c/users/steamuser"
            "/AppData/Local/AbioticFactor/Saved/Logs/AbioticFactor.log")
STATE = Path.home() / ".cache/aftest-mods"  # marker + baseline (outside the repo and Amethyst)
MARKER = STATE / "deployed.json"
BASELINE = STATE / "ue5_deployed.default.txt"


class ModsError(Exception):
    pass


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else ""


def ue4ss_mod_name(modlist):
    names = [line[1:] for line in modlist if line[:1] == "+" and line[1:].startswith("UE4SS v")]
    if len(names) != 1:
        raise ModsError(f"expected exactly one enabled 'UE4SS v...' mod in the default modlist, found {names}")
    return names[0]


def mod_files(mod):
    root = MODS / mod
    return [p.relative_to(root).as_posix() for p in root.rglob("*") if p.is_file() or p.is_symlink()]


def write_profile(wanted):
    src, dst = PROFILES / DEFAULT, PROFILES / TEST
    modlist = (src / "modlist.txt").read_text(encoding="utf-8").splitlines()
    ue4ss = ue4ss_mod_name(modlist)
    keep = {ue4ss, REPO_MOD}
    out = []
    for line in modlist:
        name = line[1:]
        if line[:1] in "+-" and not name.endswith("_separator"):
            line = ("+" if name in keep else "-") + name
        out.append(line)
    repo_mods = {p.name for p in (MODS / REPO_MOD).iterdir() if p.is_dir()}
    missing = wanted - repo_mods
    if missing:
        raise ModsError(f"not in the {REPO_MOD} Amethyst mod: {sorted(missing)} (deploy them there first)")
    excluded = {
        # UE4SS's bundled Lua mods and their load lists; `shared` is a library, not a mod.
        ue4ss: sorted(k.lower() for k in mod_files(ue4ss)
                      if k.lower().startswith("ue4ss/mods/") and not k.lower().startswith("ue4ss/mods/shared/")),
        REPO_MOD: sorted(k.lower() for k in mod_files(REPO_MOD) if k.split("/", 1)[0] not in wanted),
    }
    state = json.loads((src / "profile_state.json").read_text(encoding="utf-8"))
    state.pop("download_install_history", None)
    state["excluded_mod_files"] = {k: v for k, v in excluded.items() if v}
    dst.mkdir(exist_ok=True)
    (dst / "modlist.txt").write_text("\n".join(out) + "\n", encoding="utf-8")
    shutil.copyfile(src / "plugins.txt", dst / "plugins.txt")
    (dst / "profile_state.json").write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")


def deploy(profile):
    r = subprocess.run(["amethyst-mod-manager-cli", "deploy", GAME, profile], capture_output=True, text=True)
    if r.returncode != 0 or "Deploy finished OK" not in r.stdout + r.stderr:
        raise ModsError(f"amethyst deploy {profile} failed: {(r.stdout + r.stderr).strip()[-500:]}")


def deployed_mods():
    """UE4SS mods the game would load: ue4ss/Mods/<dir> with enabled.txt, plus mods.txt entries."""
    mods_dir = GAME_DIR / "Binaries/Win64/ue4ss/Mods"
    enabled = {p.parent.name for p in mods_dir.glob("*/enabled.txt")}
    listed = mods_dir / "mods.txt"
    if listed.exists():
        for line in listed.read_text(encoding="utf-8", errors="replace").splitlines():
            name, _, flag = line.partition(":")
            if flag.strip() == "1" and not name.strip().startswith(";"):
                enabled.add(name.strip())
    return enabled


def deployed_paks():
    paks = GAME_DIR / "Content/Paks"
    return sorted(p.relative_to(paks).as_posix() for d in ("~mods", "LogicMods") for p in (paks / d).rglob("*.pak"))


def marker():
    try:
        return json.loads(MARKER.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None


def ours():
    """The current deployment is an aftest deployment made by this tool (not changed since)."""
    m = marker()
    return bool(m) and m.get("deployed_sha256") == digest(DEPLOYED)


def amethyst_gui_running():
    """The Amethyst GUI redeploys the active profile on start, so it would swap the files of
    a running test (seen: it redeployed `default` under a running aftest session)."""
    out = subprocess.run(["ps", "-eo", "args="], capture_output=True, text=True).stdout
    # Match the executable (argv[0], or argv[1] under a python interpreter), not any command line
    # that merely mentions it (shells running this tool would match a substring test).
    for line in out.splitlines():
        argv = line.split()
        if argv and argv[0].rsplit("/", 1)[-1].startswith("python"):
            argv = argv[1:]
        if argv and argv[0].rsplit("/", 1)[-1] == "amethyst-mod-manager":
            return True
    return False


def use(*mods):
    if amethyst_gui_running():
        raise ModsError("the Amethyst GUI is open; close it first (it redeploys profiles on its own)")
    wanted = set(mods) | ALWAYS
    m = marker()
    if m and ours() and set(m["mods"]) == wanted and deployed_mods() == wanted:
        return
    if afgame.process_running():
        if not ours():
            raise ModsError("the game is running under a deployment this tool did not make "
                            "(a user session?); close it first")
        afgame.quit_game(force=True)  # our own test session with another mod set
    if not ours():
        STATE.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(DEPLOYED, BASELINE)
    write_profile(wanted)
    deploy(TEST)
    MARKER.write_text(json.dumps({"mods": sorted(wanted), "deployed_sha256": digest(DEPLOYED)}) + "\n",
                      encoding="utf-8")
    loaded, paks = deployed_mods(), deployed_paks()
    if loaded != wanted or paks:
        raise ModsError(f"aftest deployment check failed: UE4SS mods {sorted(loaded)} (want {sorted(wanted)}), "
                        f"pak mods {paks}")


def restore():
    if not marker():
        return
    if afgame.process_running():
        if not ours():
            raise ModsError("the game is running under a deployment this tool did not make; not restoring")
        afgame.quit_game(force=True)
    deploy(DEFAULT)
    same = BASELINE.exists() and BASELINE.read_bytes() == DEPLOYED.read_bytes()
    MARKER.unlink(missing_ok=True)
    if not same:
        raise ModsError(f"default redeployed, but ue5_deployed.txt differs from {BASELINE} "
                        "(default profile changed meanwhile?); check the deployment")
    BASELINE.unlink(missing_ok=True)


def check_no_ugc(log=GAME_LOG):
    """mod.io UGC plugins are outside Amethyst; fail if the running session loaded any."""
    try:
        text = log.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return
    plugins = sorted({line.split("Added UGC plugin '", 1)[1].split("'", 1)[0]
                      for line in text.splitlines() if "Added UGC plugin '" in line})
    if plugins:
        raise ModsError(f"the game loaded mod.io UGC plugins {plugins}; the test is not isolated")


@contextlib.contextmanager
def test_mods(*mods):
    """Deploy AFTestBridge + `mods` only for the duration of the block, then restore default."""
    use(*mods)
    try:
        yield
    finally:
        restore()


def run_isolated(mods, test, world=True):
    """Exit code of `test()` run with only `mods` (+ AFTestBridge) deployed, in the AFTest world
    unless world=False. Restores the default profile afterwards unless AFTEST_KEEP_MODS=1
    (for several runs in a row; finish with `aftest_mods.py restore`)."""
    rc = 1
    try:
        use(*mods)
        if world:
            afgame.start()
            afgame.enter(afgame.WORLD_PREFIX)
        check_no_ugc()
        rc = test()
    except (ModsError, afgame.GameError, afgame.BridgeError) as e:
        print(f"FAIL: {e}")
    finally:
        if os.environ.get("AFTEST_KEEP_MODS") != "1":
            try:
                restore()
            except (ModsError, afgame.GameError) as e:
                print(f"FAIL: restoring the default mod profile: {e}")
                rc = 1
    return rc


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="action", required=True)
    sub.add_parser("use").add_argument("mods", nargs="*")
    sub.add_parser("restore")
    sub.add_parser("status")
    a = p.parse_args()
    try:
        if a.action == "use":
            use(*a.mods)
        elif a.action == "restore":
            restore()
        m = marker()
        print(f"deployed: {'aftest ' + ','.join(m['mods']) if m and ours() else 'not an aftest deployment'}"
              f"{' (stale marker)' if m and not ours() else ''}")
        print(f"UE4SS mods: {', '.join(sorted(deployed_mods()))}; pak mods: {len(deployed_paks())}")
        print(f"game: {afgame.state()}")
    except (ModsError, afgame.GameError) as e:
        print(f"aftest_mods: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
