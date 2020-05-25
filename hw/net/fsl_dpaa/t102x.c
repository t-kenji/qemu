/*
 * QorIQ T102x Data Path Acceleration Architecture definitions.
 *
 * Copyright (c) 2020 t-kenji <protect.2501@gmail.com>
 */

#include "qemu/osdep.h"
#include "exec/address-spaces.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "hw/misc/gen-reg.h"
#include "qemu/log.h"

#define ENABLE_DEBUG
#define DEBUG_SEC 1
#define DEBUG_QMAN 1
#define DEBUG_BMAN 1
#define DEBUG_FMAN 1

enum T102xDPAAMemorySize {
    SEC_MMIO_SIZE = 0x66000,
    QMAN_MMIO_SIZE = 0x2000,
    BMAN_MMIO_SIZE = 0x1000,
    FMAN_MMIO_SIZE = 0x100000,
};

#define TYPE_T102X_SEC "t102x-sec"
#define T102X_SEC(obj) OBJECT_CHECK(T102xSECState, (obj), TYPE_T102X_SEC)
#define TYPE_T102X_QMAN "t102x-qman"
#define T102X_QMAN(obj) OBJECT_CHECK(T102xQMANState, (obj), TYPE_T102X_QMAN)
#define TYPE_T102X_BMAN "t102x-bman"
#define T102X_BMAN(obj) OBJECT_CHECK(T102xBMANState, (obj), TYPE_T102X_BMAN)
#define TYPE_T102X_FMAN "t102x-fman"
#define T102X_FMAN(obj) OBJECT_CHECK(T102xFMANState, (obj), TYPE_T102X_FMAN)

