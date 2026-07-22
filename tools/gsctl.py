#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 ARMSX2 Contributors
# SPDX-License-Identifier: GPL-3.0+
"""Query and control a running ARMSX2 instance over its PINE socket.

Built for GS performance work: read the statistics that normally only appear on
the OSD, and toggle settings without restarting the emulator. Most GS settings
apply in place, so a whole settings sweep can run against one booted instance
sitting on a savestate.

Requires `EmuCore/EnablePINE = true` in the INI (or the Big Picture UI toggle).

Examples:
    gsctl.py stats
    gsctl.py stats --watch 1.0
    gsctl.py get EmuCore/GS accurate_blending_unit
    gsctl.py set EmuCore/GS accurate_blending_unit 3
    gsctl.py loadstate 2
    gsctl.py frameadvance

Pure stdlib; no build step. Output is JSON on stdout so it composes with jq.
"""

import argparse
import json
import os
import socket
import struct
import sys
import time

DEFAULT_SLOT = 28011

# Opcodes. 0x00-0x0F are upstream PINE; 0x10+ are ARMSX2-local extensions.
MSG_SAVE_STATE = 0x09
MSG_LOAD_STATE = 0x0A
MSG_TITLE = 0x0B
MSG_STATUS = 0x0F
MSG_GET_STATS = 0x10
MSG_GET_SETTING = 0x11
MSG_SET_SETTING = 0x12
MSG_FRAME_ADVANCE = 0x13

IPC_OK = 0
STATUS_NAMES = {0: "running", 1: "paused", 2: "shutdown"}


class PineError(Exception):
    pass


def socket_path(slot):
    """Mirrors PINEServer::Initialize. Note the emulator name is still 'pcsx2'."""
    if sys.platform == "darwin":
        base = os.environ.get("TMPDIR", "/tmp")
    else:
        base = os.environ.get("XDG_RUNTIME_DIR", "/tmp")
    name = "pcsx2.sock" if slot == DEFAULT_SLOT else "pcsx2.sock.%d" % slot
    return os.path.join(base, name)


def lp_string(s):
    """Length-prefixed string argument: [u32 len][bytes], no NUL."""
    raw = s.encode("utf-8")
    return struct.pack("<I", len(raw)) + raw


class Pine:
    def __init__(self, slot=DEFAULT_SLOT, timeout=10.0):
        self.slot = slot
        if sys.platform == "win32":
            self.sock = socket.create_connection(("127.0.0.1", slot), timeout=timeout)
        else:
            path = socket_path(slot)
            if not os.path.exists(path):
                raise PineError(
                    "no PINE socket at %s -- is the emulator running with "
                    "EmuCore/EnablePINE=true?" % path
                )
            self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            self.sock.settimeout(timeout)
            self.sock.connect(path)

    def close(self):
        self.sock.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def _recv_exactly(self, n):
        buf = b""
        while len(buf) < n:
            chunk = self.sock.recv(n - len(buf))
            if not chunk:
                raise PineError("connection closed by emulator")
            buf += chunk
        return buf

    def request(self, opcode, payload=b""):
        """Send one command, return its reply payload (after the result byte)."""
        body = struct.pack("<B", opcode) + payload
        packet = struct.pack("<I", len(body) + 4) + body
        self.sock.sendall(packet)

        (reply_len,) = struct.unpack("<I", self._recv_exactly(4))
        if reply_len < 5:
            raise PineError("malformed reply length %d" % reply_len)
        rest = self._recv_exactly(reply_len - 4)
        if rest[0] != IPC_OK:
            raise PineError(
                "emulator rejected opcode 0x%02X (no VM running, or "
                "unsupported by this build)" % opcode
            )
        return rest[1:]

    @staticmethod
    def _read_string(payload):
        (size,) = struct.unpack("<I", payload[:4])
        # size includes the trailing NUL.
        return payload[4 : 4 + size - 1].decode("utf-8", "replace")

    def stats(self):
        return json.loads(self._read_string(self.request(MSG_GET_STATS)))

    def title(self):
        return self._read_string(self.request(MSG_TITLE))

    def status(self):
        (raw,) = struct.unpack("<I", self.request(MSG_STATUS)[:4])
        return STATUS_NAMES.get(raw, "unknown(%d)" % raw)

    def get_setting(self, section, key):
        return self._read_string(
            self.request(MSG_GET_SETTING, lp_string(section) + lp_string(key))
        )

    def set_setting(self, section, key, value):
        payload = lp_string(section) + lp_string(key) + lp_string(str(value))
        return json.loads(self._read_string(self.request(MSG_SET_SETTING, payload)))

    def load_state(self, slot):
        self.request(MSG_LOAD_STATE, struct.pack("<B", slot))

    def save_state(self, slot):
        self.request(MSG_SAVE_STATE, struct.pack("<B", slot))

    def frame_advance(self):
        self.request(MSG_FRAME_ADVANCE)


def split_section_key(arg):
    """'EmuCore/GS/Key' or a separate section and key. Sections contain slashes."""
    if "/" not in arg:
        raise PineError("expected <Section>/<Key>, got '%s'" % arg)
    section, _, key = arg.rpartition("/")
    return section, key


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--slot", type=int, default=DEFAULT_SLOT, help="PINE slot (default %d)" % DEFAULT_SLOT)
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("stats", help="dump performance/GS statistics as JSON")
    p.add_argument("--watch", type=float, metavar="SECONDS",
                   help="poll forever at this interval, one JSON object per line")

    sub.add_parser("status", help="running / paused / shutdown")
    sub.add_parser("title", help="current game title")
    sub.add_parser("frameadvance", help="advance a paused VM by one frame")

    p = sub.add_parser("get", help="read a setting")
    p.add_argument("section")
    p.add_argument("key", nargs="?")

    p = sub.add_parser("set", help="write a setting and apply it")
    p.add_argument("section")
    p.add_argument("key")
    p.add_argument("value", nargs="?")

    p = sub.add_parser("loadstate", help="load a savestate slot")
    p.add_argument("slot", type=int)

    p = sub.add_parser("savestate", help="save to a savestate slot")
    p.add_argument("slot", type=int)

    args = ap.parse_args()

    try:
        with Pine(args.slot) as pine:
            if args.cmd == "stats":
                if args.watch:
                    while True:
                        print(json.dumps(pine.stats()), flush=True)
                        time.sleep(args.watch)
                else:
                    print(json.dumps(pine.stats(), indent=2))
            elif args.cmd == "status":
                print(pine.status())
            elif args.cmd == "title":
                print(pine.title())
            elif args.cmd == "frameadvance":
                pine.frame_advance()
            elif args.cmd == "get":
                # Accept both 'get EmuCore/GS Key' and 'get EmuCore/GS/Key'.
                section, key = (args.section, args.key) if args.key else split_section_key(args.section)
                print(pine.get_setting(section, key))
            elif args.cmd == "set":
                if args.value is None:
                    section, key = split_section_key(args.section)
                    value = args.key
                else:
                    section, key, value = args.section, args.key, args.value
                result = pine.set_setting(section, key, value)
                print(json.dumps(result))
                if result.get("restart_required"):
                    print("note: this key forces a GS device reopen", file=sys.stderr)
            elif args.cmd == "loadstate":
                pine.load_state(args.slot)
            elif args.cmd == "savestate":
                pine.save_state(args.slot)
    except KeyboardInterrupt:
        pass
    except (PineError, OSError) as e:
        print("gsctl: %s" % e, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
