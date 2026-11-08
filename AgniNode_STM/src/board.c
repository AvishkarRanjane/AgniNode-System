/* board.c - 64 MHz clock, GPIO, TIM3-triggered ADC+DMA acquisition, USART1 ring, IWDG */
#include "board.h"
#include "config.h"
#include <string.h>

#define FRAME_LEN (ACQ_FRAME_SAMPLES * ACQ_NCH)

static UART_HandleTypeDef huart1;
static ADC_HandleTypeDef hadc1;
static DMA_HandleTypeDef hdma_adc1;
static TIM_HandleTypeDef htim3;
static TIM_HandleTypeDef htim14;
#if WATCHDOG_ENABLED
static IWDG_HandleTypeDef hiwdg;
#endif

/* ---- UART rings ---- */
static volatile uint8_t rx_ring[UART_RX_RING_SIZE];
static volatile uint16_t rx_head, rx_tail;
static volatile uint32_t rx_errors;
static volatile uint8_t tx_ring[UART_TX_RING_SIZE];
static volatile uint16_t tx_head, tx_tail;
static volatile bool tx_active;

/* ---- ADC frame queue (2 frames) ---- */
static uint16_t adc_dma_buf[2U * FRAME_LEN];
static uint16_t frames[2][FRAME_LEN];
static volatile uint8_t fr_head, fr_tail;
static volatile uint32_t fr_drops;

static const char *reset_cause = "POWER";

/* ------------------------------------------------------------------------- */
/* Interrupt handlers                                                        */
/* ------------------------------------------------------------------------- */
void SysTick_Handler(void) {
    HAL_IncTick();
}

void DMA1_Channel1_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_adc1);
}

static void frame_push(const uint16_t *src) {
    if ((uint8_t)(fr_head - fr_tail) >= 2U) {
        fr_drops++;
        return;
    }
    memcpy(frames[fr_head & 1U], src, sizeof(frames[0]));
    fr_head++;
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1)
        frame_push(&adc_dma_buf[0]);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1)
        frame_push(&adc_dma_buf[FRAME_LEN]);
}

const uint16_t *board_frame_peek(void) {
    if (fr_head == fr_tail)
        return NULL;
    return frames[fr_tail & 1U];
}

void board_frame_release(void) {
    fr_tail++;
}

uint32_t board_frame_drops(void) {
    return fr_drops;
}

void USART1_IRQHandler(void) {
    uint32_t isr = USART1->ISR;

    if (isr & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE)) {
        USART1->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF;
        rx_errors++;
    }
    if (isr & USART_ISR_RXNE_RXFNE) {
        uint8_t b = (uint8_t)USART1->RDR;
        uint16_t next = (uint16_t)((rx_head + 1U) & (UART_RX_RING_SIZE - 1U));
        if (next != rx_tail) {
            rx_ring[rx_head] = b;
            rx_head = next;
        } else {
            rx_errors++;
        }
    }
    if ((isr & USART_ISR_TXE_TXFNF) && (USART1->CR1 & USART_CR1_TXEIE_TXFNFIE)) {
        if (tx_head != tx_tail) {
            USART1->TDR = tx_ring[tx_tail];
            tx_tail = (uint16_t)((tx_tail + 1U) & (UART_TX_RING_SIZE - 1U));
        } else {
            CLEAR_BIT(USART1->CR1, USART_CR1_TXEIE_TXFNFIE);
            tx_active = false;
        }
    }
}

void board_uart_send(const uint8_t *d, uint16_t n) {
    for (uint16_t i = 0; i < n; i++) {
        uint16_t next = (uint16_t)((tx_head + 1U) & (UART_TX_RING_SIZE - 1U));
        while (next == tx_tail) {
            /* buffer full; pump transmitter */
            if (!tx_active) {
                tx_active = true;
                SET_BIT(USART1->CR1, USART_CR1_TXEIE_TXFNFIE);
            }
        }
        tx_ring[tx_head] = d[i];
        tx_head = next;
    }
    __disable_irq();
    if (!tx_active && tx_head != tx_tail) {
        tx_active = true;
        SET_BIT(USART1->CR1, USART_CR1_TXEIE_TXFNFIE);
    }
    __enable_irq();
}

