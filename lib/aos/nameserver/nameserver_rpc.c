#include <nameserver_rpc.h>


/**
 * \brief Initiate handshake by sending local cap to server.
 */
errval_t aos_ns_handshake_send_handler(void* void_args)
{
    // 0. get cycle counter value
    uintptr_t* args = (uintptr_t*) void_args;

    struct aos_ns_rpc *rpc = (struct aos_ns_rpc*) args[0];
    errval_t err;
    do {
        err = lmp_chan_send1(&rpc->lc, LMP_FLAG_SYNC, rpc->lc.local_cap,
                AOS_NS_HANDSHAKE);
    } while (err_is_fail(err));

    // N. get new cycle counter value, show result

    return SYS_ERR_OK;
}

/**
 * \brief Finalize handshake by receiving ack from server.
 */
errval_t aos_ns_handshake_recv_handler(void* void_args)
{
    // 0. get cycle counter value

    uintptr_t* args = (uintptr_t*) void_args;

    struct aos_ns_rpc* rpc = (struct aos_ns_rpc*) args[0];
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;;
    struct capref cap;

    errval_t err = lmp_chan_recv(&rpc->lc, &msg, &cap);
    if (err_is_fail(err) && lmp_err_is_transient(err)) {
        // Reregister.
        lmp_chan_register_recv(&rpc->lc, rpc->ws,
                MKCLOSURE((void*) aos_ns_handshake_recv_handler, args));
    }

    // We should have an ACK.
    assert(msg.buf.msglen == 1);
    assert(msg.words[0] == AOS_NS_OK);

    rpc->lc.remote_cap = cap;

    // No need to rereister here, as handshake is complete;
    return err;
}

/**
 * \brief General-purpose blocking RPC send-and-receive function.
 */
errval_t aos_ns_send_and_receive(uintptr_t* args, void* send_handler,
        void* rcv_handler)
{
    struct aos_ns_rpc* rpc = (struct aos_ns_rpc*) args[0];
    // 1. Set send handler.
    CHECK("nameserver_rpc.c#aos_ns_send_and_receive: lmp_chan_register_send",
            lmp_chan_register_send(&rpc->lc, rpc->ws,
                    MKCLOSURE(send_handler, args)));

    // 2. Set receive handler.
    CHECK("nameserver_rpc.c#aos_ns_send_and_receive: lmp_chan_register_recv",
            lmp_chan_register_recv(&rpc->lc, rpc->ws,
                    MKCLOSURE(rcv_handler, args)));

    // 3. Block until channel is ready to send.
    CHECK("nameserver_rpc.c#aos_ns_send_and_receive: event_dispatch send",
            event_dispatch(rpc->ws));

    // 4. Block until channel is ready to receive.
    CHECK("nameserver_rpc.c#aos_ns_send_and_receive: event_dispatch receive",
            event_dispatch(rpc->ws));

    return SYS_ERR_OK;
}

errval_t aos_ns_init(struct aos_ns_rpc *rpc, struct waitset* ws)
{
    // 0. Assign waitset to use from now on.
    rpc->ws = ws;

    // 1. Create local channel using init as remote endpoint.
    CHECK("nameserver_rpc.c#aos_ns_init: lmp_chan_accept",
            lmp_chan_accept(&rpc->lc, DEFAULT_LMP_BUF_WORDS, cap_nsep));
    debug_printf("aos_ns_init: LOCAL CAP HAS SLOT %d\n", rpc->lc.local_cap.slot);

    // 2. Marshal args.
    uintptr_t args = (uintptr_t) rpc;

    CHECK("lmp_chan_alloc_recv_slot", 
            lmp_chan_alloc_recv_slot(&rpc->lc));    

    // 2. Send handshake request to init and wait for ACK.
    CHECK("nameserver_rpc.c#aos_ns_init: aos_ns_send_and_receive",
            aos_ns_send_and_receive(&args, aos_ns_handshake_send_handler,
                    aos_ns_handshake_recv_handler));

    // debug_printf("Successfully started connection with NS\n");

    // By now we've successfully established the underlying LMP channel for RPC.
    return SYS_ERR_OK;
}

