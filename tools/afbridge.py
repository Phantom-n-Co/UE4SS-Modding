#!/usr/bin/env python3
"""Client for the dev-only AFTestBridge UE4SS mod (tools/AFTestBridge).

Protocol: one request per line, tab-separated UTF-8 args; one JSON reply per line.

  tools/afbridge.py ping
  tools/afbridge.py console "HighResShot 1"
  tools/afbridge.py get player BaseWalkSpeed
  tools/afbridge.py set player BaseWalkSpeed 600
  tools/afbridge.py props player
  tools/afbridge.py find W_MainMenu_Play_C
  tools/afbridge.py call /Script/Engine.Default__KismetMathLibrary Add_IntInt A=2 B=3
  tools/afbridge.py inventory player
  tools/afbridge.py items
  tools/afbridge.py --wait 120 ping        # retry connecting for up to 120 s (game starting)

Targets: player | pc | first:<ClassName> | full object path, each optionally followed by
->Prop steps through object properties (e.g. "first:W_MainMenu_Master_C->MainMenuWidgetSwitcher").
call/set values are UE text format (what `get` prints); FString params take the raw text.
Exit code: 0 = ok, 1 = bridge returned an error, 2 = connection failed.
Importable: `from afbridge import request` (raises BridgeError on connection failure).
"""
import argparse
import json
import socket
import sys
import time

DEFAULT_PORT = 27931  # AFTestBridge::kPort


class BridgeError(Exception):
    pass


def connect(host="127.0.0.1", port=DEFAULT_PORT, wait=0):
    deadline = time.monotonic() + wait
    while True:
        try:
            return socket.create_connection((host, port), timeout=2)
        except OSError as e:
            if time.monotonic() >= deadline:
                raise BridgeError(f"cannot connect to {host}:{port}: {e}") from e
            time.sleep(1)


def send(sock, args, timeout=15):
    if any("\t" in a or "\n" in a for a in args):
        raise ValueError("arguments must not contain tabs or newlines")
    sock.settimeout(timeout)
    try:
        sock.sendall(("\t".join(args) + "\n").encode("utf-8"))
        buf = b""
        while not buf.endswith(b"\n"):
            chunk = sock.recv(1 << 20)
            if not chunk:
                raise BridgeError("connection closed before reply")
            buf += chunk
    except OSError as e:  # includes socket timeouts
        raise BridgeError(f"no reply within {timeout:g}s ({e or 'timeout'})") from e
    return json.loads(buf.decode("utf-8"))


def request(*args, host="127.0.0.1", port=DEFAULT_PORT, wait=0, timeout=15):
    """One command on a fresh connection; returns the decoded reply dict."""
    with connect(host, port, wait) as sock:
        return send(sock, list(args), timeout)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=DEFAULT_PORT)
    p.add_argument("--wait", type=float, default=0, help="seconds to keep retrying the connection")
    p.add_argument("--timeout", type=float, default=15, help="seconds to wait for a reply")
    p.add_argument("command", nargs="+", help="command and its arguments")
    a = p.parse_args()
    try:
        reply = request(*a.command, host=a.host, port=a.port, wait=a.wait, timeout=a.timeout)
    except (BridgeError, OSError, ValueError) as e:
        print(f"afbridge: {e}", file=sys.stderr)
        return 2
    print(json.dumps(reply, indent=2, ensure_ascii=False))
    return 0 if reply.get("ok") else 1


if __name__ == "__main__":
    sys.exit(main())
