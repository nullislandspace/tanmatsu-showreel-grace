#pragma once
// =====================================================================
//  Showreel  --  debug-console command listener
// ---------------------------------------------------------------------
//  After tanmatsu-idf6tests' console.c (itself after tanmatsu-fonttest).
//  A task reads newline-terminated commands from the USB-serial/JTAG
//  peripheral through the DRIVER, never through stdin (mixing the two
//  loses bytes); printf/ESP_LOG output keeps its normal console path.
//
//  Commands:
//    PING              answered with a PONG record by the listener
//    RUN <test> [k=v]  queued for the main loop (devtest.h)
//    EXIT              queued: return to the launcher
//    BADGELINK         queued, same as EXIT. The host sends this to ask
//                      the LAUNCHER for BadgeLink mode; if the showreel
//                      is still running, it leaves first, so the host's
//                      next attempt reaches the launcher.
//
//  While idle it emits a READY record every 2 s (app, git build id,
//  engine version, current scene). Unlike the test apps it was modelled
//  on there is no idle timeout: outside a test the showreel is a normal
//  app and keeps running.
// =====================================================================

#include <stdbool.h>

#define DEBUGCON_LINE_MAX 256

// Start the listener task (installs the USB-serial/JTAG driver).
void debugcon_start(void);

// Take the next queued command line, if any, without blocking.
bool debugcon_poll(char out[DEBUGCON_LINE_MAX]);

// The READY banner: on while idle, off while a command is being carried
// out, so a test's records are not interleaved with it.
//
// Queueing a command turns it off by itself, straight away, so nothing
// slips out between the queue and the main loop picking it up -- and a
// command that cannot be queued turns it back on. Whoever runs the
// command owns the flag from then on and clears it when it is back to
// idle (devtest.c does, at the end of a test). A runner that always
// leaves for the launcher never has to.
void debugcon_set_busy(bool busy);

// Emit the identity record (READY / PONG / BEGIN use the same fields).
void debugcon_hello(char const* kind);
