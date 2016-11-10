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
#include <spawn/spawn.h>

#include "coreboot.h"
#include "mem_alloc.h"

coreid_t my_core_id;
struct bootinfo *bi;

static domainid_t last_issued_pid = 1;

#define MAX_CLIENT_RAM 64 * 1024 * 1024

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
} *clients = NULL;

struct system_ps {
    struct spawninfo process;
    domainid_t pid;
    char* name;
    int curr_size;
    // Doubly-linked list.
    struct system_ps* next;
    struct system_ps* prev;
} *ps = NULL;


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
void* process_spawn_request(struct lmp_recv_msg* msg,
        struct capref* remote_cap);
void* process_get_process_name_request(struct lmp_recv_msg* msg,
        struct capref* remote_cap);
void* process_get_process_list_request(struct lmp_recv_msg* msg,
        struct capref* remote_cap);
void* process_getchar_request(struct lmp_recv_msg* msg,
        struct capref* remote_cap);

errval_t send_handshake(void* args);
errval_t send_memory(void* args);
errval_t send_serial_getchar(void* args);
errval_t send_simple_ok(void* args);
errval_t send_pid(void* args);
errval_t send_process_name(void* args);
errval_t send_ps_list(void* args);


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
 * \brief Process a spawn request by allocating a frame for the client and
 * returning a cap for it.
 * The client the binary to spawn msg->words[2].
 * Namely, message format is (request_id_spawn, client_id?, binary_id).
 * The requested size is rounded to BASE_PAGE_SIZE and limited to 64 MB.
 */
 void* process_spawn_request(struct lmp_recv_msg* msg,
        struct capref* remote_cap)
 {
    uint32_t remaining = msg->words[2];

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

    if (client->spawn_buf == NULL) {
        client->spawn_buf = (char*) malloc(remaining * sizeof(char));
        client->spawn_buf_idx = 0;
    }

    size_t stop = remaining < 24 ? remaining : 24;
    for (size_t i = 0; i < stop; ++i) {
        uint32_t word = msg->words[3 + i / 4];
        client->spawn_buf[client->spawn_buf_idx++] =
                (char) (word >> (8 * (i % 4)));
    }

    remaining -= stop;
    if (remaining == 0) {
        // Print and reset.
        //sys_print(client->spawn_buf, client->spawn_buf_idx);
        //debug_printf("Trying to spawn %s\n", client->spawn_buf);
        
        if (strcmp(client->spawn_buf, "init") == 0) {
            size_t args_size = ROUND_UP(sizeof(struct lmp_chan), 4)
                + ROUND_UP(sizeof(errval_t), 4);

            void* args = malloc(args_size);
            void *return_args = args;
            
            // 1. Channel to send down.
            *((struct lmp_chan*) args) = client->lc;
            
            // 2. Error code fromm ram_alloc.
            args = (void*) ROUND_UP((uintptr_t) args + sizeof(struct lmp_chan), 4);
            *((errval_t*) args) = SPAWN_ERR_FIND_SPAWNDS;

            return return_args;
        }

        struct system_ps* new_ps = (struct system_ps*) malloc(sizeof(struct system_ps));
        if (ps == NULL) {
            new_ps->next = new_ps->prev = NULL;
            new_ps->curr_size = 1;
        } else {
            ps->prev = new_ps;
            new_ps->curr_size = ps->curr_size+1;
            new_ps->next = ps;
            new_ps->prev = NULL;
        }

        //Set process pid
        new_ps->pid = ++last_issued_pid;

        //Spawn process and fill spawinfo
        //struct spawninfo* process_info = (struct spawninfo*) malloc(sizeof(struct spawninfo));
        errval_t err = spawn_load_by_name(client->spawn_buf,
                (struct spawninfo*) malloc(sizeof(struct spawninfo)), my_core_id);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "slot_alloc_init");
        }
        //TODO: remeber to free spawninfo at some point
        
        //ps->process = process_info;
        new_ps->name = (char*) malloc(strlen(client->spawn_buf) * sizeof(char));
        strcpy(new_ps->name,  client->spawn_buf);
        //Add process to process list
        ps = new_ps;

        //debug_printf("Succesfully to spawn %s\n", client->spawn_buf);

        size_t args_size = ROUND_UP(sizeof(struct lmp_chan), 4)
            + ROUND_UP(sizeof(errval_t), 4)
            + ROUND_UP(sizeof(domainid_t), 4);

        void* args = malloc(args_size);
        void *return_args = args;
        
        // 1. Channel to send down.
        *((struct lmp_chan*) args) = client->lc;
        
        // 2. Error code fromm spawn process.
        args = (void*) ROUND_UP((uintptr_t) args + sizeof(struct lmp_chan), 4);
        *((errval_t*) args) = err;

        args = (void*) ROUND_UP((uintptr_t) args + sizeof(errval_t), 4);
        *((domainid_t*) args) = new_ps->pid;
        //debug_printf("%s process pid is %u\n", client->spawn_buf, *((domainid_t*) args));
        
        
        client->spawn_buf_idx = 0;
        free(clients->spawn_buf);
        client->spawn_buf = NULL;

        return return_args;

    } else {
        debug_printf("In else statement\n");
        size_t args_size = ROUND_UP(sizeof(struct lmp_chan), 4);

        void* args = malloc(args_size);
        void *return_args = args;
        
        // 1. Channel to send down.
        *((struct lmp_chan*) args) = client->lc;
        
        // 2. Error code fromm spawn process.
        /*args = (void*) ROUND_UP((uintptr_t) args + sizeof(struct lmp_chan), 4);
        *((errval_t*) args) = err;

        args = (void*) ROUND_UP((uintptr_t) args + sizeof(errval_t), 4);
        *((domainid_t*) args) = -1;*/

        return return_args;
    }

    // Return response args.
    //return (void*) &client->lc;
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

