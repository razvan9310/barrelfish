/**
 * \file
 * \brief Local memory allocator for init till mem_serv is ready to use
 */

#include "mem_alloc.h"
#include <mm/mm.h>
#include <aos/paging.h>

/// MM allocator instance data
struct mm aos_mm;

static errval_t aos_ram_alloc_aligned(struct capref *ret, size_t size, size_t alignment)
{
    return mm_alloc_aligned(&aos_mm, size, alignment, ret);
}

errval_t aos_ram_free(struct capref cap, size_t bytes)
{
    errval_t err;
    struct frame_identity fi;
    err = frame_identify(cap, &fi);
    if (bytes > fi.bytes) {
        bytes = fi.bytes;
    }
    return mm_free(&aos_mm, cap, fi.base, bytes);
}

/**
 * \brief Setups a local memory allocator for init to use till the memory server
 * is ready to be used.
 */
errval_t initialize_ram_alloc(genpaddr_t* remaining_mem_base,
        gensize_t* remaining_mem_size)
{
    debug_printf("initialize_ram_alloc\n");
    errval_t err;

    // Init slot allocator
    static struct slot_prealloc init_slot_alloc;
    struct capref cnode_cap = {
        .cnode = {
            .croot = CPTR_ROOTCN,
            .cnode = ROOTCN_SLOT_ADDR(ROOTCN_SLOT_SLOT_ALLOC0),
            .level = CNODE_TYPE_OTHER,
        },
        .slot = 0,
    };
    err = slot_prealloc_init(&init_slot_alloc, cnode_cap, L2_CNODE_SLOTS, &aos_mm);
    if (err_is_fail(err)) {
        return err_push(err, MM_ERR_SLOT_ALLOC_INIT);
    }

    // Initialize aos_mm
    err = mm_init(&aos_mm, ObjType_RAM, NULL,
                  slot_alloc_prealloc, slot_prealloc_refill,
                  &init_slot_alloc);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "Can't initalize the memory manager.");
    }

    // Give aos_mm a bit of memory for the initialization
    static char nodebuf[sizeof(struct mmnode)*64];
    slab_grow(&aos_mm.slabs, nodebuf, sizeof(nodebuf));

    // Walk bootinfo and add all RAM caps to allocator handed to us by the kernel
    uint64_t mem_avail = 0;
    struct capref mem_cap = {
        .cnode = cnode_super,
        .slot = 0,
    };

    size_t ram_regions_seen = 0;
    *remaining_mem_base = 0;
    *remaining_mem_size = 0;
    genpaddr_t base = 0;
    for (int i = 0; i < bi->regions_length; i++) {
        if (bi->regions[i].mr_type == RegionType_Empty) {
            if (ram_regions_seen == my_core_id) {
                err = mm_add(&aos_mm, mem_cap, bi->regions[i].mr_base, bi->regions[i].mr_bytes);
                if (err_is_ok(err)) {
                    if (base == 0) {
                        base = bi->regions[i].mr_base;
                    }
                    mem_avail += bi->regions[i].mr_bytes;
                } else {
                    DEBUG_ERR(err, "Warning: adding RAM region %d (%p/%zu) FAILED", i, bi->regions[i].mr_base, bi->regions[i].mr_bytes);
                }

                err = slot_prealloc_refill(aos_mm.slot_alloc_inst);
                if (err_is_fail(err) && err_no(err) != MM_ERR_SLOT_MM_ALLOC) {
                    DEBUG_ERR(err, "in slot_prealloc_refill() while initialising"
                            " memory allocator");
                    abort();
                }

                mem_cap.slot++;
            } else {
                if (*remaining_mem_base == 0) {
                    *remaining_mem_base = bi->regions[i].mr_base;
                }
                *remaining_mem_size += bi->regions[i].mr_bytes;
            }
            ++ram_regions_seen;
        }
    }
    debug_printf("Added %"PRIu64" MB of physical memory.\n", mem_avail / 1024 / 1024);

    // Finally, we can initialize the generic RAM allocator to use our local allocator
    err = ram_alloc_set(aos_ram_alloc_aligned);
    if (err_is_fail(err)) {
        return err_push(err, LIB_ERR_RAM_ALLOC_SET);
    }
    // Also intialize the generic RAM free function to ours.
    err = ram_free_set(aos_ram_free);
    if (err_is_fail(err)) {
        return err_push(err, LIB_ERR_RAM_ALLOC_SET);
    }

    return SYS_ERR_OK;
}
