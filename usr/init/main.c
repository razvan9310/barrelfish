/**
 * \file
 * \brief init process for child spawning
 */

/*
 * Copyright (c) 2007, 2008, 2009, 2010, 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/waitset.h>
#include <aos/morecore.h>
#include <aos/paging.h>

#include <mm/mm.h>
#include "mem_alloc.h"
#include <spawn/spawn.h>

coreid_t my_core_id;
struct bootinfo *bi;

#define MAX_CLIENT_RAM 64 * 1024 * 1024

struct client_state {
    struct lmp_chan lc;  // LMP channel.
    char* str_buf;       // buffer for send_string messages.
    size_t str_buf_idx;  // current index in string buffer.
    size_t ram;          // how much RAM this client's currently holding.

    struct EndPoint remote_ep;  // Used to identify clients.

    // Doubly-linked list.
    struct client_state* next;
    struct client_state* prev;
} *clients = NULL;

struct client_state* identify_client(struct capref* cap);

errval_t recv_handler(void* arg);

void* process_handshake_request(struct capref* remote_cap);
void* process_memory_request(struct lmp_recv_msg* msg,
        struct capref* remote_cap);
void* process_number_request(struct lmp_recv_msg* msg,
        struct capref* remote_cap);
void* process_putchar_request(struct lmp_recv_msg* msg,
        struct capref* remote_cap);
void* process_string_request(struct lmp_recv_msg* msg,
        struct capref* remote_cap);

errval_t send_handshake(void* args);
errval_t send_memory(void* args);
errval_t send_simple_ok(void* args);

struct client_state* identify_client(struct capref* cap)
{
    struct capability ret;
    errval_t err = debug_cap_identify(*cap, &ret);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "identifying client cap");
        return NULL;
    }

    struct client_state* client = clients;
    while (client != NULL) {
        if (client->remote_ep.listener == ret.u.endpoint.listener
                && client->remote_ep.epoffset == ret.u.endpoint.epoffset) {
            break;
        }
        client = client->next;
    }
    
    return client;
}

void* process_handshake_request(struct capref* remote_cap)
{
    struct client_state* existing = identify_client(remote_cap);
    if (existing != NULL) {
        // No point opening new channel.
        return (void*) &existing->lc;
    }

    // Create state for newly connecting client.
    struct client_state* new_client = (struct client_state*) malloc(
            sizeof(struct client_state));
    if (clients == NULL) {
        new_client->next = new_client->prev = NULL;
    } else {
        clients->prev = new_client;
        new_client->next = clients;
        new_client->prev = NULL;
    }
    clients = new_client;

    // Initialize client state.
    new_client->ram = 0;
    new_client->str_buf = NULL;
    new_client->str_buf_idx = 0;

    // Endpoint for further reference.
    struct capability ret;
    debug_cap_identify(*remote_cap, &ret);
    new_client->remote_ep = ret.u.endpoint;

    // New channel.
    lmp_chan_accept(&new_client->lc, DEFAULT_LMP_BUF_WORDS, *remote_cap);

    // Return response args.
    return (void*) &new_client->lc;
}

/**
 * \brief Process a memory request by allocating a frame for the client and
 * returning a cap for it.
 * The client specifies how much memory it wants in msg->words[2].
 * Namely, message format is (request_id_ram, client_id, size_requested).
 * The requested size is rounded to BASE_PAGE_SIZE and limited to 64 MB.
 */
void* process_memory_request(struct lmp_recv_msg* msg,
        struct capref* remote_cap)
{
    struct client_state* client = identify_client(remote_cap);
    if (client == NULL) {
        debug_printf("ERROR: process_memory_request: could not identify client\n");
        return NULL;
    }

    size_t req_size = (size_t) msg->words[1];
    req_size = ROUND_UP(req_size, BASE_PAGE_SIZE);
    if (req_size == 0) {
        // Waht?!
        req_size = BASE_PAGE_SIZE;
    }
    if (req_size + client->ram >= MAX_CLIENT_RAM) {
        // Limit to 64 MB.
        req_size = MAX_CLIENT_RAM - client->ram;
    }

    // Allocate RAM.
    struct capref mem_cap;
    errval_t err = ram_alloc(&mem_cap, req_size);//frame_alloc(mem_cap, req_size, &ret_size);
    client->ram += req_size;

    // Response args.
    size_t args_size = ROUND_UP(sizeof(struct lmp_chan), 4)
            + ROUND_UP(sizeof(errval_t), 4)
            + ROUND_UP(sizeof(struct capref), 4)
            + sizeof(size_t);
    void* args = malloc(args_size);
    void *return_args = args;
    
    // 1. Channel to send down.
    *((struct lmp_chan*) args) = client->lc;
    
    // 2. Error code fromm ram_alloc.
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(struct lmp_chan), 4);
    *((errval_t*) args) = err;

    // 3. Cap for newly allocated RAM.
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(errval_t), 4);
    *((struct capref*) args) = mem_cap;

    // 4. Size returned by ram_alloc.
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(struct capref), 4);
    *((size_t*) args) = req_size;

    return return_args;
}

