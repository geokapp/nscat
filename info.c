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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "info.h"
#include "namespace.h"
#include "process.h"

info_t *info;

/**
 * @name clear_args - Clear the arguments.
 * @param args: The address of the arguments object.
 * @return Void.
 */
void clear_args(callargs_t **args) {
  if (!args || !(*args))
    return;
  
  safe_free((void **)&((*args)->proc_mnt));
  safe_free((void **)args);
}

/**
 * @name clear_info - Clear the info object.
 * @return Void.
 */
void clear_info() {
  unsigned int i;
  
  if (info) {    
    // Clear namespaces.
    for (i = 0; i < NSCOUNT; i++) 
      if (info->namespace[i])
	clear_namespace_tree(&(info->namespace[i]));

    // Clear processes.
    clear_process_list(&(info->process));

    // Clear arguments.
    clear_args(&(info->args));

    // Delete info.
    safe_free((void *) &info);
  }
}

/**
 * @name print_info - Print the collected information.
 * @return Void.
 */
void print_info() {
  unsigned short type;
  process_t *p = NULL;
  tree_t *nt = NULL;

  if (!info || !(info->args))
    return;

  // Check if a particular namespace ID was requested.
  if (info->args->ns) {
    for (type = 0; type < NSCOUNT; type++) 
      if (info->namespace[type])
	if (nt = search_namespace_tree(info->namespace[type], info->args->ns))
	  break;
    if (!nt) {      
      report_error(NULL, "No such namespace", ERROR_MSG);
      return;
    }
    print_namespace_info(nt->namespace, 0);
    return;
  }

  // Check if a particular process PID was requested.
  if (info->args->pid) {
    if (!(p = search_process_list(info->process, info->args->pid))) {        
      report_error(NULL, "No such process", ERROR_MSG);
      return;
    }
    // Check if the user wants the process descendants.
    if (info->args->flags & FLAG_DESCS) {
      for (type = 0; type < NSCOUNT; type++) {
        if ((info->args->flags & FLAG_NSWANT) && !(info->args->wanted[type]))
          continue;

        if (!(info->namespace[type]) || !(p->namespace[type]))
          continue;
        
        if (!(nt = search_namespace_tree(info->namespace[type], p->namespace[type]->nid)))
          continue;
        
        printf("Namespace: %s\n", get_name_from_type(type));
        print_parented_namespaces(nt);
        print_orphaned_namespaces(nt);
        printf("\n");
      }
    } else {
      for (type = 0; type < NSCOUNT; type++) {
        if ((info->args->flags & FLAG_NSWANT) && !(info->args->wanted[type]))
          continue;

        if (!(p->namespace[type]))
          continue;
        
        printf("Namespace: %s\n", get_name_from_type(type));
        print_namespace_tree(p->namespace[type], 0);
        printf("\n");
      }
    }
    return;    
  }

  // The default case.
  for (type = 0; type < NSCOUNT; type++) {
    // Skip any namespaces that the user did not requested.
    if ((info->args->flags & FLAG_NSWANT) && !(info->args->wanted[type]))
      continue;
    
    printf("Namespace: %s\n", get_name_from_type(type));
    print_parented_namespaces(info->namespace[type]);
    print_orphaned_namespaces(info->namespace[type]);
    printf("\n");
  }
  return;  
}

/**
 * @name build_info - Collect namespace information.
 * @return RET_OK on success, or an error code in case of an error.
 */
int build_info() {
  ino_t nid;
  int status;
  unsigned short type;
  unsigned int size;
  list_t *l;
  process_t *p;
  namespace_t *ns;
  char *path;
  tree_t * ns_tree;

  if (!info || !(info->args) || !(info->process)) {
    report_error("build_info", debug_message(RET_ERR_PARAM), DEBUG_MSG);
    return RET_ERR_PARAM;
  }
  
  // Sort the process list first.
  sort_process_list(&(info->process));

  // Traverse each process.
  for (l = info->process; l; l = l->next) {
    // Link with the parent (if it is still alive).
    p = search_process_list(info->process, l->process->ppid);
    l->process->parent = p;

    // Get the process path on procfs.
    size = strlen(info->args->proc_mnt) + 11;
    if (!(path = malloc(size))) {
      report_error(NULL, debug_message(RET_ERR_NOMEM), ERROR_MSG); 
      return  RET_ERR_NOMEM;
    }
    memset(path, 0, size);    
    snprintf(path, strlen(info->args->proc_mnt) + 10,
	     "%s/%d\0", info->args->proc_mnt, l->process->pid);    
    
    // Process the  namespaces.
    for (type = 0; type < NSCOUNT; type++) {      
      if ((status = get_proc_namespace(path, type, &nid)) != RET_OK ) {
	l->process->namespace[type] = NULL;
	continue;
      }
      
      // Search the namespace in the tree.
      if ((ns_tree = search_namespace_tree(info->namespace[type], nid))) {
	
	// Found. Link it with the current process.
	ns = ns_tree->namespace;	
	l->process->namespace[type] = ns;
	if ((status = insert_process_list(&(ns->members), l->process)) != RET_OK) {
	  safe_free((void **)&path);
	  return status;
	}
      } else {
	// Not found. Build a new namespace entry.
	if (!(ns = create_empty_namespace())) {
	  report_error(NULL, debug_message(RET_ERR_NOMEM), ERROR_MSG); 
	  safe_free((void **)&path);
	  return RET_ERR_NOMEM;
	}
	ns->nid = nid;
	ns->type = type;
	if (type == USER) {
	  // It's a user namespace. We have to read the uid/gid map files.
	  if ((status = get_proc_uid_map(path, ns->uid_map)) != RET_OK) {
	    safe_free((void **)&path);
	    delete_namespace(&ns);
	    return status;
	  }
	  if ((status = get_proc_gid_map(path, ns->gid_map)) != RET_OK) {
	    safe_free((void **)&path);
	    delete_namespace(&ns);
	    return status;
	  }
	}
	ns->creator = l->process;
	ns->creator_pid = l->process->pid;
	if (p) {
	  if (p->namespace[type]) {	      
	    ns->pnid = p->namespace[type]->nid;
	  }
	} else {
	  ns->pnid = 0;
	}

	// Link the namespace with the current process.
	l->process->namespace[type] = ns;
	if ((status = insert_process_list(&(ns->members), l->process)) != RET_OK) {
	  safe_free((void **)&path);
	  delete_namespace(&ns);
	  return status;
	}

	//Add the new namespace to the tree.
	if ((status = insert_namespace_tree(&(info->namespace[type]), ns))!= RET_OK) {
	  safe_free((void **)&path);
	  ns->members = NULL;
	  safe_free((void **)&ns);
	  return status;
	}
      }
    }
    safe_free((void **)&path);
  }
  return RET_OK;
}