#if defined(ENABLE_DEBUG)
#define DBG(type, format, ...)                          \
    do {                                                \
        if (DEBUG_##type) {                             \
            qemu_log("%s: " format "\n",                \
                     TYPE_T102X_##type, ##__VA_ARGS__); \
        }                                               \
    } while (0)
#else
#define DBG(type, format, ...) do {} while (0)
#endif
#define ERR(type, format, ...)                              \
    do {                                                    \
        qemu_log_mask(LOG_GUEST_ERROR, "%s: " format "\n",  \
                      TYPE_T102X_##type, ##__VA_ARGS__);    \
    } while (0)

#define GET_FIELD(reg, fld, val) \
    (((val) & reg##__##fld##_MASK) >> (31 - reg##__##fld##_BIT))

#define CLEAR_FIELD(reg, fld, var) \
    (var) &= ~reg##__##fld##_MASK

#define SET_FIELD(reg, fld, var, val)                                        \
    ({                                                                       \
        CLEAR_FIELD(reg, fld, var);                                          \
        (var) |= ((val) << (31 - reg##__##fld##_BIT)) & reg##__##fld##_MASK; \
    })

enum {
    SEC_MCFGR,
    SEC_SCFGR,
    SEC_JR0LIODNR_MS,
    SEC_JR0LIODNR_LS,
    SEC_JR1LIODNR_MS,
    SEC_JR1LIODNR_LS,
    SEC_JR2LIODNR_MS,
    SEC_JR2LIODNR_LS,
    SEC_JR3LIODNR_MS,
    SEC_JR3LIODNR_LS,
    SEC_QISDID,
    SEC_CRNR_MS,
    SEC_CRNR_LS,
    SEC_CTPR_MS,
    SEC_CTPR_LS,
    SEC_IRBAR_JR0h,
    SEC_IRBAR_JR0l,
    SEC_IRSR_JR0,
    SEC_IRSAR_JR0,
    SEC_IRJAR_JR0,
    SEC_ORBAR_JR0h,
    SEC_ORBAR_JR0l,
    SEC_ORSR_JR0,
    SEC_ORJRR_JR0,
    SEC_ORSFR_JR0,
    SEC_JRSTAR_JR0,
    SEC_JRINTR_JR0,
    SEC_JRCFGR_JR0_MS,
    SEC_JRCFGR_JR0_LS,
    SEC_IRRIR_JR0,
    SEC_ORWIR_JR0,
};

static const RegDef32 t102x_sec_regs[] = {
    REG_ITEM(SEC_MCFGR,         0x0004, 0x00002140, 0xFFFFFFFF),
    REG_ITEM(SEC_SCFGR,         0x000C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SEC_JR0LIODNR_MS,  0x0010, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SEC_JR0LIODNR_LS,  0x0014, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SEC_JR1LIODNR_MS,  0x0018, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SEC_JR1LIODNR_LS,  0x001C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SEC_JR2LIODNR_MS,  0x0020, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SEC_JR2LIODNR_LS,  0x0024, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SEC_JR3LIODNR_MS,  0x0028, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SEC_JR3LIODNR_LS,  0x002C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SEC_QISDID,        0x0050, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SEC_CRNR_MS,       0x0FA0, 0x00000124, 0x00000000),
    REG_ITEM(SEC_CRNR_LS,       0x0FA4, 0x44134107, 0x00000000),
    REG_ITEM(SEC_CTPR_MS,       0x0FA8, 0x4EBF0201, 0x00000000),
    REG_ITEM(SEC_CTPR_LS,       0x0FAC, 0x00007FFB, 0x00000000),
    REG_ITEM(SEC_IRBAR_JR0h,    0x1000, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SEC_IRBAR_JR0l,    0x1004, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SEC_IRSR_JR0,      0x100C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SEC_IRSAR_JR0,     0x1014, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SEC_IRJAR_JR0,     0x101C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SEC_ORBAR_JR0h,    0x1020, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SEC_ORBAR_JR0l,    0x1024, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SEC_ORSR_JR0,      0x102C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SEC_ORJRR_JR0,     0x1034, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SEC_ORSFR_JR0,     0x103C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SEC_JRSTAR_JR0,    0x1044, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SEC_JRINTR_JR0,    0x104C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SEC_JRCFGR_JR0_MS, 0x1050, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SEC_JRCFGR_JR0_LS, 0x1054, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SEC_IRRIR_JR0,     0x105C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SEC_ORWIR_JR0,     0x1064, 0x00000000, 0xFFFFFFFF),
};

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[ARRAY_SIZE(t102x_sec_regs)];
} T102xSECState;

static uint64_t t102x_sec_read(void *opaque, hwaddr offset, unsigned size)
{
    T102xSECState *s = T102X_SEC(opaque);
    RegDef32 reg = regdef_find(t102x_sec_regs, offset);

    if (reg.index < 0) {
        ERR(SEC, "Bad read offset %#" HWADDR_PRIx, offset);
        return 0;
    }

    uint64_t value = s->regs[reg.index];

    DBG(SEC, "Read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    return value;
}

static void t102x_sec_write(void *opaque, hwaddr offset, uint64_t value,
                            unsigned size)
{
    T102xSECState *s = T102X_SEC(opaque);
    RegDef32 reg = regdef_find(t102x_sec_regs, offset);

    if (reg.index < 0) {
        ERR(SEC, "Bad write offset %#" HWADDR_PRIx, offset);
        return;
    }

    DBG(SEC, "Write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    if (!!(value & ~reg.write_mask)) {
        ERR(SEC, "Maybe write to a read only bit %#" PRIx64,
            (value & ~reg.write_mask));
    }

    switch (reg.index) {
    default:
        s->regs[reg.index] = value;
        break;
    }
}

static void t102x_sec_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = t102x_sec_read,
        .write      = t102x_sec_write,
        .endianness = DEVICE_BIG_ENDIAN,
    };

    T102xSECState *s = T102X_SEC(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_T102X_SEC, SEC_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void t102x_sec_reset(DeviceState *dev)
{
    T102xSECState *s = T102X_SEC(dev);

    for (int i = 0; i < ARRAY_SIZE(s->regs); ++i) {
        s->regs[i] = t102x_sec_regs[i].reset_value;
    }
}

static void t102x_sec_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = t102x_sec_realize;
    dc->reset   = t102x_sec_reset;
}

enum {
    QMAN_BARE,
    QMAN_BAR,
    QMAN_SRCIDR,
    QMAN_LIODNR,
    QCSP0_LIO_CFG,
    QCSP0_IO_CFG,
    QCSP0_DD_CFG,
    QCSP1_LIO_CFG,
    QCSP1_IO_CFG,
    QCSP1_DD_CFG,
    QCSP2_LIO_CFG,
    QCSP2_IO_CFG,
    QCSP2_DD_CFG,
    QCSP3_LIO_CFG,
    QCSP3_IO_CFG,
    QCSP3_DD_CFG,
    QCSP4_LIO_CFG,
    QCSP4_IO_CFG,
    QCSP4_DD_CFG,
    QCSP5_LIO_CFG,
    QCSP5_IO_CFG,
    QCSP5_DD_CFG,
    QCSP6_LIO_CFG,
    QCSP6_IO_CFG,
    QCSP6_DD_CFG,
    QCSP7_LIO_CFG,
    QCSP7_IO_CFG,
    QCSP7_DD_CFG,
    QCSP8_LIO_CFG,
    QCSP8_IO_CFG,
    QCSP8_DD_CFG,
    QCSP9_LIO_CFG,
    QCSP9_IO_CFG,
    QCSP9_DD_CFG,
};

static const RegDef32 t102x_qman_regs[] = {
    REG_ITEM(QMAN_BARE,     0x0C80, 0x00000000, 0x0000FFFF),
    REG_ITEM(QMAN_BAR,      0x0C84, 0x00000000, 0xFE000000),
    REG_ITEM(QMAN_SRCIDR,   0x0D04, 0x0000003C, 0x00000000),
    REG_ITEM(QMAN_LIODNR,   0x0D08, 0x00000000, 0x00000FFF),
    REG_ITEM(QCSP0_LIO_CFG, 0x1000, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(QCSP0_IO_CFG,  0x1004, 0x00000000, 0x00FF0FFF),
    REG_ITEM(QCSP0_DD_CFG,  0x100C, 0x00000000, 0x01FF01FF),
    REG_ITEM(QCSP1_LIO_CFG, 0x1010, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(QCSP1_IO_CFG,  0x1014, 0x00000000, 0x00FF0FFF),
    REG_ITEM(QCSP1_DD_CFG,  0x101C, 0x00000000, 0x01FF01FF),
    REG_ITEM(QCSP2_LIO_CFG, 0x1020, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(QCSP2_IO_CFG,  0x1024, 0x00000000, 0x00FF0FFF),
    REG_ITEM(QCSP2_DD_CFG,  0x102C, 0x00000000, 0x01FF01FF),
    REG_ITEM(QCSP3_LIO_CFG, 0x1030, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(QCSP3_IO_CFG,  0x1034, 0x00000000, 0x00FF0FFF),
    REG_ITEM(QCSP3_DD_CFG,  0x103C, 0x00000000, 0x01FF01FF),
    REG_ITEM(QCSP4_LIO_CFG, 0x1040, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(QCSP4_IO_CFG,  0x1044, 0x00000000, 0x00FF0FFF),
    REG_ITEM(QCSP4_DD_CFG,  0x104C, 0x00000000, 0x01FF01FF),
    REG_ITEM(QCSP5_LIO_CFG, 0x1050, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(QCSP5_IO_CFG,  0x1054, 0x00000000, 0x00FF0FFF),
    REG_ITEM(QCSP5_DD_CFG,  0x105C, 0x00000000, 0x01FF01FF),
    REG_ITEM(QCSP6_LIO_CFG, 0x1060, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(QCSP6_IO_CFG,  0x1064, 0x00000000, 0x00FF0FFF),
    REG_ITEM(QCSP6_DD_CFG,  0x106C, 0x00000000, 0x01FF01FF),
    REG_ITEM(QCSP7_LIO_CFG, 0x1070, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(QCSP7_IO_CFG,  0x1074, 0x00000000, 0x00FF0FFF),
    REG_ITEM(QCSP7_DD_CFG,  0x107C, 0x00000000, 0x01FF01FF),
    REG_ITEM(QCSP8_LIO_CFG, 0x1080, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(QCSP8_IO_CFG,  0x1084, 0x00000000, 0x00FF0FFF),
    REG_ITEM(QCSP8_DD_CFG,  0x108C, 0x00000000, 0x01FF01FF),
    REG_ITEM(QCSP9_LIO_CFG, 0x1090, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(QCSP9_IO_CFG,  0x1094, 0x00000000, 0x00FF0FFF),
    REG_ITEM(QCSP9_DD_CFG,  0x109C, 0x00000000, 0x01FF01FF),
};

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[ARRAY_SIZE(t102x_qman_regs)];
} T102xQMANState;

static uint64_t t102x_qman_read(void *opaque, hwaddr offset, unsigned size)
{
    T102xQMANState *s = T102X_QMAN(opaque);
    RegDef32 reg = regdef_find(t102x_qman_regs, offset);

    if (reg.index < 0) {
        ERR(QMAN, "Bad read offset %#" HWADDR_PRIx, offset);
        return 0;
    }

    uint64_t value = s->regs[reg.index];

    DBG(QMAN, "Read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    return value;
}

static void t102x_qman_write(void *opaque, hwaddr offset, uint64_t value,
                            unsigned size)
{
    T102xQMANState *s = T102X_QMAN(opaque);
    RegDef32 reg = regdef_find(t102x_qman_regs, offset);

    if (reg.index < 0) {
        ERR(QMAN, "Bad write offset %#" HWADDR_PRIx, offset);
        return;
    }

    DBG(QMAN, "Write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    if (!!(value & ~reg.write_mask)) {
        ERR(QMAN, "Maybe write to a read only bit %#" PRIx64,
            (value & ~reg.write_mask));
    }

    switch (reg.index) {
    default:
        s->regs[reg.index] = value;
        break;
    }
}

static void t102x_qman_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = t102x_qman_read,
        .write      = t102x_qman_write,
        .endianness = DEVICE_BIG_ENDIAN,
    };

    T102xQMANState *s = T102X_QMAN(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_T102X_QMAN, QMAN_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void t102x_qman_reset(DeviceState *dev)
{
    T102xQMANState *s = T102X_QMAN(dev);

    for (int i = 0; i < ARRAY_SIZE(s->regs); ++i) {
        s->regs[i] = t102x_qman_regs[i].reset_value;
    }
}

static void t102x_qman_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = t102x_qman_realize;
    dc->reset   = t102x_qman_reset;
}

enum {
    BMAN_SRCIDR,
    BMAN_LIODNR,
};

static const RegDef32 t102x_bman_regs[] = {
    REG_ITEM(BMAN_SRCIDR, 0x0D04, 0x00000018, 0x00000000),
    REG_ITEM(BMAN_LIODNR, 0x0D08, 0x00000000, 0x00000FFF),
};

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[ARRAY_SIZE(t102x_bman_regs)];
} T102xBMANState;

static uint64_t t102x_bman_read(void *opaque, hwaddr offset, unsigned size)
{
    T102xBMANState *s = T102X_BMAN(opaque);
    RegDef32 reg = regdef_find(t102x_bman_regs, offset);

    if (reg.index < 0) {
        ERR(BMAN, "Bad read offset %#" HWADDR_PRIx, offset);
        return 0;
    }

    uint64_t value = s->regs[reg.index];

    DBG(BMAN, "Read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    return value;
}

static void t102x_bman_write(void *opaque, hwaddr offset, uint64_t value,
                            unsigned size)
{
    T102xBMANState *s = T102X_BMAN(opaque);
    RegDef32 reg = regdef_find(t102x_bman_regs, offset);

    if (reg.index < 0) {
        ERR(BMAN, "Bad write offset %#" HWADDR_PRIx, offset);
        return;
    }

    DBG(BMAN, "Write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    if (!!(value & ~reg.write_mask)) {
        ERR(BMAN, "Maybe write to a read only bit %#" PRIx64,
            (value & ~reg.write_mask));
    }

    switch (reg.index) {
    default:
        s->regs[reg.index] = value;
        break;
    }
}

static void t102x_bman_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = t102x_bman_read,
        .write      = t102x_bman_write,
        .endianness = DEVICE_BIG_ENDIAN,
    };

    T102xBMANState *s = T102X_BMAN(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_T102X_BMAN, BMAN_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void t102x_bman_reset(DeviceState *dev)
{
    T102xBMANState *s = T102X_BMAN(dev);

    for (int i = 0; i < ARRAY_SIZE(s->regs); ++i) {
        s->regs[i] = t102x_bman_regs[i].reset_value;
    }
}

static void t102x_bman_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = t102x_bman_realize;
    dc->reset   = t102x_bman_reset;
}

enum {
    FMBM_SPLIODN_1,
    FMBM_SPLIODN_2,
    FMBM_SPLIODN_3,
    FMBM_SPLIODN_4,
    FMBM_SPLIODN_5,
    FMBM_SPLIODN_6,
    FMBM_SPLIODN_7,
    FMBM_SPLIODN_8,
    FMBM_SPLIODN_9,
    FMBM_SPLIODN_10,
    FMBM_SPLIODN_11,
    FMBM_SPLIODN_12,
    FMBM_SPLIODN_13,
    FMBM_SPLIODN_14,
    FMBM_SPLIODN_15,
    FMBM_SPLIODN_16,
    FMBM_SPLIODN_17,
    FMBM_SPLIODN_40,
    FMBM_SPLIODN_41,
    FMBM_SPLIODN_42,
    FMBM_SPLIODN_43,
    FMBM_SPLIODN_44,
    FMBM_SPLIODN_45,
    FMBM_SPLIODN_46,
    FMBM_SPLIODN_47,
    FMBM_SPLIODN_48,
    FMBM_SPLIODN_49,
    FMDM_SR,
    FMDM_MR,
    FMDM_TR,
    FMDM_HY,
    FMDM_SETR,
    FMDM_TAH,
    FMDM_TAL,
    FMDM_TCID,
    FMDM_WCR,
    FMDM_EBCR,
    FMDM_DCR,
    FMDM_EMSR,
    FMDM_PLR0,
    FMDM_PLR1,
    FMDM_PLR2,
    FMDM_PLR3,
    FMDM_PLR4,
    FMDM_PLR5,
    FMDM_PLR6,
    FMDM_PLR7,
    FMDM_PLR8,
    FMDM_PLR9,
    FMDM_PLR10,
    FMDM_PLR11,
    FMDM_PLR12,
    FMDM_PLR13,
    FMDM_PLR14,
    FMDM_PLR15,
    FMDM_PLR16,
    FMDM_PLR17,
    FMDM_PLR18,
    FMDM_PLR19,
    FMDM_PLR20,
    FMDM_PLR21,
    FMDM_PLR22,
    FMDM_PLR23,
    FMDM_PLR24,
    FMDM_PLR25,
    FMDM_PLR26,
    FMDM_PLR27,
    FMDM_PLR28,
    FMDM_PLR29,
    FMDM_PLR30,
    FMDM_PLR31,
};

static const RegDef32 t102x_fman_regs[] = {
    REG_ITEM(FMBM_SPLIODN_1,    0x80304, 0x00000000, 0xF0FF0FFF),
    REG_ITEM(FMBM_SPLIODN_2,    0x80308, 0x00000000, 0xF0FF0FFF),
    REG_ITEM(FMBM_SPLIODN_3,    0x8030C, 0x00000000, 0xF0FF0FFF),
    REG_ITEM(FMBM_SPLIODN_4,    0x80310, 0x00000000, 0xF0FF0FFF),
    REG_ITEM(FMBM_SPLIODN_5,    0x80314, 0x00000000, 0xF0FF0FFF),
    REG_ITEM(FMBM_SPLIODN_6,    0x80318, 0x00000000, 0xF0FF0FFF),
    REG_ITEM(FMBM_SPLIODN_7,    0x8031C, 0x00000000, 0xF0FF0FFF),
    REG_ITEM(FMBM_SPLIODN_8,    0x80320, 0x00000000, 0xF0FF0FFF),
    REG_ITEM(FMBM_SPLIODN_9,    0x80324, 0x00000000, 0xF0FF0FFF),
    REG_ITEM(FMBM_SPLIODN_10,   0x80328, 0x00000000, 0xF0FF0FFF),
    REG_ITEM(FMBM_SPLIODN_11,   0x8032C, 0x00000000, 0xF0FF0FFF),
    REG_ITEM(FMBM_SPLIODN_12,   0x80330, 0x00000000, 0xF0FF0FFF),
    REG_ITEM(FMBM_SPLIODN_13,   0x80334, 0x00000000, 0xF0FF0FFF),
    REG_ITEM(FMBM_SPLIODN_14,   0x80338, 0x00000000, 0xF0FF0FFF),
    REG_ITEM(FMBM_SPLIODN_15,   0x8033C, 0x00000000, 0xF0FF0FFF),
    REG_ITEM(FMBM_SPLIODN_16,   0x80340, 0x00000000, 0xF0FF0FFF),
    REG_ITEM(FMBM_SPLIODN_17,   0x80344, 0x00000000, 0xF0FF0FFF),
    REG_ITEM(FMBM_SPLIODN_40,   0x803A0, 0x00000000, 0xF0FF0FFF),
    REG_ITEM(FMBM_SPLIODN_41,   0x803A4, 0x00000000, 0xF0FF0FFF),
    REG_ITEM(FMBM_SPLIODN_42,   0x803A8, 0x00000000, 0xF0FF0FFF),
    REG_ITEM(FMBM_SPLIODN_43,   0x803AC, 0x00000000, 0xF0FF0FFF),
    REG_ITEM(FMBM_SPLIODN_44,   0x803B0, 0x00000000, 0xF0FF0FFF),
    REG_ITEM(FMBM_SPLIODN_45,   0x803B4, 0x00000000, 0xF0FF0FFF),
    REG_ITEM(FMBM_SPLIODN_46,   0x803B8, 0x00000000, 0xF0FF0FFF),
    REG_ITEM(FMBM_SPLIODN_47,   0x803BC, 0x00000000, 0xF0FF0FFF),
    REG_ITEM(FMBM_SPLIODN_48,   0x803C0, 0x00000000, 0xF0FF0FFF),
    REG_ITEM(FMBM_SPLIODN_49,   0x803C4, 0x2000E800, 0xF0FF0FFF),
    REG_ITEM(FMDM_SR,           0xC2000, 0x19001900, 0xFFFFFFFF),
    REG_ITEM(FMDM_MR,           0xC2004, 0x11000000, 0xFFFFFFFF),
    REG_ITEM(FMDM_TR,           0xC2008, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(FMDM_HY,           0xC200C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(FMDM_SETR,         0xC2010, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(FMDM_TAH,          0xC2014, 0x00000000, 0x00000000),
    REG_ITEM(FMDM_TAL,          0xC2018, 0x00000000, 0x00000000),
    REG_ITEM(FMDM_TCID,         0xC201C, 0x00000000, 0x00000000),
    REG_ITEM(FMDM_WCR,          0xC2028, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(FMDM_EBCR,         0xC202C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(FMDM_DCR,          0xC2054, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(FMDM_EMSR,         0xC2058, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(FMDM_PLR0,         0xC2060, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR1,         0xC2064, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR2,         0xC2068, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR3,         0xC206C, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR4,         0xC2070, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR5,         0xC2074, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR6,         0xC2078, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR7,         0xC207C, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR8,         0xC2080, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR9,         0xC2084, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR10,        0xC2088, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR11,        0xC208C, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR12,        0xC2090, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR13,        0xC2094, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR14,        0xC2098, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR15,        0xC209C, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR16,        0xC20A0, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR17,        0xC20A4, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR18,        0xC20A8, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR19,        0xC20AC, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR20,        0xC20B0, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR21,        0xC20B4, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR22,        0xC20B8, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR23,        0xC20BC, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR24,        0xC20C0, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR25,        0xC20C4, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR26,        0xC20C8, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR27,        0xC20CC, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR28,        0xC20D0, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR29,        0xC20D4, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR30,        0xC20D8, 0x00000000, 0x0FFF0FFF),
    REG_ITEM(FMDM_PLR31,        0xC20DC, 0x00000000, 0x0FFF0FFF),
};

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[ARRAY_SIZE(t102x_fman_regs)];
} T102xFMANState;

static uint64_t t102x_fman_read(void *opaque, hwaddr offset, unsigned size)
{
    T102xFMANState *s = T102X_FMAN(opaque);
    RegDef32 reg = regdef_find(t102x_fman_regs, offset);

    if (reg.index < 0) {
        ERR(FMAN, "Bad read offset %#" HWADDR_PRIx, offset);
        return 0;
    }

    uint64_t value = s->regs[reg.index];

    DBG(FMAN, "Read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    return value;
}

static void t102x_fman_write(void *opaque, hwaddr offset, uint64_t value,
                            unsigned size)
{
    T102xFMANState *s = T102X_FMAN(opaque);
    RegDef32 reg = regdef_find(t102x_fman_regs, offset);

    if (reg.index < 0) {
        ERR(FMAN, "Bad write offset %#" HWADDR_PRIx, offset);
        return;
    }

    DBG(FMAN, "Write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    if (!!(value & ~reg.write_mask)) {
        ERR(FMAN, "Maybe write to a read only bit %#" PRIx64,
            (value & ~reg.write_mask));
    }

    switch (reg.index) {
    default:
        s->regs[reg.index] = value;
        break;
    }
}

static void t102x_fman_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = t102x_fman_read,
        .write      = t102x_fman_write,
        .endianness = DEVICE_BIG_ENDIAN,
    };

    T102xFMANState *s = T102X_FMAN(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_T102X_FMAN, FMAN_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void t102x_fman_reset(DeviceState *dev)
{
    T102xFMANState *s = T102X_FMAN(dev);

    for (int i = 0; i < ARRAY_SIZE(s->regs); ++i) {
        s->regs[i] = t102x_fman_regs[i].reset_value;
    }
}

static void t102x_fman_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = t102x_fman_realize;
    dc->reset   = t102x_fman_reset;
}

static void t102x_dpaa_register_types(void)
{
    static const TypeInfo info[] = {
        {
            .name = TYPE_T102X_SEC,
            .parent = TYPE_SYS_BUS_DEVICE,
            .instance_size = sizeof(T102xSECState),
            .class_init = t102x_sec_class_init,
        },
        {
            .name = TYPE_T102X_QMAN,
            .parent = TYPE_SYS_BUS_DEVICE,
            .instance_size = sizeof(T102xQMANState),
            .class_init = t102x_qman_class_init,
        },
        {
            .name = TYPE_T102X_BMAN,
            .parent = TYPE_SYS_BUS_DEVICE,
            .instance_size = sizeof(T102xBMANState),
            .class_init = t102x_bman_class_init,
        },
        {
            .name = TYPE_T102X_FMAN,
            .parent = TYPE_SYS_BUS_DEVICE,
            .instance_size = sizeof(T102xFMANState),
            .class_init = t102x_fman_class_init,
        },
    };

    for (int i = 0; i < ARRAY_SIZE(info); ++i) {
        type_register_static(&info[i]);
    }
}

type_init(t102x_dpaa_register_types)
