/**
 * \file
 * \brief init core booting.
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

errval_t start_core(coreid_t core_to_start, coreid_t my_core_id,
		struct bootinfo *bi)
{
    if (core_to_start == 0 || core_to_start == my_core_id) {
        return SYS_ERR_OK;
    }

    // 1. Fill in an arm_core_data instance.
    // Get a KCB cap;
    struct capref ram;
    CHECK("allocating RAM for KCB", ram_alloc(&ram, OBJSIZE_KCB));

    // Retype into KCB.
    struct capref kcb;
    CHECK("allocating slot for KCB cap", slot_alloc(&kcb));
    CHECK("retyping RAM into KCB",
            cap_retype(kcb, ram, 0, ObjType_KernelControlBlock, OBJSIZE_KCB, 1));

    struct capref core_data_frame;
    size_t retsize;
    CHECK("allocating core_data_frame",
            frame_alloc(&core_data_frame, OBJSIZE_KCB, &retsize));
    CHECK("cloning kcb for core_data", invoke_kcb_clone(kcb, core_data_frame));

    struct arm_core_data* core_data;
    CHECK("mapping core_data frame into vspace",
            paging_map_frame(get_current_paging_state(), (void**) &core_data,
                    BASE_PAGE_SIZE, core_data_frame, NULL, NULL));

    // Fill in init data in the core_data
    char* init_module_name = "init";
    struct mem_region* init_module = multiboot_find_module(bi,
            init_module_name);
    if (init_module == NULL) {
        DPRINT("Module %s not found", init_module_name);
        return SPAWN_ERR_FIND_MODULE;
    }

    struct multiboot_modinfo init_modinfo = {
        .mod_start = init_module->mr_base,
        .mod_end = init_module->mr_base + init_module->mrmod_size,
        .string = init_module->mrmod_data,
        .reserved = 0
    };
    core_data->monitor_module = init_modinfo;

    size_t init_memory_size = 2u * ARM_CORE_DATA_PAGES * BASE_PAGE_SIZE;
    void* init_memory = malloc(init_memory_size);
    core_data->memory_base_start = (uint32_t) init_memory;
    core_data->memory_bytes = init_memory_size;

    core_data->cmdline = (lvaddr_t) core_data->cmdline_buf;

    // 2. Load and relocate the CPU driver.
    char* cpu_driver_module_name = "cpu_omap44xx";
    // Get the binary from multiboot image
    struct mem_region *cpu_driver_module = multiboot_find_module(bi,
            cpu_driver_module_name);
    if (cpu_driver_module == NULL) {
        DPRINT("Module %s not found", cpu_driver_module_name);
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

    // Allocate memory for loading the relocated segment into.
    struct capref relocate_frame;
    CHECK("allocating frame for relocation",
            frame_alloc(&relocate_frame, cpu_driver_frame_id.bytes, &retsize));
    void* relocate_addr;
    CHECK("mapping relocation frame",
            paging_map_frame(get_current_paging_state(), &relocate_addr,
                    retsize, relocate_frame, NULL, NULL));

    struct frame_identity relocate_frame_id;
    CHECK("idenifying relocaion frame",
        frame_identify(relocate_frame, &relocate_frame_id));

    CHECK("loading CPU relocatable segment",
            load_cpu_relocatable_segment(elf_addr,
                    relocate_addr,
                    relocate_frame_id.base + KERNEL_WINDOW,
                    core_data->kernel_load_base,
                    &core_data->got_base));

    // 3. Clean & invalidate cache..
    // for (size_t level = 1; level <= 7; ++level) {
    //     full_cache_op(level, true /*clean*/, true /*invalidate*/);
    // }
    sys_debug_flush_cache();

    sys_armv7_cache_invalidate(relocate_addr, relocate_addr + retsize);
    sys_armv7_cache_clean_pou(relocate_addr, relocate_addr + retsize);
    sys_armv7_cache_clean_poc(relocate_addr, relocate_addr + retsize);

    sys_armv7_cache_invalidate(core_data, core_data + sizeof(core_data));
    sys_armv7_cache_clean_pou(core_data, core_data + sizeof(core_data));
    sys_armv7_cache_clean_poc(core_data, core_data + sizeof(core_data));

    // 4. Spawn second core.
    CHECK("monitor spawn core",
            invoke_monitor_spawn_core(core_to_start, CPU_ARM7,
                    (forvaddr_t) core_data->entry_point));

    return SYS_ERR_OK;
}
