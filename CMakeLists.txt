# Local ZMK module for the AlleinDrach config repository.
#
# ZMK's build action passes this repository as `-DZMK_EXTRA_MODULES=<workspace>`
# whenever the config repo contains `zephyr/module.yml`, so this CMakeLists is
# picked up automatically both in CI and in local builds.
#
# It currently provides:
#   * `include/` — compatibility shim headers for the pinned PS/2 TrackPoint
#     module (see `include/zmk/mouse/`)
#   * `src/rgb_indicators.c` + `src/behaviors/behavior_rgb_indicators.c` — the
#     RGB status-indicator subsystem ported back from the alleindrach zmk-ps2
#     fork (the `zmk,rgb-indicators` driver behind `chosen { zmk,indicators }`)

zephyr_include_directories(include)

target_sources_ifdef(CONFIG_ZMK_RGB_INDICATORS app PRIVATE src/rgb_indicators.c)
target_sources_ifdef(CONFIG_ZMK_RGB_INDICATORS app PRIVATE
                     src/behaviors/behavior_rgb_indicators.c)
