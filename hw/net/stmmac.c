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
#include "net/net.h"
#include "qemu/log.h"

#define TYPE_STMMAC "stmmaceth"
#define STMMAC(obj) OBJECT_CHECK(STMMACState, (obj), TYPE_STMMAC)

enum STMMACMemoryMap {
    STMMAC_MMIO_SIZE = 0x9000,
};

enum STMMACValues {
    DMA_BUS_MODE_SFT_RESET = 0x00000001,
    DMA_STATUS_TI          = 0x00000001,
    DMA_STATUS_RI          = 0x00000040,
    DMA_STATUS_NIS         = 0x00010000,
};

enum STMMACMIIValues {
    MII_BUSY          = 0x0001,
    BMCR_RESET        = 0x8000,
    BMSR_LSTATE       = 0x0004,
    BMSR_AUTONEG_CMPL = 0x0020,
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
    GMAC_FRAME_FILTER,
    GMAC_HASH_HI,
    GMAC_HASH_LO,
    GMAC_MII_ADDR,
    GMAC_MII_DATA,
    GMAC_VER,
    GMAC_INT_STATUS,
    GMAC_INT_MASK,
    GMAC_ADDR_HI,
    GMAC_ADDR_LO,
    MMC_CTRL,
    MMC_RX_INT_MASK,
    MMC_TX_INT_MASK,
    MMC_RX_IPC_INT_MASK,
    DMA_BUS_MODE,
    DMA_TX_POLL_DEMAND,
    DMA_RX_POLL_DEMAND,
    DMA_RX_BASE_ADDR,
    DMA_TX_BASE_ADDR,
    DMA_STATUS,
    DMA_CTRL,
    DMA_INT_ENA,
    DMA_RX_WATCHDOG,
    DMA_AXI_BUS_MODE,
    DMA_HW_FEAT,
    GMAC_RCPD,
    GMAC_TCPD,
    STMMAC_NUM_REGS
};

static const STMMACReg mac_regs[] = {
    REG_ITEM(GMAC_CTRL,           0x0000, 0x00000000),
    REG_ITEM(GMAC_FRAME_FILTER,   0x0004, 0x00000000),
    REG_ITEM(GMAC_HASH_HI,        0x0008, 0x00000000),
    REG_ITEM(GMAC_HASH_LO,        0x000C, 0x00000000),
    REG_ITEM(GMAC_MII_ADDR,       0x0010, 0x00000000),
    REG_ITEM(GMAC_MII_DATA,       0x0014, 0x00000000),
    REG_ITEM(GMAC_VER,            0x0020, 0x00001037),
    REG_ITEM(GMAC_INT_STATUS,     0x0038, 0x00000000),
    REG_ITEM(GMAC_INT_MASK,       0x003C, 0x00000000),
    REG_ITEM(GMAC_ADDR_HI,        0x0040, 0x0000FFFF),
    REG_ITEM(GMAC_ADDR_LO,        0x0044, 0xFFFFFFFF),
    REG_ITEM(MMC_CTRL,            0x0100, 0x00000000),
    REG_ITEM(MMC_RX_INT_MASK,     0x010C, 0x00000000),
    REG_ITEM(MMC_TX_INT_MASK,     0x0110, 0x00000000),
    REG_ITEM(MMC_RX_IPC_INT_MASK, 0x0200, 0x00000000),
    REG_ITEM(DMA_BUS_MODE,        0x1000, 0x00000000),
    REG_ITEM(DMA_TX_POLL_DEMAND,  0x1004, 0x00000000),
    REG_ITEM(DMA_RX_POLL_DEMAND,  0x1008, 0x00000000),
    REG_ITEM(DMA_RX_BASE_ADDR,    0x100C, 0x00000000),
    REG_ITEM(DMA_TX_BASE_ADDR,    0x1010, 0x00000000),
    REG_ITEM(DMA_STATUS,          0x1014, 0x00000000),
    REG_ITEM(DMA_CTRL,            0x1018, 0x00000000),
    REG_ITEM(DMA_INT_ENA,         0x101C, 0x00000000),
    REG_ITEM(DMA_RX_WATCHDOG,     0x1024, 0x00000000),
    REG_ITEM(DMA_AXI_BUS_MODE,    0x1028, 0x00000000),
    REG_ITEM(GMAC_RCPD,           0x8008, 0x00000000),
    REG_ITEM(GMAC_TCPD,           0x8010, 0x00000000),
    REG_ITEM(DMA_HW_FEAT,         0x1058, 0x01050A03),
};

