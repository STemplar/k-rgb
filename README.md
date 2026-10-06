# k-rgb

A native **KDE / Qt 6** application (plus a CLI and a Python reference tool) for
controlling keyboard lighting on Linux, including **Alienware AW410K**,
**be quiet! Light Mount**, and **Logitech HID++ RGB** keyboards — no vendor
software, no root daemon, no cloud. Support varies by device; see the table below.

It talks directly to the keyboard's vendor HID interface over `hidraw`, so it's
small, fast, and dependency-light.

> Features include solid colours, a visual per-key editor, named profiles,
> static rainbow, native effects, brightness, a system-tray quick-switcher,
> and restore-on-login. Hardware verification and available effects are listed
> per device below.

## Features

- **Solid colour** across the whole keyboard
- **Per-key custom editor** — a graphical keyboard you paint directly: click,
  box-select, or Ctrl-click keys, pick a colour, and assign it per key; with
  one-click **USA** and **Ukraine** flag presets
- **Named profiles** — save any number of lighting setups and switch between
  them from the window or the tray; the active one is restored at login
- **Auto-detects** your keyboard and presents controls for its lighting protocol;
  the per-key editor adapts to the supported model's layout
- **Per-key static rainbow** (all 107 keys individually addressed on AW410K)
- **Alienware hardware effects** that run on the keyboard itself: Breathing,
  Pulse, Spectrum, Single Wave, Rainbow Wave, Scanner
- **Light Mount hardware effects**: Static, Color Wave, Tornado, Breathing,
  Reactive, and Matrix; plus Per-key Custom and a static Rainbow mode
- **Speed** and **direction** controls (per effect)
- **Brightness** controls with live preview
- **System-tray icon** (`KStatusNotifierItem`) with quick controls — profile
  switcher, Off, Rainbow, Spectrum, colour presets, show/hide, quit
- **Remembers your setup** and can **restore it at login**
- Runs **without root** via a udev rule

## Supported hardware

