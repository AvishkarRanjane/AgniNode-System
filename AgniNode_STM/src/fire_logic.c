/* fire_logic.c - autonomous fire hazard decision engine */
#include "fire_logic.h"
#include <string.h>

static fire_state_t state;

void fire_init(void) {
    memset(&state, 0, sizeof(state));
}

void fire_step(const sensors_t *s, uint32_t now_ms) {
    state.flame_leg = s->flame_hazard;
    state.smoke_leg = s->smoke_hazard;
    state.temp_leg = s->temp_hazard;

    if (state.flame_leg || state.smoke_leg || state.temp_leg) {
        if (!state.fire_detected) {
            state.fire_detected = true;
            state.triggered_ms = now_ms;
        }
    }
}

const fire_state_t *fire_get_state(void) {
    return &state;
}

bool fire_is_detected(void) {
    return state.fire_detected;
}

void fire_reset(void) {
    /* Clear fire latch */
    state.fire_detected = false;
    state.flame_leg = false;
    state.smoke_leg = false;
    state.temp_leg = false;
}
