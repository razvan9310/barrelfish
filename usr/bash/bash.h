/**
 * \file
 * \brief Barrelfish bash implementation.
 */

/*
 * Copyright (c) 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef _BASH_H
#define _BASH_H

// Helper Functions
void get_command(void);
void do_backslash(void);
void clean_buffer(void);
bool is_space(char c);
errval_t sanitize_input(char *);
void execute_command(char **argc, int argv);
char** get_suggestion(char *command);

// Command Functions
errval_t handle_echo(char *argc[], int argv);
errval_t handle_ps(char *argc[], int argv);
errval_t handle_threads(char *argc[], int argv);
errval_t handle_light_led(char *argc[], int argv);
errval_t handle_memtest(char *argc[], int argv);
errval_t handle_pwd(char *argc[], int argv);
errval_t handle_cat(char *argc[], int argv);
errval_t handle_ls(char *argc[], int argv);
errval_t handle_cd(char *argc[], int argv);
errval_t handle_wc(char *argc[], int argv);
errval_t handle_grep(char *argc[], int argv);
errval_t handle_oncore(char *argc[], int argv);
errval_t handle_help(char *argc[], int argv);
errval_t handle_mkdir(char *argc[], int argv);
errval_t handle_rm(char *argc[], int argv);
errval_t handle_rmdir(char *argc[], int argv);
errval_t handle_clear(char *argc[], int argv);

typedef void (*command_handler_fn)(char* const argc[], int argv);
struct command_handler_entry
{
    char* name;
    command_handler_fn handler;
};

#define SHELL_PRINTF(...) fprintf(__VA_ARGS__)

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#endif
