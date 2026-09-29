# zmk-config-AlleinDrach — `studio` 分支 / `studio` branch

给 AlleinDrach（一块内建 PS/2 TrackPoint 的一体式无线键盘）加上 **ZMK Studio**
支持。`main` 分支保持原样，全部改动都在 `studio` 分支。

Adding **ZMK Studio** support to the AlleinDrach keyboard (a unibody wireless
keyboard with a built-in PS/2 TrackPoint). `main` is left untouched; everything
lives on the `studio` branch.

---

## 简体中文

### 这个分支做了什么

| 项目 | 状态 |
| --- | --- |
| ZMK Studio（网页改键位 / 图层） | ✅ 已启用，`CONFIG_ZMK_STUDIO_LOCKING=n`，插 USB 即可连 |
| 指点杆（PS/2 TrackPoint） | ✅ 保留，UART PS/2 驱动 + 自动图层切换 |
| 物理布局（Studio 预览必需） | ✅ 由 `config/info.json` 生成，46 键 |
| RGB 状态指示灯（Caps/Num、BLE、图层） | ✅ 从 fork 移植回本仓库 |
| `&mmm` 鼠标「移动/滚动」模式切换 | ❌ 未移植（见下） |
| USB 日志 | ❌ 已关闭（USB 只暴露一个串口，见「注意事项」） |

### 为什么不是 DYA Studio

DYA Studio 是 cormoran 的那套栈：`cormoran/zmk@main+dya`（**ZMK main / Zephyr 4.1**）
外加十几个 Studio RPC 模块（`zmk-feature-module-physical-layout`、
`zmk-module-settings-rpc`、`zmk-feature-runtime-macro` 等）。它**必须**建在
ZMK main / Zephyr 4.1 上。

本分支按你的选择只上**官方 ZMK Studio**：基座是主线发行版
`zmkfirmware/zmk@v0.3.0`（Zephyr 3.5），依赖少、可控。想升级到完整 DYA 栈的话，
`config/west-dependency.yml` 换成 cormoran 的 manifest 即可，但那是另一件工程。

### 架构与两处关键改动

**1. ZMK 基座与 PS/2 驱动**

`config/west-dependency.yml` 里 pin 了两个东西：

```yaml
- name: zmk
  revision: v0.3.0            # 主线发行版，自带 ZMK Studio + 物理布局
  import: app/west.yml
- name: kb_zmk_ps2_mouse_trackpoint_driver
  remote: badjeff             # 主线化的 PS/2 TrackPoint 驱动
  revision: 05b309d995bf9a7edcdc9cf641d5a2f20ce83fa1
```

这个 PS/2 模块的 `src/mouse/input_listener_ps2.c` 要 include
`zmk/mouse/types.h` 和 `zmk/mouse/hid.h`。这两个头来自
**ZMK PR #2027**，而那个 PR **已关闭、从未合并** —— 所以它在任何 ZMK 发行版和
ZMK mainline 里都不存在（只有 `infused-kim/zmk` 的 PR 分支有，而那条线没有
Studio）。

解法：本仓库自己作为 ZMK module（`zephyr/module.yml`）提供两个**兼容垫片头**：

```
include/zmk/mouse/types.h   ->  #include <zmk/hid.h>
include/zmk/mouse/hid.h     ->  #include <zmk/hid.h>
```

模块实际只用到 `zmk_hid_mouse_button_press/release`、`zmk_hid_mouse_movement_set`、
`zmk_hid_mouse_scroll_set`，这些在 v0.3.0 的 `zmk/hid.h` 里都有，所以转发即可。
好处是**驱动模块本身零改动**，`west update` 之后依然好用。

**2. RGB 状态指示灯移植回本仓库**

fork 里那套 `zmk,rgb-indicators` 在 mainline 不存在，按你的选择移植回来，
放在本仓库的本地模块里：

