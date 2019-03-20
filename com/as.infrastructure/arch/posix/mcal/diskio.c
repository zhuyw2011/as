/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2017  AS <parai@foxmail.com>
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
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef USE_FATFS
#include "diskio.h"
#endif
#include "asdebug.h"
#ifdef USE_LWEXT4
#include "ext4.h"
#include "ext4_mkfs.h"
#include "ext4_config.h"
#include "ext4_blockdev.h"
#include "ext4_errno.h"
#include "file_dev.h"
#endif
#ifdef USE_DEV
#include "device.h"
#endif
/* ============================ [ MACROS    ] ====================================================== */
#define AS_LOG_FATFS 0
#define AS_LOG_EXTFS 0
#define AS_LOG_BLKDEV 0
/* Definitions of physical drive number for each drive */
#define DEV_MMC		0	/* Example: Map MMC/SD card to physical drive 0 : default */
#define DEV_RAM		1	/* Example: Map Ramdisk to physical drive 1 */
#define DEV_USB		2	/* Example: Map USB MSD to physical drive 2 */
#define EXTFS_IMG	"asblk1.img"
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
#ifdef USE_DEV
static int asblk_open  (const device_t* device);
static int asblk_close (const device_t* device);
static int asblk_read  (const device_t* device, size_t pos, void *buffer, size_t size);
static int asblk_write (const device_t* device, size_t pos, const void *buffer, size_t size);
static int asblk_ctrl  (const device_t* device, int cmd,    void *args);
#endif
/* ============================ [ DATAS     ] ====================================================== */
const char* FATFS_IMG = "asblk0.img";
#ifdef USE_DEV
const device_t device_asblk0 = {
	"asblk0",
	{
		asblk_open,
		asblk_close,
		asblk_read,
		asblk_write,
		asblk_ctrl,
	},
	(void*) 0
};

const device_t device_asblk1 = {
	"asblk1",
	{
		asblk_open,
		asblk_close,
		asblk_read,
		asblk_write,
		asblk_ctrl,
	},
	(void*) 1
};
#endif
/* ============================ [ LOCALS    ] ====================================================== */
#ifdef USE_DEV
static int asblk_open  (const device_t* device)
{
	char name[64];
	FILE* fp;
	uint8_t *data;
	uint32_t blkid = (uint32_t)(unsigned long)device->priv;

	snprintf(name, sizeof(name), "asblk%d.img", blkid);
	fp = fopen(name,"rb");
	if(NULL == fp)
	{
		fp = fopen(name,"wb+");
		asAssert(fp);
		data = malloc(1024*1024);
		asAssert(data);
		memset(data, 0xFF, 1024*1024);
		for(int i=0;i<32;i++)
		{
			fwrite(data,1,1024*1024,fp);
		}
		free(data);
		fclose(fp);
		ASLOG(BLKDEV, ("simulation on new created 32Mb %s\n", name));
	}
	else
	{
		ASLOG(BLKDEV, ("simulation on old %s\n", name));
		fclose(fp);
	}

	return 0;
}

static int asblk_close (const device_t* device)
{
	return 0;
}

static int asblk_read  (const device_t* device, size_t pos, void *buffer, size_t size)
{
	int res = 0;
	int len;
	char name[64];
	FILE* fp;
	uint32_t blkid = (uint32_t)(unsigned long)device->priv;

	snprintf(name, sizeof(name), "asblk%d.img", blkid);
	fp=fopen(name,"rb");
	asAssert(fp);
	fseek(fp,512*pos,SEEK_SET);
	len=fread(buffer,sizeof(char),size*512,fp);
	if(len!=size*512)
	{
		res= -1;
	}
	fclose(fp);
	return res;
}

static int asblk_write (const device_t* device, size_t pos, const void *buffer, size_t size)
{
	int res = 0;
	int len;
	char name[64];
	FILE* fp;
	uint32_t blkid = (uint32_t)(unsigned long)device->priv;

	snprintf(name, sizeof(name), "asblk%d.img", blkid);
	fp=fopen(name,"rb+");
	asAssert(fp);
	fseek(fp,512*pos,SEEK_SET);
	len=fwrite(buffer,sizeof(char),size*512,fp);
	if(len!=size*512)
	{
		res= -1;
	}
	fclose(fp);
	return res;
}

