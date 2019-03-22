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
#ifdef USE_PCI
/* ============================ [ INCLUDES  ] ====================================================== */
#include "Std_Types.h"
#include "pci_core.h"
#include "asdebug.h"
#ifdef USE_STDRT
#include "rtthread.h"
#include "rthw.h"
#endif

#ifdef USE_DEV
#include "device.h"
#endif

/* ============================ [ MACROS    ] ====================================================== */
enum {
	IMG_FATFS = 0,
	IMG_EXT4,
	IMG_BLK2,
	IMG_BLK3,
	IMG_MAX
};
enum{
	REG_BLKID     = 0x00,
	REG_BLKSZ     = 0x04,
	REG_BLKNBR    = 0x08,
	REG_DATA      = 0x0C,
	REG_LENGTH    = 0x10,
	REG_BLKSTATUS = 0x14,
	REG_CMD       = 0x18,
};

/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
int PciBlk_Init(uint32_t blkid);
int PciBlk_Read(uint32_t blkid, uint32_t blksz, uint32_t blknbr, uint8_t* data);
int PciBlk_Write(uint32_t blkid, uint32_t blksz, uint32_t blknbr, const uint8_t* data);
int PciBlk_Size(uint32_t blkid, uint32_t *size);
#ifdef USE_DEV
static int asblk_open  (const device_t* device);
static int asblk_close (const device_t* device);
static int asblk_read  (const device_t* device, size_t pos, void *buffer, size_t size);
static int asblk_write (const device_t* device, size_t pos, const void *buffer, size_t size);
static int asblk_ctrl  (const device_t* device, int cmd,    void *args);
#endif
/* ============================ [ DATAS     ] ====================================================== */
static pci_dev *pdev = NULL;
static void* __iobase= NULL;

#ifdef RT_USING_DFS
static struct rt_device devF[IMG_MAX];
static struct rt_mutex lock[IMG_MAX];
static struct rt_semaphore sem[IMG_MAX];
#endif /* RT_USING_DFS */

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
	uint32_t blkid = (uint32_t)(unsigned long)device->priv;
	return PciBlk_Init(blkid);
}

static int asblk_close (const device_t* device)
{
	return 0;
}
static int asblk_read  (const device_t* device, size_t pos, void *buffer, size_t size)
{
	uint32_t blkid = (uint32_t)(unsigned long)device->priv;

	while(size > 0)
	{
		PciBlk_Read(blkid,512,pos,buffer);
		size--;
		pos++;
		buffer += 512;
	}

	return 0;
}
static int asblk_write (const device_t* device, size_t pos, const void *buffer, size_t size)
{
	uint32_t blkid = (uint32_t)(unsigned long)device->priv;

	while(size > 0)
	{
		PciBlk_Write(blkid,512,pos,buffer);
		size--;
		pos++;
		buffer += 512;
	}

	return 0;
}
static int asblk_ctrl  (const device_t* device, int cmd,    void *args)
{
	uint32_t blkid = (uint32_t)(unsigned long)device->priv;
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
			ercd = PciBlk_Size(blkid, &size);
			*(size_t*)args = size/512;
			break;
		case DEVICE_CTRL_GET_DISK_SIZE:
			ercd = PciBlk_Size(blkid, &size);
			*(size_t*)args = size;
		break;
		default:
			ercd = EINVAL;
			break;
	}

	return ercd;
}
#endif
#ifdef RT_USING_DFS
static rt_err_t rt_asblk_init_internal(rt_device_t dev)
{
    return RT_EOK;
}

static rt_err_t rt_asblk_open(rt_device_t dev, rt_uint16_t oflag)
{
    int blkid = (int)dev->user_data;
	PciBlk_Init(blkid);
    return RT_EOK;
}

static rt_err_t rt_asblk_close(rt_device_t dev)
{
    return RT_EOK;
}

/* position: block page address, not bytes address
 * buffer:
 * size  : how many blocks
 */
static rt_size_t rt_asblk_read(rt_device_t dev, rt_off_t position, void *buffer, rt_size_t size)
{
	rt_size_t doSize = size;

    int blkid = (int)dev->user_data;

    rt_mutex_take(&lock[blkid], RT_WAITING_FOREVER);


	while(size > 0)
	{
		PciBlk_Read(blkid,512,position,buffer);
		size--;
		position++;
		buffer += 512;
	}

	rt_mutex_release(&lock[blkid]);

    return doSize;
}

/* position: block page address, not bytes address
 * buffer:
 * size  : how many blocks
 */
