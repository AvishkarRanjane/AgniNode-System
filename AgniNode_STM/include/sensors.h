#ifndef SENSORS_H
#define SENSORS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    /* Analog reconstructed values (compensated for 3.2x voltage divider) */
    float v_rms;            /* AC mains RMS voltage calculated from PA0 (V) */
    float smoke_analog_v;   /* True MQ-2 AO sensor voltage (0.0 to 5.0 V)   */
    uint16_t raw_zmpt_adc;  /* 12-bit raw ADC count on PA0                  */
    uint16_t raw_smoke_adc; /* 12-bit raw ADC count on PA1                  */

    /* Digital readings */
    uint8_t smoke_dig; /* MQ-2 DO state on PB0 (0 = normal, 1 = active) */
    uint8_t flame_dig; /* Flame DO state on PB14 (0 = flame, 1 = clear) */

    /* Temperature reading on PA8 */
    float temp_c;          /* Current temperature in deg C                 */
    float temp_rate_c_min; /* Rate of rise in deg C / minute               */
    bool temp_valid;       /* True if sensor reading is valid              */

    /* Debounced logical hazard flags */
    bool smoke_hazard; /* Smoke analog > threshold or smoke DO active  */
    bool flame_hazard; /* Flame sensor optical detection active        */
    bool temp_hazard;  /* Temperature > 55 C or rate of rise > 8 C/min */
} sensors_t;

void sensors_init(void);
void sensors_update(uint32_t now_ms);
const sensors_t *sensors_get(void);

#endif /* SENSORS_H */
