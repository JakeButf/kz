#include <stdint.h>
#include "trainers.h"
#include "kz.h"
#include "settings.h"
#include "input.h"
#include "gfx.h"

#define CHEST_TEXT_ID       0x778
#define CHEST_READY_TEXT_ID 0x779
#define CHEST_RI_X          0
#define CHEST_RI_Y          0
#define CHEST_RI_LINE_H     10
#define CHEST_RI_TIMEOUT    30
#define CHEST_RI_SHOW_TIME  100
#define NO_FRAME            (-1)
#define CHEST_RI_MASK_OFFSET 0
#define CHEST_RI_CS_FRAMES  150

enum ri_pause_state {
    RI_PAUSE_IDLE,
    RI_PAUSE_WAIT_WINDOW,
    RI_PAUSE_WAIT_PRESS,
    RI_PAUSE_DONE,
    RI_PAUSE_SKIPPED,
};

enum ri_mask_state {
    RI_MASK_IDLE,
    RI_MASK_WAIT_CS_START,
    RI_MASK_WAIT_CS_END,
    RI_MASK_WAIT_PRESS,
    RI_MASK_DONE,
};

static int32_t  ri_frame;
static int32_t  ri_gframe;
static int      pause_state = RI_PAUSE_IDLE;
static int      mask_state = RI_MASK_IDLE;
static int32_t  pause_window;
static int32_t  pause_press;
static int32_t  mask_target;
static int32_t  mask_last_press;
static int      pause_delta;
static int      mask_delta;
static _Bool    prev_cs;
static _Bool    mask_only;
static int      prev_pause_state = RI_PAUSE_IDLE;
static int      prev_mask_state = RI_MASK_IDLE;
static int32_t  pause_done_frame;
static int32_t  mask_done_frame;

static _Bool keaton_pressed(uint16_t pressed) {
    return (pressed & (BUTTON_C_LEFT | BUTTON_C_DOWN | BUTTON_C_RIGHT)) != 0;
}

static void ri_reset(void) {
    pause_state = RI_PAUSE_WAIT_WINDOW;
    mask_state = RI_MASK_WAIT_CS_START;
    pause_window = NO_FRAME;
    pause_press = NO_FRAME;
    mask_target = NO_FRAME;
    mask_last_press = NO_FRAME;
    mask_only = 0;
}

static void chest_ri_update(void) {
    ri_frame++;
    if(z2_game.pause_ctx.state == 0) {
        ri_gframe++;
    }

    uint16_t pressed = z2_ctxt.input[0].pad_pressed;
    _Bool in_cs = z2_Play_InCsMode(&z2_game);
    _Bool text_open = z2_game.message_state_1 != 0 &&
                      (z2_game.message_text_id == CHEST_TEXT_ID || z2_game.message_text_id == CHEST_READY_TEXT_ID);

    if(text_open && pause_state != RI_PAUSE_WAIT_WINDOW && pause_state != RI_PAUSE_WAIT_PRESS) {
        ri_reset();
    }

    if(pause_state == RI_PAUSE_WAIT_WINDOW || pause_state == RI_PAUSE_WAIT_PRESS) {
        if((pressed & BUTTON_START) && pause_press == NO_FRAME) {
            pause_press = ri_frame;
        }
        if(pause_state == RI_PAUSE_WAIT_WINDOW) {
            if(z2_game.message_state_1 == 0 && !in_cs) {
                pause_window = ri_frame + 1;
                pause_state = RI_PAUSE_WAIT_PRESS;
            }
        }
        if(pause_state == RI_PAUSE_WAIT_PRESS) {
            if(pause_press != NO_FRAME) {
                pause_delta = pause_press - pause_window;
                pause_state = RI_PAUSE_DONE;
            } else if(ri_frame - pause_window > CHEST_RI_TIMEOUT) {
                pause_state = RI_PAUSE_DONE;
                pause_delta = 0x7FFF;
            }
        }
    }

    _Bool keaton = keaton_pressed(pressed);
    _Bool paused = z2_game.pause_ctx.state != 0;
    if(!paused) switch(mask_state) {
        case RI_MASK_WAIT_CS_START:
            if((pause_window != NO_FRAME || mask_only) && in_cs) {
                mask_target = ri_gframe + CHEST_RI_CS_FRAMES;
                mask_state = RI_MASK_WAIT_CS_END;
            }
            break;
        case RI_MASK_WAIT_CS_END:
            if(!in_cs && ri_gframe + CHEST_RI_MASK_OFFSET < mask_target) {
                mask_target = ri_gframe + CHEST_RI_MASK_OFFSET;
            }
            if(!in_cs || ri_gframe + CHEST_RI_MASK_OFFSET >= mask_target) {
                mask_state = RI_MASK_WAIT_PRESS;
            }
            break;
        default:
            break;
    }
    if(!paused && mask_state == RI_MASK_WAIT_PRESS) {
        int late = ri_gframe - mask_target;
        if((keaton && late >= 0) || late > CHEST_RI_TIMEOUT) {
            int early = mask_last_press == NO_FRAME ? 0x7FFF : mask_target - mask_last_press;
            if(!keaton) {
                late = 0x7FFF;
            }
            mask_delta = early < late ? -early : late;
            if(early == 0x7FFF && late == 0x7FFF) {
                mask_delta = 0x7FFF;
            }
            mask_state = RI_MASK_DONE;
        }
    }
    if(keaton && !paused && (mask_state == RI_MASK_WAIT_CS_START || mask_state == RI_MASK_WAIT_CS_END ||
                             (mask_state == RI_MASK_WAIT_PRESS && ri_gframe < mask_target))) {
        mask_last_press = ri_gframe;
    }
    prev_cs = in_cs;
}

