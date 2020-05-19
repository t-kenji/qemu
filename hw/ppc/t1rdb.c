/*
 * Freescale T-Series board
 *
 * Copyright 2020 t-keni <protect.2501@gmail.com>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of  the GNU General  Public License as published by
 * the Free Software Foundation;  either version 2 of the  License, or
 * (at your option) any later version.
 */

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "qemu/units.h"
#include "qapi/error.h"
#include "cpu.h"
#include "e500.h"
#include "e500-ccsr.h"
#include "sysemu/sysemu.h"
#include "sysemu/device_tree.h"
#include "sysemu/reset.h"
#include "hw/boards.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "hw/loader.h"
#include "hw/char/serial.h"
#include "hw/ppc/openpic.h"
#include "hw/ppc/ppc.h"
#include "hw/pci/pci.h"
#include "hw/i2c/i2c.h"
#include "hw/i2c/smbus_eeprom.h"
#include "elf.h"

#define EPAPR_MAGIC                (0x45504150)
#define DTC_LOAD_PAD               0x1800000
#define DTC_PAD_MASK               0xFFFFF
#define DTB_MAX_SIZE               (8 * MiB)
#define INITRD_LOAD_PAD            0x2000000
#define INITRD_PAD_MASK            0xFFFFFF

#define RAM_SIZES_ALIGN            (64 * MiB)

/* TODO: parameterize */
#define T102x_MPIC_REGS_OFFSET   0x040000ULL
#define T102x_DUART0_REGS_OFFSET 0x11C500ULL
#define T102x_DUART1_REGS_OFFSET 0x11D500ULL

struct boot_info
{
    uint32_t dt_base;
    uint32_t dt_size;
    uint32_t entry;
};

static int t102x_load_device_tree(PPCE500MachineState *pms,
                                  hwaddr addr,
                                  hwaddr initrd_base,
                                  hwaddr initrd_size,
                                  hwaddr kernel_base,
                                  hwaddr kernel_size,
                                  bool dry_run)
{
    QemuOpts *machine_opts = qemu_get_machine_opts();
    const char *dtb_file = qemu_opt_get(machine_opts, "dtb");
    int fdt_size;
    void *fdt;

    char *filename;
    filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, dtb_file);
    if (!filename) {
        return -1;
    }

    fdt = load_device_tree(filename, &fdt_size);
    g_free(filename);
    if (!fdt) {
        return -1;
    }

    if (!dry_run) {
        qemu_fdt_dumpdtb(fdt, fdt_size);
        cpu_physical_memory_write(addr, fdt, fdt_size);
    }

    return fdt_size;
}

typedef struct DeviceTreeParams {
    PPCE500MachineState *machine;
    hwaddr addr;
    hwaddr initrd_base;
    hwaddr initrd_size;
    hwaddr kernel_base;
    hwaddr kernel_size;
    Notifier notifier;
} DeviceTreeParams;

static void t102x_reset_device_tree(void *opaque)
{
    DeviceTreeParams *p = opaque;
    t102x_load_device_tree(p->machine, p->addr, p->initrd_base,
                           p->initrd_size, p->kernel_base, p->kernel_size,
                           false);
}

static void t102x_init_notify(Notifier *notifier, void *data)
{
    DeviceTreeParams *p = container_of(notifier, DeviceTreeParams, notifier);
    t102x_reset_device_tree(p);
}

static int t102x_prep_device_tree(PPCE500MachineState *machine,
                                  hwaddr addr,
                                  hwaddr initrd_base,
                                  hwaddr initrd_size,
                                  hwaddr kernel_base,
                                  hwaddr kernel_size)
{
    DeviceTreeParams *p = g_new(DeviceTreeParams, 1);
    p->machine = machine;
    p->addr = addr;
    p->initrd_base = initrd_base;
    p->initrd_size = initrd_size;
    p->kernel_base = kernel_base;
    p->kernel_size = kernel_size;

    qemu_register_reset(t102x_reset_device_tree, p);
    p->notifier.notify = t102x_init_notify;
    qemu_add_machine_init_done_notifier(&p->notifier);

    /* Issue the device tree loader once, so that we get the size of the blob */
    return t102x_load_device_tree(machine, addr, initrd_base, initrd_size,
                                  kernel_base, kernel_size, true);
}

