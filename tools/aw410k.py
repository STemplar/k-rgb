#!/usr/bin/env python3
"""
aw410k.py — reference driver / CLI for the Alienware AW410K RGB keyboard on Linux.

This talks directly to the keyboard's vendor HID interface (interface 2,
usage page 0xFF00) via /dev/hidrawN. No external dependencies.

It is the protocol reference for the k-rgb project: every packet layout here is
mirrored from observed/known AW410K behaviour and will be ported to the C++/Qt app.

USAGE
    sudo ./aw410k.py info
    sudo ./aw410k.py solid 0 255 255           # whole keyboard cyan (direct)
    sudo ./aw410k.py static 255 0 0            # whole keyboard red (hardware static)
    sudo ./aw410k.py off
    sudo ./aw410k.py mode spectrum             # hardware rainbow spectrum
    sudo ./aw410k.py mode breathing 0 128 255 --speed normal
    sudo ./aw410k.py wave rainbow --dir left   # rainbow wave
    sudo ./aw410k.py rainbow                    # per-key static rainbow demo
    sudo ./aw410k.py key ESC 255 0 0 --also W 0 255 0   # light specific keys
    ./aw410k.py solid 0 255 255 --dry-run      # print packets, touch nothing

(You only need sudo until the udev rule in packaging/udev/ is installed.)
"""

import argparse
import fcntl
import os
import sys
import time

# --------------------------------------------------------------------------- #
# Device identity                                                             #
# --------------------------------------------------------------------------- #
VID = 0x04F2
PID = 0x1968
LIGHTING_INTERFACE = 2          # vendor HID interface that accepts LED commands
REPORT_LEN = 65                 # 1 report-id byte (0x00) + 64 payload bytes

# Modes (value written to byte 3 of a mode packet) ------------------------- #
MODE = {
    "off":        0x00,
    "direct":     0x01,
    "pulse":      0x02,
    "morph":      0x03,
    "breathing":  0x07,
    "spectrum":   0x08,
    "wave":       0x0F,   # single-colour wave
    "rainbowwave":0x10,
    "scanner":    0x11,
    "static":     0x13,
}

# colour-mode byte (byte 0x0E in mode packets) ----------------------------- #
COLOR_SINGLE = 0x01
COLOR_TWO    = 0x02
COLOR_RAINBOW= 0x03

# speeds (byte 0x0A) — smaller = faster ------------------------------------ #
SPEED = {"slowest": 0x2D, "normal": 0x19, "fastest": 0x0A}

# wave directions (byte 0x0F) ---------------------------------------------- #
DIRECTION = {"right": 0x01, "left": 0x02, "down": 0x03, "up": 0x04}

# --------------------------------------------------------------------------- #
# Key map: (label, hardware index). Physical order, 107 keys.                  #
# Used for per-key addressing and the rainbow demo.                           #
# --------------------------------------------------------------------------- #
KEYMAP = [
    ("ESC",0xB0),("F1",0x98),("F2",0x90),("F3",0x88),("F4",0x80),("F5",0x70),
    ("F6",0x68),("F7",0x60),("F8",0x58),("F9",0x50),("F10",0x48),("F11",0x40),
    ("F12",0x38),("PRTSC",0x30),("SCRLK",0x28),("PAUSE",0x20),("MUTE",0x18),
    ("VOLDN",0x10),("VOLUP",0x08),
    ("`",0xB1),("1",0xA1),("2",0x99),("3",0x91),("4",0x89),("5",0x81),("6",0x79),
    ("7",0x71),("8",0x69),("9",0x61),("0",0x59),("MINUS",0x51),("EQUALS",0x49),
    ("BACKSPACE",0x39),("INS",0x31),("HOME",0x29),("PGUP",0x21),("NUMLK",0x19),
    ("NP/",0x11),("NP*",0x09),("NP-",0x01),
    ("TAB",0xB2),("Q",0xA2),("W",0x9A),("E",0x92),("R",0x8A),("T",0x82),("Y",0x7A),
    ("U",0x72),("I",0x6A),("O",0x62),("P",0x5A),("[",0x52),("]",0x4A),("\\",0x42),
    ("DEL",0x32),("END",0x2A),("PGDN",0x22),("NP7",0x1A),("NP8",0x12),("NP9",0x0A),
    ("NP+",0x03),
    ("CAPS",0xB3),("A",0xA3),("S",0x9B),("D",0x93),("F",0x8B),("G",0x83),("H",0x7B),
    ("J",0x73),("K",0x6B),("L",0x63),(";",0x5B),("'",0x53),("ENTER",0x43),
    ("NP4",0x1B),("NP5",0x13),("NP6",0x0B),
    ("LSHIFT",0xB4),("Z",0xA4),("X",0x9C),("C",0x94),("V",0x8C),("B",0x84),("N",0x7C),
    ("M",0x74),(",",0x6C),(".",0x64),("/",0x5C),("RSHIFT",0x4C),("UP",0x2C),
    ("NP1",0x1C),("NP2",0x14),("NP3",0x0C),("NPENTER",0x05),
    ("LCTRL",0xB5),("LWIN",0xAD),("LALT",0xA5),("SPACE",0x85),("RALT",0x65),
    ("RFUNC",0x5D),("MENU",0x55),("RCTRL",0x4D),("LEFT",0x35),("DOWN",0x2D),
    ("RIGHT",0x25),("NP0",0x1D),("NP.",0x0D),
]
LABEL_TO_IDX = {label.upper(): idx for label, idx in KEYMAP}

