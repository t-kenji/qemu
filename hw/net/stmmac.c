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
#include "hw/net/mii.h"
#include "net/net.h"
#include "net/checksum.h"
#include "sysemu/dma.h"
#include "qemu/log.h"

//#define ENABLE_DEBUG

#define TYPE_STMMAC "stmmaceth"
#define STMMAC(obj) OBJECT_CHECK(STMMACState, (obj), TYPE_STMMAC)

enum STMMACMemoryMap {
    STMMAC_MMIO_SIZE = 0x9000,
    STMMAC_FRAME_SIZE = 0x2000,
};

enum STMMACValues {
    DMA_BUS_MODE_SFT_RESET = 0x00000001,
    DMA_STATUS_TI          = 0x00000001,
    DMA_STATUS_RI          = 0x00000040,
    DMA_STATUS_RU          = 0x00000080,
    DMA_STATUS_AIS         = 0x00008000,
    DMA_STATUS_NIS         = 0x00010000,
    DMA_CTRL_SR            = 0x00000002,
    DMA_CTRL_ST            = 0x00002000,
    DMA_DESC_LAST_DESC     = 0x00000100,
    DMA_DESC_1ST_DESC      = 0x00000200,
    DMA_DESC_END_RING      = 0x00200000,
    DMA_DESC_CSUM_INS      = 0x00C00000,
    DMA_DESC_LAST_SEG      = 0x20000000,
    DMA_DESC_OWNERED       = 0x80000000,
};

enum STMMACMIIValues {
    MII_BUSY = 0x0001,
};

#define MII_BMCR_INIT (MII_BMCR_AUTOEN | MII_BMCR_FD \
                       | MII_BMCR_SPEED1000)

#define MII_BMSR_INIT (MII_BMSR_100TX_FD | MII_BMSR_100TX_HD \
                       | MII_BMSR_10T_FD | MII_BMSR_10T_HD \
                       | MII_BMSR_EXTSTAT | MII_BMSR_MFPS \
                       | MII_BMSR_AN_COMP | MII_BMSR_AUTONEG \
                       | MII_BMSR_LINK_ST | MII_BMSR_EXTCAP)

#define MII_ANAR_INIT (MII_ANAR_PAUSE_ASYM | MII_ANAR_PAUSE \
                       | MII_ANAR_TXFD | MII_ANAR_TX \
                       | MII_ANAR_10FD | MII_ANAR_10)

#define MII_ANLPAR_INIT (MII_ANLPAR_ACK | MII_ANLPAR_PAUSE \
                         | MII_ANLPAR_TXFD | MII_ANLPAR_TX \
                         | MII_ANLPAR_10FD | MII_ANLPAR_10 \
                         | MII_ANLPAR_CSMACD)

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
    GMAC_FLOW_CTRL,
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
    REG_ITEM(GMAC_FLOW_CTRL,      0x0018, 0x00000000),
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

static const STMMACReg mii_regs[] = {
    REG_ITEM(MII_BMCR,     0x00, MII_BMCR_INIT),
    REG_ITEM(MII_BMSR,     0x01, MII_BMSR_INIT),
    REG_ITEM(MII_PHYID1,   0x02, 0x1234),
    REG_ITEM(MII_PHYID2,   0x03, 0x5678),
    REG_ITEM(MII_ANAR,     0x04, MII_ANAR_INIT),
    REG_ITEM(MII_ANLPAR,   0x05, MII_ANLPAR_INIT),
    REG_ITEM(MII_CTRL1000, 0x09, MII_CTRL1000_FULL | MII_CTRL1000_HALF),
    REG_ITEM(MII_STAT1000, 0x0A, MII_STAT1000_FULL | MII_STAT1000_HALF),
    REG_ITEM(MII_EXTSTAT,  0x0F, 0x3000),
};

#define MII_NUM_REGS ARRAY_SIZE(mii_regs)

#undef REG_ITEM

