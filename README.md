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
| 滚动模式（原 `&mmm`） | ✅ 用主线 `zmk,input-listener` 的按层重映射实现，按住 TP 层的键再推指点杆即可滚动（见下） |
| USB 日志 | ❌ 已关闭（USB 只暴露一个串口，见「注意事项」） |
| TP Set 层（指点杆运行时调参） | ✅ 保留；修正了上游「最高速度加档」被写成减档的问题（见「TP Set 层」） |

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

### 滚动模式（原 `&mmm`）怎么解决的

`&mmm`（鼠标移动/滚动模式切换）是 alleindrach 自己模块 fork 里的
`zmk,behavior-mouse-mode`，mainline 没有。但主线给了条更干净的路：
**`zmk,input-listener` 的「按层覆盖 + input-processor」**。

做法（都在 `config/include/mouse_tp.dtsi`）：

1. 驱动模块自带的 listener（`zmk,input-listener-ps2`）设成 `status = "disabled"` ——
   它的 binding 只有 `device / xy-swap / x-invert / y-invert / scale-* /
   layer-toggle*`，**完全没有滚动能力**；
2. 改用主线的 `zmk,input-listener` 接同一个 `&mouse_ps2`，并用处理器复原原来 listener 的功能：
   - `layer-toggle` → `zip_temp_layer MOUSE_TP 150`（移动时自动上 TP 层，停 150ms 撤下）
   - `y-invert` → `zip_xy_transform (INPUT_TRANSFORM_Y_INVERT)`
3. 再加一个只对 `SCROLL` 层生效的子节点：

   ```dts
   scroll {
       layers = <SCROLL>;
       process-next;                                           // 滚动时 TP 层照旧联动
       input-processors =
           <&zip_xy_to_scroll_mapper>,                         // Y→滚轮, X→横向滚轮
           <&zip_scroll_transform (INPUT_TRANSFORM_Y_INVERT)>, // 方向与指针一致
           <&zip_scroll_scaler 1 8>;                           // 8 格位移 = 1 格滚轮
   };
   ```

键位表里新增 `Scroll_layer`（全是 `&trans`，纯粹当开关用），并把原来 `&mmm` 的两个位置
改成 `&mo SCROLL`：**TP 层第 1 排第 7 键**（`Y` 的位置）和**底排第 1 键**。

> ⚠️ `Scroll_layer` **必须留在 keymap 的最后**。ZMK 的层索引就是节点出现顺序，
> 把它插到中间会把 `MOUSE_TP_SET`（4）挤到 5，而 `U_TOG_TP_SET` 还写着 4 ——
> 结果就是「按 TP Set 键进了滚动层」。这也是为什么它排在 `MouseSettings_layer` 之后。

**用法：按住这两个键之一，再推动指点杆就是滚动。**
TP 层只在指点杆移动时才激活（150ms 超时），所以想不碰指点杆就先按住的话，
可以长按底排的 `&lt 3 TAB` 手动把 TP 层叫出来。

调节：`zip_scroll_scaler 1 8` 的第二个数越大滚得越慢。滚反了就把
`zip_scroll_transform` 那行删掉。

差异：原 `layer-toggle-delay-ms = <150>`（要求先移动 150ms 才激活层）在 `zip_temp_layer`
里没有对应项，现在是「一动就激活」。想避免打字后误触发，可以给它加
`require-prior-idle-ms = <150>;`。

### TP Set 层（指点杆运行时调参）

`MouseSettings_layer`（层 4）用驱动模块的 `&mms`（`zmk,behavior-mouse-setting`）在运行时改
指点杆参数。整层只有 10 个键有功能，其余是 `&none`（不是 `&trans`，所以按住这层时那些键是
死的，不会漏到底层）：

| 位置（BASE 层的对应键） | 绑定 | 作用 |
| --- | --- | --- |
| 第1排第11/12键（`P` / `BSPC`） | `U_MSS_TP_S_D` / `U_MSS_TP_S_I` | 灵敏度 −10 / +10（默认 128） |
| 第2排第1键（`LCTRL`） | `U_MSS_RESET` | 清掉已存进 flash 的设置并回默认 |
| 第2排第11/12键（`;` / `'`） | `U_MSS_TP_V6_D` / `U_MSS_TP_V6_I` | 最高速度 −5 / +5（默认 97） |
| 第3排第1键（`LSHFT`） | `U_MSS_LOG` | 把当前 4 个值打到串口日志 |
| 第3排第11/12键（`/` / `RSHFT`） | `U_MSS_TP_NI_D` / `U_MSS_TP_NI_I` | 负惯性 −1 / +1（默认 6） |
| 底排 `TAB` / `Space` / `Enter` / `←` | `&mkp MCLK` / `LCLK` / `RCLK` / `MCLK` | 鼠标中键 / 左键 / 右键 |
| 底排 `↑` / `→` | `U_MSS_TP_PT_D` / `U_MSS_TP_PT_I` | 按点选阈值 −1 / +1（默认 8） |

