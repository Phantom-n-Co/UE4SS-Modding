#!/usr/bin/env python3
"""Start/enter/quit the game for automated tests, through AFTestBridge only (no input).

  tools/afgame.py up [--world AFTest]   # start the game if needed, then enter the test world
  tools/afgame.py start                 # launch via Steam, wait for the bridge
  tools/afgame.py enter [--world AFTest]
  tools/afgame.py state                 # menu page / map the game is on
  tools/afgame.py quit [--force]        # `console quit`; --force kills a crashed/hung game

`enter` continues the world if it exists, otherwise creates it (singleplayer) and
finishes character creation with the defaults. Only worlds whose name starts with
"AFTest" are allowed: loading a world autosaves it and syncs it to Steam Cloud, so
the user's real worlds must never be touched.
"""
import argparse
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from afbridge import BridgeError, connect, request  # noqa: E402

APP_ID = "427410"
WORLD_PREFIX = "AFTest"
MASTER = "first:W_MainMenu_Master_C"
SWITCHER = MASTER + "->MainMenuWidgetSwitcher"
BTN = "_K2Node_ComponentBoundEvent_{}_ButtonPressed__DelegateSignature"
INITIAL_PLAY = "BndEvt__W_MainMenuButton_Play_Host" + BTN.format(5)
PLAY_HOST = "BndEvt__W_MainMenu_Play_Button_HostGame" + BTN.format(1)
PLAY_CONTINUE = "BndEvt__W_MainMenu_Play_Button_Continue" + BTN.format(3)
HOST_BUTTON = "BndEvt__W_MainMenu_HostGame_W_MainMenuButton_Host" + BTN.format(1)  # also on ContinueGame
NOTICE_OK = "BndEvt__W_BetaNotice_Button_Enter_K2Node_ComponentBoundEvent_2_OnButtonClickedEvent__DelegateSignature"
JOB_NEXT = "BndEvt__W_Character_Job_Selection_Button_Finish" + BTN.format(1)
TRAITS_FINISH = "BndEvt__Button_Finish" + BTN.format(0)


class GameError(Exception):
    pass


def bridge(*args, timeout=15):
    reply = request(*args, timeout=timeout)
    if not reply.get("ok"):
        raise GameError(f"{' '.join(args[:3])}: {reply.get('error')}")
    return reply["result"]


def alive():
    """Bridge up and the game thread ticking (a crashed game still accepts connections)."""
    try:
        return request("ping", timeout=15).get("ok", False)
    except BridgeError:
        return False


def process_running():
    return bool(game_pids())


def game_pids():
    out = subprocess.run(["ps", "-eo", "pid=,args="], capture_output=True, text=True).stdout
    return [int(line.split(None, 1)[0]) for line in out.splitlines()
            if "AbioticFactor-Win64-Shipping.exe" in line]


def bridge_listening():
    try:
        connect(wait=0).close()
        return True
    except BridgeError:
        return False


def wait(what, check, timeout, interval=1.0):
    deadline = time.monotonic() + timeout
    while True:
        try:
            value = check()
        except (BridgeError, GameError):
            value = None  # loading screens time out the bridge; keep polling
        if value:
            return value
        if time.monotonic() >= deadline:
            raise GameError(f"timed out after {timeout:.0f}s waiting for {what}")
        time.sleep(interval)


def count(cls):
    return bridge("find", cls)["count"]


def player_map():
    """Map path of the live player character, or None."""
    try:
        obj = bridge("get", "player", "bHidden")["object"]
    except GameError:
        return None
    return obj.split(" ", 1)[1].split(":", 1)[0] if " " in obj else None


def in_world():
    m = player_map()
    return bool(m) and "MainMenu" not in m and count("W_PlayerHUD_Main_C") > 0


def active_world():
    return bridge("get", "first:Abiotic_GameInstance_C", "ActiveWorldSaveName")["value"]


def menu_page():
    value = bridge("call", SWITCHER, "GetActiveWidget")["params"]["ReturnValue"]
    # ".../W_MainMenu_Play.W_MainMenu_Play_C'...W_MainMenu_Play'" -> W_MainMenu_Play
    return value.rstrip("'").rsplit(".", 1)[-1] if value else None


def state():
    if not alive():
        if not process_running():
            return "not running"
        return "hung or crashed (process alive, game thread not ticking; check for a 'Fatal error!' dialog)" \
            if bridge_listening() else "starting (bridge not up)"
    if in_world():
        return f"in world {active_world()!r}: {player_map()}"
    try:
        return f"main menu: {menu_page()}"
    except GameError as e:
        return f"unknown ({e})"


def start():
    if alive():
        return
    if not process_running():
        subprocess.Popen(["steam", "-applaunch", APP_ID], stdout=subprocess.DEVNULL,
                         stderr=subprocess.DEVNULL, start_new_session=True)
    wait("the bridge", alive, 300, 2)


def goto_page(page, handler_target, handler):
    if menu_page() != page:
        bridge("call", handler_target, handler)
        wait(f"menu page {page}", lambda: menu_page() == page, 20)