void* process_getchar_request(struct lmp_recv_msg* msg,
            struct capref* remote_cap)
{
    // Identify Client
    struct client_state* client = identify_client(remote_cap);
    if (client == NULL) {
        debug_printf("ERROR: process_putchar_request: could not idetify client\n");
        return NULL;
    }

    // Get character.
    while(1) {
        sys_getchar((char*) &msg->words[2]);
        if(msg->words[2]) break;
    }

    debug_printf("main.c: process_getchar_request value of input char %c\n", msg->words[2]);

    // Response args.
    size_t args_size = ROUND_UP(sizeof(struct lmp_chan), 4)
            + ROUND_UP(sizeof(char), 4);
    void* args = malloc(args_size);
    void *return_args = args;
    
    // 1. Channel to send down.
    *((struct lmp_chan*) args) = client->lc;

    // 2. Character returned by sys_getchar
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(char), 4);
    *((char*) args) = msg->words[2];

    return return_args;
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

void* process_get_process_name_request(struct lmp_recv_msg* msg,
        struct capref* remote_cap)
{
    debug_printf("Server received pid %u\n", (uint32_t) msg->words[1]);
    domainid_t pid = (domainid_t) msg->words[1];

    // Identify client.
    struct client_state* client = identify_client(remote_cap);
    if (client == NULL) {
        debug_printf("ERROR: process_pid_request: could not idetify client");
        return NULL;
    }

    //look for process pid in list
    struct system_ps* aux;
    char* process_name;
    size_t length;

    for (aux = ps; aux != NULL; aux = aux->next) {
        if (aux->pid == pid) {
            break;
        }
    }

    length = strlen(aux->name);
    process_name = (char *) malloc(length * sizeof(char));
    strncpy(process_name, aux->name, length);

    debug_printf("Found process name %s for pid %u\n", aux->name, pid);

    if (aux == NULL) {
        debug_printf("Unable to find process with PID %u\n", pid);
        //new arguments
    }

    size_t args_size = ROUND_UP(sizeof(struct lmp_chan), 4)
        + ROUND_UP(length * sizeof(char), 4);

    void* args = malloc(args_size);
    void *return_args = args;
    
    // 1. Channel to send down.
    *((struct lmp_chan*) args) = client->lc;
    
    // 2. Error code fromm spawn process.
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(struct lmp_chan), 4);
    strncpy((char *)args, process_name, length);

    return return_args;
}

void* process_get_process_list_request(struct lmp_recv_msg* msg,
        struct capref* remote_cap)
{
    //TODO
    //parse request
    // Identify client.
    debug_printf("main.c process_get_process_list_request %u\n", (uint32_t) msg->words[1]);

    struct client_state* client = identify_client(remote_cap);
    if (client == NULL) {
        debug_printf("ERROR: process_get_list_request: could not idetify client");
        return NULL;
    }
    struct system_ps* aux = ps;
    const uint32_t ps_size = ps->curr_size;
    domainid_t pids[ps_size];
    
    for(int i=0; i<ps_size; i++)
    {
        pids[i] = aux->pid;
        aux = aux->next;
    }

