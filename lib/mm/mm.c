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
    struct slab_allocator slabs;
    slab_init(&slabs, sizeof(struct mmnode), slab_refill_func);
    mm->slabs = slabs;

    mm->objtype = objtype;
    mm->slot_alloc = slot_alloc_func;
    mm->slot_refill = slot_refill_func;
    mm->slot_alloc_inst = slot_alloc_inst;
    ((struct slot_prealloc*) mm->slot_alloc_inst)->mm = mm;

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
    struct capinfo info = {
        .cap = cap,
        .base = base,
        .size = size,
    };
    struct mmnode node = {
        .type = NodeType_Free,
        .cap = info,
        .base = base,
        .size = size,
        .prev = NULL,
        .next = NULL,
    };
    add_node(&mm->head, &node);
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
    printf("mm_alloc needs to allocate size=%d\n", size);
    printf("\n");

    struct mmnode *node = mm->head;
    assert(node != NULL);
    printf("Type of head is %d\n\n", mm->head->type);
    while (size > 0) {
        if (node == NULL) {
            /* We shouldn't allocate over our capacity of nodes. */
            return LIB_ERR_RAM_ALLOC_WRONG_SIZE;
        }
        printf("Found node of type %d; type free is %d\n\n", node->type, NodeType_Free);
        if (node->type != NodeType_Free) {
            node = node->next;
            continue;
        }
        if (node->size <= size) {
            printf("node->size <= size\n\n");
            node->type = NodeType_Allocated;
            struct capinfo info = {
                .cap = *retcap,
                .base = node->base,
                .size = node->size,
            };
            node->cap = info;
            size -= node->size;
        } else {
            /* Node size is larger than we need -- we need to split it into
               an assigned node and a free one. */
            size_t remaining = node->size - size;
            printf("Remaining free memory in node = %d\n\n", remaining);
            struct capinfo info = {
                .cap = *retcap,
                .base = node->cap.base + remaining,
                .size = size,
            };
            struct mmnode new_node = {
                .type = NodeType_Allocated,
                .cap = info,
                .base = node->base + remaining,
                .size = size,
                .prev = node,
                .next = node->next,
            };
            node->size = remaining;
            if (node->next != NULL) {
                node->next->prev = &new_node;
            }
            node->next = &new_node;
            size = 0;
        }
        node = node->next;
    }

    size_t free_mem = 0;
    int free_nodes = 0, total_nodes = 0;
    node = mm->head;
    while (node != NULL) {
        printf("Found node type %d of size %d\n\n", node->type, node->size);
        if (node->type == NodeType_Free) {
            ++free_nodes;
            free_mem += node->size;
        }
        ++total_nodes;
        node = node->next;
    }

    printf("Successfully allocated\n");
    printf("Remaining free memory = %d\n", free_mem);
    printf("%d free nodes out of %d total\n", free_nodes, total_nodes);
    printf("\n");
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
    // struct mmnode *node = mm->head;
    // while (node != NULL) {
    //     if (node->cap.base == base && node->cap.size == size) {
    //         node->type = NodeType_Free;
    //     }
    // }
    // return SYS_ERR_OK;
    return LIB_ERR_NOT_IMPLEMENTED;
}
