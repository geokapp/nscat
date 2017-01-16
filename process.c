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
#define _XOPEN_SOURCE 500
#define _GNU_SOURCE
#include <errno.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "common.h"
#include "info.h"
#include "namespace.h"
#include "process.h"

/**
 * @name create_emtpy_process - Create an empty process object.
 * @return Pointer to a process object or NULL.
 */
process_t *create_empty_process() {
  unsigned short type; 
  process_t *p;
  
  if (!(p = malloc(sizeof(process_t)))) {
    report_error(NULL, debug_message(RET_ERR_NOMEM), ERROR_MSG);
    return NULL;
  }

  p->pid = p->ppid = 0;
  p->uid = 0;
  p->gid = 0;
  p->name = NULL;
  p->parent = NULL;  
  
  if (!(p->namespace = malloc(NSCOUNT * sizeof(namespace_t)))) {
    report_error(NULL, debug_message(RET_ERR_NOMEM), ERROR_MSG);
    safe_free((void **)&p);
    return NULL;
  }

  for (type = 0; type < NSCOUNT; type++) 
    p->namespace[type] = NULL;
  return p;
}

/**
 * @name delete_process - Delete a process object.
 * @param p: The address of the process object to delete.
 * @return Void.
 */
void delete_process(process_t **p) {
  unsigned short type;
  
  if (!p || !(*p)) {
    report_error("delete_process", debug_message(RET_ERR_PARAM), DEBUG_MSG);  
    return;
  }
  
  (*p)->parent = NULL;
  for (type = 0; type < NSCOUNT; type++) 
    (*p)->namespace[type] = NULL;
  safe_free((void **)&((*p)->name));
  safe_free((void **)&(*p)->namespace);    
  safe_free((void **)p);
}

/**
 * @name clear_process_list - Clear a process list.
 * @param l: The address of the process list object to clear.
 * @return Void.
 */
void clear_process_list(list_t **l) {
  list_t *c, *d;

  if (!l || !(*l)) {
    report_error("clear_process_list", debug_message(RET_ERR_PARAM), DEBUG_MSG);
    return;
  }

  c = *l;
  while (c) {
    d = c;
    c = c->next;
    delete_process(&(d->process));
    safe_free((void **)&d);
  }
}

/**
 * @name insert_process_list - Insert a new process in a process list.
 * @param l: The address of the process list object.
 * @param p: The process object to insert.
 * @return RET_OK on success, or an error code on error.
 */
int insert_process_list(list_t **l, process_t *p) {
  list_t *a, *c;

  if (!l || !p) {
    report_error("insert_process_list", debug_message(RET_ERR_PARAM), DEBUG_MSG);  
    return RET_ERR_PARAM;
  }
  
  if (!(a = malloc(sizeof(list_t)))) {
    report_error(NULL, debug_message(RET_ERR_NOMEM), ERROR_MSG);  
    return RET_ERR_NOMEM;
  }  
  a->process = p;
  a->next = NULL;
  if (*l) {
    for (c = *l; c; c = c->next) 
      if (c->next == NULL) {
	c->next = a;
	return RET_OK;
      }
  } else {
    *l = a;
  }
  return RET_OK;
}

/**
 * @name count_process_list - Count the processes in the process list.
 * @param l: The process list object.
 * @return Number of processes in the given list.
 */
unsigned long count_process_list(list_t *l) {
  list_t *c;
  unsigned long count = 0;

  if (!l) {
    report_error("count_process_list", debug_message(RET_ERR_PARAM), DEBUG_MSG);  
    return 0;
  }
  
  for (c = l; c; c = c->next)
    count++;
  return count;
}

/**
 * @name search_process_list - Search for a process in the process list.
 * @param l: The process list object.
 * @return A process object or NULL.
 */
process_t *search_process_list(list_t *l, const pid_t pid) {
  list_t *c;

  if (!l) {
    report_error("search_process_list", debug_message(RET_ERR_PARAM), DEBUG_MSG);  
    return NULL;
  }

  for (c = l; c; c = c->next) 
    if (c->process) 
      if (c->process->pid == pid)
	return c->process;
  return NULL;
}

/**
 * @name sort_process_list - Sort the process list by pid.
 * @param l: The address of the process list object.
 * @return Void.
 */
