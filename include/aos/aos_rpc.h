/**
 * \file
 * \brief Interface declaration for AOS rpc-like messaging
 */

/*
 * Copyright (c) 2013, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef _LIB_BARRELFISH_AOS_MESSAGES_H
#define _LIB_BARRELFISH_AOS_MESSAGES_H

#include <aos/aos.h>

#define AOS_RPC_OK         0       // General-purpose OK message.
#define AOS_RPC_FAILED     1       // RPC failure.
#define AOS_RPC_HANDSHAKE  1 << 2  // Message passed at handshake time.
#define AOS_RPC_MEMORY     1 << 3  // ID for memory requests.
#define AOS_RPC_NUMBER     1 << 5  // ID for send number requests.
#define AOS_RPC_PUTCHAR    1 << 7  // ID for putchar requests.
#define AOS_RPC_STRING     1 << 11 // ID for send string requests.
#define AOS_RPC_SPAWN      1 << 13 // ID for process spawn requests.
#define AOS_RPC_GET_PNAME  1 << 15 // ID for get process name requests.
#define AOS_RPC_GET_PLIST  1 << 19 // ID for get process name requests.
#define AOS_RPC_GETCHAR    1 << 21 // ID for getchar requests.
#define AOS_RPC_DEVICE     1 << 23  // ID for get device cap requests.
#define AOS_RPC_IRQ        1 << 27  // ID for get IRQ cap requests.
#define AOS_RPC_LIGHT_LED  1 << 4  // ID for light_led requests.
#define AOS_RPC_MEMTEST    1 << 6  // ID for memtest requests.
#define AOS_RPC_SPAWN_ARGS 1 << 8  // ID for memtest requests.

struct aos_rpc {
    struct lmp_chan lc;
    struct waitset* ws;
    char* buffer;
    int offset;
    domainid_t* ps_list;
    int ps_offset;
};

errval_t aos_rpc_handshake_send_handler(void* void_args);
errval_t aos_rpc_handshake_recv_handler(void* void_args);
errval_t aos_rpc_putchar_send_handler(void* void_args);
errval_t aos_rpc_putchar_recv_handler(void* void_args);
errval_t aos_rpc_send_number_send_handler(void* void_args);
errval_t aos_rpc_send_number_recv_handler(void* void_args);
errval_t aos_rpc_send_string_send_handler(void* void_args);
errval_t aos_rpc_send_string_recv_handler(void* void_args);
errval_t aos_rpc_ram_send_handler(void* void_args);
errval_t aos_rpc_ram_recv_handler(void* void_args);
errval_t aos_rpc_process_spawn_send_handler(void* void_args);
errval_t aos_rpc_process_spawn_recv_handler(void* void_args);
errval_t aos_rpc_process_spawn_args_send_handler(void* void_args);
errval_t aos_rpc_process_spawn_args_recv_handler(void* void_args);
errval_t aos_rpc_process_get_name_send_handler(void* void_args);
errval_t aos_rpc_process_get_name_recv_handler(void* void_args);
errval_t aos_rpc_process_get_process_list_send_handler(void* void_args);
errval_t aos_rpc_process_get_process_list_recv_handler(void* void_args);
errval_t aos_rpc_serial_getchar_send_handler(void* void_args);
errval_t aos_rpc_serial_getchar_recv_handler(void* void_args);
errval_t aos_rpc_light_led_send_handler(void* void_args);
errval_t aos_rpc_light_led_recv_handler(void* void_args);
errval_t aos_rpc_device_cap_send_handler(void* void_args);
errval_t aos_rpc_device_cap_recv_handler(void* void_args);
errval_t aos_rpc_irq_send_handler(void* void_args);
errval_t aos_rpc_irq_recv_handler(void* void_args);


/**
 * \brief send a number over the given channel
 */
errval_t aos_rpc_send_number(struct aos_rpc *chan, uintptr_t val,
        coreid_t core);

/**
 * \brief send a string over the given channel
 */
errval_t aos_rpc_send_string(struct aos_rpc *chan, const char *string,
        coreid_t core);

/**
 * \brief request a RAM capability with >= request_bits of size over the given
 * channel.
 */
errval_t aos_rpc_get_ram_cap(struct aos_rpc *chan, size_t bytes,
        struct capref *retcap, size_t *ret_bytes);

/**
 * \brief get one character from the serial port
 */
errval_t aos_rpc_light_led(struct aos_rpc *chan);

/**
 * \brief get one character from the serial port
 */
errval_t aos_rpc_serial_getchar(struct aos_rpc *chan, char *retc);

/**
 * \brief send one character to the serial port
 */
errval_t aos_rpc_serial_putchar(struct aos_rpc *chan, char c);

/**
 * \brief Request process manager to start a new process
 * \arg name the name of the process that needs to be spawned (without a
 *           path prefix)
 * \arg newpid the process id of the newly spawned process
 */
errval_t aos_rpc_process_spawn(struct aos_rpc *chan, char *name,
        coreid_t core, domainid_t *newpid);

/**
 * \brief Request process manager to start a new process with arguments
 * \arg name the name of the process that needs to be spawned (without a
 *           path prefix)
 * \arg argc count of the arguments
 * \arg argv array storing the values of arguments
 * \arg newpid the process id of the newly spawned process
 */
errval_t aos_rpc_process_spawn_args(struct aos_rpc *chan, struct capref *proc_info,
                                    coreid_t core, domainid_t *newpid);

/**
 * \brief Get name of process with id pid.
 * \arg pid the process id to lookup
 * \arg name A null-terminated character array with the name of the process
 * that is allocated by the rpc implementation. Freeing is the caller's
 * responsibility.
 */
errval_t aos_rpc_process_get_name(struct aos_rpc *chan, domainid_t pid,
        coreid_t core, char **name);

/**
 * \brief Get process ids of all running processes
 * \arg pids An array containing the process ids of all currently active
 * processes. Will be allocated by the rpc implementation. Freeing is the
 * caller's  responsibility.
 * \arg pid_count The number of entries in `pids' if the call was successful
 */
errval_t aos_rpc_process_get_all_pids(struct aos_rpc *chan,
        coreid_t core, domainid_t **pids, size_t *pid_count);

/**
 * \brief General-purpose blocking RPC send-and-receive function.
 */
errval_t aos_rpc_send_and_receive(uintptr_t* args, void* send_handler,
		void* rcv_handler);

/**
 * \brief Gets a capability to device registers
 * \param rpc  the rpc channel
 * \param paddr physical address of the device
 * \param bytes number of bytes of the device memory
 * \param frame returned devframe
 */
errval_t aos_rpc_get_device_cap(struct aos_rpc *rpc, lpaddr_t paddr, size_t bytes,
                                struct capref *frame);

/**
 * \brief Gets an interrupt (IRQ) capability.
 */
errval_t aos_rpc_get_irq_cap(struct aos_rpc* rpc, struct capref* retcap);
/**
 * \brief Initialize given rpc channel.
 * TODO: you may want to change the inteface of your init function, depending
 * on how you design your message passing code.
 */
errval_t aos_rpc_init(struct aos_rpc *rpc, struct waitset* ws);

#endif // _LIB_BARRELFISH_AOS_MESSAGES_H
