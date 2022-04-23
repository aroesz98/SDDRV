/*
 * sd_drv.c
 *
 *  Created on: Apr 18, 2022
 *      Author: asz
 */

#include "sd_drv.h"

uint16_t tim1, tim2;

static volatile DSTATUS Stat = STA_NOINIT;
static uint8_t SDType;
static uint8_t pwrFlag = 0;

static void transmitByte(uint8_t data)
{
	HAL_SPI_Transmit(SDPORT, &data, 1, timeout);
}

static uint8_t receiveByte(void)
{
	uint8_t dummy, data;
	dummy = 0xFF;

	HAL_SPI_TransmitReceive(SDPORT, &dummy, &data, 1, timeout);

	return data;
}

static void transmitBuffer(uint8_t *buffer, uint16_t len)
{
	HAL_SPI_Transmit(SDPORT, buffer, len, timeout);
}

static uint8_t wait4ready(void)
{
	uint8_t res;
	tim2 = 500;

	do {
		res = receiveByte();
	} while ((res != 0xFF) && tim2);

	return res;
}

static void turnOn(void)
{
	uint8_t args[6];
	uint32_t cnt = 0x1FFF;

	SD_CS_PORT->BSRR = SD_CS_PIN;

	for(int i = 0; i < 10; i++)
	{
		transmitByte(0xFF);
	}

	SD_CS_PORT->BSRR = SD_CS_PIN << 16U;

	args[0] = CMD0;
	args[1] = 0;
	args[2] = 0;
	args[3] = 0;
	args[4] = 0;
	args[5] = 0x95;

	transmitBuffer(args, sizeof(args));

	while ((receiveByte() != 0x01) && cnt)
	{
		cnt--;
	}

	SD_CS_PORT->BSRR = SD_CS_PIN;
	transmitByte(0XFF);

	pwrFlag = 1;
}

static void turnOff(void)
{
	pwrFlag = 0;
}

static uint8_t chkPwr(void)
{
	return pwrFlag;
}

static bool receiveBlock(BYTE *buff, UINT len)
{
	uint8_t token;
	tim1 = 200;

	do {
		token = receiveByte();
	} while((token == 0xFF) && tim1);


	if(token != 0xFE) return false;

	do {
		*buff++ = receiveByte();
	} while(len--);

	receiveByte();
	receiveByte();

	return true;
}

static bool transmitBlock(const uint8_t *buff, BYTE token)
{
	uint8_t resp = 0;
	uint8_t i = 0;

	if (wait4ready() != 0xFF) return false;

	transmitByte(token);

	if (token != 0xFD)
	{
		transmitBuffer((uint8_t*)buff, 512);

		receiveByte();
		receiveByte();

		while (i <= 64)
		{
			resp = receiveByte();

			if ((resp & 0x1F) == 0x05) break;
			i++;
		}

		while (receiveByte() == 0);
	}

	if ((resp & 0x1F) == 0x05) return true;
	return false;
}

static BYTE writeCommand(BYTE cmd, uint32_t arg)
{
	uint8_t crc, res;

	if (wait4ready() != 0xFF) return 0xFF;

	transmitByte(cmd);
	transmitByte((uint8_t)(arg >> 24));
	transmitByte((uint8_t)(arg >> 16));
	transmitByte((uint8_t)(arg >> 8));
	transmitByte((uint8_t)arg);

	if(cmd == CMD0) crc = 0x95;
	else if(cmd == CMD8) crc = 0x87;
	else crc = 1;

	transmitByte(crc);

	if (cmd == CMD12) receiveByte();

	uint8_t n = 10;

	do {
		res = receiveByte();
	} while ((res & 0x80) && --n);

	return res;
}

DSTATUS sdInit(BYTE drv)
{
	uint8_t n, type, ocr[4];

	if(drv) return STA_NOINIT;
	if(Stat & STA_NODISK) return Stat;

	turnOn();

	SD_CS_PORT->BSRR = SD_CS_PIN << 16U;

	type = 0;

	if (writeCommand(CMD0, 0) == 1)
	{
		tim1 = 1000;
		if (writeCommand(CMD8, 0x1AA) == 1)
		{
			for (n = 0; n < 4; n++)
			{
				ocr[n] = receiveByte();
			}

			if (ocr[2] == 0x01 && ocr[3] == 0xAA)
			{
				do {
					if (writeCommand(CMD55, 0) <= 1 && writeCommand(CMD41, 1UL << 30) == 0) break;
				} while (tim1);

				if (tim1 && writeCommand(CMD58, 0) == 0)
				{
					for (n = 0; n < 4; n++)
					{
						ocr[n] = receiveByte();
					}

					type = (ocr[0] & 0x40) ? SD2 | BLOCK : SD2;
				}
			}
		}
		else
		{
			type = (writeCommand(CMD55, 0) <= 1 && writeCommand(CMD41, 0) <= 1) ? SD1 : MMC;

			do {
				if (type == SD1)
				{
					if (writeCommand(CMD55, 0) <= 1 && writeCommand(CMD41, 0) == 0) break;
				}

				else
				{
					if (writeCommand(CMD1, 0) == 0) break;
				}
			} while (tim1);

			if (!tim1 || writeCommand(CMD16, 512) != 0) type = 0;
		}
	}

	SDType = type;

	SD_CS_PORT->BSRR = SD_CS_PIN;
	receiveByte();

	if(type) Stat &= ~STA_NOINIT;
	else turnOff();

	return Stat;
}

