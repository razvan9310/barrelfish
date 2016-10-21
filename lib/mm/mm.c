/**
 * \file
 * \brief A library for managing physical memory (i.e., caps)
 */

#include <mm/mm.h>
#include <aos/debug.h>

void add_node(struct mmnode **head, struct mmnode *node)
{
    assert(node != NULL);
    if (*head == NULL) {
        *head = node;
        (*head)->prev = NULL;
        (*head)->next = NULL;
        return;
    }
    struct mmnode *aux = *head;
    while (aux->next != NULL) {
        aux = aux->next;
    }
    aux->next = node;
    node->prev = aux;
    node->next = NULL; 
}

/* Removes a MemoryManager node and the cap it holds. */
void remove_node(struct mm *mm, struct mmnode *node)
{
    assert(node != NULL);
    if (node->prev != NULL) {
        node->prev->next = node->next;
    }
    if (node->next != NULL) {
        node->next->prev = node->prev;
        if (mm->head == node) {
            mm->head = node->next;
        }
    }
    node->type = NodeType_Free;
    slab_free(&mm->slabs, node->bh);
}

/* Merges two adjacent nodes and their caps. */
void merge_nodes(struct mm *mm, struct mmnode *fst, struct mmnode *snd)
{
    /* Nodes. */
    fst->next = snd->next;
    if (snd->next != NULL) {
        snd->next->prev = fst;
    }
    fst->size += snd->size;

    remove_node(mm, snd);
}

void *mm_slab_alloc(struct mm *mm)
{
    if (slab_freecount(&mm->slabs) < 6 && !mm->slab_refilling) {
        if (!mm->slabs.refill_func) {
            return NULL;
        }
        mm->slab_refilling = true;
        errval_t err = mm->slabs.refill_func(&mm->slabs);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "slab refill_func failed");
            return NULL;
        }
        mm->slab_refilling = false;   
    }

    void *bh = slab_alloc(&mm->slabs);
    return bh;
}

errval_t mm_slot_alloc(struct mm *mm, uint64_t nslots, struct capref *retcap)
{
    struct slot_prealloc* sa = (struct slot_prealloc*) mm->slot_alloc_inst;
    if (sa->meta[sa->current].free < 6 && sa->meta[!sa->current].free < 6 && !mm->slot_refilling) {
        /* Need to refill slots. */
        if (mm->slot_refill == NULL) {
            return MM_ERR_SLOT_MM_ALLOC;
        }
        mm->slot_refilling = true;
        mm->slot_refill(mm->slot_alloc_inst);
        mm->slot_refilling = false;
    }
    return mm->slot_alloc(mm->slot_alloc_inst, nslots, retcap);
}

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
    mm->head = NULL;

    mm->slab_refilling = false;
    mm->slot_refilling = false;

    return SYS_ERR_OK;
}

/**
 * Destroys the memory allocator.
 */
void mm_destroy(struct mm *mm)
{
    struct mmnode *node = mm->head;
    while (node != NULL) {
        cap_destroy(node->cap.cap);
        remove_node(mm, node);
        node = node->next;
    }
}

/**
 * Adds a capability to the memory manager.
 *
 * \param  cap  Capability to add
 * \param  base Physical base address of the capability
 * \param  size Size of the capability (in bytes)
 */
errval_t mm_add(struct mm *mm, struct capref cap, genpaddr_t base, size_t size)
{
    void *bh = mm_slab_alloc(mm);
    if (bh == NULL) {
        return LIB_ERR_RAM_ALLOC;
    }
    struct mmnode *node = (struct mmnode*) bh;
    node->bh = bh;
    node->cap.cap = cap;
    node->cap.base = base;
    node->cap.size = size;

    node->base = base;
    node->size = size;
    node->prev = node->next = NULL;
    node->type = NodeType_Free;

    add_node(&mm->head, node);
    assert(mm->head != NULL);

    /* TODO: This will get overwritten if mm_add is called multiple times. */
    mm->initial_base = base;
    mm->initial_size = size;

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
errval_t mm_alloc_aligned(struct mm *mm, size_t size, size_t alignment, struct capref *retcap)
{
    if (alignment < BASE_PAGE_SIZE) {
        alignment = BASE_PAGE_SIZE;
    }
    if (size % alignment != 0) {
        size += alignment - size % alignment;
    }

    struct mmnode *node = mm->head;
    assert(node != NULL);
    while (true) {
        if (node == NULL) {
            /* We shouldn't allocate over our capacity of nodes. */
            return LIB_ERR_RAM_ALLOC_WRONG_SIZE;
        }
        if (node->type != NodeType_Free) {
            node = node->next;
            continue;
        }

        /* Note that we're ignoring nodes of size < required size, because all
           continuous free nodes will be merged together after call to mm_free.
           This means that any free node is either followed by an allocated node
           or by NULL (end-of-list). */
        if (node->size >= size) {
            /* Need to split the cap. */
            uint64_t remaining = node->size - size;
            /* Create new (allocated) node -- add it "to the left". */
            void *bh = mm_slab_alloc(mm);
            if (bh == NULL) {
                return LIB_ERR_RAM_ALLOC;
            }
            struct mmnode *new_node = (struct mmnode*) bh;
            new_node->bh = bh;
            new_node->type = NodeType_Allocated;

            new_node->prev = node->prev;
            if (node->prev != NULL) {
                node->prev->next = new_node;
            }
            new_node->next = node;
            node->prev = new_node;

            /* retcap (newly allocated cap) needs a slot. */
            errval_t err = mm_slot_alloc(mm, 1, &(new_node->cap.cap));
            if (err_is_fail(err)) {
                debug_printf("mm_slot_alloc failed with %s\n", err_getstring(err));
                return MM_ERR_SLOT_MM_ALLOC;
            }
            gensize_t offset = (gensize_t) (node->base - mm->initial_base);
            err = cap_retype(new_node->cap.cap, node->cap.cap, offset, mm->objtype, size, 1);
            
            new_node->base = node->base;
            new_node->size = size;
            node->base += size;
            node->size = remaining;

            *retcap = new_node->cap.cap;
            // new_node->cap.base = new_node->base;
            // new_node->cap.size = new_node->size;
            if (mm->head == node) {
                mm->head = new_node;
            }

            break;
        }
        node = node->next;
    }

    return SYS_ERR_OK;
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
    return mm_alloc_aligned(mm, size, 4u * BASE_PAGE_SIZE, retcap);
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
    struct mmnode *node = mm->head;
    while (true) {
        if (node == NULL) {
            return MM_ERR_MM_FREE;
        }
        if (node->base == base && node->size == size) {
            /* No need to compare cap itself, since the MM instance is process
               specific. */
            node->type = NodeType_Free;
            cap_destroy(node->cap.cap);
            break;
        }
        node = node->next;
    }

    /* Merge the node together with its predecessor/successor, if both free. */
    if (node->next != NULL && node->next->type == NodeType_Free) {
        merge_nodes(mm, node, node->next);
    }
    if (node->prev != NULL && node->prev->type == NodeType_Free) {
        merge_nodes(mm, node->prev, node);
    }
    return SYS_ERR_OK;
}

