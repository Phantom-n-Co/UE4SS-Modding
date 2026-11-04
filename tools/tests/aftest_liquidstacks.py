#!/usr/bin/env python3
"""Live StackAnything liquid-stack checks through AFTestBridge, in the AFTest world with only
AFTestBridge + StackAnything deployed (tools/aftest_mods.py; starts the game if needed).

Each scenario writes the hotbar (CharacterHotbarInventory.CurrentInventory) directly,
then calls the real Server_TryChangeValueInLiquidContainer / AreItemsStackable so
StackAnything's hooks run, and checks the resulting slots. The hotbar is cleared
at the end. Prints ALL PASS / exits 1 on any failure.

Not covered: the held item actor (ItemInHand_BP) path. Drinking computes the new
level from the actor's copy, which only exists when an item is really equipped.

  tools/tests/aftest_liquidstacks.py
"""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # tools/: harness modules
from afbridge import BridgeError, request  # noqa: E402
from aftest_mods import run_isolated  # noqa: E402

HOTBAR = "player->CharacterHotbarInventory"
FN_LIB = "/Game/Blueprints/Libraries/ItemFunctionLibrary.Default__ItemFunctionLibrary_C"
SLOTS = 8
EMPTY = "(ChangeableData=(CurrentItemDurability=0.000000,MaxItemDurability=0.000000,CurrentStack=0))"
failures = []


def bridge(*args, timeout=15):
    r = request(*args, timeout=timeout)
    if not r.get("ok"):
        raise RuntimeError(f"{args[0]} {args[1:3]}: {r.get('error')}")
    return r["result"]


def slot(row, stack, level=0, liquid="Empty", made="", asset="AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"):
    made_text = made.replace("\\", "\\\\").replace('"', '\\"')
    return (f'(ItemDataTable=(RowName="{row}"),ChangeableData=(AssetID="{asset}",'
            f"CurrentItemDurability=100.000000,MaxItemDurability=100.000000,CurrentStack={stack},"
            f'LiquidLevel={level},CurrentLiquid={liquid},PlayerMadeString="{made_text}"))')


def set_hotbar(*slots):
    items = list(slots) + [EMPTY] * (SLOTS - len(slots))
    bridge("set", HOTBAR, "CurrentInventory", "(" + ",".join(items) + ")")


def split_top(text):
    """Top-level comma-separated (...) groups of a UE text-format array."""
    out, depth, start, in_str = [], 0, None, False
    for i, ch in enumerate(text):
        if in_str:
            if ch == "\\":
                continue
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


def field(text, name, default=None):
    m = re.search(rf'\b{name}=("(?:[^"\\]|\\.)*"|[^,)]*)', text)
    if not m:
        return default
    v = m.group(1)
    return v[1:-1].replace('\\"', '"').replace("\\\\", "\\") if v.startswith('"') else v


def hotbar():
    value = bridge("get", HOTBAR, "CurrentInventory")["value"]
    slots = []
    for s in split_top(value):
        row = field(s, "RowName", "None")
        slots.append({
            "row": row,
            "stack": int(field(s, "CurrentStack", "1")),
            "level": int(field(s, "LiquidLevel", "-1")),
            "liquid": field(s, "CurrentLiquid", "Empty"),
            "made": field(s, "PlayerMadeString", ""),
            "asset": field(s, "AssetID", "-1"),
        })
    return slots


def used(slots):
    return [(i, s) for i, s in enumerate(slots) if s["row"] != "None" and s["stack"] > 0]


def check(name, ok, detail=""):
    print(f"{'ok  ' if ok else 'FAIL'} {name}" + (f": {detail}" if detail and not ok else ""))
    if not ok:
        failures.append(name)


def change(inv, index, value, liquid, row="None"):
    return bridge("call", "player", "Server_TryChangeValueInLiquidContainer", f"Inventory={inv}",
                  f"SlotIndex={index}", f"NewLiquidValue={value}", f"LiquidType={liquid}",
                  f'OptionalItemRow=(RowName="{row}")')


def summary(slots):
    return str([(i, s["row"], s["stack"], s["level"], s["liquid"], s["made"][:20], s["asset"][:8])
                for i, s in used(slots)])


def param_true(reply, name):
    return str(reply.get("params", {}).get(name, "")).lower() == "true"


