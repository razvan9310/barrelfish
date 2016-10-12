/**
 * \file
 * \brief AOS paging helpers.
 */

/*
 * Copyright (c) 2012, 2013, 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <aos/aos.h>
#include <aos/paging.h>
#include <aos/except.h>
#include <aos/slab.h>
#include "threads_priv.h"

#include <stdio.h>
#include <string.h>

static struct paging_state current;

/**
 * \brief Helper function that allocates a slot and
 *        creates a ARM l2 page table capability
 */
__attribute__((unused))
static errval_t arml2_alloc(struct paging_state * st, struct capref *ret)
{
    errval_t err;
    err = st->slot_alloc->alloc(st->slot_alloc, ret);
    if (err_is_fail(err)) {
        debug_printf("slot_alloc failed: %s\n", err_getstring(err));
        return err;
    }
    err = vnode_create(*ret, ObjType_VNode_ARM_l2);
    if (err_is_fail(err)) {
        debug_printf("vnode_create failed: %s\n", err_getstring(err));
        return err;
    }
    return SYS_ERR_OK;
}

errval_t paging_init_state(struct paging_state *st, lvaddr_t start_vaddr,
        struct capref pdir, struct slot_allocator *ca)
{
    debug_printf("paging_init_state\n");

    // M2:
    // Slot allocator.
    st->slot_alloc = ca;

    // Slab allocator. 16 nodes should be enough, as we'll have the Memory
    // Manager up and running before we really start mapping vaddresses.
    slab_init(&st->slabs, sizeof(struct paging_node), slab_default_refill);
    static char paging_buf[sizeof(struct paging_node) * 16];
    slab_grow(&st->slabs, paging_buf, sizeof(paging_buf));

    // We don't have any L2 pagetables yet, thus make sure the flags are unset. 
    for (int i = 0; i < L1_PAGETABLE_ENTRIES; ++i) {
        st->l2_pagetables[i].initialized = false;
    }

    size_t capacity = (size_t) (0xFFFFFFFF - start_vaddr);
    // Four initial empty nodes.
    st->head = (struct paging_node*) slab_alloc(&st->slabs);
    st->head->base = start_vaddr;
    st->head->size = capacity;
    st->head->type = NodeType_Free;
    st->head->prev = NULL;

    // TODO: What should we do with pdir?

    // TODO (M4): Implement page fault handler that installs frames when a page fault
    // occurs and keeps track of the virtual address space.
    return SYS_ERR_OK;
}

/**
 * \brief This function initializes the paging for this domain
 * It is called once before main.
 */
errval_t paging_init(void)
{
    debug_printf("paging_init\n");

    // M2:
    // TODO: Where should the pdir capref come from?
    struct capref pdir;
    paging_init_state(&current, VADDR_OFFSET, pdir,
            get_default_slot_allocator());
    set_current_paging_state(&current);

    // TODO (M4): initialize self-paging handler
    // TIP: use thread_set_exception_handler() to setup a page fault handler
    // TIP: Think about the fact that later on, you'll have to make sure that
    // you can handle page faults in any thread of a domain.
    // TIP: it might be a good idea to call paging_init_state() from here to
    // avoid code duplication.

    return SYS_ERR_OK;
}

/**
 * \brief Initialize per-thread paging state
 */
void paging_init_onthread(struct thread *t)
{
    // TODO (M4): setup exception handler for thread `t'.
}

/**
 * \brief return a pointer to a bit of the paging region `pr`.
 * This function gets used in some of the code that is responsible
 * for allocating Frame (and other) capabilities.
 */
errval_t paging_region_init(struct paging_state *st, struct paging_region *pr, size_t size)
{
    void *base;
    errval_t err = paging_alloc(st, &base, size);
    if (err_is_fail(err)) {
        debug_printf("paging_region_init: paging_alloc failed\n");
        return err_push(err, LIB_ERR_VSPACE_MMU_AWARE_INIT);
    }
    pr->base_addr    = (lvaddr_t)base;
    pr->current_addr = pr->base_addr;
    pr->region_size  = size;
    return SYS_ERR_OK;
}