static int asblk_ctrl  (const device_t* device, int cmd,    void *args)
{
	char name[64];
	FILE* fp;
	uint32_t blkid = (uint32_t)(unsigned long)device->priv;
	size_t size;
	int ercd = 0;

	snprintf(name, sizeof(name), "asblk%d.img", blkid);
	switch(cmd)
	{
		case DEVICE_CTRL_GET_SECTOR_SIZE:
			*(size_t*)args = 512;
			break;
		case DEVICE_CTRL_GET_BLOCK_SIZE:
			*(size_t*)args = 4096;
			break;
		case DEVICE_CTRL_GET_SECTOR_COUNT:
			fp = fopen(name,"rb");
			asAssert(fp);
			fseek(fp, 0L, SEEK_END);
			size = ftell(fp);
			fclose(fp);
			*(size_t*)args = size/512;
			break;
		case DEVICE_CTRL_GET_DISK_SIZE:
			fp = fopen(name,"rb");
			asAssert(fp);
			fseek(fp, 0L, SEEK_END);
			size = ftell(fp);
			fclose(fp);
			*(size_t*)args = size;
		break;
		default:
			ercd = EINVAL;
			break;
	}

	return ercd;
}
#endif
/* ============================ [ FUNCTIONS ] ====================================================== */
#ifdef USE_FATFS
#ifndef USE_DEV
DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	DSTATUS stat = STA_NOINIT;

	ASLOG(FATFS, ("%s %d\n", __func__, pdrv));

	switch (pdrv) {
	case DEV_RAM :
		break;

	case DEV_MMC :
		stat = RES_OK;
		break;

	case DEV_USB :
		break;
	}
	return stat;
}

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	DSTATUS stat = STA_NOINIT;
	ASLOG(FATFS, ("%s %d\n", __func__, pdrv));
	switch (pdrv) {
	case DEV_RAM :
		break;

	case DEV_MMC :
	{
		FILE* fp = fopen(FATFS_IMG,"rb");
		if(NULL == fp)
		{
			fp = fopen(FATFS_IMG,"wb+");
			asAssert(fp);
			for(int i=0;i<32*1024*1024;i++)
			{
				uint8_t data = 0xFF;
				fwrite(&data,1,1,fp);
			}
			fclose(fp);
			ASLOG(FATFS, ("simulation on new created 32Mb %s\n", FATFS_IMG));
		}
		else
		{
			ASLOG(FATFS, ("simulation on old %s\n", FATFS_IMG));
			fclose(fp);
		}
		stat = 0;
		break;
	}
	case DEV_USB :
		break;
	}
	return stat;
}

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	DRESULT res = RES_PARERR;
	ASLOG(FATFS, ("%s %d %d %d\n", __func__, pdrv, sector, count));
	switch (pdrv) {
	case DEV_RAM :
		break;

	case DEV_MMC :
	{
		FILE* fp=fopen(FATFS_IMG,"rb");
		asAssert(fp);
		fseek(fp,512*sector,SEEK_SET);
		int len=fread(buff,sizeof(char),count*512,fp);

		if(len!=count*512)
		{
			res=RES_ERROR;
		}
		else
		{
			res = RES_OK;
		}
		fclose(fp);
		break;
	}

	case DEV_USB :
		break;
	}

	return res;
}

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	DRESULT res = RES_PARERR;
	ASLOG(FATFS, ("%s %d %d %d\n", __func__, pdrv, sector, count));
	switch (pdrv) {
	case DEV_RAM :
		break;

	case DEV_MMC :
	{
		FILE* fp=fopen(FATFS_IMG,"rb+");
		asAssert(fp);
		fseek(fp,512*sector,SEEK_SET);
		int len=fwrite(buff,sizeof(char),count*512,fp);

		if(len!=count*512)
		{
			res=RES_ERROR;
		}
		else
		{
			res = RES_OK;
		}
		fclose(fp);
		break;
	}
	case DEV_USB :
		break;
	}

	return res;
}

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	DRESULT res = RES_PARERR;
	ASLOG(FATFS, ("%s %d %d\n", __func__, pdrv, cmd));
	switch (pdrv) {
	case DEV_RAM :
		break;

	case DEV_MMC :
	{
		switch (cmd) {
		case CTRL_SYNC:
			res = RES_OK;
			break;

		case GET_SECTOR_COUNT:
		{
			FILE* fp = fopen(FATFS_IMG,"rb");
			asAssert(fp);
			fseek(fp, 0L, SEEK_END);
			*(DWORD*)buff = ftell(fp)/512;
			res = RES_OK;
			fclose(fp);
			break;
		}
		case GET_SECTOR_SIZE:
			*(DWORD*)buff = 512;
			res = RES_OK;
			break;

		case GET_BLOCK_SIZE:
			*(DWORD*)buff = 1024*1024; /* 1M */
			res = RES_OK;
			break;

		}
		break;
	}
	case DEV_USB :
		break;
	}

	return res;
}