DSTATUS sdStatus(BYTE drv)
{
	if (drv) return STA_NOINIT;
	return Stat;
}

DRESULT sdRead(BYTE pdrv, BYTE* buff, DWORD sector, UINT count)
{
	if (pdrv || !count) return RES_PARERR;

	if (Stat & STA_NOINIT) return RES_NOTRDY;

	if (!(SDType & SD2)) sector *= 512;

	SD_CS_PORT->BSRR = SD_CS_PIN << 16U;

	if (count == 1)
	{
		if ((writeCommand(CMD17, sector) == 0) && receiveBlock(buff, 512)) count = 0;
	}
	else
	{
		if (writeCommand(CMD18, sector) == 0)
		{
			do {
				if (!receiveBlock(buff, 512)) break;
				buff += 512;
			} while (--count);

			writeCommand(CMD12, 0);
		}
	}

	SD_CS_PORT->BSRR = SD_CS_PIN << 16U;
	receiveByte();

	return count ? RES_ERROR : RES_OK;
}

DRESULT sdWrite(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count)
{
	if (pdrv || !count) return RES_PARERR;

	if (Stat & STA_NOINIT) return RES_NOTRDY;

	if (Stat & STA_PROTECT) return RES_WRPRT;

	if (!(SDType & SD2)) sector *= 512;

	SD_CS_PORT->BSRR = SD_CS_PIN << 16U;

	if (count == 1)
	{
		if ((writeCommand(CMD24, sector) == 0) && transmitBlock(buff, 0xFE))
			count = 0;
	}
	else
	{
		if (SDType & SD1)
		{
			writeCommand(CMD55, 0);
			writeCommand(CMD23, count);
		}

		if (writeCommand(CMD25, sector) == 0)
		{
			do {
				if(!transmitBlock(buff, 0xFC)) break;
				buff += 512;
			} while (--count);

			if(!transmitBlock(0, 0xFD))
			{
				count = 1;
			}
		}
	}

	SD_CS_PORT->BSRR = SD_CS_PIN;
	receiveByte();

	return count ? RES_ERROR : RES_OK;
}

DRESULT sdCtrl(BYTE drv, BYTE ctrl, void *buff)
{
	DRESULT res;
	uint8_t n, csd[16], *ptr = buff;
	WORD csize;

	if (drv) return RES_PARERR;
	res = RES_ERROR;

	if (ctrl == CTRL_POWER)
	{
		switch (*ptr)
		{
		case 0:
			turnOff();
			res = RES_OK;
			break;
		case 1:
			turnOn();
			res = RES_OK;
			break;
		case 2:
			*(ptr + 1) = chkPwr();
			res = RES_OK;
			break;
		default:
			res = RES_PARERR;
		}
	}
	else
	{
		if (Stat & STA_NOINIT) return RES_NOTRDY;

		SD_CS_PORT->BSRR = SD_CS_PIN << 16U;

		switch (ctrl)
		{
		case GET_SECTOR_COUNT:
			if ((writeCommand(CMD9, 0) == 0) && receiveBlock(csd, 16))
			{
				if ((csd[0] >> 6) == 1)
				{
					csize = csd[9] + ((WORD) csd[8] << 8) + 1;
					*(DWORD*) buff = (DWORD) csize << 10;
				}
				else
				{
					n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
					csize = (csd[8] >> 6) + ((WORD) csd[7] << 2) + ((WORD) (csd[6] & 3) << 10) + 1;
					*(DWORD*) buff = (DWORD) csize << (n - 9);
				}
				res = RES_OK;
			}
			break;
		case GET_SECTOR_SIZE:
			*(WORD*) buff = 512;
			res = RES_OK;
			break;
		case CTRL_SYNC:
			if (wait4ready() == 0xFF) res = RES_OK;
			break;
		case MMC_GET_CSD:
			if (writeCommand(CMD9, 0) == 0 && receiveBlock(ptr, 16)) res = RES_OK;
			break;
		case MMC_GET_CID:
			if (writeCommand(CMD10, 0) == 0 && receiveBlock(ptr, 16)) res = RES_OK;
			break;
		case MMC_GET_OCR:
			if (writeCommand(CMD58, 0) == 0)
			{
				for (n = 0; n < 4; n++)
				{
					*ptr++ = receiveByte();
				}
				res = RES_OK;
			}
		default:
			res = RES_PARERR;
		}

		SD_CS_PORT->BSRR = SD_CS_PIN;
		receiveByte();
	}

	return res;
}