/**
 * \brief return a pointer to a bit of the paging region `pr`.
 * This function gets used in some of the code that is responsible
 * for allocating Frame (and other) capabilities.
 */
errval_t paging_region_map(struct paging_region *pr, size_t req_size,
                           void **retbuf, size_t *ret_size)
{
    lvaddr_t end_addr = pr->base_addr + pr->region_size;
    ssize_t rem = end_addr - pr->current_addr;
    if (rem > req_size) {
        // ok
        *retbuf = (void*)pr->current_addr;
        *ret_size = req_size;
        pr->current_addr += req_size;
    } else if (rem > 0) {
        *retbuf = (void*)pr->current_addr;
        *ret_size = rem;
        pr->current_addr += rem;
        debug_printf("exhausted paging region, "
                "expect badness on next allocation\n");
    } else {
        return LIB_ERR_VSPACE_MMU_AWARE_NO_SPACE;
    }
    return SYS_ERR_OK;
}

/**
 * \brief free a bit of the paging region `pr`.
 * This function gets used in some of the code that is responsible
 * for allocating Frame (and other) capabilities.
 * NOTE: Implementing this function is optional.
 */
errval_t paging_region_unmap(struct paging_region *pr, lvaddr_t base, size_t bytes)
{
    // TIP: you will need to keep track of possible holes in the region
    return SYS_ERR_OK;
}

bool should_refill_slabs(struct paging_state *st)
{
    return slab_freecount(&st->slabs) < 6 && !st->slab_refilling;
}

/**
 *
 * \brief Find a bit of free virtual address space that is large enough to
 *        accomodate a buffer of size `bytes`.
 */
errval_t paging_alloc(struct paging_state *st, void **buf, size_t bytes)
{
    // TODO: M2 Implement this function
    struct paging_node *node = st->head;
    while (node != NULL) {
        if (node->type == NodeType_Free && node->size >= bytes) {
            *buf = (void*) node->base;
            return SYS_ERR_OK;
        }
        node = node->next;
    }
    *buf = NULL;
    return LIB_ERR_VREGION_NOT_FOUND;
}

/**
 * \brief map a user provided frame, and return the VA of the mapped
 *        frame in `buf`.
 */
errval_t paging_map_frame_attr(struct paging_state *st, void **buf,
                               size_t bytes, struct capref frame,
                               int flags, void *arg1, void *arg2)
{
    if (should_refill_slabs(st)) {
        st->slab_refilling = true;
        errval_t err = st->slabs.refill_func(&st->slabs);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "slab refill_func failed");
            return LIB_ERR_VREGION_MAP;
        }
        st->slab_refilling = false;   
    }
    errval_t err = paging_alloc(st, buf, bytes);
    if (err_is_fail(err)) {
        return err;
    }
    return paging_map_fixed_attr(st, (lvaddr_t)(*buf), frame, bytes, flags);
}

errval_t
slab_refill_no_pagefault(struct slab_allocator *slabs, struct capref frame, size_t minbytes)
{
    // Refill the two-level slot allocator without causing a page-fault
    return SYS_ERR_OK;
}

/**
 * \brief map a user provided frame at user provided VA.
 * TODO(M1): Map a frame assuming all mappings will fit into one L2 pt
 * TODO(M2): General case 
 */
