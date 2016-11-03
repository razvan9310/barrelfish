/**
 * \file
 * \brief A library for managing physical memory (i.e., caps)
 */

/*

Design decisions:

- we will store RAM chunks in a doubly-linked list
- we will always merge adjacent free nodes in mm_free, and assume there are no adjacent free nodes throughout the code

Notes:

- not at all reentrant in general, though can survive refilling slabs & slots
- my memory manager leaks memory :D (does not return slabs & slots)
- mm_destroy() leaves stuff hanging around, but destroys its own caps, so maybe nobody cares

*/

#include <aos/aos.h>
#include <aos/debug.h>
#include <bitmacros.h>
#include <mm/mm.h>

#define SLAB_RESERVE 8
#define SLOT_RESERVE 8

///// private function definitions ///////////////////////////////////////////

static errval_t mm_slot_alloc(struct mm *mm, struct capref *retcap) {
    struct slot_prealloc* sa = (struct slot_prealloc*) mm->slot_alloc_inst;
    if (sa->meta[sa->current].free < SLOT_RESERVE &&
            sa->meta[!sa->current].free < SLOT_RESERVE &&
            !mm->slots_refilling) { // need to refill slots
        if (mm->slot_refill == NULL) {
            return MM_ERR_SLOT_MM_ALLOC;
        }
        mm->slots_refilling = true;
        mm->slot_refill(mm->slot_alloc_inst);
        mm->slots_refilling = false;
    }
    return mm->slot_alloc(mm->slot_alloc_inst, 1, retcap);
}

static void *mm_slab_alloc(struct mm *mm) {
    if (slab_freecount(&mm->slabs) < SLAB_RESERVE && !mm->slabs_refilling) {
        CHECK_COND(mm->slabs.refill_func, "no function to refill slabs", return NULL);
        mm->slabs_refilling = true;
        errval_t err = mm->slabs.refill_func(&mm->slabs);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "refilling slabs failed");
            mm->slabs_refilling = false;
            return NULL;
        }
        mm->slabs_refilling = false;
    }
    return slab_alloc(&mm->slabs);
}

static void mm_slab_free(struct mm *mm, void *block) {
    slab_free(&mm->slabs, block);
}

static void node_fill(struct mmnode *node, struct capinfo cap, genpaddr_t base, gensize_t size, enum nodetype type) {
    node->cap  = cap;
    node->base = base;
    node->size = size;
    node->type = type;
    node->prev = node->next = NULL;
}

// Adds the new node *after* the old one.
static void node_add(struct mmnode *old, struct mmnode *new) {
    assert(old != NULL);
    assert(new != NULL);
    struct mmnode *after = old->next;
    old->next = new;
    new->prev = old;
    if (after != NULL) after->prev = new;
    new->next = after;
}

static void node_rm(struct mmnode *node) {
    assert(node != NULL);
    struct mmnode *before = node->prev;
    struct mmnode *after  = node->next;
    assert(before != NULL);
    before->next = after;
    if (after != NULL) after->prev = before;
}

static errval_t make_cap_for_node(struct mm *mm, struct mmnode *node) {
    struct capinfo region = node->cap;
    CHECK("allocating slot for new RAM cap", mm_slot_alloc(mm, &node->cap.cap));
    gensize_t offset = node->base - region.base;
    // debug_printf("calling cap_retype, offset = %llx (%llx - %llx)\n", offset, node->base, region.base);
    CHECK("cap_retype for the newly allocated RAM chunk",
          cap_retype(node->cap.cap, region.cap, offset, mm->objtype, node->size, 1));
    return SYS_ERR_OK;
}

static errval_t find_parent_region_cap(struct mm *mm, genpaddr_t base, gensize_t size, struct capinfo *retcapi) {
    for (struct mmnode *found = &mm->head; found != NULL; found = found->next) {
        if (found->type == NodeType_Parent &&
            found->base <= base && found->base + found->size >= base + size) {
            *retcapi = found->cap;
            return SYS_ERR_OK;
        }
    }
    return LIB_ERR_RAM_ALLOC;
}

// static void print_mm_state(struct mm *mm) {
//     for (struct mmnode *node = &mm->head; node != NULL; node = node->next) {
//         debug_printf("       STATE:  * base %llx, size %llx, end %llx, type %d, cap slot %x\n", node->base, node->size, node->base + node->size, node->type, node->cap.cap.slot);
//     }
// }

///// Public function definitions ///////////////////////////////////////////