    size_t args_size = ROUND_UP(sizeof(struct lmp_chan), 4)
        + ROUND_UP(sizeof(domainid_t), 4) 
        + ROUND_UP(ps_size * sizeof(int), 4);

    void* args = malloc(args_size);
    void* return_args = args;
    
    // 1. Channel to send down.
    *((struct lmp_chan*) args) = client->lc;
    
    // 2. Size of pids list.
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(struct lmp_chan), 4);
    *((uint32_t*)args) = ps_size;

    // 3. List of pids.
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(uint32_t), 4);
    args = (void*) pids;
    
    return return_args;
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
            case AOS_RPC_GETCHAR:
                response = (void*) send_serial_getchar;
                response_args = process_getchar_request(&msg, &cap);
                break;
            case AOS_RPC_STRING:
                response = (void*) send_simple_ok;
                response_args = process_string_request(&msg, &cap);
                break;
            case AOS_RPC_SPAWN:
                response = (void*) send_pid;
                response_args = process_spawn_request(&msg, &cap);
                break;
            case AOS_RPC_GET_PNAME:
                response = (void*) send_process_name;
                response_args = process_get_process_name_request(&msg, &cap);
                break;
            case AOS_RPC_GET_PLIST:
                printf("In AOS_RPC_GET_PLIST %u \n", AOS_RPC_GET_PLIST);
                response = (void*) send_ps_list;
                response_args = process_get_process_list_request(&msg, &cap);
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

errval_t send_serial_getchar(void* args)
{
    debug_printf("main.c send_serial_getchar!!\n");
    // 1. Get channel to send down.
    struct lmp_chan* lc = (struct lmp_chan*) args;

    // 2. Get returned char.
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(char), 4);
    char* get_char = (char*) args;

    // 3. Send response.
    CHECK("lmp_chan_send memory",
            lmp_chan_send2(lc, LMP_FLAG_SYNC, NULL_CAP, AOS_RPC_OK,
                    *get_char));

    // 4. Free args.
    free(args);

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
    CHECK("lmp_chan_send simple_ok",
            lmp_chan_send1(lc, LMP_FLAG_SYNC, NULL_CAP, AOS_RPC_OK));

    return SYS_ERR_OK;
}

errval_t send_pid(void* args)
{
    // 1. Get channel to send down.
    struct lmp_chan* lc = (struct lmp_chan*) args;

    // 2. Get error code.
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(struct lmp_chan), 4);
    errval_t* err = (errval_t*) args;

    // 3. Get pid.
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(errval_t), 4);
    domainid_t* pid = (domainid_t*) args;

    size_t code = err_is_fail(*err) ? AOS_RPC_FAILED : AOS_RPC_OK;

    // 2. Send response.
    CHECK("lmp_chan_send send_pid",
            lmp_chan_send3(lc, LMP_FLAG_SYNC, NULL_CAP, code, *err, *pid));

    return SYS_ERR_OK;
}