/* Create -kernel TLB entries for BookE.  */
static int booke206_initial_map_tsize(CPUPPCState *env)
{
    struct boot_info *bi = env->load_info;
    hwaddr dt_end;
    int ps;

    /* Our initial TLB entry needs to cover everything from 0 to
       the device tree top */
    dt_end = bi->dt_base + bi->dt_size;
    ps = booke206_page_size_to_tlb(dt_end) + 1;
    if (ps & 1) {
        /* e500v2 can only do even TLB size bits */
        ps++;
    }
    return ps;
}

static uint64_t mmubooke_initial_mapsize(CPUPPCState *env)
{
    int tsize;

    tsize = booke206_initial_map_tsize(env);
    return (1ULL << 10 << tsize);
}

static void mmubooke_create_initial_mapping(CPUPPCState *env,
                                            target_ulong va,
                                            hwaddr pa)
{
    ppcmas_tlb_t *tlb = booke206_get_tlbm(env, 1, 0, 0);
    hwaddr size;
    int ps;

    ps = booke206_initial_map_tsize(env);
    size = (ps << MAS1_TSIZE_SHIFT);
    tlb->mas1 = MAS1_VALID | size;
    tlb->mas2 = va & TARGET_PAGE_MASK;
    tlb->mas7_3 = pa & TARGET_PAGE_MASK;
    tlb->mas7_3 |= MAS3_UR | MAS3_UW | MAS3_UX | MAS3_SR | MAS3_SW | MAS3_SX;

    env->tlb_dirty = true;
}

static void t102x_cpu_reset_sec(void *opaque)
{
    PowerPCCPU *cpu = opaque;
    CPUState *cs = CPU(cpu);

    cpu_reset(cs);

    /* Secondary CPU starts in halted state for now. Needs to change when
       implementing non-kernel boot. */
    cs->halted = 1;
    cs->exception_index = EXCP_HLT;
}

static void t102x_cpu_reset(void *opaque)
{
    PowerPCCPU *cpu = opaque;
    CPUState *cs = CPU(cpu);
    CPUPPCState *env = &cpu->env;
    struct boot_info *bi = env->load_info;

    cpu_reset(cs);

    /* Set initial guest state. */
    env->nip = bi->entry;
    cs->halted = 0;

    /* Create a mapping for the kernel.  */
    env->gpr[1] = (16 * MiB) - 8;
    env->gpr[3] = bi->dt_base;
    env->gpr[4] = 0;
    env->gpr[5] = 0;
    env->gpr[6] = tswap32(EPAPR_MAGIC);
    env->gpr[7] = mmubooke_initial_mapsize(env);
    env->gpr[8] = 0;
    env->gpr[9] = 0;
    mmubooke_create_initial_mapping(env, 0, 0);
}

static DeviceState *t102x_init_mpic(PPCE500MachineState *pms,
                                    MemoryRegion *ccsr,
                                    IrqLines *irqs)
{
    MachineState *machine = MACHINE(pms);
    const PPCE500MachineClass *pmc = PPCE500_MACHINE_GET_CLASS(pms);
    unsigned int smp_cpus = machine->smp.cpus;
    DeviceState *dev = NULL;
    SysBusDevice *s;

    dev = qdev_create(NULL, TYPE_OPENPIC);
    object_property_add_child(OBJECT(machine), "pic", OBJECT(dev),
                              &error_fatal);
    qdev_prop_set_uint32(dev, "model", pmc->mpic_version);
    qdev_prop_set_uint32(dev, "nb_cpus", smp_cpus);

    qdev_init_nofail(dev);
    s = SYS_BUS_DEVICE(dev);

    int k = 0;
    for (int i = 0; i < smp_cpus; i++) {
        for (int j = 0; j < OPENPIC_OUTPUT_NB; j++) {
            sysbus_connect_irq(s, k++, irqs[i].irq[j]);
        }
    }
    memory_region_add_subregion(ccsr, T102x_MPIC_REGS_OFFSET,
                                s->mmio[0].memory);

    return dev;
}