/**
 * Initialize the memory manager.
 *
 * \param  mm               The mm struct to initialize.
 * \param  objtype          The cap type this manager should deal with.
 * \param  slab_refill_func Custom function for refilling the slab allocator.
 * \param  slot_alloc_func  Function for allocating capability slots.
 * \param  slot_refill_func Function for refilling (making) new slots.
 * \param  slot_alloc_inst  Pointer to a slot allocator instance (typically passed to the alloc and refill functions).
 */
errval_t mm_init(struct mm *mm, enum objtype objtype,
                 slab_refill_func_t slab_refill_func,
                 slot_alloc_t slot_alloc_func,
                 slot_refill_t slot_refill_func,
                 void *slot_alloc_inst)
{
    assert(mm != NULL);
    if (slab_refill_func == NULL) {
        slab_refill_func = slab_default_refill;
    }
    slab_init(&mm->slabs, sizeof(struct mmnode), slab_refill_func);

    mm->objtype = objtype;
    mm->slot_alloc = slot_alloc_func;
    mm->slot_refill = slot_refill_func;
    mm->slot_alloc_inst = slot_alloc_inst;
    struct slot_prealloc *sp = (struct slot_prealloc*) slot_alloc_inst;
    sp->mm = mm;
    mm->head = (struct mmnode){
        .type = NodeType_Head,
        .prev = NULL,
        .next = NULL,
    };
    mm->slabs_refilling = false;
    mm->slots_refilling = false;

    return SYS_ERR_OK;
}

/**
 * Destroys the memory allocator.
 */
void mm_destroy(struct mm *mm)
{
    debug_printf("mm: self-destruct sequence initiated\n");
    for (struct mmnode *found = &mm->head; found != NULL; found = found->next) {
        if (found->type == NodeType_Parent) { // we do not want to hold these anymore
            errval_t err = cap_destroy(found->cap.cap);
            if (err_is_fail(err)) debug_printf("ERROR destroying mem region cap: %s\n", err_getstring(err));
        }
    }
    // since we've destroyed all caps to the memory we've been holding, we
    // should be done even though we did not reclaim nodes or whatever...
    // we've nuked it all anyway
}

/**
 * Adds a capability to the memory manager.
 *
 * \param  cap  Capability to add
 * \param  base Physical base address of the capability
 * \param  size Size of the capability (in bytes)
 */
errval_t mm_add(struct mm *mm, struct capref capr, genpaddr_t base, size_t size)
{
    // debug_printf("mm: adding region of base 0x%llx, size 0x%x\n", base, size);
    struct capinfo capi = {
        .cap = capr,
        .base = base,
    };
    // I save the original "parent" region that I won't ever touch so that I can
    //   cleanly mm_destroy() easily
    struct mmnode *parent = mm_slab_alloc(mm);
    CHECK_COND(parent != NULL, "allocating space for new mmnode", return LIB_ERR_RAM_ALLOC);
    struct mmnode *node   = mm_slab_alloc(mm);
    CHECK_COND(node != NULL, "allocating space for new mmnode", mm_slab_free(mm, parent); return LIB_ERR_RAM_ALLOC);

    node_fill(parent, capi, base, size, NodeType_Parent);
    node_fill(node, capi, base, size, NodeType_Free);
    node_add(&mm->head, parent);
    node_add(&mm->head, node);
    return SYS_ERR_OK;
}

/**
 * Allocate aligned physical memory.
 *
 * \param       mm        The memory manager.
 * \param       size      How much memory to allocate.
 * \param       alignment The alignment requirement of the base address for your memory.
 * \param[out]  retcap    Capability for the allocated region.
 */
