/*
 * Freescale T1 SoC definitions.
 *
 * Copyright (c) 2020 t-kenji <protect.2501@gmail.com>
 */

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "qemu/units.h"
#include "qapi/error.h"
#include "cpu.h"
#include "sysemu/sysemu.h"
#include "sysemu/reset.h"
#include "hw/boards.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "hw/loader.h"
#include "hw/char/serial.h"
#include "hw/ppc/openpic.h"
#include "hw/ppc/ppc.h"
#include "e500.h"
#include "e500-ccsr.h"
#include "fsl-t1.h"

#define EPAPR_MAGIC (0x45504150)

enum {
    T102X__RAM_SIZES_ALIGN  = 64 * MiB,

    T102X__CCSRBAR_BASE = 0xFFE000000ULL,
    T102X__CCSRBAR_SIZE = 0x01000000ULL,

    T102X__LCC_OFFSET = 0x000000,
    T102X__LAW_OFFSET = 0x000C00,
    T102X__DDR_OFFSET = 0x008000,
    T102X__CPC_OFFSET = 0x010000,
    T102X__MPIC_OFFSET = 0x040000,
    T102X__DCFG_OFFSET = 0x0E0000,
    T102X__CLKING_OFFSET = 0x0E1000,
    T102X__RCPM_OFFSET = 0x0E2000,
    T102X__ESPI_OFFSET = 0x110000,
    T102X__UART1_OFFSET = 0x11C500,
    T102X__UART2_OFFSET = 0x11C600,
    T102X__UART3_OFFSET = 0x11D500,
    T102X__UART4_OFFSET = 0x11D600,
    T102X__I2C_OFFSET = 0x118000,
    T102X__IFC_OFFSET = 0x124000,
    T102X__QUICC_OFFSET = 0x140000,
    T102X__USBPHY_OFFSET = 0x214000,
    T102X__PEX1_OFFSET = 0x240000,
    T102X__PEX2_OFFSET = 0x250000,
    T102X__PEX3_OFFSET = 0x260000,
    T102X__SEC_OFFSET = 0x300000,
    T102X__QMAN_OFFSET = 0x318000,
    T102X__BMAN_OFFSET = 0x31A000,
    T102X__FMAN_OFFSET = 0x400000,

    T102X__SYSCLK = 400000000,
    T102X__TBCLK_DIV = 16,
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
    PPCBootInfo *bi = env->load_info;
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
     * implementing non-kernel boot.
     */
    cs->halted = 1;
    cs->exception_index = EXCP_HLT;
}

