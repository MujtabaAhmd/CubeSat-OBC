/**
 * @file    scheduler.h
 * @brief   OBC central task scheduler
 *
 * The scheduler task runs at the highest application priority.
 * It evaluates the mode state machine and activates/suspends
 * application tasks accordingly.
 *
 * All application task handles are declared here so the
 * scheduler can suspend/resume them on mode changes.
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "FreeRTOS.h"
#include "task.h"
#include "sat_modes.h"

/* =========================================================
 * APPLICATION TASK HANDLES
 * Populated during task creation in scheduler_init().
 * ========================================================= */
typedef struct {
    TaskHandle_t housekeeping;
    TaskHandle_t adcs;
    TaskHandle_t comms;
    TaskHandle_t telecommand;
    TaskHandle_t payload;
    TaskHandle_t data_mgmt;
    TaskHandle_t fdir;
} AppTaskHandles_t;

extern AppTaskHandles_t g_app_tasks;

/* =========================================================
 * PUBLIC API
 * ========================================================= */

/**
 * @brief  Create all application tasks and the scheduler task.
 *         Call once from main(), before vTaskStartScheduler().
 */
void Scheduler_Init(void);

/**
 * @brief  The scheduler FreeRTOS task function.
 *         Do not call directly -- passed to xTaskCreate.
 */
void Scheduler_Task(void *pvParameters);

#endif /* SCHEDULER_H */
