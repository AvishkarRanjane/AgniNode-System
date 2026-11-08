#ifndef FIRE_LOGIC_H
#define FIRE_LOGIC_H

#include "sensors.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool fire_detected;    /* Latched or active fire hazard condition */
    bool flame_leg;        /* Flame sensor triggered                 */
    bool smoke_leg;        /* Smoke sensor triggered (AO or DO)      */
    bool temp_leg;         /* Temperature sensor triggered           */
    uint32_t triggered_ms; /* Timestamp when fire was first detected */
} fire_state_t;

void fire_init(void);
void fire_step(const sensors_t *s, uint32_t now_ms);
const fire_state_t *fire_get_state(void);
bool fire_is_detected(void);
void fire_reset(void);

#endif /* FIRE_LOGIC_H */
