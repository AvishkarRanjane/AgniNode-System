#ifndef ANALYSIS_H
#define ANALYSIS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool valid;
    uint32_t frame_seq;

    /* Mains Voltage Analysis (PA0 ZMPT101B) */
    float v_rms;        /* Reconstructed true AC RMS Voltage in Volts */
    float hf_noise;     /* High-frequency 2nd-difference variance      */
    uint16_t arc_score; /* Accumulated arc fault distortion score      */
    bool arc_fault;     /* True if persistent arc signatures detected  */
    bool shock_fault;   /* True if severe voltage drop / sag or >260V  */
    bool mains_present; /* True if AC line is energized (>50V)         */

    /* Smoke Analog Analysis (PA1 MQ-2 AO) */
    float smoke_analog_v;   /* True reconstructed sensor voltage (0 - 5V) */
    uint16_t smoke_adc_raw; /* Average raw 12-bit ADC counts on PA1        */
} frame_result_t;

void analysis_init(void);
bool analysis_poll(void);
const frame_result_t *analysis_last(void);

#endif /* ANALYSIS_H */
