/**
 * \file
 * \brief Barrelfish library initialization.
 */

/*
 * Copyright (c) 2007-2016, ETH Zurich.
 * Copyright (c) 2014, HP Labs.
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
#include <aos/dispatch.h>
#include <aos/curdispatcher_arch.h>
#include <aos/dispatcher_arch.h>
#include <aos/inthandler.h>
#include <barrelfish_kpi/dispatcher_shared.h>
#include <aos/morecore.h>
#include <aos/paging.h>
#include <barrelfish_kpi/domain_params.h>
#include "threads_priv.h"
#include "init.h"

/// Are we the init domain (and thus need to take some special paths)?
static bool init_domain;

/// RPC instance for non-init domains.
static struct aos_rpc rpc;

extern size_t (*_libc_terminal_read_func)(char *, size_t);
extern size_t (*_libc_terminal_write_func)(const char *, size_t);
extern void (*_libc_exit_func)(int);
extern void (*_libc_assert_func)(const char *, const char *, const char *, int);

void libc_exit(int);

void libc_exit(int status)
{
    // Use spawnd if spawned through spawnd
    if(disp_get_domain_id() == 0) {
        errval_t err = cap_revoke(cap_dispatcher);
        if (err_is_fail(err)) {
            sys_print("revoking dispatcher failed in _Exit, spinning!", 100);
            while (1) {}
        }
        err = cap_delete(cap_dispatcher);
        sys_print("deleting dispatcher failed in _Exit, spinning!", 100);

        // XXX: Leak all other domain allocations
    } else {
        debug_printf("libc_exit NYI!\n");
    }

    thread_exit(status);
    // If we're not dead by now, we wait
    while (1) {}
}

static void libc_assert(const char *expression, const char *file,
                        const char *function, int line)
{
    char buf[512];
    size_t len;

    /* Formatting as per suggestion in C99 spec 7.2.1.1 */
    len = snprintf(buf, sizeof(buf), "Assertion failed on core %d in %.*s: %s,"
                   " function %s, file %s, line %d.\n",
                   disp_get_core_id(), DISP_NAME_LEN,
                   disp_name(), expression, function, file, line);
    sys_print(buf, len < sizeof(buf) ? len : sizeof(buf));
}

static size_t syscall_terminal_write(const char *buf, size_t len)
{
    if (len) {
        return sys_print(buf, len);
    }
    return 0;
}

// static size_t aos_terminal_write(const char* buf, size_t len)
// {
//     if (len > 0) {
//         return aos_rpc_send_string(get_init_rpc(), buf, 0);
//     }
//     return 0;
// }

static size_t syscall_terminal_read(char *buf, size_t len)
{
    len = sys_getchar(buf);
    return len;
}

static size_t aos_terminal_read(char *buf, size_t len)
{
    aos_rpc_serial_getchar(get_init_rpc(), buf);
    return len;
}

/* Set libc function pointers */
void barrelfish_libc_glue_init(void)
{
    // XXX: FIXME: Check whether we can use the proper kernel serial, and
    // what we need for that
    // TODO: change these to use the user-space serial driver if possible
    _libc_terminal_write_func = syscall_terminal_write;
    _libc_exit_func = libc_exit;
    _libc_assert_func = libc_assert;
    /* morecore func is setup by morecore_init() */

    // XXX: set a static buffer for stdout
    // this avoids an implicit call to malloc() on the first printf
    static char buf[BUFSIZ];
    setvbuf(stdout, buf, _IOLBF, sizeof(buf));
    static char ebuf[BUFSIZ];
    setvbuf(stderr, ebuf, _IOLBF, sizeof(buf));
}

static void handle_pagefault(int subtype,
        void* addr,
        arch_registers_state_t* regs,
        arch_registers_fpu_state_t* fpuregs)
{
    // TODO(razvan): What should be done based on subtype, regs, fpuregs?

    lvaddr_t vaddr = (lvaddr_t) addr;
    
    if (vaddr < BASE_PAGE_SIZE) {
        debug_printf("Thread attempted to dereference NULL-reserved memory at"
                "%u, killing it\n", vaddr);
        thread_exit(THREAD_EXIT_PAGEFAULT);
    }

    if (vaddr >= KERNEL_WINDOW) {
        // This is not the heap, kill the thread.
        debug_printf("Thread attempted to access non-heap address %u,"
                "killing it\n", vaddr);
        thread_exit(THREAD_EXIT_PAGEFAULT);
    }

    vaddr -= vaddr % BASE_PAGE_SIZE;

    struct paging_state* st = get_current_paging_state();

    if (!is_vregion_claimed(st, vaddr)) {
        // Some other thread has already allocated this?
        debug_printf("Pagefault handler: page at %u has not been claimed yet\n",
                vaddr);
        thread_exit(THREAD_EXIT_PAGEFAULT);
    }

    errval_t err;
    if (paging_should_refill_slabs(st)) {
        // Should first refill paging slabs before attempting to map.
        err = paging_refill_slabs(st);
        if (err_is_fail(err)) {
            debug_printf("Pagefault handler erred during paging_refill_slabs:"
                    "%s\n", err_getstring(err));
            thread_exit(THREAD_EXIT_PAGEFAULT);
        }
    }

    struct capref frame;
    size_t retsize;
    err = frame_alloc(&frame, 4 * BASE_PAGE_SIZE, &retsize);
    if (err_is_fail(err)) {
        debug_printf("Pagefault handler erred during frame_alloc: %s\n",
                err_getstring(err));
        thread_exit(THREAD_EXIT_PAGEFAULT);
    }

    err = paging_map_fixed(get_current_paging_state(), vaddr, frame, retsize);
    if (err_is_fail(err)) {
        debug_printf("Pagefault handler erred during paging_map_fixed: %s\n",
                err_getstring(err));
        // TODO(razvan): free frame before killing thread.
        thread_exit(THREAD_EXIT_PAGEFAULT);
    }
}

