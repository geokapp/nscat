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
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"

/**
 * @name debug_message - Convert an error code to string.
 * @param error: The error code.
 * @return An error message in string form.
 */
const char *debug_message(const int error) {
  switch (error) {
      case RET_ERR_PARAM:
	return "Function parameter not initialized";
      case RET_ERR_NOMEM:
	return "Cannot allocate memory";
      case RET_ERR_NOFILE:
	return "Cannot open file";
      case RET_ERR_NOLINK:
	return "Cannot read link";
      case RET_ERR_NOENTRY:
	return "An entry does not exist";
      default:
	return "Unknown error";
    }
}

/**
 * @name report_error - Report an error or debugging information.
 * @param caller: The caller function.
 * @param message: The error message.
 * @param type: The error type (error or debug).
 * @return Void.
 */
void report_error(const char *caller, const char *message,
		  const unsigned short type) {
  if (type == ERROR_MSG) {
    if (caller)
      fprintf(stderr, "[nscat] %s(): %s.\n", caller, message);
    else
      fprintf(stderr, "nscat: %s.\n", message);
  } else {
    if (DEBUG == 1)
      if (caller)
	fprintf(stderr, "[nscat] %s(): %s.\n", caller, message);
      else
	fprintf(stderr, "nscat: %s.\n", message);
  }  
}

/**
 * @name warn_permissions - Warn the user for insuficient permissions.
 * @return Void.
 */
void warn_permissions() {
  static unsigned short flag = 0;

  if (!flag) {
    fprintf(stderr, "[nscat] Warning: Some system resources cannot be accessed due to "
	    "insufficient permissions of the caller's account. The following results "
	    "may be incomplete. Please run this program as the root user to get the "
	    "complete results.\n\n");
    flag++;
  }
}

/**
 * @name delete_spaces - Delete trailing spaces from a string
 * @param src: The address of the string.
 * @return Void.
 */
void delete_spaces(char **src){
  int s, d = 0;

  if (!src || !(*src))
    return;
  
  for (s = 0; (*src)[s] != 0; s++)
    if (!isspace((*src)[s])) {
      (*src)[d] = (*src)[s];
      d++;
    }
  (*src)[d] = 0;
}

/**
 * @name safe_free - Free memory safely.
 * @param p: The address of the memory to free.
 * @return Void.
 */
void safe_free(void **p) {
  if (p && *p) {
    free(*p);
    *p = NULL;
  }
}