void sort_process_list(list_t **l) {
  list_t *a, *c, *p, *h;

  if (!l || !(*l)) {
    report_error("sort_process_list", debug_message(RET_ERR_PARAM), DEBUG_MSG);  
    return;
  }

  if (!(*l)->next)
    // The list contains only one element.
    return;

  a = *l;
  h = NULL;
  while (a) {
    c = a;
    a = a->next;
    if (!h || c->process->pid < h->process->pid) {
      c->next = h;
      h = c;
    } else {
      p = h;
      while (p) {
	if (!(p->next) ||
	    c->process->pid < p->next->process->pid) {
	  c->next = p->next;
	  p->next = c;
	  break;
	}
	p = p->next;
      }
    }
  }
  *l = h;
}

/**
 * @name get_proc_ppid - Get the parent PID of a process.
 * @param proc_path: The process path in procfs.
 * @param ppid: Pointer to a pid_t where the result will be placed.
 * @return RET_OK on sucess, an error code on error.
 */
int get_proc_ppid(const char *proc_path, pid_t *ppid) {
  char buffer[BUFFER_SIZE];
  FILE *fd;
  char *target_path;
  unsigned int size;
  
  if (!proc_path || !ppid) {
    report_error("get_proc_ppid", debug_message(RET_ERR_PARAM), DEBUG_MSG);  
    return RET_ERR_PARAM;
  }  
  size = strlen(proc_path) + strlen("/status") + 1;
  if (!(target_path = malloc(size+1))) {
    report_error(NULL, debug_message(RET_ERR_NOMEM), ERROR_MSG);    
    return RET_ERR_NOMEM;
  }
  memset(target_path, 0, size+1);  
  snprintf(target_path, size, "%s%s\0", proc_path, "/status");
  if (!(fd = fopen(target_path, "r"))) {
    report_error(target_path, strerror(errno), DEBUG_MSG);
    safe_free((void **)&target_path); 
    return RET_ERR_NOFILE;
  }
  safe_free((void **)&target_path); 
  while (fgets(buffer, sizeof(buffer), fd)) {
    if (strstr(buffer, "PPid")) {
      fclose(fd);
      *ppid = atoi(buffer + strlen("PPid:"));
      return RET_OK;      
    }
  }
  fclose(fd);
  report_error("get_proc_ppid", debug_message(RET_ERR_NOENTRY), DEBUG_MSG);
  return RET_ERR_NOENTRY;
}

/**
 * @name get_proc_name - Get the name of a process.
 * @param proc_path: The process path in procfs.
 * @param pname: The address of a memory location where the result will be placed.
 * @return RET_OK on sucess, an error code on error.
 */
int get_proc_name(const char *proc_path, char **pname) {
  unsigned int size;
  FILE *fd;
  char *target_path;

  if (!proc_path || !pname) {
    report_error("get_proc_name", debug_message(RET_ERR_PARAM), DEBUG_MSG);  
    return RET_ERR_PARAM;
  }
  size = strlen(proc_path) + strlen(PROCNAMEFILE) + 1;  
  if (!(target_path = malloc(size+1))) {
    report_error(NULL, debug_message(RET_ERR_NOMEM), ERROR_MSG);    
    return RET_ERR_NOMEM;
  }
  memset(target_path, 0, size+1);
  snprintf(target_path, size, "%s%s\0", proc_path, PROCNAMEFILE);
  if (!(fd = fopen(target_path, "r"))) {    
    report_error(target_path, strerror(errno), DEBUG_MSG);
    safe_free((void **)&target_path);
    return RET_ERR_NOFILE;
  }
  safe_free((void **)&target_path);
  if (!(*pname = malloc(BUFFER_SIZE))) {    
    report_error(NULL, debug_message(RET_ERR_NOMEM), ERROR_MSG);    
    return RET_ERR_NOMEM;
  }
  memset(*pname, 0, BUFFER_SIZE);
  fgets(*pname, BUFFER_SIZE, fd);
  fclose(fd);
  delete_spaces(pname);    
  return RET_OK;
}

/**
 * @name get_proc_uid - Get the UID of a process.
 * @param proc_path: The process path in procfs.
 * @param uid: Pointer to uid_t where the result will be placed.
 * @return RET_OK on sucess, an error code on error.
 */
