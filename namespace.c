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
#include <errno.h>
#include <grp.h>
#include <pwd.h>
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
#include <sys/ioctl.h>

/**
 * @name create_empty_namespace - Create an empty namespace object.
 * @return Pointer to the new namespace object or NULL.
 */
namespace_t *create_empty_namespace() {
  namespace_t *n;
  unsigned short i;
  
  if (!(n = malloc(sizeof(namespace_t)))) {
    report_error(NULL, debug_message(RET_ERR_NOMEM), ERROR_MSG);
    return NULL;
  }  
  n->nid = n->pnid = 0;
  n->creator_pid = 0;
  n->creator = NULL;
  n->members = NULL;
  for (i = 0; i < MAP_LIMIT; i++) {
    n->uid_map[i].uid_inside = 0;
    n->uid_map[i].uid_inside = 0;
    n->uid_map[i].uid_outside = 0;
    n->uid_map[i].length = 0;
    n->gid_map[i].gid_inside = 0;
    n->gid_map[i].gid_outside = 0;
    n->gid_map[i].length = 0;
  }
  return n;
}

/**
 * @name delete_namespace - Delete a namespace.
 * @param ns: The address of the namespace object.
 * @return Void.
 */
void delete_namespace(namespace_t **ns) {
  list_t *c, *d;

  if (!ns || !(*ns)) 
    return;

    
  (*ns)->creator = NULL;
  c = (*ns)->members;
  while (c) {
    d = c;
    c = c->next;
    d->process = NULL;      
    safe_free((void **)&d);
  }
  (*ns)->members = NULL;
  safe_free((void **)ns);
}

/**
 * @name clear_namespace_tree - Clear a namespace tree.
 * @param tree: The address of a namespace tree.
 * @return Void.
 */
void clear_namespace_tree(tree_t **tree) {
  if (!tree || !(*tree)) 
    return;
  
  clear_namespace_tree(&(*tree)->child);
  clear_namespace_tree(&(*tree)->sibling);
  delete_namespace(&(*tree)->namespace);
  safe_free((void **)(tree));   
}

/**
 * @name count_namespace_tree - Count the nodes of a tree.
 * @param tree: Pointer to the tree object.
 * @return The number of nodes that exist on the given tree.
 */
unsigned long count_namespace_tree(tree_t *tree) {
  unsigned long count = 0;

  if (!tree) {
    report_error("count_namespace_tree", debug_message(RET_ERR_PARAM),
		 DEBUG_MSG);
    return 0;
  }
  
  count++;
  count += count_namespace_tree(tree->child);
  count += count_namespace_tree(tree->sibling);
  return count;
}

/**
 * @name search_namespace_tree - Search the tree for a namespace
 * @param tree: Pointer to a tree object.
 * @param nid: Namespace ID.
 * @return A tree node or NULL.
 */
tree_t *search_namespace_tree(tree_t *tree, const ino_t nid) {
  tree_t *result = NULL;
  
  if (!tree) {
    report_error("search_namespace_tree", debug_message(RET_ERR_PARAM),
		 DEBUG_MSG);
    return NULL;
  }

  if (tree->namespace) 
    if (tree->namespace->nid == nid)
      return tree;

  if ((result = search_namespace_tree(tree->child, nid)))
    return result;
  
  if ((result = search_namespace_tree(tree->sibling, nid)))
    return result;

  return NULL;
}

/**
 * @name insert_namespace_tree - Insert a namespace into the tree.
 * @param tree: The address of the namespace tree where the
 *              namespace will be inserted.
 * @param ns: Pointer to the namespace object that will be added to
 *        the tree.
 * @return RET_OK on success or an error code in case of an error.
 *
 * This method inserts a namespace into the given namespace tree.
 * If the namespace has a parent and its parent exists in the tree,
 * then it is inserted under its parent. Otherwise, it is inserted
 * directly under the root.
 */
