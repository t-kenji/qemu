/*
 * Kuusou/nue board
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
#include "sysemu/block-backend.h"
#include "sysemu/device_tree.h"
#include "sysemu/reset.h"
#include "hw/boards.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "hw/loader.h"
#include "hw/char/serial.h"
#include "hw/block/flash.h"
#include "hw/ppc/openpic.h"
#include "hw/ppc/ppc.h"
#include "hw/pci/pci.h"
#include "hw/ssi/ssi.h"
#include "hw/i2c/i2c.h"
#include "hw/i2c/smbus_eeprom.h"
#include "elf.h"

#define TYPE_NUE1_MACHINE  MACHINE_TYPE_NAME("nue1")
#define NUE1_MACHINE(obj) OBJECT_CHECK(Nue1MachineState, (obj), TYPE_NUE1_MACHINE)

typedef struct {
    PPCE500MachineState parent_obj;

    uint32_t rcw[16];
} Nue1MachineState;

#define EPAPR_MAGIC (0x45504150)
#define SVR_T1014 (0x85440000)

#define DTC_LOAD_PAD (0x1800000)
#define DTC_PAD_MASK (0xFFFFF)
#define DTB_MAX_SIZE (8 * MiB)
#define INITRD_LOAD_PAD (0x2000000)
#define INITRD_PAD_MASK (0xFFFFFF)
#define UBOOT_ENTRY (0x30000000)
#define UBOOT_SPL_ENTRY (0xFFFD8000)
#define RESET_VECTOR_ADDRESS (0xFFFFFFFC)

#define RAM_SIZES_ALIGN (64 * MiB)

/* TODO: parameterize */
#define T102x_CCSRBAR_SIZE (0x01000000ULL)
#define T102x_MPIC_REGS_OFFSET (0x040000ULL)
#define T102x_UART1_REGS_OFFSET (0x11C500ULL)
#define T102x_UART2_REGS_OFFSET (0x11C600ULL)
#define T102x_UART3_REGS_OFFSET (0x11D500ULL)
#define T102x_UART4_REGS_OFFSET (0x11D600ULL)

struct boot_info
{
    uint32_t dt_base;
    uint32_t dt_size;
    uint32_t entry;
};

/* Create reset TLB entries for BookE, mapping only the flash memory.  */
static void mmubooke_create_initial_mapping_uboot(CPUPPCState *env)
{
    ppcmas_tlb_t *tlb = booke206_get_tlbm(env, 1, 0, 0);
    hwaddr size;

    /* on reset the flash is mapped by a shadow TLB,
     * but since we don't implement them we need to use
     * the same values U-Boot will use to avoid a fault.
     */
    size = (booke206_page_size_to_tlb(256 * MiB) << MAS1_TSIZE_SHIFT);
    tlb->mas1 = MAS1_VALID | size; /* up to 0xFFFFFFFF */
    tlb->mas2 = 0xF0000000ULL & TARGET_PAGE_MASK;
    tlb->mas7_3 = 0xF0000000ULL & TARGET_PAGE_MASK;
    tlb->mas7_3 |= MAS3_UR | MAS3_UW | MAS3_UX | MAS3_SR | MAS3_SW | MAS3_SX;

    env->tlb_dirty = true;
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

    size = (booke206_initial_map_tsize(env) << MAS1_TSIZE_SHIFT);
    tlb->mas1 = MAS1_VALID | size;
    tlb->mas2 = va & TARGET_PAGE_MASK;
    tlb->mas7_3 = pa & TARGET_PAGE_MASK;
    tlb->mas7_3 |= MAS3_UR | MAS3_UW | MAS3_UX | MAS3_SR | MAS3_SW | MAS3_SX;

    env->tlb_dirty = true;
}

static void t102x_cpu_reset_2nd(void *opaque)
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

    env->spr[SPR_E500_SVR] = SVR_T1014;

    /* Set initial guest state. */
    switch (bi->entry) {
    case UBOOT_ENTRY:
        env->nip = bi->entry;
        mmubooke_create_initial_mapping_uboot(env);
        break;
    case UBOOT_SPL_ENTRY:
        env->nip = RESET_VECTOR_ADDRESS;
        mmubooke_create_initial_mapping_uboot(env);
        break;
    default:
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
        break;
    }
}

