/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2018  AS <parai@foxmail.com>
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation; See <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
/* ============================ [ INCLUDES  ] ====================================================== */
#include <errno.h>
#include "device.h"
#include "asdebug.h"
#include "Os.h"
/* ============================ [ MACROS    ] ====================================================== */
#define AS_LOG_SDCARD 0

/* MMC/SD command */
#define CMD0	(0)			/* GO_IDLE_STATE */
#define CMD1	(1)			/* SEND_OP_COND (MMC) */
#define	ACMD41	(0x80+41)	/* SEND_OP_COND (SDC) */
#define CMD8	(8)			/* SEND_IF_COND */
#define CMD9	(9)			/* SEND_CSD */
#define CMD10	(10)		/* SEND_CID */
#define CMD12	(12)		/* STOP_TRANSMISSION */
#define ACMD13	(0x80+13)	/* SD_STATUS (SDC) */
#define CMD16	(16)		/* SET_BLOCKLEN */
#define CMD17	(17)		/* READ_SINGLE_BLOCK */
#define CMD18	(18)		/* READ_MULTIPLE_BLOCK */
#define CMD23	(23)		/* SET_BLOCK_COUNT (MMC) */
#define	ACMD23	(0x80+23)	/* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24	(24)		/* WRITE_BLOCK */
#define CMD25	(25)		/* WRITE_MULTIPLE_BLOCK */
#define CMD32	(32)		/* ERASE_ER_BLK_START */
#define CMD33	(33)		/* ERASE_ER_BLK_END */
#define CMD38	(38)		/* ERASE */
#define CMD55	(55)		/* APP_CMD */
#define CMD58	(58)		/* READ_OCR */

/* MMC card type flags (MMC_GET_TYPE) */
#define CT_MMC		0x01		/* MMC ver 3 */
#define CT_SD1		0x02		/* SD ver 1 */
#define CT_SD2		0x04		/* SD ver 2 */
#define CT_SDC		(CT_SD1|CT_SD2)	/* SD */
#define CT_BLOCK	0x08		/* Block addressing */

