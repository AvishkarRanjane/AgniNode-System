#ifndef PROTECTION_H
#define PROTECTION_H

#include "analysis.h"
#include "sensors.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool fire;             /* Confirmed Fire Hazard (Flame, Smoke, or Temp)   */
    bool arc_fault;        /* High-frequency AC waveform distortion arc fault */
    bool shock;            /* Voltage anomaly / surge / severe shock sag      */
    bool ssr_tripped;      /* Open-Drain SSR state (true = tripped / open)    */
    bool buzzer_active;    /* Buzzer alarm active                             */
    uint32_t last_trip_ms; /* Timestamp of the most recent safety trip        */
} protection_state_t;

void protection_init(void);
void protection_on_frame(const frame_result_t *r, uint32_t now_ms);
void protection_step(const sensors_t *s, uint32_t now_ms);
const protection_state_t *protection_get_state(void);

bool protection_reset(uint32_t now_ms);
void protection_manual_trip(uint32_t now_ms);
void protection_buzzer_override(bool enable);

#endif /* PROTECTION_H */
