/**
 * \file
 * \brief Nameserver application
 */

/*
 * Copyright (c) 2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, CAB F.78, Universitaetstr. 6, CH-8092 Zurich,
 * Attn: Systems Group.
 */


#include <stdio.h>
#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/waitset.h>
#include <aos/paging.h>
#include <nameserver_rpc.h>
//#include <rpc_server.h>

struct nsclient_state {
    struct lmp_chan lc;  // LMP channel.
    
    // enum ns_service_class service_class;
    char* name;

    struct EndPoint remote_ep;  // Used to identify clients.
    struct EndPoint local_ep;  // Used to identify clients.

    bool is_server;
    struct capref service_ep;

    // Doubly-linked list.
    struct nsclient_state* next;
    struct nsclient_state* prev;
};

struct nsclient_state* clients;

struct nsclient_state* identify_client(struct capref* cap);
struct nsclient_state* identify_client_by_local_cap(struct capref* cap);

char** ns_service_list(size_t* len, size_t** total_string_length);

void* ns_process_handshake(struct capref* cap);
void ns_send_handshake(void* arg);

void* ns_process_register(struct lmp_chan* lc, struct capref* client_cap, struct lmp_recv_msg msg);
errval_t ns_send_simple_ok(void* arg);

void* ns_process_deregister(struct lmp_chan* lc);
errval_t ns_send_simple_ack(void* args);

// void* ns_process_ennumerate(struct lmp_chan* lc);
void* ns_process_enumerate(struct lmp_chan* lc);
errval_t ns_send_service_list(void* args);

void* ns_process_lookup(struct lmp_chan* lc, struct lmp_recv_msg msg);
errval_t ns_send_ep(void* args);

void ns_serve_rpc(void* arg);

struct nsclient_state* identify_client(struct capref* cap)
{
    struct capability ret;
    errval_t err = debug_cap_identify(*cap, &ret);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "identifying client cap");
        return NULL;
    }


    struct nsclient_state* client = clients;
    while (client != NULL) {
        if (client->remote_ep.listener == ret.u.endpoint.listener
                && client->remote_ep.epoffset == ret.u.endpoint.epoffset) {
            break;
        }
        client = client->next;
    }
    
    return client;
}

struct nsclient_state* identify_client_by_local_cap(struct capref* cap)
{
    struct capability ret;
    errval_t err = debug_cap_identify(*cap, &ret);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "identifying client cap");
        return NULL;
    }

    struct nsclient_state* client = clients;
    while (client != NULL) {
        //debug_printf("IN WHILE LOOP!!\n");
        if (client->local_ep.listener == ret.u.endpoint.listener
                && client->local_ep.epoffset == ret.u.endpoint.epoffset) {
            break;
        }
        /*debug_printf("client->local_ep.listener = %u," 
                        "ret.u.endpoint.listener = %u,"
                        "client->local_ep.epoffset = %u," 
                        "ret.u.endpoint.epoffset = %u\n",  
                        client->local_ep.listener, 
                        ret.u.endpoint.listener, 
                        client->local_ep.epoffset, 
                        ret.u.endpoint.epoffset);*/

        client = client->next;
    }
    
    return client;
}

void* ns_process_handshake(struct capref* cap)
{
    struct nsclient_state* client = identify_client(cap);
    if (client != NULL) {
        // Client already exists?
        debug_printf("Got second hanshake request from same client, "
                "ignoring it\n");
        return NULL;
    }

    client = (struct nsclient_state*) malloc(sizeof(struct nsclient_state));
    if (clients == NULL) {
        client->next = NULL;
    } else {
        clients->prev = client;
        client->next = clients;
    }
    client->prev = NULL;
    client->is_server = false;
    client->service_ep = NULL_CAP;

    clients = client;

    // Endpoint for further reference.
    struct capability ret;
    debug_cap_identify(*cap, &ret);
    client->remote_ep = ret.u.endpoint;

    // New channel.
    lmp_chan_accept(&client->lc, DEFAULT_LMP_BUF_WORDS, *cap);
    errval_t err = lmp_chan_alloc_recv_slot(&client->lc);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "lmp_chan_alloc_recv_slot for new client");
    }

    debug_cap_identify(client->lc.local_cap, &ret);
    client->local_ep = ret.u.endpoint;

    err = lmp_chan_register_recv(&client->lc, get_default_waitset(),
            MKCLOSURE(ns_serve_rpc, &client->lc));
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "lmp_chan_register_recv for new client");
    }

    // debug_printf("SERVER: received handshake request from process\n");
    // Response args.
    return (void*) &client->lc;
}

