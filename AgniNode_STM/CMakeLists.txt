cmake_minimum_required(VERSION 3.20)

project(stm32g0b1_fire_detector LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(MCU_FLAGS
    -mcpu=cortex-m0plus
    -mthumb
    -mfloat-abi=soft
)

set(HAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32G0xx_HAL_Driver")
set(CMSIS_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Include")
set(CMSIS_DEVICE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32G0xx/Include")

add_executable(fire_detector
    src/main.c
    src/actuators.c
    src/analysis.c
    src/board.c
    src/comms.c
    src/fire_logic.c
    src/protection.c
    src/sensors.c
    Core/Src/system_stm32g0xx.c
    Core/Startup/startup_stm32g0b1retx.s
    "${HAL_DIR}/Src/stm32g0xx_hal.c"
    "${HAL_DIR}/Src/stm32g0xx_hal_adc.c"
    "${HAL_DIR}/Src/stm32g0xx_hal_adc_ex.c"
    "${HAL_DIR}/Src/stm32g0xx_hal_cortex.c"
    "${HAL_DIR}/Src/stm32g0xx_hal_dma.c"
    "${HAL_DIR}/Src/stm32g0xx_hal_exti.c"
    "${HAL_DIR}/Src/stm32g0xx_hal_flash.c"
    "${HAL_DIR}/Src/stm32g0xx_hal_flash_ex.c"
    "${HAL_DIR}/Src/stm32g0xx_hal_gpio.c"
    "${HAL_DIR}/Src/stm32g0xx_hal_i2c.c"
    "${HAL_DIR}/Src/stm32g0xx_hal_i2c_ex.c"
    "${HAL_DIR}/Src/stm32g0xx_hal_iwdg.c"
    "${HAL_DIR}/Src/stm32g0xx_hal_pwr.c"
    "${HAL_DIR}/Src/stm32g0xx_hal_pwr_ex.c"
    "${HAL_DIR}/Src/stm32g0xx_hal_rcc.c"
    "${HAL_DIR}/Src/stm32g0xx_hal_rcc_ex.c"
    "${HAL_DIR}/Src/stm32g0xx_hal_tim.c"
    "${HAL_DIR}/Src/stm32g0xx_hal_tim_ex.c"
    "${HAL_DIR}/Src/stm32g0xx_hal_uart.c"
    "${HAL_DIR}/Src/stm32g0xx_hal_uart_ex.c"
)

target_include_directories(fire_detector PRIVATE
    Core/Inc
    include
    "${HAL_DIR}/Inc"
    "${CMSIS_CORE_DIR}"
    "${CMSIS_DEVICE_DIR}"
)

target_compile_definitions(fire_detector PRIVATE
    USE_HAL_DRIVER
    STM32G0B1xx
)

target_compile_options(fire_detector PRIVATE
    ${MCU_FLAGS}
    -ffunction-sections
    -fdata-sections
    $<$<COMPILE_LANGUAGE:C>:-Wall>
    $<$<COMPILE_LANGUAGE:C>:-Wextra>
    $<$<COMPILE_LANGUAGE:C>:-Wpedantic>
)

target_link_options(fire_detector PRIVATE
    ${MCU_FLAGS}
    "--specs=nano.specs"
    "--specs=nosys.specs"
    "-T${CMAKE_CURRENT_SOURCE_DIR}/STM32G0B1RETx_FLASH.ld"
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/fire_detector.map"
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -u
    _printf_float
)

target_link_libraries(fire_detector PRIVATE m)
set_target_properties(fire_detector PROPERTIES SUFFIX ".elf")

add_custom_command(TARGET fire_detector POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O ihex
            "$<TARGET_FILE:fire_detector>"
            "${CMAKE_CURRENT_BINARY_DIR}/fire_detector.hex"
    COMMAND "${CMAKE_OBJCOPY}" -O binary
            "$<TARGET_FILE:fire_detector>"
            "${CMAKE_CURRENT_BINARY_DIR}/fire_detector.bin"
    COMMENT "Generating Intel HEX and raw binary images"
    VERBATIM
)