/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
extern void sd_spi_init(int sd);
/* when txData is NULL, the SPI must send 0xFF */
extern int  sd_spi_transmit(int sd, const uint8_t* txData, uint8_t* rxData, size_t size);
extern void sd_chip_selected(int sd, int select);
extern int  sd_is_detected(int sd);
extern void sd_spi_clk_slow(int sd);
extern void sd_spi_fast_fast(int sd);
/* ============================ [ DATAS     ] ====================================================== */
static uint8_t sd_type;
/* ============================ [ LOCALS    ] ====================================================== */
#if AS_LOG_SDCARD
static void sd_show_csd_v1(const uint8_t* csd)
{
	printf("CSD V1:\n");
	printf(" STRUCT     : %X\n", (uint32_t)((csd[0]>>6)&0x03));
	printf(" reserved   : %X\n", (uint32_t)(csd[0]&0x7F));
	printf(" TAAC       : %X\n", (uint32_t)csd[1]);
	printf(" NSAC       : %X\n", (uint32_t)csd[2]);
	printf(" TRAN_SPEED : %X\n", (uint32_t)csd[3]);
	printf(" CCC        : %X\n", (uint32_t)(csd[4]<<4)+(csd[5]>>4));
	printf(" READ_BL_LEN: %X\n", (uint32_t)(csd[5]&0xF));
	printf(" READ_BL_PARTIAL   : %X\n", (uint32_t)((csd[6]>>7)&0x1));
	printf(" WRITE_BLK_MISALIGN: %X\n", (uint32_t)((csd[6]>>6)&0x1));
	printf(" READ_BLK_MISALIGN : %X\n", (uint32_t)((csd[6]>>5)&0x1));
	printf(" DSR_IMP    : %X\n", (uint32_t)((csd[6]>>4)&0x1));
	printf(" reserved   : %X\n", (uint32_t)((csd[6]>>2)&0x3));
	printf(" C_SIZE     : %X\n", (uint32_t)((((csd[6])&0x3)<<10) + (csd[7]<<2) + ((csd[8]>>6)&0x3)));
	printf(" VDD_R_CURR_MIN    : %X\n", (uint32_t)((csd[8]>>3)&0x7));
	printf(" VDD_R_CURR_MAX    : %X\n", (uint32_t)(csd[8]&0x7));
	printf(" VDD_W_CURR_MAX    : %X\n", (uint32_t)((csd[9]>>5)&0x7));
	printf(" VDD_W_CURR_MAX    : %X\n", (uint32_t)((csd[9]>>2)&0x7));
	printf(" C_SIZE_MULT       : %X\n", (uint32_t)((csd[9]&0x3)<<1)+(csd[10]>>7));
	printf(" ERASE_BLK_EN      : %X\n", (uint32_t)((csd[10]>>6)&0x1));
	printf(" SECTOR_SIZE       : %X\n", (uint32_t)((csd[10]&0x3F)<<1)+(csd[11]>>7));
	printf(" WP_GRP_SIZE       : %X\n", (uint32_t)(csd[11]&0x3F));
	printf(" WP_GRP_ENABLE     : %X\n", (uint32_t)(csd[12]>>7));
	printf(" reserved  : %X\n", (uint32_t)((csd[12]>>5)&0x3));
	printf(" R2W_FACTOR: %X\n", (uint32_t)((csd[12]>>2)&0x7));
	printf(" WRITE_BL_LEN      : %X\n", (uint32_t)((((csd[12])&0x3)<<10) + ((csd[13]>>6)&0x3)));
	printf(" WRITE_BL_PARTIAL  : %X\n", (uint32_t)((csd[13]>>5)&0x1));
	printf(" reserved  : %X\n", (uint32_t)(csd[13]&0x1F));
	printf(" FILE_FORMAT_GRP   : %X\n", (uint32_t)((csd[14]>>7)&0x01));
	printf(" COPY      : %X\n", (uint32_t)((csd[14]>>6)&0x01));
	printf(" PERM_WRITE_PROTECT: %X\n", (uint32_t)((csd[14]>>5)&0x01));
	printf(" TMP_WRITE_PROTECT : %X\n", (uint32_t)((csd[14]>>4)&0x01));
	printf(" FILE_FORMAT       : %X\n", (uint32_t)((csd[14]>>2)&0x03));
}

static void sd_show_csd_v2(const uint8_t* csd)
{

}

static void sd_show_csd(const uint8_t* csd)
{
	asmem("CSD:", csd, 16);
	if ((csd[0] >> 6) == 1)
	{
		sd_show_csd_v2(csd);
	}
	else
	{
		sd_show_csd_v1(csd);
	}
}
#else
#define sd_show_csd(csd)
#endif
static uint8_t sd_spi_xchg(int sd, uint8_t txByte)
{
	uint8_t rxByte = 0xFF;

	(void)sd_spi_transmit(sd, &txByte, &rxByte, 1);

	return rxByte;
}

static int sd_wait_ready (int sd, uint32_t wt)
{
	uint8_t d;
	TimerType timer;
	StartTimer(&timer);
	do {
		d = sd_spi_xchg(sd, 0xFF);
		/* This loop takes a time. Insert rot_rdq() here for multi-task environment. */
	} while ((d != 0xFF) && (GetTimer(&timer)<(MS2TICKS(wt))));	/* Wait for card goes ready or timeout */

	return (d == 0xFF) ? 0 : ETIMEDOUT;
}

static int sd_rcvr_datablock (int sd, uint8_t *buff, size_t size)
{
	uint8_t token;
	TimerType timer;

	StartTimer(&timer);
	do {							/* Wait for DataStart token in timeout of 200ms */
		token = sd_spi_xchg(sd, 0xFF);
		/* This loop will take a time. Insert rot_rdq() here for multitask envilonment. */
	} while ((token == 0xFF) && (GetTimer(&timer)<=(MS2TICKS(200))));
	if(token != 0xFE) return -1;		/* Function fails if invalid DataStart token or timeout */

	sd_spi_transmit(sd, NULL, buff, size);		/* Store trailing data to the buffer */
	sd_spi_xchg(sd, 0xFF); sd_spi_xchg(sd, 0xFF);	/* Discard CRC */

	return 0;						/* Function succeeded */
}