errval_t send_process_name(void* args)
{
    // 1. Get channel to send down.
    struct lmp_chan* lc = (struct lmp_chan*) args;

    // 2. Get process name.
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(struct lmp_chan), 4);
    size_t length = strlen((char*) args);
    
    char* name = (char *) malloc(length * sizeof(char));
    strncpy(name, args, length);
    //debug_printf("Length is %u, name is %s\n", length, name);

    size_t rem_length = length;

    uintptr_t* arguments = (uintptr_t*) malloc(9 * sizeof(uintptr_t));

    char words[32];
    char buf[4];
    //memcpy(words, string + i*32, 32);
    arguments[9] = (uintptr_t) ((uint32_t*) malloc(sizeof(uint32_t)));
    *((uint32_t*) arguments[9]) = rem_length;
    errval_t err;
    if(rem_length > 32) {
        debug_printf("THIS MESSAGE SHOULD NOT BE PRINTED!!\n");

        memcpy(words, name, 32);

        for(int j = 0; j < 8; j++) {
            memcpy(buf, words+(j * 4), 4);
            arguments[j+1] = (uintptr_t) ((char*) malloc(4*sizeof(char)));
            *((char*) arguments[j + 1]) = *buf;
            *((char*) arguments[j + 1] + 1) = *(buf + 1);
            *((char*) arguments[j + 1] + 2) = *(buf + 2);
            *((char*) arguments[j + 1] + 3) = *(buf + 3);
        }
        rem_length -= 32;
        //offset pointer by 32 bytes
        name += 32;

        err = lmp_chan_send9(lc, LMP_FLAG_SYNC, NULL_CAP, 
                *((uintptr_t*) arguments[9]), *((uintptr_t*) arguments[1]), 
                *((uintptr_t*) arguments[2]), *((uintptr_t*) arguments[3]), 
                *((uintptr_t*) arguments[4]), *((uintptr_t*) arguments[5]), 
                *((uintptr_t*) arguments[6]), *((uintptr_t*) arguments[7]),
                *((uintptr_t*) arguments[8]));
        
        //TODO: determine if rounded needed
        size_t args_size = ROUND_UP(sizeof(struct lmp_chan), 4)
            + ROUND_UP(rem_length * sizeof(char), 4);

        void* new_args = malloc(args_size);
        void *return_args = new_args;
        
        // 1. Channel to send down.
        *((struct lmp_chan*) new_args) = *lc;
        
        // 2. Error code fromm spawn process.
        new_args = (void*) ROUND_UP((uintptr_t) new_args + sizeof(struct lmp_chan), 4);
        strncpy((char *)new_args, name, rem_length);

        CHECK("lmp_chan_register_send parent",
            lmp_chan_register_send(lc, get_default_waitset(),
                    MKCLOSURE((void*) send_process_name, return_args)));


    } else {
        memcpy(words, name, rem_length);
        rem_length = 32;
        //printf("Remaining Length %d\n", rem_length);
        for(int k = length; k < rem_length; k++) {
            words[k] = 0;
        }
        //printf("%s\n", words);
        // for(int p=0; p<32; p++) {
        //     printf("%s\n", words[p]);
        // }
        
        for(int j = 0; j < (rem_length / 4); j++) {
            memcpy(buf, words+(j * 4), 4);
            //debug_printf("%%%%%%%% %c %c %c %c",buf[j], buf[j+1], buf[j+2], buf[j+3]);

            arguments[j+1] = (uintptr_t) ((char*) malloc(4*sizeof(char)));
            *((char*) arguments[j + 1]) = *buf;
            *((char*) arguments[j + 1] + 1) = *(buf + 1);
            *((char*) arguments[j + 1] + 2) = *(buf + 2);
            *((char*) arguments[j + 1] + 3) = *(buf + 3);
        }

        err = lmp_chan_send9(lc, LMP_FLAG_SYNC, NULL_CAP, 
                *((uintptr_t*) arguments[9]), *((uintptr_t*) arguments[1]), 
                *((uintptr_t*) arguments[2]), *((uintptr_t*) arguments[3]), 
                *((uintptr_t*) arguments[4]), *((uintptr_t*) arguments[5]), 
                *((uintptr_t*) arguments[6]), *((uintptr_t*) arguments[7]),
                *((uintptr_t*) arguments[8]));
    }

    return err;
}