void board_uart_send_str(const char *s) {
    if (s) {
        board_uart_send((const uint8_t *)s, (uint16_t)strlen(s));
    }
}

bool board_uart_rx_pop(uint8_t *b) {
    if (rx_head == rx_tail)
        return false;
    *b = rx_ring[rx_tail];
    rx_tail = (uint16_t)((rx_tail + 1U) & (UART_RX_RING_SIZE - 1U));
    return true;
}

uint32_t board_uart_rx_errors(void) {
    return rx_errors;
}

uint16_t board_micros16(void) {
    return (uint16_t)__HAL_TIM_GET_COUNTER(&htim14);
}

const char *board_reset_cause(void) {
    return reset_cause;
}

void board_wdg_kick(void) {
#if WATCHDOG_ENABLED
    HAL_IWDG_Refresh(&hiwdg);
#endif
}

/* ------------------------------------------------------------------------- */
/* Hardware Setup                                                            */
/* ------------------------------------------------------------------------- */
static void clock_init(void) {
    RCC_OscInitTypeDef o = {0};
    RCC_ClkInitTypeDef c = {0};

    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

    o.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    o.HSIState = RCC_HSI_ON;
    o.HSIDiv = RCC_HSI_DIV1;
    o.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    o.PLL.PLLState = RCC_PLL_ON;
    o.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    o.PLL.PLLM = RCC_PLLM_DIV1;
    o.PLL.PLLN = 8;
    o.PLL.PLLP = RCC_PLLP_DIV2;
    o.PLL.PLLQ = RCC_PLLQ_DIV2;
    o.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&o) != HAL_OK)
        Error_Handler();

    c.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
    c.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    c.AHBCLKDivider = RCC_SYSCLK_DIV1;
    c.APB1CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&c, FLASH_LATENCY_2) != HAL_OK)
        Error_Handler();
}

static void gpio_init(void) {
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Initial states: Buzzer OFF, SSR untripped (High-Z) */
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SSR_PORT, SSR_PIN, GPIO_PIN_SET);

    /* Buzzer: Push-Pull Output (PB12) */
    g.Pin = BUZZER_PIN;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BUZZER_PORT, &g);

    /* SSR: Open-Drain Output (PB13) */
    g.Pin = SSR_PIN;
    g.Mode = GPIO_MODE_OUTPUT_OD;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SSR_PORT, &g);

    /* MQ-2 Digital Input: PB0 (GPIO_NOPULL, active LOW) */
    g.Pin = SMOKE_D0_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(SMOKE_D0_PORT, &g);

    /* Flame Digital Input: PB14 (GPIO_PULLUP, active LOW) */
    g.Pin = FLAME_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(FLAME_PORT, &g);

    /* Temperature Sensor DATA: PA8 */
    g.Pin = TEMP_SENSOR_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(TEMP_SENSOR_PORT, &g);

    /* Analog Inputs: PA0 (ZMPT101B ADC_IN0), PA1 (MQ-2 AO ADC_IN1) */
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    g.Mode = GPIO_MODE_ANALOG;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &g);
}