static int sd_xmit_datablock (int sd, const uint8_t *buff, uint8_t token)
{
	uint8_t resp;

	if (sd_wait_ready(sd, 500)) return -1;		/* Wait for card ready */

	sd_spi_xchg(sd, token);				/* Send token */
	if (token != 0xFD) {				/* Send data if token is other than StopTran */
		sd_spi_transmit(sd, buff, NULL, 512);			/* Data */
		sd_spi_xchg(sd, 0xFF); sd_spi_xchg(sd, 0xFF);	/* Dummy CRC */

		resp = sd_spi_xchg(sd, 0xFF);			/* Receive data resp */
		if ((resp & 0x1F) != 0x05) return -2;	/* Function fails if the data packet was not accepted */
	}
	return 0;
}

static void sd_deselect (int sd)
{
	sd_chip_selected(sd, 1);		/* Set CS# high */
	sd_spi_xchg(sd, 0xFF);	/* Dummy clock (force DO hi-z for multiple slave SPI) */
}

static int sd_select (int sd)
{
	sd_spi_xchg(sd, 0xFF);	/* Dummy clock (force DO enabled) */
	sd_chip_selected(sd, 0);		/* Set CS# low */

	if (0 == sd_wait_ready(sd, 500)) return 0;	/* Wait for card ready */

	sd_deselect(sd);
	return -1;	/* Timeout */
}

static uint8_t sd_send_command(int sd, uint8_t cmd, long arg)
{
	uint8_t n, res;

	if (cmd & 0x80) {	/* Send a CMD55 prior to ACMD<n> */
		cmd &= 0x7F;
		res = sd_send_command(sd, CMD55, 0);
		if (res > 1) return res;
	}

	/* Select the card and wait for ready except to stop multiple block read */
	if (cmd != CMD12) {
		sd_deselect(sd);
		if (sd_select(sd)) return 0xFF;
	}

	/* Send command packet */
	sd_spi_xchg(sd, 0x40 | cmd);				/* Start + command index */
	sd_spi_xchg(sd, (uint8_t)(arg >> 24));		/* Argument[31..24] */
	sd_spi_xchg(sd, (uint8_t)(arg >> 16));		/* Argument[23..16] */
	sd_spi_xchg(sd, (uint8_t)(arg >> 8));			/* Argument[15..8] */
	sd_spi_xchg(sd, (uint8_t)arg);				/* Argument[7..0] */
	n = 0x01;							/* Dummy CRC + Stop */
	if (cmd == CMD0) n = 0x95;			/* Valid CRC for CMD0(0) */
	if (cmd == CMD8) n = 0x87;			/* Valid CRC for CMD8(0x1AA) */
	sd_spi_xchg(sd, n);

	/* Receive command resp */
	if (cmd == CMD12) sd_spi_xchg(sd, 0xFF);	/* Diacard following one byte when CMD12 */
	n = 10;								/* Wait for response (10 bytes max) */
	do {
		res = sd_spi_xchg(sd, 0xFF);
	} while ((res & 0x80) && --n);

	return res;							/* Return received response */
}

static size_t sd_sector_count(int sd)
{
	size_t csize = 0;
	uint8_t csd[16];
	uint8_t n;

	if ((sd_send_command(sd, CMD9, 0) == 0) &&
			(sd_rcvr_datablock(sd, csd, 16) == 0))
	{
		if ((csd[0] >> 6) == 1)
		{	/* SDC ver 2.00 */
			csize = csd[9] + ((size_t)csd[8] << 8) + ((size_t)(csd[7] & 63) << 16) + 1;
			csize = csize << 10;
		}
		else
		{	/* SDC ver 1.XX or MMC ver 3 */
			n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
			csize = (csd[8] >> 6) + ((size_t)csd[7] << 2) + ((size_t)(csd[6] & 3) << 10) + 1;
			csize = csize << (n - 9);
		}
	}
	sd_deselect(sd);

	ASLOG(SDCARD, ("SD sector count %d!\n", (uint32_t)csize));
	sd_show_csd(csd);

	return csize;
}

