/*
 * Copyright (c) 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <fs/dirent.h>
#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include "bash.h"
#include <fs/fs.h>
#include <fs/ramfs.h>
#include <aos/inthandler.h>
#include <aos/waitset.h>

// Some global variable
char *input;                  // User Input String
char *wd;                     // Working directory
uint64_t max_len_wd = 20;     // Current Max Length of working directory
uint64_t pos_wd = 0;		  // Current Length of working directory
uint64_t buf_size = 16;		  // Buffer size of input array
uint64_t pos = 0;			  // Current size of buffer array
uint32_t inputRedirect = 0;   // Input Redirection Bit
uint32_t outputRedirect = 0;  // Output Redirection Bit
FILE *fout;					  // File out
FILE *fin;					  // File in

// BIG TODO: CHANGE ALL FUNCTION DEFINITION TO ERRVAL_T
// Better error handling mechanisms

// Extra Challenges
// [OK] Move IRQ CAP to BASH Process
// [OK] Redirect Input/Output to File
// [] Pipe Commands
// [OK] Tab to autocomplete


static struct command_handler_entry commands[] = 
			   {{.name = "mkdir",  	.handler = (void*) handle_mkdir},
				{.name = "rmdir",  	.handler = (void*) handle_rmdir},
				{.name = "help",   	.handler = (void*) handle_help},
				{.name = "led",    	.handler = (void*) handle_light_led},
				{.name = "oncore", 	.handler = (void*) handle_oncore},
				{.name = "threads", .handler = (void*) handle_threads},
				{.name = "cd", 		.handler = (void*) handle_cd},
				{.name = "ls", 		.handler = (void*) handle_ls},
				{.name = "memtest", .handler = (void*) handle_memtest},
				{.name = "echo", 	.handler = (void*) handle_echo},
				{.name = "ps", 		.handler = (void*) handle_ps},
				{.name = "pwd", 	.handler = (void*) handle_pwd},
				{.name = "cat", 	.handler = (void*) handle_cat},
				{.name = "wc", 		.handler = (void*) handle_wc},
				{.name = "grep", 	.handler = (void*) handle_grep},
				{.name = "rm", 		.handler = (void*) handle_rm}};

// Helper Functions:

char** get_suggestion(char *command) 
{
	char **result;
	result = malloc(16*sizeof(char *));
	uint32_t commands_size = sizeof(commands)/sizeof(commands[0]);
	uint32_t len = strlen(command);
	uint32_t j = 0;
	for(int i=0; i<commands_size; i++) {
		if(strncmp(command, commands[i].name, len-1) == 0) {
			result[j] = commands[i].name;
			j++;
		}
	}
	return result;
}

void do_backslash(void)
{
	aos_rpc_serial_putchar(get_init_rpc(), '\b');
	aos_rpc_serial_putchar(get_init_rpc(), ' ');
	aos_rpc_serial_putchar(get_init_rpc(), '\b');
}

bool is_space(char c)
{
	if(c == ' ') return true;
	else if(c == '\t') return true;
	else if(c == '\n') return true;
	else if(c == '\r') return true;
	return false;
}

void clean_buffer(void)
{
	free(input);
	pos = 0;
	if(inputRedirect>0) fclose(fin);
	if(outputRedirect>0) fclose(fout);
	inputRedirect = 0;
	outputRedirect = 0;
	buf_size = 16;
	input = malloc(buf_size);
	fout = stdout;
	fin = stdin;
}

static void terminal_read_handler(void *params)
{
	aos_rpc_serial_getchar(get_init_rpc(), (input+pos));
	if(*(input+pos) == '\r') {
		*(input+pos) = 0;
		sanitize_input(input);
	}else {
		if(*(input+pos) != '\t'){
			if(*(input+pos) == 127) {
				if(pos) {
					do_backslash();
					pos--;
				}else {
					printf("%c", '\a');
					fflush(stdout);
				}
			}else {
				aos_rpc_serial_putchar(get_init_rpc(), *(input+pos));
				pos++;
			}
			if(pos == buf_size) {
				input = realloc(input, 2*buf_size);
				buf_size = 2*buf_size;
			}
		}else {
			printf("\n");
			printf("\033[2K\r");
			char **result;
			result = get_suggestion(input);
			uint32_t mov_r = 4+strlen(input)+1;
			uint32_t mov_l = 0;
			while(*result){
				SHELL_PRINTF(fout, "%s  ", *result);
				mov_l+=(strlen(*result)+2);
				result++;
			}
			printf("\033[1A");
			printf("\033[%dD", mov_l);
			printf("\033[%dC", mov_r);
			fflush(stdout);
		}
	}
}

errval_t handle_echo(char *argc[], int argv)
{
	for(int i=1; i<argv; i++) {
		SHELL_PRINTF(fout, "%s ", argc[i]);
	}
	SHELL_PRINTF(fout, "\n");
	return SYS_ERR_OK;
}

errval_t handle_ps(char *argc[], int argv)
{
	domainid_t *pids_core0;
	size_t pid_count_core0;
	domainid_t *pids_core1;
	size_t pid_count_core1;
	coreid_t core = 0;
	errval_t err; 
	CHECK("Err in getting all the pids", aos_rpc_process_get_all_pids(get_init_rpc(),core,
								 &pids_core0, &pid_count_core0));

	char *name;
	SHELL_PRINTF(fout, "Core      Pids    Process Name\n");
	SHELL_PRINTF(fout, "================================\n");
	for(int i=0; i<pid_count_core0; i++) {
		aos_rpc_process_get_name(get_init_rpc(), *pids_core0, core, &name);
		SHELL_PRINTF(fout, "%d          %d       %s \n", core, *pids_core0, name);
		pids_core0++;
	}
	core = 1;
	err = aos_rpc_process_get_all_pids(get_init_rpc(),core, &pids_core1, 
										&pid_count_core1);
	if(err_is_fail(err)) {
		DEBUG_ERR(err, "Getting all pids");
	}
	for(int i=0; i<pid_count_core1; i++) {
		aos_rpc_process_get_name(get_init_rpc(), *pids_core1, core, &name);
		SHELL_PRINTF(fout, "%d          %d       %s \n", core, *pids_core1, name);
		pids_core1++;
	}
	return SYS_ERR_OK;
}

static void pesudo_task(void)
{
	// char *a = malloc(10);
	// a = "hello";
	// printf("%s\n", a);
	SHELL_PRINTF(fout, "###### Hello World from a new thread &&&&&&\n");
	//SHELL_PRINTF("My id is: %d\n", thread_get_id(thread_self()));
}

errval_t handle_threads(char *argc[], int argv)
{
	if(argv != 2) {
		printf("Syntax error: threads <number of threads>\n");
		return BASH_ERR_SYNTAX;
	}
	int num_threads = atoi(argc[1]);
	struct thread *t[num_threads];
	for(int i=0; i<num_threads; i++) {
		t[i] = thread_create((thread_func_t) pesudo_task, NULL);
	}

	for(int i=0; i<num_threads; i++) {
		thread_join(t[i], NULL);
	}
	return SYS_ERR_OK;
}

static void thread_handle_memtest(void *params)
{
	size_t *size = (size_t *)params;
	char *a =  malloc(*size);
	for(int i=0; i<*size; i++) {
		*(a+i) = 'J';
	}
	SHELL_PRINTF(fout, "Result of memtest: %s\n", a);
}

errval_t handle_memtest(char *argc[], int argv)
{
	size_t *size = malloc(sizeof(size_t));
	*size = atoi(argc[1]);
	struct thread *t = thread_create((thread_func_t) thread_handle_memtest, (void*)size);
	thread_join(t, NULL);
	return SYS_ERR_OK;
}

errval_t handle_light_led(char *argc[], int argv)
{
	if(argv != 2) {
		printf("Syntax error: led on/off\n");
		return BASH_ERR_SYNTAX;
	}
	uintptr_t status = 0;
	coreid_t core = 0;
	if(strncmp(argc[1], "on", 2) == 0) status = 1;
	else if(strncmp(argc[1], "off", 3) == 0) status = 2;
	else {
		printf("Syntax error: led on/off\n");
		return BASH_ERR_SYNTAX;
	}
	CHECK("ERR in aos_rpc_light_led", aos_rpc_light_led(get_init_rpc(), status, core));
	return SYS_ERR_OK;
}

errval_t handle_pwd(char *argc[], int argv)
{
	SHELL_PRINTF(fout, "Current directory is: %s\n", wd);
	return SYS_ERR_OK;	
}

errval_t handle_ls(char *argc[], int argv)
{
	fs_dirhandle_t ent;
	opendir(wd, &ent);
	char *name;
	while((readdir(ent, &name)) == SYS_ERR_OK) {
		SHELL_PRINTF(fout, "%s\n", name);
	}
	return SYS_ERR_OK;
}

errval_t handle_mkdir(char *argc[], int argv)
{
	CHECK("Error in creating directory\n", mkdir(argc[1]));
	return SYS_ERR_OK;
}

errval_t handle_rm(char *argc[], int argv)
{
	CHECK("Error in deleting file\n", rm(argc[1]));
	return SYS_ERR_OK;
}

errval_t handle_rmdir(char *argc[], int argv)
{
	CHECK("Error in deleting folder\n", rmdir(argc[1]));
	return SYS_ERR_OK;
}

errval_t handle_cd(char *argc[], int argv)
{
	if(argv != 2) {
		printf("Correct Syntax for cd is: cd <name of directory>\n");
		return BASH_ERR_SYNTAX;
	}
	if(strlen(wd)+strlen(argc[1]) >= (max_len_wd-1)) {
		max_len_wd = max(2*max_len_wd, strlen(wd)+strlen(argc[1]));
		wd = realloc(wd, 2*max_len_wd);
	}
	if(strncmp(argc[1], "..", 2) == 0) {
		if (pos_wd == 1) return SYS_ERR_OK;
		pos_wd-=2;
		wd[pos_wd] = '\0';
		pos_wd--;
		while(wd[pos_wd] != '/') {
			wd[pos_wd] = '\0';
			pos_wd--;
		}
		pos_wd++;
	}else {
		strcat(wd, argc[1]);
		pos_wd+=strlen(argc[1]);

		if(wd[pos_wd] != '/') {
			wd[pos_wd] = '/';
			pos_wd++;
		}
		pos_wd++;
	}
	//strcat(wd, argc[1]);
	return SYS_ERR_OK;
}

static errval_t private_cd(char *name)
{
	if(strlen(wd)+strlen(name) >= (max_len_wd-1)) {
		max_len_wd = max(2*max_len_wd, strlen(wd)+strlen(name));
		wd = realloc(wd, 2*max_len_wd);
	}
	if(strncmp(name, "..", 2) == 0) {
		if (pos_wd == 1) return SYS_ERR_OK;
		pos_wd-=2;
		wd[pos_wd] = '\0';
		pos_wd--;
		while(wd[pos_wd] != '/') {
			wd[pos_wd] = '\0';
			pos_wd--;
		}
		pos_wd++;
	}else {
		strcat(wd, name);
		pos_wd+=strlen(name);
		if(wd[pos_wd] != '/') {
			wd[pos_wd] = '/';
			pos_wd++;
		}
		pos_wd++;
	}
	//strcat(wd, name);
	return SYS_ERR_OK;
}

errval_t handle_cat(char *argc[], int argv)
{
	FILE *fp;
	int c = 0;
	fp = fopen(argc[1], "r");
	if(fp == NULL) {
		printf("No file named %s\n", argc[1]);
		return BASH_ERR_FILE_NOT_FOUND;
	}
	while ((c = fgetc(fp)) != EOF) { 
		SHELL_PRINTF(fout, "%c", c);
	}
	fclose(fp);
	return SYS_ERR_OK;
}

errval_t handle_wc(char *argc[], int argv)
{
	if(argv != 2) {
		printf("Correct Syntax for cat is : wc <name of the file>\n");
		return BASH_ERR_SYNTAX;
	}
	FILE *fp;
	fp = fopen(argc[1], "r");
	if(fp == NULL) {
		printf("No file named %s\n", argc[1]);
		return BASH_ERR_FILE_NOT_FOUND;
	}
	int tot_chars = 0;     /* total characters */
	int tot_lines = 0;     /* total lines */
	int tot_words = 0;     /* total words */
	int in_space = 1;
	int c, last = '\n';

	while ((c = fgetc(fp)) != EOF) {
		last = c;
		tot_chars++;
		if (is_space(c)) {
			in_space = 1;
			if (c == '\n') {
				tot_lines++;
			}
		} else {
			tot_words += in_space;
			in_space = 0;
		}
	}
	if (last != '\n') {
		/* count last line if not linefeed terminated */
		tot_lines++;
	}

	SHELL_PRINTF(fout, "Lines, Words, Characters\n");
	SHELL_PRINTF(fout, "%3d    %3d     %3d\n", tot_lines, tot_words, tot_chars);
	fclose(fp);
	return SYS_ERR_OK;
}
int	grep(char*, FILE*, char*);
int	match(char*, char*);
int	matchhere(char*, char*);
int	matchstar(int, char*, char*);