```
src/rgb_indicators.c                      # 驱动，渲染到 chosen { zmk,indicators }
src/behaviors/behavior_rgb_indicators.c   # &rgb_ind 行为
include/zmk/rgb_indicators.h
include/dt-bindings/zmk/indicator.h
dts/behaviors/rgb_indicators.dtsi
dts/bindings/zmk,rgb-indicators.yaml
dts/bindings/zmk,behavior-rgb-indicators.yaml
```

两处适配：

* `zmk_ble_profile_status(i) == 2`（fork 的枚举，2 = 已连接）→
  `zmk_ble_profile_is_connected(i)`（v0.3.0 的布尔接口）；
* mainline 的 `behaviors.dtsi` 不会自动 include 这个行为（fork 改了自己的
  `behaviors.dtsi`），所以 `config/alleindrach.keymap` 里显式
  `#include <behaviors/rgb_indicators.dtsi>`。

Kconfig 开关仍在 `boards/shields/alleindrach/Kconfig.defconfig` 里（fork 就是这么放的），
已在 `boards/shields/alleindrach/alleindrach.conf` 打开。
`indicator_toggle` / `ht_ind_tog` 节点也已恢复（当前键位表未引用，可自行绑定）。

### +`&mmm` 为什么没移植

`&mmm`（鼠标移动/滚动模式切换）是 alleindrach 自己模块 fork 里的
`zmk,behavior-mouse-mode`，mainline 也没有。按你的选择，这块交给 ZMK 原生的
指点杆/滚动能力，键位表里原来用 `&mmm` 的两个位置现在是 `&trans`。

### 构建

需要 `west`、CMake、Ninja 和 **Zephyr SDK 0.17.0**（Zephyr 3.5 用）。

```sh
cd <这个仓库>
make init         # west init -l config --mf west-standalone.yml
                  # west update --narrow && west zephyr-export
make build        # → build/alleindrach/zephyr/zmk.uf2
make build-reset  # → build/settings_reset/zephyr/zmk.uf2（清空设置用）
make clean        # 删掉 build/
```

依赖会下载到 `./dependencies/`（已在 `.gitignore` 里）。

### 烧录

| 产物 | 用途 |
| --- | --- |
| `zmk.uf2` | 键盘固件，双击复位键进 UF2 引导模式后拖进去 |
| `zmk.uf2`（settings_reset） | 清空已存设置（键位、配对信息），从固件默认值重来 |

### 用 ZMK Studio

1. 用 USB 线连接键盘；
2. 打开 <https://zmk.studio>，选对应的串口；
3. 直接改键位 / 图层，改完即时生效（无需重刷固件）。

物理布局预览来自 `boards/shields/alleindrach/alleindrach-layouts.dtsi`，
由 `config/info.json` 生成，共 46 键。

### 注意事项

* **USB 上只有一个串口。** 本 shield 把 `uart0` 让给了 PS/2 驱动
  （`config/include/mouse_tp.dtsi`），而 stock ZMK 的 nice_nano 根本没有
  `zephyr,console`；`CONFIG_ZMK_USB_LOGGING=y` 会 `select USB_UART_CONSOLE`、
  因此**必须**有个 console 节点，唯一的正规来源是上游 `zmk-usb-logging`
  snippet —— 但它用不了：ZMK 的 build action 每条只接受一个 `snippet:`，那个
  位置被 `studio-rpc-usb-uart` 占了。所以这里把日志关掉了，插上 USB 只会出现
  ZMK Studio 那一个 CDC-ACM 口，在 ZMK Studio 里直接选它。
  要把日志开回来（会多出第二个口，调指点杆时有用）：**同时**取消注释
  `config/alleindrach.conf` 里的 `CONFIG_ZMK_USB_LOGGING=y`，以及
  `boards/shields/alleindrach/boards/nice_nano_v2.overlay` 末尾那段
  “Console / logging transport”。
* `ws2812@1` 的 unit-address 警告是原仓库就有的（`reg = <0>` 与 `@1` 不一致），
  不影响功能，本次未改动。