enum MIIRegs {
    MII_BMCR,
    MII_BMSR,
    MII_PHYSID1,
    MII_PHYSID2,
    MII_ADVERTISE,
    MII_LPA,
    MII_CTRL1000,
    MII_STAT1000,
    MII_ESTATUS,
    MII_NUM_REGS
};

static const STMMACReg mii_regs[] = {
    REG_ITEM(MII_BMCR,    0x00, 0x0000),
    /*
     * Autoneg | 10-half | 10-full | 100-half | 100-full
     * 1000-half | 1000-full
     */
    REG_ITEM(MII_BMSR,      0x01, 0x7900),
    REG_ITEM(MII_PHYSID1,   0x02, 0x1234),
    REG_ITEM(MII_PHYSID2,   0x03, 0x5678),
    /*
     * 10-half | 10-full | 100-half | 100-full
     */
    REG_ITEM(MII_ADVERTISE, 0x04, 0x0060),
    /*
     * 10-half | 10-full | 100-half | 100-full
     */
    REG_ITEM(MII_LPA,       0x05, 0x0180),
    /*
     * 1000-half | 1000-full
     */
    REG_ITEM(MII_CTRL1000,  0x09, 0x0300),
    /*
     * 1000-half | 1000-full
     */
    REG_ITEM(MII_STAT1000,  0x0A, 0x0C00),
    REG_ITEM(MII_ESTATUS,   0x0F, 0x3000),
};

#undef REG_ITEM

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    qemu_irq irq;
    NICState *nic;
    NICConf conf;
    uint32_t mac_regs[STMMAC_NUM_REGS];
    uint16_t mii_regs[MII_NUM_REGS];
} STMMACState;

static const VMStateDescription stmmac_vmstate = {
    .name = TYPE_STMMAC,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_UINT32_ARRAY(mac_regs, STMMACState, STMMAC_NUM_REGS),
        VMSTATE_UINT16_ARRAY(mii_regs, STMMACState, MII_NUM_REGS),
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
        qemu_log("%s(mii): Bad read addr %#x reg %#x\n", TYPE_STMMAC, addr, reg);
        return 0;
    }

    uint16_t value = 0xFFFF;
    if (addr == 0) {
        switch (index) {
        case MII_BMCR:
            value = s->mii_regs[MII_BMCR];
            s->mii_regs[MII_BMCR] &= ~BMCR_RESET;
            break;
        case MII_BMSR:
            value = s->mii_regs[MII_BMSR];
            break;
        case MII_PHYSID1:
            value = s->mii_regs[MII_PHYSID1];
            break;
        case MII_PHYSID2:
            value = s->mii_regs[MII_PHYSID2];
            break;
        case MII_ADVERTISE:
            value = s->mii_regs[MII_ADVERTISE];
            break;
        case MII_LPA:
            value = s->mii_regs[MII_LPA];
            break;
        case MII_CTRL1000:
            value = s->mii_regs[MII_CTRL1000];
            break;
        case MII_STAT1000:
            value = s->mii_regs[MII_STAT1000];
            break;
        default:
            break;
        }
    }
#if 0
    qemu_log("%s(mii): read %#x from addr %#x %s (reg %#x)\n",
             TYPE_STMMAC, value, addr, mii_regs[index].name, reg);
#endif
    return value;
}