# HIDIOCSFEATURE(len) ioctl number: _IOC(_IOC_WRITE|_IOC_READ, 'H', 0x06, len)
def _HIDIOCSFEATURE(size):
    return (3 << 30) | (size << 16) | (ord('H') << 8) | 0x06


# --------------------------------------------------------------------------- #
# Device discovery                                                            #
# --------------------------------------------------------------------------- #
def find_device():
    """Return /dev/hidrawN for the AW410K lighting interface, or None."""
    base = "/sys/class/hidraw"
    if not os.path.isdir(base):
        return None
    for name in sorted(os.listdir(base)):
        devlink = os.path.join(base, name, "device")
        try:
            uevent = open(os.path.join(devlink, "uevent")).read()
        except OSError:
            continue
        # HID_ID line looks like: HID_ID=0003:000004F2:00001968 (bus:vid:pid)
        vid = pid = None
        for line in uevent.splitlines():
            if line.startswith("HID_ID="):
                parts = line.split("=", 1)[1].split(":")
                if len(parts) == 3:
                    try:
                        vid, pid = int(parts[1], 16), int(parts[2], 16)
                    except ValueError:
                        pass
        if vid != VID or pid != PID:
            continue
        # Confirm this is the lighting interface (bInterfaceNumber == 2)
        real = os.path.realpath(devlink)
        ifnum = None
        for parent in (os.path.dirname(real), os.path.dirname(os.path.dirname(real))):
            p = os.path.join(parent, "bInterfaceNumber")
            if os.path.exists(p):
                try:
                    ifnum = int(open(p).read().strip(), 16)
                except ValueError:
                    ifnum = None
                break
        if ifnum is None or ifnum == LIGHTING_INTERFACE:
            return os.path.join("/dev", name)
    return None


