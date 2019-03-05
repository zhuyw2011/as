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
#include "vfs.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/queue.h>
#include "asdebug.h"
#ifdef USE_SHELL
#include "shell.h"
#endif
/* ============================ [ MACROS    ] ====================================================== */
#define AS_LOG_VFS 0
#define in_range(c, lo, up)  ((uint8_t)c >= lo && (uint8_t)c <= up)
#define isprint(c)           in_range(c, 0x20, 0x7f)
#ifndef VFS_FPRINTF_BUFFER_SIZE
#define VFS_FPRINTF_BUFFER_SIZE 512
#endif

#ifndef VFS_LOCK
#define VFS_LOCK() do { imask_t mask; Irq_Save(mask)
#endif

#ifndef VFS_UNLOCK
#define VFS_UNLOCK() Irq_Restore(mask); } while(0)
#endif
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
#ifdef USE_FATFS
extern const struct vfs_filesystem_ops fatfs_ops;
#endif
#ifdef USE_LWEXT4
extern const struct vfs_filesystem_ops lwext_ops;
#endif
#if defined(__WINDOWS__) || defined(__LINUX__)
extern const struct vfs_filesystem_ops hofs_ops;
#endif

#ifdef USE_SHELL
static int lsFunc(int argc, char* argv[]);
static int chdirFunc(int argc, char* argv[]);
static int mkdirFunc(int argc, char* argv[]);
static int rmFunc(int argc, char* argv[]);
static int cpFunc(int argc, char* argv[]);
static int pwdFunc(int argc, char* argv[]);
static int catFunc(int argc, char* argv[]);
static int hexdumpFunc(int argc, char* argv[]);
#endif
/* ============================ [ DATAS     ] ====================================================== */
static const struct vfs_filesystem_ops* vfs_ops[] =
{
#ifdef USE_FATFS
	&fatfs_ops,
#endif
#ifdef USE_LWEXT4
	&lwext_ops,
#endif
#if defined(__WINDOWS__) || defined(__LINUX__)
	&hofs_ops,
#endif
	NULL
};

static TAILQ_HEAD(vfs_mount_head, vfs_mount_s) vfs_mount_list = TAILQ_HEAD_INITIALIZER(vfs_mount_list);

static char vfs_cwd[FILENAME_MAX] = "/";

#ifdef USE_SHELL
static SHELL_CONST ShellCmdT lsVfsCmd  = {
	lsFunc,
	0,1,
	"ls",
	"ls [path]",
	"list files of current directory or path directory\n",
	{NULL,NULL}
};
SHELL_CMD_EXPORT(lsVfsCmd);

static SHELL_CONST ShellCmdT chdirVfsCmd  = {
	chdirFunc,
	0,1,
	"cd",
	"cd path",
	"change current working directory\n",
	{NULL,NULL}
};
SHELL_CMD_EXPORT(chdirVfsCmd);

static SHELL_CONST ShellCmdT pwdVfsCmd  = {
	pwdFunc,
	0,0,
	"pwd",
	"pwd",
	"show full path of current working directory\n",
	{NULL,NULL}
};
SHELL_CMD_EXPORT(pwdVfsCmd);

static SHELL_CONST ShellCmdT mkdirVfsCmd  = {
	mkdirFunc,
	1,1,
	"mkdir",
	"mkdir path",
	"making a directory specified by path\n",
	{NULL,NULL}
};
SHELL_CMD_EXPORT(mkdirVfsCmd);

static SHELL_CONST ShellCmdT rmVfsCmd  = {
	rmFunc,
	1,1,
	"rm",
	"rm path",
	"remove a directory or file specified by path\n",
	{NULL,NULL}
};
SHELL_CMD_EXPORT(rmVfsCmd);

static SHELL_CONST ShellCmdT cpVfsCmd  = {
	cpFunc,
	2,2,
	"cp",
	"cp file path",
	"copy a file to another path\n",
	{NULL,NULL}
};
SHELL_CMD_EXPORT(cpVfsCmd);

static SHELL_CONST ShellCmdT catVfsCmd  = {
	catFunc,
	1,1,
	"cat",
	"cat file",
	"show file content in ascii mode\n",
	{NULL,NULL}
};
SHELL_CMD_EXPORT(catVfsCmd);

