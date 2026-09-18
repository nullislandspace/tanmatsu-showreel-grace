#pragma once
// =====================================================================
//  Showreel  --  automated device tests, driven over the debug console
// ---------------------------------------------------------------------
//  The host (tools/testrun.py) sends a command; the showreel runs the
//  test inside its normal frame loop, reports CRC-framed records
//  (report.h) and then returns to the launcher by itself, so a whole
//  build -> install -> run -> test cycle needs nobody at the badge.
//
//    RUN perf scene=<name> [secs=<whole seconds>]
//        Play the scene in real time from t = 0 for `secs` seconds (the
//        scene's duration, or 30 s for an endless scene). One PERF
//        record per second (phase split, primitive counts, SRAM), a
//        SHOTPERF record per shot at the end, then END.
//
//    RUN shots scene=<name> ms=<t1>,<t2>,...
//        Render exactly those scene instants, in milliseconds (the show
//        clock is set, not run -- every scene is a pure function of t),
//        and save each to
//        /sd/showreel/test/<scene>_<ms>.png. A SHOT record per image
//        (path + FNV-1a hash of the framebuffer, so a regression check
//        needs no download), then END.
//
//    EXIT / BADGELINK
//        Return to the launcher now.
//
//  Records: BEGIN, PERF, SHOTPERF, SHOT, END {status: ok|bad|error},
//  BYE (just before the restart).
// =====================================================================

#include <stdbool.h>
#include <stdint.h>
#include "pax_gfx.h"

// Start the console listener. `stats_restart` is called when a test
// begins, so the frame statistics start clean with it.
void devtest_start(void (*stats_restart)(void));

// Per frame, in on_update AFTER showtime_frame() and BEFORE
// reel_frame(): takes commands and steers the show clock.
void devtest_update(void);

// Per frame, in on_render after scene_rasterize(): captures shots,
// accumulates perf. `rast_us` is this frame's rasterize time.
void devtest_after_render(pax_buf_t* fb, int64_t rast_us);

// Once per stats period (main.c's log_frame_stats, before prof_flush):
// emits a PERF record while a perf test runs.
void devtest_period(float fps, float frame_ms);