| Device | USB ID | Interface | Current k-rgb status |
| --- | --- | --- | --- |
| Alienware AW410K RGB Mechanical Keyboard | `04f2:1968` | 2 (vendor HID, `0xFF00`) | full GUI/per-key |
| Alienware AW510K Low-Profile RGB Keyboard | `04f2:1830` | 2 (vendor HID, `0xFF00`) | full GUI/per-key* |
| be quiet! Light Mount (US ANSI) | `373f:0002` | 2 (vendor HID); 3 (HID LampArray control) | **hardware-verified**: all six native effects, GUI/per-key, static rainbow; [details](#be-quiet-light-mount) |
| be quiet! Dark Mount | PID not yet recorded | not yet verified | model identity can be read from USB descriptors; lighting support not yet verified |
| be quiet! Light Mount TKL | PID not yet recorded | not yet verified | model identity can be read from USB descriptors; lighting support not yet verified |
| Other Logitech HID++ 2.0 RGB keyboards | `046d:<PID>`; a known PID is not required | HID++ endpoint discovered at runtime | capability-based support for implemented lighting features; model-specific geometry may be unavailable |
| Logitech G213 Prodigy | `046d:c336` | HID++ endpoint discovered at runtime | **capture-derived**: 5-zone RGB / `0x8070`; hardware-unverified |
| Logitech G410 Atlas Spectrum | `046d:c330` | HID++ endpoint discovered at runtime | **resource-derived** per-key metadata; hardware-unverified |
| Logitech G413 Carbon | `046d:c33a` | HID++ endpoint discovered at runtime | **capture-derived** brightness/breathing behavior; write path not implemented |
| Logitech G512 Carbon | `046d:c342` | HID++ endpoint discovered at runtime | identity/discovery only; no matching packet dump in current evidence set |
| Logitech G513 Carbon | `046d:c33c` | HID++ endpoint discovered at runtime | identity/discovery only; no matching packet dump in current evidence set |
| Logitech G610 Orion | `046d:c333` / `046d:c338` | HID++ endpoint discovered at runtime | resource/reference-derived `0x8080` per-key path†; hardware-unverified |
| Logitech G810 Orion Spectrum | `046d:c331` / `046d:c337` | HID++ endpoint discovered at runtime | `c331` **hardware-verified**; G810 packet captures also available; `c337` resource-derived |
| Logitech G815 LIGHTSYNC | `046d:c33f` | HID++ endpoint discovered at runtime | **capture-derived** effects/per-key/logo/media/G-key protocol; k-rgb write path not implemented |
| Logitech G910 Orion Spark | `046d:c32b` | HID++ endpoint discovered at runtime | **capture-derived** per-key/logo/G-key/M-key protocol; k-rgb write path not implemented |
| Logitech G910 Orion Spectrum | `046d:c335` | HID++ endpoint discovered at runtime | resource/reference-derived; no matching packet dump in current evidence set |
| Logitech G PRO | `046d:c339` | HID++ endpoint discovered at runtime | resource/reference-derived per-key protocol; ANSI87/ISO88 TKL form visually confirmed; hardware-unverified |

k-rgb **auto-detects** which model is plugged in and names it in the window and
`krgb-cli info`. The two Alienware models share the same lighting protocol and
LED index map (the
AW510K simply lacks the discrete volume keys), so all effects and the per-key
editor work on either. Additional Alienware models are added as a one-line entry in the
`kModels` table in `src/core/keymap.h` — see [Adding a keyboard](#adding-a-keyboard).

\* The AW510K mapping is derived from the [OpenRGB](https://openrgb.org/)
Alienware drivers and hasn't yet been confirmed on physical AW510K hardware —
reports welcome. Detection covers only models in the table; an unknown keyboard
can't be assumed compatible because the protocol is reverse-engineered per
model. Contributions for other Alienware devices are welcome.

† G610 support is derived from Logitech Gaming Software resources. G610 and
G810 share the physical key/media/control address scheme; the G610 has white
LEDs, so k-rgb collapses RGB input to a single intensity value. The G610 path
has not yet been verified on physical G610 hardware.

Logitech HID++ discovery is capability-based. k-rgb scans Logitech hidraw
endpoints, resolves Feature Set `0x0001` through ROOT and then enumerates the
device's complete non-root Feature Set. ROOT itself is recorded explicitly as
feature `0x0000` at runtime index 0; `FeatureSet.GetFeatureID()` is only
called for the valid non-root indexes `1..count`. Every reported feature ID,
runtime index and type/flags is retained, including features unknown to k-rgb.
Feature versions are taken from the Feature Set only when that Feature Set
version defines the version byte.

ROOT protocol reporting follows the newer Logitech ROOT semantics:
`getProtocolVersion()` returns a protocol number and, for protocol 3+, a
target-software hint. k-rgb therefore does not format values such as
`protocolNum=4,targetSw=2` as a synthetic `HID++ 4.2` version.

Known lighting features such as `0x8040`, `0x8070`, `0x8071`,
`0x8080`, and `0x8081` are looked up in the runtime feature table rather
than individually probed or assigned by USB PID. Features marked by Logitech as
hidden, engineering, manufacturing-deactivatable, or compliance-deactivatable
remain visible in diagnostics but are not automatically activated by normal
capability discovery. For `0x8080`, k-rgb keeps firmware-reported counts separate from the wider
GetKeyColors candidate address space. For G610/G810 models with a matching LGS
layout resource, diagnostics also intersect those candidates with the physical
geometry and report a `physical` count; candidate-only addresses are retained
and explicitly marked rather than discarded.

Known PIDs are used only for USB identity, udev permissions, optional geometry
metadata, and properties or quirks that HID++ does not report. Unknown Logitech
HID++ 2.0 lighting keyboards can therefore be identified and controlled without
adding their PID to protocol code first, provided the corresponding hidraw
endpoint is accessible and the device exposes lighting features implemented by
k-rgb. The hardware table lists model-specific evidence, not a PID whitelist.
Unsupported feature formats still need implementation, and an unknown model
does not automatically have a matching visual per-key layout.
`krgb-cli logitech info` prints the full enumerated
feature table so captures and hardware reports can be compared without confusing
feature IDs with runtime indexes.

After enumeration, `krgb-cli logitech info` also performs read-only,
feature-specific capability queries where the wire format is sufficiently
documented. Current parsers cover device-reported type/name (`0x0005`),
international keyboard layout (`0x4540`), keyboard-disable capabilities
(`0x4521/0x4522`), reprogrammable-control tables (`0x1b00..0x1b04`),
report rate (`0x8060/0x8061`), brightness v1 (`0x8040`), legacy and modern
RGB effect topology (`0x8070/0x8071`), both per-key lighting families
(`0x8080/0x8081`), and mode status v1 (`0x8090`). Feature presence remains
authoritative even when k-rgb has no detailed parser yet; G-keys, M-keys, macro
recording and onboard profiles are currently reported this way rather than
guessed from the product ID.

The HID++ `0x4540` layout value uses Logitech's own keyboard-layout country
enumeration. Its values overlap enough with USB HID `bCountryCode` to look
similar, but they are not the HID table: for example `0x02` is International,
`0x0D` is Swiss and `0x11` is Belgian. `krgb-cli logitech info` prints both
the Logitech layout name and the derived physical/keycap family (for example
`country=International, family=ISO/QWERTY`), and preserves the raw value for
unknown codes. The layout value is a regional/localization hint, not a complete
drawing. Neither `0x4540` nor the per-key lighting features provide generic
x/y/width/height key geometry, so LGS/resource geometry remains a separate
fallback for the visual keyboard where available.

The Logitech status labels distinguish evidence from implementation. **Hardware-
verified** means tested on physical hardware in k-rgb; **capture-derived** means
packet captures exist for that exact family but k-rgb has not verified the path
on hardware; **resource/reference-derived** means LGS resources and/or an
independent implementation support the mapping; **identity/discovery only**
means k-rgb knows the product but has no matching packet dump in the current
evidence set.

G213 is modeled as a five-zone RGB keyboard: the zone count is read from HID++
`0x8070` at runtime, while its physical five-zone interpretation and 1..5
region-address quirk are corroborated by the G213 packet captures. G413 has
captures for brightness and breathing, but its write path is not implemented in
k-rgb. G815 has captures for effects, all-key/per-key writes, multimedia, G-keys
and logo; G910 Spark has captures for keyboard colour, logo and G/M-key traffic.
Those captures raise their protocol evidence status, but do not by themselves
make the corresponding k-rgb write paths hardware-verified or implemented.

### be quiet! Light Mount

The be quiet! keyboard range includes Dark Mount, Light Mount, and Light Mount
TKL. Only the full-size Light Mount currently has a verified lighting backend
in k-rgb. The Dark Mount and Light Mount TKL PIDs have not yet been recorded in
this project. Their model names can be obtained from USB product descriptors;
identifying a model does not by itself establish its lighting protocol or layout.
Current be quiet! discovery selects `373f:0002` and uses the Light Mount name;
generic product-name discovery for the other models is not implemented yet.

The Light Mount TKL is the tenkeyless variant of the full-size Light Mount.
According to the user's hardware description, it retains the base layout but
omits the numpad block, and its top light bar is shortened at the right-hand
end. A TKL visual layout should reflect those differences. Its topbar LED count,
LED addresses, and lighting protocol have not yet been verified; removing the
numpad from the drawing alone does not establish a compatible write map.

The Light Mount has its own GUI page and lighting backend. Static, Color Wave,
Tornado, Breathing, Reactive, and Matrix have all been confirmed working on
physical Light Mount hardware through user testing. Per-key Custom and the
static Rainbow mode have also been tested.

- **Mode-specific controls:** colour and speed controls are enabled where the
  selected native effect supports them. Color Wave offers single colour, dual
  colour, and gradient/rainbow choices. Reactive exposes two colours.
- **Direction:** Color Wave and Matrix offer Up, Down, Left, and Right; Tornado
  offers Clockwise and Counter-clockwise. Direction is remembered separately
  for each effect within a profile.
- **Speed and brightness:** sliders select 10% steps from 10% to 100% and apply
  and save the chosen value. Colour, colour mode, and speed currently share the
  profile's settings across modes; only direction is stored separately per effect.
- **Per-key editor:** the US-ANSI layout includes 109 keys, the 3D Media Wheel,
  45 topbar LEDs, and five LEDs on each side strip: 165 selectable lighting
  elements. USA Flag and Ukraine Flag presets are available on this page.
- **Physical presentation:** the editor shows the chassis outline, the be quiet!
  logo, and the media wheel. The underside light strips are displayed outside
  the outline for selection. Geometry uses the confirmed chassis/control sizes
  and product images; it is not a manufacturer CAD model.

Native effects use vendor HID interface 2. The GUI releases HID LampArray host
control through interface 3 before applying a native effect. The included udev
rule covers `373f:0002`; follow the permissions instructions below to access the
device without root.

The CLI also exposes Light Mount commands, for example:

```bash
krgb-cli lightmount info
krgb-cli lightmount general static 255 80 0 40
krgb-cli lightmount general breathing 40 50
```

The numbers in these examples are RGB values followed by brightness for Static,
and brightness followed by speed for Breathing. Run `krgb-cli --help` for the
remaining Light Mount commands and parameters.

### Case / chassis lighting (experimental)

k-rgb also drives the **Alienware "AW-ELC" lighting controller** (`187c:0550` /
`187c:0551`) that runs case/chassis zones on many Alienware desktops and
laptops. It's a different (animation-based) protocol from the keyboard; the zone
count is auto-discovered from the controller. Protocol referenced from OpenRGB's
`AlienwareController`.

When a controller is detected, the GUI grows a **Case** tab (alongside
**Keyboard**) with:

- a **whole-case colour** (Apply to All / Off), and
- a **drag-to-arrange zone canvas** over a stylised **Alienware Aurora R12**
  case reference: turn on **Arrange** and drag the small zone markers so the
  on-screen layout mirrors your physical case (saved per machine), then turn it
  off to **select and paint** zones. **Identify** lights only the selected zones
  on the case so you can locate them (zones have no fixed layout); hovering a
  marker enlarges it to show its number.

Whole-case and per-zone choices are saved with each profile and restored at
login; the zone *arrangement* is saved per machine. The CLI exposes the same
lighting via `krgb-cli case`.

## Requirements

- Linux with a recent KDE Plasma / Qt 6 desktop
- Qt 6, KDE Frameworks 6
- A C++17 compiler and CMake ≥ 3.16

### Build dependencies (Debian/Ubuntu)

```bash
sudo apt install build-essential cmake extra-cmake-modules \
  qt6-base-dev qt6-base-dev-tools \
  libkf6coreaddons-dev libkf6i18n-dev libkf6config-dev libkf6configwidgets-dev \
  libkf6widgetsaddons-dev libkf6xmlgui-dev libkf6dbusaddons-dev \
  libkf6statusnotifieritem-dev
```

## Build

```bash
git clone https://github.com/BusyBeaverSoftware/k-rgb.git
cd k-rgb
cmake -S . -B build -DBUILD_GUI=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

This produces:

- `build/bin/krgb` — the KDE GUI
- `build/krgb-cli` — a command-line tool

The core engine and CLI have **no Qt/KF6 dependency** — omit `-DBUILD_GUI=ON`
to build just those.

## Install

```bash
sudo cmake --install build
```

Installs `krgb` and `krgb-cli` to `/usr/local/bin`, the desktop entry, icon,
AppStream metainfo, and the udev rule (to `/lib/udev/rules.d`). Pass
`-DCMAKE_INSTALL_PREFIX=/usr` at configure time to install under `/usr` instead.
After installing, **k-rgb** appears in your application menu; reload udev (below)
once so the keyboard is accessible without root.

## Permissions (run without root)

Install the udev rule so your desktop user can access the keyboard's lighting
interface:

```bash
sudo cp packaging/udev/60-alienware-rgb.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=hidraw
```

(You may need to replug the keyboard once.) Your user must be in the `plugdev`
group. Until this is installed you can still run the tools with `sudo`.

## Usage

### GUI

```bash
./build/bin/krgb
```

Pick a mode/colour and hit **Apply**, or right-click the tray icon for quick
presets. Closing the window hides it to the tray; quit from the tray menu.

Two independent login options live at the bottom of the window:

- **Restore lighting at login** — re-applies the active profile's lighting at
  login (runs `krgb --apply` headlessly and exits; no window, no tray icon).
- **Start k-rgb in the system tray at login** — launches k-rgb hidden to the
  tray (`krgb --tray`) so the quick-switcher is always available. Tick both if
  you want the tray icon *and* your lighting restored.

**Per-key editor:** click the **Per-Key Editor…** button next to the mode
selector (or choose **Per-key (custom)** as the mode) to reveal a graphical
keyboard. Click a key to select it, drag a box to select many, or Ctrl-click to
add/remove. Pick a colour, then **Paint Selected** (or **Fill All**); **Off
Selected** turns keys dark. Changes apply live. The **Presets** row offers
ready-made **USA Flag** and **Ukraine Flag** layouts as a starting point.

**Profiles:** use the **Profile** bar at the top to create, rename, or delete
named setups. Switching profiles (in the window or from the tray's **Profiles**
submenu) applies it immediately; the active profile is the one restored at
login.

### CLI

```bash
krgb-cli solid 0 255 255            # whole keyboard cyan
krgb-cli rainbow                    # per-key static rainbow
krgb-cli spectrum                   # animated hardware rainbow
krgb-cli breathing 0 128 255
krgb-cli key ESC 255 0 0            # light a single key (others off)
krgb-cli perkey W=255,0,0 A=255,0,0 S=255,0,0 D=255,0,0   # set several keys
krgb-cli perkey ESC=#00aaff F1=#00aaff                     # #RRGGBB also works
krgb-cli perkey-file mylayout.txt  # 'KEY R G B' / 'KEY=#RRGGBB' lines (- = stdin)
krgb-cli off
```

`perkey` and `perkey-file` set the listed keys in one frame; any key you don't
list is turned off. Key labels are case-insensitive (see `src/core/keymap.h`);
colours accept `R,G,B`, `R G B`, or `#RRGGBB`. File lines may use `#` comments.

Case / chassis lighting (Alienware AW-ELC controller):

```bash
krgb-cli case info             # detected controller, firmware, zone count
krgb-cli case solid 0 90 255   # all case zones blue
krgb-cli case off
krgb-cli case reset
```

### Python reference tool

`tools/aw410k.py` is a zero-dependency implementation of the full protocol,
useful for experimentation and as living documentation:

```bash
python3 tools/aw410k.py --dry-run solid 0 255 255   # print packets, touch nothing
python3 tools/aw410k.py rainbow
```

## How it works

The AW410K exposes a vendor-defined HID interface (usage page `0xFF00`,
interface 2). Lighting is set by writing 65-byte reports to its `hidraw` node:

- a feature-report "prelude" puts the keyboard into software control,
- solid/per-key frames stream 4 keys per packet,
- hardware effects are selected with a single mode packet.

The packet format was learned from the open-source
[OpenRGB](https://openrgb.org/) project's Alienware drivers (the protocol
facts); this is an original, independent implementation.

## Adding a keyboard

Supported keyboards live in the `kModels` table in `src/core/keymap.h`. To add a
model in the same protocol family:

1. Add a `KeyboardModel` row — `name`, USB `vendorId`/`productId`, lighting
   interface, and a unique model bit (`modelBit(ModelXXX)`).
2. If its key layout differs from the AW410K, tag the affected `kKeyMap` entries
   with a `models` mask so only the right model includes them (e.g. the AW510K
   excludes the discrete volume keys). Identical layouts need no changes.
3. Add the USB id to `packaging/udev/60-alienware-rgb.rules`.

Detection, naming, the per-key editor, and effects then work automatically. If
the model uses a *different* lighting protocol, the packet builders in
`src/core/aw410k_device.cpp` would need a variant — open an issue.

## Project layout

```
src/core/     AW410KDevice — model-driven hidraw protocol engine (no Qt/KF6)
src/cli/      krgb-cli — command-line front-end
src/gui/      krgb — KDE/Qt 6 GUI (KStatusNotifierItem, KConfig, KColorButton)
tools/        aw410k.py — Python reference driver
packaging/    udev rule for rootless access
```

## Roadmap

- Import/export profiles to a file
- More Alienware devices (mice, headsets, chassis AlienFX)

## Credits

- [OpenRGB](https://openrgb.org/) — open-source reverse engineering of the
  AW410K lighting protocol
- [AKBL](https://github.com/rsm-gh/akbl) — Alienware chassis lighting on Linux

## License

[GPL-2.0-or-later](LICENSE). © 2026 Randy Yates / BusyBeaverSoftware.


### G810 ISO105 GUI geometry

For the hardware-verified G810 PID `c331`, the GUI can now use the physical
`ISO105` layout inferred from HID++ `0x4540` and the Logitech
`G810_PIDC331_INTL.xml` resource. The editor exposes the same 116 physical
lighting elements used by normal `0x8080` writes (105 keyboard keys, five
media keys, one logo and five indicator/control lights). Candidate-only
`GetKeyColors` addresses are deliberately excluded from the editor. The
static rainbow and custom per-key modes use this layout directly.
