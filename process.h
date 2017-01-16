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

#ifndef NSCAT_PROCESS_H
#define NSCAT_PROCESS_H

#include <ftw.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "namespace.h"

static const char PROCNAMEFILE[] = "/comm";

// Process.
typedef struct process {
  pid_t pid;
  pid_t ppid;
  uid_t uid;
  gid_t gid;
  char *name;
  struct process *parent;
  struct namespace **namespace;
} process_t;

// Process list.
typedef struct list {
  struct process *process;
  struct list *next;
} list_t;

struct FTW;

process_t *create_empty_process();
void delete_process(process_t **p);
int get_proc_ppid(const char *proc_path, pid_t *ppid);
int get_proc_name(const char *proc_path, char **pname);
int get_proc_uid(const char *proc_path, uid_t *uid);
int get_proc_gid(const char *proc_path, gid_t *gid);
int collect_processes();
int handle_proc_entry(const char *fpath, const struct stat *sb,
		      int tflag, struct FTW *ftwbuf);
int insert_process_list(list_t **l, process_t *p);
unsigned long count_process_list(list_t *l);
process_t *search_process_list(list_t *l, const pid_t pid);
void sort_process_list(list_t **l);
void clear_process_list(list_t **l);

#endif
