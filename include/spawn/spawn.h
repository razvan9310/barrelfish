/**
 * \file
 * \brief create child process library
 */

/*
 * Copyright (c) 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef _INIT_SPAWN_H_
#define _INIT_SPAWN_H_

#include "aos/slot_alloc.h"
#include "aos/paging.h"

struct spawninfo {

    // Information about the binary
    char * binary_name;     // Name of the binary

    // Cap for L1Cnode.
    struct capref l1_cnode_cap;
    // L2Cnodes.
    struct cnoderef l2_cnodes[ROOTCN_SLOTS_USER];

    // Caps for L1 pagetable in {parent, child}.
    struct capref l1_pagetable_me;
    struct capref l1_pagetable_child;

    // Child's paging state.
    struct paging_state pg_state;

    // Tracking slots for child vspace caps.
    cslot_t next_slot;

    // Child's dispatcher.
    struct capref dispatcher;
    struct capref dispatcher_frame;
    dispatcher_handle_t disp_handle;
    arch_registers_state_t* enabled_area;

    // executable image's properties
    genvaddr_t got_ubase; // in the child's vspace
    genvaddr_t entry_point;
};

// Start a child process by binary name. Fills in si
errval_t spawn_load_by_name(void * binary_name, struct spawninfo * si,
        coreid_t core_id);

errval_t spawn_load_by_name_args(void * binary_name, struct spawninfo * si,
        coreid_t core_id, char* extra_args);

errval_t setup_cspace(struct spawninfo *si);
errval_t mapping_cb(void* mapping_state, struct capref cap);
errval_t setup_vspace(struct spawninfo *si);
errval_t setup_elf(struct spawninfo* si, lvaddr_t vaddr, size_t bytes);
errval_t elf_alloc_section(void* sate, genvaddr_t base, size_t bytes,
        uint32_t flags, void** ret);
errval_t setup_dispatcher(struct spawninfo *si, coreid_t core_id);
errval_t elf_section_allocate(void *state, genvaddr_t base, size_t size,
                              uint32_t flags, void **ret);
errval_t setup_args(struct spawninfo* si, struct mem_region* mr);
errval_t setup_args_extra(struct spawninfo* si, struct mem_region* mr, char *extra);

#endif /* _INIT_SPAWN_H_ */
