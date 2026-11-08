# AgniNode STM32G0B1RE firmware

The STM32G0B1RE safety MCU firmware uses STM32Cube HAL, CMSIS, and the
STM32G0B1RETx linker layout (512 KB Flash / 144 KB RAM). Application modules
are in `src/` and their interfaces and board configuration are in `include/`.

## Build

With PlatformIO Core installed:

```powershell
platformio run
```

The project also supports the CMake preset, using CMake, Ninja, and the Arm GNU
Toolchain:

```powershell
cmake --preset default
cmake --build build
```

Build outputs include `fire_detector.elf`, `fire_detector.hex`, and
`fire_detector.bin`.

The PlatformIO upload command uses OpenOCD's native ST-LINK SWD transport,
which supports ST-LINK/V2 with the installed OpenOCD 12 package. Upload with:

```powershell
platformio run --environment agninode_g0b1 --target upload
```

## Configuration and hardware

- The STM32 firmware has no Wi-Fi or Firebase client. Those credentials belong
  to the ESP32 companion project, in its ignored local `include/secrets.h`.
- Pin assignments, thresholds, and peripheral setup are in `include/config.h`
  and `src/board.c`; calibrate them for the actual sensors and wiring.
- `MAINS_AUTO_CLOSE` is enabled: after its boot delay and self-check, firmware
  may assert the mains SSR output. `PB0` controls the suppression SSR. Keep
  mains and suppression hardware disconnected during initial verification, and
  have the system reviewed and tested on the exact board before deployment.
- The project targets the STM32G0B1RE (512 KB Flash). Confirm the exact MCU
  marking before flashing; the attached ST-LINK alone does not identify it.

The STM32 HAL, CMSIS, startup assembly, and linker script retain their
respective STMicroelectronics license files.

## Buzzer hardware test

For bench testing, connect a 3.3 V USB-to-UART adapter to the STM32 command
UART: adapter TX to PA10 (STM32 RX), adapter RX to PA9 (STM32 TX), and a
shared GND. Disconnect or isolate the ESP32 TX connection from PA10 while
using the adapter. Do not connect an RS-232-level adapter directly to MCU pins.

Install pySerial and run the test from PowerShell, replacing `COM5` with the
adapter's COM port:

```powershell
py -m pip install pyserial
py .\buzzer_test.py COM5
```

The script toggles the buzzer for two seconds on and two seconds off, checking
the STM32 ACK for each command. Stop with Ctrl+C; it attempts to send a final
OFF command. The ON request also has a bounded firmware timeout in case the PC
disconnects. Optional settings include `--on-seconds 1`, `--off-seconds 3`,
and `--cycles 5`. This test uses the normal firmware command path and does not
bypass the fire-alarm latch; do not use it as a substitute for safety testing.

For a direct GPIO test that does not use UART, the standalone C++ source
`src/buzzer_pin_test.cpp` configures PA6 and holds it HIGH. Build and flash it
with the dedicated PlatformIO environment:

```powershell
platformio run --environment buzzer_pin_test --target upload
```

The default CMake and PlatformIO environments still build the production
firmware. The test holds the buzzer on until the MCU is reset or power is
removed.
