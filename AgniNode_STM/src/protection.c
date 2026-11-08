/* protection.c - autonomous electrical protection, SSR open-drain trip, and buzzer alarm */
#include "protection.h"
#include "actuators.h"
#include "fire_logic.h"
#include <string.h>

static protection_state_t pstate;
static bool buzzer_manual_mute = false;

void protection_init(void) {
    memset(&pstate, 0, sizeof(pstate));
    buzzer_manual_mute = false;
    actuator_set_ssr(false);    /* SSR untripped / closed */
    actuator_set_buzzer(false); /* Buzzer silent */
}

void protection_on_frame(const frame_result_t *r, uint32_t now_ms) {
    if (!r || !r->valid)
        return;

    if (r->arc_fault) {
        pstate.arc_fault = true;
    }
    if (r->shock_fault) {
        pstate.shock = true;
    }

    /* Fast path immediate trip if arc or shock fault is detected */
    if (pstate.arc_fault || pstate.shock) {
        if (!pstate.ssr_tripped) {
            pstate.ssr_tripped = true;
            pstate.last_trip_ms = now_ms;
            actuator_set_ssr(true); /* Immediately cut load */
        }
        if (!buzzer_manual_mute) {
            pstate.buzzer_active = true;
            actuator_set_buzzer(true);
        }
    }
}

void protection_step(const sensors_t *s, uint32_t now_ms) {
    (void)s;
    pstate.fire = fire_is_detected();

    /* Trigger protection if FIRE is confirmed */
    if (pstate.fire || pstate.arc_fault || pstate.shock) {
        if (!pstate.ssr_tripped) {
            pstate.ssr_tripped = true;
            pstate.last_trip_ms = now_ms;
            actuator_set_ssr(true);
        }
        if (!buzzer_manual_mute) {
            pstate.buzzer_active = true;
            actuator_set_buzzer(true);
        }
    } else {
        if (!pstate.ssr_tripped) {
            actuator_set_ssr(false);
        }
        if (!buzzer_manual_mute && !pstate.ssr_tripped) {
            pstate.buzzer_active = false;
            actuator_set_buzzer(false);
        }
    }
}

const protection_state_t *protection_get_state(void) {
    return &pstate;
}

bool protection_reset(uint32_t now_ms) {
    (void)now_ms;
    /* Clear fire latch */
    fire_reset();

    /* Reset faults */
    pstate.fire = false;
    pstate.arc_fault = false;
    pstate.shock = false;
    pstate.ssr_tripped = false;
    pstate.buzzer_active = false;
    buzzer_manual_mute = false;

    actuator_set_ssr(false);
    actuator_set_buzzer(false);
    return true;
}

void protection_manual_trip(uint32_t now_ms) {
    pstate.ssr_tripped = true;
    pstate.last_trip_ms = now_ms;
    actuator_set_ssr(true);
}

void protection_buzzer_override(bool enable) {
    buzzer_manual_mute = !enable;
    pstate.buzzer_active = enable;
    actuator_set_buzzer(enable);
}