/* grep: search for regexp in file */
int grep(char *regexp, FILE *f, char *name)
{
	int n, nmatch;
	char buf[1024];

	nmatch = 0;
	while (fgets(buf, sizeof buf, f) != NULL) {
		n = strlen(buf);
		if (n > 0 && buf[n-1] == '\n')
			buf[n-1] = '\0';
		if (match(regexp, buf)) {
			nmatch++;
			if (name != NULL)
				SHELL_PRINTF(fout, "%s: ", name);
			SHELL_PRINTF(fout, "%s\n", buf);
		}
	}
	return nmatch;
}

/* matchhere: search for regexp at beginning of text */
int matchhere(char *regexp, char *text)
{
	if (regexp[0] == '\0')
		return 1;
	if (regexp[1] == '*')
		return matchstar(regexp[0], regexp+2, text);
	if (regexp[0] == '$' && regexp[1] == '\0')
		return *text == '\0';
	if (*text!='\0' && (regexp[0]=='.' || regexp[0]==*text))
		return matchhere(regexp+1, text+1);
	return 0;
}

/* match: search for regexp anywhere in text */
int match(char *regexp, char *text)
{
	if (regexp[0] == '^')
		return matchhere(regexp+1, text);
	do {	/* must look even if string is empty */
		if (matchhere(regexp, text))
			return 1;
	} while (*text++ != '\0');
	return 0;
}

