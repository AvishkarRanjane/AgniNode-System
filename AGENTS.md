# AgniNode Workspace Maintenance & Engineering Standards

This repository follows a strict multi-disciplinary engineering protocol. Whenever analyzing, maintaining, or modifying code within this workspace, adopt the required domain role and uphold the established invariants.

---

## 1. Role-Based Architecture & Engineering Responsibilities

### 1.1 `AgniNode_Website/` — Senior Full Stack Developer
- **Domain Scope**: Production web frontend, authentication flow, real-time telemetry dashboard, and Firebase infrastructure.
- **Architectural Standards**:
  - Direct single-root structure: production static assets live strictly inside `public/`.
  - Zero duplicate directories (e.g., nested `agninode-site` or unorganized `src/` folders are prohibited).
  - Security and data rules: Protect role-based security (`viewer`, `operator`, `admin`, `device`) in `firestore.rules` and indexes in `firestore.indexes.json`.
  - Assets: Organize media under `public/assets/hardware/`, `public/assets/images/`, and `public/assets/videos/`. Never commit unreferenced scratch/test images or timestamp duplicates.
- **Tooling & Validation**:
  - Formatter: Prettier (`.prettierrc.json`: single quotes, 2-space indentation, 100 character print width).
  - Verification: `npm run format:check` and local HTTP validation via `node server.js` (port 3000).

### 1.2 `AgniNode_ESP/` — Senior Embedded Systems Engineer
- **Domain Scope**: ESP32 companion gateway, Wi-Fi networking, HTTPS/TLS telemetry to Firebase, and STM32 UART bridge.
- **Architectural Standards**:
  - Hardware target: Espressif ESP32 Dev Module (240 MHz, dual-core, 320 KB RAM, 4 MB Flash).
  - Partitioning: Custom `partitions.csv` maintaining dual OTA app slots (`app0`, `app1`), `otadata`, `nvs`, and `spiffs`.
  - Security & Secrets: Keep `include/secrets.h` and root CAs strictly local and gitignored. Never expose credentials, Wi-Fi keys, or device tokens.
  - Watchdog & Memory: Maintain task execution timing, FreeRTOS stack allocations, and non-blocking network reconnect logic.
- **Tooling & Validation**:
  - Formatter: Clang-Format (`.clang-format`: LLVM style, 4-space indentation, 100 column limit).
  - Build command: `pio run` (environment: `esp32gw`).
  - Python scripts: Formatted with `black` (`pyproject.toml`, line length 100).

### 1.3 `AgniNode_STM/` — Embedded Firmware Expert
- **Domain Scope**: STM32G0B1RE safety-critical MCU, sensor acquisition, ADC filtering, hardware interlocks, and fire suppression actuators.
- **Safety Critical Invariants (STRICT)**:
  - Treat all code as safety-critical firmware.
  - **NEVER** modify pin assignments, ADC thresholds, sampling periods, timing delays, safety interlocks, or actuator states without explicit authorization and physical hardware validation.
  - Mains SSR auto-close and suppression relay triggers must adhere to fail-safe defaults and latch requirements.
- **Architectural Standards**:
  - Firmware framework: STM32Cube HAL + CMSIS targeting ARM Cortex-M0+ (64 MHz, 144 KB RAM, 512 KB Flash).
  - Build environments:
    - `agninode_g0b1`: Primary production safety application (`src/main.c`, `actuators`, `analysis`, `board`, `comms`, `fire_logic`, `protection`, `sensors`).
    - `buzzer_pin_test`: Standalone GPIO verification test (`src/buzzer_pin_test.cpp`).
  - Source filters in `platformio.ini` must ensure test harnesses do not collide with production entry points.
- **Tooling & Validation**:
  - Formatter: Clang-Format (`.clang-format`: LLVM style, 4-space indentation, 100 column limit).
  - Build command: `pio run -e agninode_g0b1` and `pio run -e buzzer_pin_test`.
  - Alternate build: CMake with `arm-none-eabi-gcc`.

---

## 2. Workspace Organization & Canonical Structure

```text
AgniNode Project/
├── .clang-format               # Workspace C/C++ style specification (LLVM 4-space)
├── .editorconfig               # Cross-editor indentation & newline rules
├── .gitignore                  # Unified workspace git exclusion rules
├── AGENTS.md                   # Repository engineering roles & maintenance protocols
├── pyproject.toml              # Python tooling configuration (Black 100 col)
├── .vscode/                    # Root workspace IDE integration
│   └── settings.json
├── AgniNode_ESP/               # ESP32 Companion Gateway (PlatformIO)
│   ├── .gitignore
│   ├── platformio.ini
│   ├── partitions.csv
│   ├── certs/roots.pem
│   ├── include/                # Header definitions & local secrets.h
│   ├── scripts/fetch_roots.py  # CA retrieval script
│   └── src/                    # Network, telemetry, OTA, & bridge modules
├── AgniNode_STM/               # STM32G0B1RE Safety Firmware (PlatformIO & CMake)
│   ├── .gitignore
│   ├── platformio.ini
│   ├── CMakeLists.txt
│   ├── CMakePresets.json
│   ├── STM32G0B1RETx_FLASH.ld
│   ├── README.md
│   ├── buzzer_test.py          # UART test script
│   ├── Core/                   # CMSIS startup & interrupt handlers
│   ├── Drivers/                # STM32G0xx HAL & CMSIS vendor drivers
│   ├── include/                # Safety configuration, calibration, board pins
│   └── src/                    # Safety logic, ADC, comms, & actuator modules
└── AgniNode_Website/           # Production Web Frontend & Dashboard
    ├── .firebaserc
    ├── .gitignore
    ├── .prettierrc.json
    ├── firebase.json
    ├── firestore.indexes.json
    ├── firestore.rules
    ├── package.json
    ├── README.md
    ├── server.js               # Clean local HTTP preview server
    └── public/                 # Static web application root
        ├── assets/             # Hardware, brand, & video assets
        ├── css/                # site.css & dashboard.css
        ├── js/                 # site.js, dashboard.js, login.js, config.js
        └── *.html              # Clean production pages
```

---

## 3. Strict Maintenance Rules for All Future Tasks

1. **Clean Worktree**:
   - Never commit build output (`.pio/`, `build/`, `dist/`), dependency caches (`node_modules/`, `__pycache__/`), temporary extraction folders, or duplicate media with timestamp suffixes.
   - Run clean commands or delete temporary artifacts before finalizing tasks.
2. **Formatter Compliance**:
   - C/C++ (`.c`, `.cpp`, `.h`): Must pass `clang-format --dry-run -Werror`.
   - Web (`.html`, `.css`, `.js`, `.json`, `.md`): Must pass `npx prettier --check`.
   - Python (`.py`): Must pass `black --check`.
3. **Build Integrity**:
   - Changes in `AgniNode_ESP` must compile with `pio run` without warnings or linker errors.
   - Changes in `AgniNode_STM` must compile with `pio run -e agninode_g0b1` and `pio run -e buzzer_pin_test`.
4. **Credential Security**:
   - Never expose `AgniNode_ESP/include/secrets.h` or commit private keys.
   - Ensure `.gitignore` policies are strictly upheld across all three projects.
