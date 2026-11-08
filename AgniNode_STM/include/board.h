#ifndef BOARD_H
#define BOARD_H

#include "stm32g0xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

void board_init(void);
void board_wdg_kick(void);
const char *board_reset_cause(void);
uint16_t board_micros16(void);

/* Acquisition: TIM3 -> ADC1 -> DMA, 2 channels x 256 samples per frame (20 ms) */
const uint16_t *board_frame_peek(void);
void board_frame_release(void);
uint32_t board_frame_drops(void);

/* UART Bridge to ESP32 Gateway */
void board_uart_send(const uint8_t *d, uint16_t n);
void board_uart_send_str(const char *s);
bool board_uart_rx_pop(uint8_t *b);
uint32_t board_uart_rx_errors(void);

void Error_Handler(void);

#endif /* BOARD_H */