* 自动鼠标图层（指点杆一动就激活 `MOUSE_TP` 层）是保留的 —— 因为
  `input_listener_ps2.c` 被完整编进固件，`config/include/mouse_tp.dtsi` 里的
  `layer-toggle` / `layer-toggle-delay-ms` / `layer-toggle-timeout-ms` 照旧生效。
* 固件占用（关闭日志后）：FLASH 247,364 B / 792 KB（30.5%），RAM 76,194 B /
  256 KB（29.1%）；`settings_reset` 固件 FLASH 46,188 B。（开日志时是 37.5% / 33.6%。）

---

## English

### What this branch does

| Item | Status |
| --- | --- |
| ZMK Studio (edit keymap/layers in a browser) | ✅ enabled, `CONFIG_ZMK_STUDIO_LOCKING=n` |
| TrackPoint (PS/2) | ✅ kept — UART PS/2 driver + automatic layer toggle |
| Physical layout (required by Studio) | ✅ generated from `config/info.json`, 46 keys |
| RGB status indicators (Caps/Num, BLE, layer) | ✅ ported back into this repo |
| `&mmm` mouse move/scroll mode toggle | ❌ not ported (see below) |
| USB logging | ✅ kept (adds a second serial port — see caveats) |

### Why not DYA Studio

DYA Studio is cormoran's stack: `cormoran/zmk@main+dya` (**ZMK main / Zephyr 4.1**)
plus a dozen Studio RPC modules (`zmk-feature-module-physical-layout`,
`zmk-module-settings-rpc`, `zmk-feature-runtime-macro`, …). It **requires**
ZMK main / Zephyr 4.1.

Per your choice this branch ships **official ZMK Studio only**, on the mainline
release `zmkfirmware/zmk@v0.3.0` (Zephyr 3.5) — fewer moving parts. Moving to the
full DYA stack later is a manifest swap in `config/west-dependency.yml`, but it is
a separate piece of work.

### Architecture, and the two things that needed fixing

**1. ZMK base and the PS/2 driver**

`config/west-dependency.yml` pins two things:

```yaml
- name: zmk
  revision: v0.3.0            # mainline release, has ZMK Studio + physical layouts
  import: app/west.yml
- name: kb_zmk_ps2_mouse_trackpoint_driver
  remote: badjeff             # mainline-oriented PS/2 TrackPoint driver
  revision: 05b309d995bf9a7edcdc9cf641d5a2f20ce83fa1
```

The module's `src/mouse/input_listener_ps2.c` includes `zmk/mouse/types.h` and
`zmk/mouse/hid.h`. Those headers come from **ZMK PR #2027**, which was
**closed and never merged** — so they exist in no ZMK release and not in ZMK
mainline either (only `infused-kim/zmk`'s PR branch carries them, and that line
has no Studio).

Fix: this repo is itself a ZMK module (`zephyr/module.yml`) and supplies two
**shim headers**:

```
include/zmk/mouse/types.h   ->  #include <zmk/hid.h>
include/zmk/mouse/hid.h     ->  #include <zmk/hid.h>
```

The module only actually uses `zmk_hid_mouse_button_press/release`,
`zmk_hid_mouse_movement_set`, `zmk_hid_mouse_scroll_set`, all of which live in
v0.3.0's `zmk/hid.h`, so forwarding is enough. The upside is that the **driver
module stays completely unmodified**, so `west update` keeps working.

**2. RGB status indicators ported back**

The fork's `zmk,rgb-indicators` never existed in mainline. Per your choice it is
ported into this repo's local module:

```
src/rgb_indicators.c                      # driver, renders to chosen { zmk,indicators }
src/behaviors/behavior_rgb_indicators.c   # the &rgb_ind behaviour
include/zmk/rgb_indicators.h
include/dt-bindings/zmk/indicator.h
dts/behaviors/rgb_indicators.dtsi
dts/bindings/zmk,rgb-indicators.yaml
dts/bindings/zmk,behavior-rgb-indicators.yaml
```

