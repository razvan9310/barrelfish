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

#ifndef _INIT_SCHEDULER_H_
#define _INIT_SCHEDULER_H_

#include <aos/aos.h>
#include <aos/waitset.h>
#include <urpc/urpc.h>

#include "cross_core_rpc.h"

#define IC_FRAME_BUF_CAPACITY 10u
#define RPC_TASK_LIMIT 5

enum UrpcOpType {
	UrpcOpType_Refill,
	UrpcOpType_Channel
};

enum RpcStatus {
	RpcStatus_New,
	RpcStatus_Allocate_Cframe,
    RpcStatus_Bind_Sframe,
	RpcStatus_Write_Cframe
};

struct scheduler {
	coreid_t my_core_id;  // ID of the core this scheduler is running on.
	void* urpc_buf;       // URPC buffer, used to communicate with other core.

    // Inter-core channel buffer frames, for allocating c- and s-frames.
    size_t ic_frame_buf_size;
    struct ic_frame_node* ic_frame_list;
    struct ic_frame_node* tail_ic_frame_list;
    bool is_refilling_buffer;

    // RPC task queue.
	struct rpc_task* rpc_queue;
	struct rpc_task* tail_rpc_queue;

    // URPC task queue.
	struct urpc_task* urpc_queue;
	struct urpc_task* tail_urpc_queue;

    struct client_state* local_clients;   // List of clients on this core.
    struct client_state* remote_clients;  // List of clients on the other core.
};

struct urpc_task {
	enum UrpcOpType type;
	struct client_state* client;

	struct urpc_task* next;
	struct urpc_task* prev;

}; 

struct rpc_task {
    enum RpcStatus status;

    struct lmp_recv_msg msg;

	struct client_state* client;
    struct event_closure closure;

	struct rpc_task* next;
	struct rpc_task* prev;
};

/**
 * \brief Whether the inter-core channel frame buffer should be refilled.
 */
static inline bool should_refill_ic_frame_buffer(struct scheduler* sc)
{
    return sc->ic_frame_buf_size == 0 && !sc->is_refilling_buffer;
}

/**
 * \brief Processes a URPC task by writing the appropriate request into the
 * shared URPC buffer.
 */
errval_t process_urpc_task(struct scheduler* sc, struct urpc_task* task);
/**
 * \brief Enqueues a new URPC task.
 */
void add_urpc_task(struct scheduler* sc, struct urpc_task* task);
/**
 * \brief Returns the next URPC task from the queue, or NULL if there is none.
 */
struct urpc_task* pop_urpc_task(struct scheduler* sc);

/**
 * \brief Processes an RPC task.
 */
errval_t process_rpc_task(struct scheduler* sc, struct rpc_task* task);
/**
 * \brief Enqueues a new RPC task.
 */
void add_rpc_task(struct scheduler* sc, struct rpc_task* task);
/**
 * \brief Returns the next RPC task from the queue, or NULL if there is none.
 */
struct rpc_task* pop_rpc_task(struct scheduler* sc);

/**
 * \brief Refills the inter-core channel frame buffer. Core 0 just mallocs,
 * whereas core1 enqueues a buffer refill URPC task.
 */
errval_t refill_ic_frame_buffer(struct scheduler* sc);
/**
 * \brief Allocates a new inter-core channel frame node using the scheduler's
 * frame buffer.
 */
struct ic_frame_node* allocate_ic_frame_node(struct scheduler* sc);
/**
 * \brief Binds a remote client to the server on this core, using the given
 * s-frame. The client is identified by its c-frame, whose base and size are
 * marshaled into the given bind_req buffer.
 */
errval_t bind_remote_client(struct scheduler* sc, void* bind_req,
        struct ic_frame_node* server_frame);

/**
 * \brief Processes a request received via the shared URPC buffer.
 */
errval_t process_urpc_request(struct scheduler* sc, uint32_t code,
        size_t req_len, void* req, size_t* resp_len, void** resp);
/**
 * \brief Processes a response received via the shared URPC buffer.
 */
errval_t process_urpc_response(struct scheduler* sc, uint32_t code,
        size_t resp_len, void* resp);

/**
 * \brief Processes an RPC request received from another core.
 */
errval_t process_rpc_request(struct scheduler* sc, uint32_t code,
        size_t req_len, void* req, size_t* resp_len, void** resp);
/**
 * \brief Processes an RPC response received from another core.
 */
errval_t process_rpc_response(struct scheduler* sc, uint32_t code,
        size_t resp_len, void* resp, struct client_state* client,
        void** local_response_fn, void** local_response);

/**
 * \brief Checks if there is any available URPC task this core can perform.
 */
errval_t check_task_urpc(struct scheduler* sc);
/**
 * \brief Checks if there is any available RPC task this core can perform.
 */
errval_t check_task_rpc(struct scheduler* sc);

/**
 * \brief Attempts to serve a request locally (on the same core). Also fills in
 * LMP message-related fields. Returns true iff succeeded.
 */
bool try_serving_locally(struct scheduler* sc, struct rpc_task* task,
        struct lmp_recv_msg* msg, struct capref* client_cap);

/**
 * \brief Initializez the scheduler.
 */
void scheduler_init(struct scheduler* sc, coreid_t my_core_id, void* urpc_buf);
/**
 * \brief Starts polling the URPC and RPC queues for new tasks.
 */
errval_t scheduler_start(struct scheduler* sc, struct lmp_chan* lc);

#endif /* _INIT_SCHEDULER_H_ */