static rt_size_t rt_asblk_write(rt_device_t dev, rt_off_t position, const void *buffer, rt_size_t size)
{

	rt_size_t doSize = size;

    int blkid = (int)dev->user_data;

    rt_mutex_take(&lock[blkid], RT_WAITING_FOREVER);

	while(size > 0)
	{
		PciBlk_Read(blkid,512,position,buffer);
		size--;
		position ++;
		buffer += 512;
	}
	rt_mutex_release(&lock[blkid]);

    return doSize;
}

static rt_err_t rt_asblk_control(rt_device_t dev, int cmd, void *args)
{
    RT_ASSERT(dev != RT_NULL);

    if (cmd == RT_DEVICE_CTRL_BLK_GETGEOME)
    {
        uint32_t size;
        int blkid = (int)dev->user_data;
        struct rt_device_blk_geometry *geometry;

        PciBlk_Size(blkid, &size);
        geometry = (struct rt_device_blk_geometry *)args;
        if (geometry == RT_NULL) return -RT_ERROR;

        geometry->bytes_per_sector = 512;
        geometry->block_size = 512;

        geometry->sector_count = size / 512;
    }

    return RT_EOK;
}
#endif /* RT_USING_DFS */
/* ============================ [ FUNCTIONS ] ====================================================== */
int PciBlk_Init(uint32_t blkid)
{
	imask_t mask;
	if(NULL == __iobase)
	{
		pdev = find_pci_dev_from_id(0xcaac,0x0003);
		if(NULL != pdev)
		{
			__iobase = pci_get_memio(pdev, 1);
			enable_pci_resource(pdev);
		}
	}

	asAssert(__iobase);

	Irq_Save(mask);
	writel(__iobase+REG_BLKID, blkid);
	writel(__iobase+REG_CMD, 0); /* cmd init */
	Irq_Restore(mask);

	return 0;
}

int PciBlk_Read(uint32_t blkid, uint32_t blksz, uint32_t blknbr, uint8_t* data)
{
	uint32_t i;
	imask_t mask;

	asAssert(__iobase);

	Irq_Save(mask);
	writel(__iobase+REG_BLKID, blkid);
	writel(__iobase+REG_BLKSZ, blksz);
	writel(__iobase+REG_BLKNBR, blknbr);
	writel(__iobase+REG_CMD, 1); /* cmd read */
	for(i=0; i < blksz; i++)
	{
		data[i] = readl(__iobase+REG_DATA);
	}
	Irq_Restore(mask);

	return 0;
}

int PciBlk_Write(uint32_t blkid, uint32_t blksz, uint32_t blknbr, const uint8_t* data)
{
	uint32_t i;
	imask_t mask;

	asAssert(__iobase);

	Irq_Save(mask);
	writel(__iobase+REG_BLKID, blkid);
	writel(__iobase+REG_BLKSZ, blksz);
	writel(__iobase+REG_BLKNBR, blknbr);

	for(i=0; i < blksz; i++)
	{
		writel(__iobase+REG_DATA,data[i]);
	}

	writel(__iobase+REG_CMD, 2); /* cmd write */
	Irq_Restore(mask);

	return 0;
}

int PciBlk_Size(uint32_t blkid, uint32_t *size)
{
	imask_t mask;

	asAssert(__iobase);

	Irq_Save(mask);
	writel(__iobase+REG_BLKID, blkid);

	*size = readl(__iobase+REG_LENGTH);
	Irq_Restore(mask);

	return 0;
}

#ifdef RT_USING_DFS
void rt_hw_asblk_init(int blkid)
{
    struct rt_device *device;
    char buffer[16] = "asblk0";

    if( blkid >= IMG_MAX) return;

    rt_mutex_init(&lock[blkid],"fdlock", RT_IPC_FLAG_FIFO);
	rt_sem_init(&sem[blkid], "fdsem", 0, RT_IPC_FLAG_FIFO);

    device = &(devF[blkid]);

    device->type  = RT_Device_Class_Block;
    device->init = rt_asblk_init_internal;
    device->open = rt_asblk_open;
    device->close = rt_asblk_close;
    device->read = rt_asblk_read;
    device->write = rt_asblk_write;
    device->control = rt_asblk_control;
    device->user_data = (void*) blkid;

    buffer[5] = '0' + blkid;
    rt_device_register(device, buffer,
                       RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_REMOVABLE | RT_DEVICE_FLAG_STANDALONE);

}

void rt_hw_asblk_init_all(void)
{
    int i;
    for(i=0; i < IMG_MAX; i++)
    {
        rt_hw_asblk_init(i);
    }
}
#endif /* RT_USING_DFS */

#endif /* USE_PCI */