static void t102x_cpu_reset(void *opaque)
{
    PowerPCCPU *cpu = opaque;
    CPUState *cs = CPU(cpu);
    CPUPPCState *env = &cpu->env;
    PPCBootInfo *bi = env->load_info;

    cpu_reset(cs);

    /* Set initial guest state. */
    switch (bi->entry) {
    case FSL_T102X__UBOOT_ENTRY:
    case FSL_T102X__RESET_VECTOR_ADDRESS:
        env->nip = bi->entry;
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

static DeviceState *t102x_mpic_init(FslT102xState *s, MemoryRegion *ccsr,
                                    hwaddr offset)
{
    MachineState *machine = MACHINE(qdev_get_machine());
    const FslT102xClass *tc = FSL_T102X_GET_CLASS(s);
    unsigned int smp_cpus = machine->smp.cpus;
    DeviceState *dev = NULL;
    SysBusDevice *sbd;

    dev = qdev_create(NULL, TYPE_OPENPIC);
    object_property_add_child(OBJECT(machine), "pic", OBJECT(dev),
                              &error_fatal);
    qdev_prop_set_uint32(dev, "model", tc->mpic_version);
    qdev_prop_set_uint32(dev, "nb_cpus", smp_cpus);

    qdev_init_nofail(dev);
    sbd = SYS_BUS_DEVICE(dev);

    int k = 0;
    for (int i = 0; i < smp_cpus; i++) {
        for (int j = 0; j < OPENPIC_OUTPUT_NB; j++) {
            sysbus_connect_irq(sbd, k++, s->irqs[i].irq[j]);
        }
    }
    memory_region_add_subregion(ccsr, offset, sbd->mmio[0].memory);

    return dev;
}

static void t102x_realize(DeviceState *dev, Error **errp)
{
    MachineState *machine = MACHINE(qdev_get_machine());
    FslT102xState *s = FSL_T102X(dev);
    const FslT102xClass *tc = FSL_T102X_GET_CLASS(s);
    PPCBootInfo *bi = g_malloc0(sizeof(*bi));
    unsigned int smp_cpus = machine->smp.cpus;
    MemoryRegion *address_space_mem = get_system_memory();
    MemoryRegion *ccsr_addr_space;
    PPCE500CCSRState *ccsr;
    DeviceState *peridev, *mpicdev;
    SysBusDevice *sbd;

    if (smp_cpus > FSL_T102X__NUM_CPUS) {
        error_setg(errp, "%s: Only %d CPUs are supported (%d requested)",
                   TYPE_FSL_T102X, FSL_T102X__NUM_CPUS, smp_cpus);
        return;
    }

    for (int i = 0; i < smp_cpus; ++i) {
        PowerPCCPU *cpu = &s->cpus[i];
        CPUState *cs = CPU(cpu);
        CPUPPCState *env = &cpu->env;

        object_property_set_bool(OBJECT(cpu), true, "realized", &error_abort);

        if (env->mmu_model != POWERPC_MMU_BOOKE206) {
            error_setg(errp, "MMU model %i not supported by this machine",
                       env->mmu_model);
            return;
        }

        qemu_irq *inputs = (qemu_irq *)env->irq_inputs;
        s->irqs[i].irq[OPENPIC_OUTPUT_INT] = inputs[PPCE500_INPUT_INT];
        s->irqs[i].irq[OPENPIC_OUTPUT_CINT] = inputs[PPCE500_INPUT_CINT];
        env->spr_cb[SPR_BOOKE_PIR].default_value = cs->cpu_index = i;
        env->spr_cb[SPR_E500_SVR].default_value = s->svr;
        env->mpic_iack = tc->ccsrbar_base + T102X__MPIC_OFFSET + 0xA0;

        ppc_booke_timers_init(cpu, T102X__SYSCLK / T102X__TBCLK_DIV,
                              PPC_TIMER_E500);

        /* Register reset handler */
        if (!i) {
            /* Primary CPU */
            qemu_register_reset(t102x_cpu_reset, cpu);
            env->load_info = bi;
        } else {
            /* Secondary CPUs */
            qemu_register_reset(t102x_cpu_reset_2nd, cpu);
        }
    }

    peridev = qdev_create(NULL, "e500-ccsr");
    object_property_add_child(OBJECT(dev), "e500-ccsr",
                              OBJECT(peridev), NULL);
    qdev_init_nofail(peridev);
    ccsr = CCSR(peridev);
    ccsr_addr_space = &ccsr->ccsr_space;
    memory_region_add_subregion(address_space_mem, tc->ccsrbar_base,
                                ccsr_addr_space);

    /* aliased 32bit memory region */
    memory_region_init_alias(&ccsr->ccsr_space_alias, NULL, "t102x-ccsr.alias",
                             ccsr_addr_space, 0, T102X__CCSRBAR_SIZE);
    memory_region_add_subregion(address_space_mem,
                                (uint32_t)tc->ccsrbar_base,
                                &ccsr->ccsr_space_alias);

    /* Local Configuration Control */
    peridev = qdev_create(NULL, "t102x-lcc");
    sbd = SYS_BUS_DEVICE(peridev);
    qdev_init_nofail(peridev);
    memory_region_add_subregion(ccsr_addr_space, T102X__LCC_OFFSET,
                                sysbus_mmio_get_region(sbd, 0));

    /* Local Access Window */
    peridev = qdev_create(NULL, "t102x-law");
    sbd = SYS_BUS_DEVICE(peridev);
    qdev_init_nofail(peridev);
    memory_region_add_subregion(ccsr_addr_space, T102X__LAW_OFFSET,
                                sysbus_mmio_get_region(sbd, 0));

    /* DDR */
    peridev = qdev_create(NULL, "t102x-ddr");
    sbd = SYS_BUS_DEVICE(peridev);
    qdev_init_nofail(peridev);
    memory_region_add_subregion(ccsr_addr_space, T102X__DDR_OFFSET,
                                sysbus_mmio_get_region(sbd, 0));

    /* CoreNet Platform Cache */
    peridev = qdev_create(NULL, "t102x-cpc");
    sbd = SYS_BUS_DEVICE(peridev);
    qdev_init_nofail(peridev);
    memory_region_add_subregion(ccsr_addr_space, T102X__CPC_OFFSET,
                                sysbus_mmio_get_region(sbd, 0));

    /* Work-Area for TLB initialization */
    MemoryRegion *wa = g_new(MemoryRegion, 1);
    memory_region_allocate_system_memory(wa, NULL, "t102x-ccsr.wa", 0x4000);
    memory_region_add_subregion(ccsr_addr_space, 0x03C000, wa);

    /* MPIC-Global */
    mpicdev = t102x_mpic_init(s, ccsr_addr_space, T102X__MPIC_OFFSET);

    /* Device Configuration/Pin Control */
    peridev = qdev_create(NULL, "t102x-dcfg");
    sbd = SYS_BUS_DEVICE(peridev);
    qdev_prop_set_ptr(peridev, "rcw", s->rcw);
    qdev_init_nofail(peridev);
    memory_region_add_subregion(ccsr_addr_space, T102X__DCFG_OFFSET,
                                sysbus_mmio_get_region(sbd, 0));

    /* Clocking */
    peridev = qdev_create(NULL, "t102x-clking");
    sbd = SYS_BUS_DEVICE(peridev);
    qdev_init_nofail(peridev);
    memory_region_add_subregion(ccsr_addr_space, T102X__CLKING_OFFSET,
                                sysbus_mmio_get_region(sbd, 0));

    /* Run Control/Power Management */
    peridev = qdev_create(NULL, "t102x-rcpm");
    sbd = SYS_BUS_DEVICE(peridev);
    qdev_init_nofail(peridev);
    memory_region_add_subregion(ccsr_addr_space, T102X__RCPM_OFFSET,
                                sysbus_mmio_get_region(sbd, 0));

    /* Enhanced serial peripheral interface */
    peridev = qdev_create(NULL, "fsl-espi");
    sbd = SYS_BUS_DEVICE(peridev);
    object_property_add_child(OBJECT(dev), "espi",
                              OBJECT(peridev), NULL);
    qdev_init_nofail(peridev);
    memory_region_add_subregion(ccsr_addr_space, T102X__ESPI_OFFSET,
                                sysbus_mmio_get_region(sbd, 0));

    /* I2C */
    peridev = qdev_create(NULL, "mpc-i2c");
    sbd = SYS_BUS_DEVICE(peridev);
    object_property_add_child(OBJECT(dev), "i2c",
                              OBJECT(peridev), NULL);
    qdev_init_nofail(peridev);
    sysbus_connect_irq(sbd, 0, qdev_get_gpio_in(mpicdev, 22));
    memory_region_add_subregion(ccsr_addr_space, T102X__I2C_OFFSET,
                                sysbus_mmio_get_region(sbd, 0));

    /* Integrated Flash Controller */
    peridev = qdev_create(NULL, "fsl-ifc");
    sbd = SYS_BUS_DEVICE(peridev);
    qdev_init_nofail(peridev);
    memory_region_add_subregion(ccsr_addr_space, T102X__IFC_OFFSET,
                                sysbus_mmio_get_region(sbd, 0));

    /* QUICC Engine */
    peridev = qdev_create(NULL, "fsl-quicc");
    sbd = SYS_BUS_DEVICE(peridev);
    qdev_init_nofail(peridev);
    memory_region_add_subregion(ccsr_addr_space, T102X__QUICC_OFFSET,
                                sysbus_mmio_get_region(sbd, 0));

    /* Dual USB PHY */
    peridev = qdev_create(NULL, "t102x-usb-phy");
    sbd = SYS_BUS_DEVICE(peridev);
    qdev_init_nofail(peridev);
    memory_region_add_subregion(ccsr_addr_space, T102X__USBPHY_OFFSET,
                                sysbus_mmio_get_region(sbd, 0));

    /* PCI Express 1 */
    peridev = qdev_create(NULL, "t102x-pex");
    sbd = SYS_BUS_DEVICE(peridev);
    qdev_init_nofail(peridev);
    memory_region_add_subregion(ccsr_addr_space, T102X__PEX1_OFFSET,
                                sysbus_mmio_get_region(sbd, 0));

    /* PCI Express 2 */
    peridev = qdev_create(NULL, "t102x-pex");
    sbd = SYS_BUS_DEVICE(peridev);
    qdev_init_nofail(peridev);
    memory_region_add_subregion(ccsr_addr_space, T102X__PEX2_OFFSET,
                                sysbus_mmio_get_region(sbd, 0));

    /* PCI Express 3 */
    peridev = qdev_create(NULL, "t102x-pex");
    sbd = SYS_BUS_DEVICE(peridev);
    qdev_init_nofail(peridev);
    memory_region_add_subregion(ccsr_addr_space, T102X__PEX3_OFFSET,
                                sysbus_mmio_get_region(sbd, 0));

    /* Security */
    peridev = qdev_create(NULL, "t102x-sec");
    sbd = SYS_BUS_DEVICE(peridev);
    qdev_init_nofail(peridev);
    memory_region_add_subregion(ccsr_addr_space, T102X__SEC_OFFSET,
                                sysbus_mmio_get_region(sbd, 0));

    /* Queue Manager */
    peridev = qdev_create(NULL, "t102x-qman");
    sbd = SYS_BUS_DEVICE(peridev);
    qdev_init_nofail(peridev);
    memory_region_add_subregion(ccsr_addr_space, T102X__QMAN_OFFSET,
                                sysbus_mmio_get_region(sbd, 0));

    /* Buffer Manager */
    peridev = qdev_create(NULL, "t102x-bman");
    sbd = SYS_BUS_DEVICE(peridev);
    qdev_init_nofail(peridev);
    memory_region_add_subregion(ccsr_addr_space, T102X__BMAN_OFFSET,
                                sysbus_mmio_get_region(sbd, 0));

    /* Frame Manager */
    peridev = qdev_create(NULL, "t102x-fman");
    sbd = SYS_BUS_DEVICE(peridev);
    qdev_init_nofail(peridev);
    memory_region_add_subregion(ccsr_addr_space, T102X__FMAN_OFFSET,
                                sysbus_mmio_get_region(sbd, 0));

    /* Serial */
    static const struct {
        hwaddr addr;
        int irq;
    } uarts[] = {
        {T102X__UART1_OFFSET, 20},
        {T102X__UART2_OFFSET, 20},
        {T102X__UART3_OFFSET, 21},
        {T102X__UART3_OFFSET, 21},
    };
    for (int i = 0; i < ARRAY_SIZE(uarts); ++i) {
        if (serial_hd(i)) {
            serial_mm_init(ccsr_addr_space, uarts[i].addr, 0,
                           qdev_get_gpio_in(mpicdev, uarts[i].irq), 115200,
                           serial_hd(i), DEVICE_BIG_ENDIAN);
        }
    }
}

static void t102x_init(Object *obj)
{
    MachineState *machine = MACHINE(qdev_get_machine());
    FslT102xState *s = FSL_T102X(obj);
    char name[32];

    for (int i = 0; i < machine->smp.cpus; ++i) {
        snprintf(name, sizeof(name), "cpu%d", i);
        object_initialize_child(obj, name, &s->cpus[i], sizeof(s->cpus[i]),
                                machine->cpu_type, &error_abort, NULL);
    }
}

static void t102x_class_init(ObjectClass *oc, void *data)
{
    static Property props[] = {
        DEFINE_PROP_UINT32("svr", FslT102xState, svr, SVR_T1024),
        DEFINE_PROP_PTR("rcw", FslT102xState, rcw),
        DEFINE_PROP_END_OF_LIST()
    };

    DeviceClass *dc = DEVICE_CLASS(oc);
    FslT102xClass *tc = FSL_T102X_CLASS(oc);

    tc->mpic_version = OPENPIC_MODEL_FSL_MPIC_42;
    tc->ccsrbar_base = T102X__CCSRBAR_BASE;

    dc->realize = t102x_realize;
    dc->props = props;

    /* Reason: Uses serial_hds and nd_table in realize() directly */
    dc->user_creatable = false;
    dc->desc = "Freescale T102x SoC";
}

static void t1_register_types(void)
{
    static const TypeInfo types[] = {
        {
            .name = TYPE_FSL_T102X,
            .parent = TYPE_DEVICE,
            .instance_size = sizeof(FslT102xState),
            .instance_init = t102x_init,
            .class_size = sizeof(FslT102xClass),
            .class_init = t102x_class_init,
        },
    };

    for (int i = 0; i < ARRAY_SIZE(types); ++i) {
        type_register_static(&types[i]);
    }
}

type_init(t1_register_types)
