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
    QUATRO_CLK_SYSCG_ADDR  = 0x0400,

    QUATRO_CLK_MMIO_SIZE   = 0x10000,
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

enum QuatroClkRegs {
    SYSPLL_DIV12_0,
    SYSPLL_DIV12_1,
    SYSPLL_DIV12_2,
    SYSPLL_DIV12_3,
    SYSCG_CLKSTATSW1,
    SYSCG_CLKMUXCTRL1,
    SYSCG_CLKDIVCTRL0,
    SYSCG_CLKDIVCTRL1,

    QUATRO_CLK_NUM_REGS
};

#define REG_ITEM(index, offset, reset_value) \
    [index] = {#index, offset, reset_value}

static const QuatroClkReg quatro_clk_regs[] = {
    REG_ITEM(SYSPLL_DIV12_0,    0x0018, 0x00000000),
    REG_ITEM(SYSPLL_DIV12_1,    0x001C, 0x00000000),
    REG_ITEM(SYSPLL_DIV12_2,    0x0020, 0x00000000),
    REG_ITEM(SYSPLL_DIV12_3,    0x0024, 0x00000007),
    REG_ITEM(SYSCG_CLKSTATSW1,  0x0414, CLKSTATSW1_SYS_IS_MUX_CLK),
    REG_ITEM(SYSCG_CLKMUXCTRL1, 0x0430, 0x00000003),
    REG_ITEM(SYSCG_CLKDIVCTRL0, 0x0458, 0x00000001),
    REG_ITEM(SYSCG_CLKDIVCTRL1, 0x045C, 0x00000001),
};

#undef REG_ITEM

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

static int quatro_clk_offset_to_index(hwaddr offset)
{
    for (int i = 0; i < QUATRO_CLK_NUM_REGS; ++i) {
        if (quatro_clk_regs[i].offset == offset) {
            return i;
        }
    }

    return -1;
}

static uint64_t quatro_clk_read(void *opaque, hwaddr offset, unsigned size)
{
    const QuatroClkState *s = QUATRO_CLK(opaque);
    int index = quatro_clk_offset_to_index(offset);

    if (index < 0) {
        qemu_log("%s: Bad read offset 0x%" HWADDR_PRIx "\n",
                 TYPE_QUATRO_CLK, offset);
        return 0;
    }

    uint64_t value = s->regs[index];
    qemu_log("%s: read 0x%" PRIx64 " from offset 0x%" HWADDR_PRIx "\n",
             TYPE_QUATRO_CLK, offset, value);
    return value;
}

static void quatro_clk_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    QuatroClkState *s = QUATRO_CLK(opaque);
    int index = quatro_clk_offset_to_index(offset);

    switch (index) {
    case SYSPLL_DIV12_0:
        s->regs[SYSPLL_DIV12_0] = (uint32_t)value;
        break;
    case SYSPLL_DIV12_1:
        s->regs[SYSPLL_DIV12_1] = (uint32_t)value;
        break;
    case SYSPLL_DIV12_2:
        s->regs[SYSPLL_DIV12_2] = (uint32_t)value;
        break;
    case SYSPLL_DIV12_3:
        s->regs[SYSPLL_DIV12_3] = (uint32_t)value;
        break;
    case SYSCG_CLKSTATSW1:
        s->regs[SYSCG_CLKSTATSW1] = (uint32_t)value;
        break;
    case SYSCG_CLKMUXCTRL1:
        s->regs[SYSCG_CLKMUXCTRL1] = (uint32_t)value;
        break;
    case SYSCG_CLKDIVCTRL0:
        s->regs[SYSCG_CLKDIVCTRL0] = (uint32_t)value;
        break;
    case SYSCG_CLKDIVCTRL1:
        s->regs[SYSCG_CLKDIVCTRL1] = (uint32_t)value;
        break;
    default:
        qemu_log("%s: Bad write offset 0x%" HWADDR_PRIx "\n",
                 TYPE_QUATRO_CLK, offset);
        return;
    }
    qemu_log("%s: write 0x%" PRIx64 " to offset 0x%" HWADDR_PRIx "\n",
             TYPE_QUATRO_CLK, value, offset);
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
        .read = quatro_clk_read,
        .write = quatro_clk_write,
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
    dc->reset = quatro_clk_reset;
    dc->vmsd = &quatro_clk_vmstate;
    dc->props = props;
}

static void quatro_clk_register_type(void)
{
    static const TypeInfo tinfo = {
        .name = TYPE_QUATRO_CLK,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroClkState),
        .class_init = quatro_clk_class_init,
    };

    type_register_static(&tinfo);
}

type_init(quatro_clk_register_type)