def world_entries():
    """{FolderName: WorldEntryItem path} of the world list (filled by the Continue page)."""
    entries = {}
    for obj in bridge("find", "WorldEntryItem")["objects"]:
        path = obj.split(" ", 1)[1]
        name = bridge("get", path, "FolderName")["value"]
        if bridge("get", path, "bIsBackup")["value"] == "False":
            entries[name] = path
    return entries


def finish_character_creation():
    """New world or new character: accept the notice, default job and traits."""
    if count("W_Character_Trait_Selection_C") == 0:
        return False
    if count("W_MenuPopup_YesNo_C") > 0:
        bridge("call", "first:W_MenuPopup_YesNo_C", NOTICE_OK)
        time.sleep(1)
    bridge("call", "first:W_Character_Job_Selection_C", JOB_NEXT)
    time.sleep(1)
    bridge("call", "first:W_Character_Trait_Selection_C", TRAITS_FINISH)
    time.sleep(1)
    bridge("call", "first:W_Character_Trait_Selection_C", "PopupYesSelected")
    return True


def enter(world):
    if not world.startswith(WORLD_PREFIX):
        raise GameError(f"refusing world {world!r}: test worlds must start with {WORLD_PREFIX!r}")
    if in_world():
        current = active_world()
        if current != world:
            raise GameError(f"already in world {current!r}, not {world!r}; quit first")
        print(f"already in {world}")
        return
    wait("the main menu", lambda: count("W_MainMenu_Master_C") > 0, 120)
    if bridge("get", MASTER, "HasStartedSplashFade")["value"] != "True":
        bridge("call", MASTER, "SplashKeyDown")
    wait("the main menu after the splash", lambda: menu_page() is not None, 30)
    goto_page("W_MainMenu_Play", "first:W_MainMenu_Initial_C", INITIAL_PLAY)

    goto_page("W_MainMenu_ContinueGame", "first:W_MainMenu_Play_C", PLAY_CONTINUE)
    entries = wait("the world list", world_entries, 30)
    if world in entries:
        print(f"continuing {world}")
        bridge("call", "first:W_WorldSelectMenu_C", "SelectNewWorldEntry", f"World={entries[world]}")
        selected = bridge("get", "first:W_WorldSelectMenu_C->SelectedWorld", "FolderName")["value"]
        if selected != world:
            raise GameError(f"world select picked {selected!r}, not {world!r}; not hosting")
        bridge("call", "first:W_MainMenu_ContinueGame_C", HOST_BUTTON)
    else:
        print(f"creating {world}")
        bridge("call", "first:W_MainMenu_ContinueGame_C",
               "BndEvt__W_MainMenu_HostGame_W_MainMenuButton_Back" + BTN.format(0))
        wait("menu page W_MainMenu_Play", lambda: menu_page() == "W_MainMenu_Play", 20)
        goto_page("W_MainMenu_HostGame", "first:W_MainMenu_Play_C", PLAY_HOST)
        bridge("call", "first:W_HostGameSetup_C->TextInput_WorldName", "SetText", f"InText={world}")
        if bridge("get", "first:W_MainMenu_HostGame_C", "SingleplayerOnly")["value"] != "True":
            raise GameError("new-world page is not singleplayer-only; not hosting")
        bridge("call", "first:W_MainMenu_HostGame_C", HOST_BUTTON)

    deadline = time.monotonic() + 180
    while time.monotonic() < deadline:
        try:
            if in_world():
                current = active_world()
                if current != world:
                    raise SystemExit(f"afgame: loaded world {current!r} instead of {world!r}")
                print(f"in world {world!r}: {player_map()}")
                return
            finish_character_creation()
        except (BridgeError, GameError):
            pass  # loading
        time.sleep(2)
    raise GameError("timed out entering the world (popup? check a computer_use capture)")


def quit_game(force):
    if alive():
        try:
            request("console", "quit", timeout=5)
        except BridgeError:
            pass
        wait("the game to exit", lambda: not process_running(), 60)
        return
    if not process_running():
        return
    if not force:
        raise GameError("game is not responding (crashed or loading); rerun with --force to kill it")
    for pid in game_pids():
        subprocess.run(["kill", str(pid)], check=False)
    try:
        wait("the game to exit after SIGTERM", lambda: not process_running(), 20)
    except GameError:
        for pid in game_pids():
            subprocess.run(["kill", "-9", str(pid)], check=False)
        wait("the game to exit after SIGKILL", lambda: not process_running(), 20)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("action", choices=["up", "start", "enter", "state", "quit"])
    p.add_argument("--world", default=WORLD_PREFIX)
    p.add_argument("--force", action="store_true", help="quit: kill the game if it does not respond")
    a = p.parse_args()
    try:
        if a.action == "state":
            print(state())
        elif a.action == "start":
            start()
        elif a.action == "enter":
            enter(a.world)
        elif a.action == "up":
            start()
            enter(a.world)
        elif a.action == "quit":
            quit_game(a.force)
    except (GameError, BridgeError) as e:
        print(f"afgame: {e}", file=sys.stderr)
        print(f"afgame: state: {state()}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
