/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2019 AS <parai@foxmail.com>
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
#ifndef _DEVICE_H_
#define _DEVICE_H_
/* ============================ [ INCLUDES  ] ====================================================== */
#include "Std_Types.h"
#include <stddef.h>
#include <stdint.h>
#include <sys/queue.h>
#include <errno.h>
/* ============================ [ MACROS    ] ====================================================== */
/* controls for block devices */
#define DEVICE_CTRL_GET_SECTOR_SIZE  0
#define DEVICE_CTRL_GET_BLOCK_SIZE   1
#define DEVICE_CTRL_GET_SECTOR_COUNT 2
#define DEVICE_CTRL_GET_DISK_SIZE    3
/* ============================ [ TYPES     ] ====================================================== */
typedef struct device device_t;

typedef struct
{
	int (*open)  (const device_t* device);
	int (*close) (const device_t* device);
	int (*read)  (const device_t* device, size_t pos, void *buffer, size_t size);
	int (*write) (const device_t* device, size_t pos, const void *buffer, size_t size);
	int (*ctrl)  (const device_t* device, int cmd,    void *args);
} device_ops_t;

struct device
{
	const char* name;
	const device_ops_t ops;
	void* priv;
};
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
void device_init(void);
int device_register(const device_t* device);
int device_unregister(const device_t* device);
const device_t* device_find(const char* name);
#endif /* _DEVICE_H_ */