static DeviceState *t102x_init_mpic(Nue1MachineState *nms,
                                    MemoryRegion *ccsr,
                                    IrqLines *irqs)
{
    MachineState *machine = MACHINE(nms);
    const PPCE500MachineClass *mc = PPCE500_MACHINE_GET_CLASS(nms);
    unsigned int smp_cpus = machine->smp.cpus;
    DeviceState *dev = NULL;
    SysBusDevice *s;

    dev = qdev_create(NULL, TYPE_OPENPIC);
    object_property_add_child(OBJECT(machine), "pic", OBJECT(dev),
                              &error_fatal);
    qdev_prop_set_uint32(dev, "model", mc->mpic_version);
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

static void nue_init(MachineState *machine)
{
    MemoryRegion *address_space_mem = get_system_memory();
    MemoryRegion *ram = g_new(MemoryRegion, 1);
    Nue1MachineState *nms = NUE1_MACHINE(machine);
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
    unsigned int smp_cpus = machine->smp.cpus;
    IrqLines *irqs;
    DeviceState *dev, *mpicdev;
    CPUPPCState *firstenv = NULL;
    MemoryRegion *ccsr_addr_space;
    SysBusDevice *s;
    PPCE500CCSRState *ccsr;

    irqs = g_new0(IrqLines, smp_cpus);
    for (int i = 0; i < smp_cpus; i++) {
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

#define FSL_SYSCLK (400000000)
#define FSL_TBCLK_DIV (16)
        ppc_booke_timers_init(cpu, FSL_SYSCLK / FSL_TBCLK_DIV, PPC_TIMER_E500);

        /* Register reset handler */
        if (!i) {
            /* Primary CPU */
            struct boot_info *boot_info;
            boot_info = g_malloc0(sizeof(struct boot_info));
            qemu_register_reset(t102x_cpu_reset, cpu);
            env->load_info = boot_info;
        } else {
            /* Secondary CPUs */
            qemu_register_reset(t102x_cpu_reset_2nd, cpu);
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
                             ccsr_addr_space, 0, T102x_CCSRBAR_SIZE);
    memory_region_add_subregion(address_space_mem, 0xFE000000,
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

    MemoryRegion *l3_cache = g_new(MemoryRegion, 1);
    memory_region_init_ram(l3_cache, NULL, "t102x-cpc.l3_cache", 256 * KiB,
                           &error_abort);
    memory_region_add_subregion(address_space_mem, 0xFFFC0000, l3_cache);

    /* Work-Area for TLB initialization */
    MemoryRegion *wa = g_new(MemoryRegion, 1);
    memory_region_allocate_system_memory(wa, NULL, "t102x-ccsr.wa", 0x4000);
    memory_region_add_subregion(ccsr_addr_space, 0x03C000, wa);

    /* Device Configuration/Pin Control */
    dev = qdev_create(NULL, "t102x-dcfg");
    s = SYS_BUS_DEVICE(dev);
    qdev_prop_set_ptr(dev, "rcw", nms->rcw);
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

    /* Enhanced serial peripheral interface */
    dev = qdev_create(NULL, "fsl-espi");
    s = SYS_BUS_DEVICE(dev);
    qdev_init_nofail(dev);
    memory_region_add_subregion(ccsr_addr_space, 0x110000,
                                sysbus_mmio_get_region(s, 0));
    {
        SSIBus *spi = (SSIBus *)qdev_get_child_bus(dev, "spi");

        if (spi != NULL) {
            DeviceState *nand = ssi_create_slave_no_init(spi, "spi-nand");

            qdev_prop_set_drive(nand, "drive", blk_by_name("spi-nand"),
                                &error_fatal);
            qdev_prop_set_uint8(nand, "manufacturer_id", NAND_MFR_MICRON);
            qdev_prop_set_uint8(nand, "device_id", 0x35);
            qdev_init_nofail(nand);

            sysbus_connect_irq(SYS_BUS_DEVICE(dev), 1,
                               qdev_get_gpio_in_named(nand, SSI_GPIO_CS, 0));
        }
    }

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

    mpicdev = t102x_init_mpic(nms, ccsr_addr_space, irqs);

    /* Serial */
    static const struct {
        hwaddr addr;
        int irq;
    } uarts[] = {
        {T102x_UART1_REGS_OFFSET, 20},
        {T102x_UART2_REGS_OFFSET, 20},
        {T102x_UART3_REGS_OFFSET, 21},
        {T102x_UART3_REGS_OFFSET, 21},
    };
    for (int i = 0; i < ARRAY_SIZE(uarts); ++i) {
        if (serial_hd(i)) {
            serial_mm_init(ccsr_addr_space, uarts[i].addr, 0,
                           qdev_get_gpio_in(mpicdev, uarts[i].irq), 115200,
                           serial_hd(i), DEVICE_BIG_ENDIAN);
        }
    }

    /* I2C */
    dev = qdev_create(NULL, "mpc-i2c");
    s = SYS_BUS_DEVICE(dev);
    qdev_init_nofail(dev);
    sysbus_connect_irq(s, 0, qdev_get_gpio_in(mpicdev, 22));
    memory_region_add_subregion(ccsr_addr_space, 0x118000,
                                sysbus_mmio_get_region(s, 0));
    {
        I2CBus *i2c = (I2CBus *)qdev_get_child_bus(dev, "i2c");

        if (i2c != NULL) {
            /* SPD EEPROM on RAM module */
            Error *err = NULL;
            uint8_t *spd_data = spd_data_generate(DDR3, ram_size, &err);
            if (err != NULL) {
                warn_report_err(err);
            }
            smbus_eeprom_init_one(i2c, 0x51, spd_data);
        }
    }

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

#if 0
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
printf("dt_base: %#" HWADDR_PRIx "\n", dt_base);
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
#else
    (void)payload_size;
    (void)kernel_as_payload;
    (void)cur_base;
    (void)initrd_size;
    (void)initrd_base;
    (void)kernel_size;
    (void)kernel_base;
    dt_base = 0;
    dt_size = 0;
    bios_entry = loadaddr = UBOOT_SPL_ENTRY;

    rom_add_file_fixed(filename, loadaddr, -1);

    g_free(filename);
#endif

    boot_info = env->load_info;
    boot_info->entry = bios_entry;
    boot_info->dt_base = dt_base;
    boot_info->dt_size = dt_size;
}

static void nue1_fixup_devtree(void *fdt)
{
    const char model[] = "kuusou,nue1";
    const char compatible[] = "kuusou,nue1";
printf("%s: called\n", __func__);

    qemu_fdt_setprop(fdt, "/", "model", model, sizeof(model));
    qemu_fdt_setprop(fdt, "/", "compatible", compatible,
                     sizeof(compatible));
}

static void nue1_init(MachineState *machine)
{
    nue_init(machine);
}

static char *nue1_get_rcw(Object *obj, Error **errp)
{
    Nue1MachineState *s = NUE1_MACHINE(obj);
    char *rcw_str = g_malloc0(256);
    size_t cur = 0;

    for (int i = 0; i < ARRAY_SIZE(s->rcw); i += 4) {
        cur += snprintf(rcw_str + cur, 255 - cur, "%08x %08x %08x %08x\n",
                       s->rcw[i], s->rcw[i + 1], s->rcw[i + 2], s->rcw[i + 3]);
    }

    return rcw_str;
}

static char *get_token(char *string)
{
    char *next = strchr(string, ' ');
    if (next != NULL) {
        *next = '\0';
        ++next;
    }
    return next;
}

static void nue1_set_rcw(Object *obj, const char *value, Error **errp)
{
    static char buf[1024] = {0};

    Nue1MachineState *s = NUE1_MACHINE(obj);
    int fd;

    fd = open(value, O_RDONLY);
    if (fd < 0) {
        error_report("RCW: cannot open %s", value);
        return;
    }

    ssize_t read_len = read(fd, buf, sizeof(buf));
    if (read_len <= 0) {
        error_report("RCW: cannot read %s", value);
    } else {
        char *head = buf, *cur = strchr(head, '\n');
        int count = 0;
        while (cur != NULL) {
            *cur = '\0';

            if (*head != '#') {
                char *next;
                uint32_t value;
                while (head != NULL) {
                    next = get_token(head);
                    value = strtoul(head, NULL, 16);
                    ++count;
                    if (count > 2) {
                        s->rcw[count - 3] = value;
                    }
                    head = next;
                }
            }

            head = cur + 1;
            cur = strchr(head, '\n');
        }
    }

    close(fd);
}

static void nue1_instance_init(Object *obj)
{
    Nue1MachineState *s = NUE1_MACHINE(obj);

    /* Default rcw is T1024RDB default */
    static const uint32_t t1024rdb_rcw[ARRAY_SIZE(s->rcw)] = {
        0x0810000C, 0x00000000, 0x00000000, 0x00000000,
        0x4A800003, 0x80000012, 0x5C027000, 0x21000000,
        0x00000000, 0x00000000, 0x00000000, 0x00030810,
        0x00000000, 0x0B005A08, 0x00000000, 0x00000006,

    };
    memcpy(s->rcw, t1024rdb_rcw, sizeof(s->rcw));
    object_property_add_str(obj, "rcw", nue1_get_rcw,nue1_set_rcw, NULL);
}

static void nue1_machine_class_init(ObjectClass *oc, void *data)
{
    PPCE500MachineClass *pmc = PPCE500_MACHINE_CLASS(oc);
    MachineClass *mc = MACHINE_CLASS(oc);

    pmc->pci_first_slot = 0x1;
    pmc->pci_nr_slots = 3;
    pmc->fixup_devtree = nue1_fixup_devtree;
    pmc->mpic_version = OPENPIC_MODEL_FSL_MPIC_42;
    pmc->ccsrbar_base = 0xFFE000000ULL;
    pmc->pci_pio_base = 0xFF8000000ULL;
    pmc->pci_mmio_base = 0xC00000000ULL;
    pmc->pci_mmio_bus_base = 0xE0000000ULL;
    pmc->spin_base = 0xFEF000000ULL;

    mc->desc = "kuusou/nue1 board";
    mc->init = nue1_init;
    mc->max_cpus = 2;
    mc->default_cpu_type = POWERPC_CPU_TYPE_NAME("e500mc");
}

static void nue_register_types(void)
{
    static const TypeInfo nue1_info = {
        .name          = TYPE_NUE1_MACHINE,
        .parent        = TYPE_PPCE500_MACHINE,
        .class_init    = nue1_machine_class_init,
        .instance_init = nue1_instance_init,
        .instance_size = sizeof(Nue1MachineState),
    };

    type_register_static(&nue1_info);
}
type_init(nue_register_types)
