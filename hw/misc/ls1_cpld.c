/*
 * Copyright (c) 2019 t-kenji <protect.2501@gmail.com>
 *
 * QorIQ LayerScape1 CPLD pseudo-device
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
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * *****************************************************************
 *
 * The documentation for this device is noted in the LS1046A documentation,
 * file name "LS1046ARDBRM.pdf". You can easily find it on the web.
 *
 */

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qemu/log.h"

//#define ENABLE_DEBUG

#define TYPE_LS1_CPLD "ls1.cpld"
#define LS1_CPLD(obj) OBJECT_CHECK(LS1CPLDState, (obj), TYPE_LS1_CPLD)

#if defined(ENABLE_DEBUG)
#define DEBUG(format, ...) \
    qemu_log("%s: " format "\n", TYPE_LS1_CPLD, ##__VA_ARGS__)
#else
#define DEBUG(format, ...)
#endif
#define ERROR(format, ...) \
    qemu_log_mask(LOG_GUEST_ERROR, "%s: " format "\n", TYPE_LS1_CPLD, ##__VA_ARGS__)

enum {
    LS1_CPLD_MMIO_SIZE = 0x00000100,
};

typedef struct {
    const char *name;
    hwaddr offset;
    uint8_t reset_value;
} RegInfo;

#define REG(i, o, r) \
    [i] = {.name=#i, .offset=(o), .reset_value=(r)}

enum LS1CPLDRegs {
    CPLD_VER,
    CPLD_VER_SUB,
    PCBA_VER,
    REG_SYSTEM_RST,
    REG_SOFT_MUX_ON,
    REG_CFG_RCW_SRC1,
    REG_CFG_RCW_SRC2,
    REG_QSPI_BANK,
    REG_SYSCLK_SEL,
    REG_UART1_SEL,
    REG_SD1REFCLK_SEL,
    REG_RGMII_1588_SEL,
    REG_1588_CLK_SEL,
    REG_STATUS_LED,
    REG_GLOBAL_RST,
    REG_SD_EMM,
    REG_VDD_EN,
    REG_VDD_SEL,
    REG_SFP_TXEN,
    REG_SFP_STATUS,
};

static const RegInfo ls1_cpld_regs[] = {
    REG(CPLD_VER,           0x00, 0x02),
    REG(CPLD_VER_SUB,       0x01, 0x01),
    REG(PCBA_VER,           0x02, 0x02),
    REG(REG_SYSTEM_RST,     0x03, 0x00),
    REG(REG_SOFT_MUX_ON,    0x04, 0x00),
    REG(REG_CFG_RCW_SRC1,   0x05, 0x04),
    REG(REG_CFG_RCW_SRC2,   0x06, 0x04),
    REG(REG_QSPI_BANK,      0x07, 0x00),
    REG(REG_SYSCLK_SEL,     0x08, 0x00),
    REG(REG_UART1_SEL,      0x09, 0x00),
    REG(REG_SD1REFCLK_SEL,  0x0A, 0x01),
    REG(REG_RGMII_1588_SEL, 0x0B, 0x00),
    REG(REG_1588_CLK_SEL,   0x0C, 0x00),
    REG(REG_STATUS_LED,     0x0D, 0x00),
    REG(REG_GLOBAL_RST,     0x0E, 0x00),
    REG(REG_SD_EMM,         0x0F, 0x00),
    REG(REG_VDD_EN,         0x10, 0x00),
    REG(REG_VDD_SEL,        0x11, 0x00),
    REG(REG_SFP_TXEN,       0x12, 0x00),
    REG(REG_SFP_STATUS,     0x13, 0x00),
};

#undef REG

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint8_t system_reset;
    uint16_t cfg_rcw_src;
    uint8_t qspi_bank;
    uint8_t sd1refclk_sel;
} LS1CPLDState;

static const VMStateDescription ls1_cpld_vmstate = {
    .name = TYPE_LS1_CPLD,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_UINT8(system_reset, LS1CPLDState),
        VMSTATE_UINT16(cfg_rcw_src, LS1CPLDState),
        VMSTATE_UINT8(qspi_bank, LS1CPLDState),
        VMSTATE_UINT8(sd1refclk_sel, LS1CPLDState),
        VMSTATE_END_OF_LIST()
    },
};

static int internal_offset_to_index(const RegInfo *regs, size_t length, hwaddr offset)
{
    for (size_t i = 0; i < length; ++i) {
        if (regs[i].offset == offset) {
            return i;
        }
    }

    return -1;
}

#define offset_to_index(regs, offset) \
    internal_offset_to_index(regs, ARRAY_SIZE(regs), offset)

static uint64_t ls1_cpld_read(void *opaque, hwaddr offset, unsigned size)
{
    LS1CPLDState *s = LS1_CPLD(opaque);
    int index = offset_to_index(ls1_cpld_regs, offset);

    uint64_t value;
    switch (index) {
    case CPLD_VER:
        value = ls1_cpld_regs[CPLD_VER].reset_value;
        break;
    case CPLD_VER_SUB:
        value = ls1_cpld_regs[CPLD_VER_SUB].reset_value;
        break;
    case PCBA_VER:
        value = ls1_cpld_regs[PCBA_VER].reset_value;
        break;
    case REG_SYSTEM_RST:
        value = s->system_reset;
        break;
    case REG_CFG_RCW_SRC1:
        value = s->cfg_rcw_src & 0xFF;
        break;
    case REG_CFG_RCW_SRC2:
        value = (s->cfg_rcw_src >> 8) & 0xFF;
        break;
    case REG_QSPI_BANK:
        value = s->qspi_bank;
        break;
    case REG_SD1REFCLK_SEL:
        value = s->sd1refclk_sel;
        break;
    default:
        ERROR("Bad read offset %#" HWADDR_PRIx, offset);
        return 0;
    }

    DEBUG("Read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")",
          value, ls1_cpld_regs[index].name, offset);
    return value;
}

