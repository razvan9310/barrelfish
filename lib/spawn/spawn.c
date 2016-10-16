#include <aos/aos.h>
#include <spawn/spawn.h>

#include <elf/elf.h>
#include <aos/dispatcher_arch.h>
#include <barrelfish_kpi/paging_arm_v7.h>
#include <barrelfish_kpi/domain_params.h>
#include <spawn/multiboot.h>

#define DPRINT(fmt, args...)  debug_printf("spawn: " fmt "\n", args)

#define CHECK(where, err)  if (err_is_fail(err)) {  \
                               DPRINT("ERROR %s: %s", where, err_getstring(err));  \
                               return err;  \
                           }

extern struct bootinfo *bi;

// TODO(M2): Implement this function such that it starts a new process
// TODO(M4): Build and pass a messaging channel to your child process
errval_t spawn_load_by_name(void * binary_name, struct spawninfo * si) {

    DPRINT("loading and starting: %s", binary_name);

    // Init spawninfo
    memset(si, 0, sizeof(*si));
    si->binary_name = binary_name;

    // - Get the binary from multiboot image

    struct mem_region *module = multiboot_find_module(bi, binary_name);
    if (!module) {
        DPRINT("Module %s not found", binary_name);
        return SPAWN_ERR_FIND_MODULE;
    }

    struct capref child_frame = {
        .cnode = cnode_module,
        .slot = module->mrmod_slot,
    };

    // - Map multiboot module in your address space

    struct frame_identity child_frame_id;
    CHECK("identifying frame",
          frame_identify(child_frame, &child_frame_id));

    void *mapped_elf;
    CHECK("mapping frame",
          paging_map_frame(get_current_paging_state(), &mapped_elf,
                           child_frame_id.bytes, child_frame, NULL, NULL));
    DPRINT("ELF header: %0x %c %c %c", ((char*)mapped_elf)[0], ((char*)mapped_elf)[1], ((char*)mapped_elf)[2], ((char*)mapped_elf)[3]);

    struct Elf32_Ehdr *elf_header = mapped_elf;
    if (!IS_ELF(*elf_header)) {
        DPRINT("Module %s is not an ELF executable", binary_name);
        return ELF_ERR_HEADER;
    }

    // - Setup childs cspace

    // Create L1 CNode
    struct capref c1node;
    struct cnoderef c1node_ref;
    CHECK("new L1 cnode", cnode_create_l1(&c1node, &c1node_ref));
    
    // Link L2 CNode in L1 CNode
    struct cnoderef c2node_ref;
    CHECK("create new L2 cnode", cnode_create_foreign_l2(c1node, c1node.slot, &c2node_ref));
    // - Setup childs vspace
    // - Load the ELF binary
    // - Setup dispatcher
    // - Setup environment
    // - Make dispatcher runnable

    return SYS_ERR_OK;
}