void default_exception_handler(enum exception_type type, int subtype,
        void *addr, arch_registers_state_t *regs,
        arch_registers_fpu_state_t *fpuregs);
void default_exception_handler(enum exception_type type, int subtype,
        void *addr, arch_registers_state_t *regs,
        arch_registers_fpu_state_t *fpuregs)
{
    switch (type) {
        case EXCEPT_PAGEFAULT:
            handle_pagefault(subtype, addr, regs, fpuregs);
            break;
        default:
            debug_printf("Unhandled exception type %d. Killing thread.\n",
                    type);
            thread_exit(THREAD_EXIT_UNHANDLED_EXCEPTION);
    }
}

void terminal_read_handler(void *params);
void terminal_read_handler(void *params)
{
    printf("We got an interrupt for a character\n");
}
/** \brief Initialise libbarrelfish.
 *
 * This runs on a thread in every domain, after the dispatcher is setup but
 * before main() runs.
 */
errval_t barrelfish_init_onthread(struct spawn_domain_params *params)
{
    errval_t err;

    // do we have an environment?
    if (params != NULL && params->envp[0] != NULL) {
        extern char **environ;
        environ = params->envp;
    }

    // Init default waitset for this dispatcher
    struct waitset *default_ws = get_default_waitset();
    waitset_init(default_ws);

    // Initialize ram_alloc state
    ram_alloc_init();
    /* All domains use smallcn to initialize */
    err = ram_alloc_set(init_domain ? ram_alloc_fixed : NULL);
    if (err_is_fail(err)) {
        return err_push(err, LIB_ERR_RAM_ALLOC_SET);
    }

    err = paging_init();
    if (err_is_fail(err)) {
        return err_push(err, LIB_ERR_VSPACE_INIT);
    }

    err = morecore_init();
    if (err_is_fail(err)) {
        return err_push(err, LIB_ERR_MORECORE_INIT);
    }

    err = slot_alloc_init();
    if (err_is_fail(err)) {
        return err_push(err, LIB_ERR_SLOT_ALLOC_INIT);
    }

    lmp_endpoint_init();

    // Setup main thread exception handler.
    static char stack_base[8192 * 4] = {0};
    char* stack_top = stack_base + 8192 * 2;
    CHECK("init.c#barrelfish_init_onthread: thread_set_exception_handler (2)",
            thread_set_exception_handler(
                    (exception_handler_fn) default_exception_handler,
                    NULL,
                    (void*) stack_base, (void*) stack_top,
                    NULL, NULL));

    debug_printf("SUCCESSFULLY SET HANDLER\n");

    // init domains only get partial init
    if (init_domain) {
        CHECK("Retype selfep from dispatcher", cap_retype(cap_selfep, 
                cap_dispatcher, 0, ObjType_EndPoint, 0, 1));
        
        return SYS_ERR_OK;
    }

    // Initialize RPC channel.
    CHECK("init.c#barrelfish_init_onthread: aos_rpc_init",
            aos_rpc_init(&rpc, get_default_waitset()));
    // Set domain init rpc.
    set_init_rpc(&rpc);
    debug_printf("init.c: successfully setup connection with init\n");
    // right now we don't have the nameservice & don't need the terminal
    // and domain spanning, so we return here
    return SYS_ERR_OK;
}


/**
 *  \brief Initialise libbarrelfish, while disabled.
 *
 * This runs on the dispatcher's stack, while disabled, before the dispatcher is
 * setup. We can't call anything that needs to be enabled (ie. cap invocations)
 * or uses threads. This is called from crt0.
 */
void barrelfish_init_disabled(dispatcher_handle_t handle, bool init_dom_arg);
void barrelfish_init_disabled(dispatcher_handle_t handle, bool init_dom_arg)
{
    init_domain = init_dom_arg;
    disp_init_disabled(handle);
    thread_init_disabled(handle, init_dom_arg);
}
