/*
 * sysfs.c
 * Helpers to read information from Linux /sys tree
 */

#include <pathutil.h>
#include <mempool.h>
#include <tsdirent.h>
#include <plat/posixdecl.h>
#include <plat/sysfs.h>

#include <string.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>

int hi_linux_sysfs_readstr(const char* root, const char* name, const char* object,
						   char* str, int len) {
	char path[256];
	int fd;
	uint64_t i;
	ssize_t num;

	path_join(path, 256, root, name, object, NULL);

	fd = open(path, O_RDONLY);

	if(fd == -1) {
		return HI_LINUX_SYSFS_ERROR;
	}

	num = read(fd, str, len);
	*(str + num) = '\0';

	close(fd);

	hi_sysfs_dprintf("hi_linux_sysfs_readstr: %s/%s/%s\n", root, name, object);

	return HI_LINUX_SYSFS_OK;
}

/**
 * Read unsigned integer from sysfs */
uint64_t hi_linux_sysfs_readuint(const char* root, const char* name, const char* object,
								 uint64_t defval) {
	char str[64];
	uint64_t i;

	if(hi_linux_sysfs_readstr(root, name, object, str, 64) != HI_LINUX_SYSFS_OK)
		return defval;

	/* No need to fix string here: strtoull simply discard any extra-characters */
	i = strtoull(str, NULL, 10);

	if(i == ULLONG_MAX && errno == ERANGE)
		return defval;

	hi_sysfs_dprintf("hi_linux_sysfs_readuint: %s/%s/%s => %llx\n", root, name, object, i);

	return i;
}

static
int bitmap_add_interval(char* p, char* pm, char* ps, uint32_t* bitmap, int len) {
	int i, b, top, bot;

	if(pm == NULL) {
		top = bot = strtol(ps, &p, 10);
	}
	else {
		bot = strtol(ps, &pm, 10);
		++pm;
		top = strtol(pm, &p, 10);
	}

	for(; bot <= top; ++bot) {
		i = bot / 32;
		if(i > len)
			return HI_LINUX_SYSFS_ERROR;

		/* Set bit */
		b = bot % 32;
		bitmap[i] |= 1 << b;

		hi_sysfs_dprintf("hi_linux_sysfs_parsebitmap: bit[%d] is set\n", bot);
	}

	return HI_LINUX_SYSFS_OK;
}

int hi_linux_sysfs_parsebitmap(const char* str, uint32_t* bitmap, int len) {
	int ret;
	char *p = str;
	char *pm = NULL;
	char *ps = p;

	/* Parse bitmap. May be implemented on top of strchr/sscanf,
	 * but dumb implementation is simpler
	 *
	 * 0  -  7  ,  9
	 * ^ps^pm   ^p*/
	do {
		if(*p == '-')
			pm = p;

		if(*p == ',') {
			/* Found delimiter */
			ret = bitmap_add_interval(p, pm, ps, bitmap, len);
			if(ret != HI_LINUX_SYSFS_OK)
				return ret;

			pm = NULL;
			ps = p + 1;
		}

		++p;
	} while(*p);

	bitmap_add_interval(p, pm, ps, bitmap, len);

	return HI_LINUX_SYSFS_OK;
}

int hi_linux_sysfs_readbitmap(const char* root, const char* name, const char* object,
						   uint32_t* bitmap, int len) {
	char str[128];
	int i;

	if(hi_linux_sysfs_readstr(root, name, object, str, 128) != HI_LINUX_SYSFS_OK)
		return HI_LINUX_SYSFS_ERROR;

	/* Erase bitmap */
	for(i = 0; i < len; ++i)
		bitmap[i] = 0;

	hi_sysfs_dprintf("hi_linux_sysfs_readbitmap: %s/%s/%s\n", root, name, object);

	return hi_linux_sysfs_parsebitmap(str, bitmap, len);
}

/**
 * Fix sysfs string: replace \n with spaces */
void hi_linux_sysfs_fixstr(char* p) {
	while(*p) {
		if(*p == '\n')
			*p = ' ';
		++p;
	}
}

int hi_linux_sysfs_walk(const char* root,
					   void (*proc)(const char* name, void* arg), void* arg) {
	plat_dir_t* dirp;
	plat_dir_entry_t* de;

	if((dirp = plat_opendir(root)) == NULL)
		return HI_LINUX_SYSFS_ERROR;

	hi_sysfs_dprintf("hi_linux_sysfs_walk: %s\n", root);

	do {
		de = plat_readdir(dirp);

		if(de != NULL) {
			if(plat_dirent_hidden(de))
				continue;

			proc(de->d_name, arg);
		}
	} while(de != NULL);

	plat_closedir(dirp);
	return HI_LINUX_SYSFS_OK;
}

int hi_linux_sysfs_walkbitmap(const char* root, const char* name, const char* object, int count,
					   	      void (*proc)(int id, void* arg), void* arg) {
	int len = count / 32;
	uint32_t* bitmap = mp_malloc(len * sizeof(uint32_t));
	int i = 0;
	int ret = 0;

	ret = hi_linux_sysfs_readbitmap(root, name, object, bitmap, len);

	if(ret != HI_LINUX_SYSFS_OK) {
		mp_free(bitmap);
		return HI_LINUX_SYSFS_ERROR;
	}

	hi_sysfs_dprintf("hi_linux_sysfs_walkbitmap: %s/%s/%s => [%x, ...]\n", root, name, object, bitmap[0]);

	for(i = 0; i < count; ++i) {
		if(bitmap[i / 32] & (1 << (i % 32)))
			proc(i, arg);
	}

	mp_free(bitmap);
	return HI_LINUX_SYSFS_OK;
}