struct dma_desc {
    uint32_t ctrl_stat;
    uint16_t buffer1_size;
    uint16_t buffer2_size;
    uint32_t buffer1_addr;
    uint32_t buffer2_addr;
    uint32_t ext_stat;
    uint32_t reserve;
    uint32_t timestamp_lo;
    uint32_t timestamp_hi;
};

struct rx_tx_stats {
    uint64_t rx_bytes;
    uint64_t tx_bytes;

    uint64_t rx_count;
    uint64_t rx_count_bcast;
    uint64_t rx_count_mcast;
    uint64_t tx_count;
};

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    qemu_irq irq;
    NICState *nic;
    NICConf conf;
    struct rx_tx_stats stats;
    uint32_t cur_rx_desc_addr;
    uint32_t cur_tx_desc_addr;
    uint32_t mac_regs[STMMAC_NUM_REGS];
    uint16_t mii_regs[MII_NUM_REGS];
} STMMACState;

static const VMStateDescription stats_vmstate = {
    .name = TYPE_STMMAC "-stats",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_UINT64(rx_bytes, struct rx_tx_stats),
        VMSTATE_UINT64(tx_bytes, struct rx_tx_stats),
        VMSTATE_UINT64(rx_count, struct rx_tx_stats),
        VMSTATE_UINT64(rx_count_bcast, struct rx_tx_stats),
        VMSTATE_UINT64(rx_count_mcast, struct rx_tx_stats),
        VMSTATE_UINT64(tx_count, struct rx_tx_stats),
        VMSTATE_END_OF_LIST()
    },
};

static const VMStateDescription stmmac_vmstate = {
    .name = TYPE_STMMAC,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_STRUCT(stats, STMMACState, 0, stats_vmstate, struct rx_tx_stats),
        VMSTATE_UINT32(cur_rx_desc_addr, STMMACState),
        VMSTATE_UINT32(cur_tx_desc_addr, STMMACState),
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
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s(mii): Bad read addr %#x reg %#x\n",
                      TYPE_STMMAC, addr, reg);
        return 0;
    }

    uint16_t value = 0xFFFF;
    if (addr == 0) {
        switch (index) {
        case MII_BMCR:
            value = s->mii_regs[MII_BMCR];
            s->mii_regs[MII_BMCR] &= ~MII_BMCR_RESET;
            break;
        case MII_BMSR:
            value = s->mii_regs[MII_BMSR];
            break;
        case MII_PHYID1:
            value = s->mii_regs[MII_PHYID1];
            break;
        case MII_PHYID2:
            value = s->mii_regs[MII_PHYID2];
            break;
        case MII_ANAR:
            value = s->mii_regs[MII_ANAR];
            break;
        case MII_ANLPAR:
            value = s->mii_regs[MII_ANLPAR];
            break;
        case MII_CTRL1000:
            value = s->mii_regs[MII_CTRL1000];
            break;
        case MII_STAT1000:
            value = s->mii_regs[MII_STAT1000];
            break;
        case MII_EXTSTAT:
            value = s->mii_regs[MII_EXTSTAT];
            break;
        default:
            break;
        }
    }
#if defined(ENABLE_DEBUG)
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
        if (s->mii_regs[MII_BMCR] & MII_BMCR_RESET) {
            s->mii_regs[MII_BMCR] = MII_BMCR_INIT;
            s->mii_regs[MII_BMSR] = MII_BMSR_INIT;
        }
        break;
    case MII_ANAR:
        s->mii_regs[MII_ANAR] = value;
        break;
    case MII_CTRL1000:
        s->mii_regs[MII_CTRL1000] = value;
        break;
    case MII_STAT1000:
        s->mii_regs[MII_STAT1000] = value;
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s(mii): Bad write %#x to addr %#x reg %#x\n",
                      TYPE_STMMAC, value, addr, reg);
        return;
    }
#if defined(ENABLE_DEBUG)
    qemu_log("%s(mii): write %#x to addr %#x %s (reg %#x)\n",
             TYPE_STMMAC, value, addr, mii_regs[index].name, reg);
#endif
}

static void stmmac_update_irq(STMMACState *s)
{
    int level = s->mac_regs[DMA_STATUS] & s->mac_regs[DMA_INT_ENA];
    qemu_set_irq(s->irq, level);
}