static SHELL_CONST ShellCmdT hexdumpVfsCmd  = {
	hexdumpFunc,
	1,5,
	"hexdump",
	"hexdump file [-s offset -n size]",
	"show file content in hex mode\n",
	{NULL,NULL}
};
SHELL_CMD_EXPORT(hexdumpVfsCmd);
#endif
/* ============================ [ LOCALS    ] ====================================================== */
static char* serach(char* const p, const char* file)
{
	VFS_DIR* dir;
	vfs_dirent_t *dirent;
	vfs_stat_t stat;
	size_t len;
	size_t lenp;
	char* cs;

	len = strlen(file);
	lenp = strlen(p);
	if('/' != p[lenp-1]) {
		p[lenp] = '/';
		p[lenp+1] = '\0';
		lenp += 1;
	}
	cs = &p[lenp];

	strcpy(cs,file);
	if((0 == vfs_stat(p, &stat)) && VFS_ISREG(stat.st_mode))
	{
		return p;
	}

	cs[0] = '\0';
	dir = vfs_opendir(p);
	if(NULL != dir)
	{
		dirent = vfs_readdir(dir);
		while(NULL != dirent)
		{
			strcpy(cs,dirent->d_name);
			if((0 == vfs_stat(p, &stat)) && VFS_ISDIR(stat.st_mode))
			{
				if( (0 != strcmp(dirent->d_name,".")) &&
					(0 != strcmp(dirent->d_name,"..")) )
				{
					char* r = serach(p, file);
					if(r)
					{
						vfs_closedir(dir);
						return r;
					}
				}
			}
			dirent = vfs_readdir(dir);
		}
		vfs_closedir(dir);
	}

	return NULL;
}

static const struct vfs_filesystem_ops* search_ops(const char *type)
{
	const struct vfs_filesystem_ops *ops, **o;

	o = vfs_ops;
	ops = NULL;
	while(*o != NULL)
	{
		if(0 == strcmp((*o)->name, type))
		{
			ops = *o;
			break;
		}
		o++;
	}

	ASLOG(VFS, "search_ops(%s) = %s\n", type, (NULL != ops) ? ops->name : NULL);

	return ops;
}

static const vfs_mount_t* search_mnt(const char *filepath)
{
	const vfs_mount_t *mnt, *m;
	int len;
	int best = 0;

	mnt = NULL;

	VFS_LOCK();
	TAILQ_FOREACH(m, &vfs_mount_list, entry)
	{
		len = strlen(m->mount_point);
		if(0 == strncmp(m->mount_point, filepath, len))
		{
			if(best< len)
			{
				mnt = m;
				best = len;
			}
		}
	}
	VFS_UNLOCK();

	ASLOG(VFS, "search_mnt(%s) = %s\n", filepath, (NULL != mnt) ? mnt->mount_point : NULL);

	return mnt;
}

static char* relpath(const char * path)
{
	char* abspath;
	char* p;
	const char* s;

	abspath = malloc(FILENAME_MAX);

	if(NULL != abspath)
	{
		p = abspath;

		memset(p,0,FILENAME_MAX);

		if('/' != path[0])
		{ 	/* relative path */
			s = vfs_cwd;
			while('\0' != *s)
			{
				*p = *s;
				p++;
				s++;
			}

			if(*(p-1) != '/')
			{
				*p = '/';
				p++;
			}

			s = path;
		}
		else
		{
			s = path;
		}

		while('\0' != *s)
		{
			if(('.' == *s) && ('.' == *(s+1)))
			{
				if(('/' == *(p-1)) && ((p-abspath)>=2))
				{
					p = p-2;
				}

				while(('/' != *p) && (p > abspath))
				{
					p--;
				}

				s = s+2;
			}
			else if('.' == *s)
			{
				if( ('/' == *(p-1)) && ( ('/'==*(s+1)) || ('\0'==*(s+1)) ) )
				{
					p = p-1;
				}
				else
				{
					*p = *s;
					p++;
				}
				s++;
			}
			else if(('/' == *s) && ('/' == *(p-1)))
			{
				/* skip extra '/' */
				s++;
			}
			else
			{
				*p = *s;
				p++;
				s++;
			}
		}

		if(p == abspath)
		{
			*p = '/';
			p++;
		}

		*p = '\0';
	}

	ASLOG(VFS, "relpath(%s) = %s\n", path, abspath);
	return abspath;
}
#ifdef USE_SHELL
static int lsFunc(int argc, char* argv[])
{
	int r = 0;
	const char* path;
	VFS_DIR* dir;
	vfs_dirent_t * dirent;
	vfs_stat_t st;

	if(1 == argc)
	{
		path = vfs_cwd;
	}
	else
	{
		path = argv[1];
	}

	dir = vfs_opendir(path);

	if(NULL != dir)
	{
		dirent = vfs_readdir(dir);
		while(NULL != dirent)
		{
			r = vfs_stat(dirent->d_name, &st);
			if(0 == r)
			{
				SHELL_printf("%srw-rw-rw- 1 as vfs %11ld %s\r\n", VFS_ISDIR(st.st_mode)?"d":"-", st.st_size, dirent->d_name);
				dirent = vfs_readdir(dir);
			}
			else
			{
				dirent = NULL; /* stat error, stop listing */
			}
		}
		vfs_closedir(dir);
	}
	else
	{
		r = -1;
	}

	return r;
}

