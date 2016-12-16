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
#define LED_BIT 8

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

struct device_cap_node {
    lpaddr_t base;
    size_t bytes;
    struct capref cap;

    struct device_cap_node* prev;
    struct device_cap_node* next;
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

void add_process_ps_list(char *name);

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

static inline void rpc_light_led(uint32_t status)
{
    // Forge frame for the led memory segment
    struct capref led_cap;
    errval_t err;
    void *ret;
    //size_t retsize = 8; 
    genpaddr_t start = (genpaddr_t) 0x4A310000;

    // 1. Allocate some slot for led_cap
    err = slot_alloc(&led_cap);
    //DEBUG_ERR(err, "allocating slot for led_cap");

    // 2. Forge frame from kernel
    err = frame_forge(led_cap, start, BASE_PAGE_SIZE, 0);
    //DEBUG_ERR(err, "forging out_enab");

    struct frame_identity ret1;
    err = frame_identify(led_cap, &ret1);
    //DEBUG_ERR(err, "identify frame for led_cap");

    //debug_printf("Slot of new cap is: %d base: %ld size: %ld\n", led_cap.slot, ret1.base, ret1.bytes);
    // 3. Map led_cap frame to our pagetable
    err = paging_map_frame(get_current_paging_state(), &ret, BASE_PAGE_SIZE, led_cap,
            NULL, NULL);
    //DEBUG_ERR(err, "ERROR in mapping page for led_cap");
    
    volatile int * out_enab = (int *)(ret + 0x134);
    volatile int * dataout  = (int *)(ret + 0x13C);
    int mask = 1 << LED_BIT;
    *out_enab &= ~(mask);
    if(status == 1) {
        *dataout = mask;
    }
    else if(status == 2) {
        *dataout = 0;
    }
}

/**
 * \brief Returns a character from terminal input.
 */
static inline char rpc_getchar(void)
{
    char c;
    sys_getchar(&c);
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
 * \brief Returns the DevFrame cap using given base and bytes size.
 */
errval_t rpc_device_cap(lpaddr_t base, size_t bytes, struct capref* retcap);
/**
 * \brief Returns the interrupt (IRQ) capability.
 */
static inline errval_t rpc_irq_cap(struct capref* retcap)
{
    CHECK("rpc_irq_cap: allocating slot for retcap", slot_alloc(retcap));
    return cap_copy(*retcap, cap_irq);
}
/**
 * \brief Spawns a new process, returning its PID.
 */
errval_t rpc_spawn(char* name, domainid_t* pid);
/**
 * \brief Spawns a new process, returning its PID.
 */
errval_t rpc_spawn_args(char* name , domainid_t* pid);
/**
 * \brief Returns the name of the process with the given PID.
 */
char* rpc_process_name(domainid_t pid, size_t* len);
/**
 * \brief Returns the list of active processes on the current core.
 */
domainid_t* rpc_process_list(size_t* len);
/**
 * \brief Returns the DevFrame cap using given base and bytes size.
 */
/**
 * \brief Returns the SDMA driver's endpoint capability.
 */
static inline errval_t rpc_sdma_ep_cap(struct capref* retcap)
{
    CHECK("rpc_sdma_ep_cap: allocating slot for retcap", slot_alloc(retcap));
    return cap_copy(*retcap, cap_sdma_ep);
}

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
 * \brief Processes a terminal driver getchar request.
 */
void* process_local_light_led_request(struct lmp_recv_msg* msg,
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
 * \brief Processes a same-core spawn new process request.
 */
void* process_local_spawn_args_request(struct lmp_recv_msg* msg,
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
 * \brief Processes a same-core get device cap request.
 */
void* process_local_device_cap_request(struct lmp_recv_msg* msg,
        struct capref* request_cap, struct client_state* clients);
/**
 * \brief Processes a same-core get IRQ cap request.
 */
void* process_local_irq_cap_request(struct lmp_recv_msg* msg,
        struct capref* request_cap, struct client_state* clients);
/**
 * \brief Processes a same-core get SDMA driver endpoint cap request.
 */
void* process_local_sdma_ep_cap_request(struct lmp_recv_msg* msg,
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
/**
 * \brief Device cap response handler.
 */
errval_t send_device_cap(void* args);
/**
 * \brief Send cap response handler.
 */
errval_t send_cap(void* args);

#endif /* _INIT_RPC_SERVER_H_ */
