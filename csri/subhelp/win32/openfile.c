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

#include <windows.h>
#include <string.h>

#include "subhelp.h"
#include <csri/openerr.h>

csri_inst *subhelp_open_file(csri_rend *renderer,
	csri_inst *(*memopenfunc)(csri_rend *renderer, const void *data,
		size_t length, struct csri_openflag *flags),
	const char *filename, struct csri_openflag *flags)
{
	csri_inst *rv = NULL;
	void *data;
	struct csri_openflag *err;
	HANDLE file, mapping;
	DWORD size;
	int namesize;
	wchar_t *namebuf;

	namesize = MultiByteToWideChar(CP_UTF8, 0, filename, -1, NULL, 0);
	if (!namesize)
		goto out_err;
	namesize++;
	namebuf = malloc(sizeof(wchar_t) * namesize);
	MultiByteToWideChar(CP_UTF8, 0, filename, -1, namebuf, namesize);

	file = CreateFileW(namebuf, GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, 0, NULL);
	free(namebuf);
	if (file == INVALID_HANDLE_VALUE)
		goto out_err;
	size = GetFileSize(file, NULL);
	if (size == INVALID_FILE_SIZE || !size)
		goto out_closefile;
	mapping = CreateFileMappingW(file, NULL, PAGE_READONLY, 0, 0, NULL);
	if (!mapping)
		goto out_closefile;
	data = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, size);
	if (!data)
		goto out_closemap;

	rv = memopenfunc(renderer, data, size, flags);

	UnmapViewOfFile(data);
	CloseHandle(mapping);
	CloseHandle(file);
	return rv;

out_closemap:
	CloseHandle(mapping);
out_closefile:
	CloseHandle(file);
out_err:
	for (err = flags; err; err = err->next)
		if (!strcmp(err->name, CSRI_EXT_OPENERR))
			break;
	if (err) {
		struct csri_openerr_flag *errd = err->data.otherval;
		errd->flags = CSRI_OPENERR_FILLED | CSRI_OPENERR_WINERR;
		errd->winerr = GetLastError();
	}
	return NULL;
}

