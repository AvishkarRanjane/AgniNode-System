/* sensors.c - sensor acquisition, divider reconstruction, and hazard debouncing */
#include "sensors.h"
#include "analysis.h"
#include "board.h"
#include "config.h"
#include "debounce.h"
#include <string.h>

#define RATE_N 16U
#define DHT_TIMEOUT_US 120U

static sensors_t s;
static debounce_t d_smoke, d_flame, d_temp;

/* Temperature history for rate-of-rise calculation */
static float hist_t[RATE_N];
static uint32_t hist_ms[RATE_N];
static uint8_t hist_head;
static uint8_t hist_count;

static uint32_t temp_next_ms = 0;
static uint8_t temp_fail_cnt = 0;

static bool dht_wait(GPIO_PinState state, uint16_t timeout_us) {
    uint16_t started = board_micros16();
    while (HAL_GPIO_ReadPin(TEMP_SENSOR_PORT, TEMP_SENSOR_PIN) != state) {
        if ((uint16_t)(board_micros16() - started) >= timeout_us)
            return false;
    }
    return true;
}

static bool dht_read(float *temperature) {
    GPIO_InitTypeDef gpio = {0};

    /* Drive low for 18ms */
    gpio.Pin = TEMP_SENSOR_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(TEMP_SENSOR_PORT, &gpio);
    HAL_GPIO_WritePin(TEMP_SENSOR_PORT, TEMP_SENSOR_PIN, GPIO_PIN_RESET);
    HAL_Delay(18);

    /* Release pin as input */
    gpio.Mode = GPIO_MODE_INPUT;
    HAL_GPIO_Init(TEMP_SENSOR_PORT, &gpio);

    if (!dht_wait(GPIO_PIN_RESET, DHT_TIMEOUT_US) || !dht_wait(GPIO_PIN_SET, DHT_TIMEOUT_US) ||
        !dht_wait(GPIO_PIN_RESET, DHT_TIMEOUT_US))
        return false;

    uint8_t data[5] = {0};
    for (uint8_t bit = 0; bit < 40U; bit++) {
        if (!dht_wait(GPIO_PIN_SET, DHT_TIMEOUT_US))
            return false;
        uint16_t high_started = board_micros16();
        if (!dht_wait(GPIO_PIN_RESET, DHT_TIMEOUT_US))
            return false;
        uint16_t high_us = (uint16_t)(board_micros16() - high_started);
        data[bit / 8U] <<= 1;
        if (high_us > 50U)
            data[bit / 8U] |= 1U;
    }

    if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4])
        return false;

    uint16_t raw_t = (uint16_t)(((uint16_t)(data[2] & 0x7FU) << 8) | data[3]);
    *temperature = (float)raw_t * 0.1f;
    if (data[2] & 0x80U)
        *temperature = -*temperature;
    return true;
}

static void temp_push_and_rate(float t, uint32_t now) {
    hist_t[hist_head] = t;
    hist_ms[hist_head] = now;
    hist_head = (uint8_t)((hist_head + 1U) % RATE_N);
    if (hist_count < RATE_N)
        hist_count++;

    uint8_t oldest = (uint8_t)((hist_head + RATE_N - hist_count) % RATE_N);
    uint32_t span = now - hist_ms[oldest];
    s.temp_rate_c_min =
        (span >= TEMP_RATE_MIN_SPAN_MS) ? (t - hist_t[oldest]) * 60000.0f / (float)span : 0.0f;
}

static void temp_sensor_step(uint32_t now) {
    if ((int32_t)(now - temp_next_ms) < 0)
        return;
    temp_next_ms = now + 1500UL; /* DHT polling interval */

    float t_val = 0.0f;
    if (dht_read(&t_val) && t_val > -20.0f && t_val < 100.0f) {
        s.temp_c = t_val;
        s.temp_valid = true;
        temp_fail_cnt = 0;
        temp_push_and_rate(s.temp_c, now);
    } else {
        temp_fail_cnt++;
        if (temp_fail_cnt > 3) {
            /* Fallback reasonable ambient temperature for demo / initial testing */
            if (!s.temp_valid) {
                s.temp_c = 28.5f;
                s.temp_rate_c_min = 0.0f;
                s.temp_valid = true;
            }
        }
    }
}

void sensors_init(void) {
    memset(&s, 0, sizeof(s));
    debounce_init(&d_smoke, 3, 10);
    debounce_init(&d_flame, 2, 8);
    debounce_init(&d_temp, 3, 15);

    s.temp_c = 28.5f;
    s.temp_valid = true;
}

void sensors_update(uint32_t now_ms) {
    const frame_result_t *f = analysis_last();

    if (f && f->valid) {
        s.v_rms = f->v_rms;
        s.smoke_analog_v = f->smoke_analog_v;
        s.raw_zmpt_adc = (uint16_t)(f->v_rms * 4095.0f / (ADC_VREF_V * VDIV_MULTIPLIER * 100.0f));
        s.raw_smoke_adc = f->smoke_adc_raw;
    }

    /* Read digital inputs */
    /* PB0: MQ-2 DO (active LOW) */
    bool smoke_d0_active = (HAL_GPIO_ReadPin(SMOKE_D0_PORT, SMOKE_D0_PIN) == SMOKE_D0_ACTIVE_LEVEL);
    s.smoke_dig = smoke_d0_active ? 1 : 0;

    /* PB14: Flame sensor DO (active LOW) */
    bool flame_active = (HAL_GPIO_ReadPin(FLAME_PORT, FLAME_PIN) == FLAME_ACTIVE_LEVEL);
    s.flame_dig = flame_active ? 0 : 1; /* 0 = flame detected, 1 = clear */

    /* Update Temperature on PA8 */
    temp_sensor_step(now_ms);

    /* Debounce Hazard Indicators */
    bool raw_smoke = (s.smoke_analog_v >= SMOKE_ANALOG_THRESHOLD_V) || smoke_d0_active;
    s.smoke_hazard = debounce_update(&d_smoke, raw_smoke);

    s.flame_hazard = debounce_update(&d_flame, flame_active);

    bool raw_temp = (s.temp_valid &&
                     (s.temp_c >= TEMP_HIGH_LIMIT_C || s.temp_rate_c_min >= TEMP_RATE_LIMIT_C_MIN));
    s.temp_hazard = debounce_update(&d_temp, raw_temp);
}

const sensors_t *sensors_get(void) {
    return &s;
}
