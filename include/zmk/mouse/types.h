/*
 * Copyright (c) 2020 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * Compatibility shim.
 *
 * `badjeff/kb_zmk_ps2_mouse_trackpoint_driver` includes `zmk/mouse/types.h` and
 * `zmk/mouse/hid.h`. Those headers come from ZMK PR #2027 ("Feature: pointer
 * movement/scrolling"), which was closed without being merged, so they exist in
 * no ZMK release and not in ZMK mainline either.
 *
 * ZMK v0.3.0 keeps the equivalent declarations in `zmk/hid.h`:
 *
 *     zmk_hid_mouse_button_press()   zmk_hid_mouse_button_release()
 *     zmk_hid_mouse_movement_set()   zmk_hid_mouse_scroll_set()
 *     ZMK_MOUSE_HID_NUM_BUTTONS
 *
 * ...which is all the pinned module actually uses, so forwarding is enough.
 *
 * This file lives in the config repo (loaded as a ZMK module via
 * `zephyr/module.yml`, which puts `include/` on the include path) rather than in
 * the module itself, so the module stays unmodified and `west update` keeps
 * working.
 */

#pragma once

#include <zmk/hid.h>
