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
#ifdef USE_LWEXT4
/* ============================ [ INCLUDES  ] ====================================================== */
#include "vfs.h"
#include "ext4.h"
#include "ext4_mkfs.h"
#include "asdebug.h"
#include "device.h"
/* ============================ [ MACROS    ] ====================================================== */
#define AS_LOG_LWEXT 0
#define AS_LOG_LWEXTE 1
#define TO_LWEXT_PATH(f) (&((f)[4]))
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
extern const struct vfs_filesystem_ops lwext_ops;
extern const device_t device_asblk1;

static int blockdev_open(struct ext4_blockdev *bdev);
static int blockdev_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
			 uint32_t blk_cnt);
static int blockdev_bwrite(struct ext4_blockdev *bdev, const void *buf,
			  uint64_t blk_id, uint32_t blk_cnt);
static int blockdev_close(struct ext4_blockdev *bdev);
static int blockdev_lock(struct ext4_blockdev *bdev);
static int blockdev_unlock(struct ext4_blockdev *bdev);
/* ============================ [ DATAS     ] ====================================================== */
static const device_t* lwext_device_table[CONFIG_EXT4_BLOCKDEVS_COUNT] = {
	&device_asblk1,
};

static size_t disk_sector_size[CONFIG_EXT4_BLOCKDEVS_COUNT] = {0};

EXT4_BLOCKDEV_STATIC_INSTANCE(ext4_blkdev0, 4096, 0, blockdev_open,
			      blockdev_bread, blockdev_bwrite, blockdev_close,
			      blockdev_lock, blockdev_unlock);

#if CONFIG_EXT4_BLOCKDEVS_COUNT > 1
EXT4_BLOCKDEV_STATIC_INSTANCE(ext4_blkdev1, 4096, 0, blockdev_open,
			      blockdev_bread, blockdev_bwrite, blockdev_close,
			      blockdev_lock, blockdev_unlock);
#endif
#if CONFIG_EXT4_BLOCKDEVS_COUNT > 2
EXT4_BLOCKDEV_STATIC_INSTANCE(ext4_blkdev2, 4096, 0, blockdev_open,
			      blockdev_bread, blockdev_bwrite, blockdev_close,
			      blockdev_lock, blockdev_unlock);
#endif
#if CONFIG_EXT4_BLOCKDEVS_COUNT > 3
EXT4_BLOCKDEV_STATIC_INSTANCE(ext4_blkdev3, 4096, 0, blockdev_open,
			      blockdev_bread, blockdev_bwrite, blockdev_close,
			      blockdev_lock, blockdev_unlock);
#endif

#if CONFIG_EXT4_BLOCKDEVS_COUNT > 4
#error vfs_lwext4 by default support only 4 partitions!
#endif

static struct ext4_blockdev * const ext4_blkdev_list[CONFIG_EXT4_BLOCKDEVS_COUNT] =
{
	&ext4_blkdev0,
#if CONFIG_EXT4_BLOCKDEVS_COUNT > 1
	&ext4_blkdev1,
#endif
#if CONFIG_EXT4_BLOCKDEVS_COUNT > 2
	&ext4_blkdev2,
#endif
#if CONFIG_EXT4_BLOCKDEVS_COUNT > 3
	&ext4_blkdev3,
#endif
};
/* ============================ [ LOCALS    ] ====================================================== */
static VFS_FILE* lwext_fopen (const char *filename, const char *opentype)
{
	VFS_FILE *f;
	int r;

	ASLOG(LWEXT, "fopen(%s,%s)\n", filename, opentype);

	f = malloc(sizeof(VFS_FILE));
	if(NULL != f)
	{
		f->priv = malloc(sizeof(ext4_file));
		if(NULL == f->priv)
		{
			free(f);
			f = NULL;
		}
		else
		{
			r = ext4_fopen(f->priv, TO_LWEXT_PATH(filename), opentype);
			if (0 != r)
			{
				free(f->priv);
				free(f);
				f = NULL;
			}
			else
			{
				f->fops = &lwext_ops;
			}
		}
	}

	return f;
}

static int lwext_fclose (VFS_FILE* stream)
{
	int r;

	r = ext4_fclose(stream->priv);

	if (0 == r)
	{
		free(stream->priv);
		free(stream);
	}

	return r;
}

static int lwext_fread (void *data, size_t size, size_t count, VFS_FILE *stream)
{
	size_t bytesread = 0;
	int r;

	r = ext4_fread(stream->priv, data, size*count, &bytesread);
	if (0 != r)
	{
		bytesread = 0;
	}

	return (bytesread/size);
}

