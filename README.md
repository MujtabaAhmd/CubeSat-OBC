#### **COMMS Testing**

---

The testing is being done on ESP32 S3 board. The test is for the interface that the 2 subsystems are interacting with each other correctly.


###### **Wiring**

ESP32-S3			<-> STM32F407

GPIO10 (CS/NSS) 	<-> PA3 (SPI1\_NSS)

GPIO11 (MOSI)    	<-> PA7 (SPI1\_MOSI)

GPIO12 (MISO)    	<-> PA6 (SPI1\_MISO)

GPIO13 (SCK)     	<-> PA5 (SPI1\_SCK)

GND              		<-> GND



###### **SPI Mode:**

Mode 0 (CPOL=0, CPHA=0) -- match this on STM32 side.

The STM32 pulls the CS pin low every 6 seconds and runs the COMMS\_Init function. It just sends a single text packet. Further communication is to be done between the systems.


**NOTE:** The ESP32 Setup is a dud and a test for the STM32 Board. After the Comms system is setup, full functionality between the 2 will be tested.