int insert_namespace_tree(tree_t **tree, namespace_t *ns) {
  tree_t *c = NULL, *p = NULL, *s = NULL;

  if (!tree || !ns) {    
    report_error("insert_namespace_tree", debug_message(RET_ERR_PARAM),
		 DEBUG_MSG);
    return RET_ERR_PARAM;
  }
  
  if (!(c = malloc(sizeof(tree_t)))) {
    report_error(NULL, debug_message(RET_ERR_NOMEM), ERROR_MSG);  
    return RET_ERR_NOMEM;
  }
  c->namespace = ns;
  c->sibling = NULL;
  c->child = NULL;

  if(!(*tree)) {
    *tree = c;
    (*tree)->depth = 0;
    return RET_OK;
  }
  // If the namespace is orphan then insert it directly under the root.
  // Otherwise, insert it under its parent namespace.
  if (is_orphaned_namespace(ns))
    p = *tree;
  else
    if (!(p = search_namespace_tree(*tree, ns->pnid))) 
      p = *tree;

  c->depth = p->depth + 1;
  if (!(p->child)) {
    p->child = c;
  } else {
    s = p->child->sibling;
    if (!(p->child->sibling)) {
      p->child->sibling = c;
    } else {	
      for (s = p->child->sibling; s; s = s->sibling)
	if (!(s->sibling)) {
	  s->sibling = c;
	  break;
	}
    }
  }
  return RET_OK;
}

/**
 * @name print_width - Print spaces on the left of a sibling.
 * @return Void.
 */
void print_width(const unsigned int depth) {
  unsigned int i;
  
  for (i = 0; i < depth; i++)   
    printf("     | ");
}

/**
 * @name print_branch - Print spaces on the left of a child.
 * @return void.
 */
void print_branch(const unsigned int depth) {
  unsigned int i;
  
  for (i = 0; i < depth; i++)   
    printf("     +");
}

/**
 * @name print_namespace_info - Print extended namespace information.
 * @param ns: Pointer to a namespace.
 * @param depth: The namespace's depth on the tree.
 * @return Void.
 */
