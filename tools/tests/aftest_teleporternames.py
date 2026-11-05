#!/usr/bin/env python3
"""Live RandomTweaks nameable-teleporter checks through AFTestBridge, in the AFTest world with
only AFTestBridge + RandomTweaks deployed (tools/aftest_mods.py; starts the game if needed).

Spawns teleporter pads next to the player and drives the real BP functions the mod hooks:
  - server path: DeliverString("<id>:<name>") / DeliverString("<tag>") on the pad,
  - load path: OnLoadedFromSave (named pad, and a never-upgraded pre-1.0 pad),
  - UI path: InteractWith_B_LocalFX opens the tag popup; text typed via SetText, Accept handler,
  - persistence: quits the game, re-enters AFTest and checks the saved pads (skip: --no-reload).
The pads are removed from the world save at the end. Prints ALL PASS / exits 1 on any failure.

Not covered (human check): typing with the real keyboard (keybinds bubbling up from the text
box), the popup's look, a multiplayer client with/without the mod.

  tools/tests/aftest_teleporternames.py [--no-reload]
"""
import re
import sys
import time
import uuid
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # tools/: harness modules
import afgame  # noqa: E402
from afbridge import request  # noqa: E402
from aftest_mods import run_isolated  # noqa: E402

PAD_CLASS = "/Game/Blueprints/DeployedObjects/Furniture/Deployed_TeleporterPad.Deployed_TeleporterPad_C"
STATICS = "/Script/Engine.Default__GameplayStatics"
POPUP_CLASS = "W_MenuPopup_TeleporterInput_C"
ACCEPT = "BndEvt__W_MenuPopup_TeleporterInput_Button_Accept_K2Node_ComponentBoundEvent_8_OnButtonClickedEvent__DelegateSignature"
OPTION_CLASS = "W_TeleporterTag_Option_C"
OPTION_CLICK = "BndEvt__W_TeleporterTag_Option_Button_Accept_K2Node_ComponentBoundEvent_0_OnButtonClickedEvent__DelegateSignature"
failures = []


def bridge(*args, timeout=15):
    r = request(*args, timeout=timeout)
    if not r.get("ok"):
        raise RuntimeError(f"{args[0]} {args[1:3]}: {r.get('error')}")
    return r["result"]


def check(cond, what):
    print(("ok   " if cond else "FAIL ") + what)
    if not cond:
        failures.append(what)


def name_id(name):
    """Port of teleporter_names::nameToId."""
    h = 2166136261
    for c in name:
        v = ord(c.lower()) if "A" <= c <= "Z" else ord(c)
        v &= 0xFFFF
        h = ((h ^ (v & 0xFF)) * 16777619) & 0xFFFFFFFF
        h = ((h ^ (v >> 8)) * 16777619) & 0xFFFFFFFF
    return 1000 + (h & 0x3FFFFFFF)


def obj_path(export):
    """Object path from an export like Class'/Game/...:PersistentLevel.X'."""
    m = re.search(r"'([^']+)'", export)
    return m.group(1) if m else None


def spawn_pad(dx):
    loc = bridge("call", "player", "K2_GetActorLocation")["params"]["ReturnValue"]
    x, y = (float(v) for v in re.findall(r"[XY]=(-?[\d.]+)", loc)[:2])
    transform = f"(Translation=(X={x + dx},Y={y + 300},Z=110))"
    pad = obj_path(bridge("call", STATICS, "BeginDeferredActorSpawnFromClass", "WorldContextObject=player",
                          f"ActorClass={PAD_CLASS}", f"SpawnTransform={transform}",
                          "CollisionHandlingOverride=AlwaysSpawn")["params"]["ReturnValue"])
    if not pad:
        raise RuntimeError("pad spawn failed")
    bridge("call", STATICS, "FinishSpawningActor", f"Actor={pad}", f"SpawnTransform={transform}")
    # the world save skips deployables with AssetID "-1"; real deploys get a GUID
    data = changeable(pad).replace('AssetID="-1"', f'AssetID="{uuid.uuid4().hex.upper()}"')
    bridge("set", pad, "ChangeableData", data)
    bridge("set", pad, "DeployedByPlayer", "True")
    return pad


def changeable(pad):
    return bridge("get", pad, "ChangeableData")["value"]


def pad_name(pad):
    m = re.search(r'PlayerMadeString="((?:[^"\\]|\\.)*)"', changeable(pad))
    return m.group(1) if m else ""


def frequency(pad):
    """TeleporterFrequency dynamic property; the export omits Value when it is 0."""
    m = re.search(r"\(Key=TeleporterFrequency(?:,Value=(-?\d+))?\)", changeable(pad))
    return (int(m.group(1)) if m.group(1) else 0) if m else None


