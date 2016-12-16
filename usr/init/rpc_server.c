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

#include "rpc_server.h"

extern coreid_t my_core_id;
static domainid_t last_issued_pid = 1;

int n_requests = 0;

struct system_ps* ps = NULL;

struct client_state* identify_client(struct capref* cap,
        struct client_state* clients)
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

errval_t serve_locally(struct lmp_recv_msg* msg, struct capref* cap,
        struct client_state** clients)
{
    void* response;
    void* response_args;

    switch (msg->words[0]) {
        case AOS_RPC_HANDSHAKE:
            response = (void*) send_handshake;
            response_args = process_local_handshake_request(cap, clients);
            break;
        case AOS_RPC_MEMORY:
            response = (void*) send_memory;
            response_args = process_local_memory_request(msg, cap, *clients);
            break;
        case AOS_RPC_NUMBER:
            response = (void*) send_simple_ok;
            response_args = process_local_number_request(msg, cap, *clients);
            break;
        case AOS_RPC_PUTCHAR:
            response = (void*) send_simple_ok;
            response_args = process_local_putchar_request(msg, cap, *clients);
            break;
        case AOS_RPC_GETCHAR:
            response = (void*) send_serial_getchar;
            response_args = process_local_getchar_request(msg, cap, *clients);
            break;
        case AOS_RPC_STRING:
            response = (void*) send_simple_ok;
            response_args = process_local_string_request(msg, cap, *clients);
            break;
        case AOS_RPC_SPAWN:
            response = (void*) send_pid;
            response_args = process_local_spawn_request(msg, cap, *clients);
            break;
        case AOS_RPC_GET_PNAME:
            response = (void*) send_process_name;
            response_args = process_local_get_process_name_request(
                    msg, cap, *clients);
            break;
        case AOS_RPC_GET_PLIST:
            response = (void*) send_ps_list;
            response_args = process_local_get_process_list_request(
                    msg, cap, *clients);
            break;
      
        default:
            return 1;  // TODO: More meaning plz
    }

    struct lmp_chan* out = (struct lmp_chan*) response_args;
    CHECK("lmp_chan_register_send parent",
            lmp_chan_register_send(out, get_default_waitset(),
                    MKCLOSURE(response, response_args)));

    return SYS_ERR_OK;
}

void* process_local_handshake_request(struct capref* request_cap,
        struct client_state** clients)
{
    struct client_state* existing = identify_client(request_cap, *clients);
    if (existing != NULL) {
        // No point opening new channel.
        return (void*) &existing->lc;
    }

    // Create state for newly connecting client.
    struct client_state* new_client = (struct client_state*) malloc(
            sizeof(struct client_state));
    if (*clients == NULL) {
        new_client->next = new_client->prev = NULL;
    } else {
        (*clients)->prev = new_client;
        new_client->next = *clients;
        new_client->prev = NULL;
    }
    new_client->client_frame = NULL;
    new_client->server_frame = NULL;

    // Initialize client state.
    new_client->ram = 0;
    new_client->str_buf = NULL;
    new_client->str_buf_idx = 0;
    
    *clients = new_client;

    // Endpoint for further reference.
    struct capability ret;
    debug_cap_identify(*request_cap, &ret);
    new_client->remote_ep = ret.u.endpoint;

    // New channel.
    lmp_chan_accept(&new_client->lc, DEFAULT_LMP_BUF_WORDS, *request_cap);

    if (n_requests == 0) {
        n_requests++;
        errval_t err = cap_copy(cap_nsep, *request_cap);
        if(err_is_fail(err)) {
            printf("%s\n", err_getstring(err));
        }
    }

    // Return response args.
    return (void*) &new_client->lc;
}

errval_t rpc_ram_alloc(struct capref* retcap, size_t size, size_t* retsize)
{
    *retsize = ROUND_UP(size, BASE_PAGE_SIZE);
    if (*retsize == 0) {
        *retsize = BASE_PAGE_SIZE;
    }
    return ram_alloc(retcap, *retsize);
}

void* process_local_memory_request(struct lmp_recv_msg* msg,
        struct capref* request_cap, struct client_state* clients)
{
    struct client_state* client = identify_client(request_cap, clients);
    if (client == NULL) {
        debug_printf("ERROR: process_local_memory_request: could not identify "
                "client\n");
        return NULL;
    }

    size_t req_size = (size_t) msg->words[1];
    if (req_size + client->ram >= MAX_CLIENT_RAM) {
        // Limit to MAX_CLIENT_RAM.
        req_size = MAX_CLIENT_RAM - client->ram;
    }

