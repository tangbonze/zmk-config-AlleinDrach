# SPDX-License-Identifier: MIT
#
# Local build helpers for the AlleinDrach.
#
#   make init            download ZMK + Zephyr into ./dependencies
#   make build           build the keyboard firmware (ZMK Studio enabled)
#   make build-reset     build the settings_reset firmware
#
# Artifacts land in ./build/<target>/zephyr/zmk.uf2

BOARD  ?= nice_nano_v2
SHELL  := /bin/sh

# `-DZMK_EXTRA_MODULES` points at this repository: its `zephyr/module.yml` makes
# it a ZMK module whose board root is the repository root, which is where
# `boards/shields/alleindrach` lives.
ZMK_ARGS = -DZMK_CONFIG=$(CURDIR)/config \
           -DZMK_EXTRA_MODULES=$(CURDIR)

.PHONY: init build build-reset clean

init:
	west init -l config --mf west-standalone.yml
	west update --narrow
	west zephyr-export

build:
	west build -s dependencies/zmk/app -d build/alleindrach -b $(BOARD) \
	  -S studio-rpc-usb-uart -- $(ZMK_ARGS) -DSHIELD=alleindrach

build-reset:
	west build -s dependencies/zmk/app -d build/settings_reset -b $(BOARD) \
	  -- $(ZMK_ARGS) -DSHIELD=settings_reset

clean:
	rm -rf build
