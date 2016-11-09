/**
 * \file
 * \brief init core booting.
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

errval_t start_core(coreid_t core_to_start, coreid_t my_core_id,
		struct bootinfo *bi);

#endif /* _INIT_CORE_BOOT_H_ */