void ns_send_handshake(void* arg)
{
    // 1. Channel to send down.
    struct lmp_chan* lc = (struct lmp_chan*) arg;

    // 2. Send response.
    errval_t err = lmp_chan_send1(lc, LMP_FLAG_SYNC, lc->local_cap,
            AOS_NS_OK);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "lmp_chan_send handshake");
    }
}

errval_t ns_send_simple_ok(void* args)
{
    // 1. Get channel to send down.
    struct lmp_chan* lc = (struct lmp_chan*) args;

    // 2. Send response.
    CHECK("lmp_chan_send simple_ok",
            lmp_chan_send1(lc, LMP_FLAG_SYNC, NULL_CAP, AOS_NS_OK));

    return SYS_ERR_OK;
}

void* ns_process_register(struct lmp_chan* lc, struct capref* client_cap, struct lmp_recv_msg msg)
{
    struct nsclient_state* client = identify_client_by_local_cap(&lc->local_cap);
    if (client == NULL) {
        // Client already exists?
        debug_printf("Unable to identify client by local cap (register)\n");
        return NULL;
    }

    client->service_ep = *client_cap;
    client->is_server = true;

    int rem_words = 7;
    int word_offset = 9 - rem_words;

    int length = msg.words[1];
    char* name = (char*)malloc(length * sizeof(char));
    char* message = (char*)malloc(rem_words * 4 * sizeof(char));
    char* message_offset = message;

    for (int i = 0; i < rem_words; i++) {
        // debug_printf("here %d\n", i);
        memcpy(message_offset, (char*)&(msg.words[word_offset + i]), 4);
        message_offset += 4;
    }

    strncpy(name, message, length);
    client->name = name;

    // debug_printf("SERVER: received register request from %s\n", client->name);
    // Response args.
    return (void*) lc;
}

errval_t ns_send_simple_ack(void* args)
{
    // 1. Get channel to send down.
    struct lmp_chan* lc = (struct lmp_chan*) args;

    // 2. Get process name.
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(struct lmp_chan), 4);
    bool correct = (bool) args;

    int error_tag = (correct) ? AOS_NS_OK : AOS_NS_FAILED;
    // 2. Send response.
    CHECK("lmp_chan_send simple_ok",
            lmp_chan_send1(lc, LMP_FLAG_SYNC, NULL_CAP, error_tag));

    return SYS_ERR_OK;
}

void* ns_process_deregister(struct lmp_chan* lc)
{
    bool correct = true;

    struct nsclient_state* client = identify_client_by_local_cap(&lc->local_cap);
    if (client == NULL) {
        // Client already exists?
        debug_printf("Unable to identify client by local cap (deregister)\n");
        //do something since this dergister call is "illegal"
        //return NULL;
        correct = false;
    }

    client->is_server = false;

    // debug_printf("SERVER: received deregister request from %s\n", client->name);

    size_t args_size = ROUND_UP(sizeof(struct lmp_chan), 4)
        + ROUND_UP(sizeof(bool), 4);

    void* args = malloc(args_size);
    void* return_args = args;
    
    // 1. Channel to send down.
    *((struct lmp_chan*) args) = client->lc;
    
    // 2. Requested process name.
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(struct lmp_chan), 4);
    *( (bool*) args) = correct;
    // Response args.
    return return_args;
}