static int chdirFunc(int argc, char* argv[])
{
	int r;

	if(2 == argc)
	{
		r = vfs_chdir(argv[1]);
	}
	else
	{
		r = vfs_chdir("/");
	}

	return r;
}

static int pwdFunc(int argc, char* argv[])
{
	SHELL_printf("\n%s\n",vfs_cwd);
	return 0;
}

static int mkdirFunc(int argc, char* argv[])
{
	return vfs_mkdir(argv[1],0);
}

static int rmFunc(int argc, char* argv[])
{
	int r;
	vfs_stat_t st;

	r = vfs_stat(argv[1], &st);

	if(0 == r)
	{
		if(VFS_ISDIR(st.st_mode))
		{
			r = vfs_rmdir(argv[1]);
		}
		else
		{
			r = vfs_unlink(argv[1]);
		}
	}

	return r;
}

static int cpFunc(int argc, char* argv[])
{
	int r = -1;
	int len;
	VFS_FILE* fps;
	VFS_FILE* fpt;
	char buf[512];

	fps = vfs_fopen(argv[1],"rb");
	if(NULL == fps)
	{
		SHELL_printf("open %s failed!\n",argv[1]);
	}
	else
	{
		fpt = vfs_fopen(argv[2],"wb");
		if(NULL == fpt)
		{
			SHELL_printf("create %s failed!\n",argv[2]);
			vfs_fclose(fps);
		}
		else
		{
			do {
				len = vfs_fread(buf, 1, sizeof(buf), fps);
				if(len > 0)
				{
					r = vfs_fwrite(buf, 1, len, fpt);
					if(len != r)
					{
						SHELL_printf("write to %s failed!\n",argv[2]);
						r = -2;
						break;
					}
				}
			} while(len > 0);

			vfs_fclose(fps);
			vfs_fclose(fpt);
		}
	}

	return r;
}

