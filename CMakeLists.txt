cmake_minimum_required(VERSION 3.16)

# Use the RISC-V cross-compiler toolchain
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/toolchain-app.cmake")
endif()

project(app C)

# App source files — add your .c files here
set(APP_SOURCES
    main/main.c
    main/showtime.c
    main/reel.c
    main/xform.c
    main/camera.c
    main/horizon.c
    main/backdrop.c
    main/mesh.c
    main/mesh_render.c
    main/assets/texcache.c
    main/assets/flame.c
    main/assets/player_ship.c
    main/assets/starfield.c
    main/assets/station_mesh.c
    main/assets/station.c
    main/assets/title_text_mesh.c
    main/assets/title_text.c
    main/assets/planet_base_mesh.c
    main/assets/planet_base.c
    main/assets/planet.c
    main/assets/asteroid_mesh.c
    main/assets/asteroid.c
    main/assets/warp.c
    main/assets/explosion.c
    main/assets/space_dust.c
    main/assets/marauder_mesh.c
    main/assets/marauder.c
    main/assets/laser.c
    main/scenes/turntable.c
    main/scenes/asset_viewer.c
    main/scenes/horizon_test.c
    main/scenes/title.c
    main/scenes/planet_landing.c
    main/scenes/marauder_approach.c
    main/scenes/flight.c
    main/scenes/pad_strafe.c
    main/scenes/emergency_takeoff.c
    main/scenes/formation.c
    main/scenes/marauder_pursuit.c
    main/scenes/spacestation_flyby.c
    main/profile.c
    main/screenshot.c
    main/report.c
    main/debugcon.c
    main/devtest.c
    main/crt0.c
)

# Build identity for the debug console's READY records: the git commit
# (marked -dirty with uncommitted changes) and the build time. `make build`
# re-runs cmake, so this is fresh on every build.
execute_process(
    COMMAND git describe --always --dirty --abbrev=10
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE APP_GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT APP_GIT_HASH)
    set(APP_GIT_HASH "unknown")
endif()
string(TIMESTAMP APP_BUILD_TIME "%Y-%m-%dT%H:%M:%SZ" UTC)
configure_file(main/app_version.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/app_version.h @ONLY)

# MJPEG video export (main/export_mjpeg.h): renders the playlist once at a
# fixed 30 fps into /sd/showreel/showreel.avi instead of playing it live.
# Off in normal builds; `make export` builds with it in build-export/.
option(SHOWREEL_EXPORT_MJPEG "Build the MJPEG video export instead of the live reel" OFF)
if(SHOWREEL_EXPORT_MJPEG)
    list(APPEND APP_SOURCES main/export_mjpeg.c)
    add_compile_definitions(SHOWREEL_EXPORT_MJPEG=1)
endif()

# Include directories — add paths to your headers here
set(APP_INCLUDES
    main
    include
    include/esp_additions/include
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

# Compile app sources as a relocatable object library
add_library(app_obj OBJECT ${APP_SOURCES})
target_include_directories(app_obj PRIVATE ${APP_INCLUDES})
target_compile_options(app_obj PRIVATE
    -Wall -Wextra -Wno-unused-parameter
    -Os -fPIC
    -ffunction-sections -fdata-sections -fno-common
    -D__thread=
    -DESP_PLATFORM
)

# ESP-IDF normally force-includes FreeRTOS headers for all components
target_compile_options(app_obj PRIVATE
    "SHELL:-include freertos/FreeRTOS.h"
    "SHELL:-include freertos/semphr.h"
    "SHELL:-include freertos/task.h"
    "SHELL:-include freertos/queue.h"
    "SHELL:-include freertos/event_groups.h"
)

# SynthEngine3D engine component (see synthengine3D/, a git submodule).
# In this plain-CMake build it provides a relocatable OBJECT library;
# add_subdirectory MUST precede the link-script file(GENERATE) below so
# $<TARGET_OBJECTS:synthengine3d> resolves. Linking it to app_obj propagates
# the engine's PUBLIC include dir (synthengine3D/include) so app sources can
# #include "synthengine3d.h"; the engine's object files are folded into app.so
# explicitly in the link script (no double-counting -- the custom command, not
# CMake, performs the link).
add_subdirectory(synthengine3D)
target_link_libraries(app_obj PRIVATE synthengine3d)

# Collect object files into a list file, then link via a response file
# This avoids issues with generator expressions in custom commands
set(LINK_SCRIPT ${CMAKE_BINARY_DIR}/link_app.sh)
file(GENERATE OUTPUT ${LINK_SCRIPT} CONTENT
"#!/bin/sh
${CMAKE_C_COMPILER} \
    -march=rv32imafc_zicsr_zifencei_zaamo_zalrsc_xesploop_xespv2p1 -mabi=ilp32f \
    -shared -nostdlib \
    -Wl,--gc-sections \
    -Wl,-Bsymbolic \
    -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/app_version.script \
    -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/app.ld \
    -Wl,-Map=${CMAKE_BINARY_DIR}/app.map \
    -L${CMAKE_CURRENT_SOURCE_DIR}/fakelib \
    $<JOIN:$<TARGET_OBJECTS:app_obj>, > \
    $<JOIN:$<TARGET_OBJECTS:synthengine3d>, > \
    -lall \
    -o ${CMAKE_BINARY_DIR}/app.so
")

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/app.so
    COMMAND sh ${LINK_SCRIPT}
    DEPENDS app_obj
        $<TARGET_OBJECTS:app_obj>
        synthengine3d
        $<TARGET_OBJECTS:synthengine3d>
        ${CMAKE_CURRENT_SOURCE_DIR}/app.ld
        ${CMAKE_CURRENT_SOURCE_DIR}/app_version.script
        ${CMAKE_CURRENT_SOURCE_DIR}/fakelib/liball.so
    COMMENT "Linking app.so"
)

add_custom_target(app ALL DEPENDS ${CMAKE_BINARY_DIR}/app.so)

# Print size after build
add_custom_command(TARGET app POST_BUILD
    COMMAND ${CMAKE_SIZE} ${CMAKE_BINARY_DIR}/app.so
    COMMENT "app.so size:"
)
