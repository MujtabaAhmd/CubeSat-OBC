/*
 * ESP32-S3 - SPI Slave for STM32F407 Master Testing
 * Hybrid version: attachInterrupt() on CS pin (FALLING) + IDF SPI slave driver
 *
 * Wiring (ESP32-S3 <-> STM32F407):
 *   GPIO10 (CS/NSS)  <-> PA4 (SPI1_NSS)
 *   GPIO11 (MOSI)    <-> PA7 (SPI1_MOSI)
 *   GPIO12 (MISO)    <-> PA6 (SPI1_MISO)
 *   GPIO13 (SCK)     <-> PA5 (SPI1_SCK)
 *   GND              <-> GND
 *
 * Verify these GPIO numbers against your specific S3 board's pinout
 * before wiring; broken-out pins vary between boards.
 *
 * SPI Mode: Mode 0 (CPOL=0, CPHA=0) -- match this on STM32 side.
 *
 * ARCHITECTURE NOTE:
 * The attachInterrupt() here is a raw GPIO-edge interrupt on the CS line,
 * separate from the SPI peripheral itself. It fires the instant CS drops,
 * giving you a precise "transaction starting" timestamp/flag independent
 * of the SPI hardware/DMA pipeline. The actual byte data is still only
 * available after the IDF SPI slave driver completes the queued
 * transaction (when CS returns high), since the ESP32-S3 SPI peripheral
 * does not expose per-byte interrupts the way the ATmega328P does.
 *
 * Because GPIO10 is also being used as the dedicated SPI slave CS pin
 * (spics_io_num), wiring the same physical line to interrupt logic on
 * the same pin works fine on the ESP32 (unlike the AVR, there's no
 * separate INT0/INT1 restriction here) -- you do not need a second
 * jumper wire as was required on the Nano.
 */

#include <Arduino.h>
#include "driver/spi_slave.h"

// ---- Pin configuration ----
#define PIN_MOSI   11
#define PIN_MISO   12
#define PIN_SCK    13
#define PIN_CS     10

#define SPI_HOST_USED  SPI2_HOST

// ---- Buffer configuration ----
#define BUFFER_SIZE  64   // must cover the max expected transaction size

WORD_ALIGNED_ATTR uint8_t txBuffer[BUFFER_SIZE];
WORD_ALIGNED_ATTR uint8_t rxBuffer[BUFFER_SIZE];

// ---- Flags shared between ISR and loop() ----
volatile bool     csFallingFlag   = false;  // set by attachInterrupt ISR
volatile uint32_t csFallingMicros = 0;       // timestamp of CS falling edge
volatile bool     transactionReady = false;  // set by SPI driver post_trans_cb

// =================================================================
// Raw GPIO interrupt: fires the instant CS goes LOW
// =================================================================
void IRAM_ATTR csFallingISR() {
  csFallingFlag   = true;
  csFallingMicros = micros();
}

// =================================================================
// SPI slave driver callback: fires when CS goes HIGH (transaction done)
// =================================================================
void IRAM_ATTR my_post_trans_cb(spi_slave_transaction_t *trans) {
  transactionReady = true;
}

void setupSPISlave() {
  esp_err_t ret;

  spi_bus_config_t buscfg = {};
  buscfg.mosi_io_num = PIN_MOSI;
  buscfg.miso_io_num = PIN_MISO;
  buscfg.sclk_io_num = PIN_SCK;
  buscfg.quadwp_io_num = -1;
  buscfg.quadhd_io_num = -1;
  buscfg.max_transfer_sz = BUFFER_SIZE;

  spi_slave_interface_config_t slvcfg = {};
  slvcfg.mode = 0;                  // Mode 0, must match STM32 master
  slvcfg.spics_io_num = PIN_CS;
  slvcfg.queue_size = 3;
  slvcfg.flags = 0;
  slvcfg.post_setup_cb = nullptr;   // not used in this version; attachInterrupt covers CS-low detection
  slvcfg.post_trans_cb = my_post_trans_cb;

  ret = spi_slave_initialize(SPI_HOST_USED, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);

  if (ret != ESP_OK) {
    Serial.printf("SPI slave init failed, error code: %d\n", ret);
    while (1) { delay(1000); }
  }

  Serial.println("SPI slave initialized successfully.");
}

void queueNextTransaction() {
  static spi_slave_transaction_t t;
  memset(&t, 0, sizeof(t));

  // Test response payload -- modify as needed
  txBuffer[0] = 0xA5;
  txBuffer[1] = 0x5A;
  txBuffer[2] = 0x00;

  t.length    = BUFFER_SIZE * 8;   // length in BITS
  t.tx_buffer = txBuffer;
  t.rx_buffer = rxBuffer;

  esp_err_t ret = spi_slave_queue_trans(SPI_HOST_USED, &t, portMAX_DELAY);
  if (ret != ESP_OK) {
    Serial.printf("Failed to queue SPI transaction, error: %d\n", ret);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32-S3 SPI Slave - STM32 Test (attachInterrupt version)");

  memset(txBuffer, 0x00, BUFFER_SIZE);
  memset(rxBuffer, 0xFF, BUFFER_SIZE);  // known pattern for debugging

  setupSPISlave();

  // attachInterrupt on the CS line for a precise "CS went low" event,
  // independent of the SPI driver's internal state machine.
  // Note: PIN_CS is being driven by the SPI peripheral as a dedicated
  // CS input at the same time; this dual use is fine on ESP32-S3 GPIOs.
  pinMode(PIN_CS, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_CS), csFallingISR, FALLING);

  queueNextTransaction();

  Serial.println("Ready. Waiting for STM32 master...");
}

void loop() {
  if (csFallingFlag) {
    csFallingFlag = false;
    Serial.printf(">> CS FALLING interrupt fired at %lu us\n",
                  (unsigned long)csFallingMicros);
  }

  if (transactionReady) {
    transactionReady = false;

    spi_slave_transaction_t *retTrans;
    esp_err_t ret = spi_slave_get_trans_result(SPI_HOST_USED, &retTrans, 0);

    if (ret == ESP_OK) {
      size_t bytesReceived = retTrans->trans_len / 8;

      Serial.printf("Transaction complete. Bits received: %lu (%u bytes)\n",
                    (unsigned long)retTrans->trans_len, (unsigned)bytesReceived);

      Serial.print("RX data: ");
      for (size_t i = 0; i < bytesReceived && i < BUFFER_SIZE; i++) {
        Serial.printf("%02X ", rxBuffer[i]);
      }
      Serial.println();
    } else {
      Serial.printf("Failed to get transaction result, error: %d\n", ret);
    }
    Serial.print("Waiting for new data...");
    queueNextTransaction();
  }
}