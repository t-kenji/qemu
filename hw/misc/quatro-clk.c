/*
 * CSR Quatro 5500 Clock Controller emulation
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

#define TYPE_QUATRO_CLK "quatro5500.clk"
#define QUATRO_CLK(obj) OBJECT_CHECK(QuatroClkState, (obj), TYPE_QUATRO_CLK)

enum QuatroClkMemoryMap {
    QUATRO_CLK_SYSPLL_ADDR = 0x0000,
    QUATRO_CLK_SYSCG_ADDR = 0x0400,

    QUATRO_CLK_MMIO_SIZE = 0x10000,
};

enum QuatroClkSyscgValues {
    CLKSTATSW1_SYS_IS_MUX_CLK  = 0x00040000,
    CLKSTATSW1_SYS_IS_LP_CLK   = 0x00020000,
    CLKSTATSW1_SYS_IS_XIN0_CLK = 0x00010000,
};

typedef struct {
    const char *name;
    hwaddr offset;
    uint32_t reset_value;
} QuatroClkReg;

static const QuatroClkReg quatro_clk_regs[] = {
    {"SYSPLL_DIV12+0",      0x0018, 0x00000000},
    {"SYSPLL_DIV12+1",      0x001C, 0x00000000},
    {"SYSPLL_DIV12+2",      0x0020, 0x00000000},
    {"SYSPLL_DIV12+3",      0x0024, 0x00000007},
    {"SYSCG_CLKSTATSW1",    0x0414, CLKSTATSW1_SYS_IS_MUX_CLK},
    {"SYSCG_CLKMUXCTRL1",   0x0430, 0x00000003},
};

#define QUATRO_CLK_NUM_REGS ARRAY_SIZE(quatro_clk_regs)

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[QUATRO_CLK_NUM_REGS];
} QuatroClkState;

static const VMStateDescription quatro_clk_vmstate = {
    .name = TYPE_QUATRO_CLK,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_UINT32_ARRAY(regs, QuatroClkState, QUATRO_CLK_NUM_REGS),
        VMSTATE_END_OF_LIST()
    },
};

static uint64_t quatro_clk_read(void *opaque, hwaddr offset, unsigned size)
{
    const QuatroClkState *s = QUATRO_CLK(opaque);

    for (int i = 0; i < QUATRO_CLK_NUM_REGS; ++i ) {
        const QuatroClkReg *reg = &quatro_clk_regs[i];
        if (reg->offset == offset) {
            qemu_log("%s: read offset 0x%" HWADDR_PRIx ", value: 0x%" PRIx32 "\n", TYPE_QUATRO_CLK, offset, s->regs[i]);
            return s->regs[i];
        }
    }

    qemu_log("%s: Bad read offset 0x%" HWADDR_PRIx "\n",
             TYPE_QUATRO_CLK, offset);

    return 0;
}

static void quatro_clk_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    qemu_log("%s: Bad write offset 0x%" HWADDR_PRIx "\n", TYPE_QUATRO_CLK, offset);
}

static void quatro_clk_reset(DeviceState *dev)
{
    QuatroClkState *s = QUATRO_CLK(dev);

    for (int i = 0; i < QUATRO_CLK_NUM_REGS; ++i) {
        s->regs[i] = quatro_clk_regs[i].reset_value;
    }
}

static void quatro_clk_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = quatro_clk_read,
        .write      = quatro_clk_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
    };

    QuatroClkState *s = QUATRO_CLK(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s, TYPE_QUATRO_CLK,
                          QUATRO_CLK_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void quatro_clk_class_init(ObjectClass *oc, void *data)
{
    static Property props[] = {
        DEFINE_PROP_END_OF_LIST()
    };

    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = quatro_clk_realize;
    dc->reset   = quatro_clk_reset;
    dc->vmsd    = &quatro_clk_vmstate;
    dc->props   = props;
}

static void quatro_clk_register_type(void)
{
    static const TypeInfo tinfo = {
        .name          = TYPE_QUATRO_CLK,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroClkState),
        .class_init    = quatro_clk_class_init,
    };

    type_register_static(&tinfo);
}

type_init(quatro_clk_register_type)