static void uart_init(void) {
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_USART1_CLK_ENABLE();

    /* PA9 (TX) and PA10 (RX) */
    g.Pin = ESP_UART_TX_PIN | ESP_UART_RX_PIN;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF1_USART1;
    HAL_GPIO_Init(GPIOA, &g);

    huart1.Instance = USART1;
    huart1.Init.BaudRate = UART_BAUDRATE;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart1) != HAL_OK)
        Error_Handler();

    SET_BIT(USART1->CR1, USART_CR1_RXNEIE_RXFNEIE);
    HAL_NVIC_SetPriority(USART1_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

static void acq_init(void) {
    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_ADC_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();

    /* DMA1 Channel 1 for ADC1 */
    hdma_adc1.Instance = DMA1_Channel1;
    hdma_adc1.Init.Request = DMA_REQUEST_ADC1;
    hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_adc1.Init.Mode = DMA_CIRCULAR;
    hdma_adc1.Init.Priority = DMA_PRIORITY_HIGH;
    if (HAL_DMA_Init(&hdma_adc1) != HAL_OK)
        Error_Handler();
    __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

    /* ADC1 configuration */
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
    hadc1.Init.LowPowerAutoWait = DISABLE;
    hadc1.Init.LowPowerAutoPowerOff = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.NbrOfConversion = 2;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T3_TRGO;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hadc1.Init.DMAContinuousRequests = ENABLE;
    hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
    hadc1.Init.SamplingTimeCommon1 = ADC_SAMPLETIME_12CYCLES_5;
    hadc1.Init.SamplingTimeCommon2 = ADC_SAMPLETIME_12CYCLES_5;
    hadc1.Init.OversamplingMode = DISABLE;
    hadc1.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_HIGH;
    if (HAL_ADC_Init(&hadc1) != HAL_OK)
        Error_Handler();

    /* Rank 1: PA0 (ZMPT101B) */
    ADC_ChannelConfTypeDef ch = {0};
    ch.Channel = ADC_CHANNEL_0;
    ch.Rank = ADC_REGULAR_RANK_1;
    ch.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
    if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK)
        Error_Handler();

    /* Rank 2: PA1 (MQ-2 AO) */
    ch.Channel = ADC_CHANNEL_1;
    ch.Rank = ADC_REGULAR_RANK_2;
    ch.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
    if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK)
        Error_Handler();

    HAL_ADCEx_Calibration_Start(&hadc1);

    /* TIM3 TRGO trigger at 12.8 kHz */
    TIM_MasterConfigTypeDef m = {0};
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 0;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = (SYSCLK_HZ / ACQ_SAMPLE_RATE_HZ) - 1U; /* 4999 */
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
        Error_Handler();
    m.MasterOutputTrigger = TIM_TRGO_UPDATE;
    m.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &m) != HAL_OK)
        Error_Handler();

    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_dma_buf, 2U * FRAME_LEN);
    HAL_TIM_Base_Start(&htim3);
}

static void timer14_init(void) {
    __HAL_RCC_TIM14_CLK_ENABLE();
    htim14.Instance = TIM14;
    htim14.Init.Prescaler = (SYSCLK_HZ / 1000000UL) - 1U; /* 1 us ticks */
    htim14.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim14.Init.Period = 0xFFFFU;
    htim14.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim14.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim14) != HAL_OK)
        Error_Handler();
    HAL_TIM_Base_Start(&htim14);
}

static void check_reset_cause(void) {
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST))
        reset_cause = "IWDG";
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST))
        reset_cause = "WWDG";
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST))
        reset_cause = "PIN";
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PWRRST))
        reset_cause = "POWER";
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_OBLRST))
        reset_cause = "OBL";
    __HAL_RCC_CLEAR_RESET_FLAGS();
}

#if WATCHDOG_ENABLED
static void watchdog_init(void) {
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_64; /* ~32 kHz / 64 = 500 Hz */
    hiwdg.Init.Reload = 1000;                 /* ~2 seconds timeout    */
    hiwdg.Init.Window = IWDG_WINDOW_DISABLE;
    if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
        Error_Handler();
}
#endif

void board_init(void) {
    HAL_Init();
    clock_init();
    check_reset_cause();
    gpio_init();
    timer14_init();
    uart_init();
    acq_init();
#if WATCHDOG_ENABLED
    watchdog_init();
#endif
}

void Error_Handler(void) {
    __disable_irq();
    while (1) {
    }
}