void* process_number_request(struct lmp_recv_msg* msg,
        struct capref* remote_cap)
{
    // Print what we got.
    debug_printf("Server received number %u\n", (uint32_t) msg->words[1]);

    // Identify client.
    struct client_state* client = identify_client(remote_cap);
    if (client == NULL) {
        debug_printf("ERROR: process_number_request: could not idetify client");
        return NULL;
    }

    // Return channel for response handler.
    return (void*) &client->lc;
}

void* process_putchar_request(struct lmp_recv_msg* msg,
        struct capref* remote_cap)
{
    // Put character.
    sys_print((char*) &msg->words[1], 1);

    // Identify client.
    struct client_state* client = identify_client(remote_cap);
    if (client == NULL) {
        debug_printf("ERROR: process_putchar_request: could not idetify client");
        return NULL;
    }

    // Return chanel for response handler.
    return (void*) &client->lc;
}

void* process_string_request(struct lmp_recv_msg* msg,
        struct capref* remote_cap)
{
    uint32_t remaining = msg->words[1];

    struct client_state* client = clients;
    while (client != NULL) {
        struct capability ret;
        debug_cap_identify(*remote_cap, &ret);
        if (client->remote_ep.listener == ret.u.endpoint.listener
                && client->remote_ep.epoffset == ret.u.endpoint.epoffset) {
            break;
        }
        client = client->next;
    }
    if (client == NULL) {
        debug_printf("main.c: process_string_request: could not find client\n");
        return NULL;
    }

    if (client->str_buf == NULL) {
        client->str_buf = (char*) malloc(remaining * sizeof(char));
        client->str_buf_idx = 0;
    }

    size_t stop = remaining < 28 ? remaining : 28;
    for (size_t i = 0; i < stop; ++i) {
        uint32_t word = msg->words[2 + i / 4];
        client->str_buf[client->str_buf_idx++] =
                (char) (word >> (8 * (i % 4)));
    }

    remaining -= stop;
    if (remaining == 0) {
        // Print and reset.
        sys_print(client->str_buf, client->str_buf_idx);
        client->str_buf_idx = 0;
        free(clients->str_buf);
        client->str_buf = NULL;
    }

    // Return response args.
    return (void*) &client->lc;
}

errval_t recv_handler(void* arg)
{
    struct lmp_chan* lc = (struct lmp_chan*) arg;
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;
    // printf("msg buflen %d\n", msg.buf.buflen);
    struct capref cap;
    errval_t err = lmp_chan_recv(lc, &msg, &cap);

    // Reregister.
    CHECK("Create Slot", lmp_chan_alloc_recv_slot(lc));
    lmp_chan_register_recv(lc, get_default_waitset(),
            MKCLOSURE((void *)recv_handler, arg));

    if (msg.buf.msglen > 0) {
        void* response;
        void* response_args;
        switch (msg.words[0]) {
            case AOS_RPC_HANDSHAKE:
                response = (void*) send_handshake;
                response_args = process_handshake_request(&cap);
                break;
            case AOS_RPC_MEMORY:
                response = (void*) send_memory;
                response_args = process_memory_request(&msg, &cap);
                break;
            case AOS_RPC_NUMBER:
                response = (void*) send_simple_ok;
                response_args = process_number_request(&msg, &cap);
                break;
            case AOS_RPC_PUTCHAR:
                response = (void*) send_simple_ok;
                response_args = process_putchar_request(&msg, &cap);
                break;
            case AOS_RPC_STRING:
                response = (void*) send_simple_ok;
                response_args = process_string_request(&msg, &cap);
                break;
            default:
                return 1;  // TODO: More meaning plz
        }

        struct lmp_chan* out = (struct lmp_chan*) response_args;
        CHECK("lmp_chan_register_send parent",
                lmp_chan_register_send(out, get_default_waitset(),
                        MKCLOSURE(response, response_args)));  
    }

    return err;
}

errval_t send_handshake(void* args)
{
    // 1. Get channel to send down.
    struct lmp_chan* lc = (struct lmp_chan*) args;

    // 2. Send response.
    CHECK("lmp_chan_send handshake",
            lmp_chan_send1(lc, LMP_FLAG_SYNC, NULL_CAP, AOS_RPC_OK));

    return SYS_ERR_OK;
}

errval_t send_memory(void* args)
{
    // 1. Get channel to send down.
    struct lmp_chan* lc = (struct lmp_chan*) args;

    // 2. Get error code.
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(struct lmp_chan), 4);
    errval_t* err = (errval_t*) args;

    // 3. Get cap for memory.
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(errval_t), 4);
    struct capref* retcap = (struct capref*) args;

    // 4. Get returned size.
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(struct capref), 4);
    size_t* size = (size_t*) args;

    // 5. Generate response code.
    size_t code = err_is_fail(*err) ? AOS_RPC_FAILED : AOS_RPC_OK;

    // 6. Send response.
    CHECK("lmp_chan_send memory",
            lmp_chan_send3(lc, LMP_FLAG_SYNC, *retcap, code, (uintptr_t) *err,
                    *size));

    // 7 Free args.
    free(args);

    return SYS_ERR_OK;
}

