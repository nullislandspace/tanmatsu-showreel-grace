cmake_minimum_required(VERSION 3.16)

# Use the RISC-V cross-compiler toolchain
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/toolchain-app.cmake")
endif()

project(app C)

# App source files — add your .c files here
set(APP_SOURCES
    main/main.c
    main/reel.c
    main/xform.c
    main/camera.c
    main/horizon.c
    main/backdrop.c
    main/mesh.c
    main/mesh_render.c
    main/common/texcache.c
    main/space/assets/flame.c
    main/space/assets/player_ship.c
    main/common/starfield.c
    main/space/assets/station_mesh.c
    main/space/assets/station.c
    main/space/assets/title_text_mesh.c
    main/space/assets/title_text.c
    main/space/assets/planet_base_mesh.c
    main/space/assets/planet_base.c
    main/space/assets/planet.c
    main/space/assets/asteroid_mesh.c
    main/space/assets/asteroid.c
    main/space/assets/warp.c
    main/space/assets/explosion.c
    main/space/assets/space_dust.c
    main/space/assets/marauder_mesh.c
    main/space/assets/marauder.c
    main/space/assets/laser.c
    main/space/scenes/turntable.c
    main/space/scenes/asset_viewer.c
    main/dev/horizon_test.c
    main/space/scenes/title.c
    main/space/scenes/planet_landing.c
    main/space/scenes/marauder_approach.c
    main/space/scenes/flight.c
    main/space/scenes/pad_strafe.c
    main/space/scenes/emergency_takeoff.c
    main/space/scenes/formation.c
    main/space/scenes/marauder_pursuit.c
    main/space/scenes/spacestation_flyby.c
    main/space/scenes/system2.c
    main/space/scenes/warp_out.c
    main/space/scenes/asteroid_ambush.c
    main/space/scenes/marauder_downfall.c
    main/space/scenes/hero_rolls.c
    main/craftminer/voxel/voxel_world.c
    main/craftminer/voxel/voxel_mesh.c
    main/craftminer/voxel/voxel_render.c
    main/craftminer/voxel/voxel_fx.c
    main/craftminer/voxel/voxel_sky.c
    main/craftminer/assets/miner_mesh.c
    main/craftminer/assets/miner.c
    main/craftminer/scenes/cm_common.c
    main/craftminer/scenes/cm_cabin.c
    main/craftminer/scenes/cm_title.c
    main/craftminer/scenes/cm_overworld.c
    main/craftminer/scenes/cm_walk.c
    main/craftminer/scenes/cm_mining.c
    main/craftminer/scenes/cm_building.c
    main/craftminer/scenes/cm_nightfall.c
    main/craftminer/scenes/cm_world_test.c
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
# The engine's textured-triangle list, raised from its default 1024: the
# block world's textured views need up to ~1300 (claudeplans/craftminer.md,
# F-40, D-51). Before add_subdirectory(synthengine3D), so the engine sees
# it; tools/scenecheck gets the same value from the Makefile.
add_compile_definitions(SE_SCENE_TEXTURED_TRI_CAP=2048)

option(SHOWREEL_EXPORT_MJPEG "Build the MJPEG video export instead of the live reel" OFF)
if(SHOWREEL_EXPORT_MJPEG)
    list(APPEND APP_SOURCES main/export_mjpeg.c)
    add_compile_definitions(SHOWREEL_EXPORT_MJPEG=1)
endif()

# One act only: the scenes of that segment play, and nothing else (see
# reel.c). Empty, the default, plays the whole reel. `make export
# SEGMENT=craftminer` makes a video of a single act.
set(SHOWREEL_SEGMENT "" CACHE STRING "Play only this segment's act (space, craftminer; empty: all)")
if(SHOWREEL_SEGMENT)
    add_compile_definitions(SHOWREEL_SEGMENT="${SHOWREEL_SEGMENT}")
endif()
# Automated device tests (main/testkit/, from the template): on, with
# the reel's own paths. The shot tests need a build id in every record,
# which the app_version.h above provides.
list(APPEND APP_SOURCES
    main/testkit/debugcon.c
    main/testkit/devtest.c
    main/testkit/profile.c
    main/testkit/report.c
    main/testkit/screenshot.c
    main/testkit/showtime.c
)
add_compile_definitions(SCREENSHOT_DIR="/sd/showreel")

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

# SynthEngine3D, the 3D engine (https://github.com/nullislandspace/synthengine3D),
# if this app has it. It is not part of the template: an app that wants 3D adds
# it as a git submodule at synthengine3D/ (`make engine`, see README), and this
# block then builds it. Without it every variable below stays empty and the
# link line is exactly the one a plain app has always had, so apps that do not
# use the engine are unaffected by merges from the template.
#
# In this plain-CMake build the engine is a relocatable OBJECT library, so
# add_subdirectory MUST come before the link script's file(GENERATE) for
# $<TARGET_OBJECTS:synthengine3d> to resolve. Linking it to app_obj propagates
# the engine's PUBLIC include directory (synthengine3D/include), which is what
# lets app sources #include "synthengine3d.h"; its object files are folded into
# app.so by the link script below, since the custom command, not CMake, links.
set(ENGINE_OBJS "")
set(ENGINE_DEPS "")
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/synthengine3D/CMakeLists.txt)
    message(STATUS "SynthEngine3D: building synthengine3D/")
    add_subdirectory(synthengine3D)
    target_link_libraries(app_obj PRIVATE synthengine3d)
    set(ENGINE_OBJS "$<JOIN:$<TARGET_OBJECTS:synthengine3d>, >")
    list(APPEND ENGINE_DEPS synthengine3d $<TARGET_OBJECTS:synthengine3d>)
endif()

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
    $<JOIN:$<TARGET_OBJECTS:app_obj>, > ${ENGINE_OBJS} \
    -lall \
    -o ${CMAKE_BINARY_DIR}/app.so
")

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/app.so
    COMMAND sh ${LINK_SCRIPT}
    DEPENDS app_obj
        $<TARGET_OBJECTS:app_obj>
        ${ENGINE_DEPS}
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
