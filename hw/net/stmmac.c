/*
 *  STMicro STMMAC Ethernet
 *
 *  Copyright (C) 2018 t-kenji <protect.2501@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 *  for more details.
 */

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qemu/log.h"

#define TYPE_STMMAC "stmmaceth"
#define STMMAC(obj) OBJECT_CHECK(STMMACState, (obj), TYPE_STMMAC)

enum STMMACMemoryMap {
    STMMAC_MMIO_SIZE = 0x9000,
};

enum STMMACMIIValues {
    MII_BUSY = 0x0001,
};

typedef struct {
    const char *name;
    hwaddr offset;
    uint32_t reset_value;
} STMMACReg;

#define REG_ITEM(index, offset, reset_value) \
    [index] = {#index, (offset), (reset_value)}

enum STMMACRegs {
    GMAC_CTRL,
    GMAC_MII_ADDR,
    GMAC_MII_DATA,
    GMAC_VER,
    DMA_HW_FEAT,
    STMMAC_NUM_REGS
};

static const STMMACReg mac_regs[] = {
    REG_ITEM(GMAC_CTRL,     0x0000, 0x00000000),
    REG_ITEM(GMAC_MII_ADDR, 0x0010, 0x00000000),
    REG_ITEM(GMAC_MII_DATA, 0x0014, 0x00000000),
    REG_ITEM(GMAC_VER,      0x0020, 0x00001037),
    REG_ITEM(DMA_HW_FEAT,   0x1058, 0x01050203),
};

enum MIIRegs {
    MII_BMCR,
    MII_BMSR,
    MII_PHYSID1,
    MII_PHYSID2,
    MII_NUM_REGS
};

static const STMMACReg mii_regs[] = {
    REG_ITEM(MII_BMCR,    0x00, 0x0000),
    REG_ITEM(MII_BMSR,    0x01, 0x0000),
    REG_ITEM(MII_PHYSID1, 0x02, 0x1234),
    REG_ITEM(MII_PHYSID2, 0x03, 0x5678),
};

#undef REG_ITEM

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t mac_regs[STMMAC_NUM_REGS];
    uint32_t mii_regs[MII_NUM_REGS];
} STMMACState;

static const VMStateDescription stmmac_vmstate = {
    .name = TYPE_STMMAC,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_UINT32_ARRAY(mac_regs, STMMACState, STMMAC_NUM_REGS),
        VMSTATE_UINT32_ARRAY(mii_regs, STMMACState, MII_NUM_REGS),
        VMSTATE_END_OF_LIST()
    },
};

static int stmmac_offset_to_index(const STMMACReg *regs,
                                  int length,
                                  hwaddr offset)
{
    for (int i = 0; i < length; ++i) {
        if (regs[i].offset == offset) {
            return i;
        }
    }

    return -1;
}

static uint16_t mii_read(STMMACState *s, uint8_t addr, uint8_t reg)
{
    int index = stmmac_offset_to_index(mii_regs, MII_NUM_REGS, reg);

    if (index < 0) {
        qemu_log("%s(mii): Bad read addr 0x%x reg 0x%x\n", TYPE_STMMAC, addr, reg);
        return 0;
    }

    uint16_t value = 0xFFFF;
    if (addr == 0) {
        switch (index) {
        case MII_PHYSID1:
            value = s->mii_regs[MII_PHYSID1];
            break;
        case MII_PHYSID2:
            value = s->mii_regs[MII_PHYSID2];
            break;
        default:
            break;
        }
    }
    qemu_log("%s(mii): read 0x%x from addr 0x%x reg 0x%x\n",
             TYPE_STMMAC, value, addr, reg);
    return value;
}

static void mii_write(STMMACState *s, uint8_t addr, uint8_t reg, uint16_t value)
{
    int index = stmmac_offset_to_index(mii_regs, MII_NUM_REGS, reg);

    switch (index) {
    default:
        qemu_log("%s(mii): Bad write addr 0x%x reg 0x%x\n",
                 TYPE_STMMAC, addr, reg);
        return;
    }
}

static uint64_t stmmac_read(void *opaque, hwaddr offset, unsigned size)
{
    STMMACState *s = STMMAC(opaque);
    int index = stmmac_offset_to_index(mac_regs, STMMAC_NUM_REGS, offset);

    if (index < 0) {
        qemu_log("%s: Bad read offset 0x%" HWADDR_PRIx "\n",
                 TYPE_STMMAC, offset);
        return 0;
    }

    uint64_t value = s->mac_regs[index];
    switch (index) {
    case GMAC_MII_ADDR:
        s->mac_regs[GMAC_MII_ADDR] &= ~MII_BUSY;
        break;
    default:
        break;
    }
    qemu_log("%s: read 0x%" PRIx64 " from offset 0x%" HWADDR_PRIx "\n",
             TYPE_STMMAC, value, offset);
    return value;
}

static void stmmac_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    STMMACState *s = STMMAC(opaque);
    int index = stmmac_offset_to_index(mac_regs, STMMAC_NUM_REGS, offset);

    switch (index) {
    case GMAC_CTRL:
        s->mac_regs[GMAC_CTRL] = (uint32_t)value;
        break;
    case GMAC_MII_ADDR: {
            s->mac_regs[GMAC_MII_ADDR] = (uint32_t)value;

            uint8_t addr = (s->mac_regs[GMAC_MII_ADDR] >> 11) & 0x1F;
            uint8_t reg = (s->mac_regs[GMAC_MII_ADDR] >> 6) & 0x1F;
            bool is_write = (s->mac_regs[GMAC_MII_ADDR] >> 1) & 0x01;
            if (is_write) {
                mii_write(s, addr, reg, s->mac_regs[GMAC_MII_DATA]);
            } else {
                s->mac_regs[GMAC_MII_DATA] = mii_read(s, addr, reg);
            }
            qemu_log("%s(mii): %s addr 0x%x reg 0x%x\n",
                     TYPE_STMMAC, (is_write ? "write" : "read"), addr, reg);
        }
        break;
    case GMAC_MII_DATA:
        s->mac_regs[GMAC_MII_DATA] = (uint32_t)value;
        break;
    case GMAC_VER:
        s->mac_regs[GMAC_VER] = (uint32_t)value;
        break;
    default:
        qemu_log("%s: Bad write offset 0x%" HWADDR_PRIx "\n",
                 TYPE_STMMAC, offset);
        return;
    }
    qemu_log("%s: write 0x%" PRIx64 " to offset 0x%" HWADDR_PRIx "\n",
             TYPE_STMMAC, value, offset);
}

static void stmmac_reset(DeviceState *dev)
{
    STMMACState *s = STMMAC(dev);

    for (int i = 0; i < STMMAC_NUM_REGS; ++i) {
        s->mac_regs[i] = mac_regs[i].reset_value;
    }
    for (int i = 0; i < MII_NUM_REGS; ++i) {
        s->mii_regs[i] = mii_regs[i].reset_value;
    }
}

static void stmmac_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = stmmac_read,
        .write      = stmmac_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
    };

    STMMACState *s = STMMAC(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_STMMAC, STMMAC_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void stmmac_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = stmmac_realize;
    dc->reset   = stmmac_reset;
    dc->vmsd    = &stmmac_vmstate;
}

static void stmmac_register_types(void)
{
    static const TypeInfo tinfo = {
        .name          = TYPE_STMMAC,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(STMMACState),
        .class_init    = stmmac_class_init,
    };

    type_register_static(&tinfo);
}

type_init(stmmac_register_types)