改完 **60 秒**（`CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE`）才写 flash，这段时间断电等于白调。
想永久固定就把值写进 `config/include/mouse_tp.dtsi` 的 `&mouse_ps2 { ... }`
（`MS_RESET` 之后生效的正是那里硬编码的值）。

三个遗留问题：

* 上游把「最高速度」的加档键写成了减档（两个都是 `U_MSS_TP_V6_D`），本分支已修成
  `U_MSS_TP_V6_I` —— 生成 DT 里现在是 `&mms 0xf &mms 0xe`（15 = VALUE6_DECR，14 = INCR）。
* `U_TOG_TP_SET` 和 `mo_ctrl_or_tp`（`&mo 0x2` / `&mo 0x4` 的 morph）在这个 keymap 里
  **只定义、没有任何层引用**，所以层 4 目前没有入口 —— 想用就自己接一个键
  （上游原版是挂在 Control 层上），或者在 ZMK Studio 里改。
* `U_MSS_LOG` 走 `LOG_INF`，而日志已经关掉（见「注意事项」），所以它现在按了没反应。

> 读这层**别按源码换行**：config 里这份被重排成 15/15/16，而物理排布是 12/12/12/10。
> 换行是假象，**扁平顺序**才是真的（总数仍是 46，和原版 `boards/shields/alleindrach/
> alleindrach.keymap` 里那份用框线对齐的逐项一致）。

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
* 自动鼠标图层（指点杆一动就激活 `MOUSE_TP` 层）是保留的，但**已改由主线 listener 实现**：
  模块自带的 `zmk,input-listener-ps2` 被 `status = "disabled"`，等价效果来自
  `zip_temp_layer MOUSE_TP 150`（见「滚动模式」一节）。
* TP Set 层（层 4）的调参键位、以及它当前没有入口这件事，见「TP Set 层」一节。
* 固件占用：FLASH 246,876 B / 792 KB（30.4%），RAM 77,298 B / 256 KB（29.5%）；
  `settings_reset` 固件 FLASH 46,188 B。（开着 USB 日志时是 37.5% / 33.6%。）

---

## English

### What this branch does

| Item | Status |
| --- | --- |
| ZMK Studio (edit keymap/layers in a browser) | ✅ enabled, `CONFIG_ZMK_STUDIO_LOCKING=n` |
| TrackPoint (PS/2) | ✅ kept — UART PS/2 driver + automatic layer toggle |
| Physical layout (required by Studio) | ✅ generated from `config/info.json`, 46 keys |
| RGB status indicators (Caps/Num, BLE, layer) | ✅ ported back into this repo |
| Scroll mode (former `&mmm`) | ✅ rebuilt on mainline `zmk,input-listener` per-layer overrides — hold a TP-layer key and move the stick (see below) |
| USB logging | ❌ off (USB exposes a single serial port — see caveats) |

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

### How the scroll mode (former `&mmm`) was solved

`&mmm` (mouse move/scroll mode toggle) is `zmk,behavior-mouse-mode` from alleindrach's
own module fork and does not exist in mainline. Mainline has a cleaner route though:
**`zmk,input-listener`'s per-layer overrides plus `input-processor`s**.

How it works (all in `config/include/mouse_tp.dtsi`):

1. The driver module's own listener (`zmk,input-listener-ps2`) is set to
   `status = "disabled"` — its binding only offers `device / xy-swap / x-invert /
   y-invert / scale-* / layer-toggle*`, i.e. **no scrolling at all**.
2. Mainline's `zmk,input-listener` takes over the same `&mouse_ps2` device and
   processors reproduce what the module listener did:
   - `layer-toggle` -> `zip_temp_layer MOUSE_TP 150` (auto TP layer while moving)
   - `y-invert` -> `zip_xy_transform (INPUT_TRANSFORM_Y_INVERT)`
3. A child node makes the remapping layer-conditional:

   ```dts
   scroll {
       layers = <SCROLL>;
       process-next;                                           // keep the TP layer live
       input-processors =
           <&zip_xy_to_scroll_mapper>,                         // Y -> wheel, X -> h-wheel
           <&zip_scroll_transform (INPUT_TRANSFORM_Y_INVERT)>, // match pointer direction
           <&zip_scroll_scaler 1 8>;                           // 8 units of travel per notch
   };
   ```

The keymap gains a `Scroll_layer` (all `&trans`; it is just a switch) and the two
positions that used `&mmm` now hold `&mo SCROLL`: **7th key of the TP layer's top row**
(the `Y` spot) and **first key of its bottom row**.