static void t102x_init(MachineState *machine)
{
    MemoryRegion *address_space_mem = get_system_memory();
    MemoryRegion *ram = g_new(MemoryRegion, 1);
    PPCE500MachineState *pms = PPCE500_MACHINE(machine);
    const PPCE500MachineClass *pmc = PPCE500_MACHINE_GET_CLASS(machine);
    CPUPPCState *env = NULL;
    uint64_t loadaddr;
    hwaddr kernel_base = -1LL;
    int kernel_size = 0;
    hwaddr dt_base = 0;
    hwaddr initrd_base = 0;
    int initrd_size = 0;
    hwaddr cur_base = 0;
    char *filename;
    const char *payload_name;
    bool kernel_as_payload;
    hwaddr bios_entry = 0;
    target_long payload_size;
    struct boot_info *boot_info;
    int dt_size;
    int i;
    unsigned int smp_cpus = machine->smp.cpus;
    IrqLines *irqs;
    DeviceState *dev, *mpicdev;
    CPUPPCState *firstenv = NULL;
    MemoryRegion *ccsr_addr_space;
    SysBusDevice *s;
    PPCE500CCSRState *ccsr;
    I2CBus *i2c;

    irqs = g_new0(IrqLines, smp_cpus);
    for (i = 0; i < smp_cpus; i++) {
        PowerPCCPU *cpu;
        CPUState *cs;
        qemu_irq *input;

        cpu = POWERPC_CPU(cpu_create(machine->cpu_type));
        env = &cpu->env;
        cs = CPU(cpu);

        if (env->mmu_model != POWERPC_MMU_BOOKE206) {
            error_report("MMU model %i not supported by this machine",
                         env->mmu_model);
            exit(1);
        }

        if (!firstenv) {
            firstenv = env;
        }

        input = (qemu_irq *)env->irq_inputs;
        irqs[i].irq[OPENPIC_OUTPUT_INT] = input[PPCE500_INPUT_INT];
        irqs[i].irq[OPENPIC_OUTPUT_CINT] = input[PPCE500_INPUT_CINT];
        env->spr_cb[SPR_BOOKE_PIR].default_value = cs->cpu_index = i;
        env->mpic_iack = pmc->ccsrbar_base + T102x_MPIC_REGS_OFFSET + 0xa0;

        ppc_booke_timers_init(cpu, 400000000, PPC_TIMER_E500);

        /* Register reset handler */
        if (!i) {
            /* Primary CPU */
            struct boot_info *boot_info;
            boot_info = g_malloc0(sizeof(struct boot_info));
            qemu_register_reset(t102x_cpu_reset, cpu);
            env->load_info = boot_info;
        } else {
            /* Secondary CPUs */
            qemu_register_reset(t102x_cpu_reset_sec, cpu);
        }
    }

    env = firstenv;

    /* Fixup Memory size on a alignment boundary */
    ram_size &= ~(RAM_SIZES_ALIGN - 1);
    machine->ram_size = ram_size;

    /* Register Memory */
    memory_region_allocate_system_memory(ram, NULL, "t102x.ram", ram_size);
    memory_region_add_subregion(address_space_mem, 0, ram);

    dev = qdev_create(NULL, "e500-ccsr");
    object_property_add_child(qdev_get_machine(), "e500-ccsr",
                              OBJECT(dev), NULL);
    qdev_init_nofail(dev);
    ccsr = CCSR(dev);
    ccsr_addr_space = &ccsr->ccsr_space;
    memory_region_add_subregion(address_space_mem, pmc->ccsrbar_base,
                                ccsr_addr_space);

    /* aliased 32bit memory region */
    memory_region_init_alias(&ccsr->ccsr_space_alias, NULL, "t102x-ccsr.alias",
                             ccsr_addr_space, 0, 0x1000);
    memory_region_add_subregion(address_space_mem, 0xfe000000,
                                &ccsr->ccsr_space_alias);

    /* Local Configuration Control */
    dev = qdev_create(NULL, "t102x-lcc");
    s = SYS_BUS_DEVICE(dev);
    qdev_init_nofail(dev);
    memory_region_add_subregion(ccsr_addr_space, 0x000000,
                                sysbus_mmio_get_region(s, 0));

    /* Local Access Window */
    dev = qdev_create(NULL, "t102x-law");
    s = SYS_BUS_DEVICE(dev);
    qdev_init_nofail(dev);
    memory_region_add_subregion(ccsr_addr_space, 0x000C00,
                                sysbus_mmio_get_region(s, 0));

    /* DDR */
    dev = qdev_create(NULL, "t102x-ddr");
    s = SYS_BUS_DEVICE(dev);
    qdev_init_nofail(dev);
    memory_region_add_subregion(ccsr_addr_space, 0x008000,
                                sysbus_mmio_get_region(s, 0));

    /* CoreNet Platform Cache */
    dev = qdev_create(NULL, "t102x-cpc");
    s = SYS_BUS_DEVICE(dev);
    qdev_init_nofail(dev);
    memory_region_add_subregion(ccsr_addr_space, 0x010000,
                                sysbus_mmio_get_region(s, 0));

    /* Work-Area for TLB initialization */
    MemoryRegion *wa = g_new(MemoryRegion, 1);
    memory_region_allocate_system_memory(wa, NULL, "t102x-ccsr.wa", 0x4000);
    memory_region_add_subregion(ccsr_addr_space, 0x03C000, wa);

    /* Device Configuration/Pin Control */
    dev = qdev_create(NULL, "t102x-dcfg");
    s = SYS_BUS_DEVICE(dev);
    qdev_init_nofail(dev);
    memory_region_add_subregion(ccsr_addr_space, 0x0E0000,
                                sysbus_mmio_get_region(s, 0));

    /* Clocking */
    dev = qdev_create(NULL, "t102x-clking");
    s = SYS_BUS_DEVICE(dev);
    qdev_init_nofail(dev);
    memory_region_add_subregion(ccsr_addr_space, 0x0E1000,
                                sysbus_mmio_get_region(s, 0));

    /* Run Control/Power Management */
    dev = qdev_create(NULL, "t102x-rcpm");
    s = SYS_BUS_DEVICE(dev);
    qdev_init_nofail(dev);
    memory_region_add_subregion(ccsr_addr_space, 0x0E2000,
                                sysbus_mmio_get_region(s, 0));

    /* Integrated Flash Controller */
    dev = qdev_create(NULL, "fsl-ifc");
    s = SYS_BUS_DEVICE(dev);
    qdev_init_nofail(dev);
    memory_region_add_subregion(ccsr_addr_space, 0x124000,
                                sysbus_mmio_get_region(s, 0));

    /* QUICC Engine */
    dev = qdev_create(NULL, "fsl-quicc");
    s = SYS_BUS_DEVICE(dev);
    qdev_init_nofail(dev);
    memory_region_add_subregion(ccsr_addr_space, 0x140000,
                                sysbus_mmio_get_region(s, 0));

    /* Dual USB PHY */
    dev = qdev_create(NULL, "t102x-usb-phy");
    s = SYS_BUS_DEVICE(dev);
    qdev_init_nofail(dev);
    memory_region_add_subregion(ccsr_addr_space, 0x214000,
                                sysbus_mmio_get_region(s, 0));

    /* PCI Express 1 */
    dev = qdev_create(NULL, "t102x-pex");
    s = SYS_BUS_DEVICE(dev);
    qdev_init_nofail(dev);
    memory_region_add_subregion(ccsr_addr_space, 0x240000,
                                sysbus_mmio_get_region(s, 0));

    /* PCI Express 2 */
    dev = qdev_create(NULL, "t102x-pex");
    s = SYS_BUS_DEVICE(dev);
    qdev_init_nofail(dev);
    memory_region_add_subregion(ccsr_addr_space, 0x250000,
                                sysbus_mmio_get_region(s, 0));

    /* PCI Express 3 */
    dev = qdev_create(NULL, "t102x-pex");
    s = SYS_BUS_DEVICE(dev);
    qdev_init_nofail(dev);
    memory_region_add_subregion(ccsr_addr_space, 0x260000,
                                sysbus_mmio_get_region(s, 0));

    /* Security */
    dev = qdev_create(NULL, "t102x-sec");
    s = SYS_BUS_DEVICE(dev);
    qdev_init_nofail(dev);
    memory_region_add_subregion(ccsr_addr_space, 0x300000,
                                sysbus_mmio_get_region(s, 0));

    /* Queue Manager */
    dev = qdev_create(NULL, "t102x-qman");
    s = SYS_BUS_DEVICE(dev);
    qdev_init_nofail(dev);
    memory_region_add_subregion(ccsr_addr_space, 0x318000,
                                sysbus_mmio_get_region(s, 0));

    /* Buffer Manager */
    dev = qdev_create(NULL, "t102x-bman");
    s = SYS_BUS_DEVICE(dev);
    qdev_init_nofail(dev);
    memory_region_add_subregion(ccsr_addr_space, 0x31A000,
                                sysbus_mmio_get_region(s, 0));

    /* Frame Manager */
    dev = qdev_create(NULL, "t102x-fman");
    s = SYS_BUS_DEVICE(dev);
    qdev_init_nofail(dev);
    memory_region_add_subregion(ccsr_addr_space, 0x400000,
                                sysbus_mmio_get_region(s, 0));

    mpicdev = t102x_init_mpic(pms, ccsr_addr_space, irqs);

    /* Serial */
    if (serial_hd(0)) {
        serial_mm_init(ccsr_addr_space, T102x_DUART0_REGS_OFFSET,
                       0, qdev_get_gpio_in(mpicdev, 20), 115200,
                       serial_hd(0), DEVICE_BIG_ENDIAN);
    }

    if (serial_hd(1)) {
        serial_mm_init(ccsr_addr_space, T102x_DUART1_REGS_OFFSET,
                       0, qdev_get_gpio_in(mpicdev, 21), 115200,
                       serial_hd(1), DEVICE_BIG_ENDIAN);
    }

    /* I2C */
    dev = qdev_create(NULL, "mpc-i2c");
    s = SYS_BUS_DEVICE(dev);
    qdev_init_nofail(dev);
    sysbus_connect_irq(s, 0, qdev_get_gpio_in(mpicdev, 22));
    memory_region_add_subregion(ccsr_addr_space, 0x118000,
                                sysbus_mmio_get_region(s, 0));
    i2c = (I2CBus *)qdev_get_child_bus(dev, "i2c");

    /* SPD EEPROM on RAM module */
    Error *err = NULL;
    uint8_t *spd_data = spd_data_generate(DDR3, ram_size, &err);
    if (err != NULL) {
        warn_report_err(err);
    }
    smbus_eeprom_init_one(i2c, 0x51, spd_data);

    /*
     * Smart firmware defaults ahead!
     *
     * We follow the following table to select which payload we execute.
     *
     *  -kernel | -bios | payload
     * ---------+-------+---------
     *     N    |   Y   | u-boot
     *     N    |   N   | u-boot
     *     Y    |   Y   | u-boot
     *     Y    |   N   | kernel
     *
     * This ensures backwards compatibility with how we used to expose
     * -kernel to users but allows them to run through u-boot as well.
     */
    kernel_as_payload = false;
    if (bios_name == NULL) {
        if (machine->kernel_filename) {
            payload_name = machine->kernel_filename;
            kernel_as_payload = true;
        } else {
            payload_name = "u-boot.e500";
        }
    } else {
        payload_name = bios_name;
    }

    filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, payload_name);

    payload_size = load_elf(filename, NULL, NULL, NULL,
                            &bios_entry, &loadaddr, NULL,
                            1, PPC_ELF_MACHINE, 0, 0);
    if (payload_size < 0) {
        /*
         * Hrm. No ELF image? Try a uImage, maybe someone is giving us an
         * ePAPR compliant kernel
         */
        loadaddr = LOAD_UIMAGE_LOADADDR_INVALID;
        payload_size = load_uimage(filename, &bios_entry, &loadaddr, NULL,
                                   NULL, NULL);
        if (payload_size < 0) {
            error_report("could not load firmware '%s'", filename);
            exit(1);
        }
    }

    g_free(filename);

    if (kernel_as_payload) {
        kernel_base = loadaddr;
        kernel_size = payload_size;
    }

    cur_base = loadaddr + payload_size;
    if (cur_base < 32 * MiB) {
        /* u-boot occupies memory up to 32MB, so load blobs above */
        cur_base = 32 * MiB;
    }

    /* Load bare kernel only if no bios/u-boot has been provided */
    if (machine->kernel_filename && !kernel_as_payload) {
        kernel_base = cur_base;
        kernel_size = load_image_targphys(machine->kernel_filename,
                                          cur_base,
                                          ram_size - cur_base);
        if (kernel_size < 0) {
            error_report("could not load kernel '%s'",
                         machine->kernel_filename);
            exit(1);
        }

        cur_base += kernel_size;
    }

    /* Load initrd. */
    if (machine->initrd_filename) {
        initrd_base = (cur_base + INITRD_LOAD_PAD) & ~INITRD_PAD_MASK;
        initrd_size = load_image_targphys(machine->initrd_filename, initrd_base,
                                          ram_size - initrd_base);

        if (initrd_size < 0) {
            error_report("could not load initial ram disk '%s'",
                         machine->initrd_filename);
            exit(1);
        }

        cur_base = initrd_base + initrd_size;
    }

    /*
     * Reserve space for dtb behind the kernel image because Linux has a bug
     * where it can only handle the dtb if it's within the first 64MB of where
     * <kernel> starts. dtb cannot not reach initrd_base because INITRD_LOAD_PAD
     * ensures enough space between kernel and initrd.
     */
    dt_base = (loadaddr + payload_size + DTC_LOAD_PAD) & ~DTC_PAD_MASK;
    if (dt_base + DTB_MAX_SIZE > ram_size) {
            error_report("not enough memory for device tree");
            exit(1);
    }

    dt_size = t102x_prep_device_tree(pms, dt_base,
                                     initrd_base, initrd_size,
                                     kernel_base, kernel_size);
    if (dt_size < 0) {
        error_report("couldn't load device tree");
        exit(1);
    }
    assert(dt_size < DTB_MAX_SIZE);

    boot_info = env->load_info;
    boot_info->entry = bios_entry;
    boot_info->dt_base = dt_base;
    boot_info->dt_size = dt_size;
}

