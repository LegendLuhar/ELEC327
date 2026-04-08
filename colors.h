/* ============================================================
 * colors.h
 * ELEC327 Simon Game — LED message declarations
 *
 * Declares every const leds_message_t used by the FSM.
 * Definitions are in colors.c.
 *
 * Button-to-LED color mapping:
 *   Button 0 (SW1) → Blue
 *   Button 1 (SW2) → Red
 *   Button 2 (SW3) → Green
 *   Button 3 (SW4) → Yellow
 * ============================================================ */
#ifndef colors_include
#define colors_include

#include "leds.h"   /* leds_message_t */

/* ---- All LEDs off ---- */
extern const leds_message_t leds_off;

/* ---- All four LEDs on in their button colors ---- */
extern const leds_message_t leds_all_on;

/* ---- Legacy alias for leds_all_on (backward compatibility) ---- */
extern const leds_message_t leds_on;

/* ---- Single-button feedback: led_single[i] lights only LED i ----
 * Used during sequence playback and player input.                   */
extern const leds_message_t led_single[4];

/* ---- Win animation frames: diagonal LED pairs ----
 * leds_pair_02 = LED 0 (Blue)  + LED 2 (Green)
 * leds_pair_13 = LED 1 (Red)   + LED 3 (Yellow)                    */
extern const leds_message_t leds_pair_02;
extern const leds_message_t leds_pair_13;

/* ---- Lose animation frames: adjacent LED pairs ----
 * leds_pair_01 = LED 0 (Blue)  + LED 1 (Red)
 * leds_pair_23 = LED 2 (Green) + LED 3 (Yellow)                    */
extern const leds_message_t leds_pair_01;
extern const leds_message_t leds_pair_23;

#endif /* colors_include */