#ifndef ACTUATORS_H
#define ACTUATORS_H

#include <stdbool.h>
#include <stdint.h>

void actuators_init(void);
void actuator_set_buzzer(bool on);
void actuator_set_ssr(bool tripped);

bool actuator_buzzer_is_on(void);
bool actuator_ssr_is_tripped(void);

#endif /* ACTUATORS_H */
