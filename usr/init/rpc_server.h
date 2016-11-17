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

struct client_state {
    struct lmp_chan lc;  // LMP channel.
    char* str_buf;       // buffer for send_string messages.
    size_t str_buf_idx;  // current index in string buffer.
    size_t ram;          // how much RAM this client's currently holding.

    char* spawn_buf;       // buffer for process spawn messages.
    size_t spawn_buf_idx;  // current index in process spawn buffer.

    struct EndPoint remote_ep;  // Used to identify clients.

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
struct client_state* identify_client(struct capref* cap);

/**
 * \bief General-purpose RPC server-side receive handler. This is the entry
 * point into request handling.
 */
errval_t recv_handler(void* arg);

/**
 * \brief Processes a new client handshake request.
 */
void* process_handshake_request(struct capref* request_cap);
/**
 * \brief Processes a client RAM request.
 */
void* process_memory_request(struct lmp_recv_msg* msg,
        struct capref* request_cap);
/**
 * \brief Processes a send number request, potentially for another core.
 */
void* process_number_request(struct lmp_recv_msg* msg,
        struct capref* request_cap, bool remote);
/**
 * \brief Processes a terminal driver putchar request.
 */
void* process_putchar_request(struct lmp_recv_msg* msg,
        struct capref* request_cap);
/**
 * \brief Processes a terminal driver getchar request.
 */
void* process_getchar_request(struct lmp_recv_msg* msg,
        struct capref* request_cap);
/**
 * \brief Processes a send string request.
 */
void* process_string_request(struct lmp_recv_msg* msg,
        struct capref* request_cap, bool remote);
/**
 * \brief Processes a spawn new process request, potentially on another core.
 */
void* process_spawn_request(struct lmp_recv_msg* msg,
        struct capref* request_cap, bool remote);
/**
 * \brief Processes a get process name for PID request, potentially from another
 * core.
 */
void* process_get_process_name_request(struct lmp_recv_msg* msg,
        struct capref* request_cap, bool remote);
/**
 * \brief Processes a get processes list request, potentially from another core.
 */
void* process_get_process_list_request(struct lmp_recv_msg* msg,
        struct capref* request_cap, bool remote);

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
