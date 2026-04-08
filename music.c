/* ============================================================
 * music.c
 * ELEC327 Simon Game — Animation data arrays
 *
 * Each animation is a const array of animation_note_t frames.
 * Each frame specifies:
 *   .note      – buzzer_state_t {period, sound_on}
 *   .leds      – pointer to an leds_message_t (from colors.c)
 *   .duration  – how many SIXTEENTH_NOTE (200-tick, 125 ms) units
 *                to display this frame before advancing
 *
 * AdvanceAnimation() in state_machine_logic.c steps through these
 * arrays and wraps back to frame 0 after the last frame, creating
 * a seamless loop.
 *
 * The three animations are deliberately distinct:
 *
 *   boot_animation : rotating LED chase + all-flash.  ~2 s cycle.
 *   win_animation  : diagonal pairs, ascending tones. ~3.6 s cycle.
 *   lose_animation : adjacent pairs, low tones.       ~3.2 s cycle.
 * ============================================================ */
#include "music.h"              /* TONE_* macros                     */
#include "state_machine_logic.h" /* animation_note_t, SIXTEENTH_NOTE */
#include "colors.h"             /* led_single[], leds_all_on, etc.   */

/* ===========================================================
 * boot_animation
 * Rotating LED chase (Blue→Red→Green→Yellow), each lit with its
 * own tone, followed by two full-flash pulses.
 * Duration: 8 frames × 2 × 125 ms = 2.0 s per cycle.
 * =========================================================== */
const animation_note_t boot_animation[] = {
    /* ---- Rotating chase: one LED + its tone ---- */
    { .note = {TONE_BTN_0, true},  .leds = &led_single[0], .duration = 2 }, /* Blue   + G5 */
    { .note = {TONE_BTN_1, true},  .leds = &led_single[1], .duration = 2 }, /* Red    + C6 */
    { .note = {TONE_BTN_2, true},  .leds = &led_single[2], .duration = 2 }, /* Green  + E6 */
    { .note = {TONE_BTN_3, true},  .leds = &led_single[3], .duration = 2 }, /* Yellow + G6 */
    /* ---- All-flash pulse (twice) ---- */
    { .note = {TONE_MID,   true},  .leds = &leds_all_on,   .duration = 2 }, /* All on + B5 */
    { .note = {0,          false}, .leds = &leds_off,       .duration = 2 }, /* Silence     */
    { .note = {TONE_MID,   true},  .leds = &leds_all_on,   .duration = 2 }, /* All on + B5 */
    { .note = {0,          false}, .leds = &leds_off,       .duration = 2 }, /* Silence     */
};
const int boot_animation_length = sizeof(boot_animation) / sizeof(animation_note_t);


/* ===========================================================
 * win_animation
 * Diagonal LED pairs (0+2 and 1+3) alternate with ascending
 * tones, culminating in an all-LED flash at the highest note.
 * Visually: an "X" pattern that twinkles.
 * Duration: (3+3+3+3+4+2) × 125 ms ≈ 3.5 s per cycle.
 *
 * Clearly different from lose_animation:
 *   - Uses DIAGONAL pairs vs adjacent
 *   - Uses ascending (high) tones vs descending (low) tones
 *   - Ends with an all-LED climax
 * =========================================================== */
const animation_note_t win_animation[] = {
    { .note = {TONE_BTN_2, true},  .leds = &leds_pair_02,  .duration = 3 }, /* Diag A + E6   */
    { .note = {TONE_BTN_3, true},  .leds = &leds_pair_13,  .duration = 3 }, /* Diag B + G6   */
    { .note = {TONE_BTN_2, true},  .leds = &leds_pair_02,  .duration = 3 }, /* Diag A + E6   */
    { .note = {TONE_HIGH,  true},  .leds = &leds_pair_13,  .duration = 3 }, /* Diag B + C7   */
    { .note = {TONE_HIGH,  true},  .leds = &leds_all_on,   .duration = 4 }, /* All on + C7   */
    { .note = {0,          false}, .leds = &leds_off,       .duration = 2 }, /* Silence       */
};
const int win_animation_length = sizeof(win_animation) / sizeof(animation_note_t);


/* ===========================================================
 * lose_animation
 * Adjacent LED pairs (0+1 and 2+3) alternate with low
 * descending tones — a somber "game over" feel.
 * Visually: a left/right "swing" pattern with dim colors.
 * Duration: (6+6+4) × 125 ms = 2.0 s per cycle.
 *
 * Clearly different from win_animation:
 *   - Uses ADJACENT pairs vs diagonal
 *   - Uses descending low tones (A3, E3) vs ascending high tones
 *   - No all-LED climax
 * =========================================================== */
const animation_note_t lose_animation[] = {
    { .note = {TONE_LOW_A, true},  .leds = &leds_pair_01,  .duration = 6 }, /* Left  + A3 */
    { .note = {TONE_LOW_E, true},  .leds = &leds_pair_23,  .duration = 6 }, /* Right + E3 */
    { .note = {0,          false}, .leds = &leds_off,       .duration = 4 }, /* Silence    */
};
const int lose_animation_length = sizeof(lose_animation) / sizeof(animation_note_t);