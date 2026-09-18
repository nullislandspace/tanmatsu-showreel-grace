#pragma once
// =====================================================================
//  Showreel asset  --  starfield
// ---------------------------------------------------------------------
//  A fixed sky of single-pixel stars (scene_point). Each star is a
//  direction, drawn at the camera position plus that direction times a
//  large distance: they turn with the camera but never move with it --
//  infinitely far away, as stars should be. Seeded, so every run (and
//  every scene) sees the same sky.
//
//  Brightness follows a steep power law (a few bright stars, many faint
//  ones), with a mild white / blue / yellow tint spread; ~40% of the
//  stars crowd a band across the sky, a hint of a galactic plane.
// =====================================================================

// Build the star table. Idempotent.
void starfield_init(void);

// Submit the stars for the current camera. Call after the scene's camera
// is set; geometry drawn afterwards still hides them (depth test).
void starfield_submit(void);