errval_t aos_ns_register_send_handler(void* void_args)
{
    uintptr_t* args = (uintptr_t*) void_args;
    
    // TODO: implement functionality to send a number ofer the channel
    // given channel and wait until the ack gets returned.
    struct aos_ns_rpc* rpc = (struct aos_ns_rpc*) args[0];
    //coreid_t* core = (coreid_t*) args[1];

    errval_t err;
    size_t retries = 0;
    do {
        err = lmp_chan_send9(&rpc->lc, LMP_FLAG_SYNC, rpc->lc.local_cap,
                AOS_NS_REGISTER,
                *((uintptr_t*) args[1]), *((uintptr_t*) args[2]),
                *((uintptr_t*) args[3]), *((uintptr_t*) args[4]), 
                *((uintptr_t*) args[5]), *((uintptr_t*) args[6]),
                *((uintptr_t*) args[7]), *((uintptr_t*) args[8]));

        ++retries;
    } while (err_is_fail(err) && retries < 5);
    if (retries == 5) {
        return err;
    }

    return SYS_ERR_OK;
}

errval_t aos_ns_register_recv_handler(void* void_args)
{
    uintptr_t* args = (uintptr_t*) void_args;
    
    struct aos_ns_rpc* rpc = (struct aos_ns_rpc*) args[0];
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;

    struct capref cap;
    errval_t err = lmp_chan_recv(&rpc->lc, &msg, &cap);
    if (err_is_fail(err) && lmp_err_is_transient(err)) {
        // Reregister.
        lmp_chan_register_recv(&rpc->lc, rpc->ws,
                MKCLOSURE((void*) aos_ns_register_recv_handler, args));
    }

    assert(msg.buf.msglen == 1);

    assert(msg.words[0] == AOS_NS_OK);
    // No need to reregister, we got our RAM.
    return err;
}

errval_t register_service(struct aos_ns_rpc* rpc, char* name)
{
    uintptr_t* args = (uintptr_t*) malloc(9 * sizeof(uintptr_t));
    args[0] = (uintptr_t) ((struct aos_ns_rpc*) malloc(sizeof(struct aos_ns_rpc)));
    *((struct aos_ns_rpc*) args[0]) = *rpc;

    args[1] = (uintptr_t) ((int*) malloc(sizeof(int)));
    int length = (int)strlen(name);
    *((int*) args[1]) = length;
    int romaining_length = length;

    int word_offset = 2;

    for (int i = 0; i < 7; i++) {
        args[i + word_offset] = (uintptr_t) ((char*) malloc(4*sizeof(char)));
    }

    for (int i = 0; i <= length / 4; i++) {
        memcpy((void*)args[i + word_offset], name + (i * 4), (romaining_length > 4) ? 4 : romaining_length);
        // debug_printf("Args[%d] = %s\n", i + word_offset, (char*)args[i + word_offset]);
        romaining_length -= 4;
    }

    // debug_printf("BEFORE Calling send_and_receive (register): %s\n", name);

    CHECK("nameserver_rpc.c#register_service: aos_ns_send_and_receive",
    aos_ns_send_and_receive(args, aos_ns_register_send_handler,
            aos_ns_register_recv_handler));

    return SYS_ERR_OK;
}


errval_t aos_ns_deregister_send_handler(void* void_args)
{
    uintptr_t* args = (uintptr_t*) void_args;
    
    // TODO: implement functionality to send a number ofer the channel
    // given channel and wait until the ack gets returned.
    struct aos_ns_rpc* rpc = (struct aos_ns_rpc*) args[0];
    //coreid_t* core = (coreid_t*) args[1];

    errval_t err;
    size_t retries = 0;
    do {
        // debug_printf("Before try number %d\n", retries + 1);

        err = lmp_chan_send1(&rpc->lc, LMP_FLAG_SYNC, rpc->lc.local_cap,
                AOS_NS_DEREGISTER);

        // debug_printf("After try number %d\n", retries + 1);
        ++retries;
    } while (err_is_fail(err) && retries < 5);
    if (retries == 5) {
        return err;
    }

    return SYS_ERR_OK;   
}

errval_t aos_ns_deregister_recv_handler(void* void_args)
{
    uintptr_t* args = (uintptr_t*) void_args;
    
    struct aos_ns_rpc* rpc = (struct aos_ns_rpc*) args[0];
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;

    struct capref cap;
    errval_t err = lmp_chan_recv(&rpc->lc, &msg, &cap);
    if (err_is_fail(err) && lmp_err_is_transient(err)) {
        // Reregister.
        lmp_chan_register_recv(&rpc->lc, rpc->ws,
                MKCLOSURE((void*) aos_ns_register_recv_handler, args));
    }

    assert(msg.buf.msglen == 1);

    assert(msg.words[0] == AOS_NS_OK);
    // No need to reregister, we got our RAM.
    return err;
}

