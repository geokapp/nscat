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
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "common.h"
#include "info.h"
#include "namespace.h"
#include "process.h"

/**
 * @name print_usage - Print usage information and exit.
 * @param is_error: 1 if the function was called as a response to an error.
 *        0 otherwise.
 * @return Void.
 */
void print_usage(const unsigned short is_error) {
  static const char *usage_template =
      "Usage: %s [ options ]\n"
      "   -t, --ns-type NS[,NS]...    Print information about the given\n"
      "                               namespaces only. The NS parameter\n"
      "                               can be one of: IPC, MNT, NET, PID,\n"
      "                               USER, UTS, CGROUP. The default is to\n"
      "                               print information about all namespaces.\n"
      "   -n, --ns NID                Print information only for the given\n"
      "                               namespace whose identifier matches NID.\n"
      "   -p, --pid PID               Print namespace information only\n"
      "                               for the process whose process ID\n"
      "                               matches PID.\n"
      "   -d, --descendants           This option can be used in conjuction\n"
      "                               with the --pid flag. It instructs the\n"
      "                               tool to print namespace information for\n"
      "                               the given process and its descendants.\n"
      "   -r, --show-procs            This option causes the tool to display\n"
      "                               all the process members of each namespace.\n"
      "   -e, --extend-info           Print extended information for each\n"
      "                               namespace.\n"
      "   -h, --help                  Print this help message and exit.\n"
      "   -v, --version               Print the version number and exit.\n";
  
  fprintf(is_error ? stderr : stdout, usage_template, "nscat");
  exit(is_error ? EXIT_FAILURE : EXIT_SUCCESS);
}

/**
 * @name print_version - Print program version and exit.
 * @return Void.
 */
void print_version() {
  printf("nscat version %s.\n", VERSION);
  exit(EXIT_SUCCESS);
}

/**
 * @name check_environment - Check the environment.
 * @return RET_OK on success, an error code in case of an error.
 *
 * This method checks if the procfs mount point is accessible. it also
 * checks if the caller's accunt has the apropriate privileges to access
 * the procfs entries. Finally, it checks if the system supports the
 * requested namespaces. unsupported namespaces are disabled.
 */
int check_environment() {
  char *path;
  unsigned int size;
  unsigned short type;

  if (!info || !(info->args) || !(info->args->proc_mnt)) {
    report_error("check_environment", debug_message(RET_ERR_PARAM), DEBUG_MSG);
    return RET_ERR_PARAM;
  }
  
  // Check the procfs mount point.
  if (access(info->args->proc_mnt, F_OK|R_OK)) {
    report_error(NULL, "The procfs mountpoint is not accessible", ERROR_MSG);
    return RET_ERR_NOFILE;
  }

  // Check the user ID.
  if (getuid())
    warn_permissions();
  
  // Check the system namespaces.
  for (type = 0; type < NSCOUNT; type++) {
    // Skip any namespaces that the user did not requested.
    if ((info->args->flags & FLAG_NSWANT) && !(info->args->wanted[type]))
      continue;

    size = strlen(info->args->proc_mnt) + 64;
    if (!(path = malloc(size+1))) {
      report_error(NULL, debug_message(RET_ERR_NOMEM), ERROR_MSG);
      return RET_ERR_NOMEM;
    }
    memset(path, 0, size+1);
    snprintf(path, size, "%s/%d/%s\0", info->args->proc_mnt, getpid(),
	     get_namespace_file(type));
    if (access(path, F_OK|R_OK)) {
      fprintf(stderr, "nsinfo: Warning - Your system does not support %s namespace.\n",
	      get_name_from_type(type));
      // disable this namespace.
      info->args->flags |= FLAG_NSWANT;
      info->args->wanted[type] = 0;
    }
    safe_free((void **)&path);    
  }
  return RET_OK;
}

/**
 * @name init - Initializes program arguments.
 * @param argc: main's argc.
 * @param argv: main's argv.
 * @return RET_OK on success, an error code in case of an error.
 */
