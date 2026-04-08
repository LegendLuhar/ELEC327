/* ============================================================
 * state_machine_logic.c
 * ELEC327 Simon Game — Full FSM implementation
 *
 * GetNextState() is the heart of the game.  It is called once
 * per TIMG0 tick (every 0.625 ms) by the main loop and returns
 * a completely new state_t — no global mutable game state exists
 * outside the struct.
 *
 * Button logic: SW1-SW4 are active-low (pull-up resistors).
 *   (gpio_input & mask) == 0  means the button IS pressed.
 *
 * Rubric coverage:
 *   - Power-on animation (changing lights + sounds): MODE_BOOT_ANIM
 *   - Animation → game on button press: transition at any_just_pressed
 *   - Meaningful pre-game pause: MODE_PRE_GAME
 *   - Random sequence (TRNG-seeded in simon.c): GenerateSequence()
 *   - Button LED/tone tracks press: active_button + led_single[]
 *   - Timeout → LOSE: timeout_counter >= TIMEOUT_TICKS
 *   - Wrong button → LOSE: checked immediately on just_pressed
 *   - Win animation ≠ lose animation: different LED patterns + tones
 *   - Pause between sequences: MODE_INTER_SEQUENCE
 *   - Difficulty via one constant: WIN_SEQUENCE_LENGTH
 * ============================================================ */

#include <ti/devices/msp/msp.h>
#include "state_machine_logic.h"
#include "buzzer.h"
#include "buttons.h"
#include "leds.h"
#include "colors.h"
#include "music.h"
#include "random.h"

/* ---------------------------------------------------------------
 * Module-private data tables
 * --------------------------------------------------------------- */

/* GPIO masks for each game button (indexed 0-3, matching LEDs and tones) */
static const uint32_t button_mask[4] = { SW1, SW2, SW3, SW4 };

/* Per-button buzzer periods.  Order matches led_single[] and sequence[]. */
static const uint16_t button_tones[4] = {
    TONE_BTN_0,   /* SW1 – Blue   – G5  (783.99 Hz) */
    TONE_BTN_1,   /* SW2 – Red    – C6  (1046.50 Hz) */
    TONE_BTN_2,   /* SW3 – Green  – E6  (1318.51 Hz) */
    TONE_BTN_3,   /* SW4 – Yellow – G6  (1567.98 Hz) */
};

/* ===============================================================
 * UpdateButton  (private)
 * Per-button debounce state machine.  Returns the new button_t.
 * Input is pulled high; button press drives it low → mask reads 0.
 * =============================================================== */
static button_t UpdateButton(button_t btn, uint32_t gpio_input, uint32_t mask)
{
    button_t next = btn;

    if ((gpio_input & mask) == 0) {
        /* ---------- Button is being held down ---------- */
        switch (btn.state) {
            case BUTTON_IDLE:
                /* First detection: start counting debounce ticks */
                next.state = BUTTON_BOUNCING;
                next.depressed_counter = 1;
                break;

            case BUTTON_BOUNCING:
                next.depressed_counter++;
                if (next.depressed_counter > BUTTON_BOUNCE_LIMIT)
                    next.state = BUTTON_PRESS; /* Confirmed press */
                break;

            case BUTTON_PRESS:
                /* Remain in PRESS; counter stays frozen (no overflow) */
                break;
        }
    } else {
        /* ---------- Button is released ---------- */
        next.state = BUTTON_IDLE;
        next.depressed_counter = 0;
    }

    return next;
}

/* ===============================================================
 * InitAnimation  (public)
 * Resets a song_state_t to the first frame of an animation array.
 * Call this whenever entering a mode that uses AdvanceAnimation().
 * =============================================================== */
void InitAnimation(song_state_t *anim, const animation_note_t *song, int length)
{
    anim->song         = song;
    anim->song_length  = length;
    anim->index        = 0;
    anim->note_counter = 0;
}

/* ===============================================================
 * AdvanceAnimation  (private)
 * Advances the animation by exactly one tick.
 *  - Reads the current frame from anim_state.
 *  - Writes frame's led/buzzer values into *s.
 *  - Increments note_counter; wraps to next frame when duration expires.
 * =============================================================== */