errval_t deregister_service(struct aos_ns_rpc* rpc)
{
    uintptr_t* args = (uintptr_t*) malloc(9 * sizeof(uintptr_t));
    args[0] = (uintptr_t) ((struct aos_ns_rpc*) malloc(sizeof(struct aos_ns_rpc)));
    *((struct aos_ns_rpc*) args[0]) = *rpc;

    // debug_printf("BEFORE Calling send_and_receive (deregister)\n");

    CHECK("nameserver_rpc.c#register_service: aos_ns_send_and_receive",
    aos_ns_send_and_receive(args, aos_ns_deregister_send_handler,
            aos_ns_deregister_recv_handler));

    return SYS_ERR_OK;  
}


errval_t aos_ns_lookup_send_handler(void* void_args)
{
    
    // TODO: implement functionality to send a number ofer the channel
    // given channel and wait until the ack gets returned.
    uintptr_t* args = (uintptr_t*) void_args;

    struct aos_ns_rpc* rpc = (struct aos_ns_rpc*) args[0];
    //coreid_t* core = (coreid_t*) args[1];

    errval_t err;
    size_t retries = 0;
    do {
        err = lmp_chan_send9(&rpc->lc, LMP_FLAG_SYNC, rpc->lc.local_cap,
                AOS_NS_LOOKUP,
                *((uintptr_t*) args[1]), *((uintptr_t*) args[2]),
                *((uintptr_t*) args[3]), *((uintptr_t*) args[4]), 
                *((uintptr_t*) args[5]), *((uintptr_t*) args[6]),
                *((uintptr_t*) args[7]), *((uintptr_t*) args[8]));

        ++retries;
    } while (err_is_fail(err) && retries < 5);
    if (retries == 5) {
        return err;
    }

    return SYS_ERR_OK;}

errval_t aos_ns_lookup_recv_handler(void* void_args)
{
    uintptr_t* args = (uintptr_t*) void_args;
    
    struct aos_ns_rpc* rpc = (struct aos_ns_rpc*) args[0];
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;
    struct capref* dst = (struct capref*) args[9];

    errval_t err = lmp_chan_recv(&rpc->lc, &msg, dst);
    if (err_is_fail(err) && lmp_err_is_transient(err)) {
        // Reregister.
        lmp_chan_register_recv(&rpc->lc, rpc->ws,
                MKCLOSURE((void*) aos_ns_register_recv_handler, args));
    }

    assert(msg.buf.msglen == 1);

    assert(msg.words[0] == AOS_NS_OK);
    debug_printf("ns lookup recv: received in cap at %p\n", dst);
    return err;
}

errval_t lookup(struct aos_ns_rpc* rpc, char* name, struct capref* ep)
{
    uintptr_t* args = (uintptr_t*) malloc(10 * sizeof(uintptr_t));
    // args[0] = (uintptr_t) ((struct aos_ns_rpc*) malloc(sizeof(struct aos_ns_rpc)));
    // *((struct aos_ns_rpc*) args[0]) = *rpc;
    args[0] = (uintptr_t) rpc;

    args[1] = (uintptr_t) ((int*) malloc(sizeof(int)));
    int length = (int)strlen(name);
    *((int*) args[1]) = length;
    int romaining_length = length;

    int word_offset = 2;

    for (int i = 0; i < 7; i++) {
        args[i + word_offset] = (uintptr_t) ((char*) malloc(4*sizeof(char)));
    }

    for (int i = 0; i <= length / 4; i++) {
        memcpy((void*)args[i + word_offset], name + (i * 4), (romaining_length > 4) ? 4 : romaining_length);
        // debug_printf("Args[%d] = %s\n", i + word_offset, (char*)args[i + word_offset]);
        romaining_length -= 4;
    }


    args[9] = (uintptr_t) ep;

    CHECK("lmp_chan_alloc_recv_slot for lookup", 
            lmp_chan_alloc_recv_slot(&rpc->lc));  
    // debug_printf("BEFORE Calling send_and_receive (lookup): %s\n", name);

    CHECK("nameserver_rpc.c#lookup: aos_ns_send_and_receive",
    aos_ns_send_and_receive(args, aos_ns_lookup_send_handler,
            aos_ns_lookup_recv_handler));

    return SYS_ERR_OK;
}