errval_t mm_alloc_aligned(struct mm *mm, size_t wanted_size, size_t alignment, struct capref *retcap)
{
    // debug_printf("*** mm: in mm_alloc_aligned, allocating size 0x%x, alignment 0x%x\n", wanted_size, alignment);
    // print_mm_state(mm);

    // RAM caps must aligned to BASE_PAGE_SIZE on both ends
    gensize_t size = ROUND_UP(wanted_size, BASE_PAGE_SIZE);
    alignment = ROUND_UP(alignment, BASE_PAGE_SIZE);
    if (alignment == 0) {
        debug_printf("some idiot wants to get RAM with alignment 0\n");
        alignment = BASE_PAGE_SIZE;
    }

    struct mmnode *before = mm_slab_alloc(mm);
    CHECK_COND(before != NULL, "allocating space for new mmnode", return LIB_ERR_RAM_ALLOC);
    struct mmnode *after = mm_slab_alloc(mm);
    CHECK_COND(after != NULL, "allocating space for new mmnode", mm_slab_free(mm, before); return LIB_ERR_RAM_ALLOC);

    // look for a free node that's big enough
    // NOTE: acquire lock here
    for (struct mmnode *found = &mm->head; found != NULL; found = found->next) {
        // debug_printf("*** mm: found node: base %llx, size %llx, type %d\n", found->base, found->size, found->type);

        if (found->type != NodeType_Free) continue;
        genpaddr_t real_base = ROUND_UP(found->base, alignment);
        if (real_base >= found->base + found->size) continue;
        gensize_t useful_size = found->size - (real_base - found->base);
        if (useful_size < size) continue;
        gensize_t remaining = useful_size - size;

        // debug_printf("*** mm:    will use this from %llx, remaining %llx\n", real_base, remaining);

        // 0. we want to use this, so mark it
        found->type = NodeType_Allocated;
        // 1. maybe split at the beginning because alignment
        if (real_base != found->base) {
            node_fill(before, found->cap, found->base, real_base - found->base, NodeType_Free);
            node_add(found->prev, before);
        } else {
            mm_slab_free(mm, before);
        }
        // 2. maybe split at the end because size
        if (remaining) { //
            node_fill(after, found->cap, real_base + size, remaining, NodeType_Free);
            node_add(found, after);
        } else {
            mm_slab_free(mm, after);
        }
        // 3. update the allocated node
        found->base = real_base;
        found->size = size;
        // NOTE: release lock here
        CHECK("creating cap for new RAM chunk", make_cap_for_node(mm, found));

        // 4. we're done here
        // debug_printf("*** mm: allocated %llx bytes at base %llx\n", found->size, found->base);
        *retcap = found->cap.cap;
        return SYS_ERR_OK;
    }
    // NOTE: release lock here
    debug_printf("If you see this, I think there is no free RAM left\n");
    mm_slab_free(mm, before);
    mm_slab_free(mm, after);
    return LIB_ERR_RAM_ALLOC;
}

/**
 * Allocate physical memory.
 *
 * \param       mm        The memory manager.
 * \param       size      How much memory to allocate.
 * \param[out]  retcap    Capability for the allocated region.
 */
errval_t mm_alloc(struct mm *mm, size_t size, struct capref *retcap)
{
    // caps for memory regions must always be aligned to BASE_PAGE_SIZE,
    //   so that is the smallest unit
    return mm_alloc_aligned(mm, size, BASE_PAGE_SIZE, retcap);
}

/**
 * Free a certain region (for later re-use).
 *
 * \param       mm        The memory manager.
 * \param       cap       The capability to free.
 * \param       base      The physical base address of the region.
 * \param       size      The size of the region.
 */
errval_t mm_free(struct mm *mm, struct capref cap, genpaddr_t base, gensize_t size)
{
    // debug_printf("*** mm: freeing node at base %llx, size %llx\n", base, size);
    // print_mm_state(mm);

    // walk through the list, looking for the node this refers to
    // NOTE: acquire lock here
    for (struct mmnode *found = &mm->head; found != NULL; found = found->next) {
        if (found->base == base && found->size == size &&
            found->type == NodeType_Allocated) {

            // 1. set type to free
            CHECK("looking for parent region", find_parent_region_cap(mm, base, size, &found->cap));
            found->type = NodeType_Free;

            struct mmnode* freeme[2] = {NULL, NULL};
            // TODO/note to self: if I don't want to check on cap, I can check on adjacency
            // 2.1. maybe merge adjacent free node before
            if (found->prev->type == NodeType_Free && found->prev->cap.base == found->cap.base) {
                // debug_printf("*** mm_free: merging with prev\n");
                assert(found->prev->base + found->prev->size == found->base);
                freeme[0] = found->prev;
                found->base = found->prev->base;
                found->size += found->prev->size;
                node_rm(found->prev);
            }
            // 2.2. maybe merge adjacent free node after
            if (found->next != NULL && found->next->type == NodeType_Free && found->next->cap.base == found->cap.base) {
                // debug_printf("*** mm_free: merging with next\n");
                assert(found->base + found->size == found->next->base);
                freeme[1] = found->next;
                found->size += found->next->size;
                node_rm(found->next);
            }
            // NOTE: release lock here
            CHECK("mm_free: destroying cap for freed chunk", cap_destroy(cap));
            for (int i = 0; i < 2; ++i) {
                if (freeme[i] != NULL) mm_slab_free(mm, freeme[i]);
            }
            return SYS_ERR_OK;
        }
    }
    // if we got here, we didn't find anything
    // NOTE: release lock here
    debug_printf("ERROR: mm_free: given parameters don't match any actual region\n");
    return LIB_ERR_RAM_ALLOC;
}
