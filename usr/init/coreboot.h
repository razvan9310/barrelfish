/**
 * \file
 * \brief user-space core booting.
 */

/*
 * Copyright (c) 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef _INIT_CORE_BOOT_H_
#define _INIT_CORE_BOOT_H_

#include <aos/aos.h>
#include <aos/coreboot.h>
#include <aos/kernel_cap_invocations.h>
#include <spawn/multiboot.h>

/**
 * \brief Maps the frame under cap_urpc to vspace. If "my_core_id" is 0, then
 * this will first allocate a frame of size "bytes" before mapping.
 */
errval_t map_urpc_frame_to_vspace(void** ret, size_t bytes,
		coreid_t my_core_id);

/**
 * \brief Writes the given RAM base and size and struct bootinfo into the shared
 * URPC frame mapped at the given urpc_buf
 */
void write_to_urpc(void* urpc_buf, genpaddr_t base, gensize_t size,
		struct bootinfo* bi, coreid_t my_core_id);

/**
 * \brief Read memory data from URPC frame and forg a cap to it; also read the
 * bootinfo structure.
 */
errval_t read_from_urpc(void* urpc_buf, struct bootinfo** bi,
		coreid_t my_core_id);

// /**
//  * \brief Writes the given RAM base and size into the shared URPC frame mapped
//  * at the given urpc_buf.
//  */
// void write_ram_region_to_urpc(void* urpc_buf, genpaddr_t base, gensize_t size,
// 		coreid_t my_core_id);

// /**
//  * \brief Read memory data from URPC frame and forge a RAM cap to it.
//  */
// errval_t forge_ram_cap_from_urpc(void* urpc_buf, coreid_t my_core_id);

/**
 * \brief Starts the core given by "core_to_start".
 */
errval_t start_core(coreid_t core_to_start, coreid_t my_core_id,
		struct bootinfo *bi);

/**
 * \brief Reads module RAM data from URPC and forges appropriate caps.
 */
errval_t read_modules(void* urpc_buf, struct bootinfo* bi,
        coreid_t my_core_id);

#endif /* _INIT_CORE_BOOT_H_ */