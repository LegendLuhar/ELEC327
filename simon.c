/* ============================================================
 * simon.c
 * ELEC327 Simon Game — Entry point
 *
 * Responsibilities:
 *   1. Initialize all hardware peripherals
 *   2. Seed the LFSR RNG from the True Random Number Generator
 *      so each power cycle produces a different sequence
 *   3. Initialize the FSM state (MODE_BOOT_ANIM)
 *   4. Run the main timer-driven loop:
 *        a. Apply buzzer state to hardware
 *        b. Send LED state over SPI
 *        c. Sample GPIO buttons
 *        d. Advance FSM
 *        e. Sleep until next tick
 *
 * Copyright (c) 2026, Caleb Kemere / Rice University ECE
 * All rights reserved, see LICENSE.md
 * ============================================================ */

#include <ti/devices/msp/msp.h>
#include "delay.h"
#include "buttons.h"
#include "timing.h"
#include "buzzer.h"
#include "leds.h"
#include "colors.h"
#include "music.h"
#include "random.h"
#include "state_machine_logic.h"

/* ---------------------------------------------------------------
 * Helper: read-modify-write a 32-bit MMIO register.
 * Taken directly from the course TRNG gist (MSPM0G3507).
 * --------------------------------------------------------------- */
static __STATIC_INLINE void update_reg(volatile uint32_t *reg,
                                        uint32_t           val,
                                        uint32_t           mask)
{
    uint32_t tmp = *reg;
    tmp  &= ~mask;
    *reg  = tmp | (val & mask);
}

/* ---------------------------------------------------------------
 * InitializeTRNGSeed
 *
 * Turns on the TRNG peripheral, collects one hardware-random word,
 * and seeds the LFSR RNG so each power cycle produces a different
 * sequence.  Implementation follows the course gist exactly:
 *   https://gist.github.com/kemerelab/a8de90811a15cb982f5fb3d0e2410c70
 * --------------------------------------------------------------- */
static void InitializeTRNGSeed(void)
{
    /* Step 1: Reset and power on the TRNG module */
    TRNG->GPRCM.RSTCTL = TRNG_RSTCTL_RESETASSERT_ASSERT |
                          TRNG_RSTCTL_KEY_UNLOCK_W;
    TRNG->GPRCM.PWREN  = TRNG_PWREN_KEY_UNLOCK_W        |
                          TRNG_PWREN_ENABLE_ENABLE;
    delay_cycles(POWER_STARTUP_DELAY);

    /* Step 2: Set clock divider (÷2) */
    TRNG->CLKDIVIDE = (uint32_t)TRNG_CLKDIVIDE_RATIO_DIV_BY_2;

    /* Step 3: Issue "Normal Function" command and wait for it to complete */
    update_reg(&TRNG->CTL,
               (uint32_t)TRNG_CTL_CMD_NORM_FUNC,
               TRNG_CTL_CMD_MASK);
    while (!((TRNG->CPU_INT.RIS & TRNG_RIS_IRQ_CMD_DONE_MASK) ==
              TRNG_RIS_IRQ_CMD_DONE_SET));
    TRNG->CPU_INT.ICLR = TRNG_IMASK_IRQ_CMD_DONE_MASK;

    /* Step 4: Set decimation rate (÷4) */
    update_reg(&TRNG->CTL,
               ((uint32_t)0x3 << TRNG_CTL_DECIM_RATE_OFS),
               TRNG_CTL_DECIM_RATE_MASK);

    /* Step 5: Wait for a captured sample to be ready */
    while (!((TRNG->CPU_INT.RIS & TRNG_RIS_IRQ_CAPTURED_RDY_MASK) ==
              TRNG_RIS_IRQ_CAPTURED_RDY_SET));
    TRNG->CPU_INT.ICLR = TRNG_IMASK_IRQ_CAPTURED_RDY_MASK;

    /* Step 6: Read the random word and seed the LFSR */
    uint32_t raw  = TRNG->DATA_CAPTURE;
    uint16_t seed = (uint16_t)(raw & 0xFFFFu);
    if (seed == 0u) seed = 0xACE1u;   /* LFSR must never be seeded with 0 */
    srand(seed);

    /* Step 7: Power off the TRNG to save energy */
    TRNG->GPRCM.PWREN = TRNG_PWREN_KEY_UNLOCK_W | TRNG_PWREN_ENABLE_DISABLE;
}