errval_t send_ps_list(void* args)
{
    // 1. Get channel to send down.
    struct lmp_chan* lc = (struct lmp_chan*) args;

    // 2. Get process list size.
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(struct lmp_chan), 4);
    uint32_t ps_size = *((uint32_t*)args);

    // 3. Get process list     
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(uint32_t), 4);
    domainid_t* ps_list = (domainid_t*)args;

    size_t rem_length = ps_size;

    debug_printf("!!!!! Size of list is %u", ps_size);

    uintptr_t* arguments = (uintptr_t*) malloc(8 * sizeof(uintptr_t));

    arguments[0] = (uintptr_t) ((uint32_t*) malloc(sizeof(uint32_t)));
    *((uint32_t*) arguments[0]) = rem_length;
    errval_t err;

    if(rem_length > 7) {

        for(int j = 0; j < 7; j++) {
            arguments[j+1] = (uintptr_t) ((domainid_t*) malloc(sizeof(domainid_t)));
            domainid_t *dom = (domainid_t *)arguments[j + 1];
            *dom = ps_list[j];
        }
        rem_length -= 7;
        //offset pointer by 32 bytes
        ps_list += 7;

        err = lmp_chan_send9(lc, LMP_FLAG_SYNC, NULL_CAP, AOS_RPC_OK,
                *((uintptr_t*) arguments[0]), *((uintptr_t*) arguments[1]), 
                *((uintptr_t*) arguments[2]), *((uintptr_t*) arguments[3]), 
                *((uintptr_t*) arguments[4]), *((uintptr_t*) arguments[5]), 
                *((uintptr_t*) arguments[6]), *((uintptr_t*) arguments[7]));
        
        //TODO: determine if rounded needed
        size_t args_size = ROUND_UP(sizeof(struct lmp_chan), 4)
            + ROUND_UP(rem_length * sizeof(char), 4);

        void* new_args = malloc(args_size);
        void *return_args = new_args;
        
        // 1. Channel to send down.
        *((struct lmp_chan*) new_args) = *lc;
        
        // 2. Error code fromm spawn process.
        new_args = (void*) ROUND_UP((uintptr_t) new_args + sizeof(struct lmp_chan), 4);
        *((uint32_t *)new_args) = rem_length;

        // 3. Copy process list.
        new_args = (void*) ROUND_UP((uintptr_t) new_args + sizeof(uint32_t), 4);
        memcpy(new_args, ps_list, rem_length);
        // *((uint32_t *)new_args) = rem_length;

        CHECK("lmp_chan_register_send parent",
            lmp_chan_register_send(lc, get_default_waitset(),
                    MKCLOSURE((void*) send_ps_list, return_args)));


    } else {
        for(int j = 0; j < rem_length; j++) {
            arguments[j+1] = (uintptr_t) ((domainid_t*) malloc(sizeof(domainid_t)));
            domainid_t *dom = (domainid_t *)arguments[j + 1];
            *dom = ps_list[j];
        }

        for(int j = rem_length; j < 7; j++) {
            arguments[j+1] = (uintptr_t) ((domainid_t*) malloc(sizeof(domainid_t)));
            domainid_t *dom = (domainid_t *)arguments[j + 1];
            *dom = -1;
        }

        err = lmp_chan_send9(lc, LMP_FLAG_SYNC, NULL_CAP, AOS_RPC_OK,
                *((uintptr_t*) arguments[0]), *((uintptr_t*) arguments[1]), 
                *((uintptr_t*) arguments[2]), *((uintptr_t*) arguments[3]), 
                *((uintptr_t*) arguments[4]), *((uintptr_t*) arguments[5]), 
                *((uintptr_t*) arguments[6]), *((uintptr_t*) arguments[7]));
    }

    return err;

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

    genpaddr_t remaining_mem_base = 0;
    gensize_t remaining_mem_size = 0;
    if (my_core_id == 0) {
        err = initialize_ram_alloc(&remaining_mem_base, &remaining_mem_size);
        if(err_is_fail(err)){
            DEBUG_ERR(err, "initialize_ram_alloc");
        }
    }

    void* urpc_buf;
    CHECK("mapping URPC frame into vspace",
            map_urpc_frame_to_vspace(&urpc_buf, 2u * BASE_PAGE_SIZE, my_core_id));
    write_to_urpc(urpc_buf, remaining_mem_base, remaining_mem_size, bi,
            my_core_id);
    CHECK("forging RAM cap & retrieving bi from URPC frame",
            read_from_urpc(urpc_buf, &bi, my_core_id));
    CHECK("start core 1", start_core(1, my_core_id, bi));

    if (my_core_id == 1) {
        err = initialize_ram_alloc(&remaining_mem_base, &remaining_mem_size);
        if(err_is_fail(err)){
            DEBUG_ERR(err, "initialize_ram_alloc");
        }
        
        CHECK("reading modules from URPC",
                read_modules(urpc_buf, bi, my_core_id));
    }

    CHECK("Retype selfep from dispatcher", cap_retype(cap_selfep, cap_dispatcher, 0, ObjType_EndPoint, 0, 1));

    struct lmp_chan* lc = (struct lmp_chan*) malloc(sizeof(struct lmp_chan));
    CHECK("Create channel for parent", lmp_chan_accept(lc, DEFAULT_LMP_BUF_WORDS, NULL_CAP));

    CHECK("Create Slot", lmp_chan_alloc_recv_slot(lc));
    CHECK("COpy to initep", cap_copy(cap_initep, lc->local_cap));

    CHECK("lmp_chan_register_recv child",
            lmp_chan_register_recv(lc, get_default_waitset(),
                    MKCLOSURE((void*) recv_handler, lc)));

    if (my_core_id == 0) {
        // Spawn "Hello" on core 0.
        CHECK("spawning hello",
                spawn_load_by_name("hello",
                        (struct spawninfo*) malloc(sizeof(struct spawninfo)), my_core_id));
    } else {
        // Spawn "Byebye" on core 0.
        CHECK("spawning byebye",
                spawn_load_by_name("byebye",
                        (struct spawninfo*) malloc(sizeof(struct spawninfo)), my_core_id));
    }

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
