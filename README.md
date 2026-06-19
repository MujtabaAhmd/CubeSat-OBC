# CubeSat OBC Firmware -- High-Level Architecture
## NSTechnologies / Team Strive | Engineering Model

---
v
## Hardware Target
- **MCU**: STM32F407VGTx (STM32F4 Discovery board)
- **RTOS**: FreeRTOS

---

## Directory Structure

```
cubesat_obc/
├── Config/
│   └── obc_config.h          -- Central config: pins, timings, thresholds
├── Core/
│   └── Src/main.c            -- Entry point, peripheral init, scheduler start
├── Scheduler/
│   ├── sat_modes.h/.c        -- Mode state machine
│   └── scheduler.h/.c        -- Task creation and mode-driven activation
├── Drivers/
│   ├── Sensors/
│   │   ├── MPU6050/          -- Gyro + accelerometer (I2C)
│   │   ├── HMC5883L/         -- Magnetometer (I2C)
│   │   └── SunSensor/        -- Solar panel ADC coarse sun sensing
│   ├── EPS/                  -- EPS UART driver
│   └── Comms/                -- AX.25 controller SPI driver
└── Apps/
    ├── ADCS_App/             -- B-dot detumble + nadir pointing stub
    ├── Housekeeping/         -- Telemetry collection and health checks
    ├── FDIR/                 -- Fault detection and recovery
    ├── app_headers_combined.h -- Comms, TC, Payload, DataMgmt headers
    └── app_implementations.c  -- Comms, TC, Payload, DataMgmt tasks
```

---

## Bus Assignments

| Bus    | Peripheral           | Notes                              |
|--------|----------------------|------------------------------------|
| I2C1   | MPU6050, HMC5883L    | 400 kHz fast mode                  |
| SPI1   | Comms controller     | AX.25, CS on PA4                   |
| SPI2   | Payload controller   | TBD protocol, CS on PB12           |
| SPI3   | SD card (FatFS)      | CS on PB5                          |
| USART2 | EPS                  | 9600 baud, 8N1                     |
| ADC1   | Sun sensors          | 6 channels (one per panel face)    |

---

## Operating Modes and Active Tasks

| Mode           | HK | ADCS | Comms | TC | Payload | DataMgmt | FDIR |
|----------------|----|------|-------|----|---------|----------|------|
| ACTIVATION     | ✓  | ✓    |       |    |         |          |      |
| IMAGE_CAPTURE  | ✓  | ✓    |       |    | ✓       |          |      |
| DOWNLINK       | ✓  |      | ✓     | ✓  |         | ✓        |      |
| FAILSAFE       | ✓  |      | ✓     |    |         |          | ✓    |

---

## FreeRTOS Task Priorities (higher = more urgent)

| Priority | Task        |
|----------|-------------|
| 6        | Scheduler   |
| 5        | FDIR        |
| 4        | Housekeeping|
| 3        | ADCS, Comms, Telecommand |
| 2        | Payload, Data Management |

---

## Mode Transition Triggers

```
ACTIVATION  --[CAPTURE_WINDOW + DETUMBLE_DONE]--> IMAGE_CAPTURE
ACTIVATION  --[LINK_ESTABLISHED]--------------> DOWNLINK
IMAGE_CAPTURE --[CAPTURE_DONE]----------------> ACTIVATION
DOWNLINK    --[DOWNLINK_DONE / LINK_LOST]------> ACTIVATION
ANY         --[FAULT_DETECTED]----------------> FAILSAFE
FAILSAFE    --[FAULT_CLEARED]-----------------> ACTIVATION
ANY         --[GND_CMD_*]---------------------> respective mode
```

---

## ADCS Algorithm

**Detumble (ACTIVATION mode)**
- Algorithm: B-dot controller
  - Dipole command: `m = -k * dB/dt`
  - `k = ADCS_BDOT_GAIN` (1e4 A·m²/(T/s) -- tune empirically)
- Sensors: HMC5883L (B), MPU6050 (ω for verification)
- Completion: angular rate magnitude < `ADCS_DETUMBLE_THRESHOLD_DEG_S` (2 deg/s)

**Nadir Pointing (IMAGE_CAPTURE mode)**
- Stub implemented -- full controller deferred to CDR phase
- Requires: attitude estimator (MEKF or TRIAD), orbit propagator, actuator driver

---

## Inter-Task Communication

| Queue            | Producer        | Consumer        | Item type      |
|------------------|-----------------|-----------------|----------------|
| `g_hk_queue`     | Housekeeping    | Data Management | `HK_Frame_t`   |
| `g_downlink_queue`| Data Management| Comms App       | `DownlinkFrame_t` |

Fault flags (`g_fault_register`) are written atomically via
`taskENTER_CRITICAL()` and read by any task.

---

## What is NOT Implemented (Deferred to CDR/FM)

1. **Nadir pointing controller** -- needs actuator selection and orbit propagator
2. **Magnetorquer driver** -- actuator TBD
3. **Exact EPS packet format** -- must match EPS firmware protocol
4. **Exact comms controller SPI protocol** -- must match comms controller spec
5. **Payload SPI protocol** -- payload controller TBD
6. **Orbit determination** -- SGP4 or TLE-based lookup for mode trigger
7. **ADCS sensor fusion** -- MEKF or TRIAD for attitude quaternion
8. **Telecommand authentication** -- HMAC or sequence counter anti-replay
9. **FatFS integration** -- add via CubeMX middleware, init in main()
10. **Watchdog** -- add IWDG init in main(), kick from scheduler task

---

## Integration Checklist

- [ ] Generate peripheral init code in STM32CubeMX for your pin assignments
- [ ] Enable FreeRTOS in CubeMX (use CMSIS-RTOS v2 wrapper or bare FreeRTOS)
- [ ] Enable FatFS middleware in CubeMX linked to SPI3
- [ ] Add FreeRTOS heap size: suggest `configTOTAL_HEAP_SIZE = 32768` for F407
- [ ] Verify I2C pull-up resistors (4.7 kΩ recommended for 400 kHz)
- [ ] Run `MPU6050_Calibrate()` once on stable ground before flight
- [ ] Run `HMC5883L_SetHardIronOffset()` after full satellite assembly
- [ ] Verify SPI CPOL/CPHA settings against comms and payload controller datasheets
- [ ] Set EPS UART baud rate to match EPS firmware
- [ ] Confirm ADC channel-to-pin mapping for sun sensors

---

## Known Assumptions (verify before CDR)

- MPU6050 AD0 pin = GND (I2C address 0x68)
- HMC5883L at fixed address 0x1E
- SPI Mode 0 (CPOL=0, CPHA=0) for all SPI peripherals
- EPS sends a 16-byte packet with XOR checksum
- Sun sensor eclipse threshold of 100 ADC counts (12-bit, 0-4095)
- Detumble completion at 2 deg/s angular rate magnitude