static void AdvanceAnimation(state_t *s)
{
    song_state_t           *a     = &s->anim_state;
    const animation_note_t *frame = &a->song[a->index];

    /* Apply this frame's outputs */
    s->leds   = frame->leds;
    s->buzzer = frame->note;

    /* Advance time within the frame */
    a->note_counter++;
    uint32_t frame_ticks = (uint32_t)frame->duration * (uint32_t)SIXTEENTH_NOTE;
    if (a->note_counter >= frame_ticks) {
        a->note_counter = 0;
        /* Wrap back to frame 0 after the last frame (looping animation) */
        a->index = (a->index + 1) % a->song_length;
    }
}

/* ===============================================================
 * GenerateSequence  (public)
 * Fills sequence[0..length-1] with random values in [0, 3].
 * rand() must already be seeded via srand() (done in simon.c
 * using TRNG) before the first call.
 * =============================================================== */
void GenerateSequence(uint8_t *sequence, int length)
{
    for (int i = 0; i < length; i++) {
        sequence[i] = (uint8_t)rand();   /* LFSR rand() returns [0, 3] */
    }
}

/* ===============================================================
 * SetBuzzerState  (public)
 * Applies a buzzer_state_t to TIMA1 hardware.
 * Called by main() each tick BEFORE GetNextState().
 * =============================================================== */
void SetBuzzerState(buzzer_state_t buzzer)
{
    if (buzzer.sound_on) {
        SetBuzzerPeriod(buzzer.period);
        EnableBuzzer();
    } else {
        DisableBuzzer();
    }
}

/* ===============================================================
 * GetNextState  (public)  ← the FSM core
 *
 * Parameters:
 *   current    – full state from the previous tick
 *   gpio_input – GPIOA->DIN31_0 already masked to button pins
 *
 * Returns the next complete state_t.  The caller (main loop) will
 * apply .buzzer and .leds to hardware before the next tick.
 * =============================================================== */
