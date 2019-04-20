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
#include "device.h"
#include "asdebug.h"
/* ============================ [ MACROS    ] ====================================================== */
#define AS_LOG_SDCARD 1

#define sd_select()   sd_chip_selected(1)
#define sd_deselect() sd_chip_selected(0);
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
extern void sd_spi_init(void);
extern int  sd_spi_transmit(const uint8_t* txData, uint8_t* rxData, size_t size);
extern void sd_chip_selected(int select);
extern int sd_is_detected(void);
/* ============================ [ DATAS     ] ====================================================== */
/* ============================ [ LOCALS    ] ====================================================== */
static uint8_t sd_spi_tr(uint8_t txByte)
{
	uint8_t rxByte = 0xFF;

	(void)sd_spi_transmit(&txByte, &rxByte, 1);

	return rxByte;
}

static uint8_t sd_send_command(uint8_t cmd, long arg)
{
	uint8_t status;
	uint8_t retry=0;

	sd_spi_tr(0xff);
	sd_select();

	sd_spi_tr(cmd | 0x40);
	sd_spi_tr(arg>>24);
	sd_spi_tr(arg>>16);
	sd_spi_tr(arg>>8);
	sd_spi_tr(arg);
	sd_spi_tr(0x95);

	while((status = sd_spi_tr(0xff)) == 0xff)
	{
		if(retry++ > 10) break;
	}
	sd_deselect();

	return status;
}

static int sd_reset(void)
{
	uint8_t i;
	uint8_t retry;
	uint8_t status;
	retry = 0;
	do
	{
		for(i=0;i<10;i++)
		{
			sd_spi_tr(0xff);
		}
		status = sd_send_command(0,0); /* send IDLE command */
		retry++;
		if(retry>10) return -1;
	} while(status != 0x01);

	retry = 0;
	do
	{
		status = sd_send_command(1, 0); /* set trigger command */
		retry++;
		if(retry>100) return -2;
	} while(status);

	status = sd_send_command(59, 0);

	status = sd_send_command(16, 512); /* set sector size 512 */

	return 0;
}

static int sd_read(size_t sector, uint8_t* buffer)
{
	uint8_t status;

	status = sd_send_command(17, sector<<9);
	if(status != 0x00) return -1;

	sd_select();
	/* wait the starting of data */
	while(sd_spi_tr(0xff) != 0xfe);

	sd_spi_transmit(NULL, buffer, 512);

	sd_spi_tr(0xff);
	sd_spi_tr(0xff);
	sd_deselect();
	sd_spi_tr(0xff);

	return 0;
}

static int sd_write(size_t sector, const uint8_t* buffer)
{
	uint8_t status;
	int i;

	status = sd_send_command(24, sector<<9);
	if(status != 0x00) return -1;

	sd_select();

	sd_spi_tr(0xff);
	sd_spi_tr(0xff);
	sd_spi_tr(0xff);

	sd_spi_tr(0xfe);	/* send start */

	sd_spi_transmit(buffer, NULL, 512);

	sd_spi_tr(0xff);
	sd_spi_tr(0xff);

	status = sd_spi_tr(0xff);
	if( (status&0x1f) != 0x05)
	{
		sd_deselect();
		return -2;
	}

	while(!sd_spi_tr(0xff));

	sd_deselect();

	return 0;
}
static int asblk_open  (const device_t* device)
{
	int ercd;

	sd_spi_init();
	sd_deselect();

	ercd = sd_reset();

	ASLOG(SDCARD, ("reset SD %s!\n", ercd?"failed":"okay"));

	return ercd;
}

static int asblk_close (const device_t* device)
{
	sd_deselect();
	return 0;
}

static int asblk_read  (const device_t* device, size_t pos, void *buffer, size_t size)
{
	int ercd = 0;
	while((0==ercd) && (size > 0))
	{
		ercd = sd_read(pos, buffer);
		size--;
		pos++;
		buffer += 512;
	}

	return ercd;
}

static int asblk_write (const device_t* device, size_t pos, const void *buffer, size_t size)
{
	int ercd = 0;
	while((0==ercd) && (size > 0))
	{
		ercd = sd_write(pos, (const uint8_t*)buffer);
		size--;
		pos++;
		buffer += 512;
	}

	return 0;
}

static int asblk_ctrl  (const device_t* device, int cmd,    void *args)
{
	uint32_t size;
	int ercd = 0;

	switch(cmd)
	{
		case DEVICE_CTRL_GET_SECTOR_SIZE:
			*(size_t*)args = 512;
			break;
		case DEVICE_CTRL_GET_BLOCK_SIZE:
			*(size_t*)args = 4096;
			break;
		case DEVICE_CTRL_GET_SECTOR_COUNT:
			size = 256*1024*1024; /* TODO: how to dynamic get size */
			*(size_t*)args = size/512;
			break;
		case DEVICE_CTRL_GET_DISK_SIZE:
			size = 256*1024*1024;
			*(size_t*)args = size;
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
	NULL
};