//TODO: MANU change 10 for real value
errval_t ns_send_service_list(void* args)
{
    // debug_printf("In ns_send_service_list\n");
    void* aux = args;
    // void* aux2 = args;
    int offset_memcpy = 0, rem_length = 0;
    // 1. Get channel to send down.
    struct lmp_chan* lc = (struct lmp_chan*) args;
    bool last_message = false;

    args = (void*) ROUND_UP((uintptr_t) args + sizeof(struct lmp_chan), 4);

    // 2. Get process list size.
    uint32_t number_of_services = *((uint32_t*)args);
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(uint32_t), 4);

    uint32_t* lengths = (uint32_t*) malloc(number_of_services * sizeof(uint32_t));
    char** names = (char**) malloc(number_of_services);
    
    // debug_printf("In ns_send_service_list: number_ofServices = %d\n", number_of_services);

    // size_t num_messages = 0;
    size_t message_offset = 0;
    size_t rem_services = number_of_services;
    errval_t err = SYS_ERR_OK;

    for (int i = 0; i < number_of_services; i++) {
        lengths[i] = *((uint32_t*)args);

        // debug_printf("Lengths[%d] = %u\n", i, lengths[i]);
        args = (void*) ROUND_UP((uintptr_t) args + sizeof(uint32_t), 4);
        message_offset++;

        names[i] = (char*) malloc(lengths[i] * sizeof(char));
        
        strncpy(names[i], args, lengths[i]);

        args = (void*) ROUND_UP((uintptr_t) args + lengths[i], 4);
        if (i == 0) {
            offset_memcpy = args - aux;
        } else if (i == (number_of_services - 1)){
            rem_length = args - aux - offset_memcpy;
        }
    }
    // debug_printf("Service name is %s\n", names[0]);
    // debug_printf("offset = %d, remaining = %d\n", offset_memcpy, rem_length);

    uintptr_t* arguments = (uintptr_t*) malloc(8 * sizeof(uintptr_t));

    arguments[0] = (uintptr_t) ((uint32_t*) malloc(sizeof(uint32_t)));
    *((uint32_t*) arguments[0]) = rem_services;

    arguments[1] = (uintptr_t) ((uint32_t*) malloc(sizeof(uint32_t)));
    *((uint32_t*) arguments[1]) = strlen(names[0]);

    int word_offset = 2;
    int length = strlen(names[0]);
    int name_offset = 0;
    if(length > 24) {
        debug_printf("!!!!!!!!!!!!!!!!! ERROR: length is larger than 24B\n");
    }

    for (int z = 2; z < 8; z++) {
        arguments[z] = (uintptr_t) ((uint32_t*) malloc(sizeof(uint32_t)));   
    }
    
    for (int j = 0; j <= length/4; j++) {
        memcpy((uintptr_t*)(arguments[j + word_offset]), names[0] + name_offset, 4);
        name_offset += 4;
    }

    // aux2 = (void*) ROUND_UP((uintptr_t) aux2 + sizeof(struct lmp_chan), 4);
    // debug_printf("################ newargs(size) = %u\n", *((uint32_t*)aux2));
    // aux2 = (void*) ROUND_UP((uintptr_t) aux2 + sizeof(uint32_t), 4);
    // debug_printf("################ newargs(size) = %u\n", *((uint32_t*)aux2));
    // aux2 = (void*) ROUND_UP((uintptr_t) aux2 + sizeof(uint32_t), 4);
    // debug_printf("################ newargs(name) = %s\n", aux2);
    // aux2 = (void*) ROUND_UP((uintptr_t) aux2 + 9, 4);
    // debug_printf("################ newargs(size) = %u\n", *((uint32_t*)aux2));
    // aux2 = (void*) ROUND_UP((uintptr_t) aux2 + sizeof(uint32_t), 4);
    // debug_printf("################ newargs(name) = %s\n", aux2);
    // aux2 = (void*) ROUND_UP((uintptr_t) aux2 + 9, 4);

    err = lmp_chan_send9(lc, LMP_FLAG_SYNC, NULL_CAP, AOS_NS_OK,
    *((uintptr_t*) arguments[0]), *((uintptr_t*) arguments[1]),
    *((uintptr_t*) arguments[2]), *((uintptr_t*) arguments[3]),
    *((uintptr_t*) arguments[4]), *((uintptr_t*) arguments[5]), 
    *((uintptr_t*) arguments[6]), *((uintptr_t*) arguments[7]));

    // debug_printf("After send\n");
    rem_services--;
    
    if (rem_services == 0) {
        // debug_printf("SENDING THE LAST MESSAGE\n");
        last_message = true;
    }

    if (!last_message) {
        // debug_printf("####################### Sending a follow-up message\n");
        int name_size = 0;
        for (int l = 1; l < rem_services; l++) {
            name_size += ROUND_UP(strlen(names[l]), 4);
        }

        size_t args_size = ROUND_UP(sizeof(struct lmp_chan), 4)
            + ROUND_UP(sizeof(int), 4) 
            + ROUND_UP(rem_services * sizeof(int), 4)
            + ROUND_UP(name_size, 4);

        void* new_args = malloc(args_size);
        void* return_args = new_args;
    
        // 1. Channel to send down.
        // debug_printf("Args size = %d, offset = %d, remaining = %d\n", args_size, new_args - return_args, args_size - (new_args - return_args));
        *((struct lmp_chan*)new_args) = *lc;
        new_args = (void*) ROUND_UP((uintptr_t) new_args + sizeof(struct lmp_chan), 4);
        // debug_printf("Args size = %d, offset = %d, remaining = %d\n", args_size, new_args - return_args, args_size - (new_args - return_args));
        // debug_printf("Remaining services is %d\n", rem_services);
        *((uint32_t*)new_args) = rem_services;
        new_args = (void*) ROUND_UP((uintptr_t) new_args + sizeof(uint32_t), 4);
        // debug_printf("Args size = %d, offset = %d, remaining = %d\n", args_size, new_args - return_args, args_size - (new_args - return_args));
        
        memcpy(new_args, aux + offset_memcpy, rem_length);        

        /*for (int k = 1; k < rem_services + 1; k++) {

            *((uint32_t*)new_args) = lengths[k];
            debug_printf("################ newargs(size) = %u\n", *((uint32_t*)new_args));

            new_args = (void*) ROUND_UP((uintptr_t) new_args + sizeof(uint32_t), 4);
            // debug_printf("Args size = %d, offset = %d, remaining = %d\n", args_size, new_args - return_args, args_size - (new_args - return_args));
            memcpy(new_args, names[k], lengths[k] * sizeof(char));
            debug_printf("################ newargs(name) = %s\n", new_args);
            new_args = (void*) ROUND_UP((uintptr_t) new_args + lengths[k], 4);
            // debug_printf("Args size = %d, offset = %d, remaining = %d\n", args_size, new_args - return_args, args_size - (new_args - return_args));
        }*/

        CHECK("lmp_chan_register_send parent",
            lmp_chan_register_send(lc, get_default_waitset(),
                    MKCLOSURE((void*) ns_send_service_list, return_args)));    
    }   

    return err;
    // return SYS_ERR_OK;
}