void print_namespace_info(const namespace_t *ns, const unsigned int depth) {
  unsigned int j;
  list_t *pl;
  const char *titles[] = {
    "Type",
    "ID",
    "First member",
    "User",
    "Group",
    "Parent namespace ID",
    "Owner user namespace",
    "Member Processes",
    "UID Map",
    "GID Map",
    "Member processes"
  };
  unsigned int current, width = 0;
  unsigned int max_width = strlen(titles[6]);
  struct passwd *passwd;
  struct group *group;
  struct winsize w;
  char printstr[1024];
  
  if (!ns) {
    report_error("print_namespace_info", debug_message(RET_ERR_PARAM),
		 DEBUG_MSG);
    return;
  }
  
  if (depth <= 0) {    
    // Type
    printf("%-*s: %s\n", max_width, titles[0], get_name_from_type(ns->type));

    // ID
    printf("%-*s: %ld\n", max_width, titles[1], ns->nid);
  }
  
  // Creator process
  print_width(depth);    
  printf("%-*s: ", max_width, titles[2]);
  if (ns->creator)
    printf("%s <%ld>\n", ns->creator->name, ns->creator->pid);
  else if (ns->creator_pid == 0)
    printf("%s <%ld>\n", "System", ns->creator_pid);
  else
    printf("%s\n", "Unknown");

  // User
  print_width(depth);    
  printf("%-*s: ", max_width, titles[3]);
  if (ns->creator) {
    passwd = getpwuid(ns->creator->uid);
    if (passwd) 
      if (passwd->pw_name)
	printf("%s [%ld]\n", passwd->pw_name, ns->creator->uid);
      else
	printf("%s [%ld]\n", "Unknown", ns->creator->uid);
    else
      printf("%s [%ld]\n", "Unknown", ns->creator->uid);
  } else if (ns->creator_pid == 0) {
    printf("%s [%s]\n", "root", "0");
  } else {
    printf("%s [%s]\n", "Unknown", "Unknown");
  }

  // Group
  print_width(depth);    
  printf("%-*s: ", max_width, titles[4]);
  if (ns->creator) {
    group = getgrgid(ns->creator->gid);
    if (group) 
      if (group->gr_name)
	printf("%s [%ld]\n", group->gr_name, ns->creator->uid);
      else
	printf("%s [%ld]\n", "Unknown", ns->creator->uid);
    else
      printf("%s [%ld]\n", "Unknown", ns->creator->uid);
  } else if (ns->creator_pid == 0) {
    printf("%s [%s]\n", "root", "0");
  } else {
    printf("%s [%s]\n", "Unknown, Unknown");
  }

  // Parent namespace ID
  print_width(depth);    
  if (ns->pnid)
    printf("%-*s: %ld\n", max_width, titles[5], ns->pnid);
  else
    printf("%-*s: %s\n", max_width, titles[5], "-");
  
  // Owner user namespace
  print_width(depth);    
  printf("%-*s: ", max_width, titles[6]);
  if (ns->creator) 
    if (ns->creator->namespace[USER])
      printf("%ld\n", ns->creator->namespace[USER]->nid);
    else
      printf("%s\n", "Unknown");
  else if (ns->creator_pid == 0)
    if (info->namespace[USER])
      if (info->namespace[USER]->namespace)
	printf("%ld\n", info->namespace[USER]->namespace->nid);
      else
	printf("%s\n", "Unknown");
    else
      printf("%s\n", "Unknown");
  else
    printf("%s\n", "Unknown");
  
  // Member processes
  print_width(depth);    
  printf("%-*s: %ld\n", max_width, titles[7],count_process_list(ns->members));
  
  // UID & GID Map
  if (ns->type == USER) {
    for (j = 0; j < MAP_LIMIT; j++) {
      if (ns->uid_map[j].length > 0) {
	width = max_width - strlen(titles[8]) - 1;
	print_width(depth);    
	printf("%s %-*d: [%ld, %ld, %ld]\n", titles[8], width, j, 
	       ns->uid_map[j].uid_inside, ns->uid_map[j].uid_outside,
	       ns->uid_map[j].length);
      }
    }
    for (j = 0; j < MAP_LIMIT; j++) {
      if (ns->gid_map[j].length > 0) {
	width = max_width - strlen(titles[9]) - 1;
	print_width(depth);    
	printf("%s %-*d: [%ld, %ld, %ld]\n", titles[9], width, j,
	       ns->gid_map[j].gid_inside, ns->gid_map[j].gid_outside,
	       ns->gid_map[j].length);
      }
    }
  }
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  
  // Member processes
  if ((info->args->flags & FLAG_PROCESS) && depth <= 0) {
    current = printf("%-*s: ", max_width, titles[10]);
    if (pl = ns->members) {      
      current += printf("%s <%d>, ", pl->process->name, pl->process->pid);
      pl = pl->next;
    }
    while (pl) {
      memset(printstr, 0, 1024);
      snprintf(printstr, 1024, "%s <%d>, ", pl->process->name, pl->process->pid);
      if (current + strlen(printstr) > w.ws_col) {	
	printf("\n");
	current = printf("%-*s  ", max_width, "");
      }
      current += printf("%s <%d>, ", pl->process->name, pl->process->pid);
      pl = pl->next;
    }
    printf("\b\b \n");
  }
}

/**
 * @name print_namespace_tree - Print a namespace tree node.
 * @param ns: Pointer to a namespace.
 * @param depth: The namespace's depth on the tree.
 * @return Void.
 */
void print_namespace_tree(const namespace_t *ns, const unsigned int depth){
  unsigned int i, j, chars;
  list_t *pl;

  if (!ns) {
    report_error("print_namespace_tree", debug_message(RET_ERR_PARAM),
		 DEBUG_MSG);
    return;
  }

  // Print namespace.
  print_branch(depth);  
  printf("-- [%s][%ld]\n", get_name_from_type(ns->type), ns->nid);
  if (info->args->flags & FLAG_EXTEND)
    print_namespace_info(ns, depth + 1);
  
  // Print process.
  if (info->args->flags & FLAG_PROCESS) {
    print_width(depth+1);
    printf("\n");
    for (pl = ns->members; pl; pl = pl->next) {  
      print_branch(depth+1);
      printf("-- %s <%d>\n", pl->process->name, pl->process->pid);
    }
    print_width(depth+1);
    printf("\n");    
  }
}