def teleporter_ids(pad):
    child = f"{pad}->TeleporterBP->ChildActor"
    return (bridge("get", child, "MyTeleporterID")["value"], bridge("get", child, "DestinationTeleporterID")["value"])


def item_name(pad):
    """GetItemNameText as display text: INVTEXT("x") is ours, vanilla's is a LOCGEN format."""
    text = bridge("call", pad, "GetItemNameText")["params"]["ReturnValue"]
    m = re.fullmatch(r'INVTEXT\("(.*)"\)', text)
    return m.group(1) if m else text


def deliver(pad, s):
    bridge("call", pad, "DeliverString", f"String={s}", "FromSave=False")


def location_map():
    return bridge("get", "first:Abiotic_Survival_GameState_C", "TeleporterLocationMap")["value"]


def old_strings():
    return bridge("get", "first:Abiotic_Survival_GameMode_C", "OldTeleporterStrings")["value"]


def expect_named(pad, name, label):
    i = name_id(name)
    check(pad_name(pad) == name, f"{label}: name {pad_name(pad)!r} == {name!r}")
    check(frequency(pad) == i, f"{label}: frequency {frequency(pad)} == id {i}")
    check(teleporter_ids(pad) == (f"teleporterpad{i}",) * 2, f"{label}: teleporter ids {teleporter_ids(pad)}")
    check(item_name(pad).endswith(f"({name})"), f"{label}: item name {item_name(pad)!r}")


def open_popup(pad):
    """Open the pad's tag popup like the interaction does; returns its bridge target."""
    before = set(bridge("find", POPUP_CLASS)["objects"])
    bridge("call", pad, "InteractWith_B_LocalFX", "Hold=False")
    time.sleep(0.3)
    new = [o for o in bridge("find", POPUP_CLASS)["objects"] if o not in before]
    if len(new) != 1:
        raise RuntimeError(f"expected one new popup, got {new}")
    return f"named:{POPUP_CLASS}:" + new[0].rsplit(".", 1)[1]  # widget paths can't be looked up


def popup_text(popup):
    text = bridge("call", f"{popup}->TextBox_Line", "GetText")["params"]["ReturnValue"]
    m = re.fullmatch(r'INVTEXT\("(.*)"\)', text)
    return m.group(1) if m else text


def popup_options(popup):
    """[(target, label, enabled)] of the popup's list, in order."""
    children = bridge("call", f"{popup}->Scrollbox_Options", "GetAllChildren")["params"]["ReturnValue"]
    out = []
    for name in re.findall(r"\.(W_TeleporterTag_Option_C_\d+)'", children):
        target = f"named:{OPTION_CLASS}:{name}"
        label = bridge("call", f"{target}->TextName", "GetText")["params"]["ReturnValue"]
        m = re.fullmatch(r'INVTEXT\("(.*)"\)', label)
        out.append((target, m.group(1) if m else label, bridge("get", target, "bIsEnabled")["value"] == "True"))
    return out


def click_option(popup, label):
    for target, text, _ in popup_options(popup):
        if text == label:
            bridge("call", target, OPTION_CLICK)
            return
    raise RuntimeError(f"no option {label!r}")


def accept(popup, pad):
    bridge("call", popup, ACCEPT)
    bridge("call", pad, "NoClicked_Event")  # close the inventory like Cancel


def popup_accept(pad, text=None):
    """Open, optionally type `text`, accept. Returns (text shown on open, IsReadOnly, bIsEnabled)."""
    popup = open_popup(pad)
    line = f"{popup}->TextBox_Line"
    shown = popup_text(popup)
    read_only = bridge("get", line, "IsReadOnly")["value"]
    enabled = bridge("get", line, "bIsEnabled")["value"]
    if text is not None:
        bridge("call", line, "SetText", f"InText={text}")
    accept(popup, pad)
    return shown, read_only, enabled


def world_save(level):
    m = re.search(rf'\("{level}", "[^\']*\'([^\']+)\'"\)', bridge("get", "first:Abiotic_GameInstance_C", "WorldSaveMap")["value"])
    if not m:
        raise RuntimeError(f"no cached world save for {level}")
    return m.group(1)


def asset_id(pad):
    return re.search(r'AssetID="([^"]*)"', changeable(pad)).group(1)


def top_groups(text):
    """Depth-2 (...) groups of a UE text-format container export (map pairs, array items)."""
    out, depth, start, in_str = [], 0, None, False
    for i, ch in enumerate(text):
        if in_str:
            if ch == '"' and text[i - 1] != "\\":
                in_str = False
            continue
        if ch == '"':
            in_str = True
        elif ch == "(":
            depth += 1
            if depth == 2:
                start = i
        elif ch == ")":
            if depth == 2:
                out.append(text[start:i + 1])
            depth -= 1
    return out