/* matchstar: search for c*regexp at beginning of text */
int matchstar(int c, char *regexp, char *text)
{
	do {	/* a * matches zero or more instances */
		if (matchhere(regexp, text))
			return 1;
	} while (*text != '\0' && (*text++ == c || c == '.'));
	return 0;
}

errval_t handle_grep(char *argc[], int argv)
{
	if(strncmp(argc[1], "-r", 2) == 0){
		fs_dirhandle_t ent;
		CHECK("Open pwd", opendir(wd, &ent));
		char *name;
		while((readdir(ent, &name)) == SYS_ERR_OK) {
			private_cd(name);
			fs_dirhandle_t temp;
			errval_t err = opendir(wd, &temp);
			if(err == SYS_ERR_OK) {
				handle_grep(argc, argv);
			}else {
				FILE *fp;
				wd[pos_wd-2] = '\0';
				fp = fopen(wd, "r");
				if(fp == NULL) {
					printf("No file named: %s\n", wd);
					return BASH_ERR_FILE_NOT_FOUND;
				}
				grep(argc[2], fp, wd);
				fclose(fp);
				wd[pos_wd-2] = '/';
			}
			private_cd("..");
		}
	}else {
		FILE *fp;
		fp = fopen(argc[2], "r");
		if(fp == NULL) {
			printf("No file named: %s\n", argc[2]);
			return BASH_ERR_FILE_NOT_FOUND;
		}
		grep(argc[1], fp, argc[2]);
		fclose(fp);
	}

	return SYS_ERR_OK;
}