errval_t paging_map_fixed_attr(struct paging_state *st, lvaddr_t vaddr,
        struct capref frame, size_t bytes, int flags)
{
    size_t original_bytes = bytes;
    lvaddr_t original_vaddr = vaddr;
    /* Step 1: Check if the virtual memory area wanted by the user is in fact
               free (check corresponding page_node). */
    struct paging_node *node = st->head;
    while (bytes > 0) {
        if (node == NULL) {
            // Couldn't find node, err out.
            return LIB_ERR_VREGION_MAP;
        }
        if (node->type != NodeType_Free) {
            // Skip node if allocated.
            node = node->next;
            continue;
        }
        if (node->base > vaddr || node->base + node->size < vaddr + bytes) {
            // Current node can't hold the desired vregion.
            continue;
        }

        /* Step 2: Get reference to L1 pagetable. */
        struct capref l1_cap = {
            .cnode = cnode_page,
            .slot = 0,
        };

        /* Step 3: Compute & (if needed) create all the necessary L2 tables and
           sub-frames. */
        uint32_t mapped_size = 0;
        errval_t err;
        while (bytes > 0) {
            struct capref l2_cap;
            // Get index of next L2 pagetable to map into.
            uint16_t l2_index = ARM_L1_OFFSET(vaddr);
            if (st->l2_pagetables[l2_index].initialized) {
                l2_cap = st->l2_pagetables[l2_index].cap;
            } else {
                // Need to allocate a new L2 pagetable.
                err = arml2_alloc(st, &l2_cap);
                if (err_is_fail(err)) {
                    return err;
                }

                // Map newly created L2 to L1.
                struct capref l2_to_l1;
                st->slot_alloc->alloc(st->slot_alloc, &l2_to_l1);
                err = vnode_map(l1_cap, l2_cap, l2_index,
                    VREGION_FLAGS_READ_WRITE, 0, 1, l2_to_l1);
                if (err_is_fail(err)) {
                    DEBUG_ERR(err, "Mapping L2 to L1");
                    return err;
                }
                st->l2_pagetables[l2_index].cap = l2_cap;
                st->l2_pagetables[l2_index].initialized = true;
            }

            // Get index frame should start at in current L2 table.
            uint32_t frame_index = ARM_L2_OFFSET(vaddr);

            for (int i = frame_index; i < ARM_L2_MAX_ENTRIES && bytes > 0; ++i) {
                struct capref cap_to_map;
                st->slot_alloc->alloc(st->slot_alloc, &cap_to_map);
                size_t size_to_map = (bytes < BASE_PAGE_SIZE) ? bytes : BASE_PAGE_SIZE;

                err = cap_retype(cap_to_map,
                        frame,
                        mapped_size,
                        ObjType_Frame,
                        size_to_map,
                        1);
                if (err_is_fail(err)) {
                    DEBUG_ERR(err, "Splitting frame cap");
                    return err;
                }

                /* Step 4: Perform mapping. */
                struct capref frame_to_l2;
                st->slot_alloc->alloc(st->slot_alloc, &frame_to_l2);
                err = vnode_map(l2_cap, cap_to_map, i, flags, 0, 1, frame_to_l2);
                if (err_is_fail(err)) {
                    DEBUG_ERR(err, "Mapping frame to L2");
                    return err;
                }

                mapped_size += size_to_map;
                bytes -= size_to_map;
                vaddr += size_to_map;
            }
        }
        if (bytes > 0) {
            node = node->next;
        }
    }

    node->type = NodeType_Allocated;
    if (node->base + node->size > original_vaddr + original_bytes) {
        // Need new (free) node to the right;
        struct paging_node *right = (struct paging_node*) slab_alloc(&st->slabs);
        right->type = NodeType_Free;
        right->base = original_vaddr + original_bytes;
        right->size = node->size - (original_vaddr - node->base)
                - original_bytes;
        right->next = node->next;
        right->prev = node;
        if (node->next != NULL) {
            node->next->prev = right;
        }
        node->next = right;
        node->size -= right->size;
    }

    if (original_vaddr > node->base) {
        // Need new (free) node to the left.
        struct paging_node *left = (struct paging_node*) slab_alloc(&st->slabs);
        left->type = NodeType_Free;
        left->base = node->base;
        left->size = original_vaddr - node->base;
        left->next = node;
        left->prev = node->prev;
        if (node->prev != NULL) {
            node->prev->next = left;
        }
        if (st->head == node) {
            st->head = left;
        }
        node->prev = left;
        node->base = original_vaddr;
        node->size -= left->size;
    }

    return SYS_ERR_OK;
}

/**
 * \brief unmap a user provided frame, and return the VA of the mapped
 *        frame in `buf`.
 * NOTE: Implementing this function is optional.
 */
errval_t paging_unmap(struct paging_state *st, const void *region)
{
    return SYS_ERR_OK;
}
