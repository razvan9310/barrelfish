#include <aos/aos.h>
#include <spawn/spawn.h>

#include <elf/elf.h>
#include <aos/dispatcher_arch.h>
#include <barrelfish_kpi/paging_arm_v7.h>
#include <barrelfish_kpi/domain_params.h>
#include <spawn/multiboot.h>

#define DPRINT(fmt, args...)  debug_printf("spawn: " fmt "\n", args)

#define CHECK(where, err)  \
        if (err_is_fail(err)) {  \
            DPRINT("ERROR %s: %s", where, err_getstring(err));  \
            return err;  \
        }

#define CHECK_COND(what, cond, err)  \
        if (!(cond)) {  \
            DPRINT("FAIL %s: %s (%s)\n", what, err_getstring(err), #cond);  \
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
    si->l1_pagetable.cnode = si->pagecn;
    si->l1_pagetable.slot = 0;

//     //CHECK("", cap_copy(si->l1_pagetable, cnode_page));
     vnode_create(, ObjType_VNode_ARM_l1);
}


// Stuffs the ELF in the memory, preparing it for execution:
// - mapping sections where they need to be
// - dealing with the GOT
static errval_t load_elf_into_memory(lvaddr_t base, size_t size, genvaddr_t *entry_point, genvaddr_t *got_ubase) {
    int state = 42;  // TODO

    // stuff the sections into memory
    return elf_load(EM_ARM, elf_section_allocate, (void*)&state,
                    base, size, entry_point);

    // find the GOT -- this will be needed by the dispatcher
    struct Elf32_Shdr *got_shdr = elf32_find_section_header_name(base, size, ".got");
    *got_ubase = got_shdr->sh_addr;
}

// Handles ELF sections.
// I infer that...
//  - base is the requested virtual address -- "please make me appear here in
//    the child process's vspace", says the section header
//  - size is size
//  - flags is
//  - dest is the address in this vspace where we need to copy things (so that they appear there)
//  - state should be just paging_state (which will be in spawninfo)
static errval_t elf_section_allocate(void *state, genvaddr_t base, size_t size,
                                     uint32_t flags, void **ret) {
    return LIB_ERR_NOT_IMPLEMENTED;
}


// TODO maybe void, depending on whether I allocate the frame here or not...
static errval_t setup_dispatcher(struct spawninfo *si, void *disp_frame) {
    si->disp_handle = (dispatcher_handle_t)disp_frame; // TODO here?
    struct dispatcher_shared_generic *disp = get_dispatcher_shared_generic(si->disp_handle);
    struct dispatcher_generic *disp_gen = get_dispatcher_generic(si->disp_handle);
    struct dispatcher_shared_arm *disp_arm = get_dispatcher_shared_arm(si->disp_handle);
    arch_registers_state_t *enabled_area = dispatcher_get_enabled_save_area(si->disp_handle);
    arch_registers_state_t *disabled_area = dispatcher_get_disabled_save_area(si->disp_handle);

    disp_gen->core_id = 0; // we're single-core right now
    // disp->udisp = TODO map to child's vspace // Virtual address of the dispatcher frame in childs VSpace
    disp->disabled = 1; // Start in disabled mode
    disp->fpu_trap = 1; // Trap on fpu instructions
    strncpy(disp->name, si->binary_name, DISP_NAME_LEN); // A name (for debugging)

    disabled_area->named.pc = si->entry_point; // Set program counter (where it should start to execute)

    // Initialize offset registers
    disp_arm->got_base = si->got_ubase; // Address of .got in childs VSpace.
    enabled_area->regs[REG_OFFSET(PIC_REGISTER)]  = si->got_ubase; // same as above
    disabled_area->regs[REG_OFFSET(PIC_REGISTER)] = si->got_ubase; // same as above
    enabled_area->named.cpsr = CPSR_F_MASK | ARM_MODE_USR;
    disabled_area->named.cpsr = CPSR_F_MASK | ARM_MODE_USR;
    disp_gen->eh_frame = 0;
    disp_gen->eh_frame_size = 0;
    disp_gen->eh_frame_hdr = 0;
    disp_gen->eh_frame_hdr_size = 0;
    return SYS_ERR_OK;
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

    lvaddr_t mapped_elf;
    CHECK("mapping frame",
          paging_map_frame(get_current_paging_state(), (void**)&mapped_elf,
                           child_frame_id.bytes, child_frame, NULL, NULL));
    // DPRINT("ELF header: %0x %c %c %c", ((char*)mapped_elf)[0], ((char*)mapped_elf)[1], ((char*)mapped_elf)[2], ((char*)mapped_elf)[3]);

    // this gets checked twice (second time in elf_load)... so what :D
    struct Elf32_Ehdr *elf_header = (void*)mapped_elf;
    if (!IS_ELF(*elf_header)) {
        DPRINT("Module %s is not an ELF executable", binary_name);
        return ELF_ERR_HEADER;
    }

    // - Setup childs cspace

    setup_cspace(si);
    //CHECK("Setup CSPACE", setup_cspace(si));

    // - Setup childs vspace

    // TODO assume that someone gives me a dispatcher frame here, and also stuffs its capability into "the correct" slot
    //      assume it is called disp_frame

    // - Load the ELF binary
    CHECK("loading ELF", load_elf_into_memory(mapped_elf, child_frame_id.bytes,
                                              &si->entry_point, &si->got_ubase));

    // - Setup dispatcher
    CHECK("setting up dispatcher", setup_dispatcher(si, disp_frame));

    // - Setup environment
    // 1. get the frame from SLOT_ARGSPACE
    // 2. cast that frame into struct spawn_domain_params
    // 3. stuff the multiboot's params here

    // - Make dispatcher runnable

    return SYS_ERR_OK;
}
