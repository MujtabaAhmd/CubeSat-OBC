/**
 * @file    scheduler.c
 * @brief   OBC central task scheduler implementation
 *
 * Task activation per mode:
 *
 *  ACTIVATION    : housekeeping, adcs
 *  IMAGE_CAPTURE : housekeeping, adcs, payload
 *  DOWNLINK      : housekeeping, comms, telecommand, data_mgmt
 *  FAILSAFE      : housekeeping, comms, fdir
 *
 * Tasks not listed for a given mode are suspended.
 * Suspended tasks do not consume CPU time but retain their state.
 */

#include "scheduler.h"
#include "obc_config.h"
#include "sat_modes.h"

/* Application task includes */
#include "housekeeping_app.h"
#include "adcs_app.h"
#include "comms_app.h"
#include "telecommand_app.h"
#include "payload_app.h"
#include "datamgmt_app.h"
#include "fdir_app.h"

#include "FreeRTOS.h"
#include "task.h"

/* =========================================================
 * GLOBAL TASK HANDLE TABLE
 * ========================================================= */
AppTaskHandles_t g_app_tasks = {0};

/* =========================================================
 * INTERNAL -- SUSPEND ALL APPLICATION TASKS
 * ========================================================= */
static void _suspend_all_app_tasks(void)
{
    if (g_app_tasks.housekeeping) vTaskSuspend(g_app_tasks.housekeeping);
    if (g_app_tasks.adcs)         vTaskSuspend(g_app_tasks.adcs);
    if (g_app_tasks.comms)        vTaskSuspend(g_app_tasks.comms);
    if (g_app_tasks.telecommand)  vTaskSuspend(g_app_tasks.telecommand);
    if (g_app_tasks.payload)      vTaskSuspend(g_app_tasks.payload);
    if (g_app_tasks.data_mgmt)    vTaskSuspend(g_app_tasks.data_mgmt);
    if (g_app_tasks.fdir)         vTaskSuspend(g_app_tasks.fdir);
}

/* =========================================================
 * INTERNAL -- ACTIVATE TASKS FOR EACH MODE
 * ========================================================= */
static void _activate_mode_tasks(SatelliteMode_t mode)
{
    _suspend_all_app_tasks();

    switch (mode) {

        case SAT_MODE_ACTIVATION:
            vTaskResume(g_app_tasks.housekeeping);
            vTaskResume(g_app_tasks.adcs);
            break;

        case SAT_MODE_IMAGE_CAPTURE:
            vTaskResume(g_app_tasks.housekeeping);
            vTaskResume(g_app_tasks.adcs);
            vTaskResume(g_app_tasks.payload);
            break;

        case SAT_MODE_DOWNLINK:
            vTaskResume(g_app_tasks.housekeeping);
            vTaskResume(g_app_tasks.comms);
            vTaskResume(g_app_tasks.telecommand);
            vTaskResume(g_app_tasks.data_mgmt);
            break;

        case SAT_MODE_FAILSAFE:
            vTaskResume(g_app_tasks.housekeeping);
            vTaskResume(g_app_tasks.comms);
            vTaskResume(g_app_tasks.fdir);
            break;

        default:
            /* Unknown mode -- go to failsafe */
            vTaskResume(g_app_tasks.housekeeping);
            vTaskResume(g_app_tasks.fdir);
            break;
    }
}

/* =========================================================
 * SCHEDULER TASK
 * ========================================================= */
void Scheduler_Task(void *pvParameters)
{
    (void)pvParameters;

    SatelliteMode_t last_mode = SAT_MODE_COUNT; /* Invalid sentinel */

    for (;;) {
        /* Evaluate any pending triggers */
        ModeSM_Update();

        SatelliteMode_t current_mode = ModeSM_GetMode();

        /* Only reconfigure tasks on actual mode change */
        if (current_mode != last_mode) {
            _activate_mode_tasks(current_mode);
            last_mode = current_mode;
        }

        /* Scheduler runs every 100 ms -- tune if needed */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* =========================================================
 * INITIALISATION -- CREATE ALL TASKS
 * ========================================================= */
void Scheduler_Init(void)
{
    /* Initialise mode state machine first */
    ModeSM_Init();

    /* Create application tasks -- all start suspended */

    xTaskCreate(Housekeeping_Task,
                "HK",
                STACK_HOUSEKEEPING,
                NULL,
                PRIORITY_HOUSEKEEPING,
                &g_app_tasks.housekeeping);
    vTaskSuspend(g_app_tasks.housekeeping);

    xTaskCreate(ADCS_Task,
                "ADCS",
                STACK_ADCS,
                NULL,
                PRIORITY_ADCS,
                &g_app_tasks.adcs);
    vTaskSuspend(g_app_tasks.adcs);

    xTaskCreate(Comms_Task,
                "COMMS",
                STACK_COMMS,
                NULL,
                PRIORITY_COMMS,
                &g_app_tasks.comms);
    vTaskSuspend(g_app_tasks.comms);

    xTaskCreate(Telecommand_Task,
                "TC",
                STACK_TELECOMMAND,
                NULL,
                PRIORITY_TELECOMMAND,
                &g_app_tasks.telecommand);
    vTaskSuspend(g_app_tasks.telecommand);

    xTaskCreate(Payload_Task,
                "PLD",
                STACK_PAYLOAD,
                NULL,
                PRIORITY_PAYLOAD,
                &g_app_tasks.payload);
    vTaskSuspend(g_app_tasks.payload);

    xTaskCreate(DataMgmt_Task,
                "DM",
                STACK_DATAMGMT,
                NULL,
                PRIORITY_DATAMGMT,
                &g_app_tasks.data_mgmt);
    vTaskSuspend(g_app_tasks.data_mgmt);

    xTaskCreate(FDIR_Task,
                "FDIR",
                STACK_FDIR,
                NULL,
                PRIORITY_FDIR,
                &g_app_tasks.fdir);
    vTaskSuspend(g_app_tasks.fdir);

    /* Create the scheduler task itself */
    xTaskCreate(Scheduler_Task,
                "SCHED",
                STACK_SCHEDULER,
                NULL,
                PRIORITY_SCHEDULER,
                NULL);

    /* Activate initial mode tasks manually (ACTIVATION mode) */
    _activate_mode_tasks(SAT_MODE_ACTIVATION);
}
