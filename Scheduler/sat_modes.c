/**
 * @file    sat_modes.c
 * @brief   Satellite mode state machine implementation
 *
 * Transition table (current -> trigger -> next):
 *
 *  ACTIVATION    + DETUMBLE_DONE                   -> ACTIVATION (stays, systems init)
 *  ACTIVATION    + CAPTURE_WINDOW + DETUMBLE_DONE  -> IMAGE_CAPTURE
 *  ACTIVATION    + LINK_ESTABLISHED                -> DOWNLINK
 *  ANY           + FAULT_DETECTED                  -> FAILSAFE
 *  FAILSAFE      + FAULT_CLEARED                   -> ACTIVATION
 *  IMAGE_CAPTURE + CAPTURE_DONE                    -> ACTIVATION
 *  DOWNLINK      + DOWNLINK_DONE / LINK_LOST        -> ACTIVATION
 *  ANY           + GND_CMD_*                       -> respective mode
 *
 * Note: transition logic is intentionally simple for the EM phase.
 * Refine once orbit determination feeds real data.
 */

#include "sat_modes.h"
#include "obc_config.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include <string.h>

/* =========================================================
 * MODULE-PRIVATE STATE
 * ========================================================= */
static ModeSM_t g_sm;

/* =========================================================
 * INTERNAL HELPERS
 * ========================================================= */
static void _enter_mode(SatelliteMode_t new_mode)
{
    g_sm.previous_mode  = g_sm.current_mode;
    g_sm.current_mode   = new_mode;
    g_sm.mode_entry_tick = xTaskGetTickCount();

    /* Log transition -- replace with your telemetry logger */
    /* LOG_INFO("MODE: %s -> %s",
     *     SAT_MODE_NAMES[g_sm.previous_mode],
     *     SAT_MODE_NAMES[g_sm.current_mode]); */
}

static inline uint8_t _flag_set(ModeTrigger_t t)
{
    return (g_sm.trigger_flags & (uint32_t)t) != 0U;
}

/* =========================================================
 * PUBLIC API IMPLEMENTATION
 * ========================================================= */
void ModeSM_Init(void)
{
    memset(&g_sm, 0, sizeof(g_sm));
    g_sm.current_mode   = SAT_MODE_ACTIVATION;
    g_sm.previous_mode  = SAT_MODE_ACTIVATION;
    g_sm.lock           = xSemaphoreCreateMutex();
    configASSERT(g_sm.lock != NULL);
}

SatelliteMode_t ModeSM_GetMode(void)
{
    SatelliteMode_t mode;
    xSemaphoreTake(g_sm.lock, portMAX_DELAY);
    mode = g_sm.current_mode;
    xSemaphoreGive(g_sm.lock);
    return mode;
}

void ModeSM_SetTrigger(ModeTrigger_t trigger)
{
    xSemaphoreTake(g_sm.lock, portMAX_DELAY);
    g_sm.trigger_flags |= (uint32_t)trigger;
    xSemaphoreGive(g_sm.lock);
}

void ModeSM_ClearTrigger(ModeTrigger_t trigger)
{
    xSemaphoreTake(g_sm.lock, portMAX_DELAY);
    g_sm.trigger_flags &= ~(uint32_t)trigger;
    xSemaphoreGive(g_sm.lock);
}

void ModeSM_ForceMode(SatelliteMode_t new_mode)
{
    xSemaphoreTake(g_sm.lock, portMAX_DELAY);
    _enter_mode(new_mode);
    g_sm.trigger_flags = 0U;
    xSemaphoreGive(g_sm.lock);
}

void ModeSM_Update(void)
{
    xSemaphoreTake(g_sm.lock, portMAX_DELAY);

    /* --------------------------------------------------
     * FAULT has highest priority -- any mode -> FAILSAFE
     * -------------------------------------------------- */
    if (_flag_set(TRIGGER_FAULT_DETECTED)) {
        if (g_sm.current_mode != SAT_MODE_FAILSAFE) {
            _enter_mode(SAT_MODE_FAILSAFE);
            g_sm.trigger_flags = 0U;
        }
        goto done;
    }

    /* --------------------------------------------------
     * Ground command overrides
     * -------------------------------------------------- */
    if (_flag_set(TRIGGER_GND_CMD_SAFE)) {
        _enter_mode(SAT_MODE_FAILSAFE);
        g_sm.trigger_flags = 0U;
        goto done;
    }
    if (_flag_set(TRIGGER_GND_CMD_CAPTURE)) {
        _enter_mode(SAT_MODE_IMAGE_CAPTURE);
        ModeSM_ClearTrigger(TRIGGER_GND_CMD_CAPTURE);
        goto done;
    }
    if (_flag_set(TRIGGER_GND_CMD_DOWNLINK)) {
        _enter_mode(SAT_MODE_DOWNLINK);
        ModeSM_ClearTrigger(TRIGGER_GND_CMD_DOWNLINK);
        goto done;
    }

    /* --------------------------------------------------
     * Mode-specific transitions
     * -------------------------------------------------- */
    switch (g_sm.current_mode) {

        case SAT_MODE_ACTIVATION:
            /* Orbit det. says capture window + ADCS stabilised */
            if (_flag_set(TRIGGER_CAPTURE_WINDOW) &&
                _flag_set(TRIGGER_DETUMBLE_DONE)) {
                _enter_mode(SAT_MODE_IMAGE_CAPTURE);
                ModeSM_ClearTrigger(TRIGGER_CAPTURE_WINDOW);
                ModeSM_ClearTrigger(TRIGGER_DETUMBLE_DONE);
            }
            /* Ground station link established */
            else if (_flag_set(TRIGGER_LINK_ESTABLISHED)) {
                _enter_mode(SAT_MODE_DOWNLINK);
                ModeSM_ClearTrigger(TRIGGER_LINK_ESTABLISHED);
            }
            break;

        case SAT_MODE_IMAGE_CAPTURE:
            if (_flag_set(TRIGGER_CAPTURE_DONE)) {
                _enter_mode(SAT_MODE_ACTIVATION);
                ModeSM_ClearTrigger(TRIGGER_CAPTURE_DONE);
            }
            break;

        case SAT_MODE_DOWNLINK:
            if (_flag_set(TRIGGER_DOWNLINK_DONE) ||
                _flag_set(TRIGGER_LINK_LOST)) {
                _enter_mode(SAT_MODE_ACTIVATION);
                ModeSM_ClearTrigger(TRIGGER_DOWNLINK_DONE);
                ModeSM_ClearTrigger(TRIGGER_LINK_LOST);
            }
            break;

        case SAT_MODE_FAILSAFE:
            if (_flag_set(TRIGGER_FAULT_CLEARED)) {
                _enter_mode(SAT_MODE_ACTIVATION);
                ModeSM_ClearTrigger(TRIGGER_FAULT_CLEARED);
            }
            break;

        default:
            /* Should never reach here */
            _enter_mode(SAT_MODE_FAILSAFE);
            break;
    }

done:
    xSemaphoreGive(g_sm.lock);
}