static void stmmac_read_desc(STMMACState *s, struct dma_desc *desc, bool is_rx)
{
    uint32_t phys_addr = is_rx ? s->cur_rx_desc_addr : s->cur_tx_desc_addr;
    dma_memory_read(&address_space_memory, phys_addr, desc, sizeof(*desc));
}

static void stmmac_write_desc(STMMACState *s, struct dma_desc *desc, bool is_rx)
{
    uint32_t phys_addr = is_rx ? s->cur_rx_desc_addr : s->cur_tx_desc_addr;

    if (is_rx) {
        if ((desc->buffer1_size & 0x8000) != 0) {
            s->cur_rx_desc_addr = s->mac_regs[DMA_RX_BASE_ADDR];
        } else {
            s->cur_rx_desc_addr += sizeof(*desc);
        }
    } else {
        if ((desc->ctrl_stat & DMA_DESC_END_RING) != 0) {
            s->cur_tx_desc_addr = s->mac_regs[DMA_TX_BASE_ADDR];
        } else {
            s->cur_tx_desc_addr += sizeof(*desc);
        }
    }
    dma_memory_write(&address_space_memory, phys_addr, desc, sizeof(*desc));
}

static int stmmac_can_receive(NetClientState *nc)
{
    STMMACState *s = qemu_get_nic_opaque(nc);
    return (s->mac_regs[DMA_CTRL] & DMA_CTRL_SR) ? 1 : 0;
}

static ssize_t stmmac_receive(NetClientState *nc, const uint8_t *buf, size_t size)
{
    static const uint8_t sa_bcast[] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };

    STMMACState *s = qemu_get_nic_opaque(nc);
    bool is_uni = ~buf[0] & 0x01;
    bool is_bcast = memcmp(buf, sa_bcast, sizeof(sa_bcast));
    bool is_mcast = !is_uni && !is_bcast;
    struct dma_desc desc;
    ssize_t ret = size;

    do {
        if (size < 12) {
            s->mac_regs[DMA_STATUS] |= DMA_STATUS_NIS | DMA_STATUS_RI;
            ret = -1;
            break;
        }

        stmmac_read_desc(s, &desc, true);
        if ((desc.ctrl_stat & DMA_DESC_OWNERED) == 0) {
            s->mac_regs[DMA_STATUS] |= DMA_STATUS_AIS | DMA_STATUS_RU;
            break;
        }

        dma_memory_write(&address_space_memory, desc.buffer1_addr, buf, size);

        desc.ctrl_stat = (size << 16) | DMA_DESC_1ST_DESC | DMA_DESC_LAST_DESC;
        stmmac_write_desc(s, &desc, true);

        /* update stats */
        s->stats.rx_bytes += size;
        ++s->stats.rx_count;
        if (is_mcast) {
            ++s->stats.rx_count_mcast;
        } else if (is_bcast) {
            ++s->stats.rx_count_bcast;
        }

        s->mac_regs[DMA_STATUS] |= DMA_STATUS_NIS | DMA_STATUS_RI;
    } while (0);

    stmmac_update_irq(s);

    return ret;
}

static void stmmac_enet_send(STMMACState *s)
{
    static uint8_t frame[STMMAC_FRAME_SIZE];

    struct dma_desc desc;
    uint8_t *cur;
    size_t frame_size;
    size_t frag_size;

    cur = frame;
    frame_size = 0;

    while (1) {
        stmmac_read_desc(s, &desc, false);
        if ((desc.ctrl_stat & DMA_DESC_OWNERED) == 0) {
            break;
        }

        frag_size = (desc.buffer1_size & 0xFFF) + (desc.buffer2_size & 0xFFF);
        if (frag_size >= sizeof(frame)) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: buffer overflow %zu read into %zu buffer\n",
                          TYPE_STMMAC, frag_size, sizeof(frame));
        }

        dma_memory_read(&address_space_memory, desc.buffer1_addr, cur, frag_size);
        cur += frag_size;
        frame_size += frag_size;
        if (desc.ctrl_stat & DMA_DESC_LAST_SEG) {
            /* update stats */
            s->stats.tx_bytes += frame_size;
            ++s->stats.tx_count;

            if ((desc.ctrl_stat & DMA_DESC_CSUM_INS) != 0) {
                net_checksum_calculate(frame, frame_size);
            }
            qemu_send_packet(qemu_get_queue(s->nic), frame, frame_size);
            cur = frame;
            frame_size = 0;
            s->mac_regs[DMA_STATUS] |= DMA_STATUS_NIS | DMA_STATUS_TI;
        }

        desc.ctrl_stat &= ~DMA_DESC_OWNERED;
        stmmac_write_desc(s, &desc, false);
    }
}