static size_t sd_block_size(int sd)
{
	size_t blksz = 0;
	uint8_t csd[16];
	uint8_t n;

	if (sd_type & CT_SD2)
	{	/* SDC ver 2.00 */
		if (sd_send_command(sd, ACMD13, 0) == 0)
		{	/* Read SD status */
			sd_spi_xchg(sd, 0xFF);
			if (0 == sd_rcvr_datablock(sd, csd, 16))
			{	/* Read partial block */
				for (n = 64 - 16; n; n--)
				{
					sd_spi_xchg(sd, 0xFF);	/* Purge trailing data */
				}
				blksz = 16UL << (csd[10] >> 4);
			}
		}
	}
	else
	{	/* SDC ver 1.XX or MMC */
		if ((sd_send_command(sd, CMD9, 0) == 0) &&
				(sd_rcvr_datablock(sd, csd, 16) == 0))
		{	/* Read CSD */
			if (sd_type & CT_SD1)
			{	/* SDC ver 1.XX */
				blksz = (((csd[10] & 63) << 1) + ((size_t)(csd[11] & 128) >> 7) + 1) << ((csd[13] >> 6) - 1);
			}
			else
			{	/* MMC */
				blksz = ((size_t)((csd[10] & 124) >> 2) + 1) * (((csd[11] & 3) << 3) + ((csd[11] & 224) >> 5) + 1);
			}
		}
	}
	sd_deselect(sd);

	ASLOG(SDCARD, ("SD block size %d!\n", (uint32_t)blksz));
	sd_show_csd(csd);

	return blksz;
}

static int sd_init(int sd)
{
	int ercd;
	uint8_t n, cmd, ty, ocr[4], status;
	TimerType timer;

	sd_spi_clk_slow(sd);
	StartTimer(&timer);	/* Initialization timeout = 1 sec */

	do {
		for (n = 10; n; n--)
		{
			sd_spi_xchg(sd, 0xFF);	/* Send 80 dummy clocks */
		}
		status = sd_send_command(sd, CMD0,0); /* send IDLE command */
	} while((status != 1) && (GetTimer(&timer)<=MS2TICKS(1000)));

	ty = 0;
	if (1 == status)
	{	/* Put the card SPI/Idle state */
		if (sd_send_command(sd, CMD8, 0x1AA) == 1)
		{	/* SDv2? */
			for (n = 0; n < 4; n++)
			{	/* Get 32 bit return value of R7 resp */
				ocr[n] = sd_spi_xchg(sd, 0xFF);
			}
			if (ocr[2] == 0x01 && ocr[3] == 0xAA)
			{	/* Is the card supports vcc of 2.7-3.6V? */
				/* Wait for end of initialization with ACMD41(HCS) */
				while ((GetTimer(&timer)<=MS2TICKS(1000)) && sd_send_command(sd, ACMD41, 1UL << 30));
				if ((GetTimer(&timer)<=MS2TICKS(1000)) && sd_send_command(sd, CMD58, 0) == 0)
				{	/* Check CCS bit in the OCR */
					for (n = 0; n < 4; n++) ocr[n] = sd_spi_xchg(sd, 0xFF);
					ty = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;	/* Card id SDv2 */
				}
			}
		}
		else
		{	/* Not SDv2 card */
			if (sd_send_command(sd, ACMD41, 0) <= 1)
			{	/* SDv1 or MMC? */
				ty = CT_SD1; cmd = ACMD41;	/* SDv1 (ACMD41(0)) */
			}
			else
			{
				ty = CT_MMC; cmd = CMD1;	/* MMCv3 (CMD1(0)) */
			}

			/* Wait for end of initialization */
			while ((GetTimer(&timer)<=MS2TICKS(1000)) && sd_send_command(sd, cmd, 0)) ;
			if ((GetTimer(&timer)>MS2TICKS(1000)) || (sd_send_command(sd, CMD16, 512) != 0))
			{	/* Set block length: 512 */
				ty = 0;
			}
		}
	}

	sd_type = ty;	/* Card type */
	sd_deselect(sd);

	if (ty)
	{	/* OK */
		sd_spi_fast_fast(sd);			/* Set fast clock */
		ercd = 0;
	}
	else
	{	/* Failed */
		ercd = -1;
	}

	ASLOG(SDCARD, ("init SD<%02X> %s!\n", (uint32_t)ty, ercd?"failed":"okay"));

	return ercd;
}