errval_t handle_oncore(char *argc[], int argv)
{
	domainid_t newpid;
	if(argv <= 2) {
		printf("Correct Syntax for oncore is oncore <coreid> <proc_name> <proc_args>\n");
		return BASH_ERR_SYNTAX;
	}else if(argv == 3) {
		coreid_t core = atoi(argc[1]);
		CHECK("Trying to spawn with arguments",
			aos_rpc_process_spawn(get_init_rpc(), argc[2], core, &newpid));
	}else {
		coreid_t core = atoi(argc[1]);
		uint32_t all_args = 0;
		for(int i=2; i<argv; i++) {
			all_args += strlen(argc[i]);
		}
		char *proc_name = malloc(all_args+(argv-1));
		memset(proc_name, 0, all_args+(argv-1));
		strncpy(proc_name, argc[2], strlen(argc[2]));
		strcat(proc_name, " ");
		for(int i=3; i<argv; i++) {
			strcat(proc_name, argc[i]);
			strcat(proc_name, " ");
		}

		CHECK("Trying to spawn with arguments",
			aos_rpc_process_spawn_args(get_init_rpc(), proc_name, core, &newpid));
		free(proc_name);
	}
	return SYS_ERR_OK;
}

errval_t handle_spawn(char *argc[], int argv)
{
	domainid_t newpid;
	if(argv == 1) {
		coreid_t core = 0;
		CHECK("Trying to spawn with arguments",
			aos_rpc_process_spawn(get_init_rpc(), argc[0], core, &newpid));
	}else {
		coreid_t core = 0;
		uint32_t all_args = 0;
		for(int i=1; i<argv; i++) {
			all_args += strlen(argc[i]);
		}
		char *proc_name = malloc(all_args+(argv-1));
		memset(proc_name, 0, all_args+(argv-1));
		strncpy(proc_name, argc[0], strlen(argc[0]));
		strcat(proc_name, " ");
		for(int i=1; i<argv; i++) {
			strcat(proc_name, argc[i]);
			strcat(proc_name, " ");
		}

		CHECK("Trying to spawn with arguments",
			aos_rpc_process_spawn_args(get_init_rpc(), proc_name, core, &newpid));
		free(proc_name);
	}
	return SYS_ERR_OK;
}