def saved_entry(save, asset):
    """The (key, value) pair text of `asset` in a world save's DeployedObjectMap."""
    for pair in top_groups(bridge("get", save, "DeployedObjectMap")["value"]):
        if pair.startswith(f'("{asset}"'):
            return pair
    raise RuntimeError(f"{asset} not in {save}")


def world_save_levels():
    return re.findall(r'\(\\?"([^"\\]+)\\?",', bridge("get", "first:Abiotic_GameInstance_C", "WorldSaveMap")["value"])


def remove_pads(pads):
    for pad in pads:
        try:
            bridge("call", pad, "SaveDeployable", "RemoveFromSave=True")
            bridge("call", pad, "K2_DestroyActor")
        except RuntimeError as e:
            print(f"cleanup: {e}")


def force_save():
    """Write the world save to disk now (console quit skips saving) and wait for the queue."""
    bridge("call", "first:Abiotic_GameInstance_C", "ForceSave")
    for _ in range(240):
        time.sleep(0.5)
        if bridge("call", "first:Abiotic_GameInstance_C", "HasPendingSaves")["params"]["ReturnValue"] == "False":
            return
    raise RuntimeError("world save still pending after 120 s")


def live_pads():
    """Pads in the world; destroyed ones stay findable until GC, IsValid drops them."""
    pads = [o.split(" ", 1)[1] for o in bridge("find", "Deployed_TeleporterPad_C")["objects"]]
    return [p for p in pads if bridge("call", "/Script/Engine.Default__KismetSystemLibrary", "IsValid",
                                      f"Object={p}")["params"]["ReturnValue"] == "True"]


