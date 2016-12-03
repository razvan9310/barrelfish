/**
 * \file
 * \brief RPC-URPC scheduler.
 */

/*
 * Copyright (c) 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.  If
 * you do not find this file, copies can be found by writing to: ETH Zurich
 * D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <string.h>

#include "rpc_server.h"
#include "scheduler.h"

errval_t process_urpc_task(struct scheduler* sc, struct urpc_task* task)
{
    errval_t err;
    struct ic_frame_node* client_frame;
    struct frame_identity client_frame_id;
    switch (task->type) {
        case UrpcOpType_Refill:
            // Please refill my ic frame buffer.x
            err = urpc_write_request(sc->urpc_buf, sc->my_core_id,
                    URPC_CODE_REFILL, 0, NULL);
            break;
        case UrpcOpType_Channel:
            // Please connect this client to your server.
            client_frame = task->client->client_frame;
            CHECK("identifying client frame to create URPC channel request",
                    frame_identify(client_frame->frame, &client_frame_id));
            
            uint64_t msg[2];
            msg[0] = (uint64_t) client_frame_id.base;
            msg[1] = (uint64_t) client_frame_id.bytes;

            err = urpc_write_request(sc->urpc_buf, sc->my_core_id,
                    URPC_CODE_CHANNEL, sizeof(msg[0]) + sizeof(msg[1]), msg);
            break;
        default:
            return URPC_ERR_INVALID_CODE;
    }

    if (err_is_fail(err)) {
        // Error means we can't send a new request atm, because the previous one
        // hasn't completely been responded to.
        // Therefore we re-enqueue the task too look into it again later.
        add_urpc_task(sc, task);
    }

    return SYS_ERR_OK;
}

void add_urpc_task(struct scheduler* sc, struct urpc_task* task) 
{
    task->prev = NULL;
    if (sc->urpc_queue == NULL) {
        task->next = NULL;
        sc->tail_urpc_queue = task;
        sc->tail_urpc_queue->next = NULL;
        sc->tail_urpc_queue->prev = NULL;
    } else {
        task->next = sc->urpc_queue;
        sc->urpc_queue->prev = task;
    }
    sc->urpc_queue = task;
}

struct urpc_task* pop_urpc_task(struct scheduler* sc)
{
    if (sc->urpc_queue == NULL) {
        // printf("Careful, URPC queue is NULL\n");
        return NULL;
    }

    struct urpc_task* task = sc->urpc_queue;
    sc->urpc_queue = sc->urpc_queue->next;
    return task;
}

errval_t process_rpc_task(struct scheduler* sc, struct rpc_task* task)
{
    if (task->status == RpcStatus_New) {
        struct capref client_cap;
        if (try_serving_locally(sc, task, &task->msg, &client_cap)) {
            return SYS_ERR_OK;
        }

        struct client_state* client = identify_client(&client_cap,
                sc->local_clients);
        if (client == NULL) {
            return INIT_ERR_RPC_CANNOT_SERVE;
        }
        task->client = client;

        if (client->client_frame == NULL) {
            task->status = RpcStatus_Allocate_Cframe;
        } else if (client->server_frame == NULL) {
            task->status = RpcStatus_Bind_Sframe;
        } else {
            task->status = RpcStatus_Write_Cframe;
        }
    }
    if (task->status == RpcStatus_Allocate_Cframe) {
        if (should_refill_ic_frame_buffer(sc)) {
            refill_ic_frame_buffer(sc);
            add_rpc_task(sc, task);
            return SYS_ERR_OK;
        } else if (sc->is_refilling_buffer) {
            // We're not done refilling, nothing to do but return.
            add_rpc_task(sc, task);
            return SYS_ERR_OK;
        }
        task->client->client_frame = allocate_ic_frame_node(sc);

        task->status = RpcStatus_Bind_Sframe;
    }
    if (task->status == RpcStatus_Bind_Sframe) {
        if (task->client->server_frame != NULL) {
            task->status = RpcStatus_Write_Cframe;
        } else {
            if (!task->client->binding_in_progress) {
                struct urpc_task* urpc = (struct urpc_task*) malloc(
                        sizeof(struct urpc_task*));
                urpc->type = UrpcOpType_Channel;
                urpc->client = task->client;
                add_urpc_task(sc, urpc);

                task->client->binding_in_progress = true;
            }
            add_rpc_task(sc, task);
            return SYS_ERR_OK;
        }
    }
    if (task->status == RpcStatus_Write_Cframe) {
        size_t req_size;
        uint32_t remaining;
        size_t stop;

        errval_t err = SYS_ERR_OK;
        switch (task->msg.words[0]) {
            case AOS_RPC_MEMORY:
                req_size = (size_t) task->msg.words[1];
                if (req_size + task->client->ram >= MAX_CLIENT_RAM) {
                    // Limit to MAX_CLIENT_RAM.
                    req_size = MAX_CLIENT_RAM - task->client->ram;
                }
                err = cross_core_rpc_write_request(
                        task->client->client_frame->addr,
                        AOS_RPC_MEMORY,
                        sizeof(size_t),
                        &req_size);
                break;
            case AOS_RPC_PUTCHAR:
                err = cross_core_rpc_write_request(
                        task->client->client_frame->addr,
                        AOS_RPC_PUTCHAR,
                        sizeof(char),
                        &task->msg.words[1]);
                break;
            case AOS_RPC_GETCHAR:
                err = cross_core_rpc_write_request(
                        task->client->client_frame->addr,
                        AOS_RPC_GETCHAR,
                        0,
                        NULL);
                break;
            case AOS_RPC_STRING:
                remaining = task->msg.words[2];
                if (task->client->str_buf == NULL) {
                    task->client->str_buf = (char*) malloc(
                            remaining * sizeof(char));
                    task->client->str_buf_idx = 0;
                }

                stop = remaining < 24 ? remaining : 24;
                for (size_t i = 0; i < stop; ++i) {
                    uint32_t word = task->msg.words[3 + i / 4];
                    task->client->str_buf[task->client->str_buf_idx++] =
                            (char) (word >> (8 * (i % 4)));
                }

                remaining -= stop;
                if (remaining == 0) {
                    // Write to c-frame and reset.
                    err = cross_core_rpc_write_request(
                            task->client->client_frame->addr,
                            AOS_RPC_STRING, task->client->str_buf_idx,
                            task->client->str_buf);
                    task->client->str_buf_idx = 0;
                    free(task->client->str_buf);
                    task->client->str_buf = NULL;
                } else {
                    // Send ok to client, will keep receiving the rest of the
                    // string.
                    CHECK("register send back to local client, for string",
                            lmp_chan_register_send(&task->client->lc,
                                    get_default_waitset(),
                                    MKCLOSURE((void*) send_simple_ok,
                                            (void*) &task->client->lc)));

                    return SYS_ERR_OK;
                }
                break;
            case AOS_RPC_SPAWN:
                remaining = task->msg.words[2];
                if (task->client->spawn_buf == NULL) {
                    task->client->spawn_buf = (char*) malloc(
                            remaining * sizeof(char));
                    task->client->spawn_buf_idx = 0;
                }

                stop = remaining < 24 ? remaining : 24;
                for (size_t i = 0; i < stop; ++i) {
                    uint32_t word = task->msg.words[3 + i / 4];
                    task->client->spawn_buf[task->client->spawn_buf_idx++] =
                            (char) (word >> (8 * (i % 4)));
                }

                remaining -= stop;
                if (remaining == 0) {
                    // Write to c-frame and reset.
                    err = cross_core_rpc_write_request(
                            task->client->client_frame->addr,
                            AOS_RPC_SPAWN, task->client->spawn_buf_idx,
                            task->client->spawn_buf);
                    task->client->spawn_buf_idx = 0;
                    free(task->client->spawn_buf);
                    task->client->spawn_buf = NULL;
                } else {
                    // Send ok to client, will keep receiving the rest of the
                    // string.
                    CHECK("register send back to local client, for spawn proc",
                            lmp_chan_register_send(&task->client->lc,
                                    get_default_waitset(),
                                    MKCLOSURE((void*) send_simple_ok,
                                            (void*) &task->client->lc)));

                    return SYS_ERR_OK;
                }
                break;
            case AOS_RPC_GET_PNAME:
                err = cross_core_rpc_write_request(
                        task->client->client_frame->addr,
                        AOS_RPC_GET_PNAME,
                        sizeof(domainid_t),
                        &task->msg.words[2]);
                break;
            case AOS_RPC_GET_PLIST:
                err = cross_core_rpc_write_request(
                        task->client->client_frame->addr,
                        AOS_RPC_GET_PLIST,
                        0,
                        NULL);
                break;
        }

        if (err_is_fail(err)) {
            // Error means we can't send a new request atm, because the previous
            // one hasn't completely been responded to.
            // Therefore we re-enqueue the task too look into it again later.
            add_rpc_task(sc, task);
        }

        return SYS_ERR_OK;
    } else {
        debug_printf("Invalid RPC task status: %u\n", task->status);
        return INIT_ERR_RPC_INVALID_STATUS;
    }
}

void add_rpc_task(struct scheduler* sc, struct rpc_task* task) 
{
    task->prev = NULL;
    if (sc->rpc_queue == NULL) {
        task->next = NULL;
        sc->tail_rpc_queue = task;
        sc->tail_rpc_queue->next = NULL;
        sc->tail_rpc_queue->prev = NULL;
    } else {
        task->next = sc->rpc_queue;
        sc->rpc_queue->prev = task;
    }
    sc->rpc_queue = task;
}

struct rpc_task* pop_rpc_task(struct scheduler* sc)
{
    if (sc->rpc_queue == NULL) {
        // printf("Careful, RPC queue is NULL\n");
        return NULL;
    }

    struct rpc_task* task = sc->rpc_queue;
    sc->rpc_queue = sc->rpc_queue->next;
    return task;
}

errval_t refill_ic_frame_buffer(struct scheduler* sc)
{
    if (sc->my_core_id == 0) {
        // We can just frame_alloc.
        struct capref frames[IC_FRAME_BUF_CAPACITY];
        size_t retsize;
        for (size_t i = 0; i < IC_FRAME_BUF_CAPACITY; ++i) {
            CHECK("allocating ic frame for refill",
                    frame_alloc(&frames[i], BASE_PAGE_SIZE, &retsize));
            
            struct ic_frame_node* node = (struct ic_frame_node*) malloc(
                    sizeof(struct ic_frame_node));
            node->frame = frames[i];
            node->free = true;
            node->addr = NULL;

            if (sc->tail_ic_frame_list == NULL) {
                sc->tail_ic_frame_list = sc->ic_frame_list = node;
                sc->tail_ic_frame_list->next = sc->ic_frame_list->next = NULL;
                sc->tail_ic_frame_list->prev = sc->ic_frame_list->prev = NULL;
            } else {
                sc->tail_ic_frame_list->next = node;
                node->prev = sc->tail_ic_frame_list;
                sc->tail_ic_frame_list = node;
            }
        }
        sc->ic_frame_buf_size += IC_FRAME_BUF_CAPACITY;
    } else {
        // Need to enqueue a URPC refill task.
        sc->is_refilling_buffer = true;
        // This can be static as no two refill requests sent by the same client
        // will be alive at any given time.
        static struct urpc_task refill_task;
        refill_task.type = UrpcOpType_Refill;
        add_urpc_task(sc, &refill_task);
    }

    return SYS_ERR_OK;
}

struct ic_frame_node* allocate_ic_frame_node(struct scheduler* sc)
{
    assert(sc->ic_frame_buf_size > 0);
    sc->ic_frame_buf_size--;
    struct ic_frame_node* node = sc->tail_ic_frame_list;
    while (node != NULL) {
        if (node->free) {
            node->free = false;
            return node;
        }
        node = node->prev;
    }
    return NULL;  // This should never happen O_O.
}

errval_t bind_remote_client(struct scheduler* sc, void* bind_req,
        struct ic_frame_node* server_frame)
{
    genpaddr_t* base = (genpaddr_t*) bind_req;
    bind_req += sizeof(genpaddr_t);
    gensize_t* size = (gensize_t*) bind_req;

    struct ic_frame_node* client_frame = (struct ic_frame_node*) malloc(
            sizeof(struct ic_frame_node));
    CHECK("allocating slot for c-frame", slot_alloc(&client_frame->frame));
    CHECK("forging remote c-frame cap",
            frame_forge(client_frame->frame, *base, *size, sc->my_core_id));

    cross_core_rpc_init(&client_frame->frame,
            &server_frame->frame,
            &client_frame->addr,
            &server_frame->addr);
    struct client_state* remote_client = (struct client_state*) malloc(
            sizeof(struct client_state));
    remote_client->client_frame = client_frame;
    remote_client->server_frame = server_frame;
    
    if (sc->remote_clients == NULL) {
        remote_client->prev = remote_client->next = NULL;
    } else {
        sc->remote_clients->prev = remote_client;
        remote_client->next = sc->remote_clients;
        sc->remote_clients = remote_client;
    }
    sc->remote_clients = remote_client;

    return SYS_ERR_OK;
}

errval_t process_urpc_request(struct scheduler* sc, uint32_t code,
        size_t req_len, void* req, size_t* resp_len, void** resp)
{
    switch (code) {
        case URPC_CODE_REFILL:
            // Other core asked us to refill its ic frame buffer.
            if (sc->my_core_id != 0) {
                return URPC_ERR_REFILL;
            }

            static uint64_t refill_msg[2 * IC_FRAME_BUF_CAPACITY];
            for (size_t i = 0; i < IC_FRAME_BUF_CAPACITY; ++i) {
                struct capref frame;
                size_t retsize;
                CHECK("process_urpc_request refill frame_alloc",
                        frame_alloc(&frame, BASE_PAGE_SIZE, &retsize));
                struct frame_identity frame_id;
                CHECK("identifying urpc_request refill frame",
                        frame_identify(frame, &frame_id));
                refill_msg[2 * i] = (uint64_t) frame_id.base;
                refill_msg[2 * i + 1] = (uint64_t) frame_id.bytes;
            }

            *resp_len = (sizeof(genpaddr_t) + sizeof(gensize_t))
                    * IC_FRAME_BUF_CAPACITY;
            *resp = refill_msg;
            break;
        case URPC_CODE_CHANNEL:
            // Other core gave us a client to bind to our server.
            if (req_len != sizeof(genpaddr_t) + sizeof(gensize_t)) {
                return URPC_ERR_SETUP_CHANNEL;
            }

            if (should_refill_ic_frame_buffer(sc)) {
                refill_ic_frame_buffer(sc);
                return URPC_ERR_FRAME_BUFFER;
            } else if (sc->is_refilling_buffer) {
                return URPC_ERR_FRAME_BUFFER;
            }

            struct ic_frame_node* server_frame = allocate_ic_frame_node(sc);
            CHECK("binding other core's client to my server",
                    bind_remote_client(sc, req, server_frame));

            struct frame_identity frame_id;
            CHECK("identifying urpc_request s-frame for binding",
                    frame_identify(server_frame->frame, &frame_id));

            static uint64_t bind_msg[4];
            bind_msg[0] = *((uint64_t*) req);
            bind_msg[1] = *((uint64_t*) (req + sizeof(uint64_t)));
            bind_msg[2] = (uint64_t) frame_id.base;
            bind_msg[3] = (uint64_t) frame_id.bytes;

            *resp_len = (sizeof(genpaddr_t) + sizeof(gensize_t)) * 2;
            *resp = bind_msg;
            break;
        default:
            return URPC_ERR_INVALID_CODE;
    }

    return SYS_ERR_OK;
}

errval_t process_urpc_response(struct scheduler* sc, uint32_t code,
        size_t resp_len, void* resp)
{
    switch (code) {
        case URPC_CODE_REFILL:
            assert(resp_len == (sizeof(genpaddr_t) + sizeof(gensize_t))
                    * IC_FRAME_BUF_CAPACITY);
            // We got a bunch of buffer frames for our c- and s-frames.
            for (size_t i = 0; i < IC_FRAME_BUF_CAPACITY; ++i) {
                genpaddr_t* base = (genpaddr_t*) resp;
                resp += sizeof(genpaddr_t);
                gensize_t* size = (gensize_t*) resp;
                resp += sizeof(gensize_t);

                struct capref frame;
                CHECK("allocating slot for refill frame",
                        slot_alloc(&frame));
                CHECK("forging refill frame",
                        frame_forge(frame, *base, *size, sc->my_core_id));

                struct ic_frame_node* node = (struct ic_frame_node*) malloc(
                    sizeof(struct ic_frame_node));
                node->frame = frame;
                node->free = true;
                node->addr = NULL;

                if (sc->tail_ic_frame_list == NULL) {
                    sc->tail_ic_frame_list = sc->ic_frame_list = node;
                    sc->tail_ic_frame_list->next = sc->ic_frame_list->next =
                            NULL;
                    sc->tail_ic_frame_list->prev = sc->ic_frame_list->prev =
                            NULL;
                } else {
                    sc->tail_ic_frame_list->next = node;
                    node->prev = sc->tail_ic_frame_list;
                    sc->tail_ic_frame_list = node;
                }
            }
            sc->ic_frame_buf_size += IC_FRAME_BUF_CAPACITY;
            sc->is_refilling_buffer = false;
            break;
        case URPC_CODE_CHANNEL:
            assert(resp_len == (sizeof(genpaddr_t) + sizeof(gensize_t)) * 2);
            // Other core responded to our binding request with its s-frame.
            genpaddr_t* client_base = (genpaddr_t*) resp;
            resp += sizeof(genpaddr_t);
            gensize_t* client_size = (gensize_t*) resp;
            resp += sizeof(gensize_t);

            genpaddr_t* server_base = (genpaddr_t*) resp;
            resp += sizeof(genpaddr_t);
            gensize_t* server_size = (gensize_t*) resp;

            struct client_state* local_client = sc->local_clients;
            while (local_client != NULL) {
                struct frame_identity frame_id;
                CHECK("identifying c-frame to match with s-frame",
                        frame_identify(local_client->client_frame->frame,
                                &frame_id));
                if (frame_id.base == *client_base
                        && frame_id.bytes == *client_size) {
                    break;
                }
                local_client = local_client->next;
            }
            if (local_client == NULL) {
                return URPC_ERR_INVALID_CLIENT;
            }

            local_client->server_frame = (struct ic_frame_node*) malloc(
                    sizeof(struct ic_frame_node));
            CHECK("allocating slot for s-frame", slot_alloc(
                    &local_client->server_frame->frame));
            CHECK("forging s-frame frame",
                    frame_forge(local_client->server_frame->frame, *server_base,
                            *server_size, sc->my_core_id));

            cross_core_rpc_init(&local_client->client_frame->frame,
                    &local_client->server_frame->frame,
                    &local_client->client_frame->addr,
                    &local_client->server_frame->addr);
                    break;
            
            local_client->binding_in_progress = false;
        default:
            return URPC_ERR_INVALID_CODE;
    }

    return SYS_ERR_OK;
}

errval_t check_task_urpc(struct scheduler* sc) {
    // 1. Check if the other core has sent us a new request as a client.
    uint32_t code;
    size_t req_len;
    void* req;
    errval_t err = urpc_read_request(sc->urpc_buf, 1 - sc->my_core_id, &code,
            &req_len, &req);
    if (err_is_ok(err)) {
        // There's a request we can process & respond to.
        size_t resp_len;
        void* resp;
        err = process_urpc_request(sc, code, req_len, req, &resp_len, &resp);
        if (err_is_ok(err)) {
            CHECK("writing URPC response",
                    urpc_write_response(sc->urpc_buf, 1 - sc->my_core_id, code,
                            resp_len, resp));
        }  else if (err != URPC_ERR_REFILL && err != URPC_ERR_SETUP_CHANNEL) {
            // We failed to process the request, hence we need to re-mark it as
            // unread.
            urpc_mark_request_unread(sc->urpc_buf, 1 - sc->my_core_id);
        }
    }

    // 2. Check if a former request of ours has been responded to.
    size_t resp_len;
    void* resp;
    err = urpc_read_response(sc->urpc_buf, sc->my_core_id, &code, &resp_len,
            &resp);
    if (err_is_ok(err)) {
        // We have a response.
        CHECK("processing URPC response",
                process_urpc_response(sc, code, resp_len, resp));
    }

    // 3. Check if there are any pending URPC tasks to create new requests for.
    struct urpc_task* task = pop_urpc_task(sc);
    if (task != NULL) {
        // There's a pending task, create requests for it.
        CHECK("processing URPC task", process_urpc_task(sc, task));
    }

    return SYS_ERR_OK;
}

errval_t process_rpc_request(struct scheduler* sc, uint32_t code,
        size_t req_len, void* req, size_t* resp_len, void** resp)
{
    struct capref ram;
    size_t retsize;
    struct capref frame;
    struct frame_identity frame_id;
    errval_t err;
    domainid_t pid;
    size_t num_pids;

    switch (code) {
        case AOS_RPC_MEMORY:
            if (sc->my_core_id != 0) {
                return INIT_ERR_RPC_CANNOT_SERVE;
            }

            CHECK("rpc_ram_alloc",
                    rpc_ram_alloc(&ram, *((size_t*) req), &retsize));

            CHECK("allocating slot for corss-core mem request frame",
                    slot_alloc(&frame));

            CHECK("retyping RAM into frame to identify",
                    cap_retype(frame, ram, 0, ObjType_Frame, retsize, 1));

            CHECK("identifying frame for cross-core mem request",
                    frame_identify(frame, &frame_id));

            CHECK("destroying frame cap", cap_destroy(frame));

            *resp_len = sizeof(genpaddr_t) + sizeof(gensize_t);
            *resp = malloc(*resp_len);
            *((genpaddr_t*) *resp) = frame_id.base;
            *((gensize_t*) (*resp + sizeof(genpaddr_t))) = frame_id.bytes;
            break;
        case AOS_RPC_PUTCHAR:
            if (sc->my_core_id != 0) {
                return INIT_ERR_RPC_CANNOT_SERVE;
            }
            rpc_putchar((char*) req);
            *resp_len = 0;
            break;
        case AOS_RPC_GETCHAR:
            if (sc->my_core_id != 0) {
                return INIT_ERR_RPC_CANNOT_SERVE;
            }
            *resp_len = sizeof(char);
            *resp = malloc(*resp_len);
            *((char*) *resp) = rpc_getchar();
            break;
        case AOS_RPC_STRING:
            if (sc->my_core_id != 0) {
                return INIT_ERR_RPC_CANNOT_SERVE;
            }
            rpc_string((char*) req, req_len);
            *resp_len = 0;
            break;
        case AOS_RPC_SPAWN:
            err = rpc_spawn((char*) req, &pid);
            *resp_len = sizeof(errval_t) + sizeof(domainid_t);
            *resp = malloc(*resp_len);
            *((errval_t*) *resp) = err;
            *((domainid_t*) (*resp + sizeof(errval_t))) = pid;
            break;
        case AOS_RPC_GET_PNAME:
            *resp = rpc_process_name(*((domainid_t*) req), resp_len);
            break;
        case AOS_RPC_GET_PLIST:
            *resp = rpc_process_list(&num_pids);
            *resp_len = num_pids * sizeof(domainid_t);
            break;
        default:
            return INIT_ERR_RPC_INVALID_CODE;
    }

    return SYS_ERR_OK;
}

errval_t process_rpc_response(struct scheduler* sc, uint32_t code,
        size_t resp_len, void* resp, struct client_state* client,
        void** local_response_fn, void** local_response)
{
    genpaddr_t* base;
    gensize_t* size;
    struct capref ram;
    errval_t err;
    size_t args_size;
    void *aux;

    switch (code) {
        case AOS_RPC_MEMORY:
            assert(resp_len == sizeof(genpaddr_t) + sizeof(gensize_t));
            base = (genpaddr_t*) resp;
            resp += sizeof(genpaddr_t);
            size = (gensize_t*) resp;

            CHECK("allocating slot for ram from core 0", slot_alloc(&ram));
            err = ram_forge(ram, *base, *size, sc->my_core_id);

            // Response args.
            args_size = ROUND_UP(sizeof(struct lmp_chan), 4)
                    + ROUND_UP(sizeof(errval_t), 4)
                    + ROUND_UP(sizeof(struct capref), 4)
                    + sizeof(size_t);
            *local_response = malloc(args_size);
            aux = *local_response;
    
            // 1. Channel to send down.
            *((struct lmp_chan*) aux) = client->lc;
    
            // 2. Error code from frame_forge.
            aux = (void*) ROUND_UP((uintptr_t) aux + sizeof(struct lmp_chan), 4);
            *((errval_t*) aux) = err;

            // 3. Cap for newly allocated RAM.
            aux = (void*) ROUND_UP((uintptr_t) aux + sizeof(errval_t), 4);
            *((struct capref*) aux) = ram;

            // 4. Size returned by ram_alloc.
            aux = (void*) ROUND_UP((uintptr_t) aux + sizeof(struct capref), 4);
            *((size_t*) aux) = *size;

            *local_response_fn = (void*) send_memory;
            break;
        case AOS_RPC_PUTCHAR:
        case AOS_RPC_STRING:
            *local_response = &client->lc;
            *local_response_fn = (void*) send_simple_ok; 
            break;
        case AOS_RPC_GETCHAR:
            args_size = ROUND_UP(sizeof(struct lmp_chan), 4)
                    + ROUND_UP(sizeof(char), 4);
            *local_response = malloc(args_size);
            aux = *local_response;
            
            // 1. Channel to send down.
            *((struct lmp_chan*) aux) = client->lc;

            // 2. Character returned by sys_getchar
            aux = (void*) ROUND_UP((uintptr_t) aux + sizeof(struct lmp_chan), 4);
            *((char*) aux) = *((char*) resp);

            *local_response_fn = (void*) send_serial_getchar;
            break;
        case AOS_RPC_SPAWN:
            args_size = ROUND_UP(sizeof(struct lmp_chan), 4)
                    + ROUND_UP(sizeof(errval_t), 4)
                    + ROUND_UP(sizeof(domainid_t), 4);
            *local_response = malloc(args_size);
            aux = *local_response;

            // 1. Channel to send down.
            *((struct lmp_chan*) aux) = client->lc;

            // 2. Error code returned by spawn.
            aux = (void*) ROUND_UP((uintptr_t) aux + sizeof(struct lmp_chan), 4);
            *((errval_t*) aux) = *((errval_t*) resp);

            // 3. PID of the newly spawned process.
            aux = (void*) ROUND_UP((uintptr_t) aux + sizeof(errval_t), 4);
            *((domainid_t*) aux) = *((domainid_t*) (resp + sizeof(errval_t)));

            *local_response_fn = (void*) send_pid;
            break;
        case AOS_RPC_GET_PNAME:
            args_size = ROUND_UP(sizeof(struct lmp_chan), 4)
                    + ROUND_UP(resp_len, 4);
            *local_response = malloc(args_size);
            aux = *local_response;

            // 1. Channel to send down.
            *((struct lmp_chan*) aux) = client->lc;

            // 2. Requested process name.
            aux = (void*) ROUND_UP((uintptr_t) aux + sizeof(struct lmp_chan), 4);
            strncpy((char*) aux, (char*) resp, resp_len);

            *local_response_fn = (void*) send_process_name;
            break;
        case AOS_RPC_GET_PLIST:
            args_size = ROUND_UP(sizeof(struct lmp_chan), 4)
                    + ROUND_UP(sizeof(uint32_t), 4) 
                    + ROUND_UP(resp_len, 4);
            *local_response = malloc(args_size);
            aux = *local_response;

            // 1. Channel to send down.
            *((struct lmp_chan*) aux) = client->lc;

            // 2. Size of PIDs list.
            aux = (void*) ROUND_UP((uintptr_t) aux + sizeof(struct lmp_chan), 4);
            *((uint32_t*) aux) = resp_len / sizeof(domainid_t);

            // 3. List of PIDs.
            aux = (void*) ROUND_UP((uintptr_t) aux + sizeof(uint32_t), 4);
            memcpy(aux, resp, resp_len);

            *local_response_fn = (void*) send_ps_list;
            break;
        default:
            return INIT_ERR_RPC_INVALID_CODE;
    }

    return SYS_ERR_OK;
}

errval_t check_task_rpc(struct scheduler* sc)
{
    // 1. Check for any new requests from remote clients, i.e. clients running
    // on the other core.
    struct client_state* remote_client = sc->remote_clients;
    while (remote_client != NULL) {
        if (remote_client->client_frame == NULL) {
            // Hasn't been yet initialized, continue.
            remote_client = remote_client->next;
            continue;
        }

        uint32_t code;
        size_t req_len;
        void* req;
        errval_t err = cross_core_rpc_read_request(
                remote_client->client_frame->addr,
                &code,
                &req_len,
                &req);

        if (err_is_ok(err)) {
            // Got a new request, try to serve it.
            size_t resp_len;
            void* resp;
            CHECK("processing cross-core RPC request",
                    process_rpc_request(sc, code, req_len, req, &resp_len,
                            &resp));
            CHECK("writing cross-core RPC response",
                    cross_core_rpc_write_response(
                            remote_client->server_frame->addr,
                            code,
                            resp_len,
                            resp));
        }

        remote_client = remote_client->next;
    }

    // 2. Check if any cross-core requests performed by local clients have been
    // responded to by the other core.
    struct client_state* local_client = sc->local_clients;
    while (local_client != NULL) {
        if (local_client->server_frame == NULL) {
            // Hasn't been yet initialized, continue.
            local_client = local_client->next;
            continue;
        }
        uint32_t code;
        size_t resp_len;
        void* resp;
        errval_t err = cross_core_rpc_read_response(
                local_client->server_frame->addr,
                &code,
                &resp_len,
                &resp);
        if (err_is_ok(err)) {
            // Got response for a previous request.
            void* local_response_fn;
            void* local_response;
            CHECK("processing cross-core RPC response",
                    process_rpc_response(sc, code, resp_len, resp,
                            local_client, &local_response_fn, &local_response));

            // Send response to local client.
            struct lmp_chan* out = (struct lmp_chan*) local_response;
            CHECK("register send back to local client",
                    lmp_chan_register_send(out, get_default_waitset(),
                            MKCLOSURE(local_response_fn, local_response)));  
        }

        local_client = local_client->next;
    }

    // 3. Pop recent local RPC tasks (events) from the default waitset.
    struct waitset *default_ws = get_default_waitset();
    errval_t err;
    size_t task_count = 0;
    do {
        err = check_for_event(default_ws);
        if (err_is_ok(err)) {
            // We got an event => pop and queue-up in our RPC queue.
            struct event_closure retclosure;
            CHECK("popping next RPC event from default_ws",
                    get_next_event(default_ws, &retclosure));

            struct rpc_task* task = (struct rpc_task*) malloc(
                    sizeof(struct rpc_task));
            task->status = RpcStatus_New;
            task->closure = retclosure;
            task->client = NULL;
            add_rpc_task(sc, task);
            ++task_count;
        } else {
            break;
        }
    } while (task_count < RPC_TASK_LIMIT);

    // 4. Process local tasks.
    for (size_t i = 0; i < RPC_TASK_LIMIT; ++i) {
        struct rpc_task* task = pop_rpc_task(sc);
        if (task == NULL) {
            break;
        }
        CHECK("processing RPC task", process_rpc_task(sc, task));
    }

    return SYS_ERR_OK;
}

bool try_serving_locally(struct scheduler* sc, struct rpc_task* task,
        struct lmp_recv_msg* msg, struct capref* client_cap)
{
    struct lmp_chan* lc = (struct lmp_chan*) task->closure.arg;
    struct lmp_recv_msg aux_msg = LMP_RECV_MSG_INIT;
    *msg = aux_msg;
    
    lmp_chan_recv(lc, msg, client_cap);

    // Reregister.
    lmp_chan_alloc_recv_slot(lc);
    lmp_chan_register_recv(lc, get_default_waitset(), task->closure);

    switch (msg->words[0]) {
        case AOS_RPC_OK:
        case AOS_RPC_FAILED:
            task->closure.handler(task->closure.arg);
            // lmp_chan_send1(lc, LMP_FLAG_SYNC, NULL_CAP, msg->words[0]);
            return true;
        case AOS_RPC_DEVICE:
            break;
        case AOS_RPC_MEMORY:
        case AOS_RPC_STRING:
        case AOS_RPC_PUTCHAR:
        case AOS_RPC_GETCHAR:
            if (sc->my_core_id != 0) {
                return false;
            }
            break;
        default:
            if (msg->words[0] != AOS_RPC_HANDSHAKE
                    && sc->my_core_id != msg->words[1]) {
                return false;
            }
            break;
    }

    // RPC can be solved locally, do it.
    // TODO: Change the outer function to return error instead of bool.
    serve_locally(msg, client_cap, &sc->local_clients);

    return true;
}

void dummy_closure(void* dummy_arg);
void dummy_closure(void* dummy_arg) {

}

errval_t scheduler_start(struct scheduler* sc, struct lmp_chan* lc)
{
    CHECK("lmp_chan_register_recv child",
            lmp_chan_register_recv(lc, get_default_waitset(),
                    MKCLOSURE(dummy_closure, lc)));
    while (true) {
        CHECK("check_task_urpc", check_task_urpc(sc));
        CHECK("check_task_rpc", check_task_rpc(sc));
    }
}

void scheduler_init(struct scheduler* sc, coreid_t my_core_id, void* urpc_buf) {
    sc->my_core_id = my_core_id;
    sc->urpc_buf = urpc_buf;

    sc->rpc_queue = sc->tail_rpc_queue = NULL;
    sc->urpc_queue = sc->tail_urpc_queue = NULL;
    
    sc->ic_frame_buf_size = 0;
    sc->is_refilling_buffer = false;
    
    sc->ic_frame_list = NULL;
    sc->tail_ic_frame_list = NULL;

    sc->local_clients = sc->remote_clients = NULL;
}
