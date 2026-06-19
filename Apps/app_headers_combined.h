/**
 * @file    comms_app.h
 * @brief   Communications Application
 *
 * Active in: DOWNLINK, FAILSAFE
 *
 * Responsibilities:
 *   - Monitor link status via comms driver
 *   - Forward queued data frames to comms driver for downlink
 *   - Signal link establishment/loss to mode state machine
 *   - In FAILSAFE: transmit emergency beacon
 */

#ifndef COMMS_APP_H
#define COMMS_APP_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* Queue for data frames awaiting downlink.
 * Data management task fills this; comms app drains it. */
extern QueueHandle_t g_downlink_queue;

#define DOWNLINK_QUEUE_LENGTH   16U
#define DOWNLINK_FRAME_MAX_LEN  200U

typedef struct {
    uint8_t  data[DOWNLINK_FRAME_MAX_LEN];
    uint16_t len;
} DownlinkFrame_t;

void Comms_Task(void *pvParameters);

#endif /* COMMS_APP_H */


/* ================================================================ */


/**
 * @file    telecommand_app.h
 * @brief   Telecommand Handling Application
 *
 * Active in: DOWNLINK
 *
 * Receives uplink packets from comms driver, validates them,
 * and dispatches commands to the appropriate subsystem.
 *
 * Telecommand set is a placeholder -- define with your ground
 * segment team. At minimum handle:
 *   - Mode change commands
 *   - ADCS mode override
 *   - Payload trigger
 *   - Fault clear
 */

#ifndef TELECOMMAND_APP_H
#define TELECOMMAND_APP_H

#include "FreeRTOS.h"
#include "task.h"

/* Command opcodes (placeholders) */
#define TC_CMD_SET_MODE         0x01
#define TC_CMD_TRIGGER_PAYLOAD  0x02
#define TC_CMD_CLEAR_FAULT      0x03
#define TC_CMD_SET_ADCS_MODE    0x04
#define TC_CMD_REQUEST_HK       0x05

void Telecommand_Task(void *pvParameters);

#endif /* TELECOMMAND_APP_H */


/* ================================================================ */


/**
 * @file    payload_app.h
 * @brief   Payload Management Application
 *
 * Active in: IMAGE_CAPTURE
 *
 * The OBC triggers the payload controller over SPI and waits
 * for an acknowledgment. The payload controller manages the
 * actual image capture process internally.
 *
 * After capture, the OBC signals TRIGGER_CAPTURE_DONE to the
 * mode state machine.
 *
 * Payload controller SPI protocol: PLACEHOLDER.
 * Define once payload controller is selected.
 */

#ifndef PAYLOAD_APP_H
#define PAYLOAD_APP_H

#include "FreeRTOS.h"
#include "task.h"

#define PAYLOAD_CMD_CAPTURE   0xC1
#define PAYLOAD_CMD_STATUS    0xC2
#define PAYLOAD_CMD_RESET     0xC3

#define PAYLOAD_STATUS_IDLE   0x00
#define PAYLOAD_STATUS_BUSY   0x01
#define PAYLOAD_STATUS_DONE   0x02
#define PAYLOAD_STATUS_ERROR  0xFF

void Payload_Task(void *pvParameters);

#endif /* PAYLOAD_APP_H */


/* ================================================================ */


/**
 * @file    datamgmt_app.h
 * @brief   Data Management Application
 *
 * Active in: DOWNLINK
 *
 * Reads housekeeping frames from the HK queue and image data
 * from the SD card, packages them into downlink frames, and
 * pushes them to the downlink queue for the comms app.
 *
 * SD card interface: FatFS over SPI (SDIO also viable on F4).
 * FatFS is not initialised here -- add FATFS_Init() to main().
 */

#ifndef DATAMGMT_APP_H
#define DATAMGMT_APP_H

#include "FreeRTOS.h"
#include "task.h"

void DataMgmt_Task(void *pvParameters);

#endif /* DATAMGMT_APP_H */