static uint64_t stmmac_read(void *opaque, hwaddr offset, unsigned size)
{
    STMMACState *s = STMMAC(opaque);
    int index = stmmac_offset_to_index(mac_regs, STMMAC_NUM_REGS, offset);

    if (index < 0) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad read offset %#" HWADDR_PRIx "\n",
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
#if defined(ENABLE_DEBUG)
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
    case GMAC_FLOW_CTRL:
        s->mac_regs[GMAC_FLOW_CTRL] = (uint32_t)value;
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
        stmmac_enet_send(s);
        break;
    case DMA_RX_BASE_ADDR:
        s->mac_regs[DMA_RX_BASE_ADDR] = (uint32_t)value;
        s->cur_rx_desc_addr = s->mac_regs[DMA_RX_BASE_ADDR];
        break;
    case DMA_TX_BASE_ADDR:
        s->mac_regs[DMA_TX_BASE_ADDR] = (uint32_t)value;
        s->cur_tx_desc_addr = s->mac_regs[DMA_TX_BASE_ADDR];
        break;
    case DMA_STATUS:
        s->mac_regs[DMA_STATUS] &= ~(uint32_t)value;
        break;
    case DMA_CTRL:
        s->mac_regs[DMA_CTRL] = (uint32_t)value;
        if (stmmac_can_receive(qemu_get_queue(s->nic))) {
            qemu_flush_queued_packets(qemu_get_queue(s->nic));
        }
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
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad write %#" PRIx64 " to offset %#" HWADDR_PRIx "\n",
                      TYPE_STMMAC, value, offset);
        return;
    }

#if defined(ENABLE_DEBUG)
    qemu_log("%s: write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")\n",
             TYPE_STMMAC, value, mac_regs[index].name, offset);
#endif
    stmmac_update_irq(s);
}

static void stmmac_reset(DeviceState *dev)
{
    STMMACState *s = STMMAC(dev);
    MACAddr mac = s->conf.macaddr;

    s->stats.rx_bytes = 0;
    s->stats.tx_bytes = 0;
    s->stats.rx_count = 0;
    s->stats.rx_count_bcast = 0;
    s->stats.rx_count_mcast = 0;
    s->stats.tx_count = 0;
    s->cur_rx_desc_addr = 0;
    s->cur_tx_desc_addr = 0;
    for (int i = 0; i < STMMAC_NUM_REGS; ++i) {
        s->mac_regs[i] = mac_regs[i].reset_value;
    }
    for (int i = 0; i < MII_NUM_REGS; ++i) {
        s->mii_regs[i] = mii_regs[i].reset_value;
    }
    s->mac_regs[GMAC_ADDR_HI] = (mac.a[5] << 8)
                                | mac.a[4];
    s->mac_regs[GMAC_ADDR_LO] = (mac.a[3] << 24)
                                | (mac.a[2] << 16)
                                | (mac.a[1] << 8)
                                | mac.a[0];
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

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_STMMAC, STMMAC_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq);

    qemu_macaddr_default_if_unset(&s->conf.macaddr);
    s->nic = qemu_new_nic(&net_info, &s->conf,
                          object_get_typename(OBJECT(dev)), dev->id, s);
    qemu_format_nic_info_str(qemu_get_queue(s->nic), s->conf.macaddr.a);
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
    set_bit(DEVICE_CATEGORY_NETWORK, dc->categories);
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