static int lwext_fwrite (const void *data, size_t size, size_t count, VFS_FILE *stream)
{
	size_t byteswritten = 0;
	int r;

	r = ext4_fwrite(stream->priv, data, size*count, &byteswritten);
	if (0 != r)
	{
		byteswritten = 0;
	}

	return (byteswritten/size);
}

static int lwext_fflush (VFS_FILE *stream)
{
	(void) stream;

	return ENOTSUP;
}

static int lwext_fseek (VFS_FILE *stream, long int offset, int whence)
{
	int r;

	r = ext4_fseek(stream->priv, offset, whence);

	return r;
}

static size_t lwext_ftell (VFS_FILE *stream)
{
	return ext4_ftell(stream->priv);
}

static int lwext_unlink (const char *filename)
{
	int r;

	ASLOG(LWEXT, "unlink(%s)\n", filename);

	r = ext4_fremove(TO_LWEXT_PATH(filename));

	return r;
}

static int lwext_stat (const char *filename, vfs_stat_t *buf)
{
	int r = ENOENT;

	ASLOG(LWEXT, "stat(%s)\n", filename);

	if(('\0' == TO_LWEXT_PATH(filename)[0])
		|| (0 == strcmp(TO_LWEXT_PATH(filename),"/")) )
	{	/* just the root */
		buf->st_mode = S_IFDIR;
		buf->st_size = 0;

		r = 0;
	}
	else
	{
		union {
			ext4_dir dir;
			ext4_file f;
		} var;

		r = ext4_dir_open(&(var.dir), TO_LWEXT_PATH(filename));

		if(0 == r)
		{
			(void) ext4_dir_close(&(var.dir));
			buf->st_mode = S_IFDIR;
			buf->st_size = 0;
		}
		else
		{
			r = ext4_fopen(&(var.f), TO_LWEXT_PATH(filename), "rb");
			if( 0 == r)
			{
				buf->st_mode = S_IFREG;
				buf->st_size = ext4_fsize(&(var.f));
				(void)ext4_fclose(&(var.f));
			}
		}
	}

	return r;
}

static VFS_DIR * lwext_opendir (const char *dirname)
{
	VFS_DIR* dir;

	ASLOG(LWEXT, "opendir(%s)\n", dirname);

	dir = malloc(sizeof(VFS_DIR));

	if(NULL != dir)
	{
		const char* p;
		dir->fops = &lwext_ops;
		dir->priv = malloc(sizeof(ext4_dir));

		if(('\0' == TO_LWEXT_PATH(dirname)[0]))
		{
			p = "/";
		}
		else
		{
			p = TO_LWEXT_PATH(dirname);
		}

		if(NULL != dir->priv)
		{
			int r;
			r = ext4_dir_open(dir->priv, p);

			if(0 != r)
			{
				free(dir->priv);
				free(dir);
				dir = NULL;
				ASLOG(LWEXT, "opendir(%s) failed!(%d)\n", p, r);
			}
		}
		else
		{
			free(dir);
			dir = NULL;
		}
	}

	asAssert(dir);
	return dir;

}

static vfs_dirent_t * lwext_readdir (VFS_DIR *dirstream)
{
	const ext4_direntry * rentry;
	vfs_dirent_t * rdirent;

	static vfs_dirent_t dirent;

	rentry = ext4_dir_entry_next(dirstream->priv);

	if(NULL != rentry)
	{
		dirent.d_namlen = rentry->name_length;

		strcpy(dirent.d_name, rentry->name);

		rdirent = &dirent;

	}
	else
	{
		rdirent = NULL;
	}

	return rdirent;

}

static int lwext_closedir (VFS_DIR *dirstream)
{
	(void)ext4_fclose(dirstream->priv);

	free(dirstream->priv);
	free(dirstream);

	return 0;
}

static int lwext_chdir (const char *filename)
{

	int r = ENOTDIR;

	ASLOG(LWEXT, "chdir(%s)\n", filename);

	if(('\0' == TO_LWEXT_PATH(filename)[0]))
	{
		r = 0;
	}
	else
	{
		r = ext4_inode_exist(TO_LWEXT_PATH(filename), EXT4_DE_DIR);;
	}

	return r;

}

static int lwext_mkdir (const char *filename, uint32_t mode)
{
	int r;

	ASLOG(LWEXT, "mkdir(%s, 0x%x)\n", filename, mode);

	r = ext4_dir_mk(TO_LWEXT_PATH(filename));

	return r;
}

static int lwext_rmdir (const char *filename)
{
	int r;

	ASLOG(LWEXT, "rmdir(%s)\n", filename);

	r = ext4_dir_rm(TO_LWEXT_PATH(filename));

	return r;
}

static int lwext_rename (const char *oldname, const char *newname)
{
	int r;

	ASLOG(LWEXT, "rename (%s, %s)\n", oldname, newname);

	r = ext4_frename(TO_LWEXT_PATH(oldname), TO_LWEXT_PATH(newname));

	return r;
}