int init(const int argc, char *argv[]) {
  int next_option;
  unsigned int ns;
  const char *short_options = "hvt:n:p:drm:e";
  const char delim[2] = ",";
  char *token;
  
  const struct option long_options[] = {
    {"help",        0, NULL, 'h'},
    {"version",     0, NULL, 'v'},
    {"ns-type",     1, NULL, 't'},
    {"ns",          1, NULL, 'n'},
    {"pid",         1, NULL, 'p'},
    {"descendants", 0, NULL, 'd'},    
    {"show-procs",  0, NULL, 'r'},
    {"proc-mnt",    1, NULL, 'm'},
    {"extend-info", 0, NULL, 'e'}
  };

  // Initialize info.
  if (!(info = malloc(sizeof(info_t)))) {
    report_error(NULL, debug_message(RET_ERR_NOMEM), ERROR_MSG);
    return RET_ERR_NOMEM;
  }  
  if (!(info->args = malloc(sizeof(callargs_t)))) {
    report_error(NULL, debug_message(RET_ERR_NOMEM), ERROR_MSG);
    safe_free((void **)&info);
    return RET_ERR_NOMEM;
  }
  info->args->flags = 0;
  info->args->pid = 0;
  info->args->ns = 0;
  if (!(info->args->proc_mnt = malloc(strlen(PROCMNT)+1))) {
    report_error(NULL, debug_message(RET_ERR_NOMEM), ERROR_MSG);
    safe_free((void **)&(info->args));
    safe_free((void **)&info);
    return RET_ERR_NOMEM;
  }
  memset(info->args->proc_mnt, 0, strlen(PROCMNT)+1);
  strncpy(info->args->proc_mnt, PROCMNT, strlen(PROCMNT));
  
  info->process = NULL;
  for (ns = 0; ns < NSCOUNT; ns++) {
    info->namespace[ns] = NULL;
    info->args->wanted[ns] = 0;
  }
  
  // Process the user arguments.
  do {
    next_option = getopt_long(argc, argv, short_options, long_options, NULL);
    switch (next_option) {
      case 'h':
	clear_info();	
	print_usage(0);
	return RET_OK;	
      case 'v':
	clear_info();	
	print_version();
	return RET_OK;
      case 't':
	info->args->flags |= FLAG_NSWANT;
	token = strtok(optarg, delim);
	while (token) {
	  if (!strcmp(token, "IPC")) 
	    info->args->wanted[IPC] = 1;
	  else if (!strcmp(token, "MNT")) 
	    info->args->wanted[MNT] = 1;
	  else if (!strcmp(token, "NET")) 
	    info->args->wanted[NET] = 1;
	  else if (!strcmp(token, "PID")) 
	    info->args->wanted[PID] = 1;
	  else if (!strcmp(token, "USER")) 
	    info->args->wanted[USER] = 1;
	  else if (!strcmp(token, "UTS")) 
	    info->args->wanted[UTS] = 1;
	  else if (!strcmp(token, "CGROUP")) 
	    info->args->wanted[CGROUP] = 1;
	  else {
	    fprintf(stderr, "nscat: Unrecognized namespace parameter.\n");
	    clear_info();
	    print_usage(1);
	    return RET_ERR_PARAM;
	  }
	  token = strtok(NULL, delim);
	}
	break;
      case 'n':
	info->args->ns = atol(optarg);	
	break;
      case 'p':
	info->args->pid = atol(optarg);
	break;
      case 'd':
	info->args->flags |= FLAG_DESCS;	
	break;
      case 'r':
	info->args->flags |= FLAG_PROCESS;	
	break;
      case 'm':
	if (strlen(optarg) > 0) {	  
	  safe_free((void **)&(info->args->proc_mnt));
	  if (!(info->args->proc_mnt = malloc(strlen(optarg)+1))) {
	    report_error(NULL, debug_message(RET_ERR_NOMEM), ERROR_MSG);
	    clear_info();
	    return RET_ERR_NOMEM;
	  }
	  memset(info->args->proc_mnt, 0, strlen(optarg)+1);
	  strncpy(info->args->proc_mnt, optarg, strlen(optarg));
	}	
	break;	
      case 'e':
	info->args->flags |= FLAG_EXTEND;	
	break;
      case -1:
	// Done with options.
	break;
      default:
	clear_info();
	print_usage(1);
	return RET_ERR_PARAM;
    }
  } while (next_option != -1);
  return check_environment();
}

/**
 * @name main
 */
int32_t main(int argc, char *argv[]) {
  
  // Perform initialization.
  if (init(argc, argv) != RET_OK)
    exit(EXIT_FAILURE);

  // Collect all the processes.
  if (collect_processes() != RET_OK)
    exit(EXIT_FAILURE);

  // Retrieve the namespace information.
  if (build_info() != RET_OK)
    exit(EXIT_FAILURE);

  // Print the namespace information.
  print_info();

  // Free up memory.
  clear_info();
  
  exit(EXIT_SUCCESS);
}