errval_t handle_help(char *argc[], int argv)
{
	SHELL_PRINTF(fout, "List of available commands are:\n");
	SHELL_PRINTF(fout, "echo\n");
	SHELL_PRINTF(fout, "led on/off\n");
	SHELL_PRINTF(fout, "threads <number of threads>\n");
	SHELL_PRINTF(fout, "memtest <size of memory>\n");
	SHELL_PRINTF(fout, "oncore <coreid> <process name>\n");
	SHELL_PRINTF(fout, "ps\n");
	SHELL_PRINTF(fout, "help\n");
	SHELL_PRINTF(fout, "pwd\n");
	SHELL_PRINTF(fout, "cd <name of folder>\n");
	SHELL_PRINTF(fout, "ls\n");
	SHELL_PRINTF(fout, "cat <name of file>\n");
	SHELL_PRINTF(fout, "wc <name of file>\n");
	SHELL_PRINTF(fout, "grep <pattern> <name of file>\n");
	SHELL_PRINTF(fout, "mkdir <name of new folder>\n");
	SHELL_PRINTF(fout, "rmdir <name of new folder>\n");
	SHELL_PRINTF(fout, "rm <name of file>\n");
	return SYS_ERR_OK;
}

errval_t handle_clear(char *argc[], int argv)
{
	printf("\033[2J");
	return SYS_ERR_OK;
}

errval_t sanitize_input(char *ip)
{
	int argc_size = 5;
	int argv = 0;
	char **argc;
	argc = malloc(argc_size*sizeof(char*));
	int i = 0;
	while(is_space(ip[i])) i++;
	while(ip[i]) {
		argc[argv] = &ip[i];
		argv++;
		if(argv == argc_size) {
			argc = realloc(argc, 2*argc_size*sizeof(char*));
			argc_size = 2*argc_size;
		}
		while(!is_space(ip[i]) && ip[i]) i++;
		while(is_space(ip[i]) && ip[i]) i++;
		if(ip[i])
			ip[i-1] = '\0';
	}
	aos_rpc_serial_putchar(get_init_rpc(), '\n');
	for(int j=0; j<argv; j++) {
		if(strncmp(argc[j], "<", 1) == 0) {
			inputRedirect = 1;
			if((j+1) == argv) {
				SHELL_PRINTF(fout, "Input Redirection File Missing\n");
				return BASH_ERR_SYNTAX;
			}else {
				fin = fopen(argc[j+1], "r");
			}
		}else if(strncmp(argc[j], "<<", 2) == 0) {
			inputRedirect = 2;
			if((j+1) == argv) {
				SHELL_PRINTF(fout, "Input Redirection File Missing\n");
				return BASH_ERR_SYNTAX;
			}else {
				fin = fopen(argc[j+1], "r");
			}
		}else if(strncmp(argc[j], ">", 1) == 0) {
			outputRedirect = 1;
			if((j+1) == argv) {
				SHELL_PRINTF(fout, "Output Redirection File Missing\n");
				return BASH_ERR_SYNTAX;
			}else {
				fout = fopen(argc[j+1], "w+");
			}
		}else if(strncmp(argc[j], ">>", 1) == 0) {
			outputRedirect = 2;
			if((j+1) == argv) {
				SHELL_PRINTF(fout, "Output Redirection File Missing\n");
				return BASH_ERR_SYNTAX;
			}else {
				fout = fopen(argc[j+1], "a");
				int t = fseek(fout, -1, SEEK_END);
				printf("%d\n", t);
				if(fout == NULL) {
					printf("Fuck me!!\n");
				}
			}
		}
	}
	execute_command(argc, argv);
	clean_buffer();
	printf("\n");
	SHELL_PRINTF(fout, "$bash>");
	fflush(stdout);
	return SYS_ERR_OK;
}