errval_t aos_ns_ennumerate_send_handler(void* void_args)
{
    uintptr_t* args = (uintptr_t*) void_args;
    
    // TODO: implement functionality to send a number ofer the channel
    // given channel and wait until the ack gets returned.
    struct aos_ns_rpc* rpc = (struct aos_ns_rpc*) args[0];

    errval_t err;
    size_t retries = 0;
    do {
        err = lmp_chan_send1(&rpc->lc, LMP_FLAG_SYNC, rpc->lc.local_cap,
                AOS_NS_ENNUMERATE);
        ++retries;
    } while (err_is_fail(err) && retries < 5);

    if (retries == 5) {
        return err;
    }

    return SYS_ERR_OK;
}

errval_t aos_ns_ennumerate_recv_handler(void* void_args)
{
    // debug_printf("IN aos_ns_ennumerate_recv_handler\n");

    uintptr_t* args = (uintptr_t*) void_args;
    
    struct aos_ns_rpc* rpc = (struct aos_ns_rpc*) args[0];
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;

    // //for implicit cap minitng
    struct capref cap;
    errval_t err = lmp_chan_recv(&rpc->lc, &msg, &cap);
    if (err_is_fail(err) && lmp_err_is_transient(err)) {
        // Reregister.
        lmp_chan_register_recv(&rpc->lc, rpc->ws,
                MKCLOSURE((void*) aos_ns_ennumerate_recv_handler, args));
    }

    rpc->number = msg.words[1];
    // debug_printf("Number of remaining services to receive = %d\n", rpc->number);

    if (rpc->it == 0) {        
        rpc->names = (char**) malloc(rpc->number * sizeof(char*));
        rpc->lengths = (int*) malloc(rpc->number * sizeof(int));
        rpc->total = rpc->number;
    }

    rpc->lengths[rpc->it] = msg.words[2];

    // debug_printf("Length is %d\n", rpc->lengths[rpc->it]);
    rpc->names[rpc->it] = (char*) malloc(rpc->lengths[rpc->it] * sizeof(char));

    for (int i = 0; i <= rpc->lengths[rpc->it]/4; i++) {
        memcpy(rpc->names[rpc->it] + i * 4 , &(msg.words[i + 3]), 4);
        // debug_printf("%s\n", (char*)&(msg.words[i + 3]));
    }
    // debug_printf("[%d] Length is %d, name is %s\n", rpc->it, rpc->lengths[rpc->it], rpc->names[rpc->it]);

    rpc->it++;

    if (rpc->number > 1) {
        lmp_chan_register_recv(&rpc->lc, rpc->ws,
                MKCLOSURE((void*) aos_ns_ennumerate_recv_handler, args));
    }

    return (errval_t) msg.words[0];    
}

errval_t enumerate(struct aos_ns_rpc* rpc, size_t* num, char*** result)
{   
    uintptr_t* args = (uintptr_t*) malloc(9 * sizeof(uintptr_t));
    args[0] = (uintptr_t) ((struct aos_ns_rpc*) malloc(sizeof(struct aos_ns_rpc)));
    // ((struct aos_ns_rpc*) args[0]) = rpc;
    args[0] = (uintptr_t) rpc;

    // args[1] = (uintptr_t) (num);
    // args[2] = (uintptr_t) (result);
    // debug_printf("BEFORE Calling send_and_receive (ennumerate)\n");

    CHECK("nameserver_rpc.c#enumerate: aos_ns_send_and_receive",
    aos_ns_send_and_receive(args, aos_ns_ennumerate_send_handler,
            aos_ns_ennumerate_recv_handler));

    while (rpc->number > 1) {
        // 4. Block until channel is ready to receive.
        CHECK("nameserver_rpc.c#aos_ns_send_and_receive: event_dispatch receive",
                event_dispatch(rpc->ws));
        // debug_printf("rpc->number = %d\n", rpc->number);
    }
    // debug_printf("After send and receive\n");
    // debug_printf("Num is %d, length is %d, name is %s\n", rpc->number, rpc->lengths[0], rpc->names[0]);

    *num = rpc->total;
    *result = (char**)malloc(*num * sizeof(char*));

    for (int i = 0; i < *num; i++) {
        // debug_printf("Allocating %d bytes\n", rpc->lengths[i]);
        (*result)[i] = (char*) malloc(rpc->lengths[i] * sizeof(char));
        memcpy((*result)[i], rpc->names[i], rpc->lengths[i]);
        // debug_printf("name is %s\n", (*result)[i]);
    }

    free(rpc->names);
    free(rpc->lengths);
    rpc->number = 0;
    rpc->it = 0;
    rpc->total = 0;

    return SYS_ERR_OK;
}
