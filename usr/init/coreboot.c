/**
 * \file
 * \brief user-space core booting.
 */

/*
 * Copyright (c) 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include "coreboot.h"

errval_t map_urpc_frame_to_vspace(void** ret, size_t bytes,
        coreid_t my_core_id) {
    size_t retsize;
    if (my_core_id == 0) {
        CHECK("allocating memory for URPC frame",
            frame_alloc(&cap_urpc, bytes, &retsize));
    } else {
        retsize = bytes;
    }
    
    return paging_map_frame(get_current_paging_state(), ret, retsize, cap_urpc,
            NULL, NULL);
}

void write_to_urpc(void* urpc_buf, genpaddr_t base, gensize_t size,
        struct bootinfo* bi, coreid_t my_core_id)
{
    if (my_core_id != 0) {
        return;
    }

    // Check if there is data already produced
    char first_word = *(char*)(urpc_buf);
    urpc_buf += 2*sizeof(char);
    char second_word = *(char*)(urpc_buf);
    *(char*)(urpc_buf) = 'P';
    urpc_buf += 2*sizeof(char);

    *((genpaddr_t*) urpc_buf) = base;
    urpc_buf += sizeof(genpaddr_t);
    *((gensize_t*) urpc_buf) = size;

    urpc_buf += sizeof(gensize_t);
    *((struct bootinfo*) urpc_buf) = *bi;

    urpc_buf = (void*) ROUND_UP((uintptr_t) urpc_buf + sizeof(struct bootinfo), 4);
    memcpy(urpc_buf, bi->regions, bi->regions_length * sizeof(struct mem_region));

    // mmstrings cap, for reading up modules.
    struct capref mmstrings_cap = {
        .cnode = cnode_module,
        .slot = 0
    };
    struct frame_identity mmstrings_id;
    frame_identify(mmstrings_cap, &mmstrings_id);

    urpc_buf = (void*) ROUND_UP((uintptr_t) urpc_buf + bi->regions_length * sizeof(struct mem_region), 4);
    *((genpaddr_t*) urpc_buf) = mmstrings_id.base;
    urpc_buf += sizeof(genpaddr_t);
    *((gensize_t*) urpc_buf) = mmstrings_id.bytes;

    size_t non_zero_slot_count = 0;
    for (size_t i = 0; i < bi->regions_length; ++i) {
        if (bi->regions[i].mrmod_slot != 0) {
            ++non_zero_slot_count;
        }
    }
    urpc_buf += sizeof(gensize_t);
    *((size_t*) urpc_buf) = non_zero_slot_count;
    urpc_buf += sizeof(size_t);
    
    for (size_t i = 0; i < bi->regions_length; ++i) {
        if (bi->regions[i].mrmod_slot != 0) {
            struct capref module = {
                .cnode = cnode_module,
                .slot = bi->regions[i].mrmod_slot
            };
            struct frame_identity module_id;
            frame_identify(module, &module_id);

            *((cslot_t*) urpc_buf) = module.slot;
            urpc_buf += sizeof(cslot_t);
            *((genpaddr_t*) urpc_buf) = module_id.base;
            urpc_buf += sizeof(genpaddr_t);
            *((gensize_t*) urpc_buf) = module_id.bytes;
            urpc_buf += sizeof(gensize_t);
        }
    }
}

errval_t read_from_urpc(void* urpc_buf, struct bootinfo** bi,
        coreid_t my_core_id)
{
    if (my_core_id == 0) {
        return SYS_ERR_OK;
    }
    char first_word = *(char*)(urpc_buf);
    urpc_buf += 2*sizeof(char);
    char second_word = *(char*)(urpc_buf);
    *(char*)(urpc_buf) = 'P';
    urpc_buf += 2*sizeof(char);
     
    printf("Read: First: %c Second: %c\n", first_word, second_word);

    genpaddr_t* base = (genpaddr_t*) urpc_buf;
    urpc_buf += sizeof(genpaddr_t);
    gensize_t* size = (gensize_t*) urpc_buf;

    struct capref mem_cap = {
        .cnode = cnode_super,
        .slot = 0,
    };
    CHECK("forging RAM cap", ram_forge(mem_cap, *base, *size, my_core_id));

    // mmstrings cap, for reading up modules.
    // urpc_buf += sizeof(gensize_t);
    // genpaddr_t* mmstrings_base = (genpaddr_t*) urpc_buf;
    // urpc_buf += sizeof(genpaddr_t);
    // gensize_t* mmstrings_size = (gensize_t*) urpc_buf;

    // struct capref mmstrings_cap = {
    //     .cnode = cnode_module,
    //     .slot = 0
    // };
    // CHECK("creating mmstrings L2Cnode",
    //         cnode_create_l2(&mmstrings_cap, &mmstrings_cap.cnode));
    // CHECK("forging mmstrings cap",
    //         frame_forge(mmstrings_cap, *mmstrings_base, *mmstrings_size,
    //                 my_core_id));

    urpc_buf += sizeof(gensize_t);
    *bi = (struct bootinfo*) urpc_buf;

    urpc_buf = (void*) ROUND_UP((uintptr_t) urpc_buf + sizeof(struct bootinfo), 4);
    memcpy((*bi)->regions, urpc_buf, (*bi)->regions_length * sizeof(struct mem_region));

    return SYS_ERR_OK;
}

errval_t start_core(coreid_t core_to_start, coreid_t my_core_id,
        struct bootinfo* bi)
{
    if (core_to_start == 0 || core_to_start == my_core_id) {
        return SYS_ERR_OK;
    }

    // 1. Get an arm_core_data instance from a KCB cap.
    // Get a KCB cap.
    struct capref ram;
    CHECK("allocating RAM for KCB", ram_alloc(&ram, OBJSIZE_KCB));

    // Retype into KCB.
    struct capref kcb_frame;
    CHECK("allocating slot for KCB cap", slot_alloc(&kcb_frame));
    CHECK("retyping RAM into KCB",
            cap_retype(kcb_frame, ram, 0, ObjType_KernelControlBlock,
                    OBJSIZE_KCB, 1));

    // Allocate a core_data_frame.
    struct capref core_data_frame;
    size_t retsize;
    CHECK("allocating core_data_frame",
            frame_alloc(&core_data_frame, OBJSIZE_KCB, &retsize));
    struct frame_identity core_data_frame_id;
    CHECK("identifying core_data frame",
            frame_identify(core_data_frame, &core_data_frame_id));

    // Clone KCB into core_data_frame.
    CHECK("cloning KCB into core_data frame",
            invoke_kcb_clone(kcb_frame, core_data_frame));

    struct arm_core_data* core_data;
    CHECK("mapping core_data frame into vspace",
            paging_map_frame(get_current_paging_state(), (void**) &core_data,
                    retsize, core_data_frame, NULL, NULL));

    // Fill in core_data->kcb.
    struct frame_identity kcb_frame_id;
    CHECK("identifying KCB frame", frame_identify(kcb_frame, &kcb_frame_id));
    core_data->kcb = kcb_frame_id.base;

    // Fill in init info.
    char* init_module_name = "init";
    struct mem_region* init_module = multiboot_find_module(bi,
            init_module_name);
    if (init_module == NULL) {
        debug_printf("Module %s not found", init_module_name);
        return SPAWN_ERR_FIND_MODULE;
    }

    struct multiboot_modinfo init_modinfo = {
        .mod_start = init_module->mr_base,
        .mod_end = init_module->mr_base + init_module->mrmod_size,
        .string = init_module->mrmod_data,
        .reserved = 0
    };
    core_data->monitor_module = init_modinfo;

    struct capref init_frame;
    CHECK("allocating init frame",
            frame_alloc(&init_frame, 8u * ARM_CORE_DATA_PAGES * BASE_PAGE_SIZE,
                    &retsize));
    struct frame_identity init_frame_id;
    CHECK("identifying init frame", frame_identify(init_frame, &init_frame_id));
    core_data->memory_base_start = init_frame_id.base;
    core_data->memory_bytes = init_frame_id.bytes;

    // core_data->init_name = init_module_name;
    for (size_t i = 0; i < 4; ++i) {
        core_data->init_name[i] = init_module_name[i];
    }
    core_data->init_name[4] = '\0';

    // cmdline, src, dst.
    core_data->cmdline = offsetof(struct arm_core_data, cmdline_buf) + core_data_frame_id.base;
    core_data->src_core_id = my_core_id;
    core_data->dst_core_id = core_to_start;

    // URPC frame.
    struct frame_identity urpc_frame_id;
    CHECK("identifying URPC frame", frame_identify(cap_urpc, &urpc_frame_id));
    core_data->urpc_frame_base = urpc_frame_id.base;
    core_data->urpc_frame_size = urpc_frame_id.bytes;

    // 2. Map CPU driver ELF into vspace.
    char* cpu_driver_module_name = "cpu_omap44xx";
    // Get the binary from multiboot image
    struct mem_region *cpu_driver_module = multiboot_find_module(bi,
            cpu_driver_module_name);
    if (cpu_driver_module == NULL) {
        debug_printf("Module %s not found", cpu_driver_module_name);
        return SPAWN_ERR_FIND_MODULE;
    }

    struct capref cpu_driver_frame = {
        .cnode = cnode_module,
        .slot = cpu_driver_module->mrmod_slot,
    };

    // Map multiboot module in my address space.
    struct frame_identity cpu_driver_frame_id;
    CHECK("identifying elf frame",
          frame_identify(cpu_driver_frame, &cpu_driver_frame_id));

    void* elf_addr;
    CHECK("mapping elf frame",
            paging_map_frame(get_current_paging_state(), &elf_addr,
                    cpu_driver_frame_id.bytes, cpu_driver_frame, NULL, NULL));

    // 3. Allocate & map frame for relocated segment.
    struct capref rel_seg_frame;
    CHECK("allocating relocated segment frame",
            frame_alloc(&rel_seg_frame, cpu_driver_frame_id.bytes, &retsize));
    struct frame_identity rel_seg_frame_id;
    CHECK("identifying relocated segment frame",
            frame_identify(rel_seg_frame, &rel_seg_frame_id));

    void* rel_seg_addr;
    CHECK("mapping relocated segment frame",
            paging_map_frame(get_current_paging_state(), &rel_seg_addr,
                    retsize, rel_seg_frame, NULL, NULL));

    // 4. Load & relocate relocatable segment.
    CHECK("loading CPU relocatable segment",
            load_cpu_relocatable_segment(elf_addr,
                    rel_seg_addr,
                    rel_seg_frame_id.base,// + KERNEL_WINDOW,
                    core_data->kernel_load_base,
                    &core_data->got_base));

    // 4. Clean & invalidate cache.
    // sys_armv7_cache_invalidate((void*) (uint32_t) core_data_frame_id.base,
    //      (void*) (uint32_t) (core_data_frame_id.base + core_data_frame_id.bytes - 1));
    // sys_armv7_cache_clean_pou((void*) (uint32_t) core_data_frame_id.base,
    //      (void*) (uint32_t) (core_data_frame_id.base + core_data_frame_id.bytes - 1));
    sys_armv7_cache_clean_poc((void*) (uint32_t) core_data_frame_id.base,
            (void*) (uint32_t) (core_data_frame_id.base + core_data_frame_id.bytes - 1));

    // 5. Spawn the second core (plz God).
    CHECK("monitor spawn core",
            invoke_monitor_spawn_core(core_to_start, CPU_ARM7,
                    (forvaddr_t) core_data_frame_id.base));

    return SYS_ERR_OK;
}

errval_t read_modules(void* urpc_buf, struct bootinfo* bi,
        coreid_t my_core_id) {
    if (my_core_id == 0) {
        return SYS_ERR_OK;
    }
    
    // mmstrings cap, for reading up modules.
    urpc_buf += 4*sizeof(char);
    urpc_buf += sizeof(genpaddr_t);
    urpc_buf += sizeof(gensize_t);
    urpc_buf = (void*) ROUND_UP((uintptr_t) urpc_buf + sizeof(struct bootinfo), 4);
    urpc_buf = (void*) ROUND_UP((uintptr_t) urpc_buf + bi->regions_length * sizeof(struct mem_region), 4);

    genpaddr_t* mmstrings_base = (genpaddr_t*) urpc_buf;
    urpc_buf += sizeof(genpaddr_t);
    gensize_t* mmstrings_size = (gensize_t*) urpc_buf;

    struct capref mmstrings_cap = {
        .cnode = cnode_module,
        .slot = 0
    };

    struct capref l1cnode = {
        .cnode = cnode_task,
        .slot = TASKCN_SLOT_ROOTCN
    };

    CHECK("creating foreign mmstrings L2Cnode",
            cnode_create_foreign_l2(l1cnode, ROOTCN_SLOT_MODULECN,
                             &cnode_module));
    debug_printf("cnode_module: root: %u, cnode: %u, level: %u\n",
            cnode_module.croot, cnode_module.cnode >> 8, cnode_module.level);
    CHECK("forging mmstrings cap",
            frame_forge(mmstrings_cap, *mmstrings_base, *mmstrings_size,
                    my_core_id));

    urpc_buf += sizeof(gensize_t);
    size_t* non_zero_slot_count = (size_t*) urpc_buf;
    urpc_buf += sizeof(size_t);

    for (size_t i = 0; i < *non_zero_slot_count; ++i) {
        // Read slot, base and size & forge cap.
        cslot_t* module_slot = (cslot_t*) urpc_buf;
        urpc_buf += sizeof(cslot_t);
        genpaddr_t* module_base = (genpaddr_t*) urpc_buf;
        urpc_buf += sizeof(genpaddr_t);
        gensize_t* module_size = (gensize_t*) urpc_buf;
        urpc_buf += sizeof(gensize_t);

        struct capref module_cap = {
            .cnode = cnode_module,
            .slot = *module_slot
        };
        CHECK("forging module cap",
                frame_forge(module_cap, *module_base, *module_size,
                        my_core_id));
    }

    return SYS_ERR_OK;
}