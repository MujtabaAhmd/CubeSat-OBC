/**
 * @file    comms_app.c
 * @brief   Communications Application implementation
 */

#include "comms_app.h"
#include "comms_driver.h"
#include "sat_modes.h"
#include "fdir_app.h"
#include "obc_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

QueueHandle_t g_downlink_queue = NULL;

static uint32_t g_last_link_tick     = 0;
static uint8_t  g_beacon_buf[]       = "BEACON:NSVS-CUBESAT-EM\r\n";

void Comms_Task(void *pvParameters)
{
    (void)pvParameters;

    if (g_downlink_queue == NULL) {
        g_downlink_queue = xQueueCreate(DOWNLINK_QUEUE_LENGTH,
                                         sizeof(DownlinkFrame_t));
        configASSERT(g_downlink_queue != NULL);
    }

    COMMS_Status_t_Info status = {0};
    DownlinkFrame_t frame      = {0};
    SatelliteMode_t mode;

    for (;;) {
        mode = ModeSM_GetMode();

        /* --------------------------------------------------
         * 1. Check link status
         * -------------------------------------------------- */
        COMMS_GetStatus(&COMMS_SPI_HANDLE, &status);

        if (status.link_active) {
            g_last_link_tick = xTaskGetTickCount();
            ModeSM_SetTrigger(TRIGGER_LINK_ESTABLISHED);
        } else {
            uint32_t now = xTaskGetTickCount();
            if ((now - g_last_link_tick) > pdMS_TO_TICKS(FAULT_COMMS_TIMEOUT_MS)) {
                ModeSM_SetTrigger(TRIGGER_LINK_LOST);
                if (mode != SAT_MODE_FAILSAFE) {
                    FDIR_RaiseFault(FAULT_FLAG_COMMS_TIMEOUT);
                }
            }
        }

        /* --------------------------------------------------
         * 2. In FAILSAFE: transmit emergency beacon
         * -------------------------------------------------- */
        if (mode == SAT_MODE_FAILSAFE) {
            COMMS_SendDownlink(&COMMS_SPI_HANDLE,
                               g_beacon_buf,
                               sizeof(g_beacon_buf) - 1U);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        /* --------------------------------------------------
         * 3. In DOWNLINK: drain downlink queue
         * -------------------------------------------------- */
        while (xQueueReceive(g_downlink_queue, &frame, 0) == pdTRUE) {
            COMMS_SendDownlink(&COMMS_SPI_HANDLE, frame.data, frame.len);
        }

        vTaskDelay(pdMS_TO_TICKS(COMMS_POLL_INTERVAL_MS));
    }
}


/* ================================================================ */


/**
 * @file    telecommand_app.c
 */

#include "telecommand_app.h"
#include "comms_driver.h"
#include "sat_modes.h"
#include "adcs_app.h"
#include "fdir_app.h"
#include "obc_config.h"
#include "FreeRTOS.h"
#include "task.h"

/* Include the header definitions from combined header */
#define TC_CMD_SET_MODE         0x01
#define TC_CMD_TRIGGER_PAYLOAD  0x02
#define TC_CMD_CLEAR_FAULT      0x03
#define TC_CMD_SET_ADCS_MODE    0x04
#define TC_CMD_REQUEST_HK       0x05

void Telecommand_Task(void *pvParameters)
{
    (void)pvParameters;

    uint8_t  rx_buf[64];
    uint16_t rx_len = 0;

    for (;;) {
        COMMS_ReceiveUplink(&COMMS_SPI_HANDLE, rx_buf,
                             sizeof(rx_buf), &rx_len);

        if (rx_len < 2) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        uint8_t opcode = rx_buf[0];
        uint8_t arg    = rx_buf[1];

        switch (opcode) {
            case TC_CMD_SET_MODE:
                if (arg < SAT_MODE_COUNT) {
                    ModeSM_ForceMode((SatelliteMode_t)arg);
                }
                break;

            case TC_CMD_CLEAR_FAULT:
                FDIR_ClearFault(arg);
                break;

            case TC_CMD_SET_ADCS_MODE:
                ADCS_SetMode((ADCS_ControlMode_t)arg);
                break;

            case TC_CMD_TRIGGER_PAYLOAD:
                ModeSM_SetTrigger(TRIGGER_GND_CMD_CAPTURE);
                break;

            case TC_CMD_REQUEST_HK:
                /* Housekeeping app will push next frame to downlink queue */
                break;

            default:
                /* Unknown command -- log and ignore */
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}


/* ================================================================ */


/**
 * @file    payload_app.c
 */

#include "payload_app.h"
#include "sat_modes.h"
#include "obc_config.h"
#include "FreeRTOS.h"
#include "task.h"

#define PAYLOAD_CMD_CAPTURE   0xC1
#define PAYLOAD_CMD_STATUS    0xC2
#define PAYLOAD_CMD_RESET     0xC3
#define PAYLOAD_STATUS_IDLE   0x00
#define PAYLOAD_STATUS_BUSY   0x01
#define PAYLOAD_STATUS_DONE   0x02
#define PAYLOAD_STATUS_ERROR  0xFF

#define PAYLOAD_CAPTURE_TIMEOUT_MS   10000U

static void _cs_low(void)
{
    HAL_GPIO_WritePin(PAYLOAD_CS_PORT, PAYLOAD_CS_PIN, GPIO_PIN_RESET);
}
static void _cs_high(void)
{
    HAL_GPIO_WritePin(PAYLOAD_CS_PORT, PAYLOAD_CS_PIN, GPIO_PIN_SET);
}

void Payload_Task(void *pvParameters)
{
    (void)pvParameters;

    uint8_t cmd;
    uint8_t status_byte;
    uint32_t capture_start;

    for (;;) {
        /* --------------------------------------------------
         * 1. Trigger image capture
         * -------------------------------------------------- */
        cmd = PAYLOAD_CMD_CAPTURE;
        _cs_low();
        HAL_SPI_Transmit(&PAYLOAD_SPI_HANDLE, &cmd, 1, 50);
        _cs_high();

        /* --------------------------------------------------
         * 2. Poll for completion (up to timeout)
         * -------------------------------------------------- */
        capture_start = xTaskGetTickCount();
        status_byte   = PAYLOAD_STATUS_BUSY;

        while (status_byte == PAYLOAD_STATUS_BUSY) {
            vTaskDelay(pdMS_TO_TICKS(200));

            cmd = PAYLOAD_CMD_STATUS;
            _cs_low();
            HAL_SPI_TransmitReceive(&PAYLOAD_SPI_HANDLE,
                                    &cmd, &status_byte, 1, 50);
            _cs_high();

            if ((xTaskGetTickCount() - capture_start) >
                pdMS_TO_TICKS(PAYLOAD_CAPTURE_TIMEOUT_MS)) {
                status_byte = PAYLOAD_STATUS_ERROR;
                break;
            }
        }

        /* --------------------------------------------------
         * 3. Handle result
         * -------------------------------------------------- */
        if (status_byte == PAYLOAD_STATUS_DONE) {
            ModeSM_SetTrigger(TRIGGER_CAPTURE_DONE);
        } else {
            /* Error or timeout -- log, then also signal done
             * so satellite doesn't stay stuck in IMAGE_CAPTURE */
            ModeSM_SetTrigger(TRIGGER_CAPTURE_DONE);
        }

        /* Task will be suspended by scheduler after CAPTURE_DONE
         * is processed. Add a short delay as a safety net. */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


/* ================================================================ */


/**
 * @file    datamgmt_app.c
 * @brief   Data Management Application implementation
 *
 * Uses FatFS for SD card access. FatFS must be initialised in
 * main() before vTaskStartScheduler() is called.
 *
 * File naming: HK frames -> "HK_<timestamp>.bin"
 * Image data is managed by payload controller; OBC logs metadata.
 */

#include "datamgmt_app.h"
#include "housekeeping_app.h"
#include "comms_app.h"
#include "fdir_app.h"
#include "obc_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/*
 * FatFS include.
 * Ensure FatFS is added to your STM32CubeMX project and
 * ff.h is on the include path.
 */
#include "fatfs.h"

#define DM_HK_FILENAME     "hk_log.bin"
#define DM_MAX_WRITE_LEN   sizeof(HK_Frame_t)

void DataMgmt_Task(void *pvParameters)
{
    (void)pvParameters;

    HK_Frame_t     hk_frame    = {0};
    DownlinkFrame_t dl_frame   = {0};
    FRESULT        fr;
    FIL            fil;
    UINT           bw;

    for (;;) {
        /* --------------------------------------------------
         * 1. Drain HK queue -- write to SD and queue for DL
         * -------------------------------------------------- */
        while (xQueueReceive(g_hk_queue, &hk_frame, 0) == pdTRUE) {

            /* Write HK frame to SD card */
            fr = f_open(&fil, DM_HK_FILENAME,
                        FA_WRITE | FA_OPEN_APPEND);
            if (fr == FR_OK) {
                f_write(&fil, &hk_frame, sizeof(HK_Frame_t), &bw);
                f_close(&fil);
            } else {
                FDIR_RaiseFault(FAULT_FLAG_SD_ERROR);
            }

            /* Also pack into downlink frame */
            if (sizeof(HK_Frame_t) <= DOWNLINK_FRAME_MAX_LEN) {
                dl_frame.len = sizeof(HK_Frame_t);
                __builtin_memcpy(dl_frame.data, &hk_frame,
                                 sizeof(HK_Frame_t));
                xQueueSend(g_downlink_queue, &dl_frame,
                           pdMS_TO_TICKS(10));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
