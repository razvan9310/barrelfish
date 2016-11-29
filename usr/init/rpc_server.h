/**
 * \file
 * \brief Server-side RPC functionality for IPC.
 */

/*
 * Copyright (c) 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef _INIT_RPC_SERVER_H_
#define _INIT_RPC_SERVER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <aos/aos_rpc.h>
#include <spawn/spawn.h>

#define MAX_CLIENT_RAM 128 * 1024 * 1024

// Tracks frames for inter-core communication channel endpoints.
struct ic_frame_node {
    struct capref frame;
    bool free;
    void* addr;

    struct ic_frame_node* next;
    struct ic_frame_node* prev;
};

struct client_state {
    struct lmp_chan lc;  // LMP channel.
    char* str_buf;       // buffer for send_string messages.
    size_t str_buf_idx;  // current index in string buffer.
    size_t ram;          // how much RAM this client's currently holding.

    char* spawn_buf;       // buffer for process spawn messages.
    size_t spawn_buf_idx;  // current index in process spawn buffer.

    struct EndPoint remote_ep;  // Used to identify clients.

    // The two "channel endpoints" for inter-core communication.
    struct ic_frame_node* client_frame;
    struct ic_frame_node* server_frame;

    // Whether this client is currently being bound to a remote server.
    bool binding_in_progress;

    // Doubly-linked list.
    struct client_state* next;
    struct client_state* prev;
};

struct system_ps {
    struct spawninfo process;
    domainid_t pid;
    char* name;
    int curr_size;
    // Doubly-linked list.
    struct system_ps* next;
    struct system_ps* prev;
};

/**
 * \brief Returns the client_state instance corresponding to the given client
 * capref.
 */
struct client_state* identify_client(struct capref* cap,
        struct client_state* clients);

/**
 * \brief Serve a request locally (on the current core).
 */
errval_t serve_locally(struct lmp_recv_msg* msg, struct capref* client_cap,
        struct client_state** clients);

/**
 * \brief Allocates RAM in the given cap, returning the allocated size.
 */
errval_t rpc_ram_alloc(struct capref* retcap, size_t size, size_t* retsize);
/**
 * \brief Prints a character to terminal output.
 */
static inline void rpc_putchar(char* c)
{
    sys_print(c, 1);
}
/**
 * \brief Returns a character from terminal input.
 */
static inline char rpc_getchar(void)
{
    char c;
    while (true) {
        sys_getchar(&c);
        if (c != '\0') {
            break;
        }
    }
    return c;
}
/**
 * \brief Prints a string to terminal output.
 */
static inline void rpc_string(char* string, size_t len)
{
    sys_print(string, len);
}
/**
 * \brief Spawns a new process, returning its PID.
 */
errval_t rpc_spawn(char* name, domainid_t* pid);
/**
 * \brief Returns the name of the process with the given PID.
 */
char* rpc_process_name(domainid_t pid, size_t* len);
/**
 * \brief Returns the list of active processes on the current core.
 */
domainid_t* rpc_process_list(size_t* len);

/**
 * \bief General-purpose local RPC server-side receive handler. This is the
 * entry point into local (same-core) request handling.
 */
errval_t local_recv_handler(void* arg);

/**
 * \brief Processes a new client same-core handshake request.
 */
void* process_local_handshake_request(struct capref* request_cap,
        struct client_state** clients);
/**
 * \brief Processes a client RAM request.
 */
void* process_local_memory_request(struct lmp_recv_msg* msg,
        struct capref* request_cap, struct client_state* clients);
/**
 * \brief Processes a same-core send number request, potentially for another core.
 */
void* process_local_number_request(struct lmp_recv_msg* msg,
        struct capref* request_cap, struct client_state* clients);
/**
 * \brief Processes a terminal driver putchar request.
 */
void* process_local_putchar_request(struct lmp_recv_msg* msg,
        struct capref* request_cap, struct client_state* clients);
/**
 * \brief Processes a terminal driver getchar request.
 */
void* process_local_getchar_request(struct lmp_recv_msg* msg,
        struct capref* request_cap, struct client_state* clients);
/**
 * \brief Processes a send string request.
 */
void* process_local_string_request(struct lmp_recv_msg* msg,
        struct capref* request_cap, struct client_state* clients);
/**
 * \brief Processes a same-core spawn new process request.
 */
void* process_local_spawn_request(struct lmp_recv_msg* msg,
        struct capref* request_cap, struct client_state* clients);
/**
 * \brief Processes a same-core get process name for PID request.
 */
void* process_local_get_process_name_request(struct lmp_recv_msg* msg,
        struct capref* request_cap, struct client_state* clients);
/**
 * \brief Processes a same-core get processes list request.
 */
void* process_local_get_process_list_request(struct lmp_recv_msg* msg,
        struct capref* request_cap, struct client_state* clients);

/**
 * \brief Handshake response handler.
 */
errval_t send_handshake(void* args);
/**
 * \brief General-purpose simple OK response handler, that just acks back to the
 * client.
 */
errval_t send_simple_ok(void* args);
/**
 * \brief RAM response handler.
 */
errval_t send_memory(void* args);
/**
 * \brief Terminal driver getchar response handler.
 */
errval_t send_serial_getchar(void* args);
/**
 * \brief Spawn process response handler, returning the PID for the newly
 * spawned process.
 */
errval_t send_pid(void* args);
/**
 * \brief Process name for PID response handler.
 */
errval_t send_process_name(void* args);
/**
 * \brief Process list response handler.
 */
errval_t send_ps_list(void* args);

#endif /* _INIT_RPC_SERVER_H_ */
