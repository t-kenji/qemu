/*
 * CSR Quatro 5530 Evaluation board emulation
 *
 * Copyright (C) 2018 t-kenji <protect.2501@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "cpu.h"
#include "hw/arm/csr-quatro.h"
#include "hw/boards.h"
#include "qemu/error-report.h"
#include "qemu/log.h"
#include "exec/address-spaces.h"
#include "sysemu/sysemu.h"
#include "sysemu/qtest.h"

typedef struct {
    CsrQuatroState soc;
    MemoryRegion ram;
} CsrQuatro5530;

static void quatro5530_init(MachineState *machine)
{
    static struct arm_boot_info binfo;

    CsrQuatro5530 *ms = g_new0(CsrQuatro5530, 1);

    /* Create the memory region to pass to the SoC */
    if (machine->ram_size > CSR_QUATRO_MAX_RAM_SIZE) {
        error_report("ERROR: RAM size " RAM_ADDR_FMT " above max supported of 0x%08x",
                     machine->ram_size, CSR_QUATRO_MAX_RAM_SIZE);
        exit(1);
    }

    binfo = (struct arm_boot_info){
        .loader_start = CSR_QUATRO_DDR_RAM_ADDR,
        .board_id = -1,
        .ram_size = machine->ram_size,
        .kernel_filename = machine->kernel_filename,
        .kernel_cmdline = machine->kernel_cmdline,
        .initrd_filename = machine->initrd_filename,
        .nb_cpus = MIN(smp_cpus, CSR_QUATRO_NUM_MPU_CPUS),
    };

    object_initialize(&ms->soc, sizeof(ms->soc), TYPE_CSR_QUATRO);
    object_property_add_child(OBJECT(machine), "soc", OBJECT(&ms->soc),
                              &error_fatal);
    object_property_set_bool(OBJECT(&ms->soc), true, "realized",
                             &error_fatal);

    memory_region_allocate_system_memory(&ms->ram, NULL, "csr-quatro5530.ram",
                                         machine->ram_size);
    memory_region_add_subregion(get_system_memory(),
                                CSR_QUATRO_DDR_RAM_ADDR, &ms->ram);

    if (!qtest_enabled()) {
        arm_load_kernel(&ms->soc.mpu_cpus[0], &binfo);
    }
}

static void quatro5530_machine_class_init(MachineClass *mc)
{
    mc->desc = "CSR Quatro5530 board with 1xA9 and 1xA15, 2xM3";
    mc->init = quatro5530_init;
    mc->max_cpus = CSR_QUATRO_NUM_MPU_CPUS + CSR_QUATRO_NUM_MCU_CPUS;
    mc->default_cpus = CSR_QUATRO_NUM_MPU_CPUS;
}

DEFINE_MACHINE("quatro5530", quatro5530_machine_class_init)