def main():
    try:
        request("ping", timeout=5)
    except BridgeError as e:
        print(f"FAIL: {e} (game running with AFTestBridge?)")
        return 1
    player = bridge("get", "pc", "AcknowledgedPawn")["value"]
    pname = player.split("'")[1] if "'" in player else player
    hb = next((o.split(" ", 1)[1] for o in bridge("find", "Abiotic_InventoryComponent_C")["objects"]
               if o.endswith(pname.rsplit(".", 1)[-1] + ".CharacterHotbarInventory")), None)
    if not hb:
        print("FAIL: player hotbar component not found")
        return 1

    try:
        # 1. Soup into 3 empty bowls, no matching stack: filled one stays, 2 empties move.
        set_hotbar(slot("SoupBowl", 3))
        change(hb, 0, 500, "Soup", "soup_pea")
        s = hotbar()
        made = s[0]["made"]
        rest = [x for i, x in used(s) if i != 0]
        check("fill: hand slot holds 1 filled bowl",
              s[0]["stack"] == 1 and s[0]["level"] == 500 and s[0]["liquid"] == "Soup" and made, summary(s))
        check("fill: other 2 bowls stay empty in another slot",
              len(rest) == 1 and rest[0]["stack"] == 2 and rest[0]["level"] <= 0 and rest[0]["made"] == "", summary(s))
        check("fill: moved stack has its own AssetID",
              bool(rest) and rest[0]["asset"] not in ("-1", "", s[0]["asset"]), summary(s))

        # 2. Same fill with a matching full stack: filled one merges into it.
        set_hotbar(slot("SoupBowl", 3), slot("SoupBowl", 2, 500, "Soup", made, "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"))
        change(hb, 0, 500, "Soup", "soup_pea")
        s = hotbar()
        check("fill merge: full stack grows to 3", s[1]["stack"] == 3 and s[1]["level"] == 500, summary(s))
        check("fill merge: hand keeps 2 empty bowls",
              s[0]["stack"] == 2 and s[0]["level"] <= 0 and s[0]["made"] == "" and len(used(s)) == 2, summary(s))

        # 3. Different recipe on the other stack: no merge.
        set_hotbar(slot("SoupBowl", 3), slot("SoupBowl", 2, 500, "Soup", "not-" + made, "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"))
        change(hb, 0, 500, "Soup", "soup_pea")
        s = hotbar()
        check("fill other recipe: no merge", s[1]["stack"] == 2 and s[0]["stack"] == 1 and len(used(s)) == 3, summary(s))

        # 4. Eat from 3 full bowls twice: eaten one stays in hand, 2 full move out.
        set_hotbar(slot("SoupBowl", 3, 500, "Soup", made))
        change(hb, 0, 250, "Soup", "soup_pea")
        s = hotbar()
        rest = [x for i, x in used(s) if i != 0]
        check("eat: hand slot holds 1 half bowl", s[0]["stack"] == 1 and s[0]["level"] == 250, summary(s))
        check("eat: other 2 stay full", len(rest) == 1 and rest[0]["stack"] == 2 and rest[0]["level"] == 500
              and rest[0]["made"] == made, summary(s))
        change(hb, 0, 0, "Soup", "soup_pea")
        s = hotbar()
        rest = [x for i, x in used(s) if i != 0]
        check("eat again: only the hand bowl empties, full stack untouched",
              s[0]["stack"] == 1 and s[0]["level"] <= 0 and len(rest) == 1 and rest[0]["stack"] == 2
              and rest[0]["level"] == 500, summary(s))

        # 5. Drink from 3 full water bottles.
        set_hotbar(slot("waterbottle", 3, 100, "Water"))
        change(hb, 0, 50, "Water")
        s = hotbar()
        rest = [x for i, x in used(s) if i != 0]
        check("drink: 1 half bottle in hand, 2 full elsewhere",
              s[0]["stack"] == 1 and s[0]["level"] == 50 and len(rest) == 1 and rest[0]["stack"] == 2
              and rest[0]["level"] == 100, summary(s))

        # 6. Fill one of 3 empty cooking pots with water; the stove must find both stacks by AssetID.
        set_hotbar(slot("cookingpot", 3))
        change(hb, 0, 100, "Water")
        s = hotbar()
        check("pot fill: split into 2 stacks", len(used(s)) == 2, summary(s))
        # The stove finds held cookware through LocateInventoryItemByAssetID (DoesItemExistInSlot,
        # which never matches "-1").
        for i, x in used(s):
            r = bridge("call", HOTBAR, "LocateInventoryItemByAssetID", f'ItemRowName=(RowName="{x["row"]}")',
                       f'AssetID={x["asset"]}')
            ok = param_true(r, "Success") and str(r["params"].get("InventoryIndex")) == str(i)
            check(f"pot fill: stove lookup finds slot {i} by AssetID {x['asset'][:8]}", ok and x["asset"] != "-1",
                  str(r)[:300])

        # (Not covered: emptying a stacked Item.Pot item. A direct call on FULLWATERPOT is a vanilla
        # no-op, so the row-conversion branch is only reachable through the stove/sink flows.)

        # 8. AreItemsStackable: projectile default LiquidLevel -1 vs inventory 0.
        def stackable(a, b):
            r = bridge("call", FN_LIB, "AreItemsStackable", f"Item1={a}", f"Item2={b}", f"__WorldContext={player}")
            return param_true(r, "ReturnValue"), r
        ok, r = stackable(slot("ammo_bolt", 1, -1), slot("ammo_bolt", 5, 0))
        check("bolt -1 vs 0 stackable", ok, str(r)[:300])
        ok, r = stackable(slot("SoupBowl", 1, 500, "Soup", "a"), slot("SoupBowl", 1, 500, "Soup", "b"))
        check("different soups not stackable", not ok, str(r)[:300])
        ok, r = stackable(slot("waterbottle", 1, 50, "Water"), slot("waterbottle", 1, 100, "Water"))
        check("different water levels not stackable", not ok, str(r)[:300])
        ok, r = stackable(slot("SoupBowl", 1, 500, "Soup", "a"), slot("SoupBowl", 1, 500, "Soup", "a"))
        check("same soup stackable", ok, str(r)[:300])
    finally:
        set_hotbar()

    if failures:
        print(f"{len(failures)} FAILED")
        return 1
    print("ALL PASS")
    return 0


if __name__ == "__main__":
    sys.exit(run_isolated(["StackAnything"], main))
