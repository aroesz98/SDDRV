/*
 * sd.h
 *
 *  Created on: Apr 18, 2022
 *      Author: asz
 */

#ifndef SD_DRV_H_
#define SD_DRV_H_

#include "stdbool.h"
#include "stm32f4xx_hal.h"
#include "diskio.h"

extern SPI_HandleTypeDef 	hspi3;

#define timeout 500

#define SDPORT				&hspi3
#define	SD_CS_PORT			GPIOB
#define SD_CS_PIN			GPIO_PIN_6

#define CMD0     (0x40+0)     	// Go to sleep
#define CMD1     (0x40+1)     	// OP Cond
#define CMD8     (0x40+8)     	// IF Cond
#define CMD9     (0x40+9)     	// Send CSD
#define CMD10    (0x40+10)    	// Send CID
#define CMD12    (0x40+12)    	// End of transmission
#define CMD16    (0x40+16)    	// Set block length
#define CMD17    (0x40+17)    	// Read single blocks
#define CMD18    (0x40+18)    	// Read multiple blocks
#define CMD23    (0x40+23)    	// Set block count
#define CMD24    (0x40+24)    	// Write single block
#define CMD25    (0x40+25)    	// Write multiple blocks
#define CMD41    (0x40+41)    	// ACMD
#define CMD55    (0x40+55)    	// App command
#define CMD58    (0x40+58)    	// Read OCR

#define MMC		0x01			// MMC v3
#define SD1		0x02			// SD v1
#define SD2		0x04			// SD v2
#define SDC		0x06			// SD
#define BLOCK	0x08			// Block addressing

DSTATUS sdInit (BYTE pdrv);
DSTATUS sdStatus (BYTE pdrv);
DRESULT sdRead (BYTE pdrv, BYTE* buff, DWORD sector, UINT count);
DRESULT sdWrite (BYTE pdrv, const BYTE* buff, DWORD sector, UINT count);
DRESULT sdCtrl (BYTE pdrv, BYTE cmd, void* buff);

#endif /* SD_DRV_H_ */