char** ns_service_list(size_t* len, size_t** string_length)
{
    struct nsclient_state* aux = clients;
    *len = 0;
    int list_length = 0;
    int n_servers = 0;

    while (aux != NULL) {
        list_length++;
        if (aux->is_server) {
            n_servers++;
        }
        aux = aux->next;
    }
    aux = clients;

    char** name_list = (char**) malloc(n_servers * sizeof(char*));
    *string_length= (size_t*)malloc(n_servers * sizeof(size_t));

    int index = 0;
    for (size_t i = 0; i < list_length; ++i) {
        if (aux->is_server) {
            // debug_printf("Client %d is a server\n", i);
            int length = strlen(aux->name);
            // debug_printf("IN ns_service_list: %s length is %d\n", aux->name, length);
            (*string_length)[index] = length;            
            name_list[index] = (char*) malloc(length * sizeof(char));
            memcpy(name_list[index], aux->name, length);
            index++;
        }
        aux = aux->next;     
    }

    *len = index;

    return name_list;
}

void* ns_process_enumerate(struct lmp_chan* lc)
{
    // debug_printf("In ns_process_enumerate\n");

    struct nsclient_state* client = identify_client_by_local_cap(&lc->local_cap);
    if (client == NULL) {
        // Client already exists?
        debug_printf("Unable to identify client by local cap (register)\n");
        return NULL;
    }

    uint32_t service_list_size;
    uint32_t* service_list_name_size = NULL;

    // debug_printf("Before calling ns_service_list\n", service_list_size);

    char** names = ns_service_list(&service_list_size, &service_list_name_size);

    // debug_printf("service_list_size = %d\n", service_list_size);

    size_t name_size = 0;
    for (int i = 0; i < service_list_size; i++) {
        name_size += (ROUND_UP(service_list_name_size[i], 4));
    }
    // debug_printf("service_list_name_size = %d\n", name_size);

    size_t args_size = ROUND_UP(sizeof(struct lmp_chan), 4)
        + ROUND_UP(sizeof(uint32_t), 4) 
        + ROUND_UP(service_list_size * sizeof(uint32_t), 4)
        + ROUND_UP(name_size, 4);

    // debug_printf("sizeof(struct lmp_chan) = %d\n", ROUND_UP(sizeof(struct lmp_chan), 4));

    // debug_printf("Args size is %d\n", args_size);
    // debug_printf("%s\n", names[0]);

    // for (int i = 0; i < service_list_size; i++) {
    //     debug_printf("Length of service %d - %s is %d\n", i, names[i], service_list_name_size[i]);
    // }

    void* args = malloc(args_size);
    void* return_args = args;
    
    // 1. Channel to send down.
    *((struct lmp_chan*) args) = *lc;
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(struct lmp_chan), 4);
    
    // 2. Size of pids list.
    *((uint32_t*)args) = service_list_size;
    // debug_printf("offsef is %d, still %d bytes left\n", args - return_args, args_size - (args - return_args));
    // debug_printf("!!!!!!!!!!!!!!!! %llu %llu %llu %llu %llu %llu\n", ((uint64_t*)return_args)[0], ((uint64_t*)return_args)[1], ((uint64_t*)return_args)[2], ((uint64_t*)return_args)[3], ((uint64_t*)return_args)[4], ((uint64_t*)return_args)[5]);
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(uint32_t), 4);

    for (int i = 0; i < service_list_size; i++) {
        
        // debug_printf("service_list_name_size[%d] = %d, name = %s\n", i, service_list_name_size[i], names[i]);

        // *((uint32_t*)args) = strlen(names[i]);
        *((uint32_t*)args) = service_list_name_size[i];
        // *((uint32_t*)args) = 10;
        // debug_printf("lalalalalal\n");
        args = (void*) ROUND_UP((uintptr_t) args + sizeof(uint32_t), 4);
        // debug_printf("offsef is %d, still %d bytes left\n", args - return_args, args_size - (args - return_args));

        memcpy(args, names[i], service_list_name_size[i] * sizeof(char));
        // debug_printf("args = %s\n", args);
        args = (void*) ROUND_UP((uintptr_t) args + service_list_name_size[i], 4);
    }
    
    return return_args;            

}