/**
 * @name print_parented_namespaces - Print all the parented namespaces
 * @param tree: Pointer to a namespace tree.
 * @return Void.
 *
 * This method traverses the tree in pre-order and prints each parented
 * namespace that encounters.
 */
void print_parented_namespaces(const tree_t *tree) {
  if (tree) {    
    if (tree->namespace)
      if (!is_orphaned_namespace(tree->namespace))
	print_namespace_tree(tree->namespace, tree->depth);
    print_parented_namespaces(tree->child);
    print_parented_namespaces(tree->sibling);
  }
}

/**
 * @name print_orphaned_namespaces - Print all the orphaned namespaces
 * @param tree: Pointer to a namespace tree.
 * @return Void.
 *
 * This method traverses the tree in pre-order and prints each orphaned
 * namespace that encounters.
 */
void print_orphaned_namespaces(const tree_t *tree) {
  if (tree) 
    if (tree->namespace)
      if (is_orphaned_namespace(tree->namespace)) {
        print_namespace_tree(tree->namespace, tree->depth);        
	if (tree->depth == 0) 
	  print_orphaned_namespaces(tree->child);
	else
	  print_parented_namespaces(tree->child);
        print_orphaned_namespaces(tree->sibling);
      } else {
        print_orphaned_namespaces(tree->child);
        print_orphaned_namespaces(tree->sibling);      
      }
}

/**
 * @name is_orphaned_namespace - Check if a namespace is orphaned.
 * @param n: Pointer to a namespace object.
 * @return 0 if the namespace is not orphaned, 1 otherwise.
 */
unsigned short is_orphaned_namespace(const namespace_t *n) {
  if (n) 
    if (n->pnid || n->creator_pid == 1)
      return 0;
  return 1;
}

/**
 * @name get_name_from_type - Convert the namespace type to string.
 * @param type: The namespace type.
 * @return Namespace type string.
 */
const char *get_name_from_type(const unsigned short type) {
  switch (type) {
    case IPC:
      return "IPC";
    case MNT:
      return "MNT";
    case NET:
      return "NET";
    case PID:
      return "PID";      
    case USER:
      return "USER";      
    case UTS:
      return "UTS";
    case CGROUP:
      return "CGROUP";
    default:
      return "UNKNOWN";
  }
}

/**
 * @name get_namespace_file - Get the namespace file under procfs.
 * @param type: The namespace type.
 * @return Namespace file in string form.
 */
char *get_namespace_file(const unsigned short type) {
  switch (type) {
    case IPC:
      return "/ns/ipc";
    case MNT:
      return "/ns/mnt";
    case NET:
      return "/ns/net";
    case PID:
      return  "/ns/pid";
    case USER:
      return "/ns/user";
    case UTS:
      return "/ns/uts";
    case CGROUP:
      return "/ns/cgroup";
    default:
      return "";
  }
}

/**
 * @name get_proc_namespace - Get the namespace ID of a namespace.
 * @param proc_path: The path in procfs to look for the namespace.
 * @param type: The namespace type.
 * @param ns: Pointer to an ino_t where the result will be placed.
 * @return RET_OK on success, or an error code in case of an error.
 */
int get_proc_namespace(const char *proc_path, const unsigned short type, ino_t *ns) {
  char buffer[BUFFER_SIZE];
  char *target_path, *ns_file;
  struct stat sb;
  unsigned int size;
  
  if (!proc_path || !ns) {
    report_error("get_proc_namespace", debug_message(RET_ERR_PARAM), DEBUG_MSG);
    return RET_ERR_PARAM;
  }
  ns_file = get_namespace_file(type);
  size = strlen(proc_path) + strlen(ns_file);
  if (!(target_path = malloc(size+1))) {
    report_error(NULL, debug_message(RET_ERR_NOMEM), ERROR_MSG);
    return RET_ERR_NOMEM;
  }
  memset(target_path, 0, size+1);
  snprintf(target_path, BUFFER_SIZE, "%s%s\0", proc_path, ns_file);
  if (stat(target_path, &sb)) {
    report_error("get_proc_namespace", strerror(errno), DEBUG_MSG);
    safe_free((void **)&target_path); 
    return RET_ERR_NOLINK;
  }
  safe_free((void **)&target_path); 
  *ns =  sb.st_ino;
  return RET_OK;
}

