#ifndef CONFIG_H
#define CONFIG_H

#include "stm32g0xx_hal.h"

#define FW_VERSION "3.2.0"

/* ==========================================================================
 * HARDWARE PINOUT (STM32G0B1RE Target MCU)
 * ========================================================================== */

/* Analog Inputs (12-bit ADC, 3.3V ref, 220 ohm / 100 ohm physical dividers) */
#define ZMPT_ADC_CHANNEL ADC_CHANNEL_0  /* PA0: ZMPT101B AC voltage sensor OUT */
#define SMOKE_ADC_CHANNEL ADC_CHANNEL_1 /* PA1: MQ-2 smoke sensor AO            */

/* Digital Inputs & Outputs */
#define SMOKE_D0_PORT GPIOB /* PB0: MQ-2 smoke DO (GPIO_NOPULL, active LOW) */
#define SMOKE_D0_PIN GPIO_PIN_0
#define TEMP_SENSOR_PORT GPIOA /* PA8: Temperature sensor DATA/OUT             */
#define TEMP_SENSOR_PIN GPIO_PIN_8
#define FLAME_PORT GPIOB /* PB14: Flame sensor DO (active LOW)           */
#define FLAME_PIN GPIO_PIN_14
#define BUZZER_PORT GPIOB /* PB12: Buzzer module I/O (active HIGH)        */
#define BUZZER_PIN GPIO_PIN_12
#define SSR_PORT GPIOB /* PB13: Solid State Relay CH1/IN (Open-Drain)  */
#define SSR_PIN GPIO_PIN_13

/* UART Bridge to ESP32 Gateway (115200 8N1) */
#define ESP_UART_TX_PORT GPIOA /* PA9: USART1 TX -> ESP32 GPIO 16 (RX2)       */
#define ESP_UART_TX_PIN GPIO_PIN_9
#define ESP_UART_RX_PORT GPIOA /* PA10: USART1 RX <- ESP32 GPIO 17 (TX2)      */
#define ESP_UART_RX_PIN GPIO_PIN_10

/* Active Levels */
#define FLAME_ACTIVE_LEVEL GPIO_PIN_RESET    /* Active LOW when flame/IR is detected */
#define SMOKE_D0_ACTIVE_LEVEL GPIO_PIN_RESET /* Active LOW when smoke threshold is exceeded */

/* ==========================================================================
 * ELECTRICAL SPECIFICATIONS & VOLTAGE DIVIDER SCALING
 * ==========================================================================
 * Both 5V analog sensors pass through an R1=220 ohm / R2=100 ohm divider:
 * Ratio: R2 / (R1 + R2) = 100 / (220 + 100) = 0.3125
 * Multiplier to restore true 0-5V sensor voltage: 1 / 0.3125 = 3.2
 * ADC: 12-bit (0-4095 counts), VREF = 3.3V
 * V_pin = counts * (3.3V / 4095.0)
 * V_sensor = V_pin * 3.2 = counts * (10.56 / 4095.0)
 * ========================================================================== */
#define ADC_VREF_V 3.3f
#define ADC_MAX_COUNTS 4095.0f
#define VDIV_MULTIPLIER 3.2f
#define ADC_COUNTS_TO_SENSOR_V(counts)                                                             \
    (((float)(counts) * ADC_VREF_V * VDIV_MULTIPLIER) / ADC_MAX_COUNTS)

/* ==========================================================================
 * SAFETY THRESHOLDS & CALIBRATION
 * ========================================================================== */
/* AC Mains Voltage (ZMPT101B) */
#define ZMPT_CALIBRATION_FACTOR 230.0f /* Calibrated multiplier to true AC RMS */
#define MAINS_NOMINAL_V 230.0f
#define MAINS_PRESENT_V 50.0f       /* Minimum voltage indicating mains is energized */
#define MAINS_OVERVOLTAGE_V 260.0f  /* Surge / over-voltage trip limit (> 260V AC) */
#define MAINS_UNDERVOLTAGE_V 180.0f /* Shock anomaly / severe voltage sag (< 180V AC) */

/* Arc Fault Detection (Waveform High-Frequency Distortion) */
#define ARC_HF_NOISE_THRESHOLD 12.0f /* HF 2nd difference variance threshold */
#define ARC_TRIP_SCORE_LIMIT 20U     /* Accumulated persistent distortion frames */

/* Smoke / Gas Detection */
#define SMOKE_ANALOG_THRESHOLD_V 1.80f /* True sensor voltage above which smoke is flagged */
#define SMOKE_ADC_THRESHOLD 2200U      /* 12-bit threshold before divider compensation */

/* Temperature Limits */
#define TEMP_HIGH_LIMIT_C 55.0f    /* Safe upper thermal limit */
#define TEMP_RATE_LIMIT_C_MIN 8.0f /* Thermal rate of rise limit (deg C / min) */
#define TEMP_RATE_MIN_SPAN_MS 10000UL

/* ==========================================================================
 * TIMING & OPERATING CONSTANTS
 * ========================================================================== */
#define SYSCLK_HZ 64000000UL       /* 64 MHz HSI16 PLL */
#define ACQ_SAMPLE_RATE_HZ 12800UL /* 256 samples per 20ms AC cycle (50 Hz) */
#define ACQ_FRAME_SAMPLES 256U
#define ACQ_NCH 2U /* PA0 (ZMPT101B), PA1 (MQ-2 AO) */
#define ACQ_IDX_ZMPT 0U
#define ACQ_IDX_SMOKE 1U

#define LOOP_PERIOD_MS 100U      /* 10 Hz main safety loop */
#define TELEMETRY_PERIOD_MS 500U /* 500ms UART report period */
#define UART_BAUDRATE 115200U
#define UART_RX_RING_SIZE 256U
#define UART_TX_RING_SIZE 1024U
#define WATCHDOG_ENABLED 1

#endif /* CONFIG_H */