errval_t ns_send_ep(void* args)
{
    // 1. Get channel to send down.
    struct lmp_chan* lc = (struct lmp_chan*) args;
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(struct lmp_chan), 4);

    bool found = *((bool*)args);
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(bool), 4);

    struct capref* ep = args;

    // 2. Send response
    if (found) {
        CHECK("lmp_chan_send simple_ok",
            lmp_chan_send1(lc, LMP_FLAG_SYNC, *ep, AOS_NS_OK));
    } else {
        CHECK("lmp_chan_send simple_ok",
            lmp_chan_send1(lc, LMP_FLAG_SYNC, NULL_CAP, AOS_NS_FAILED));    
    }
    
    return SYS_ERR_OK;
}

void* ns_process_lookup(struct lmp_chan* lc, struct lmp_recv_msg msg)
{
    struct nsclient_state* client = identify_client_by_local_cap(&lc->local_cap);
    if (client == NULL) {
        // Client already exists?
        debug_printf("Unable to identify client by local cap (lookup)\n");
        return NULL;
    }

    int rem_words = 7;
    int word_offset = 9 - rem_words;

    int length = msg.words[1];
    char* name = (char*)malloc(length * sizeof(char));
    char* message = (char*)malloc(rem_words * 4 * sizeof(char));
    char* message_offset = message;

    for (int i = 0; i < rem_words; i++) {
        // debug_printf("here %d\n", i);
        memcpy(message_offset, (char*)&(msg.words[word_offset + i]), 4);
        message_offset += 4;
    }

    strncpy(name, message, length);
    bool found = false;
    struct nsclient_state* aux = clients;

    while (aux != NULL) {
        if(aux->is_server && strcmp(aux->name, name) == 0) {
            found = true;
            break;
        }
        aux = aux->next;
    }
    // debug_printf("SERVER: received lookup request for service %s, %d\n", aux->name, aux->is_server);

    size_t args_size = ROUND_UP(sizeof(struct lmp_chan), 4)
        + ROUND_UP(sizeof(bool), 4)
        + ROUND_UP(sizeof(struct capref), 4);

    void* args = malloc(args_size);
    void* return_args = args;
    
    // 1. Channel to send down.
    *((struct lmp_chan*) args) = *lc;
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(struct lmp_chan), 4);
    
    // 2. Requested process name.
    *((bool*) args) = found;
    args = (void*) ROUND_UP((uintptr_t) args + sizeof(bool), 4);

    if (found) {
        *((struct capref*) args) = aux->service_ep;
    }

    // Response args.
    return return_args;
}