/**
 * @name get_proc_uid_map - Get the UID map of a process.
 * @param proc_path: The path in procfs to look for the namespace.
 * @param uid_map: Pointer to a uid_map_t where the result will be placed.
 * @return RET_OK on success, or an error code in case of an error.
 */
int get_proc_uid_map(const char *proc_path, uid_map_t *uid_map) {
  char buffer[BUFFER_SIZE];
  int ret, i;
  char *target_path;
  FILE *fd;
  unsigned int size;
  
  if (!proc_path || !uid_map) {
    report_error("get_proc_uid_map", debug_message(RET_ERR_PARAM), DEBUG_MSG);
    return RET_ERR_PARAM;
  }
  size = strlen(proc_path) + strlen(PROCUIDMAPFILE);
  if (!(target_path = malloc(size+1))) {
    report_error(NULL, debug_message(RET_ERR_NOMEM), ERROR_MSG);
    return RET_ERR_NOMEM;
  }
  memset(target_path, 0, size+1);
  snprintf(target_path, BUFFER_SIZE, "%s%s\0", proc_path, PROCUIDMAPFILE);
  if (!(fd = fopen(target_path, "r"))) {
    report_error("get_proc_uid_map", strerror(errno), DEBUG_MSG);
    safe_free((void **)&target_path); 
    return RET_ERR_NOFILE;
  }
  safe_free((void **)&target_path); 
  i = 0;
  ret = fscanf(fd, "%ld%ld%ld\n",&(uid_map[i++].uid_inside),
               &(uid_map[i++].uid_outside),
               &(uid_map[i++].length));
  while (ret != EOF && i < MAP_LIMIT)
    ret = fscanf(fd, "%ld%ld%ld\n",&(uid_map[i++].uid_inside),
		 &(uid_map[i++].uid_outside),
		 &(uid_map[i++].length));
  
  fclose(fd);
  return RET_OK;
}

/**
 * @name get_proc_gid_map - Get the GID map of a process.
 * @param proc_path: The path in procfs to look for the namespace.
 * @param gid_map: Pointer to a gid_map_t where the result will be placed.
 * @return RET_OK on success, or an error code in case of an error.
 */
int get_proc_gid_map(const char *proc_path, gid_map_t *gid_map) {
  char buffer[BUFFER_SIZE];
  int ret, i;
  char *target_path;
  FILE *fd;
  unsigned int size;
  
  if (!proc_path || !gid_map) {
    report_error("get_proc_gid_map", debug_message(RET_ERR_PARAM), DEBUG_MSG);
    return RET_ERR_PARAM;
  }
  size = strlen(proc_path) + strlen(PROCGIDMAPFILE);
  if (!(target_path = malloc(size+1))) {
    report_error(NULL, debug_message(RET_ERR_NOMEM), ERROR_MSG);
    return RET_ERR_NOMEM;
  }
  memset(target_path, 0, size+1);
  snprintf(target_path, BUFFER_SIZE, "%s%s\0", proc_path, PROCGIDMAPFILE);
  if (!(fd = fopen(target_path, "r"))) {
    report_error("get_proc_gid_map", strerror(errno), DEBUG_MSG);
    safe_free((void **)&target_path); 
    return RET_ERR_NOFILE;
  }
  safe_free((void **)&target_path); 
  i = 0;
  ret = fscanf(fd, "%d%d%d\n",&(gid_map[i++]).gid_inside,
               &(gid_map[i++]).gid_outside,
               &(gid_map[i++]).length);
  while (ret != EOF && i < MAP_LIMIT)
    ret = fscanf(fd, "%d%d%d\n",&(gid_map[i++]).gid_inside,
		 &(gid_map[i++]).gid_outside,
		 &(gid_map[i++]).length);
  

  fclose(fd);
  return RET_OK;
}
