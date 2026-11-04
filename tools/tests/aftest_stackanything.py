#!/usr/bin/env python3
"""Live StackAnything check through AFTestBridge, in the AFTest world with only AFTestBridge +
StackAnything deployed (tools/aftest_mods.py; starts the game if needed).

For every row of every loaded item table (except ItemTable_Pets and Hacker/Companion
equipment, which StackAnything skips), the row matched by the deployed config.lua
(longest pattern first, ties by pattern text, same as config.cpp buildConfig) must
have StackSize >= the configured max. StackAnything only raises, so a larger vanilla
StackSize is left alone and also passes. Prints ALL PASS / exits 1 on any failure.

  tools/tests/aftest_stackanything.py [--config PATH]
"""
import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # tools/: harness modules
from afbridge import BridgeError, request  # noqa: E402
from aftest_mods import run_isolated  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CONFIG = ROOT / "AbioticFactor/AbioticFactor/ue4ss/Mods/StackAnything/config.lua"
SKIPPED_EQUIP_SLOTS = {12, 15}  # EInventorySlotType::EquipmentSlot_Hacker / _Companion (GameTypes.hpp)
LINE_RE = re.compile(r'^\s*\["(.*)"\]\s*=\s*(\d+)\s*,?\s*$')  # config.cpp parseConfigText


def load_config(path):
    entries = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.split("--", 1)[0]
        m = LINE_RE.match(line)
        if not m:
            continue
        try:
            entries.append((m.group(1), re.compile(m.group(1)), int(m.group(2))))
        except re.error as e:
            print(f"warning: pattern {m.group(1)!r} not valid in Python re ({e}); skipped")
    entries.sort(key=lambda e: (-len(e[0]), e[0]))
    return entries


def expected_max(config, row):
    for pattern, regex, size in config:
        if regex.search(row):
            return pattern, size
    return None, None


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--config", type=Path, default=DEFAULT_CONFIG)
    a = p.parse_args()

    config = load_config(a.config)
    if not config:
        print(f"FAIL: no patterns parsed from {a.config}")
        return 1
    try:
        reply = request("items", timeout=60)
    except BridgeError as e:
        print(f"FAIL: {e} (game running with AFTestBridge?)")
        return 1
    if not reply.get("ok"):
        print(f"FAIL: bridge: {reply.get('error')}")
        return 1

    tables = reply["result"]["tables"]
    if not tables:
        print("FAIL: no item tables loaded; load a world first (the main menu does not keep them loaded)")
        return 1
    checked = raised = 0
    failures = []
    for table in tables:
        if "ItemTable_Pets" in table["path"]:
            continue
        for row, stack, equip in table["rows"]:
            if equip in SKIPPED_EQUIP_SLOTS:
                continue
            pattern, want = expected_max(config, row)
            if pattern is None:
                continue
            checked += 1
            if stack < want:
                failures.append(f"{table['path'].rsplit('.', 1)[-1]}:{row} StackSize={stack} < {want} ({pattern!r})")
            elif stack == want:
                raised += 1

    rows_total = sum(len(t["rows"]) for t in tables)
    print(f"{len(config)} patterns, {len(tables)} item tables, {rows_total} rows; "
          f"{checked} matched rows checked ({raised} at exactly the configured max)")
    for f in failures:
        print("FAIL:", f)
    if checked == 0:
        print("FAIL: no row matched any pattern")
        return 1
    if failures:
        print(f"{len(failures)} FAILED")
        return 1
    print("ALL PASS")
    return 0


if __name__ == "__main__":
    sys.exit(run_isolated(["StackAnything"], main))