void ext_mount(void)
{
	int rc;
	struct ext4_blockdev * bd;
	const device_t* device = lwext_device_table[0];
	bd = ext4_blkdev_list[0];

	rc = ext4_device_register(bd, device->name);
	if(rc != EOK)
	{
		ASLOG(ERROR, "register ext4 device failed\n");
	}

	rc = ext4_mount(device->name, "/", false);
	if (rc != EOK)
	{
		static struct ext4_fs fs;
		static struct ext4_mkfs_info info = {
			.block_size = 4096,
			.journal = TRUE,
		};

		ASWARNING("%s is invalid, do mkfs!\n", device->name);

		rc = ext4_mkfs(&fs, bd, &info, F_SET_EXT4);
		if (rc != EOK)
		{
			ASLOG(ERROR,"ext4_mkfs error: %d\n", rc);
		}
		else
		{
			rc = ext4_mount(device->name, "/", false);
			if (rc != EOK)
			{
				ASLOG(ERROR, "mount ext4 device failed\n");
			}
		}
	}

	ASLOG(LWEXT, "mount ext4 device %s on '/' OK\n", device->name);
}


static int get_bdev(struct ext4_blockdev * bdev)
{
	int index;
	int ret = -1;

	for (index = 0; index < CONFIG_EXT4_BLOCKDEVS_COUNT; index ++)
	{
		if (ext4_blkdev_list[index] == bdev){
			ret = index;
			break;
		}
	}

	return ret;
}

static int blockdev_open(struct ext4_blockdev *bdev)
{
	size_t size;
	int index;
	int ret = -1;
	const device_t *device;

	index = get_bdev(bdev);

	if(index >= 0)
	{
		device = lwext_device_table[index];
		if( (device != NULL) &&
			(device->ops.open != NULL) &&
			(device->ops.ctrl != NULL) )
		{
			ret = device->ops.open(device);
			ret += device->ops.ctrl(device, DEVICE_CTRL_GET_SECTOR_SIZE, &disk_sector_size[index]);
			ret += device->ops.ctrl(device, DEVICE_CTRL_GET_DISK_SIZE, &size);
		}
	}

	if(0 == ret)
	{
		bdev->part_offset = 0;
		bdev->part_size = size;
		bdev->bdif->ph_bcnt = bdev->part_size / bdev->bdif->ph_bsize;
	}

	return ret;

}

static int blockdev_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
			 uint32_t blk_cnt)
{
	int index;
	int ret = -1;
	const device_t *device;

	index = get_bdev(bdev);

	if(index >= 0)
	{
		device = lwext_device_table[index];
		if( (device != NULL) &&
			(device->ops.read != NULL) )
		{
			ret = device->ops.read(device,
					blk_id*(bdev->bdif->ph_bsize/disk_sector_size[index]),
					buf, blk_cnt*(bdev->bdif->ph_bsize/disk_sector_size[index]));
		}
	}

	return ret;
}


static int blockdev_bwrite(struct ext4_blockdev *bdev, const void *buf,
			  uint64_t blk_id, uint32_t blk_cnt)
{
	int index;
	int ret = -1;
	const device_t *device;

	index = get_bdev(bdev);

	if(index >= 0)
	{
		device = lwext_device_table[index];
		if( (device != NULL) &&
			(device->ops.write != NULL) )
		{
			ret = device->ops.write(device,
					blk_id*(bdev->bdif->ph_bsize/disk_sector_size[index]),
					buf, blk_cnt*(bdev->bdif->ph_bsize/disk_sector_size[index]));
		}
	}

	return ret;
}

static int blockdev_close(struct ext4_blockdev *bdev)
{
	return 0;
}

static int blockdev_lock(struct ext4_blockdev *bdev)
{
	return 0;
}

static int blockdev_unlock(struct ext4_blockdev *bdev)
{
	return 0;
}
/* ============================ [ FUNCTIONS ] ====================================================== */
const struct vfs_filesystem_ops lwext_ops =
{
	.name = "/ext",
	.fopen = lwext_fopen,
	.fclose = lwext_fclose,
	.fread = lwext_fread,
	.fwrite = lwext_fwrite,
	.fflush = lwext_fflush,
	.fseek = lwext_fseek,
	.ftell = lwext_ftell,
	.unlink = lwext_unlink,
	.stat = lwext_stat,
	.opendir = lwext_opendir,
	.readdir = lwext_readdir,
	.closedir = lwext_closedir,
	.chdir = lwext_chdir,
	.mkdir = lwext_mkdir,
	.rmdir = lwext_rmdir,
	.rename = lwext_rename
};
#endif
