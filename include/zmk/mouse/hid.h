/*
 * Copyright (c) 2020 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * Compatibility shim — see `types.h` in this directory for the full
 * explanation. ZMK PR #2027 split the mouse declarations out of `zmk/hid.h`
 * into `zmk/mouse/{types,hid}.h`; that PR was never merged, so on ZMK v0.3.0
 * both paths resolve to the original `zmk/hid.h`.
 */

#pragma once

#include <zmk/hid.h>
