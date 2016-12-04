/**
 * \file
 * \brief Barrelfish bash implementation.
 */

/*
 * Copyright (c) 2007, 2008, ETH Zurich.
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

char *wd;
uint64_t max_len_wd = 20;
uint64_t pos_wd = 0;

void handle_echo(char *argc[], int argv)
{
	// TODO: Handle ("")
	for(int i=1; i<argv; i++) {
		printf("%s ", argc[i]);
	}
	printf("\n");
}

void handle_ps(char *argc[], int argv)
{
	domainid_t *pids;
	size_t pid_count;
	coreid_t core = 0;
	errval_t err; 
	err = aos_rpc_process_get_all_pids(get_init_rpc(),core, &pids, &pid_count);
	if(err_is_fail(err)) {
		DEBUG_ERR(err, "Getting all pids");
	}
	char *name;
	printf("Pids    Process Name\n");
	printf("====================\n");
	for(int i=0; i<pid_count; i++) {
		aos_rpc_process_get_name(get_init_rpc(), *pids, core, &name);
		printf("%d       %s \n", *pids, name);
		pids++;
	}
}

static void pesudo_task(void)
{
	//SHELL_PRINTF("Hello World");
}

void handle_threads(char *argc[], int argv)
{
	int num_threads = atoi(argc[1]);
	struct thread *t[num_threads];
	for(int i=0; i<num_threads; i++) {
		t[i] = thread_create((thread_func_t) pesudo_task, NULL);
	}

	for(int i=0; i<num_threads; i++) {
		thread_join(t[i], NULL);
	}
}

void handle_memtest(char *argc[], int argv)
{
	struct capref cap;
	size_t size = atoi(argc[1]);
	size_t retsize;
	aos_rpc_get_ram_cap(get_init_rpc(), size, &cap, &retsize);
}

void handle_light_led(char *argc[], int argv)
{
	int status = -1;
	if(strncmp(argc[1], "on", 2) == 0) status = 1;
	else if(strncmp(argc[1], "off", 3) == 0) status = 0;
	aos_rpc_light_led(get_init_rpc(), status);
}

void handle_pwd(char *argc[], int argv)
{
	printf("Current directory is: %s\n", wd);	
}

void handle_ls(char *argc[], int argv)
{
	fs_dirhandle_t ent;
	opendir(wd, &ent);
	SHELL_PRINTF("After opendir\n");
	char *name;
	while((readdir(ent, &name)) == SYS_ERR_OK) {
		while(*name) {
			printf("%c", *name);
			name++;
		}
		printf("\n");
	}
}

void handle_mkdir(char *argc[], int argv)
{
	mkdir(argc[1]);
}

void handle_cd(char *argc[], int argv)
{
	if(strlen(wd) == (max_len_wd-1)) {
		//max_len_wd = max(2*max_len_wd, max_len_wd)
		wd = realloc(wd, 2*max_len_wd);
		max_len_wd = 2*max_len_wd;
	}
	for(int i=0; i < strlen(argc[1]); i++) {
		wd[pos_wd] = argc[1][i];
		pos_wd++;
	}
	if(wd[pos_wd] != '/') {
		wd[pos_wd] = '/';
		pos_wd++;
	}
	pos_wd++;
	//strcat(wd, argc[1]);
}

void handle_cat(char *argc[], int argv)
{
	FILE *fp;
	fp = fopen(argc[1], "r");
	fseek(fp, 0, SEEK_END);
	uint64_t size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char *string = malloc(size+1);
	fread(string, size, 1, fp);
	fclose(fp);
	string[size] = 0;
	printf("%s\n", string);
}

void handle_wc(char *argc[], int argv)
{

}

void handle_grep(char *argc[], int argv)
{

}

void handle_oncore(char *argc[], int argv)
{

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
	return false;
}

void sanitize_input(char *input)
{
	int argc_size = 5;
	int argv = 0;
	char **argc;
	argc = malloc(argc_size*sizeof(char*));
	int i = 0;
	while(is_space(input[i])) i++;
	while(input[i]) {
		argc[argv] = &input[i];
		argv++;
		if(argv == argc_size) {
			argc = realloc(argc, 2*argc_size*sizeof(char*));
			argc_size = 2*argc_size;
		}
		while(!is_space(input[i]) && input[i]) i++;
		while(is_space(input[i]) && input[i]) i++;
		if(input[i])
			input[i-1] = '\0';
	}
	aos_rpc_serial_putchar(get_init_rpc(), '\n');
	execute_command(argc, argv);
}

void execute_command(char *argc[], int argv)
{
	/* List of commands
		- [OK] echo
		- [OK] led
		- threads
		- [OK] memtest
		- oncore
		- [OK] ps
		- help
		- [OK] pwd
		- [OK] cd
		- [OK] ls
		- [OK] cat
		- wc
		- grep
	*/
	if(strncmp(argc[0], "echo", 4) == 0) {
		handle_echo(argc, argv);
	}else if(strcmp(argc[0], "led") == 0){
		handle_light_led(argc, argv);
	}else if(strcmp(argc[0], "threads") == 0){
		handle_threads(argc, argv);
	}else if(strcmp(argc[0], "memtest") == 0){
		handle_memtest(argc, argv);
	}else if(strcmp(argc[0], "oncore") == 0){

	}else if(strcmp(argc[0], "ps") == 0){
		handle_ps(argc, argv);
	}else if(strcmp(argc[0], "help") == 0){

	}else if(strcmp(argc[0], "pwd") == 0){
		handle_pwd(argc, argv);
	}else if(strcmp(argc[0], "cd") == 0){
		handle_cd(argc, argv);
	}else if(strcmp(argc[0], "ls") == 0){
		handle_ls(argc, argv);
	}else if(strcmp(argc[0], "cat") == 0){
		handle_cat(argc, argv);
	}else if(strcmp(argc[0], "wc") == 0){
		handle_wc(argc, argv);
	}else if(strcmp(argc[0], "grep") == 0){
		handle_grep(argc, argv);
	}else if(strcmp(argc[0], "mkdir") == 0){
		handle_mkdir(argc, argv);
	}
}

