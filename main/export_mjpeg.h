#pragma once
// =====================================================================
//  Showreel  --  MJPEG video export (compile option)
// ---------------------------------------------------------------------
//  Built only with the CMake option SHOWREEL_EXPORT_MJPEG=ON (`make
//  export`). Instead of playing the reel live, the app renders it at a
//  fixed 30 frames per second of show time -- however long each frame
//  really takes -- plays every scene of the playlist once, JPEG-encodes
//  each finished frame (stb_image_write, in software) and writes them as
//  an MJPEG AVI to EXPORT_PATH, then returns to the launcher.
//
//  The show clock's fixed-step mode (showtime.h) is what makes this
//  work: every scene is a pure function of time, so rendering slower
//  than real time changes nothing in the picture.
// =====================================================================

#include "pax_gfx.h"

#define EXPORT_PATH    "/sd/showreel/showreel.avi"
#define EXPORT_FPS     30
#define EXPORT_QUALITY 85  // JPEG quality, 1..100

// Once, from on_init (after the reel is initialised).
void export_begin(void);

// Per frame, in on_update right AFTER showtime_frame() and BEFORE
// reel_frame(): puts the show clock and the reel at the start on the
// first frame.
void export_update(void);

// Per frame, in on_render after scene_rasterize(): encodes the finished
// frame. When the playlist has played once, finishes the file and
// returns to the launcher (does not return then).
void export_frame(pax_buf_t* fb);
