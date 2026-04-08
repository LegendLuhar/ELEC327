/* ============================================================
 * colors.c
 * ELEC327 Simon Game — APA102 LED message definitions
 *
 * APA102 protocol (per LED, 32 bits total):
 *   Byte 0: blue intensity
 *   Byte 1: {[7:5]=111b header, [4:0]=brightness}
 *   Byte 2: red intensity
 *   Byte 3: green intensity
 *
 * The struct byte order is already pre-arranged so that casting
 * leds_message_t to uint16_t[] and sending MSb-first over SPI
 * produces a correct APA102 bit stream.  See leds.h for details.
 *
 * Brightness is set to 5 (out of 31) for reasonable coin-cell
 * power consumption.
 * ============================================================ */
#include "colors.h"

/* ---------------------------------------------------------------
 * Helper macros — expand to apa102_led_t designated initializers
 * --------------------------------------------------------------- */
#define LED_OFF    { .blue=0x00, .brightness=0, ._header=7, .red=0x00, .green=0x00 }
#define LED_BLUE   { .blue=0xF0, .brightness=5, ._header=7, .red=0x00, .green=0x00 }
#define LED_RED    { .blue=0x00, .brightness=5, ._header=7, .red=0xF0, .green=0x00 }
#define LED_GREEN  { .blue=0x00, .brightness=5, ._header=7, .red=0x00, .green=0xF0 }
#define LED_YELLOW { .blue=0x00, .brightness=5, ._header=7, .red=0xD0, .green=0xA0 }

/* SPI start/end frames for APA102 */
#define SPI_START  { 0x0000, 0x0000 }
#define SPI_END    { 0xFFFF, 0xFFFF }

/* ===========================================================
 * Global LED messages
 * =========================================================== */

/* All LEDs off */
const leds_message_t leds_off = {
    .start = SPI_START,
    .led   = { LED_OFF, LED_OFF, LED_OFF, LED_OFF },
    .end   = SPI_END,
};

/* All LEDs on in their button colors */
const leds_message_t leds_all_on = {
    .start = SPI_START,
    .led   = { LED_BLUE, LED_RED, LED_GREEN, LED_YELLOW },
    .end   = SPI_END,
};

/* Legacy alias — same content as leds_all_on */
const leds_message_t leds_on = {
    .start = SPI_START,
    .led   = { LED_BLUE, LED_RED, LED_GREEN, LED_YELLOW },
    .end   = SPI_END,
};

/* ----------------------------------------------------------
 * Single-button LED array  (indexed by button / sequence value 0–3)
 * Each entry lights exactly one LED in that button's color.
 * Used for:
 *   - Sequence playback (show which button)
 *   - Player input feedback (LED tracks the physical button)
 * ---------------------------------------------------------- */
const leds_message_t led_single[4] = {
    /* [0] Only LED 0 on — Blue (SW1) */
    { .start = SPI_START,
      .led   = { LED_BLUE, LED_OFF, LED_OFF, LED_OFF },
      .end   = SPI_END },

    /* [1] Only LED 1 on — Red (SW2) */
    { .start = SPI_START,
      .led   = { LED_OFF, LED_RED, LED_OFF, LED_OFF },
      .end   = SPI_END },

    /* [2] Only LED 2 on — Green (SW3) */
    { .start = SPI_START,
      .led   = { LED_OFF, LED_OFF, LED_GREEN, LED_OFF },
      .end   = SPI_END },

    /* [3] Only LED 3 on — Yellow (SW4) */
    { .start = SPI_START,
      .led   = { LED_OFF, LED_OFF, LED_OFF, LED_YELLOW },
      .end   = SPI_END },
};

/* ----------------------------------------------------------
 * Win animation frames: diagonal pairs
 * LEDs 0+2 (Blue+Green) and LEDs 1+3 (Red+Yellow)
 * These create an "X-pattern" when alternating, visually
 * distinct from the adjacent-pair lose animation.
 * ---------------------------------------------------------- */
const leds_message_t leds_pair_02 = {
    .start = SPI_START,
    .led   = { LED_BLUE, LED_OFF, LED_GREEN, LED_OFF },
    .end   = SPI_END,
};

const leds_message_t leds_pair_13 = {
    .start = SPI_START,
    .led   = { LED_OFF, LED_RED, LED_OFF, LED_YELLOW },
    .end   = SPI_END,
};

/* ----------------------------------------------------------
 * Lose animation frames: adjacent pairs
 * LEDs 0+1 (Blue+Red) and LEDs 2+3 (Green+Yellow)
 * Adjacent pairs create a left/right "swing" pattern.
 * ---------------------------------------------------------- */
const leds_message_t leds_pair_01 = {
    .start = SPI_START,
    .led   = { LED_BLUE, LED_RED, LED_OFF, LED_OFF },
    .end   = SPI_END,
};

const leds_message_t leds_pair_23 = {
    .start = SPI_START,
    .led   = { LED_OFF, LED_OFF, LED_GREEN, LED_YELLOW },
    .end   = SPI_END,
};