void get_command(void)
{
	int buf_size = 16;
	char *input = malloc(buf_size);
	char *input_start = input;
	int pos = 0;
	aos_rpc_serial_getchar(get_init_rpc() , input);
	if(*input == '\r') return;
	while(*input != '\r') {
		if(*input == 127) {
			if (pos){
				do_backslash();
				input--;
				pos--;
			}
			else
				aos_rpc_serial_putchar(get_init_rpc(), '\a');
		}else {
			aos_rpc_serial_putchar(get_init_rpc(), *input);
			input++;
			pos++;
		}
		if(pos == buf_size){
			assert(&input_start[pos] == input);
			input_start = realloc(input_start, 2*buf_size);
			input = &input_start[pos];
			buf_size = 2*buf_size;
		}
		aos_rpc_serial_getchar(get_init_rpc() , input);
	}
	input_start[pos] = '\0';
	sanitize_input(input_start);
}

int main(int argc, char *argv[])
{
	printf("Starting bash\n");
	//printf("Hello\n");
	domainid_t newpid;
	aos_rpc_process_spawn(get_init_rpc(), "hello", 0, &newpid);
	aos_rpc_process_spawn(get_init_rpc(), "byebye", 0, &newpid);
	filesystem_init();
	wd = malloc(20);
	wd[0] = '/';
	wd[1] = '\0';
	pos_wd++;
	FILE *fp;
	fp = fopen("file.txt", "w+");
	fprintf(fp, "%s %s", "Hello", "World");
	fclose(fp);

	while(1) {
		SHELL_PRINTF("$bash>");
		get_command();
		aos_rpc_serial_putchar(get_init_rpc(), '\n');
	}
	return 0;
}