> ⚠️ `Scroll_layer` **must stay last** in the keymap. ZMK derives layer indices from
> node order, so inserting it earlier would push `MOUSE_TP_SET` (4) to 5 while
> `U_TOG_TP_SET` still says 4 — i.e. the TP-settings key would land on the scroll layer.
> That is why it sits after `MouseSettings_layer`.

Usage: **hold either of those keys and move the TrackPoint to scroll.** The TP layer is
only up while the stick moves (150ms timeout), so if you would rather not touch the
stick first, hold `&lt 3 TAB` on the base layer to bring the TP layer up manually.

Tuning: the second number in `zip_scroll_scaler 1 8` — bigger is slower. Scrolling the
wrong way? Delete the `zip_scroll_transform` line.

Caveat: the module's `layer-toggle-delay-ms = <150>` (the stick had to move for 150ms
before the layer activated) has no counterpart in `zip_temp_layer`; the layer now turns
on with the first movement event. Add `require-prior-idle-ms = <150>;` if you want to
stop the layer popping up right after typing.

### The TP Set layer (runtime TrackPoint tuning)

`MouseSettings_layer` (layer 4) tunes the TrackPoint at runtime through the driver
module's `&mms` (`zmk,behavior-mouse-setting`). Only 10 keys do anything; the rest
are `&none` — not `&trans` — so while the layer is held those keys are dead and do
**not** fall through to the base layer:

| Position (key it sits on in BASE) | Binding | Effect |
| --- | --- | --- |
| Row 1, keys 11/12 (`P` / `BSPC`) | `U_MSS_TP_S_D` / `U_MSS_TP_S_I` | sensitivity −10 / +10 (default 128) |
| Row 2, key 1 (`LCTRL`) | `U_MSS_RESET` | wipe the stored settings and restore defaults |
| Row 2, keys 11/12 (`;` / `'`) | `U_MSS_TP_V6_D` / `U_MSS_TP_V6_I` | upper plateau speed −5 / +5 (default 97) |
| Row 3, key 1 (`LSHFT`) | `U_MSS_LOG` | dump the four current values to the serial log |
| Row 3, keys 11/12 (`/` / `RSHFT`) | `U_MSS_TP_NI_D` / `U_MSS_TP_NI_I` | negative inertia −1 / +1 (default 6) |
| Bottom: `TAB` / `Space` / `Enter` / `←` | `&mkp MCLK` / `LCLK` / `RCLK` / `MCLK` | mouse middle / left / right button |
| Bottom: `↑` / `→` | `U_MSS_TP_PT_D` / `U_MSS_TP_PT_I` | press-to-select threshold −1 / +1 (default 8) |

Values are written to flash only after **60 s** (`CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE`),
so cutting power inside that window loses the change. To pin values down instead,
put them in `&mouse_ps2 { ... }` in `config/include/mouse_tp.dtsi` — that is what
takes effect after `MS_RESET`.

Three leftovers:

* Upstream wrote the "upper plateau speed" increase key as a decrease (both were
  `U_MSS_TP_V6_D`). This branch fixes it to `U_MSS_TP_V6_I`, so the generated DT now
  reads `&mms 0xf &mms 0xe` (15 = VALUE6_DECR, 14 = INCR).
* `U_TOG_TP_SET` and `mo_ctrl_or_tp` (the `&mo 0x2` / `&mo 0x4` morph) are **defined
  but referenced by no layer** in this keymap, so layer 4 currently has no entry
  point — bind a key to `&mo MOUSE_TP_SET` yourself (upstream had it on the Control
  layer) or change it from ZMK Studio.
* `U_MSS_LOG` goes through `LOG_INF` and USB logging is off (see caveats), so it does
  nothing at the moment.

> Do not read this layer by its source line breaks: the copy in `config/` was
> reflowed into 15/15/16 while the physical rows are 12/12/12/10. The line breaks are
> cosmetic; the **flat order** is what matters (still 46 entries, identical to the
> frame-drawn version in `boards/shields/alleindrach/alleindrach.keymap`).

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
* Automatic mouse layer (TrackPoint movement activates the `MOUSE_TP` layer) is kept,
  but it is now driven by **mainline's listener**: the module's own
  `zmk,input-listener-ps2` is `status = "disabled"` and `zip_temp_layer MOUSE_TP 150`
  does the equivalent job (see the scroll section).
* The TP Set layer (layer 4) — which keys tune what, and the fact that nothing enters
  it right now — is documented in "The TP Set layer" above.
* Firmware size: FLASH 246,876 B / 792 KB (30.4%), RAM 77,298 B / 256 KB (29.5%);
  the `settings_reset` build is FLASH 46,188 B. (With USB logging on it was
  37.5% / 33.6%.)
