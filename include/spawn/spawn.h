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

    // Cap for L1 Node and its ref to its table
    struct capref l1_cap;
    struct cnoderef l1_cnoderef;

    struct cnoderef taskcn;

    struct capref dispatcher;
    struct capref rootcn;
    struct capref dispframe;
    struct capref argspg;
    struct capref selfep;

    struct cnoderef alloc0;
    struct cnoderef alloc1;
    struct cnoderef alloc2;
    struct cnoderef base_pagecn;

    struct cnoderef pagecn;

    struct paging_state pg_state;
    struct single_slot_allocator ssa;

    dispatcher_handle_t disp_handle;

    // executable image's properties
    genvaddr_t got_ubase; // in the child's vspace
    genvaddr_t entry_point;
};

// Start a child process by binary name. Fills in si
errval_t spawn_load_by_name(void * binary_name, struct spawninfo * si);

void setup_cspace(struct spawninfo *si);
void setup_vspace(struct spawninfo *si);
errval_t setup_dispatcher(struct spawninfo *si);
errval_t load_elf_into_memory(struct spawninfo *si, lvaddr_t base, size_t size);
errval_t elf_section_allocate(void *state, genvaddr_t base, size_t size,
                              uint32_t flags, void **ret);
errval_t setup_args(struct spawninfo *si, char *const argv[], int argc);
int spawn_tokenize_cmdargs(char *s, char *argv[], size_t argv_len);

#endif /* _INIT_SPAWN_H_ */
