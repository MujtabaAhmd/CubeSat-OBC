/**
 * @file    sat_modes.h
 * @brief   Satellite operating mode definitions and state machine
 */

#ifndef SAT_MODES_H
#define SAT_MODES_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "semphr.h"

/* =========================================================
 * SATELLITE OPERATING MODES
 * ========================================================= */
typedef enum {
    SAT_MODE_ACTIVATION       = 0,
    SAT_MODE_IMAGE_CAPTURE    = 1,
    SAT_MODE_DOWNLINK         = 2,
    SAT_MODE_FAILSAFE         = 3,
    SAT_MODE_COUNT                   /* Always last -- used for bounds checks */
} SatelliteMode_t;

/* Human-readable names -- useful for telemetry logging */
static const char * const SAT_MODE_NAMES[] = {
    "ACTIVATION",
    "IMAGE_CAPTURE",
    "DOWNLINK",
    "FAILSAFE"
};

/* =========================================================
 * MODE TRANSITION TRIGGERS
 * ========================================================= */
typedef enum {
    TRIGGER_NONE             = 0,
    TRIGGER_DETUMBLE_DONE    = (1 << 0),  /* ADCS reports stable attitude */
    TRIGGER_LINK_ESTABLISHED = (1 << 1),  /* Comms app confirmed GS link  */
    TRIGGER_LINK_LOST        = (1 << 2),
    TRIGGER_CAPTURE_WINDOW   = (1 << 3),  /* Orbit det. says capture zone  */
    TRIGGER_CAPTURE_DONE     = (1 << 4),  /* Payload app done              */
    TRIGGER_DOWNLINK_DONE    = (1 << 5),
    TRIGGER_FAULT_DETECTED   = (1 << 6),  /* FDIR or Housekeeping          */
    TRIGGER_FAULT_CLEARED    = (1 << 7),
    TRIGGER_GND_CMD_CAPTURE  = (1 << 8),  /* Ground command override       */
    TRIGGER_GND_CMD_DOWNLINK = (1 << 9),
    TRIGGER_GND_CMD_SAFE     = (1 << 10),
} ModeTrigger_t;

/* =========================================================
 * MODE STATE MACHINE CONTEXT
 * ========================================================= */
typedef struct {
    SatelliteMode_t  current_mode;
    SatelliteMode_t  previous_mode;
    uint32_t         mode_entry_tick;    /* xTaskGetTickCount() at entry  */
    uint32_t         trigger_flags;      /* Bitmask of active triggers    */
    SemaphoreHandle_t lock;             /* Protects concurrent access    */
} ModeSM_t;

/* =========================================================
 * PUBLIC API
 * ========================================================= */

/**
 * @brief  Initialise the mode state machine. Call once before
 *         starting the FreeRTOS scheduler.
 */
void     ModeSM_Init(void);

/**
 * @brief  Returns the current satellite mode.
 *         Thread-safe (acquires internal mutex).
 */
SatelliteMode_t ModeSM_GetMode(void);

/**
 * @brief  Set a trigger flag. The scheduler task evaluates
 *         flags and performs transitions.
 *         Thread-safe.
 */
void     ModeSM_SetTrigger(ModeTrigger_t trigger);

/**
 * @brief  Clear a trigger flag.
 *         Thread-safe.
 */
void     ModeSM_ClearTrigger(ModeTrigger_t trigger);

/**
 * @brief  Evaluate pending triggers and perform mode transition
 *         if conditions are met. Called by the scheduler task.
 */
void     ModeSM_Update(void);

/**
 * @brief  Force an immediate mode transition. Use only from
 *         FDIR or ground command handling.
 */
void     ModeSM_ForceMode(SatelliteMode_t new_mode);

#endif /* SAT_MODES_H */
