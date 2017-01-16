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

#ifndef NSCAT_INFO_H
#define NSCAT_INFO_H

#include <sys/types.h>
#include <unistd.h>
#include "namespace.h"
#include "process.h"

// Flags
#define FLAG_PROCESS 0x00000001
#define FLAG_DESCS   0x00000010
#define FLAG_NSWANT  0x00000100
#define FLAG_EXTEND  0x00001000

// Constant messages.
static const char VERSION[] = "0.1";
static const char PROCMNT[] = "/proc/";

typedef struct callargs {
  ino_t ns;
  pid_t pid;
  unsigned int flags;
  unsigned short wanted[NSCOUNT];
  char *proc_mnt;
} callargs_t;

typedef struct info {
  struct list *process;
  struct tree *namespace[NSCOUNT];
  struct callargs *args;
} info_t;

extern info_t *info;

void clear_args(callargs_t **args);
void clear_info();
void print_info();
int build_info();

#endif