static void mii_write(STMMACState *s, uint8_t addr, uint8_t reg, uint16_t value)
{
    int index = stmmac_offset_to_index(mii_regs, MII_NUM_REGS, reg);

    switch (index) {
    case MII_BMCR:
        s->mii_regs[MII_BMCR] = value;
        if (s->mii_regs[MII_BMCR] & BMCR_RESET) {
            s->mii_regs[MII_BMSR] |= BMSR_LSTATE | BMSR_AUTONEG_CMPL;
        }
        break;
    case MII_ADVERTISE:
        s->mii_regs[MII_ADVERTISE] = value;
        break;
    case MII_CTRL1000:
        s->mii_regs[MII_CTRL1000] = value;
        break;
    case MII_STAT1000:
        s->mii_regs[MII_STAT1000] = value;
        break;
    default:
        qemu_log("%s(mii): Bad write %#x to addr %#x reg %#x\n",
                 TYPE_STMMAC, value, addr, reg);
        return;
    }
}

static uint64_t stmmac_read(void *opaque, hwaddr offset, unsigned size)
{
    STMMACState *s = STMMAC(opaque);
    int index = stmmac_offset_to_index(mac_regs, STMMAC_NUM_REGS, offset);

    if (index < 0) {
        qemu_log("%s: Bad read offset %#" HWADDR_PRIx "\n",
                 TYPE_STMMAC, offset);
        return 0;
    }

    uint64_t value = s->mac_regs[index];
    switch (index) {
    case GMAC_MII_ADDR:
        s->mac_regs[GMAC_MII_ADDR] &= ~MII_BUSY;
        break;
    case DMA_BUS_MODE:
        s->mac_regs[DMA_BUS_MODE] &= ~DMA_BUS_MODE_SFT_RESET;
    default:
        break;
    }
#if 0
    qemu_log("%s: read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")\n",
             TYPE_STMMAC, value, mac_regs[index].name, offset);
#endif
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
    case GMAC_FRAME_FILTER:
        s->mac_regs[GMAC_FRAME_FILTER] = (uint32_t)value;
        break;
    case GMAC_HASH_HI:
        s->mac_regs[GMAC_HASH_HI] = (uint32_t)value;
        break;
    case GMAC_HASH_LO:
        s->mac_regs[GMAC_HASH_LO] = (uint32_t)value;
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
        }
        break;
    case GMAC_MII_DATA:
        s->mac_regs[GMAC_MII_DATA] = (uint32_t)value;
        break;
    case GMAC_VER:
        s->mac_regs[GMAC_VER] = (uint32_t)value;
        break;
    case GMAC_INT_MASK:
        s->mac_regs[GMAC_INT_MASK] = (uint32_t)value;
        break;
    case GMAC_ADDR_HI:
        s->mac_regs[GMAC_ADDR_HI] = (uint32_t)value;
        break;
    case GMAC_ADDR_LO:
        s->mac_regs[GMAC_ADDR_LO] = (uint32_t)value;
        break;
    case MMC_CTRL:
        s->mac_regs[MMC_CTRL] = (uint32_t)value;
        break;
    case MMC_RX_INT_MASK:
        s->mac_regs[MMC_RX_INT_MASK] = (uint32_t)value;
        break;
    case MMC_TX_INT_MASK:
        s->mac_regs[MMC_TX_INT_MASK] = (uint32_t)value;
        break;
    case MMC_RX_IPC_INT_MASK:
        s->mac_regs[MMC_RX_IPC_INT_MASK] = (uint32_t)value;
        break;
    case DMA_BUS_MODE:
        s->mac_regs[DMA_BUS_MODE] = (uint32_t)value;
        break;
    case DMA_TX_POLL_DEMAND:
        s->mac_regs[DMA_TX_POLL_DEMAND] = (uint32_t)value;
        if (s->mac_regs[DMA_TX_POLL_DEMAND] != 0) {
            s->mac_regs[DMA_STATUS] |= DMA_STATUS_NIS | DMA_STATUS_TI;
            qemu_irq_raise(s->irq);
        }
        break;
    case DMA_RX_POLL_DEMAND:
        s->mac_regs[DMA_RX_POLL_DEMAND] = (uint32_t)value;
        break;
    case DMA_RX_BASE_ADDR:
        s->mac_regs[DMA_RX_BASE_ADDR] = (uint32_t)value;
        break;
    case DMA_TX_BASE_ADDR:
        s->mac_regs[DMA_TX_BASE_ADDR] = (uint32_t)value;
        break;
    case DMA_STATUS:
        s->mac_regs[DMA_STATUS] &= ~(uint32_t)value;
        if (s->mac_regs[DMA_STATUS] == 0) {
            qemu_irq_lower(s->irq);
        }
        break;
    case DMA_CTRL:
        s->mac_regs[DMA_CTRL] = (uint32_t)value;
        break;
    case DMA_INT_ENA:
        s->mac_regs[DMA_INT_ENA] = (uint32_t)value;
        break;
    case DMA_RX_WATCHDOG:
        s->mac_regs[DMA_RX_WATCHDOG] = (uint32_t)value;
        break;
    case DMA_AXI_BUS_MODE:
        s->mac_regs[DMA_AXI_BUS_MODE] = (uint32_t)value;
        break;
    case GMAC_RCPD:
        s->mac_regs[GMAC_RCPD] = (uint32_t)value;
        break;
    case GMAC_TCPD:
        s->mac_regs[GMAC_TCPD] = (uint32_t)value;
        break;
    default:
        qemu_log("%s: Bad write %#" PRIx64 " to offset %#" HWADDR_PRIx "\n",
                 TYPE_STMMAC, value, offset);
        return;
    }
