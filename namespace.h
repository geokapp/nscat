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

#ifndef NSCAT_NAMESPACE_H
#define NSCAT_NAMESPACE_H

#include <sys/types.h>
#include <unistd.h>
#include "process.h"

// Namespaces. 
#define NSCOUNT 7
#define CGROUP  0
#define IPC     1
#define MNT     2
#define NET     3
#define PID     4
#define USER    5
#define UTS     6

// uid_map / gid_map limit.
#define MAP_LIMIT 5

// uid_map / gid_map files.
static const char PROCUIDMAPFILE[] = "/uid_map";
static const char PROCGIDMAPFILE[] = "/gid_map";

typedef  struct uid_map {
  uid_t uid_inside;
  uid_t uid_outside;
  uid_t length;
} uid_map_t;

typedef struct gid_map {
  gid_t gid_inside;
  gid_t gid_outside;
  gid_t length;
} gid_map_t;

typedef struct namespace {
  struct uid_map uid_map[MAP_LIMIT];
  struct gid_map gid_map[MAP_LIMIT];
  ino_t nid;
  ino_t pnid;
  pid_t creator_pid;
  unsigned short type;
  struct process *creator;
  struct list *members;
} namespace_t;

typedef struct tree {
  struct namespace *namespace;
  unsigned int depth;  
  struct tree *child, *sibling;
} tree_t;

namespace_t *create_empty_namespace();
void delete_namespace(namespace_t **ns);
unsigned short is_orphaned_namespace(const namespace_t *n);
const char *get_name_from_type(const unsigned short type);
char *get_namespace_file(const unsigned short type);
int get_proc_namespace(const char *proc_path, const unsigned short type, ino_t *ns);
int get_proc_uid_map(const char *proc_path, uid_map_t *uid_map);
int get_proc_gid_map(const char *proc_path, gid_map_t *gid_map);
unsigned long count_namespace_tree(tree_t *tree);
tree_t *search_namespace_tree(tree_t *tree, const ino_t nid);
int insert_namespace_tree(tree_t **tree, namespace_t *ns);
void print_namespace_info(const namespace_t *ns, unsigned int depth);
void print_namespace_tree(const namespace_t *ns, const unsigned int depth);
void print_parented_namespaces(const tree_t *tree);
void print_orphaned_namespaces(const tree_t *tree);
void clear_namespace_tree(tree_t **tree);

#endif  
