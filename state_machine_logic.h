/* ============================================================
 * state_machine_logic.h
 * ELEC327 Simon Game — FSM types, timing constants, declarations
 *
 * All timing is expressed in "ticks".
 *   Timer0 (TIMG0) reloads at PERIOD ticks of LFCLK (32 kHz).
 *   => one FSM update call every PERIOD/32000 = 0.625 ms.
 *   => TICKS_PER_SEC = 1600 ticks/second.
 * ============================================================ */
#ifndef state_machine_logic_include
#define state_machine_logic_include

#include <stdint.h>
#include <stdbool.h>
#include "leds.h"        /* leds_message_t, apa102_led_t */

/* ---------------------------------------------------------------
 * TIMER / TICK CONFIGURATION
 * --------------------------------------------------------------- */
#define PERIOD              20      /* TIMG0 reload value (LFCLK ticks per ISR) */
#define TICKS_PER_SEC       1600    /* 1600 * 0.625 ms = 1.000 s                */
#define BUTTON_BOUNCE_LIMIT 3       /* Debounce: ticks in BOUNCING before PRESS */
#define SIXTEENTH_NOTE      200     /* 200 ticks = 125 ms (animation time unit) */
#define MAX_COUNTER         1600    /* Retained for legacy compatibility         */
#define FLASH_COUNTER       400     /* Retained for legacy compatibility         */

/* ---------------------------------------------------------------
 * SEQUENCE / GAMEPLAY TIMING  (all in ticks)
 * --------------------------------------------------------------- */
#define ELEM_ON_TICKS       640     /* Sequence LED/tone on: 400 ms             */
#define ELEM_GAP_TICKS      320     /* Silence between elements: 200 ms         */
#define TIMEOUT_TICKS       2400    /* Player input timeout: 1.5 s              */
#define PRE_GAME_TICKS      960     /* Pause before first sequence: 600 ms      */
#define INTER_SEQ_TICKS     1280    /* Pause between sequences: 800 ms          */

/* ---------------------------------------------------------------
 * GAME DIFFICULTY  ← change this ONE line to adjust win condition
 * --------------------------------------------------------------- */
#define WIN_SEQUENCE_LENGTH  5      /* Correct sequence length required to win  */

/* ===============================================================
 * BUTTON TYPES
 * =============================================================== */
typedef enum {
    BUTTON_IDLE = 0,    /* Not pressed (or released)      */
    BUTTON_BOUNCING,    /* Input low, counting debounce   */
    BUTTON_PRESS        /* Confirmed press                */
} button_state_t;

typedef struct {
    button_state_t state;
    uint32_t       depressed_counter;  /* Counts ticks in BOUNCING state */
} button_t;

/* ===============================================================
 * AUDIO TYPES
 * =============================================================== */

/* Instantaneous buzzer state — applied to hardware each tick */
typedef struct {
    uint16_t period;    /* TIMA1 LOAD value; ignored when sound_on = false */
    bool     sound_on;
} buzzer_state_t;

/* A single note with duration (used in music sequences) */
typedef struct {
    buzzer_state_t note;
    uint16_t       duration;  /* Duration in SIXTEENTH_NOTE (200-tick) units */
} music_note_t;

/* A single animation frame: one LED state + one audio state + duration */
typedef struct {
    buzzer_state_t        note;
    const leds_message_t *leds;
    uint16_t              duration;  /* Duration in SIXTEENTH_NOTE units */
} animation_note_t;

typedef enum { PLAYING_NOTE, INTERNOTE } internote_t;  /* Retained for compat */

/* Tracks playback position within an animation array */
typedef struct {
    const animation_note_t *song;        /* Pointer to animation array       */
    int         song_length;             /* Number of frames in the array     */
    int         index;                   /* Current frame index               */
    uint32_t    note_counter;            /* Ticks elapsed in current frame    */
} song_state_t;

/* ===============================================================
 * GAME MODE  (top-level FSM states)
 * =============================================================== */
typedef enum {
    MODE_BOOT_ANIM = 0,   /* Power-on animation; waits for any button press  */
    MODE_PRE_GAME,         /* Brief pause; separates animation from gameplay  */
    MODE_SHOW_SEQUENCE,    /* Device plays the current N-element sequence     */
    MODE_PLAYER_INPUT,     /* Player reproduces the sequence; errors → LOSE   */
    MODE_INTER_SEQUENCE,   /* Brief pause after correct response; next round  */
    MODE_WIN_ANIM,         /* Win animation; loops until button press         */
    MODE_LOSE_ANIM,        /* Lose animation; loops until button press        */
} game_mode_t;

/* ===============================================================
 * TOP-LEVEL STATE  — everything GetNextState() reads and writes
 * =============================================================== */
typedef struct {

    /* ---- Hardware output (consumed by main loop each tick) ---- */
    button_t              buttons[4];
    buzzer_state_t        buzzer;
    const leds_message_t *leds;

    /* ---- FSM control ---- */
    game_mode_t game_mode;
    uint32_t    mode_counter;   /* Ticks since entering current mode/phase   */

    /* ---- Animation playback (boot / win / lose) ---- */
    song_state_t anim_state;

    /* ---- Sequence data ---- */
    uint8_t  sequence[WIN_SEQUENCE_LENGTH]; /* Full pre-generated sequence   */
    int      seq_len;       /* Active round length (grows 1 → WIN_SEQ_LEN)  */
    int      show_idx;      /* Next element to display in MODE_SHOW_SEQUENCE */
    bool     elem_active;   /* true = element LED/tone on; false = gap       */

    /* ---- Player input tracking ---- */
    int      input_idx;       /* Next expected sequence index                */
    int      active_button;   /* Index of button currently held (-1 = none)  */
    uint32_t timeout_counter; /* Ticks since last button release             */

} state_t;

/* ===============================================================
 * PUBLIC FUNCTION DECLARATIONS
 * =============================================================== */

/** Core FSM: compute next state from current state + raw GPIO word. */
state_t GetNextState(state_t current_state, uint32_t gpio_input);

/** Apply buzzer_state_t to the TIMA1 hardware (call before GetNextState). */
void SetBuzzerState(buzzer_state_t buzzer);

/** Fill sequence[0..length-1] with random values in [0, 3]. */
void GenerateSequence(uint8_t *sequence, int length);

/** Reset a song_state_t to the beginning of an animation array. */
void InitAnimation(song_state_t *anim, const animation_note_t *song, int length);

/* ===============================================================
 * ANIMATION DATA  (defined in music.c)
 * =============================================================== */
extern const animation_note_t boot_animation[];
extern const int              boot_animation_length;

extern const animation_note_t win_animation[];
extern const int              win_animation_length;

extern const animation_note_t lose_animation[];
extern const int              lose_animation_length;

#endif /* state_machine_logic_include */