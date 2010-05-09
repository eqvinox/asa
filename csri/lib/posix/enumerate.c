/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2007  David Lamparter
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the disclaimer
 *    contained in the COPYING file.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the disclaimer
 *    contained in the COPYING file in the documentation and/or
 *    other materials provided with the distribution.
 *
 ****************************************************************************/

#define _POSIX_C_SOURCE 200112L		/* for PATH_MAX */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include <dlfcn.h>

#include "../csrilib.h"
#include "subhelp.h"

static const char csri_path[] = CSRI_PATH;

static void csrilib_enum_dir(const char *dir);

static void csrilib_add(csri_rend *rend,
	const struct csri_wrap_rend *tmp, struct csri_info *info)
{
	struct csri_wrap_rend *wrend = (struct csri_wrap_rend *)
		malloc(sizeof(struct csri_wrap_rend));
	if (!wrend)
		return;
	memcpy(wrend, tmp, sizeof(struct csri_wrap_rend));
	wrend->rend = rend;
	wrend->info = info;
	csrilib_rend_initadd(wrend);
}

static void csrilib_do_load(const char *filename, dev_t device, ino_t inode)
{
	void *dlhandle = dlopen(filename, RTLD_NOW);
	struct csri_wrap_rend tmp;
	csri_rend *rend;
	struct csri_info *(*renderer_info)(csri_rend *rend);
	csri_rend *(*renderer_default)();
	csri_rend *(*renderer_next)(csri_rend *prev);
	const char *sym;


	if (!dlhandle) {
		subhelp_log(CSRI_LOG_WARNING, "dlopen(\"%s\") says: %s",
			filename, dlerror());
		return;
	}
	if (dlsym(dlhandle, "csri_library")) {
		subhelp_log(CSRI_LOG_WARNING, "ignoring library %s",
			filename);
		return;
	}
	subhelp_log(CSRI_LOG_INFO, "loading %s", filename);

	tmp.os.dlhandle = dlhandle;
	tmp.os.device = device;
	tmp.os.inode = inode;

/* strict-aliasing. yay. */
#define _dl_map_function(x, dst, type, args) do { \
	union x { void *ptr; type (*fptr) args ; } ptr; \
	sym = "csri_" # x; \
	ptr.ptr = dlsym(dlhandle, sym);\
	if (!ptr.ptr) goto out_dlfail; \
	dst = ptr.fptr; } while (0)
#define dl_map_function(x, type, args) _dl_map_function(x, tmp.x, type, args)
	dl_map_function(query_ext, void *,
		(csri_rend *, csri_ext_id));
	subhelp_logging_pass((struct csri_logging_ext *)
		tmp.query_ext(NULL, CSRI_EXT_LOGGING));
	dl_map_function(open_file, csri_inst *,
		(csri_rend *, const char *, struct csri_openflag *));
	dl_map_function(open_mem, csri_inst *,
		(csri_rend *, const void *, size_t, struct csri_openflag *));
	dl_map_function(close, void,
		(csri_inst *));
	dl_map_function(request_fmt, int,
		(csri_inst *, const struct csri_fmt *));
	dl_map_function(render, void,
		(csri_inst *, struct csri_frame *, double));
#define dl_map_local(x, type, args) _dl_map_function(x, x, type, args)
	dl_map_local(renderer_info, struct csri_info *, (csri_rend *));
	dl_map_local(renderer_default, csri_rend *, ());
	dl_map_local(renderer_next, csri_rend *, (csri_rend *));

	rend = renderer_default();
	while (rend) {
		csrilib_add(rend, &tmp, renderer_info(rend));
		rend = renderer_next(rend);
	}
	return;

out_dlfail:
	subhelp_log(CSRI_LOG_WARNING, "%s: symbol %s not found (%s)",
		filename, sym, dlerror());
	dlclose(dlhandle);
}

static void csrilib_load(const char *filename)
{
	struct csri_wrap_rend *rend;
	struct stat st;
	if (stat(filename, &st))
		return;

	if (S_ISDIR(st.st_mode)) {
		csrilib_enum_dir(filename);
		return;
	}
	if (!S_ISREG(st.st_mode))
		return;
	if (access(filename, X_OK))
		return;

	for (rend = wraprends; rend; rend = rend->next)
		if (rend->os.device == st.st_dev
			&& rend->os.inode == st.st_ino)
			return;

	csrilib_do_load(filename, st.st_dev, st.st_ino);
}

static void csrilib_enum_dir(const char *dir)
{
	DIR *dirh;
	struct dirent *e;
	char buf[PATH_MAX];

	dirh = opendir(dir);
	if (!dirh) {
		subhelp_log(CSRI_LOG_WARNING, "ignoring directory \"%s\":"
			" %s (%d)", dir, strerror(errno), errno);
		return;
	}

	subhelp_log(CSRI_LOG_INFO, "scanning directory \"%s\"", dir);
	while ((e = readdir(dirh))) {
		if (e->d_name[0] == '.')
			continue;
		snprintf(buf, sizeof(buf), "%s/%s", dir, e->d_name);
		csrilib_load(buf);
	}
		
	closedir(dirh);
}

static void csrilib_expand_enum_dir(const char *dir)
{
	if (dir[0] == '~' && (dir[1] == '\0' || dir[1] == '/')) {
		char buf[PATH_MAX], *home = getenv("HOME");
		if (!home)
			home = "";
		snprintf(buf, sizeof(buf), "%s%s", home, dir + 1);
		csrilib_enum_dir(buf);
	} else
		csrilib_enum_dir(dir);
}

void csrilib_os_init()
{
	char buf[4096];
	char *envpath = getenv("CSRI_PATH");
	char *pos, *next = buf;

	if (envpath)
		snprintf(buf, sizeof(buf), "%s:%s", csri_path, envpath);
	else {
		strncpy(buf, csri_path, sizeof(buf));
		buf[sizeof(buf) - 1] = '\0';
	}
	do {
		pos = next;
		next = strchr(pos, ':');
		if (next)
			*next++ = '\0';
		csrilib_expand_enum_dir(pos);
	} while (next);
}

