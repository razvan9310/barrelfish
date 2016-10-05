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

void remove_node(struct mmnode **head, struct mmnode *node) {
    assert(node != NULL);
    assert(*head != NULL);
    if (node->prev != NULL) {
        node->prev->next = node->next;
    }
    if (node->next != NULL) {
        node->next->prev = node->prev;
    }
    node->type = NodeType_Free;
    cap_destroy(node->cap.cap);
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

    return SYS_ERR_OK;
}

/**
 * Destroys the memory allocator.
 */
void mm_destroy(struct mm *mm)
{
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
    struct mmnode *node = (struct mmnode*) slab_alloc(&mm->slabs);
    if (node == NULL) {
        return LIB_ERR_RAM_ALLOC;
    }
    node->cap.cap = cap;
    node->cap.base = base;
    node->cap.size = size;
    node->base = base;
    node->size = size;
    node->prev = node->next = NULL;
    node->type = NodeType_Free;

    add_node(&mm->head, node);
    assert(mm->head != NULL);

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
    // TODO: Implement
    return LIB_ERR_NOT_IMPLEMENTED;
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
    struct mmnode *node = mm->head;
    assert(node != NULL);
    while (size > 0) {
        if (node == NULL) {
            /* We shouldn't allocate over our capacity of nodes. */
            return LIB_ERR_RAM_ALLOC_WRONG_SIZE;
        }
        if (node->type != NodeType_Free) {
            node = node->next;
            continue;
        }
        if (node->size <= size) {
            node->type = NodeType_Allocated;
            node->cap.cap = *retcap;
            node->cap.base = node->base;
            node->cap.size = node->size;
            size -= node->size;
            // printf("(1) Allocated @ base=%lld, size=%lld\n\n", node->base, node->size);
        } else {
            /* Need to split node into allocated & free. */
            uint64_t remaining = node->size - size;
            // printf("Current size to alloc is: %d\n\n", size);
            // printf("Remaining free memory in current node, after alloc: %lld\n\n", remaining);
            struct mmnode *new_node = (struct mmnode*) slab_alloc(&mm->slabs);
            new_node->type = NodeType_Allocated;
            new_node->base = node->base + remaining;
            new_node->size = size;
            new_node->cap.cap = *retcap;
            new_node->cap.base = new_node->base;
            new_node->cap.size = new_node->size;

            new_node->next = node;
            new_node->prev = node->prev;
            if (node->prev != NULL) {
                node->prev->next = new_node;
            }
            node->prev = new_node;
            if (node == mm->head) {
                mm->head = new_node;
            }

            node->size = remaining;
            size = 0;
            // printf("(2) Allocated @ base=%lld, size=%lld\n\n\n", new_node->base, new_node->size);
        }
        node = node->next;
    }

    // size_t free_mem = 0;
    // int free_nodes = 0, total_nodes = 0;
    // node = mm->head;
    // while (node != NULL) {
    //     printf("Found node type %d of size %d\n\n", node->type, node->size);
    //     if (node->type == NodeType_Free) {
    //         ++free_nodes;
    //         free_mem += node->size;
    //     }
    //     ++total_nodes;
    //     node = node->next;
    // }

    // printf("Successfully allocated\n");
    // printf("Remaining free memory = %d\n", free_mem);
    // printf("%d free nodes out of %d total\n", free_nodes, total_nodes);
    // printf("\n");
    return SYS_ERR_OK;
    // return LIB_ERR_NOT_IMPLEMENTED;
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
            // printf("NODE IS NULL!! Couldn't find base=%lld, size=%lld\n\n", base, size);
            return MM_ERR_MM_FREE;
        }
        if (node->cap.base == base && node->cap.size == size) {
            /* No need to compare cap itself, since the MM instance is process
               specific. */
            node->type = NodeType_Free;
            return SYS_ERR_OK;
        }
        node = node->next;
    }
}