errval_t send_simple_ok(void* args)
{
    // 1. Get channel to send down.
    struct lmp_chan* lc = (struct lmp_chan*) args;

    // 2. Send response.
    CHECK("lmp_chan_send putchar",
            lmp_chan_send1(lc, LMP_FLAG_SYNC, NULL_CAP, AOS_RPC_OK));

    return SYS_ERR_OK;
}

int main(int argc, char *argv[])
{
    errval_t err;

    /* Set the core id in the disp_priv struct */
    err = invoke_kernel_get_core_id(cap_kernel, &my_core_id);
    assert(err_is_ok(err));
    disp_set_core_id(my_core_id);

    debug_printf("init: on core %" PRIuCOREID " invoked as:", my_core_id);
    for (int i = 0; i < argc; i++) {
       printf(" %s", argv[i]);
    }
    printf("\n");

    /* First argument contains the bootinfo location, if it's not set */
    bi = (struct bootinfo*)strtol(argv[1], NULL, 10);
    if (!bi) {
        assert(my_core_id > 0);
    }

    /* Initialize paging. */
    // err = paging_init();
    // if (err_is_fail(err)) {
    //     DEBUG_ERR(err, "paging_init");
    // }

    /* Initialize the default slot allocator. */
    // err = slot_alloc_init();
    // if (err_is_fail(err)) {
    //     DEBUG_ERR(err, "slot_alloc_init");
    // }

    err = initialize_ram_alloc();
    if(err_is_fail(err)){
        DEBUG_ERR(err, "initialize_ram_alloc");
    }

    CHECK("Retype selfep from dispatcher", cap_retype(cap_selfep, cap_dispatcher, 0, ObjType_EndPoint, 0, 1));

    struct lmp_chan* lc = (struct lmp_chan*) malloc(sizeof(struct lmp_chan));
    CHECK("Create channel for parent", lmp_chan_accept(lc, DEFAULT_LMP_BUF_WORDS, NULL_CAP));

    CHECK("Create Slot", lmp_chan_alloc_recv_slot(lc));
    CHECK("COpy to initep", cap_copy(cap_initep, lc->local_cap));

    CHECK("lmp_chan_register_recv child",
            lmp_chan_register_recv(lc, get_default_waitset(),
                    MKCLOSURE((void*) recv_handler, lc)));

    // // ALLOCATE A LOT OF MEMORY TROLOLOLOLO.
    // struct capref frame;
    // size_t retsize;
    // err = frame_alloc(&frame, 900 * 1024 * 1024, &retsize);
    // if (err_is_fail(err)) {
    //     DEBUG_ERR(err, "PANIC FRAME ALLOC 64 MB");
    // } else {
    //     debug_printf("main.c: was given frame size %u\n", retsize);
    // }
    // void* buf;
    // err = paging_map_frame_attr(get_current_paging_state(),
    //     &buf, retsize, frame,
    //     VREGION_FLAGS_READ_WRITE, NULL, NULL);
    // if (err_is_fail(err)) {
    //     DEBUG_ERR(err, "PANIC MAPPING 64 MB FRAME");
    // }

    // debug_printf("main.c: testing memory @ %p\n", buf);
    // char* cbuf = (char*)buf;
    // *cbuf = 'J';
    // sys_debug_flush_cache();
    // printf("%c\n", *cbuf);

    // cbuf += 225 * 1024 * 1024;
    // *cbuf = 'K';
    // sys_debug_flush_cache();
    // printf("%c\n", *cbuf);

    // cbuf += 225 * 1024 * 1024;
    // *cbuf = 'L';
    // sys_debug_flush_cache();
    // printf("%c\n", *cbuf);

    // cbuf += 225 * 1024 * 1024;
    // *cbuf = 'M';
    // sys_debug_flush_cache();
    // printf("%c\n", *cbuf);

    // char *a = malloc(sizeof(char));
    // *a = 'a';
    // realloc(a, 2*sizeof(char));
    // printf("Value of char after resize %c\n", *a);
    // *(a+1) = 'b';
    // printf("Value of char after resize %c\n", *(a+1));
    // spawn a few helloz
    spawn_load_by_name("hello", (struct spawninfo*) malloc(sizeof(struct spawninfo)));
    spawn_load_by_name("byebye", (struct spawninfo*) malloc(sizeof(struct spawninfo)));
    spawn_load_by_name("memeater", (struct spawninfo*) malloc(sizeof(struct spawninfo)));

    debug_printf("Message handler loop\n");
    // Hang around
    struct waitset *default_ws = get_default_waitset();
    while (true) {
        err = event_dispatch(default_ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }

    return EXIT_SUCCESS;
}