    struct capref mem_cap;
    size_t retsize;
    errval_t err = rpc_ram_alloc(&mem_cap, req_size, &retsize);
    client->ram += retsize;

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
    *((size_t*) args) = retsize;

    return return_args;
}

void* process_local_number_request(struct lmp_recv_msg* msg,
        struct capref* request_cap, struct client_state* clients)
{
    // Print what we got.
    debug_printf("Server received number %u\n", (uint32_t) msg->words[2]);

    // Identify client.
    struct client_state* client = identify_client(request_cap, clients);
    if (client == NULL) {
        debug_printf("ERROR: process_local_number_request: could not idetify client");
        return NULL;
    }

    // Return channel for response handler.
    return (void*) &client->lc;
}

void* process_local_putchar_request(struct lmp_recv_msg* msg,
        struct capref* request_cap, struct client_state* clients)
{
    // Put character.
    rpc_putchar((char*) &msg->words[1]);

    // Identify client.
    struct client_state* client = identify_client(request_cap, clients);
    if (client == NULL) {
        debug_printf("ERROR: process_local_putchar_request: could not idetify client");
        return NULL;
    }

    // Return chanel for response handler.
    return (void*) &client->lc;
}

void* process_local_getchar_request(struct lmp_recv_msg* msg,
            struct capref* request_cap, struct client_state* clients)
{
    // Identify Client
    struct client_state* client = identify_client(request_cap, clients);
    if (client == NULL) {
        debug_printf("ERROR: process_local_putchar_request: could not idetify client\n");
        return NULL;
    }

    msg->words[2] = rpc_getchar();

    // Response args.
    size_t args_size = ROUND_UP(sizeof(struct lmp_chan), 4)
            + ROUND_UP(sizeof(char), 4);
    void* args = malloc(args_size);
    void *return_args = args;
    
    // 1. Channel to send down.
    *((struct lmp_chan*) args) = client->lc;

    // 2. Character returned by sys_getchar
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(struct lmp_chan), 4);
    *((char*) args) = msg->words[2];

    return return_args;
}

void* process_local_string_request(struct lmp_recv_msg* msg,
        struct capref* request_cap, struct client_state* clients)
{
    uint32_t remaining = msg->words[2];

    struct client_state* client = clients;
    while (client != NULL) {
        struct capability ret;
        debug_cap_identify(*request_cap, &ret);
        if (client->remote_ep.listener == ret.u.endpoint.listener
                && client->remote_ep.epoffset == ret.u.endpoint.epoffset) {
            break;
        }
        client = client->next;
    }
    if (client == NULL) {
        debug_printf("main.c: process_local_string_request: could not find client\n");
        return NULL;
    }

    if (client->str_buf == NULL) {
        client->str_buf = (char*) malloc(remaining * sizeof(char));
        client->str_buf_idx = 0;
    }

    size_t stop = remaining < 24 ? remaining : 24;
    for (size_t i = 0; i < stop; ++i) {
        uint32_t word = msg->words[3 + i / 4];
        client->str_buf[client->str_buf_idx++] =
                (char) (word >> (8 * (i % 4)));
    }

    remaining -= stop;
    if (remaining == 0) {
        // Print and reset.
        rpc_string(client->str_buf, client->str_buf_idx);
        client->str_buf_idx = 0;
        free(clients->str_buf);
        client->str_buf = NULL;
    }

    // Return response args.
    return (void*) &client->lc;
}

errval_t rpc_spawn(char* name, domainid_t* pid)
{
    if (strcmp(name, "init") == 0) {
        return SPAWN_ERR_FIND_SPAWNDS;
    }

    struct system_ps* new_ps = (struct system_ps*) malloc(
            sizeof(struct system_ps));
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
    CHECK("RPC spawning process",
            spawn_load_by_name(name,
                    (struct spawninfo*) malloc(sizeof(struct spawninfo)),
                    my_core_id));
    
    //ps->process = process_local_info;
    new_ps->name = (char*) malloc(strlen(name) * sizeof(char));
    strcpy(new_ps->name, name);
    //Add process to process list
    ps = new_ps;

    *pid = new_ps->pid;

    return SYS_ERR_OK;
}

void* process_local_spawn_request(struct lmp_recv_msg* msg,
    struct capref* request_cap, struct client_state* clients)
{
    uint32_t remaining = msg->words[2];

