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


#ifndef _VISIBILITY_H
#define _VISIBILITY_H

#ifdef HAVE_CONFIG_H
#include "acconf.h"
#endif

#ifdef _WIN32
#define export __declspec(dllexport)
#define internal
#define hidden
#elif HAVE_GCC_VISIBILITY
#define export __attribute__((visibility ("default")))
#define internal __attribute__((visibility ("internal")))
#define hidden __attribute__((visibility ("hidden")))
#else
#define export
#define internal
#define hidden
#endif

#endif /* _VISIBILITY_H */