Two adaptations were needed:

* `zmk_ble_profile_status(i) == 2` (the fork's enum; 2 = connected) became
  `zmk_ble_profile_is_connected(i)` (v0.3.0's boolean API);
* mainline's `behaviors.dtsi` does not include this behaviour (the fork patched
  its own copy), so `config/alleindrach.keymap` now explicitly does
  `#include <behaviors/rgb_indicators.dtsi>`.

The Kconfig switches stay in `boards/shields/alleindrach/Kconfig.defconfig` (where
the fork put them) and are enabled in
`boards/shields/alleindrach/alleindrach.conf`. The `indicator_toggle` /
`ht_ind_tog` nodes are restored as well (unused by the current keymap — bind them
wherever you like).

### Why `&mmm` was not ported

`&mmm` (mouse move/scroll mode toggle) is `zmk,behavior-mouse-mode` from
alleindrach's own module fork and does not exist in mainline. Per your choice this
is left to ZMK's native pointing/scroll handling; the two keymap positions that
used `&mmm` now use `&trans`.

### Building

You need `west`, CMake, Ninja and **Zephyr SDK 0.17.0** (that is what Zephyr 3.5
uses).

```sh
cd <this repo>
make init         # west init -l config --mf west-standalone.yml
                  # west update --narrow && west zephyr-export
make build        # -> build/alleindrach/zephyr/zmk.uf2
make build-reset  # -> build/settings_reset/zephyr/zmk.uf2 (clears settings)
make clean        # remove build/
```

Dependencies are downloaded into `./dependencies/` (git-ignored).

### Flashing

| Artifact | Purpose |
| --- | --- |
| `zmk.uf2` | keyboard firmware — double-tap reset, drag onto the UF2 drive |
| `zmk.uf2` (settings_reset) | clears stored settings (keymap, pairings), back to firmware defaults |

### Using ZMK Studio

1. Connect the keyboard over USB.
2. Open <https://zmk.studio> and pick the serial port.
3. Edit keys/layers — changes apply live, no reflash needed.

The layout preview comes from
`boards/shields/alleindrach/alleindrach-layouts.dtsi`, generated from
`config/info.json` (46 keys).

### Caveats

* **Only one serial port on USB.** This shield hands `uart0` to the PS/2 driver
  (`config/include/mouse_tp.dtsi`), and stock ZMK's nice_nano declares no
  `zephyr,console` at all; `CONFIG_ZMK_USB_LOGGING=y` does `select USB_UART_CONSOLE`
  and therefore *requires* a console node. The only supported source is upstream's
  `zmk-usb-logging` snippet, which cannot be used here: ZMK's build action accepts
  only one `snippet:` per build entry, and that slot holds `studio-rpc-usb-uart`.
  USB logging is therefore off, and plugging in USB exposes a single CDC-ACM port
  (the ZMK Studio RPC one) — just pick it in ZMK Studio.
  To get logging back (a second port, handy when debugging the trackpoint):
  uncomment `CONFIG_ZMK_USB_LOGGING=y` in `config/alleindrach.conf` *and* the
  "Console / logging transport" block at the end of
  `boards/shields/alleindrach/boards/nice_nano_v2.overlay`.
* The `ws2812@1` unit-address warning is pre-existing (`reg = <0>` vs `@1`); it is
  harmless and was left alone.
* Automatic mouse layer (TrackPoint movement activates the `MOUSE_TP` layer) is
  preserved: `input_listener_ps2.c` is compiled into the firmware in full, so the
  `layer-toggle` / `layer-toggle-delay-ms` / `layer-toggle-timeout-ms` settings in
  `config/include/mouse_tp.dtsi` still apply.
* Firmware size (USB logging off): FLASH 247,364 B / 792 KB (30.5%), RAM
  76,194 B / 256 KB (29.1%); the `settings_reset` build is FLASH 46,188 B.
  (With logging on it was 37.5% / 33.6%.)
