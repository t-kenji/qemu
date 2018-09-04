/*
 * CSR Quatro 5500 SD Host Controller emulation
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

#define TYPE_QUATRO_SDIOCORE "quatro5500.sdiocore"
#define QUATRO_SDIOCORE(obj) OBJECT_CHECK(QuatroSDIOCoreState, (obj), TYPE_QUATRO_SDIOCORE)

#define TYPE_QUATRO_SDMCLK "quatro5500.sdmclk"
#define QUATRO_SDMCLK(obj) OBJECT_CHECK(QuatroSDMClkState, (obj), TYPE_QUATRO_SDMCLK)

typedef struct {
    const char *name;
    hwaddr offset;
    uint32_t reset_value;
} QuatroSDHCIReg;

enum QuatroSDHCIMemoryMap {
    QUATRO_SDIOCORE_MMIO_SIZE = 0x100,
    QUATRO_SDMCLK_MMIO_SIZE = 0x10000,
};

#define REG_ITEM(index, offset, reset_value) \
    [index] = {#index, offset, reset_value}

enum QuatroSDIOCoreRegs {
    SDIO0_HRS0,
    SDIO0_HRS1,
    SDIO0_HRS2,
    SDIO0_HRS44_0,
    SDIO0_HRS44_1,
    QUATRO_SDIOCORE_NUM_REGS
};

static const QuatroSDHCIReg quatro_sdiocore_regs[] = {
    REG_ITEM(SDIO0_HRS0,    0x0000, 0x00000000),
    REG_ITEM(SDIO0_HRS1,    0x0004, 0x00000000),
    REG_ITEM(SDIO0_HRS2,    0x0008, 0x00000000),
    REG_ITEM(SDIO0_HRS44_0, 0x00B0, 0x00000000),
    REG_ITEM(SDIO0_HRS44_1, 0x00B4, 0x00000000),
};

enum QuatroSDMClkRegs {
    CLKDISCTRL,
    CLKDISSTAT,
    SDIO0_EXTCTL,
    SDIO1_EXTCTL,
    QUATRO_SDMCLK_NUM_REGS
};

static const QuatroSDHCIReg quatro_sdmclk_regs[] = {
    REG_ITEM(CLKDISCTRL,   0x01D8, 0x00000000),
    REG_ITEM(CLKDISSTAT,   0x01DC, 0x00000003),
    REG_ITEM(SDIO0_EXTCTL, 0x0280, 0x00002000),
    REG_ITEM(SDIO1_EXTCTL, 0x0284, 0x00002000),
};

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[QUATRO_SDIOCORE_NUM_REGS];
} QuatroSDIOCoreState;

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[QUATRO_SDMCLK_NUM_REGS];
} QuatroSDMClkState;

static const VMStateDescription quatro_sdiocore_vmstate = {
    .name = TYPE_QUATRO_SDIOCORE,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_UINT32_ARRAY(regs, QuatroSDIOCoreState, QUATRO_SDIOCORE_NUM_REGS),
        VMSTATE_END_OF_LIST()
    },
};

static const VMStateDescription quatro_sdmclk_vmstate = {
    .name = TYPE_QUATRO_SDMCLK,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_UINT32_ARRAY(regs, QuatroSDMClkState, QUATRO_SDMCLK_NUM_REGS),
        VMSTATE_END_OF_LIST()
    },
};

static int quatro_sdiocore_offset_to_index(hwaddr offset)
{
    for (int i = 0; i < QUATRO_SDIOCORE_NUM_REGS; ++i) {
        if (quatro_sdiocore_regs[i].offset == offset) {
            return i;
        }
    }

    return -1;
}

static int quatro_sdmclk_offset_to_index(hwaddr offset)
{
    for (int i = 0; i < QUATRO_SDMCLK_NUM_REGS; ++i) {
        if (quatro_sdmclk_regs[i].offset == offset) {
            return i;
        }
    }

    return -1;
}

static uint64_t quatro_sdiocore_read(void *opaque, hwaddr offset, unsigned size)
{
    QuatroSDIOCoreState *s = QUATRO_SDIOCORE(opaque);
    int index = quatro_sdiocore_offset_to_index(offset);

    if (index < 0) {
        qemu_log("%s: Bad read offset 0x%" HWADDR_PRIx "\n",
                 TYPE_QUATRO_SDIOCORE, offset);
        return 0;
    }

    uint64_t value = s->regs[index];
    switch (index) {
    case SDIO0_HRS0:
        s->regs[SDIO0_HRS0] &= 0xFFFFFFFE;
        break;
    }

    qemu_log("%s: read 0x%" PRIx64 " from offset 0x%" HWADDR_PRIx "\n",
             TYPE_QUATRO_SDIOCORE, value, offset);
    return value;
}

static void quatro_sdiocore_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    QuatroSDIOCoreState *s = QUATRO_SDIOCORE(opaque);
    int index = quatro_sdiocore_offset_to_index(offset);

    switch (index) {
    case SDIO0_HRS0:
        s->regs[SDIO0_HRS0] = (uint32_t)value;
        break;
    case SDIO0_HRS1:
        s->regs[SDIO0_HRS1] = (uint32_t)value;
        break;
    case SDIO0_HRS2:
        s->regs[SDIO0_HRS2] = (uint32_t)value;
        break;
    case SDIO0_HRS44_0:
        s->regs[SDIO0_HRS44_0] = (uint32_t)value | 0x04000000;
        if (s->regs[SDIO0_HRS44_0] & 0x01000000) {
            s->regs[SDIO0_HRS44_0] |= 0x04000000;
        } else {
            s->regs[SDIO0_HRS44_0] &= ~0x04000000;
        }
        break;
    case SDIO0_HRS44_1:
        s->regs[SDIO0_HRS44_1] = (uint32_t)value | 0x04000000;
        if (s->regs[SDIO0_HRS44_1] & 0x01000000) {
            s->regs[SDIO0_HRS44_1] |= 0x04000000;
        } else {
            s->regs[SDIO0_HRS44_1] &= ~0x04000000;
        }
        break;
    default:
        qemu_log("%s: Bad read offset 0x%" HWADDR_PRIx "\n",
                 TYPE_QUATRO_SDIOCORE, offset);
        return;
    }
    qemu_log("%s: write 0x%" PRIx64 " to offset 0x%" HWADDR_PRIx "\n",
             TYPE_QUATRO_SDIOCORE, value, offset);
}

static void quatro_sdiocore_reset(DeviceState *dev)
{
    QuatroSDIOCoreState *s = QUATRO_SDIOCORE(dev);

    for (int i = 0; i < QUATRO_SDIOCORE_NUM_REGS; ++i) {
        s->regs[i] = quatro_sdiocore_regs[i].reset_value;
    }
}

static void quatro_sdiocore_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read = quatro_sdiocore_read,
        .write = quatro_sdiocore_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
    };

    QuatroSDIOCoreState *s = QUATRO_SDIOCORE(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s, TYPE_QUATRO_SDIOCORE,
                          QUATRO_SDIOCORE_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void quatro_sdiocore_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = quatro_sdiocore_realize;
    dc->reset = quatro_sdiocore_reset;
    dc->vmsd = &quatro_sdiocore_vmstate;
}

static uint64_t quatro_sdmclk_read(void *opaque, hwaddr offset, unsigned size)
{
    QuatroSDMClkState *s = QUATRO_SDMCLK(opaque);
    int index = quatro_sdmclk_offset_to_index(offset);

    if (index < 0) {
        qemu_log("%s: Bad read offset 0x%" HWADDR_PRIx "\n",
                 TYPE_QUATRO_SDMCLK, offset);
        return 0;
    }

    uint64_t value = s->regs[index];
    qemu_log("%s: read 0x%" PRIx64 " from offset 0x%" HWADDR_PRIx "\n",
             TYPE_QUATRO_SDMCLK, value, offset);
    return value;
}

static void quatro_sdmclk_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    QuatroSDMClkState *s = QUATRO_SDMCLK(opaque);
    int index = quatro_sdmclk_offset_to_index(offset);

    switch (index) {
    case CLKDISCTRL:
        s->regs[CLKDISCTRL] = (uint32_t)value;
        s->regs[CLKDISSTAT] |= 0x00000003;
        s->regs[CLKDISSTAT] &= 0xFFFFFFFC | (0x00000003 & s->regs[CLKDISCTRL]);
        break;
    case SDIO0_EXTCTL:
        s->regs[SDIO0_EXTCTL] = (uint32_t)value;
        break;
    case SDIO1_EXTCTL:
        s->regs[SDIO1_EXTCTL] = (uint32_t)value;
        break;
    default:
        qemu_log("%s: Bad write offset 0x%" HWADDR_PRIx "\n",
                 TYPE_QUATRO_SDMCLK, offset);
        return;
    }
    qemu_log("%s: write 0x%" PRIx64 " to offset 0x%" HWADDR_PRIx "\n",
             TYPE_QUATRO_SDMCLK, value, offset);
}

static void quatro_sdmclk_reset(DeviceState *dev)
{
    QuatroSDMClkState *s = QUATRO_SDMCLK(dev);

    for (int i = 0; i < QUATRO_SDMCLK_NUM_REGS; ++i) {
        s->regs[i] = quatro_sdmclk_regs[i].reset_value;
    }
}

static void quatro_sdmclk_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read = quatro_sdmclk_read,
        .write = quatro_sdmclk_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
    };

    QuatroSDMClkState *s = QUATRO_SDMCLK(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s, TYPE_QUATRO_SDMCLK,
                          QUATRO_SDMCLK_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void quatro_sdmclk_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = quatro_sdmclk_realize;
    dc->reset = quatro_sdmclk_reset;
    dc->vmsd = &quatro_sdmclk_vmstate;
}

static void quatro_sdhci_register_types(void)
{
    static const TypeInfo sdiocore_tinfo = {
        .name = TYPE_QUATRO_SDIOCORE,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroSDIOCoreState),
        .class_init = quatro_sdiocore_class_init,
    };
    static const TypeInfo sdmclk_tinfo = {
        .name = TYPE_QUATRO_SDMCLK,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroSDMClkState),
        .class_init = quatro_sdmclk_class_init,
    };

    type_register_static(&sdiocore_tinfo);
    type_register_static(&sdmclk_tinfo);
}

type_init(quatro_sdhci_register_types)
