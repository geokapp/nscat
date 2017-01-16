// -*- mode:C; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * nscat - Print namespace information.
 *
 * Copyright (C) 2016 Giorgos Kappes <geokapp@gmail.com>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file LICENSE.
 *
 */
#ifndef NSCAT_COMMON_H
#define NSCAT_COMMON_H

// Enable/disable debug messages.
#ifdef DEBUG
#undef DEBUG
#define DEBUG   1
#else
#define DEBUG   0
#endif


// Error message type
#define ERROR_MSG 0
#define DEBUG_MSG 1

// Buffer size
#define BUFFER_SIZE 1024

// Return Status
#define RET_OK           0
#define RET_ERR_PARAM   -1
#define RET_ERR_NOMEM   -2
#define RET_ERR_NOFILE  -3
#define RET_ERR_NOLINK  -4
#define RET_ERR_NOENTRY -5

const char *debug_message(const int error);
void report_error(const char *caller, const char *message,
		  const unsigned short type);
void delete_spaces(char **src);
void safe_free(void **p);
void warn_permissions();

#endif