# --------------------------------------------------------------------------- #
# Controller                                                                  #
# --------------------------------------------------------------------------- #
class AW410K:
    def __init__(self, path=None, dry_run=False):
        self.dry_run = dry_run
        self.path = path or find_device()
        self.fd = None
        if not self.dry_run:
            if not self.path:
                raise SystemExit("AW410K lighting interface not found. Is the keyboard plugged in?")
            try:
                self.fd = os.open(self.path, os.O_RDWR)
            except PermissionError:
                raise SystemExit(
                    f"Permission denied opening {self.path}.\n"
                    f"Run with sudo, or install packaging/udev/60-alienware-rgb.rules."
                )

    # ---- low-level ------------------------------------------------------- #
    def _buf(self, offsets):
        b = bytearray(REPORT_LEN)            # b[0] = 0x00 report id
        for off, val in offsets.items():
            b[off] = val & 0xFF
        return b

    def _write(self, buf, label=""):
        if self.dry_run:
            print(f"  WRITE  {label:<14} {buf[1:].hex(' ')}")
            return
        os.write(self.fd, bytes(buf))

    def _feature(self, buf, label=""):
        if self.dry_run:
            print(f"  FEAT   {label:<14} {buf[1:].hex(' ')}")
            return
        fcntl.ioctl(self.fd, _HIDIOCSFEATURE(REPORT_LEN), bytes(buf))

    # ---- protocol primitives -------------------------------------------- #
    def commit(self):
        self._write(self._buf({0x01:0x05, 0x02:0x01, 0x0A:0x10, 0x0B:0x0A,
                               0x0C:0x01, 0x0D:0x02, 0x0E:0x01}), "commit")
        time.sleep(0.02)

    def initialize(self):
        self._write(self._buf({0x01:0x0E, 0x02:0x01, 0x03:0x00, 0x04:0x01, 0x05:0xAD,
                               0x06:0x80, 0x07:0x10, 0x08:0xA5, 0x0A:0x0A, 0x12:0x01}),
                    "initialize")
        time.sleep(0.002)

    def feature_report(self, a, b, c, d):
        self._feature(self._buf({0x01:a, 0x02:b, 0x03:c, 0x04:d}), "feature")
        time.sleep(0.01)

    # ---- whole-keyboard solid colour (direct) --------------------------- #
    def set_direct(self, r, g, b):
        self.commit()
        self._write(self._buf({0x01:0x05, 0x02:0x01, 0x03:0x01, 0x04:r, 0x05:g, 0x06:b,
                               0x0B:0x0A, 0x0D:0x01, 0x0E:0x01}), "direct")
        time.sleep(0.002)

    # ---- hardware mode -------------------------------------------------- #
    def send_mode(self, mode, speed, direction, color_mode, r, g, b):
        self._write(self._buf({0x01:0x05, 0x02:0x01, 0x03:mode, 0x04:r, 0x05:g, 0x06:b,
                               0x0A:speed, 0x0B:0x0A, 0x0D:0x01,
                               0x0E:color_mode, 0x0F:direction}), f"mode:{mode:#04x}")

    def set_mode(self, mode, speed=SPEED["normal"], direction=0, color_mode=COLOR_SINGLE,
                 r=0, g=0, b=0):
        self.send_mode(mode, speed, direction, color_mode, r, g, b)
        self.commit()

    def set_morph(self, speed, c1, c2):
        self._write(self._buf({0x01:0x05, 0x02:0x01, 0x03:MODE["morph"],
                               0x04:c1[0], 0x05:c1[1], 0x06:c1[2],
                               0x07:c2[0], 0x08:c2[1], 0x09:c2[2],
                               0x0A:speed, 0x0B:0x0A, 0x0D:0x01, 0x0E:COLOR_TWO}),
                    "morph")
        self.commit()

    def off(self):
        self.set_mode(MODE["off"])

    # ---- per-key static ------------------------------------------------- #
    def set_keys(self, keys):
        """keys: list of (idx, r, g, b). Lights exactly those keys; others off-state."""
        self.initialize()
        self.feature_report(0x05, 0x01, 0x51, 0x00)
        self.commit()

        frame = list(keys)
        self.feature_report(0x0E, len(frame) & 0xFF, 0x00, 0x01)
        while len(frame) % 4:                       # pad to multiple of 4
            frame.append((0x00, 0, 0, 0))

        frame_idx = 0
        for i in range(0, len(frame), 4):
            frame_idx += 1
            b = self._buf({0x01:0x0E, 0x02:0x01, 0x03:0x00, 0x04:frame_idx})
            for slot in range(4):
                idx, r, g, bl = frame[i + slot]
                base = 0x05 + slot * 0x0F          # 4 key-blocks at 0x05/0x14/0x23/0x32
                b[base+0]  = idx & 0xFF
                b[base+1]  = 0x81
                b[base+3]  = 0xA5
                b[base+5]  = 0x0A
                b[base+6]  = r & 0xFF
                b[base+7]  = g & 0xFF
                b[base+8]  = bl & 0xFF
                b[base+13] = 0x01
            self._write(b, f"keys[{i//4}]")

    def close(self):
        if self.fd is not None:
            os.close(self.fd)
            self.fd = None


