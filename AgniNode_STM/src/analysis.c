/* analysis.c - RMS voltage, arc-fault HF distortion, and shock voltage anomaly engine */
#include "analysis.h"
#include "board.h"
#include "config.h"
#include <math.h>
#include <string.h>

static frame_result_t latest_result;
static uint32_t frame_sequence = 0;
static uint16_t accumulated_arc_score = 0;
static bool latched_arc_fault = false;
static bool latched_shock_fault = false;
static bool previous_mains_active = false;

void analysis_init(void) {
    memset(&latest_result, 0, sizeof(latest_result));
    frame_sequence = 0;
    accumulated_arc_score = 0;
    latched_arc_fault = false;
    latched_shock_fault = false;
    previous_mains_active = false;
}

bool analysis_poll(void) {
    const uint16_t *raw_frame = board_frame_peek();
    if (!raw_frame)
        return false;

    frame_sequence++;
    latest_result.valid = true;
    latest_result.frame_seq = frame_sequence;

    /* Channel 0: ZMPT101B (PA0)
     * Channel 1: MQ-2 AO (PA1)
     * Interleaved samples: raw_frame[2*i] = PA0, raw_frame[2*i + 1] = PA1
     */
    float sum_v = 0.0f;
    float sum_smoke = 0.0f;
    float v_samples[ACQ_FRAME_SAMPLES];

    for (uint16_t i = 0; i < ACQ_FRAME_SAMPLES; i++) {
        uint16_t zmpt_cnt = raw_frame[2U * i + ACQ_IDX_ZMPT];
        uint16_t smoke_cnt = raw_frame[2U * i + ACQ_IDX_SMOKE];

        /* Reconstruct true sensor voltages with 3.2x multiplier */
        float v_sens = ADC_COUNTS_TO_SENSOR_V(zmpt_cnt);
        v_samples[i] = v_sens;
        sum_v += v_sens;

        sum_smoke += (float)smoke_cnt;
    }

    /* 1. Smoke Analog Reconstructed Voltage */
    float avg_smoke_cnt = sum_smoke / (float)ACQ_FRAME_SAMPLES;
    latest_result.smoke_adc_raw = (uint16_t)avg_smoke_cnt;
    latest_result.smoke_analog_v = ADC_COUNTS_TO_SENSOR_V(avg_smoke_cnt);

    /* 2. ZMPT101B AC RMS Voltage Calculation */
    float v_mid = sum_v / (float)ACQ_FRAME_SAMPLES;
    float sum_sq_diff = 0.0f;
    float sum_d2_sq = 0.0f;

    for (uint16_t i = 0; i < ACQ_FRAME_SAMPLES; i++) {
        float ac_diff = v_samples[i] - v_mid;
        sum_sq_diff += (ac_diff * ac_diff);

        /* High frequency second difference for arc fault detection */
        if (i >= 2) {
            float d2 = v_samples[i] - 2.0f * v_samples[i - 1] + v_samples[i - 2];
            sum_d2_sq += (d2 * d2);
        }
    }

    float v_rms_sensor = sqrtf(sum_sq_diff / (float)ACQ_FRAME_SAMPLES);
    /* Scale sensor AC RMS to calibrated AC Mains RMS voltage */
    float v_rms_mains = v_rms_sensor * ZMPT_CALIBRATION_FACTOR;

    latest_result.v_rms = v_rms_mains;
    latest_result.mains_present = (v_rms_mains >= MAINS_PRESENT_V);

    /* 3. Arc Fault High-Frequency Voltage Distortion Analysis */
    float hf_rms = sqrtf(sum_d2_sq / (float)(ACQ_FRAME_SAMPLES - 2)) * 100.0f;
    latest_result.hf_noise = hf_rms;

    if (latest_result.mains_present && hf_rms > ARC_HF_NOISE_THRESHOLD) {
        if (accumulated_arc_score < 100)
            accumulated_arc_score += 4;
    } else if (accumulated_arc_score > 0) {
        accumulated_arc_score--;
    }

    latest_result.arc_score = accumulated_arc_score;
    if (accumulated_arc_score >= ARC_TRIP_SCORE_LIMIT) {
        latched_arc_fault = true;
    }
    latest_result.arc_fault = latched_arc_fault;

    /* 4. Shock / Voltage Anomaly Detection */
    /* Condition A: Overvoltage surge > 260V */
    if (v_rms_mains > MAINS_OVERVOLTAGE_V) {
        latched_shock_fault = true;
    }

    /* Condition B: Severe sag / drop (< 180V) while mains was previously active */
    if (previous_mains_active && v_rms_mains < MAINS_UNDERVOLTAGE_V && v_rms_mains > 20.0f) {
        latched_shock_fault = true;
    }

    if (latest_result.mains_present) {
        previous_mains_active = true;
    }

    latest_result.shock_fault = latched_shock_fault;

    board_frame_release();
    return true;
}

const frame_result_t *analysis_last(void) {
    return &latest_result;
}
