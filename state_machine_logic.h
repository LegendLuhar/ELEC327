/* ============================================================
 * state_machine_logic.h
 * ELEC327 Simon Game — FSM types, timing constants, declarations
 *
 * All timing is expressed in "ticks".
 *   Timer0 (TIMG0) reloads at PERIOD ticks of LFCLK (32 kHz).
 *   => one FSM update call every PERIOD/32000 = 0.625 ms.
 *   => TICKS_PER_SEC = 1600 ticks/second.
 *
 * Advanced features:
 *   - Difficulty selection (Easy/Normal/Hard/Expert via start button)
 *   - Multi-button simultaneous reset (all 4 buttons)
 *   - Double-button combo elements in sequences (Hard/Expert)
 *   - Per-difficulty best-time tracking with new-record celebration
 *   - Music sequencer alternate mode (hold SW1+SW4 at boot)
 * ============================================================ */
#ifndef state_machine_logic_include
#define state_machine_logic_include

#include <stdint.h>
#include <stdbool.h>
#include "leds.h"

/* ---------------------------------------------------------------
 * TIMER / TICK CONFIGURATION
 * --------------------------------------------------------------- */
#define PERIOD              20
#define TICKS_PER_SEC       1600
#define BUTTON_BOUNCE_LIMIT 3
#define SIXTEENTH_NOTE      200

/* ---------------------------------------------------------------
 * SEQUENCE LIMITS
 *
 * MAX_SEQUENCE_LENGTH sizes the sequence array.  The actual win
 * condition depends on the selected difficulty (see difficulty_config_t).
 * To change the default difficulty's win length, change the table
 * in state_machine_logic.c.
 * --------------------------------------------------------------- */
#define MAX_SEQUENCE_LENGTH  15

/* Legacy alias — normal difficulty win length for the rubric
 * requirement that "difficulty can be changed by changing one line." */
#define WIN_SEQUENCE_LENGTH  5

/* ---------------------------------------------------------------
 * FIXED TIMING CONSTANTS (mode-specific timing that doesn't
 * vary by difficulty)
 * --------------------------------------------------------------- */
#define PRE_GAME_TICKS      960     /* Pause before first sequence: 600 ms      */
#define INTER_SEQ_TICKS     1280    /* Pause between sequences: 800 ms          */
#define DIFF_FLASH_TICKS    640     /* Difficulty indicator display: 400 ms      */
#define RECORD_ANIM_TICKS   2400    /* New-record celebration: 1.5 s             */
#define CHORD_GRACE_TICKS   120     /* Window for second button in a combo: 75ms */
#define SEQ_DETECT_TICKS    160     /* 100ms window to detect sequencer entry     */

/* ===============================================================
 * DIFFICULTY
 * =============================================================== */
typedef enum {
    DIFF_EASY   = 0,   /* SW1 starts easy game   */
    DIFF_NORMAL = 1,   /* SW2 starts normal game  */
    DIFF_HARD   = 2,   /* SW3 starts hard game    */
    DIFF_EXPERT = 3,   /* SW4 starts expert game  */
} difficulty_t;

typedef struct {
    uint16_t timeout_ticks;    /* Player input timeout                     */
    uint16_t elem_on_ticks;    /* Sequence element display duration        */
    uint16_t elem_gap_ticks;   /* Gap between sequence elements            */
    uint8_t  win_length;       /* Correct sequence length required to win  */
    uint8_t  combo_start_round;/* Round at which double-presses begin (0=never) */
} difficulty_config_t;

extern const difficulty_config_t difficulty_table[4];

/* ===============================================================
 * BUTTON TYPES
 * =============================================================== */
typedef enum {
    BUTTON_IDLE = 0,
    BUTTON_BOUNCING,
    BUTTON_PRESS
} button_state_t;

typedef struct {
    button_state_t state;
    uint32_t       depressed_counter;
} button_t;

/* ===============================================================
 * AUDIO TYPES
 * =============================================================== */
typedef struct {
    uint16_t period;
    bool     sound_on;
} buzzer_state_t;

typedef struct {
    buzzer_state_t note;
    uint16_t       duration;
} music_note_t;

typedef struct {
    buzzer_state_t        note;
    const leds_message_t *leds;
    uint16_t              duration;
} animation_note_t;

typedef enum { PLAYING_NOTE, INTERNOTE } internote_t;

typedef struct {
    const animation_note_t *song;
    int         song_length;
    int         index;
    uint32_t    note_counter;
} song_state_t;

/* ===============================================================
 * GAME MODES (top-level FSM states)
 * =============================================================== */
typedef enum {
    MODE_BOOT_ANIM = 0,
    MODE_DIFFICULTY_FLASH, /* Briefly shows selected difficulty color     */
    MODE_PRE_GAME,
    MODE_SHOW_SEQUENCE,
    MODE_PLAYER_INPUT,
    MODE_INTER_SEQUENCE,
    MODE_WIN_ANIM,
    MODE_NEW_RECORD_ANIM,  /* Special celebration for beating best time  */
    MODE_LOSE_ANIM,
    MODE_SEQUENCER,        /* Free-play music sequencer (alternate app)  */
} game_mode_t;

/* ===============================================================
 * TOP-LEVEL STATE — everything GetNextState() reads and writes
 * =============================================================== */
typedef struct {

    /* ---- Hardware output ---- */
    button_t              buttons[4];
    buzzer_state_t        buzzer;
    const leds_message_t *leds;

    /* ---- FSM control ---- */
    game_mode_t game_mode;
    uint32_t    mode_counter;

    /* ---- Difficulty ---- */
    difficulty_t difficulty;

    /* ---- Animation playback ---- */
    song_state_t anim_state;

    /* ---- Sequence data ----
     * Elements are bitmasks: bit i set means button i is part of the element.
     *   Single press:  0x01, 0x02, 0x04, 0x08
     *   Double combo:  0x03, 0x05, 0x06, 0x09, 0x0A, 0x0C                  */
    uint8_t  sequence[MAX_SEQUENCE_LENGTH];
    int      seq_len;
    int      show_idx;
    bool     elem_active;

    /* ---- Player input tracking ---- */
    int      input_idx;
    uint8_t  active_buttons;   /* Bitmask of buttons currently held        */
    bool     chord_locked;     /* true once grace period expires / matched */
    uint32_t chord_timer;      /* Ticks since first button of current chord*/
    uint32_t timeout_counter;

    /* ---- Performance tracking ---- */
    uint32_t game_ticks;       /* Total ticks elapsed during active play   */
    bool     new_record;       /* Set true on win if best_time was beaten  */

} state_t;

/* ===============================================================
 * PUBLIC FUNCTION DECLARATIONS
 * =============================================================== */

state_t GetNextState(state_t current_state, uint32_t gpio_input);
void SetBuzzerState(buzzer_state_t buzzer);
void GenerateSequence(uint8_t *sequence, int length, int combo_start_round, int current_len);
void InitAnimation(song_state_t *anim, const animation_note_t *song, int length);

/* ===============================================================
 * ANIMATION DATA (defined in music.c)
 * =============================================================== */
extern const animation_note_t boot_animation[];
extern const int              boot_animation_length;

extern const animation_note_t win_animation[];
extern const int              win_animation_length;

extern const animation_note_t lose_animation[];
extern const int              lose_animation_length;

extern const animation_note_t record_animation[];
extern const int              record_animation_length;

#endif /* state_machine_logic_include */
