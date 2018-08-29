/*
 * CSR Quatro 5500 peripherals emulation
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
#include "hw/sysbus.h"
#include "qemu/log.h"

#define TYPE_QUATRO_A15GPF "quatro5500.a15gpf"
#define QUATRO_A15GPF(obj) OBJECT_CHECK(QuatroA15gpfState, (obj), TYPE_QUATRO_A15GPF)

#define TYPE_QUATRO_RSTGEN "quatro5500.rstgen"
#define QUATRO_RSTGEN(obj) OBJECT_CHECK(QuatroRstGenState, (obj), TYPE_QUATRO_RSTGEN)

#define TYPE_QUATRO_DDRMC "quatro5500.ddrmc"
#define QUATRO_DDRMC(obj) OBJECT_CHECK(QuatroDDRMCState, (obj), TYPE_QUATRO_DDRMC)

enum QuatroPeripheralMemoryMap {
    QUATRO_PERI_A15GPF_MMIO_SIZE = 0x10000,
    QUATRO_PERI_RSTGEN_MMIO_SIZE = 0x10000,
    QUATRO_PERI_DDRMC_MMIO_SIZE  = 0x10000,
};

typedef struct {
    const char *name;
    hwaddr offset;
    uint32_t reset_value;
} QuatroPeriReg;

static const QuatroPeriReg quatro_a15gpf_regs[] = {
    {"A15EVA0",             0x0040, 0x00000000},
    {"A15EVA1",             0x0044, 0x00000000},
};

#define QUATRO_A15GPF_NUM_REGS ARRAY_SIZE(quatro_a15gpf_regs)

static const QuatroPeriReg quatro_rstgen_regs[] = {
    {"PAD_INTERNAL",        0x0024, 0x00000040},
};

#define QUATRO_RSTGEN_NUM_REGS ARRAY_SIZE(quatro_rstgen_regs)

static const QuatroPeriReg quatro_ddrmc_regs[] = {
    {"EXT_ADDR_MODE",       0x4880, 0x00000000},
};

#define QUATRO_DDRMC_NUM_REGS ARRAY_SIZE(quatro_ddrmc_regs)

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[QUATRO_A15GPF_NUM_REGS];
} QuatroA15gpfState;

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[QUATRO_RSTGEN_NUM_REGS];
} QuatroRstGenState;

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[QUATRO_DDRMC_NUM_REGS];
} QuatroDDRMCState;

static const VMStateDescription quatro_a15gpf_vmstate = {
    .name               = TYPE_QUATRO_A15GPF,
    .version_id         = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_UINT32_ARRAY(regs, QuatroA15gpfState, QUATRO_A15GPF_NUM_REGS),
        VMSTATE_END_OF_LIST()
    },
};

static const VMStateDescription quatro_rstgen_vmstate = {
    .name               = TYPE_QUATRO_RSTGEN,
    .version_id         = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_UINT32_ARRAY(regs, QuatroRstGenState, QUATRO_RSTGEN_NUM_REGS),
        VMSTATE_END_OF_LIST()
    },
};

static uint64_t quatro_a15gpf_read(void *opaque, hwaddr offset, unsigned size)
{
    const QuatroA15gpfState *s = QUATRO_A15GPF(opaque);

    for (int i = 0; i < QUATRO_A15GPF_NUM_REGS; ++i) {
        const QuatroPeriReg *reg = &quatro_a15gpf_regs[i];
        if (reg->offset == offset) {
            qemu_log("%s: read offset 0x%" HWADDR_PRIx ", value: 0x%" PRIx32 "\n", TYPE_QUATRO_A15GPF, offset, s->regs[i]);
            return s->regs[i];
        }
    }

    qemu_log("%s: Bad read offset 0x%" HWADDR_PRIx "\n",
             TYPE_QUATRO_A15GPF, offset);

    return 0;
}

static void quatro_a15gpf_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    qemu_log("%s: Bad write offset 0x%" HWADDR_PRIx "\n",
             TYPE_QUATRO_A15GPF, offset);
}

static void quatro_a15gpf_reset(DeviceState *dev)
{
    QuatroA15gpfState *s = QUATRO_A15GPF(dev);

    for (int i = 0; i < QUATRO_A15GPF_NUM_REGS; ++i) {
        s->regs[i] = quatro_a15gpf_regs[i].reset_value;
    }
}

static void quatro_a15gpf_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = quatro_a15gpf_read,
        .write      = quatro_a15gpf_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
    };

    QuatroA15gpfState *s = QUATRO_A15GPF(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_QUATRO_A15GPF, QUATRO_PERI_A15GPF_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void quatro_a15gpf_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = quatro_a15gpf_realize;
    dc->reset   = quatro_a15gpf_reset;
    dc->vmsd    = &quatro_a15gpf_vmstate;
}

static uint64_t quatro_rstgen_read(void *opaque, hwaddr offset, unsigned size)
{
    const QuatroRstGenState *s = QUATRO_RSTGEN(opaque);

    for (int i = 0; i < QUATRO_RSTGEN_NUM_REGS; ++i) {
        const QuatroPeriReg *reg = &quatro_rstgen_regs[i];
        if (reg->offset == offset) {
            qemu_log("%s: read offset 0x%" HWADDR_PRIx ", value: 0x%" PRIx32 "\n", TYPE_QUATRO_RSTGEN, offset, s->regs[i]);
            return s->regs[i];
        }
    }

    qemu_log("%s: Bad read offset 0x%" HWADDR_PRIx "\n",
             TYPE_QUATRO_RSTGEN, offset);

    return 0;
}

static void quatro_rstgen_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    qemu_log("%s: Bad write offset 0x%" HWADDR_PRIx "\n",
             TYPE_QUATRO_RSTGEN, offset);
}

static void quatro_rstgen_reset(DeviceState *dev)
{
    QuatroRstGenState *s = QUATRO_RSTGEN(dev);

    for (int i = 0; i < QUATRO_RSTGEN_NUM_REGS; ++i) {
        s->regs[i] = quatro_rstgen_regs[i].reset_value;
    }
}

static void quatro_rstgen_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = quatro_rstgen_read,
        .write      = quatro_rstgen_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
    };

    QuatroRstGenState *s = QUATRO_RSTGEN(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_QUATRO_RSTGEN, QUATRO_PERI_RSTGEN_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void quatro_rstgen_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = quatro_rstgen_realize;
    dc->reset   = quatro_rstgen_reset;
    dc->vmsd    = &quatro_rstgen_vmstate;
}

static void quatro_peripherals_register_types(void)
{
    static const TypeInfo a15gpf_tinfo = {
        .name          = TYPE_QUATRO_A15GPF,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroA15gpfState),
        .class_init    = quatro_a15gpf_class_init,
    };
    static const TypeInfo rstgen_tinfo = {
        .name          = TYPE_QUATRO_RSTGEN,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroRstGenState),
        .class_init    = quatro_rstgen_class_init,
    };

    type_register_static(&a15gpf_tinfo);
    type_register_static(&rstgen_tinfo);
}

type_init(quatro_peripherals_register_types)
