/* ============================================================
 * music.h
 * ELEC327 Simon Game — Tone frequency definitions
 *
 * TIMA1 runs at 8 MHz (BUSCLK 32 MHz / DIV_BY_4).
 * Frequency = 8_000_000 / (LOAD + 1)
 * => LOAD = 8_000_000 / frequency - 1  (rounded)
 *
 * CALC_LOAD adds 0.5 before truncation for proper rounding,
 * and subtracts 1 because the timer counts from 0 to LOAD.
 * ============================================================ */
#ifndef music_include
#define music_include

#include <stdint.h>

#define MCLK_FREQUENCY  8000000.0    /* TIMA1 input clock (Hz), float for math */

/** Compute the TIMA1 LOAD register value for a given frequency in Hz. */
#define CALC_LOAD(freq)  ((uint16_t)((MCLK_FREQUENCY / (freq)) + 0.5) - 1)

/* ---------------------------------------------------------------
 * Simon game button tones  (one per button, ascending pitch)
 * These are also used during sequence playback.
 * --------------------------------------------------------------- */
#define TONE_BTN_0   CALC_LOAD( 783.99)  /* G5  – SW1 Blue    */
#define TONE_BTN_1   CALC_LOAD(1046.50)  /* C6  – SW2 Red     */
#define TONE_BTN_2   CALC_LOAD(1318.51)  /* E6  – SW3 Green   */
#define TONE_BTN_3   CALC_LOAD(1567.98)  /* G6  – SW4 Yellow  */

/* ---------------------------------------------------------------
 * Extra tones for animations
 * --------------------------------------------------------------- */
#define TONE_HIGH    CALC_LOAD(2093.00)  /* C7  – win climax note   */
#define TONE_MID     CALC_LOAD( 987.77)  /* B5  – boot all-flash    */
#define TONE_LOW_A   CALC_LOAD( 220.00)  /* A3  – lose frame 1 (low)  */
#define TONE_LOW_E   CALC_LOAD( 164.81)  /* E3  – lose frame 2 (lower) */

/* Legacy alias retained for any existing code that uses G5_LOAD */
#define G5_LOAD      TONE_BTN_0

#endif /* music_include */

/*
 * Copyright (c) 2026, Caleb Kemere / Rice University ECE
 * All rights reserved, see LICENSE.md
 */