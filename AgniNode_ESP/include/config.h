#pragma once
#include <Arduino.h>

#define FW_VERSION_ESP "3.0.0"

/* ---- UART to STM32 (3.3 V, common GND) ---- */
#define STM_UART_RX_PIN 16 /* <- STM32 PA9  */
#define STM_UART_TX_PIN 17 /* -> STM32 PA10 */
#define STM_UART_BAUD 115200

/* ---- STM32 ROM-bootloader control (for firmware flashing) ---- */
#define STM_BOOT0_PIN 4 /* -> STM32 BOOT0 (PF4)                      */
#define STM_NRST_PIN 5  /* -> STM32 NRST (open-drain, active low)    */
#define STM32_FLASH_BASE 0x08000000UL
#define STM32_FLASH_SIZE (512UL * 1024UL)
#define STM32_RAM_TOP 0x20024000UL /* 144 KB SRAM */

/* ---- timing ---- */
#define UPLOAD_INTERVAL_MS 10000UL
#define HISTORY_EVERY_N 6
#define STM_TIMEOUT_MS 8000UL
#define WIFI_RETRY_MS 10000UL
#define SERIAL_HEARTBEAT_MS 15000UL
#define HTTP_TIMEOUT_MS 8000UL
#define TTL_SECONDS (14UL * 24UL * 3600UL)
#define ALERT_IMMEDIATE true

/* ---- commands ---- */
#define COMMAND_POLL_MS 3000UL
#define CMD_MAX_AGE_S 60
#define ACK_TIMEOUT_MS 1500UL
#define CMD_MAX_ATTEMPTS 3
#define FINAL_UPDATE_RETRIES 5
#define ALLOW_REMOTE_IGNITION 1 /* 0 = SUPPRESS_* actions rejected at the gateway */

/* ---- OTA ---- */
#define ENABLE_OTA 1
#define OTA_URL_PREFIX "https://firebasestorage.googleapis.com/" /* required URL prefix */
#define OTA_MAX_BOOT_ATTEMPTS 3      /* unconfirmed boots before automatic rollback */
#define OTA_HEALTH_UPTIME_MS 45000UL /* healthy uptime + cloud OK to confirm update */
#define STM_OTA_ATTEMPTS 3
#define STM_IMAGE_PATH "/stm32.bin"
#define TASK_WDT_SECONDS 90