static void t1024rdb_fixup_devtree(void *fdt)
{
    const char model[] = "fsl,T1024RDB";
    const char compatible[] = "fsl,T1024RDB";

    qemu_fdt_setprop(fdt, "/", "model", model, sizeof(model));
    qemu_fdt_setprop(fdt, "/", "compatible", compatible,
                     sizeof(compatible));
}

static void t1024rdb_init(MachineState *machine)
{
    t102x_init(machine);
}

static void t1024rdb_machine_class_init(ObjectClass *oc, void *data)
{
    PPCE500MachineClass *pmc = PPCE500_MACHINE_CLASS(oc);
    MachineClass *mc = MACHINE_CLASS(oc);

    pmc->pci_first_slot = 0x1;
    pmc->pci_nr_slots = 3;
    pmc->fixup_devtree = t1024rdb_fixup_devtree;
    pmc->mpic_version = OPENPIC_MODEL_FSL_MPIC_42;
    pmc->ccsrbar_base = 0xFFE000000ULL;
    pmc->pci_pio_base = 0xFF8000000ULL;
    pmc->pci_mmio_base = 0xC00000000ULL;
    pmc->pci_mmio_bus_base = 0xE0000000ULL;
    pmc->spin_base = 0xFEF000000ULL;

    mc->desc = "t1024rdb";
    mc->init = t1024rdb_init;
    mc->max_cpus = 2;
    mc->default_cpu_type = POWERPC_CPU_TYPE_NAME("e500mc");
}

#define TYPE_T1024RDB_MACHINE  MACHINE_TYPE_NAME("t1024rdb")

static const TypeInfo t1024rdb_info = {
    .name          = TYPE_T1024RDB_MACHINE,
    .parent        = TYPE_PPCE500_MACHINE,
    .class_init    = t1024rdb_machine_class_init,
};

static void t1024rdb_register_types(void)
{
    type_register_static(&t1024rdb_info);
}
type_init(t1024rdb_register_types)