    struct client_state* client = clients;
    while (client != NULL) {
        struct capability ret;
        debug_cap_identify(*request_cap, &ret);
        if (client->remote_ep.listener == ret.u.endpoint.listener
                && client->remote_ep.epoffset == ret.u.endpoint.epoffset) {
            break;
        }
        client = client->next;
    }
    if (client == NULL) {
        debug_printf("main.c: process_local_spawn_request: could not find client\n");
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
        domainid_t pid;
        errval_t err = rpc_spawn(client->spawn_buf, &pid);
        
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
        *((domainid_t*) args) = pid;
        
        client->spawn_buf_idx = 0;
        free(client->spawn_buf);
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

char* rpc_process_name(domainid_t pid, size_t* len)
{
    struct system_ps* aux;
    char* process_name;
    *len = 0;
    for (aux = ps; aux != NULL; aux = aux->next) {
        if (aux->pid == pid) {
            break;
        }
    }
    if (aux == NULL) {
        debug_printf("Unable to find process with PID %u\n", pid);
        return NULL;
    }

    *len = strlen(aux->name);
    process_name = (char*) malloc(*len * sizeof(char));
    strncpy(process_name, aux->name, *len);

    return process_name;
}

void* process_local_get_process_name_request(struct lmp_recv_msg* msg,
        struct capref* request_cap, struct client_state* clients)
{
    debug_printf("Server received pid %u\n", (uint32_t) msg->words[2]);
    domainid_t pid = (domainid_t) msg->words[2];

    // Identify client.
    struct client_state* client = identify_client(request_cap, clients);
    if (client == NULL) {
        debug_printf("ERROR: procss_local_get_process_name_request: could not "
                "identify client");
        return NULL;
    }

    size_t length;
    char* process_name = rpc_process_name(pid, &length);

    size_t args_size = ROUND_UP(sizeof(struct lmp_chan), 4)
        + ROUND_UP(length * sizeof(char), 4);

    void* args = malloc(args_size);
    void *return_args = args;
    
    // 1. Channel to send down.
    *((struct lmp_chan*) args) = client->lc;
    
    // 2. Requested process name.
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(struct lmp_chan), 4);
    strncpy((char *)args, process_name, length);

    return return_args;
}

domainid_t* rpc_process_list(size_t* len)
{
    struct system_ps* aux = ps;
    *len = ps->curr_size;
    domainid_t* pids = (domainid_t*) malloc(*len * sizeof(domainid_t));

    for (size_t i = 0; i < *len; ++i) {
        pids[i] = aux->pid;
        aux = aux->next;
    }

    return pids;
}

void* process_local_get_process_list_request(struct lmp_recv_msg* msg,
        struct capref* request_cap, struct client_state* clients)
{
    struct client_state* client = identify_client(request_cap, clients);
    if (client == NULL) {
        debug_printf("ERROR: process_local_get_process_list_request: could "
                "not identify client");
        return NULL;
    }

    uint32_t ps_size;
    domainid_t* pids = rpc_process_list(&ps_size);

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
    memcpy(args, pids, ps_size * sizeof(domainid_t));
    
    return return_args;
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

errval_t send_simple_ok(void* args)
{
    // 1. Get channel to send down.
    struct lmp_chan* lc = (struct lmp_chan*) args;

    // 2. Send response.
    CHECK("lmp_chan_send simple_ok",
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
        new_args = (void*) ROUND_UP(
                (uintptr_t) new_args + sizeof(struct lmp_chan), 4);
        strncpy((char *)new_args, name, rem_length);

        CHECK("lmp_chan_register_send parent",
            lmp_chan_register_send(lc, get_default_waitset(),
                    MKCLOSURE((void*) send_process_name, return_args)));


    } else {
        memcpy(words, name, rem_length);
        rem_length = 32;
        for(int k = length; k < rem_length; k++) {
            words[k] = 0;
        }
        for(int j = 0; j < (rem_length / 4); j++) {
            memcpy(buf, words+(j * 4), 4);

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

    uintptr_t* arguments = (uintptr_t*) malloc(8 * sizeof(uintptr_t));

    arguments[0] = (uintptr_t) ((uint32_t*) malloc(sizeof(uint32_t)));
    *((uint32_t*) arguments[0]) = rem_length;
    errval_t err;

    if(rem_length > 7) {

        for(int j = 0; j < 7; j++) {
            arguments[j+1] = (uintptr_t) ((domainid_t*) malloc(
            		sizeof(domainid_t)));
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
        new_args = (void*) ROUND_UP(
                (uintptr_t) new_args + sizeof(struct lmp_chan), 4);
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
            arguments[j+1] = (uintptr_t) ((domainid_t*) malloc(
                    sizeof(domainid_t)));
            domainid_t *dom = (domainid_t *)arguments[j + 1];
            *dom = ps_list[j];
        }

        for(int j = rem_length; j < 7; j++) {
            arguments[j+1] = (uintptr_t) ((domainid_t*) malloc(
                    sizeof(domainid_t)));
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