# --------------------------------------------------------------------------- #
# Helpers                                                                      #
# --------------------------------------------------------------------------- #
def hsv_to_rgb(h, s, v):
    import colorsys
    r, g, b = colorsys.hsv_to_rgb(h, s, v)
    return int(r * 255), int(g * 255), int(b * 255)


# --------------------------------------------------------------------------- #
# CLI                                                                          #
# --------------------------------------------------------------------------- #
def main(argv=None):
    p = argparse.ArgumentParser(description="Alienware AW410K RGB control")
    p.add_argument("--device", help="override /dev/hidrawN")
    p.add_argument("--dry-run", action="store_true", help="print packets, do no I/O")
    sub = p.add_subparsers(dest="cmd", required=True)

    sub.add_parser("info")
    sub.add_parser("list-keys")
    sp = sub.add_parser("solid");  sp.add_argument("rgb", nargs=3, type=int)
    sp = sub.add_parser("static"); sp.add_argument("rgb", nargs=3, type=int)
    sub.add_parser("off")
    sp = sub.add_parser("mode")
    sp.add_argument("name", choices=[m for m in MODE if m not in ("direct",)])
    sp.add_argument("rgb", nargs="*", type=int)
    sp.add_argument("--speed", choices=SPEED, default="normal")
    sp.add_argument("--dir", choices=DIRECTION, default="left", dest="direction")
    sub.add_parser("rainbow")
    sp = sub.add_parser("key")
    sp.add_argument("label"); sp.add_argument("rgb", nargs=3, type=int)
    sp.add_argument("--also", nargs="*", default=[],
                    help="extra LABEL R G B quadruples")

    a = p.parse_args(argv)

    if a.cmd == "info":
        dev = a.device or find_device()
        print(f"AW410K lighting device : {dev or 'NOT FOUND'}")
        print(f"VID:PID                : {VID:04X}:{PID:04X}")
        print(f"Keys                   : {len(KEYMAP)}")
        return 0
    if a.cmd == "list-keys":
        for label, idx in KEYMAP:
            print(f"{label:<10} 0x{idx:02X}")
        return 0

    kb = AW410K(path=a.device, dry_run=a.dry_run)
    try:
        if a.cmd == "solid":
            kb.set_direct(*a.rgb)
        elif a.cmd == "static":
            kb.set_mode(MODE["static"], color_mode=COLOR_SINGLE,
                        r=a.rgb[0], g=a.rgb[1], b=a.rgb[2])
        elif a.cmd == "off":
            kb.off()
        elif a.cmd == "mode":
            speed = SPEED[a.speed]
            direction = DIRECTION[a.direction]
            rgb = (a.rgb + [0, 0, 0])[:3]
            if a.name == "spectrum":
                kb.set_mode(MODE["spectrum"], speed, 0, COLOR_RAINBOW)
            elif a.name == "rainbowwave":
                kb.set_mode(MODE["rainbowwave"], speed, direction, COLOR_RAINBOW)
            elif a.name == "wave":
                kb.set_mode(MODE["wave"], speed, direction, COLOR_SINGLE, *rgb)
            elif a.name == "morph":
                kb.set_morph(speed, rgb, (rgb[2], rgb[0], rgb[1]))
            else:  # pulse, breathing, scanner, static, off
                kb.set_mode(MODE[a.name], speed, 0, COLOR_SINGLE, *rgb)
        elif a.cmd == "rainbow":
            keys = []
            n = len(KEYMAP)
            for i, (_, idx) in enumerate(KEYMAP):
                keys.append((idx, *hsv_to_rgb(i / n, 1.0, 1.0)))
            kb.set_keys(keys)
        elif a.cmd == "key":
            quads = [(a.label, *a.rgb)]
            extra = a.also
            for i in range(0, len(extra) - 3, 4):
                quads.append((extra[i], int(extra[i+1]), int(extra[i+2]), int(extra[i+3])))
            keys = []
            for label, r, g, b in quads:
                idx = LABEL_TO_IDX.get(str(label).upper())
                if idx is None:
                    raise SystemExit(f"unknown key label: {label} (try list-keys)")
                keys.append((idx, int(r), int(g), int(b)))
            kb.set_keys(keys)
    finally:
        kb.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
