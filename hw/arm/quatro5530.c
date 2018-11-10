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
#include "hw/sysbus.h"
#include "hw/sd/sdhci.h"
#include "hw/sd/sd.h"
#include "hw/ssi/ssi.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "net/net.h"
#include "qemu/error-report.h"
#include "qemu/log.h"
#include "exec/address-spaces.h"
#include "sysemu/sysemu.h"
#include "sysemu/qtest.h"

typedef struct {
    CsrQuatroState soc;
    MemoryRegion ram;
} CsrQuatro5530;

static void write_board_setup(ARMCPU *cpu, const struct arm_boot_info *info)
{
    uint32_t board_setup_blob[] = {
        0xE59F1004, /* ldr r1, [pc, #4] */
        0xEE0E1F10, /* mcr p15, 0, r1, c14, c0, 0 */
        0xE12FFF1E, /* bx lr */
        0x016E3600, /* .word #24000000 */
    };
    rom_add_blob_fixed("board-setup", board_setup_blob,
                       sizeof(board_setup_blob), 0x80001000);
}

static void write_smpboot(ARMCPU *cpu, const struct arm_boot_info *info)
{
    static const uint32_t smpboot[] = {
        0x20005000,
        0x00000001,
    };

    rom_add_blob_fixed_as("smpboot", smpboot, sizeof(smpboot),
                       info->smp_loader_start,
                       arm_boot_address_space(cpu, info));
}

static void reset_secondary(ARMCPU *cpu, const struct arm_boot_info *info)
{
    CPUState *cs = CPU(cpu);
    cpu_set_pc(cs, info->smp_loader_start);
}

static void quatro5530_sdhci_init(CsrQuatroState *s, int port_num)
{
    for (int i = 0; i < port_num; ++i) {
        static const struct {
            hwaddr offset;
            int irq;
        } sdhcis[] = {
            {CSR_QUATRO_SDHCI0_ADDR, CSR_QUATRO_SDIO0_IRQ},
            {CSR_QUATRO_SDHCI1_ADDR, CSR_QUATRO_SDIO1_IRQ},
            {CSR_QUATRO_SDHCI2_ADDR, CSR_QUATRO_SDIO1_IRQ},
        };

        DeviceState *dev = qdev_create(NULL, TYPE_SYSBUS_SDHCI);
        //qdev_prop_set_uint64(dev, "capareg", 0x05E934B4);
        qdev_init_nofail(dev);

        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, sdhcis[i].offset);
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0,
                           qdev_get_gpio_in(DEVICE(&s->a7mpcore),
                                            sdhcis[i].irq));

        DriveInfo *di = drive_get_next(IF_SD);
        BlockBackend *blk = di ? blk_by_legacy_dinfo(di) : NULL;
        DeviceState *carddev = qdev_create(qdev_get_child_bus(dev, "sd-bus"), TYPE_SD_CARD);
        qdev_prop_set_drive(carddev, "drive", blk, &error_abort);
        qdev_init_nofail(carddev);
    }
}

static void quatro5530_fcspi_init(CsrQuatroState *s)
{
    DeviceState *dev = qdev_create(NULL, "quatro5500.fcspi");
    qdev_init_nofail(dev);

    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CSR_QUATRO_FCSPI_ADDR);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0,
                       qdev_get_gpio_in(DEVICE(&s->a7mpcore),
                                        CSR_QUATRO_FCSPI_IRQ));

    DriveInfo *di = drive_get_next(IF_MTD);
    BlockBackend *blk = di ? blk_by_legacy_dinfo(di) : NULL;
    SSIBus *bus = (SSIBus *)qdev_get_child_bus(dev, "spi");
    DeviceState *flashdev = ssi_create_slave_no_init(bus, "n25q512a");
    qdev_prop_set_drive(flashdev, "drive", blk, &error_abort);
    qdev_init_nofail(flashdev);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 1,
                       qdev_get_gpio_in_named(flashdev, SSI_GPIO_CS, 0));
}

static void quatro5530_stmmac_init(CsrQuatroState *s)
{
    if (nd_table[0].used) {
        qemu_check_nic_model(&nd_table[0], "stmmaceth");
        DeviceState *dev = qdev_create(NULL, "stmmaceth");
        SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
        qdev_set_nic_properties(dev, &nd_table[0]);
        qdev_init_nofail(dev);
        sysbus_mmio_map(sbd, 0, CSR_QUATRO_ETHERNET_ADDR);
        sysbus_connect_irq(sbd, 0, qdev_get_gpio_in(DEVICE(&s->a7mpcore), CSR_QUATRO_STMMAC_IRQ));
    }
}

static void quatro5530_init(MachineState *machine)
{
    static struct arm_boot_info binfo;

    CsrQuatro5530 *ms = g_new0(CsrQuatro5530, 1);

    /* Create the memory region to pass to the SoC */
    if (machine->ram_size > CSR_QUATRO_DDR_RAM_SIZE) {
        error_report("ERROR: RAM size " RAM_ADDR_FMT " above max supported of 0x%08x",
                     machine->ram_size, CSR_QUATRO_DDR_RAM_SIZE);
        exit(1);
    }

    binfo = (struct arm_boot_info){
        .loader_start = CSR_QUATRO_DDR_RAM_ADDR,
        .board_id = -1,
        .ram_size = machine->ram_size,
        .kernel_filename = machine->kernel_filename,
        .kernel_cmdline = machine->kernel_cmdline,
        .initrd_filename = machine->initrd_filename,
        .nb_cpus = MIN(smp_cpus, MAX_CPUS),

        .board_setup_addr = 0x80001000,
        .write_board_setup = write_board_setup,
        .secure_board_setup = true,
        .secure_boot = true,
        .smp_loader_start = 0,
        .write_secondary_boot = write_smpboot,
        .secondary_cpu_reset_hook = reset_secondary,
    };

    object_initialize(&ms->soc, sizeof(ms->soc), TYPE_CSR_QUATRO);
    object_property_add_child(OBJECT(machine), "soc", OBJECT(&ms->soc),
                              &error_fatal);
    object_property_set_bool(OBJECT(&ms->soc), true, "realized",
                             &error_fatal);

    quatro5530_sdhci_init(&ms->soc, CSR_QUATRO_NUM_SDHCIS);
    quatro5530_fcspi_init(&ms->soc);
    quatro5530_stmmac_init(&ms->soc);

    memory_region_allocate_system_memory(&ms->ram, NULL, "csr-quatro5530.ram",
                                         machine->ram_size);
    memory_region_add_subregion(get_system_memory(),
                                CSR_QUATRO_DDR_RAM_ADDR, &ms->ram);

    if (!qtest_enabled()) {
        arm_load_kernel(&ms->soc.ap_cpus[0], &binfo);
    }
}

static void quatro5530_machine_class_init(MachineClass *mc)
{
    mc->desc = "CSR Quatro5530 board with 1xA9 and 1xA15, 2xM3";
    mc->init = quatro5530_init;
    mc->max_cpus = MAX_CPUS;
    mc->default_cpus = DEFAULT_CPUS;
    mc->default_ram_size = GiB(1);
    mc->block_default_type = IF_SD;
}

DEFINE_MACHINE("quatro5530", quatro5530_machine_class_init)