#if 0
    qemu_log("%s: write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")\n",
             TYPE_STMMAC, value, mac_regs[index].name, offset);
#endif
}

static int stmmac_can_receive(NetClientState *nc)
{
    //STMMACState *s = qemu_get_nic_opaque(nc);

    qemu_log("%s: can_receive ?\n", TYPE_STMMAC);

    return 0;
}

static ssize_t stmmac_receive(NetClientState *nc, const uint8_t *buf, size_t size)
{
    //STMMACState *s = qemu_get_nic_opaque(nc);

    qemu_log("%s: receive\n", TYPE_STMMAC);

    return size;
}

static void stmmac_reset(DeviceState *dev)
{
    STMMACState *s = STMMAC(dev);
    MACAddr mac = s->conf.macaddr;

    for (int i = 0; i < STMMAC_NUM_REGS; ++i) {
        s->mac_regs[i] = mac_regs[i].reset_value;
    }
    for (int i = 0; i < MII_NUM_REGS; ++i) {
        s->mii_regs[i] = mii_regs[i].reset_value;
    }
    s->mac_regs[GMAC_ADDR_HI] = (mac.a[5] << 8) | mac.a[4];
    s->mac_regs[GMAC_ADDR_LO] = (mac.a[3] << 24) | (mac.a[2] << 16) | (mac.a[1] << 8) | mac.a[0];
}

static void stmmac_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = stmmac_read,
        .write      = stmmac_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
    };
    static NetClientInfo net_info = {
        .type = NET_CLIENT_DRIVER_NIC,
        .size = sizeof(NICState),
        .can_receive = stmmac_can_receive,
        .receive = stmmac_receive,
    };

    STMMACState *s = STMMAC(dev);

    qemu_macaddr_default_if_unset(&s->conf.macaddr);
    s->nic = qemu_new_nic(&net_info, &s->conf,
                          object_get_typename(OBJECT(dev)), dev->id, s);
    qemu_format_nic_info_str(qemu_get_queue(s->nic), s->conf.macaddr.a);

    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq);
    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_STMMAC, STMMAC_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void stmmac_class_init(ObjectClass *oc, void *data)
{
    static Property props[] = {
        DEFINE_NIC_PROPERTIES(STMMACState, conf),
        DEFINE_PROP_END_OF_LIST(),
    };

    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = stmmac_realize;
    dc->reset   = stmmac_reset;
    dc->vmsd    = &stmmac_vmstate;
    dc->props   = props;
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
