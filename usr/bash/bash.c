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

// BIG TODO: CHANGE ALL FUNCTION DEFINITION TO ERRVAL_T
// Better error handling mechanisms

// Helper Functions:
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

void clean_buffer(void)
{
	free(input);
	pos = 0;
	inputRedirect = 0;
	outputRedirect = 0;
	buf_size = 16;
	input = malloc(buf_size);
}

static void terminal_read_handler(void *params)
{
	aos_rpc_serial_getchar(get_init_rpc(), (input+pos));
	if(*(input+pos) == '\r') {
		pos++;
		*(input+pos) = '\0';
		sanitize_input(input);
	}else {
		if(*(input+pos) == 127) {
			if(pos) {
				do_backslash();
				*(input+pos) = '\0';
				pos--;
			}else {
				SHELL_PRINTF("%c", '\a');
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
	}
}

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
	domainid_t *pids_core0;
	size_t pid_count_core0;
	domainid_t *pids_core1;
	size_t pid_count_core1;
	coreid_t core = 0;
	errval_t err; 
	err = aos_rpc_process_get_all_pids(get_init_rpc(),core, &pids_core0, 
										&pid_count_core0);
	if(err_is_fail(err)) {
		DEBUG_ERR(err, "Getting all pids");
	}
	char *name;
	printf("Core      Pids    Process Name\n");
	printf("================================\n");
	for(int i=0; i<pid_count_core0; i++) {
		aos_rpc_process_get_name(get_init_rpc(), *pids_core0, core, &name);
		printf("%d          %d       %s \n", core, *pids_core0, name);
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
		printf("%d          %d       %s \n", core, *pids_core1, name);
		pids_core1++;
	}
}

static void pesudo_task(void)
{
	// char *a = malloc(10);
	// a = "hello";
	// printf("%s\n", a);
	SHELL_PRINTF("###### Hello World from a new thread &&&&&&\n");
	//SHELL_PRINTF("My id is: %d\n", thread_get_id(thread_self()));
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

static void thread_handle_memtest(void *params)
{
	size_t *size = (size_t *)params;
	char *a =  malloc(*size);
	for(int i=0; i<*size; i++) {
		*(a+i) = 'J';
	}
	printf("Result of memtest:\n");
	printf("%s\n", a);
}

void handle_memtest(char *argc[], int argv)
{
	size_t *size = malloc(sizeof(size_t));
	*size = atoi(argc[1]);
	struct thread *t = thread_create((thread_func_t) thread_handle_memtest, (void*)size);
	thread_join(t, NULL);
}

errval_t handle_light_led(char *argc[], int argv)
{
	uintptr_t status = 0;
	coreid_t core = 0;
	if(strncmp(argc[1], "on", 2) == 0) status = 1;
	else if(strncmp(argc[1], "off", 3) == 0) status = 2;
	errval_t err = aos_rpc_light_led(get_init_rpc(), status, core);
	return err;
}

void handle_pwd(char *argc[], int argv)
{
	printf("Current directory is: %s\n", wd);	
}

void handle_ls(char *argc[], int argv)
{
	fs_dirhandle_t ent;
	opendir(wd, &ent);
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
	fclose(fp);
}

void handle_wc(char *argc[], int argv)
{
	FILE *fp;
	fp = fopen(argc[1], "r");
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
			if (c == '\n' || c == '\r') {
				tot_lines++;
			}
		} else {
			tot_words += in_space;
			in_space = 0;
		}
	}
	if (last != '\n' || last == '\r') {
		/* count last line if not linefeed terminated */
		tot_lines++;
	}

	printf("Lines, Words, Characters\n");
	printf(" %3d %3d %3d\n", tot_lines, tot_words, tot_chars);
	fclose(fp);
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
				printf("%s:", name);
			printf("%s\n", buf);
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

void handle_grep(char *argc[], int argv)
{
	FILE *fp;
	fp = fopen(argc[2], "r");
	grep(argc[1], fp, NULL);
	fclose(fp);
}

errval_t handle_oncore(char *argc[], int argv)
{
	domainid_t newpid;
	coreid_t core = atoi(argc[1]);
	char *end_string = malloc(5);
	end_string = "END\0";
	argc[argv] = end_string;
	struct capref proc_info;
	size_t ret_size;
	void *ret;
	CHECK("Allocate some frame for proc_info",
		frame_alloc(&proc_info, BASE_PAGE_SIZE, &ret_size));
	
	CHECK("Map proc_info to vspace",
		paging_map_frame(get_current_paging_state(), &ret, 
		ret_size, proc_info, NULL, NULL));
	
	ret = (void **)argc;

	debug_printf("Slot allocated is : %d\n", proc_info.slot);
	CHECK("Trying to spawn with arguments",
		aos_rpc_process_spawn_args(get_init_rpc(), &proc_info, core, &newpid));

	return SYS_ERR_OK;
}

void handle_help(char *argc[], int argv)
{

}

void handle_clear(char *argc[], int argv)
{
	printf("\033[2J");
}

void sanitize_input(char *ip)
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
		}else if(strncmp(argc[j], "<<", 2) == 0) {
			inputRedirect = 2;
		}else if(strncmp(argc[j], ">", 1) == 0) {
			outputRedirect = 1;
		}else if(strncmp(argc[j], ">>", 1) == 0) {
			outputRedirect = 2;
		}
	}
	execute_command(argc, argv);
	clean_buffer();
	printf("\n");
	SHELL_PRINTF("$bash>");
	fflush(stdout);
}

void execute_command(char *argc[], int argv)
{
	/* List of commands
		- [OK] echo
		- [OK] led
		- [OK] threads
		- [OK] memtest
		- oncore
		- [OK] ps
		- help
		- [OK] pwd
		- [OK] cd
		- [OK] ls
		- [OK] cat
		- [Ok] wc
		- [OK] grep
		- [OK] mkdir
	*/
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
	}
}

// void get_command(void)
// {
// 	int buf_size = 16;
// 	char *input = malloc(buf_size);
// 	char *input_start = input;
// 	int pos = 0;
// 	aos_rpc_serial_getchar(get_init_rpc() , input);
// 	if(*input == '\r') return;
// 	while(*input != '\r') {
// 		if(*input == 127) {
// 			if (pos){
// 				do_backslash();
// 				input--;
// 				pos--;
// 			}
// 			else
// 				aos_rpc_serial_putchar(get_init_rpc(), '\a');
// 		}else {
// 			aos_rpc_serial_putchar(get_init_rpc(), *input);
// 			input++;
// 			pos++;
// 		}
// 		if(pos == buf_size){
// 			assert(&input_start[pos] == input);
// 			input_start = realloc(input_start, 2*buf_size);
// 			input = &input_start[pos];
// 			buf_size = 2*buf_size;
// 		}
// 		aos_rpc_serial_getchar(get_init_rpc() , input);
// 	}
// 	input_start[pos] = '\0';
// 	sanitize_input(input_start);
// }

int main(int argc, char *argv[])
{
	// TODO: Fix this interrupt handler
	filesystem_init();
	wd = malloc(max_len_wd);
	input = malloc(buf_size);
	wd[0] = '/';
	wd[1] = '\0';
	pos_wd++;

	FILE *fp;
	fp = fopen("file.txt", "w+");
	fprintf(fp, "%s %s", "Hello", "World");
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
	SHELL_PRINTF("$bash>");
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
	// while(1) {
	// 	SHELL_PRINTF("$bash>");
	// 	get_command();
	// 	aos_rpc_serial_putchar(get_init_rpc(), '\n');
	// }
	
	return 0;
}