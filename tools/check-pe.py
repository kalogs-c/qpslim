#!/usr/bin/env python3
"""Check native PE artifacts: MZ magic, x86-64 machine, GUI subsystem, DLL exports.

Run from the repo root via `mise run check`. Paths below are relative
to the root.
"""

import struct
import sys

GUI = 2  # IMAGE_SUBSYSTEM_WINDOWS_GUI

FILES = {
    "native/zig-out/bin/dx11-test.exe": GUI,
    "native/zig-out/bin/inject-test.exe": GUI,
    "native/zig-out/bin/limiter.dll": None,
}

EXPORTS = [
    b"Limiter_HookSwapChain",
    b"Limiter_SetTargetFps",
    b"Limiter_GetStats",
    b"Limiter_HookCreation",
    b"Limiter_UnhookCreation",
]


def check_pe(path, want_subsystem):
    data = open(path, "rb").read()
    assert data[:2] == b"MZ", "no MZ magic"

    pe = struct.unpack("<I", data[0x3C:0x40])[0]
    assert data[pe : pe + 4] == b"PE\0\0", "no PE signature"

    machine = struct.unpack("<H", data[pe + 4 : pe + 6])[0]
    assert machine == 0x8664, f"machine {machine:#x} != x64"

    if want_subsystem is not None:
        opt = pe + 24

        magic = struct.unpack("<H", data[opt : opt + 2])[0]
        assert magic in (0x10B, 0x20B), f"optional magic {magic:#x}"

        got = struct.unpack("<H", data[opt + 68 : opt + 70])[0]
        assert got == want_subsystem, f"subsystem {got} != {want_subsystem}"


def main():
    ok = True
    for path, want_sub in FILES.items():
        try:
            check_pe(path, want_sub)
        except (OSError, AssertionError) as e:
            print(f"FAIL {path}: {e}")
            ok = False
            continue
        tag = f", subsys {want_sub}" if want_sub else ", dll"
        print(f"ok {path} (x64{tag})")

    try:
        blob = open("native/zig-out/bin/limiter.dll", "rb").read()
        missing = [e.decode() for e in EXPORTS if e not in blob]
        assert not missing, f"missing exports: {missing}"
        print("ok limiter.dll exports")
    except (OSError, AssertionError) as e:
        print(f"FAIL limiter.dll: {e}")
        ok = False

    print("PE check passed" if ok else "PE check FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