static void ls1_cpld_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    LS1CPLDState *s = LS1_CPLD(opaque);
    int index = offset_to_index(ls1_cpld_regs, offset);

    switch (index) {
    case REG_SYSTEM_RST:
        s->system_reset = (uint8_t)value;
        break;
    case REG_CFG_RCW_SRC1:
        s->cfg_rcw_src = (uint8_t)(value & 0xFF);
        break;
    case REG_CFG_RCW_SRC2:
        s->cfg_rcw_src = (uint8_t)((value >> 8) & 0xFF);
        break;
    case REG_QSPI_BANK:
        s->qspi_bank = (uint8_t)value;
        break;
    case REG_SD1REFCLK_SEL:
        s->sd1refclk_sel = (uint8_t)value;
        break;
    default:
        ERROR("Bad write %#" PRIx64 " to offset %#" HWADDR_PRIx, value, offset);
        return;
    }
    DEBUG("Write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
          value, ls1_cpld_regs[index].name, offset);
}

static void ls1_cpld_reset(DeviceState *dev)
{
    LS1CPLDState *s = LS1_CPLD(dev);

    s->system_reset = ls1_cpld_regs[REG_SYSTEM_RST].reset_value;
    s->cfg_rcw_src = (ls1_cpld_regs[REG_CFG_RCW_SRC2].reset_value << 8)
                     + ls1_cpld_regs[REG_CFG_RCW_SRC1].reset_value;
    s->qspi_bank = ls1_cpld_regs[REG_QSPI_BANK].reset_value;
    s->sd1refclk_sel = ls1_cpld_regs[REG_SD1REFCLK_SEL].reset_value;
}

static void ls1_cpld_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = ls1_cpld_read,
        .write      = ls1_cpld_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
    };

    LS1CPLDState *s = LS1_CPLD(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_LS1_CPLD, LS1_CPLD_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void ls1_cpld_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = ls1_cpld_realize;
    dc->reset   = ls1_cpld_reset;
    dc->vmsd    = &ls1_cpld_vmstate;
}

static void ls1_cpld_register_types(void)
{
    static const TypeInfo cpld_info = {
        .name = TYPE_LS1_CPLD,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(LS1CPLDState),
        .class_init = ls1_cpld_class_init,
    };

    type_register_static(&cpld_info);
}

type_init(ls1_cpld_register_types)