static int asblk_open  (const device_t* device)
{
	int ercd;
	int sd = (int)(long)device->priv;

	sd_spi_init(sd);
	sd_deselect(sd);

	ercd = sd_init(sd);

	return ercd;
}

static int asblk_close (const device_t* device)
{
	int sd = (int)(long)device->priv;

	sd_deselect(sd);
	return 0;
}

static int asblk_read  (const device_t* device, size_t pos, void *buffer, size_t size)
{
	int sd = (int)(long)device->priv;

	if (!(sd_type & CT_BLOCK)) pos *= 512;	/* LBA ot BA conversion (byte addressing cards) */

	if (size == 1)
	{	/* Single sector read */
		if ((sd_send_command(sd, CMD17, pos) == 0)	/* READ_SINGLE_BLOCK */
			&& (0 == sd_rcvr_datablock(sd, buffer, 512)))
		{
			size = 0;
		}
	}
	else
	{	/* Multiple sector read */
		if (sd_send_command(sd, CMD18, pos) == 0)
		{	/* READ_MULTIPLE_BLOCK */
			do {
				if (sd_rcvr_datablock(sd, buffer, 512)) break;
				buffer = (void*)(((unsigned long)buffer)+512);
			} while (--size);
			sd_send_command(sd, CMD12, 0);	/* STOP_TRANSMISSION */
		}
	}
	sd_deselect(sd);

	return (int)size;
}

static int asblk_write (const device_t* device, size_t pos, const void *buffer, size_t size)
{
	int sd = (int)(long)device->priv;

	if (!(sd_type & CT_BLOCK)) pos *= 512;	/* LBA ==> BA conversion (byte addressing cards) */

	if (size == 1)
	{	/* Single sector write */
		if ((sd_send_command(sd, CMD24, pos) == 0)	/* WRITE_BLOCK */
			&& (0 == sd_xmit_datablock(sd, buffer, 0xFE)))
		{
			size = 0;
		}
	}
	else
	{	/* Multiple sector write */
		if (sd_type & CT_SDC) sd_send_command(sd, ACMD23, size);	/* Predefine number of sectors */
		if (sd_send_command(sd, CMD25, pos) == 0)
		{	/* WRITE_MULTIPLE_BLOCK */
			do {
				if (sd_xmit_datablock(sd, buffer, 0xFC)) break;
				buffer = (void*)(((unsigned long)buffer)+512);
			} while (--size);
			if (sd_xmit_datablock(sd, 0, 0xFD)) size = -1;	/* STOP_TRAN token */
		}
	}
	sd_deselect(sd);

	return (int)size;
}

static int asblk_ctrl  (const device_t* device, int cmd,    void *args)
{
	int ercd = 0;
	int sd = (int)(long)device->priv;

	switch(cmd)
	{
		case DEVICE_CTRL_GET_SECTOR_SIZE:
			*(size_t*)args = 512;
			break;
		case DEVICE_CTRL_GET_BLOCK_SIZE:
			*(size_t*)args = sd_block_size(sd);
			break;
		case DEVICE_CTRL_GET_SECTOR_COUNT:
			*(size_t*)args = sd_sector_count(sd);
			break;
		case DEVICE_CTRL_GET_DISK_SIZE:
			*(size_t*)args = sd_sector_count(sd)*512;
		break;
		default:
			ercd = EINVAL;
			break;
	}

	return ercd;
}
/* ============================ [ FUNCTIONS ] ====================================================== */
const device_t device_asblk0 = {
	"asblk0",
	{
		asblk_open,
		asblk_close,
		asblk_read,
		asblk_write,
		asblk_ctrl,
	},
	(void*)0
};