void execute_command(char *argc[], int argv)
{
	/* List of commands
		- [OK] echo
		- [OK] led
		- [OK] threads
		- [OK] memtest
		- [OK] oncore
		- [OK] ps
		- [OK] help
		- [OK] pwd
		- [OK] cd
		- [OK] ls
		- [OK] cat
		- [Ok] wc
		- [OK] grep
		- [OK] mkdir
		- [OK] rmdir
		- [OK] rm
	*/
	if(argv > 0) {
		if(strncmp(argc[0], "echo", 4) == 0) {
			handle_echo(argc, argv);
		}else if(strncmp(argc[0], "led", 3) == 0){
			handle_light_led(argc, argv);
		}else if(strncmp(argc[0], "threads", 7) == 0){
			handle_threads(argc, argv);
		}else if(strncmp(argc[0], "memtest", 7) == 0){
			handle_memtest(argc, argv);
		}else if(strncmp(argc[0], "oncore", 6) == 0){
			handle_oncore(argc, argv);
		}else if(strncmp(argc[0], "ps", 2) == 0){
			handle_ps(argc, argv);
		}else if(strncmp(argc[0], "help", 4) == 0){
			handle_help(argc, argv);
		}else if(strncmp(argc[0], "pwd", 3) == 0){
			handle_pwd(argc, argv);
		}else if(strncmp(argc[0], "cd", 2) == 0){
			handle_cd(argc, argv);
		}else if(strncmp(argc[0], "ls", 2) == 0){
			handle_ls(argc, argv);
		}else if(strncmp(argc[0], "cat", 3) == 0){
			handle_cat(argc, argv);
		}else if(strncmp(argc[0], "wc", 2) == 0){
			handle_wc(argc, argv);
		}else if(strncmp(argc[0], "grep", 4) == 0){
			handle_grep(argc, argv);
		}else if(strncmp(argc[0], "mkdir", 5) == 0){
			handle_mkdir(argc, argv);
		}else if(strncmp(argc[0], "clear", 5) == 0){
			handle_clear(argc, argv);
		}else if(strncmp(argc[0], "rmdir", 5) == 0){
			handle_rmdir(argc, argv);
		}else if(strncmp(argc[0], "rm", 2) == 0){
			handle_rm(argc, argv);
		}else{
			handle_spawn(argc, argv);
		}
	}
}

int main(int argc, char *argv[])
{
	debug_printf("size of commands %d\n", sizeof(commands)/sizeof(commands[0]));
	filesystem_init();
	fin = stdin;
	fout = stdout;
	wd = malloc(max_len_wd);
	input = malloc(buf_size);
	wd[0] = '/';
	wd[1] = '\0';
	pos_wd++;

	FILE *fp;
	fp = fopen("/file.txt", "w+");
	fprintf(fp, "%s %s\n", "Hello", "World");
	fprintf(fp, "%s %s\n", "Hello1", "World");
	fclose(fp);
	mkdir("/hello");
	fp = fopen("/hello/file1.txt", "w+");
	fprintf(fp, "%s %s\n", "Hello new", "World");
	fprintf(fp, "%s %s\n", "Hello1 new", "World");
	fclose(fp);

	errval_t err;
	err = aos_rpc_get_irq_cap(get_init_rpc(), &cap_irq);
	if (err_is_fail(err)) {
		USER_PANIC_ERR(err, "bash_get_irq_cap failed");
	}
	err = inthandler_setup_arm(terminal_read_handler, NULL, 106);
	DEBUG_ERR(err, "Setting up inthandler_setup_arm\n");
	printf("\033[2J");
	printf("Starting bash\n");
	printf("Hello from the bash\n");
	SHELL_PRINTF(fout, "$bash>");
	fflush(stdout);
	// domainid_t newpid;
	// aos_rpc_process_spawn(get_init_rpc(), "hello", 0, &newpid);
	// aos_rpc_process_spawn(get_init_rpc(), "byebye", 0, &newpid);
	// struct thread *t = thread_create((thread_func_t) pesudo_task, NULL);
	// struct thread *t1 = thread_create((thread_func_t) pesudo_task, NULL);
	// struct thread *t2 = thread_create((thread_func_t) pesudo_task, NULL);
	// thread_join(t, NULL);
	// thread_join(t1, NULL);
	// thread_join(t2, NULL);
	struct waitset *default_ws = get_default_waitset();
	while(true) {
		err = event_dispatch(default_ws);
		if (err_is_fail(err)) {
			DEBUG_ERR(err, "in event_dispatch");
			abort();
		}
	}
	return 0;
}