void ns_serve_rpc(void* arg)
{
    struct lmp_chan* lc = (struct lmp_chan*) arg;

    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;
    struct capref client_cap;

    errval_t err = lmp_chan_recv(lc, &msg, &client_cap);

    // Reregister.
    lmp_chan_alloc_recv_slot(lc);
    
    lmp_chan_register_recv(lc, get_default_waitset(),
            MKCLOSURE(ns_serve_rpc, arg));

    if (err_is_fail(err) && !lmp_err_is_transient(err)) {
        USER_PANIC_ERR(err, "lmp_chan_recv failed");
    }

    void* response_fn = NULL;
    void* response_args = NULL;

    switch (msg.words[0]) {
        
        case AOS_NS_HANDSHAKE:
            debug_printf("SERVER: serving ns_request with TAG = %s\n", "AOS_NS_HANDSHAKE");
            response_fn = (void*) ns_send_handshake;
            response_args = ns_process_handshake(&client_cap);
            break;

        case AOS_NS_REGISTER:
            debug_printf("SERVER: serving ns_request with TAG = %s\n", "AOS_NS_REGISTER");
            response_fn = (void*) ns_send_simple_ok;
            response_args = ns_process_register(lc, &client_cap, msg);
            break;
        
        case AOS_NS_DEREGISTER:
            debug_printf("SERVER: serving ns_request with TAG = %s\n", "AOS_NS_DEREGISTER");
            response_fn = (void*) ns_send_simple_ok;
            response_args = ns_process_deregister(lc);
            break;

        case AOS_NS_ENNUMERATE:
            debug_printf("SERVER: serving ns_request with TAG = %s\n", "AOS_NS_ENNUMERATE");
            response_fn = (void*) ns_send_service_list;
            // response_fn = (void*) ns_send_simple_ok;
            response_args = ns_process_enumerate(lc);
            break;
        
        case AOS_NS_LOOKUP:
            debug_printf("SERVER: serving ns_request with TAG = %s\n", "AOS_NS_LOOKUP");
            response_fn = (void*) ns_send_simple_ok;
            response_args = ns_process_lookup(lc, msg);
            break;

        default:
            debug_printf("WARNING: invalid NS RPC code\n");
    }


    if (response_fn != NULL && response_args != NULL) {
        struct lmp_chan* out = (struct lmp_chan*) response_args;
        err = lmp_chan_register_send(out, get_default_waitset(),
                MKCLOSURE(response_fn, response_args));
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "register send in ns_serve_rpc");
        }
    }
}


int main(int argc, char *argv[])
{
    // debug_printf("Hello, this is the nameserver process\n");
    
    struct lmp_chan lc = get_init_rpc()->lc;

    static struct aos_rpc new_aos_rpc;
    static struct waitset init_ws;
    waitset_init(&init_ws);
    CHECK("initializing new aos_rpc_init",
            aos_rpc_init(&new_aos_rpc, &init_ws));
    set_init_rpc(&new_aos_rpc);

    CHECK("creating NS channel slot", lmp_chan_alloc_recv_slot(&lc));

    CHECK("registering initial NS receive",
            lmp_chan_register_recv(&lc, get_default_waitset(),
                    MKCLOSURE(ns_serve_rpc, &lc)));
    
    // char* c = (char*) malloc(128 * 1024 * 1024);
    // *c = 'H';
    // sys_debug_flush_cache();
    // *(c + 64 * 1024 * 1024) = 'I';
    // sys_debug_flush_cache();

    // debug_printf("%c%c\n", *c, *(c + 64 * 1024 * 1024));


    domainid_t pid;

    errval_t err =  aos_rpc_process_spawn(get_init_rpc(), "service_a", 0, &pid);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "could not spawn a client process\n");
        return err;
    }
    // debug_printf("Pid for service_a is %u\n", pid);

    struct waitset *default_ws = get_default_waitset();

    while (true) {
        err = event_dispatch(default_ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }

    return 0;
}
