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

void setup_cspace(struct spawninfo *si) {
    errval_t err;
    err = cnode_create_l1(&(si->l1_cap), &(si->l1_cnoderef));
    if(err_is_fail(err)) {
        printf("%s\n", err_getstring(err));
    }

    // Create TASKCN
    err = cnode_create_foreign_l2(si->l1_cap, ROOTCN_SLOT_TASKCN, &(si->taskcn));
    if(err_is_fail(err)) {
        printf("%s\n", err_getstring(err));
    }

    // Create SLOT PAGECN
    err = cnode_create_foreign_l2(si->l1_cap, ROOTCN_SLOT_PAGECN, &(si->pagecn));
    if(err_is_fail(err)) {
        printf("%s\n", err_getstring(err));
    }

    // Create SLOT BASE PAGE CN
    err = cnode_create_foreign_l2(si->l1_cap, ROOTCN_SLOT_BASE_PAGE_CN, &(si->base_pagecn));
    if(err_is_fail(err)) {
        printf("%s\n", err_getstring(err));
    }

    // Create SLOT ALLOC 0
    err = cnode_create_foreign_l2(si->l1_cap, ROOTCN_SLOT_SLOT_ALLOC0, &(si->alloc0));
    if(err_is_fail(err)) {
        printf("%s\n", err_getstring(err));
    }

    // Create SLOT ALLOC 1
    err = cnode_create_foreign_l2(si->l1_cap, ROOTCN_SLOT_SLOT_ALLOC1, &(si->alloc1));
    if(err_is_fail(err)) {
        printf("%s\n", err_getstring(err));
    }

    // Create SLOT ALLOC 2
    err = cnode_create_foreign_l2(si->l1_cap, ROOTCN_SLOT_SLOT_ALLOC2, &(si->alloc2));
    if(err_is_fail(err)) {
        printf("%s\n", err_getstring(err));
    }

    // Create SLOT DISPATCHER
    si->dispatcher.cnode = si->taskcn;
    si->dispatcher.slot = TASKCN_SLOT_DISPATCHER;

    // err = cap_copy(si->dispatcher, cap_dispatcher);
    // if(err_is_fail(err)) {
    //     printf("%s\n", err_getstring(err));
    // }

    // Create SLOT ROOTCN
    si->rootcn.cnode = si->taskcn;
    si->rootcn.slot = TASKCN_SLOT_ROOTCN;

    // err = cap_copy(si->rootcn, cap_root);
    // if(err_is_fail(err)) {
    //     printf("%s\n", err_getstring(err));
    // }

    // Create SLOT DISPFRAME
    si->dispframe.cnode = si->taskcn;
    si->dispframe.slot = TASKCN_SLOT_DISPFRAME;

    // err = cap_copy(si->dispframe, cap_dispframe);
    // if(err_is_fail(err)) {
    //     printf("%s\n", err_getstring(err));
    // }

    // Create SLOT ARGSPG
    si->argspg.cnode = si->taskcn;
    si->argspg.slot = TASKCN_SLOT_ARGSPAGE;

    // err = cap_copy(si->argspg, cap_argcn);
    // if(err_is_fail(err)) {
    //     printf("%s\n", err_getstring(err));
    // }

    cap_retype(si->selfep, si->dispatcher, 0, ObjType_EndPoint, 0, 1);

}

void setup_vspace(struct spawninfo *si) {
    // 1. Create slot allocator for child vspace.
    size_t bufsize = SINGLE_SLOT_ALLOC_BUFLEN(L2_CNODE_SLOTS);
    void *buf = malloc(bufsize);
    assert(buf != NULL);

    struct capref pagecn_cap = {
        .cnode = si->l1_cnoderef,
        .slot = ROOTCN_SLOT_PAGECN
    };
    errval_t err = single_slot_alloc_init_raw(
            &si->ssa,
            pagecn_cap,
            si->pagecn,
            L2_CNODE_SLOTS,
            buf,
            bufsize);
    if (err_is_fail(err)) {
        printf("%s\n", err_getstring(err));
        return;
    }

    // 2. Create child paging state.
    // TODO: Where should the pdir capref come from?
    struct capref pdir;
    err = paging_init_state(&si->pg_state, VADDR_OFFSET, pdir, &si->ssa.a);
    if (err_is_fail(err)) {
        printf("%s\n", err_getstring(err));
        return;
    }
    
    // 3. Create child L1 pagetable.
    si->pg_state.l1_pagetable.cnode = si->pagecn;
    si->pg_state.l1_pagetable.slot = 0;
    err = vnode_create(si->pg_state.l1_pagetable, ObjType_VNode_ARM_l1);
    if (err_is_fail(err)) {
        printf("%s\n", err_getstring(err));
    }
}

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

    setup_cspace(si);
    //CHECK("Setup CSPACE", setup_cspace(si));
    // - Setup childs vspace
    // - Load the ELF binary
    // - Setup dispatcher
    // - Setup environment
    // - Make dispatcher runnable

    return SYS_ERR_OK;
}