static void ri_draw_line(int y, const char *name, int delta) {
    if(delta == 0) {
        gfx_printf_color(CHEST_RI_X, y, COLOR_GREEN, "%s: on frame", name);
    } else if(delta == 0x7FFF) {
        gfx_printf_color(CHEST_RI_X, y, COLOR_RED, "%s: no input", name);
    } else if(delta < 0) {
        gfx_printf_color(CHEST_RI_X, y, COLOR_RED, "%s: %df early", name, -delta);
    } else {
        gfx_printf_color(CHEST_RI_X, y, COLOR_RED, "%s: %df late", name, delta);
    }
}

void trainers_reset(void) {
    if(settings->chest_ri) {
        pause_state = RI_PAUSE_SKIPPED;
        mask_state = RI_MASK_WAIT_CS_START;
        mask_only = 1;
        mask_last_press = NO_FRAME;
        mask_target = NO_FRAME;
    } else {
        pause_state = RI_PAUSE_IDLE;
        mask_state = RI_MASK_IDLE;
    }
    prev_pause_state = pause_state;
    prev_mask_state = mask_state;
    prev_cs = 0;
}

void trainers_update(void) {
    if(settings->chest_ri) {
        chest_ri_update();

        if(pause_state == RI_PAUSE_DONE && prev_pause_state != RI_PAUSE_DONE) {
            pause_done_frame = ri_frame;
        }
        if(mask_state == RI_MASK_DONE && prev_mask_state != RI_MASK_DONE) {
            mask_done_frame = ri_frame;
        }
        prev_pause_state = pause_state;
        prev_mask_state = mask_state;

        if(pause_state == RI_PAUSE_DONE && ri_frame - pause_done_frame < CHEST_RI_SHOW_TIME) {
            ri_draw_line(CHEST_RI_Y, "pause", pause_delta);
        }
        if(mask_state == RI_MASK_DONE && ri_frame - mask_done_frame < CHEST_RI_SHOW_TIME) {
            ri_draw_line(CHEST_RI_Y + CHEST_RI_LINE_H, "keaton", mask_delta);
        }
    } else {
        pause_state = RI_PAUSE_IDLE;
        mask_state = RI_MASK_IDLE;
        prev_pause_state = RI_PAUSE_IDLE;
        prev_mask_state = RI_MASK_IDLE;
    }
}