/* ---------------------------------------------------------------
 * main
 * --------------------------------------------------------------- */
int main(void)
{
    /* ====================================================
     * 1. Hardware initialization
     * ==================================================== */
    InitializeButtonGPIO();
    InitializeBuzzer();
    InitializeLEDInterface();
    InitializeTimerG0();
    DisableBuzzer();       /* Buzzer init leaves it enabled; silence it now */

    /* ====================================================
     * 2. Seed the RNG from TRNG (must be done once at boot)
     * ==================================================== */
    InitializeTRNGSeed();

    /* ====================================================
     * 3. Initialize the FSM state
     * ==================================================== */
    state_t state;

    /* Button sub-states: all idle at boot */
    for (int i = 0; i < 4; i++) {
        state.buttons[i].state             = BUTTON_IDLE;
        state.buttons[i].depressed_counter = 0;
    }

    /* Hardware output defaults: silence, LEDs off */
    state.buzzer = (buzzer_state_t){ .period = 0, .sound_on = false };
    state.leds   = &leds_off;

    /* FSM starts in the boot animation */
    state.game_mode    = MODE_BOOT_ANIM;
    state.mode_counter = 0;

    /* Sequence and player-input fields (properly set in PRE_GAME) */
    state.seq_len         = 0;
    state.show_idx        = 0;
    state.elem_active     = false;
    state.input_idx       = 0;
    state.active_button   = -1;
    state.timeout_counter = 0;

    /* Zero the sequence array for safety */
    for (int i = 0; i < WIN_SEQUENCE_LENGTH; i++) {
        state.sequence[i] = 0;
    }

    /* Load boot animation into the animation playback tracker */
    InitAnimation(&state.anim_state, boot_animation, boot_animation_length);

    /* ====================================================
     * 4. Start the periodic FSM timer (TIMG0)
     * ==================================================== */
    const int message_len = (int)(sizeof(leds_message_t) / sizeof(uint16_t));
    SetTimerG0Delay(PERIOD);
    EnableTimerG0();

    /* ====================================================
     * 5. Main loop
     *
     * TIMG0_IRQHandler sets timer_wakeup = true every PERIOD
     * LFCLK ticks (0.625 ms).  We process exactly one FSM
     * tick per timer wakeup; SPI wakeups are ignored here.
     * ==================================================== */
    while (1) {

        if (timer_wakeup) {

            /* (a) Apply the current buzzer state to TIMA1 hardware */
            SetBuzzerState(state.buzzer);

            /* (b) Transmit LED state via SPI.
             *     SendSPIMessage() returns false if a previous frame
             *     is still in flight; spin until it is free.
             *     In practice this completes in <100 µs at 2 Mbps.   */
            while (!SendSPIMessage((uint16_t *)state.leds, message_len)) {
                /* busy-wait for previous SPI frame to finish */
            }

            /* (c) Sample all four button GPIO pins at once */
            uint32_t gpio_input = GPIOA->DIN31_0 & (SW1 | SW2 | SW3 | SW4);

            /* (d) Advance the FSM: returns entirely new state_t */
            state = GetNextState(state, gpio_input);

            /* (e) Clear flag and sleep until the next timer interrupt */
            timer_wakeup = false;
            __WFI();
        }
        /* Note: SPI0_IRQHandler also fires during SPI TX, which wakes
         * the CPU.  timer_wakeup will still be false after those, so
         * we simply go back to sleep without processing.             */
    }
}