static int catFunc(int argc, char* argv[])
{
	VFS_FILE* f;
	char buf[33];
	int r = 0;

	f = vfs_fopen(argv[1], "r");

	if(NULL != f)
	{
		do {
			r = vfs_fread(buf, 1, sizeof(buf)-1, f);

			if(r > 0)
			{
				buf[r] = '\0';
				SHELL_printf("%s",buf);
			}
		} while(r > 0);
	}
	else
	{
		r = -1;
	}
	return r;
}
static int hexdumpFunc(int argc, char* argv[])
{
	VFS_FILE* f;
	unsigned char buf[16];
	int r = 0;
	int i;
	unsigned long size = -1;
	unsigned long offset = 0;
	char* file = NULL;

	for(i=1; i<argc; i++)
	{
		if(0 == strcmp(argv[i],"-s"))
		{
			if(0 == strncmp(argv[i+1],"0x",2))
			{
				offset = strtoul(argv[i+1]+2, NULL, 16);
			}
			else
			{
				offset = strtoul(argv[i+1], NULL, 10);
			}
			i++;
		}
		else if(0 == strcmp(argv[i],"-n"))
		{
			size = strtoul(argv[i+1], NULL, 10);
			i++;
		}
		else
		{
			file = argv[i];
		}
	}

	if(NULL == file)
	{
		r = -EINVAL;
	}
	else
	{
		f = vfs_fopen(argv[1], "rb");

		if(NULL != f)
		{
			r = vfs_fseek(f, (long int)offset, SEEK_SET);

			if(0 == r)
			{
				printf("         :: 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
				do {
					r = vfs_fread(buf, 1, sizeof(buf), f);

					if(r > 0)
					{
						printf("%08X ::",(uint32_t)offset);
						for(i=0;i<16;i++)
						{
							if(i < r) {
								printf(" %02X",buf[i]);
							} else {
								printf("   ");
							}
						}
						printf("\t");
						for(i=0;i<16;i++)
						{
							if(i >= r) break;
							if(isprint(buf[i]))
							{
								printf("%c",buf[i]);
							}
							else
							{
								printf(".");
							}
						}
						printf("\n");
					}
					offset += sizeof(buf);
					size   -= size>sizeof(buf)?sizeof(buf):size;
				} while((r > 0) && (size>0));
			}
		}
		else
		{
			r = -1;
		}
	}

	return r;
}
#endif
/* ============================ [ FUNCTIONS ] ====================================================== */
VFS_FILE* vfs_fopen (const char *filename, const char *opentype)
{
	char* abspath;
	const vfs_mount_t *mnt;
	VFS_FILE* file = NULL;

	ASLOG(VFS, "fopen(%s,%s)\n", filename, opentype);

	abspath = relpath(filename);

	if(NULL != abspath)
	{
		mnt = search_mnt(abspath);
		if(NULL != mnt)
		{
			file = mnt->ops->fopen(mnt, abspath, opentype);
		}
		free(abspath);
	}

	return file;
}
ELF_EXPORT_ALIAS(vfs_fopen,"fopen");

int vfs_fclose (VFS_FILE* stream)
{
	return stream->fops->fclose(stream);
}
ELF_EXPORT_ALIAS(vfs_fclose,"fclose");

int vfs_fread (void *data, size_t size, size_t count, VFS_FILE *stream)
{
	return stream->fops->fread(data, size, count, stream);
}
ELF_EXPORT_ALIAS(vfs_fread,"fread");

int vfs_fwrite (const void *data, size_t size, size_t count, VFS_FILE *stream)
{
	return stream->fops->fwrite(data, size, count, stream);
}
ELF_EXPORT_ALIAS(vfs_fwrite,"fwrite");

int vfs_fflush (VFS_FILE *stream)
{
	return stream->fops->fflush(stream);
}
ELF_EXPORT_ALIAS(vfs_fflush,"fflush");

int vfs_fseek (VFS_FILE *stream, long int offset, int whence)
{
	return stream->fops->fseek(stream, offset, whence);
}
ELF_EXPORT_ALIAS(vfs_fseek,"fseek");

size_t vfs_ftell (VFS_FILE *stream)
{
	return stream->fops->ftell(stream);
}
ELF_EXPORT_ALIAS(vfs_ftell,"ftell");

int vfs_unlink (const char *filename)
{
	char* abspath;
	int rc = EACCES;
	const vfs_mount_t *mnt;

	ASLOG(VFS, "unlink(%s)\n", filename);

	abspath = relpath(filename);

	if(NULL != abspath)
	{
		mnt = search_mnt(abspath);
		if(NULL != mnt)
		{
			rc = mnt->ops->unlink(mnt, abspath);
		}
		free(abspath);
	}

	return rc;
}
ELF_EXPORT_ALIAS(vfs_unlink,"unlink");

int vfs_stat (const char *filename, vfs_stat_t *buf)
{
	char* abspath;
	int rc = EACCES;
	const vfs_mount_t *mnt;

	ASLOG(VFS, "stat(%s)\n", filename);

	abspath = relpath(filename);

	if(NULL != abspath)
	{
		mnt = search_mnt(abspath);
		if(NULL != mnt)
		{
			rc = mnt->ops->stat(mnt, abspath, buf);
		}
		free(abspath);
	}

	return rc;
}
ELF_EXPORT_ALIAS(vfs_stat,"stat");

VFS_DIR * vfs_opendir (const char *dirname)
{
	char* abspath;
	const vfs_mount_t *mnt;
	VFS_DIR* dir = NULL;

	ASLOG(VFS, "opendir(%s)\n", dirname);

	abspath = relpath(dirname);

	if(NULL != abspath)
	{
		mnt = search_mnt(abspath);
		if(NULL != mnt)
		{
			dir = mnt->ops->opendir(mnt, abspath);
		}
		free(abspath);
	}

	return dir;
}
ELF_EXPORT_ALIAS(vfs_opendir,"opendir");

vfs_dirent_t * vfs_readdir (VFS_DIR *dirstream)
{
	return dirstream->fops->readdir(dirstream);
}
ELF_EXPORT_ALIAS(vfs_readdir,"readdir");

int vfs_closedir (VFS_DIR *dirstream)
{
	return dirstream->fops->closedir(dirstream);
}
ELF_EXPORT_ALIAS(vfs_closedir,"closedir");

int vfs_chdir (const char *filename)
{
	char* abspath;
	int rc = EACCES;
	const vfs_mount_t *mnt;

	ASLOG(VFS, "chdir(%s)\n", filename);

	abspath = relpath(filename);

	if(NULL != abspath)
	{
		mnt = search_mnt(abspath);
		if(NULL != mnt)
		{
			rc = mnt->ops->chdir(mnt, abspath);
			if(0 == rc)
			{
				strncpy(vfs_cwd, abspath, FILENAME_MAX);
			}
		}
		free(abspath);
	}

	return rc;
}
ELF_EXPORT_ALIAS(vfs_chdir,"chdir");

char * vfs_getcwd  (char *buffer, size_t size)
{
	size_t rsize = strlen(vfs_cwd);

	ASLOG(VFS, "getcwd(%s)\n", vfs_cwd);

	if(NULL == buffer)
	{
		size = rsize+1;
		buffer = malloc(size);
	}

	if(size < rsize)
	{
		buffer = NULL;
	}

	if(NULL != buffer)
	{
		strncpy(buffer,vfs_cwd,size);
	}

	return buffer;
}
ELF_EXPORT_ALIAS(vfs_getcwd,"getcwd");

int vfs_mkdir (const char *filename, uint32_t mode)
{
	char* abspath;
	int rc = EACCES;
	const vfs_mount_t *mnt;

	ASLOG(VFS, "mkdir(%s, 0x%x)\n", filename, mode);

	abspath = relpath(filename);

	if(NULL != abspath)
	{
		mnt = search_mnt(abspath);
		if(NULL != mnt)
		{
			rc = mnt->ops->mkdir(mnt, abspath, mode);
		}
		free(abspath);
	}

	return rc;
}
ELF_EXPORT_ALIAS(vfs_mkdir,"mkdir");

int  vfs_rmdir (const char *filename)
{
	char* abspath;
	int rc = EACCES;
	const vfs_mount_t *mnt;

	ASLOG(VFS, "rmdir(%s)\n", filename);

	abspath = relpath(filename);

	if(NULL != abspath)
	{
		mnt = search_mnt(abspath);
		if(NULL != mnt)
		{
			rc = mnt->ops->rmdir(mnt, abspath);
		}
		free(abspath);
	}

	return rc;
}
ELF_EXPORT_ALIAS(vfs_rmdir,"rmdir");

int vfs_rename (const char *oldname, const char *newname)
{
	char* abspath_old;
	char* abspath_new;
	int rc = EACCES;
	const vfs_mount_t *mnt;

	ASLOG(VFS, "rename(%s,%s)\n", oldname, newname);

	abspath_old = relpath(oldname);
	abspath_new = relpath(newname);

	if(NULL != abspath_old)
	{
		if(NULL != abspath_new)
		{
			mnt = search_mnt(abspath_old);
			if(NULL != mnt)
			{
				rc = mnt->ops->rename(mnt, abspath_old, abspath_new);
			}
			free(abspath_new);
		}
		free(abspath_old);
	}

	return rc;
}
ELF_EXPORT_ALIAS(vfs_rename,"rename");

char* vfs_find(const char* file)
{
	char* r = NULL;
	char* p;
	char* n;

	p = malloc(FILENAME_MAX);
	if(p != NULL)
	{
		strcpy(p, "/");
		r = serach(p, file);
		if(NULL == r)
		{
			free(p);
		}
	}

	return r;
}

int vfs_fprintf (VFS_FILE* fp, const char* format, ...)
{
	int n;
	char* buf;

	va_list arg_ptr;

	buf = malloc(VFS_FPRINTF_BUFFER_SIZE);
	if(buf != NULL)
	{
		va_start(arg_ptr, format);
		n = vsnprintf(buf, VFS_FPRINTF_BUFFER_SIZE, format, arg_ptr);
		va_end(arg_ptr);
		if(n >= VFS_FPRINTF_BUFFER_SIZE)
		{
			ASLOG(ERROR, "VFS_FPRINTF_BUFFER_SIZE=%d is too small, enlarge it please\n",
					VFS_FPRINTF_BUFFER_SIZE);
		}

		n = vfs_fwrite(buf, 1, n, fp);

		free(buf);
	}
	else
	{
		n = -ENOMEM;
	}

	return n;
}
ELF_EXPORT_ALIAS(vfs_fprintf,"fprintf");

void vfs_init(void)
{
#ifdef USE_SHELL
#if !defined(USE_SHELL_SYMTAB)
	SHELL_AddCmd(&lsVfsCmd);
	SHELL_AddCmd(&chdirVfsCmd);
	SHELL_AddCmd(&pwdVfsCmd);
	SHELL_AddCmd(&mkdirVfsCmd);
	SHELL_AddCmd(&rmVfsCmd);
	SHELL_AddCmd(&cpVfsCmd);
	SHELL_AddCmd(&catVfsCmd);
	SHELL_AddCmd(&hexdumpVfsCmd);
#endif
#endif

	TAILQ_INIT(&vfs_mount_list);
}

int vfs_mount (const device_t* device, const char* type, const char* mount_point)
{
	int ercd = 0;
	vfs_mount_t* m;
	VFS_DIR* dir;
	const struct vfs_filesystem_ops* ops;

	VFS_LOCK();
	TAILQ_FOREACH(m, &vfs_mount_list, entry)
	{
		if(0 == strcmp(m->mount_point, mount_point))
		{
			ercd = EEXIST;
			break;
		}
	}
	VFS_UNLOCK();

	if(0 == ercd)
	{
		if(0 != strcmp(mount_point, "/"))
		{
			dir = vfs_opendir(mount_point);
			if(dir != NULL)
			{
				vfs_closedir(dir);
			}
			else
			{
				ercd = ENOENT;
			}
		}
	}

	if(0 == ercd)
	{
		m = malloc(strlen(mount_point)+1+sizeof(vfs_mount_t));
		if(m != NULL)
		{
			ops = search_ops(type);
			if(NULL == ops)
			{
				ercd = EINVAL;
			}
			else
			{
				ercd = ops->mount(device, mount_point);
				if(0 == ercd)
				{
					m->mount_point = (const char*)((unsigned long)m+sizeof(vfs_mount_t));
					strcpy((char*)m->mount_point, mount_point);
					m->device = device;
					m->ops = ops;

					VFS_LOCK();
					TAILQ_INSERT_TAIL(&vfs_mount_list, m, entry);
					VFS_UNLOCK();
				}
				else
				{
					free(m);
				}
			}
		}
		else
		{
			ercd = ENOMEM;
		}
	}

	ASLOG(VFS, "mount device %s on %s %s\n", device->name, mount_point,
			(0==ercd)?"okay":"failed");

	return ercd;
}

int vfs_mkfs (const device_t* device, const char* type)
{
	int ercd = 0;
	const struct vfs_filesystem_ops* ops;

	ops = search_ops(type);
	if(NULL == ops)
	{
		ercd = EINVAL;
	}
	else
	{
		ercd = ops->mkfs(device);
	}

	return ercd;
}

#if !defined(__WINDOWS__) && !defined(__LINUX__) && defined (__GNUC__)
FILE* fopen (const char *filename, const char *opentype) __attribute__((weak, alias("vfs_fopen")));
int fclose (FILE* stream) __attribute__((weak, alias("vfs_fclose")));
size_t fread (void *data, size_t size, size_t count, FILE *stream) __attribute__((weak, alias("vfs_fread")));
size_t fwrite (const void *data, size_t size, size_t count, FILE *stream) __attribute__((weak, alias("vfs_fwrite")));
int fflush (FILE *stream) __attribute__((weak, alias("vfs_fflush")));
int fseek (FILE *stream, long int offset, int whence) __attribute__((weak, alias("vfs_fseek")));
long ftell (FILE *stream) __attribute__((weak, alias("vfs_ftell")));
#endif