def main():
    reload = "--no-reload" not in sys.argv
    stale = live_pads()
    if stale:
        print(f"removing {len(stale)} pad(s) left by an earlier run")
        remove_pads(stale)
    levels_at_start = world_save_levels()
    a, b, c = spawn_pad(0), spawn_pad(250), spawn_pad(500)
    pads = [a, b, c]
    try:
        time.sleep(1.0)  # BeginPlay's 0.5 s delayed setup
        vanilla_old = old_strings()

        # server path: name, same name on a second pad (case-insensitive), tag pick clears
        deliver(a, f"{name_id('Main Base')}:Main Base")
        expect_named(a, "Main Base", "A named")
        deliver(b, f"{name_id('MAIN BASE')}:MAIN BASE")
        check(teleporter_ids(b) == teleporter_ids(a), "B 'MAIN BASE' links to A 'Main Base'")
        check(f'"{name_id("Main Base")}"' in location_map(), "GameState location map has the name id")
        deliver(b, "3")
        check(pad_name(b) == "" and frequency(b) == 3, f"B tag 3 clears the name ({pad_name(b)!r}, {frequency(b)})")
        check(teleporter_ids(b) == ("teleporterpad3",) * 2, f"B tag ids {teleporter_ids(b)}")
        check("Main" not in item_name(b) and "DELTA" in item_name(b).upper(),
              f"B item name is the tag again: {item_name(b)!r}")
        deliver(c, "5")
        check(pad_name(c) == "" and frequency(c) == 5, "C plain tag 5 stays vanilla")

        # load path: a named pad keeps its name (no AttemptSaveUpgrade), no OldTeleporterStrings entry
        bridge("call", a, "OnLoadedFromSave", "NewLifespan=-1")
        expect_named(a, "Main Base", "A after OnLoadedFromSave")
        check(old_strings() == vanilla_old, f"OldTeleporterStrings untouched ({old_strings()})")

        # load path: pre-1.0 pad (PlayerMadeString, no frequency) gets the name's id
        data = changeable(c)
        data = re.sub(r",?DynamicProperties=\([^)]*\)\)?", "", data)
        data = data[:-1] + ',PlayerMadeString="Old Lab")' if "PlayerMadeString" not in data else data
        bridge("set", c, "ChangeableData", data)
        bridge("call", c, "OnLoadedFromSave", "NewLifespan=-1")
        expect_named(c, "Old Lab", "C legacy after OnLoadedFromSave")

        # UI path: popup shows the name, is editable; typed name through Accept
        shown, read_only, enabled = popup_accept(a, text="Lab West")
        check(shown == "Main Base", f"popup shows the pad name ({shown!r})")
        check(read_only == "False" and enabled == "True",
              f"popup text line editable (IsReadOnly={read_only}, bIsEnabled={enabled})")
        expect_named(a, "Lab West", "A renamed via popup")
        popup_accept(a)  # accept untouched: keeps the name
        expect_named(a, "Lab West", "A accepted unchanged")
        popup_accept(b, text="lab west")
        check(teleporter_ids(a) == teleporter_ids(b), "A and B linked by popup names")

        # name list instead of the tags: names with pad counts, current one selected
        popup = open_popup(c)
        opts = popup_options(popup)
        labels = [o[1] for o in opts]
        check(labels == ["Lab West (2)", "Old Lab (1)"], f"list shows the pad names, no tags: {labels}")
        check([o[2] for o in opts] == [True, False], f"C's own name selected (disabled): {[o[2] for o in opts]}")
        check(popup_text(popup) == "Old Lab", f"C popup text {popup_text(popup)!r}")
        click_option(popup, "Lab West (2)")
        check(popup_text(popup) == "Lab West", f"clicking a name fills the text line ({popup_text(popup)!r})")
        accept(popup, c)
        expect_named(c, "Lab West", "C renamed via the list")
        popup_accept(c, text="Delta")

        # a tagged pad keeps its tag when accepted untouched; picking a name equal to its tag text still names it
        deliver(b, "3")  # DELTA
        popup = open_popup(b)
        check(popup_text(popup) == "DELTA", f"tagged pad shows its tag ({popup_text(popup)!r})")
        accept(popup, b)
        check(pad_name(b) == "" and frequency(b) == 3, f"B untouched accept keeps tag 3 ({pad_name(b)!r}, {frequency(b)})")
        popup = open_popup(b)
        click_option(popup, "Delta (1)")
        accept(popup, b)
        expect_named(b, "Delta", "B picked 'Delta' from the list over tag DELTA")

        # host: pads only in a level's world save (level not streamed in) are listed. The save of
        # Facility_Labs is loaded by the popup itself; a pad entry is injected into it, then restored.
        if "Facility_Labs" in levels_at_start:
            print("skip popup loading an unloaded level's save: already cached this session (fresh game start covers it)")
        else:
            check("Facility_Labs" in world_save_levels(),
                  f"popup loaded the unloaded level's save ({levels_at_start} -> {world_save_levels()})")
        labs = world_save("Facility_Labs")
        original = bridge("get", labs, "DeployedObjectMap")["value"]
        hidden = saved_entry(world_save("Facility"), asset_id(a))
        hidden_id = uuid.uuid4().hex.upper()
        hidden = hidden.replace(asset_id(a), hidden_id).replace('"Lab West"', '"Hidden Base"')
        hidden = re.sub(r"(Key=TeleporterFrequency,Value=)-?\d+", rf"\g<1>{name_id('Hidden Base')}", hidden)
        injected = f"({hidden})" if original in ("", "()") else original[:-1] + "," + hidden + ")"
        bridge("set", labs, "DeployedObjectMap", injected)
        try:
            popup = open_popup(a)
            labels = [o[1] for o in popup_options(popup)]
            check(labels == ["Delta (2)", "Hidden Base (1)", "Lab West (1)"], f"list includes the saved-only pad: {labels}")
            accept(popup, a)
        finally:
            bridge("set", labs, "DeployedObjectMap", original or "()")
        check(bridge("get", labs, "DeployedObjectMap")["value"] == original, "Facility_Labs save restored")
        expect_named(a, "Lab West", "A untouched after list check")
        d = spawn_pad(750)
        time.sleep(1.0)
        deliver(d, f"{name_id('Gone Base')}:Gone Base")
        bridge("call", d, "K2_DestroyActor")  # findable until GC; the game also drops it from the save
        popup = open_popup(a)
        labels = [o[1] for o in popup_options(popup)]
        accept(popup, a)
        check(labels == ["Delta (2)", "Lab West (1)"], f"destroyed pad not listed: {labels}")

        if reload:
            force_save()
            afgame.quit_game(False)
            afgame.start()
            afgame.enter(afgame.WORLD_PREFIX)
            time.sleep(2.0)
            pads = live_pads()
            names = sorted(pad_name(p) for p in pads)
            check(names == sorted(["Lab West", "Delta", "Delta"]), f"names after reload: {names}")
            for p in pads:
                n = pad_name(p)
                if n:
                    expect_named(p, n, f"reloaded {n!r}")
    finally:
        remove_pads(pads)
        try:
            force_save()
        except RuntimeError as e:
            print(f"cleanup: {e}")
    if failures:
        print(f"{len(failures)} FAILED")
        return 1
    print("ALL PASS")
    return 0


if __name__ == "__main__":
    sys.exit(run_isolated(["RandomTweaks"], main))