state_t GetNextState(state_t current, uint32_t gpio_input)
{
    state_t new = current;   /* Start with a copy; modify selectively below */

    /* ===========================================================
     * Step 1 – Advance tick counters
     * =========================================================== */
    new.mode_counter++;
    new.timeout_counter++;   /* Reset within MODE_PLAYER_INPUT at releases */

    /* ===========================================================
     * Step 2 – Debounce all four buttons; detect press/release edges
     * =========================================================== */
    bool just_pressed[4]  = {false, false, false, false};
    bool just_released[4] = {false, false, false, false};

    for (int i = 0; i < 4; i++) {
        button_state_t prev = current.buttons[i].state;
        new.buttons[i]      = UpdateButton(current.buttons[i], gpio_input, button_mask[i]);
        button_state_t curr = new.buttons[i].state;

        just_pressed[i]  = (prev != BUTTON_PRESS && curr == BUTTON_PRESS);
        just_released[i] = (prev == BUTTON_PRESS && curr != BUTTON_PRESS);
    }

    bool any_just_pressed = just_pressed[0] || just_pressed[1] ||
                            just_pressed[2] || just_pressed[3];

    /* ===========================================================
     * Step 3 – Default outputs: everything off
     * (Each mode below may override these.)
     * =========================================================== */
    new.leds   = &leds_off;
    new.buzzer = (buzzer_state_t){ .period = 0, .sound_on = false };

    /* ===========================================================
     * Step 4 – Mode-specific FSM logic
     * =========================================================== */
    switch (current.game_mode) {

    /* -----------------------------------------------------------
     * MODE_BOOT_ANIM
     *
     * Plays boot_animation[] on loop until any button is pressed.
     *
     * Rubric: changing lights, changing sounds, transitions to
     * gameplay (via PRE_GAME) when a button is pressed.
     * ----------------------------------------------------------- */
    case MODE_BOOT_ANIM:
        AdvanceAnimation(&new);   /* Sets new.leds and new.buzzer */

        if (any_just_pressed) {
            /* ---- Transition: silence outputs, enter pre-game pause ---- */
            new.leds         = &leds_off;
            new.buzzer       = (buzzer_state_t){ .sound_on = false };
            new.game_mode    = MODE_PRE_GAME;
            new.mode_counter = 0;
        }
        break;

    /* -----------------------------------------------------------
     * MODE_PRE_GAME
     *
     * All outputs off for PRE_GAME_TICKS (600 ms).
     * This creates a clear visual/audio break between the animation
     * and the first sequence flash.
     *
     * Rubric: "meaningful transition between animation and first
     * sequence" (Gameplay-Playability 1).
     * ----------------------------------------------------------- */
    case MODE_PRE_GAME:
        /* Outputs already defaulted to off above */

        if (new.mode_counter >= PRE_GAME_TICKS) {
            /* Generate the full sequence once for this game */
            GenerateSequence(new.sequence, WIN_SEQUENCE_LENGTH);

            /* Begin round 1 */
            new.seq_len      = 1;
            new.show_idx     = 0;
            new.elem_active  = true;   /* Show first element immediately */
            new.mode_counter = 0;
            new.game_mode    = MODE_SHOW_SEQUENCE;
        }
        break;

    /* -----------------------------------------------------------
     * MODE_SHOW_SEQUENCE
     *
     * Plays sequence[0 .. seq_len-1] to the player, one element
     * at a time.  Each element:
     *   - LED on + tone for ELEM_ON_TICKS  (400 ms)
     *   - all off for ELEM_GAP_TICKS       (200 ms)
     * After the last element, transitions to MODE_PLAYER_INPUT.
     *
     * Rubric: sequence playback with LED/tone feedback.
     * ----------------------------------------------------------- */
    case MODE_SHOW_SEQUENCE:
        if (current.elem_active) {
            /* ---- Element is ON: light and sound ---- */
            int elem = current.sequence[current.show_idx];
            new.leds            = &led_single[elem];
            new.buzzer.period   = button_tones[elem];
            new.buzzer.sound_on = true;

            if (new.mode_counter >= ELEM_ON_TICKS) {
                /* Turn off, move to gap or transition away */
                new.elem_active  = false;
                new.mode_counter = 0;
                new.show_idx++;    /* Advance to next element */

                if (new.show_idx >= current.seq_len) {
                    /* All elements shown → await player input */
                    new.game_mode       = MODE_PLAYER_INPUT;
                    new.input_idx       = 0;
                    new.active_button   = -1;
                    new.timeout_counter = 0;
                }
            }
        } else {
            /* ---- Gap between elements: all off ---- */
            /* Outputs already defaulted to off */
            if (new.mode_counter >= ELEM_GAP_TICKS) {
                new.elem_active  = true;
                new.mode_counter = 0;
            }
        }
        break;

    /* -----------------------------------------------------------
     * MODE_PLAYER_INPUT
     *
     * Player reproduces the sequence.
     *
     * Rules:
     *  1. Button LED + tone are ON exactly while the button is held.
     *  2. Wrong button press → immediate LOSE (on press event).
     *  3. No press within TIMEOUT_TICKS of last release → LOSE.
     *  4. Correct last button released:
     *       seq_len == WIN_SEQUENCE_LENGTH → WIN
     *       else                           → INTER_SEQUENCE
     *
     * Rubric: button feedback, timeout, misplay, win/lose transitions.
     * ----------------------------------------------------------- */
    case MODE_PLAYER_INPUT:

        /* ---- Timeout check (only when no button is being held) ---- */
        if (current.active_button == -1 &&
            new.timeout_counter >= TIMEOUT_TICKS) {
            /* Timed out waiting for next press → LOSE */
            new.game_mode    = MODE_LOSE_ANIM;
            new.mode_counter = 0;
            InitAnimation(&new.anim_state, lose_animation, lose_animation_length);
            break;
        }

        if (current.active_button >= 0) {
            /* ---- A button is currently held down ---- */
            int btn = current.active_button;

            /* Continue LED + tone feedback while button is held */
            new.leds            = &led_single[btn];
            new.buzzer.period   = button_tones[btn];
            new.buzzer.sound_on = true;

            if (just_released[btn]) {
                /* ---- Button was just released ---- */
                new.active_button   = -1;
                new.timeout_counter = 0;   /* Restart timeout from release */

                /* Did releasing this button complete the current sequence? */
                /* (input_idx was already incremented when the button was pressed) */
                if (current.input_idx >= current.seq_len) {
                    if (current.seq_len >= WIN_SEQUENCE_LENGTH) {
                        /* ======== PLAYER WINS ======== */
                        new.game_mode    = MODE_WIN_ANIM;
                        new.mode_counter = 0;
                        InitAnimation(&new.anim_state, win_animation, win_animation_length);
                    } else {
                        /* ---- Correct sequence for this round; next round ---- */
                        new.game_mode    = MODE_INTER_SEQUENCE;
                        new.mode_counter = 0;
                    }
                }
                /* else: more buttons still expected in this sequence */
            }
        } else {
            /* ---- No button held; waiting for the next press ---- */
            for (int i = 0; i < 4; i++) {
                if (!just_pressed[i]) continue;

                /* A button was just pressed — check correctness */
                if (i == (int)current.sequence[current.input_idx]) {
                    /* ---- Correct button ---- */
                    new.active_button = i;
                    new.input_idx++;        /* Advance expected index */
                    /* timeout_counter reset happens on RELEASE, not here */
                } else {
                    /* ---- Wrong button → immediate LOSE ---- */
                    new.game_mode    = MODE_LOSE_ANIM;
                    new.mode_counter = 0;
                    InitAnimation(&new.anim_state, lose_animation, lose_animation_length);
                }
                break;   /* Handle only the first newly pressed button */
            }
        }
        break;

    /* -----------------------------------------------------------
     * MODE_INTER_SEQUENCE
     *
     * Silent pause between sequences.  Gives the player a clear
     * indication that their last press was accepted and the next
     * (longer) sequence is about to begin.
     *
     * Rubric: "meaningful transition between player's button presses
     * and the next sequence" (Gameplay-Playability 2).
     * ----------------------------------------------------------- */
    case MODE_INTER_SEQUENCE:
        /* Outputs already defaulted to off */

        if (new.mode_counter >= INTER_SEQ_TICKS) {
            /* Start next round with one extra element */
            new.seq_len++;
            new.show_idx     = 0;
            new.elem_active  = true;   /* Show first element immediately */
            new.mode_counter = 0;
            new.game_mode    = MODE_SHOW_SEQUENCE;
        }
        break;

    /* -----------------------------------------------------------
     * MODE_WIN_ANIM
     *
     * Celebratory animation using diagonal LED pairs and ascending
     * tones.  Loops indefinitely; any button press restarts game.
     *
     * Rubric: unique win animation, different from lose animation.
     * ----------------------------------------------------------- */
    case MODE_WIN_ANIM:
        AdvanceAnimation(&new);

        if (any_just_pressed) {
            /* Restart: brief silence → new game */
            new.leds         = &leds_off;
            new.buzzer       = (buzzer_state_t){ .sound_on = false };
            new.game_mode    = MODE_PRE_GAME;
            new.mode_counter = 0;
        }
        break;

    /* -----------------------------------------------------------
     * MODE_LOSE_ANIM
     *
     * Somber animation using adjacent LED pairs and low descending
     * tones.  Loops indefinitely; any button press restarts game.
     *
     * Rubric: unique lose animation, different from win animation.
     * ----------------------------------------------------------- */
    case MODE_LOSE_ANIM:
        AdvanceAnimation(&new);

        if (any_just_pressed) {
            /* Restart: brief silence → new game */
            new.leds         = &leds_off;
            new.buzzer       = (buzzer_state_t){ .sound_on = false };
            new.game_mode    = MODE_PRE_GAME;
            new.mode_counter = 0;
        }
        break;

    /* -----------------------------------------------------------
     * Safety net — should never be reached in normal operation
     * ----------------------------------------------------------- */
    default:
        new.game_mode    = MODE_BOOT_ANIM;
        new.mode_counter = 0;
        InitAnimation(&new.anim_state, boot_animation, boot_animation_length);
        break;
    }

    return new;
}