DWORD get_fattime (void)
{
	return time(0);
}
#endif
#endif

#ifdef USE_LWEXT4
#ifndef USE_DEV
void ext_mount(void)
{
    int rc;
    struct ext4_blockdev * bd;
    FILE* fp = fopen(EXTFS_IMG,"rb");
    if(NULL == fp)
    {
		fp = fopen(EXTFS_IMG,"wb+");
		asAssert(fp);
		for(int i=0;i<32*1024*1024;i++)
		{
			uint8_t data = 0xFF;
			fwrite(&data,1,1,fp);
		}
		fclose(fp);
        ASLOG(EXTFS, ("simulation on new created 32Mb " EXTFS_IMG "\n"));
    }
    else
    {
        ASLOG(EXTFS, ("simulation on old " EXTFS_IMG "\n"));
        fclose(fp);
    }

    file_dev_name_set(EXTFS_IMG);
    bd = file_dev_get();

    rc = ext4_device_register(bd, EXTFS_IMG);
    if(rc != EOK)
    {
        ASLOG(EXTFS, ("register ext4 device failed\n"));
    }

	rc = ext4_mount(EXTFS_IMG, "/", false);
	if (rc != EOK)
    {
		static struct ext4_fs fs;
		static struct ext4_mkfs_info info = {
			.block_size = 4096,
			.journal = true,
		};

		ASWARNING(EXTFS_IMG " is invalid, do mkfs!\n");

		rc = ext4_mkfs(&fs, bd, &info, F_SET_EXT4);
		if (rc != EOK)
		{
			ASLOG(ERROR, ("ext4_mkfs error: %d\n", rc));
		}
		else
		{
			rc = ext4_mount(EXTFS_IMG, "/", false);
			if (rc != EOK)
			{
				ASLOG(EXTFS, ("mount ext4 device failed\n"));
			}
		}
    }

    ASLOG(EXTFS, ("mount ext4 device " EXTFS_IMG " on '/' OK\n"));
}
#endif
int __weak fseeko (FILE *stream, off_t offset, int whence)
{
	return fseek(stream, offset, whence);
}

off_t __weak ftello (FILE *stream)
{
	return ftello(stream);
}
#endif /* USE_LWEXT4 */

#ifdef FATFS_NATIVE_TOOL
#include "ff.h"
int main(int argc, char* argv[])
{
	int rc,len;
	UINT bw;
	FILE* fp;
	FIL fil;
	FATFS FatFs;
	char buf[512];
	if( (3==argc) &&
		(0==strcmp(argv[1],"mkfs")) )
	{
		FATFS_IMG = argv[2];
		rc = f_mkfs("", FM_ANY, 0, FatFs.win, sizeof(FatFs.win));
		if(FR_OK != rc)
		{
			printf("FatFS mkfs failed with error code %d\n", rc);
			return -1;
		}
		printf("mkfs.fatfs %s\n",FATFS_IMG);
	}
	else if( (5==argc) &&
			 (0==strcmp(argv[1],"cp")) )
	{
		FATFS_IMG = argv[4];
		rc = f_mount(&FatFs, "", 1);
		if(FR_OK != rc)
		{
			printf("FatFS mount failed with error code %d\n", rc);
			return -1;
		}
		fp = fopen(argv[2],"rb");
		if(NULL == fp)
		{
			printf("open %s failed!\n",argv[2]);
			return -1;
		}
		rc = f_open(&fil, argv[3], FA_WRITE|FA_CREATE_ALWAYS);
		if(FR_OK != rc)
		{
			printf("open %s failed with error code %d!\n",argv[3],rc);
			return -1;
		}
		do {
			len = fread(buf, 1, sizeof(buf), fp);
			if(len > 0)
			{
				rc = f_write(&fil, buf, len, &bw);
				if(FR_OK != rc)
				{
					printf("write %s failed with error code %d!\n",argv[3],rc);
					return -1;
				}
			}
		}while(len > 0);
		fclose(fp);
		f_close(&fil);
		printf("cp %s to %s:%s\n", argv[2], FATFS_IMG, argv[3]);
	}
	else
	{
		printf("%s: invalid args\n", argv[0]);
	}

	return 0;
}
#endif