int get_proc_uid(const char *proc_path, uid_t *uid) {
  char buffer[BUFFER_SIZE];
  struct stat sb;
  
  if (!proc_path || !uid) {
    report_error("get_proc_uid", debug_message(RET_ERR_PARAM), DEBUG_MSG);
    return RET_ERR_PARAM;
  }
  if (stat(proc_path, &sb)) {
    report_error("get_proc_uid", strerror(errno), DEBUG_MSG);
    return RET_ERR_NOLINK;
  }
  *uid =  sb.st_uid;
  return RET_OK;
}

/**
 * @name get_proc_gid - Get the GID of a process.
 * @param proc_path: The process path in procfs.
 * @param gid: Pointer to uid_t where the result will be placed.
 * @return RET_OK on sucess, an error code on error.
 */
int get_proc_gid(const char *proc_path, gid_t *gid) {
  char buffer[BUFFER_SIZE];
  struct stat sb;
  
  if (!proc_path || !gid) {
    report_error("get_proc_gid", debug_message(RET_ERR_PARAM), DEBUG_MSG);
    return RET_ERR_PARAM;
  }
  if (stat(proc_path, &sb)) {
    report_error("get_proc_gid", strerror(errno), DEBUG_MSG);
    return RET_ERR_NOLINK;
  }
  *gid =  sb.st_gid;
  return RET_OK;
}

/**
 * @name handle_proc_entry - Process nftw entry.
 * @param fpath: The pathname of an entry found by nftw.
 * @param sb: Pointer to a struct structure for the entry.
 * @param tflag: flags.
 * @param ftwbuf: pointer to struct FTW.
 * @return FTW code that instructs nftw to continue, to stop,
 *         or to skip a particular subtree entirely.
 *
 * This method is called for each entry found by nftw. its purpose
 * is to create a new process object for the entry and add in the
 * process list.
 */
int handle_proc_entry(const char *fpath, const struct stat *sb,
		      int tflag, struct FTW *ftwbuf) {
  ino_t nid;
  unsigned short type;
  pid_t pid, ppid;
  process_t *p = NULL;

  if (!fpath || !sb || !ftwbuf) {
    report_error("handle_proc_entry", debug_message(RET_ERR_PARAM), DEBUG_MSG);
    return FTW_STOP;
  }
 
  // We are interested only for process directories.
  if (!(pid = atoi(fpath + ftwbuf->base)))
    if (ftwbuf->level >= 1 && tflag == FTW_D)
      return FTW_SKIP_SUBTREE;
    else
      return FTW_CONTINUE;  

  // Get the parent PID.
  if ((get_proc_ppid(fpath, &ppid)) != RET_OK) 
    return FTW_CONTINUE;

  // Store the process information that we have so far.
  if (!(p = create_empty_process()))
    return FTW_CONTINUE;
  p->pid = pid;
  p->ppid = ppid;
  p->name = NULL;
  if ((get_proc_name(fpath, &(p->name))) != RET_OK) {
    safe_free((void **)&p); 
    return FTW_CONTINUE;
  }
  if ((get_proc_uid(fpath, &(p->uid))) != RET_OK) {
    safe_free((void **)&p); 
    return FTW_CONTINUE;
  }
  if ((get_proc_gid(fpath, &(p->gid))) != RET_OK) {
    safe_free((void **)&p); 
    return FTW_CONTINUE;
  }
	  
  // Add this process to the process list.
  if ((insert_process_list(&(info->process), p)) != RET_OK) {
    safe_free((void **)&p);
    return FTW_CONTINUE;
  }

  // Skip subdirectories.
   if (ftwbuf->level >= 1 && tflag == FTW_D)
     return FTW_SKIP_SUBTREE;
   else
     return FTW_CONTINUE;  
}

/**
 * @name collect_processes - Find all process that have entries in procfs.
 * @return RET_OK on success, or an error code in case of an error.
 *
 * This method uses nftw to search the path where the procfs is mounted for
 * processes.
 */
int collect_processes() {
  if (!info || !(info->args)) {
    report_error("collect_processes", debug_message(RET_ERR_PARAM), DEBUG_MSG);
    return RET_ERR_PARAM;
  }

  // Process the /proc directory.
  if (nftw(info->args->proc_mnt, handle_proc_entry, 20, FTW_ACTIONRETVAL|FTW_PHYS) == -1) {
    report_error(NULL, strerror(errno), ERROR_MSG);
    return RET_ERR_NOFILE;
  }
  return RET_OK;
}
