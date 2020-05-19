#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "hw/misc/gen-reg.h"
#include "qemu/log.h"

#define ENABLE_DEBUG
#define DEBUG_LCC 0
#define DEBUG_LAW 0
#define DEBUG_DDR 1
#define DEBUG_CPC 1
#define DEBUG_CLKING 1
#define DEBUG_DCFG 1
#define DEBUG_RCPM 1
#define DEBUG_USB_PHY 0
#define DEBUG_PEX 0
#define DEBUG_SEC 0
#define DEBUG_QMAN 0
#define DEBUG_BMAN 0
#define DEBUG_FMAN 0

enum T102xCCSRMemorySize {
    LCC_MMIO_SIZE = 0x100,
    LAW_MMIO_SIZE = 0x100,
    DDR_MMIO_SIZE = 0x1000,
    CPC_MMIO_SIZE = 0x1000,
    CLKING_MMIO_SIZE = 0x1000,
    DCFG_MMIO_SIZE = 0x1000,
    RCPM_MMIO_SIZE = 0x1000,
    USB_PHY_MMIO_SIZE = 0x1000,
    PEX_MMIO_SIZE = 0x1000,
    SEC_MMIO_SIZE = 0x66000,
    QMAN_MMIO_SIZE = 0x2000,
    BMAN_MMIO_SIZE = 0x1000,
    FMAN_MMIO_SIZE = 0x100000,
};

#define TYPE_T102x_LCC "t102x-lcc"
#define T102x_LCC(obj) OBJECT_CHECK(T102xLCCState, (obj), TYPE_T102x_LCC)
#define TYPE_T102x_LAW "t102x-law"
#define T102x_LAW(obj) OBJECT_CHECK(T102xLAWState, (obj), TYPE_T102x_LAW)
#define TYPE_T102x_DDR "t102x-ddr"
#define T102x_DDR(obj) OBJECT_CHECK(T102xDDRState, (obj), TYPE_T102x_DDR)
#define TYPE_T102x_CPC "t102x-cpc"
#define T102x_CPC(obj) OBJECT_CHECK(T102xCPCState, (obj), TYPE_T102x_CPC)
#define TYPE_T102x_CLKING "t102x-clking"
#define T102x_CLKING(obj) OBJECT_CHECK(T102xCLKingState, (obj), TYPE_T102x_CLKING)
#define TYPE_T102x_DCFG "t102x-dcfg"
#define T102x_DCFG(obj) OBJECT_CHECK(T102xDCFGState, (obj), TYPE_T102x_DCFG)
#define TYPE_T102x_RCPM "t102x-rcpm"
#define T102x_RCPM(obj) OBJECT_CHECK(T102xRCPMState, (obj), TYPE_T102x_RCPM)
#define TYPE_T102x_USB_PHY "t102x-usb-phy"
#define T102x_USB_PHY(obj) OBJECT_CHECK(T102xUSBPHYState, (obj), TYPE_T102x_USB_PHY)
#define TYPE_T102x_PEX "t102x-pex"
#define T102x_PEX(obj) OBJECT_CHECK(T102xPEXState, (obj), TYPE_T102x_PEX)
#define TYPE_T102x_SEC "t102x-sec"
#define T102x_SEC(obj) OBJECT_CHECK(T102xSECState, (obj), TYPE_T102x_SEC)
#define TYPE_T102x_QMAN "t102x-qman"
#define T102x_QMAN(obj) OBJECT_CHECK(T102xQMANState, (obj), TYPE_T102x_QMAN)
#define TYPE_T102x_BMAN "t102x-bman"
#define T102x_BMAN(obj) OBJECT_CHECK(T102xBMANState, (obj), TYPE_T102x_BMAN)
#define TYPE_T102x_FMAN "t102x-fman"
#define T102x_FMAN(obj) OBJECT_CHECK(T102xFMANState, (obj), TYPE_T102x_FMAN)

#if defined(ENABLE_DEBUG)
#define DBG(type, format, ...)                          \
    do {                                                \
        if (DEBUG_##type) {                             \
            qemu_log("%s: " format "\n",                \
                     TYPE_T102x_##type, ##__VA_ARGS__); \
        }                                               \
    } while (0)
#else
#define DBG(type, format, ...) do {} while (0)
#endif
#define ERR(type, format, ...)                              \
    do {                                                    \
        qemu_log_mask(LOG_GUEST_ERROR, "%s: " format "\n",  \
                      TYPE_T102x_##type, ##__VA_ARGS__);    \
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
    LCC_CCSRBARH,
    LCC_CCSRBARL,
    LCC_CCSRAR,
    LCC_ALTCBARH,
    LCC_ALTCBARL,
    LCC_ALTCAR,
    LCC_BSTRH,
    LCC_BSTRL,
    LCC_BSTAR,
};

static const RegDef32 t102x_lcc_regs[] = {
    REG_ITEM(LCC_CCSRBARH,  0x0000, 0x00000000, 0x0000000F),
    REG_ITEM(LCC_CCSRBARL,  0x0004, 0xFE000000, 0xFFFFFFFF),
    REG_ITEM(LCC_CCSRAR,    0x0008, 0x00000000, 0x80000000),
    REG_ITEM(LCC_ALTCBARH,  0x0010, 0x00000000, 0x0000000F),
    REG_ITEM(LCC_ALTCBARL,  0x0014, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(LCC_ALTCAR,    0x0018, 0x00000000, 0x9FF00000),
    REG_ITEM(LCC_BSTRH,     0x0020, 0x00000000, 0x0000000F),
    REG_ITEM(LCC_BSTRL,     0x0024, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(LCC_BSTAR,     0x0028, 0x01F0000B, 0x9FF0003F),
};

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[ARRAY_SIZE(t102x_lcc_regs)];
} T102xLCCState;

static uint64_t t102x_lcc_read(void *opaque, hwaddr offset, unsigned size)
{
    T102xLCCState *s = T102x_LCC(opaque);
    RegDef32 reg = regdef_find(t102x_lcc_regs, offset);

    if (reg.index < 0) {
        ERR(LCC, "Bad read offset %#" HWADDR_PRIx, offset);
        return 0;
    }

    uint64_t value = s->regs[reg.index];

    DBG(LCC, "Read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    return value;
}

static void t102x_lcc_write(void *opaque, hwaddr offset, uint64_t value,
                            unsigned size)
{
    T102xLCCState *s = T102x_LCC(opaque);
    RegDef32 reg = regdef_find(t102x_lcc_regs, offset);

    if (reg.index < 0) {
        ERR(LCC, "Bad write offset %#" HWADDR_PRIx, offset);
        return;
    }

    DBG(LCC, "Write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    if (!!(value & ~reg.write_mask)) {
        ERR(LCC, "Maybe write to a read only bit %#" PRIx64,
            (value & ~reg.write_mask));
    }

    switch (reg.index) {
    default:
        s->regs[reg.index] = value;
        break;
    }
}

static void t102x_lcc_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = t102x_lcc_read,
        .write      = t102x_lcc_write,
        .endianness = DEVICE_BIG_ENDIAN,
    };

    T102xLCCState *s = T102x_LCC(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_T102x_LCC, LCC_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void t102x_lcc_reset(DeviceState *dev)
{
    T102xLCCState *s = T102x_LCC(dev);

    for (int i = 0; i < ARRAY_SIZE(s->regs); ++i) {
        s->regs[i] = t102x_lcc_regs[i].reset_value;
    }
}

static void t102x_lcc_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = t102x_lcc_realize;
    dc->reset   = t102x_lcc_reset;
}

enum {
    LAW_LAWBARH0,
    LAW_LAWBARL0,
    LAW_LAWAR0,
    LAW_LAWBARH1,
    LAW_LAWBARL1,
    LAW_LAWAR1,
    LAW_LAWBARH2,
    LAW_LAWBARL2,
    LAW_LAWAR2,
    LAW_LAWBARH3,
    LAW_LAWBARL3,
    LAW_LAWAR3,
    LAW_LAWBARH4,
    LAW_LAWBARL4,
    LAW_LAWAR4,
    LAW_LAWBARH5,
    LAW_LAWBARL5,
    LAW_LAWAR5,
    LAW_LAWBARH6,
    LAW_LAWBARL6,
    LAW_LAWAR6,
    LAW_LAWBARH7,
    LAW_LAWBARL7,
    LAW_LAWAR7,
    LAW_LAWBARH8,
    LAW_LAWBARL8,
    LAW_LAWAR8,
    LAW_LAWBARH9,
    LAW_LAWBARL9,
    LAW_LAWAR9,
    LAW_LAWBARH10,
    LAW_LAWBARL10,
    LAW_LAWAR10,
    LAW_LAWBARH11,
    LAW_LAWBARL11,
    LAW_LAWAR11,
    LAW_LAWBARH12,
    LAW_LAWBARL12,
    LAW_LAWAR12,
    LAW_LAWBARH13,
    LAW_LAWBARL13,
    LAW_LAWAR13,
    LAW_LAWBARH14,
    LAW_LAWBARL14,
    LAW_LAWAR14,
    LAW_LAWBARH15,
    LAW_LAWBARL15,
    LAW_LAWAR15,
    LAW_LAWBARH16,
    LAW_LAWBARL16,
    LAW_LAWAR16,
};

static const RegDef32 t102x_law_regs[] = {
    REG_ITEM(LAW_LAWBARH0,  0x0000, 0x00000000, 0x0000000F),
    REG_ITEM(LAW_LAWBARL0,  0x0004, 0x00000000, 0xFFFFF000),
    REG_ITEM(LAW_LAWAR0,    0x0008, 0x00000000, 0x8FF0003F),
    REG_ITEM(LAW_LAWBARH1,  0x0010, 0x00000000, 0x0000000F),
    REG_ITEM(LAW_LAWBARL1,  0x0014, 0x00000000, 0xFFFFF000),
    REG_ITEM(LAW_LAWAR1,    0x0018, 0x00000000, 0x8FF0003F),
    REG_ITEM(LAW_LAWBARH2,  0x0020, 0x00000000, 0x0000000F),
    REG_ITEM(LAW_LAWBARL2,  0x0024, 0x00000000, 0xFFFFF000),
    REG_ITEM(LAW_LAWAR2,    0x0028, 0x00000000, 0x8FF0003F),
    REG_ITEM(LAW_LAWBARH3,  0x0030, 0x00000000, 0x0000000F),
    REG_ITEM(LAW_LAWBARL3,  0x0034, 0x00000000, 0xFFFFF000),
    REG_ITEM(LAW_LAWAR3,    0x0038, 0x00000000, 0x8FF0003F),
    REG_ITEM(LAW_LAWBARH4,  0x0040, 0x00000000, 0x0000000F),
    REG_ITEM(LAW_LAWBARL4,  0x0044, 0x00000000, 0xFFFFF000),
    REG_ITEM(LAW_LAWAR4,    0x0048, 0x00000000, 0x8FF0003F),
    REG_ITEM(LAW_LAWBARH5,  0x0050, 0x00000000, 0x0000000F),
    REG_ITEM(LAW_LAWBARL5,  0x0054, 0x00000000, 0xFFFFF000),
    REG_ITEM(LAW_LAWAR5,    0x0058, 0x00000000, 0x8FF0003F),
    REG_ITEM(LAW_LAWBARH6,  0x0060, 0x00000000, 0x0000000F),
    REG_ITEM(LAW_LAWBARL6,  0x0064, 0x00000000, 0xFFFFF000),
    REG_ITEM(LAW_LAWAR6,    0x0068, 0x00000000, 0x8FF0003F),
    REG_ITEM(LAW_LAWBARH7,  0x0070, 0x00000000, 0x0000000F),
    REG_ITEM(LAW_LAWBARL7,  0x0074, 0x00000000, 0xFFFFF000),
    REG_ITEM(LAW_LAWAR7,    0x0078, 0x00000000, 0x8FF0003F),
    REG_ITEM(LAW_LAWBARH8,  0x0080, 0x00000000, 0x0000000F),
    REG_ITEM(LAW_LAWBARL8,  0x0084, 0x00000000, 0xFFFFF000),
    REG_ITEM(LAW_LAWAR8,    0x0088, 0x00000000, 0x8FF0003F),
    REG_ITEM(LAW_LAWBARH9,  0x0090, 0x00000000, 0x0000000F),
    REG_ITEM(LAW_LAWBARL9,  0x0094, 0x00000000, 0xFFFFF000),
    REG_ITEM(LAW_LAWAR9,    0x0098, 0x00000000, 0x8FF0003F),
    REG_ITEM(LAW_LAWBARH10, 0x00A0, 0x00000000, 0x0000000F),
    REG_ITEM(LAW_LAWBARL10, 0x00A4, 0x00000000, 0xFFFFF000),
    REG_ITEM(LAW_LAWAR10,   0x00A8, 0x00000000, 0x8FF0003F),
    REG_ITEM(LAW_LAWBARH11, 0x00B0, 0x00000000, 0x0000000F),
    REG_ITEM(LAW_LAWBARL11, 0x00B4, 0x00000000, 0xFFFFF000),
    REG_ITEM(LAW_LAWAR11,   0x00B8, 0x00000000, 0x8FF0003F),
    REG_ITEM(LAW_LAWBARH12, 0x00C0, 0x00000000, 0x0000000F),
    REG_ITEM(LAW_LAWBARL12, 0x00C4, 0x00000000, 0xFFFFF000),
    REG_ITEM(LAW_LAWAR12,   0x00C8, 0x00000000, 0x8FF0003F),
    REG_ITEM(LAW_LAWBARH13, 0x00D0, 0x00000000, 0x0000000F),
    REG_ITEM(LAW_LAWBARL13, 0x00D4, 0x00000000, 0xFFFFF000),
    REG_ITEM(LAW_LAWAR13,   0x00D8, 0x00000000, 0x8FF0003F),
    REG_ITEM(LAW_LAWBARH14, 0x00E0, 0x00000000, 0x0000000F),
    REG_ITEM(LAW_LAWBARL14, 0x00E4, 0x00000000, 0xFFFFF000),
    REG_ITEM(LAW_LAWAR14,   0x00E8, 0x00000000, 0x8FF0003F),
    REG_ITEM(LAW_LAWBARH15, 0x00F0, 0x00000000, 0x0000000F),
    REG_ITEM(LAW_LAWBARL15, 0x00F4, 0x00000000, 0xFFFFF000),
    REG_ITEM(LAW_LAWAR15,   0x00F8, 0x00000000, 0x8FF0003F),
};

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[ARRAY_SIZE(t102x_law_regs)];
} T102xLAWState;

static uint64_t t102x_law_read_value(T102xLAWState *s, const RegDef32 *reg)
{
    return s->regs[reg->index];
}

static uint64_t t102x_law_read(void *opaque, hwaddr offset, unsigned size)
{
    T102xLAWState *s = T102x_LAW(opaque);
    RegDef32 reg = regdef_find(t102x_law_regs, offset);

    if (reg.index < 0) {
        ERR(LAW, "Bad read offset %#" HWADDR_PRIx, offset);
        return 0;
    }

    uint64_t value = t102x_law_read_value(s, &reg);
    DBG(LAW, "Read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);

    return value;
}

static void t102x_law_write_value(T102xLAWState *s, const RegDef32 *reg,
                                  uint32_t value)
{
    switch (reg->index) {
    default:
        s->regs[reg->index] = value;
        break;
    }
}

static void t102x_law_write(void *opaque, hwaddr offset, uint64_t value,
                            unsigned size)
{
    T102xLAWState *s = T102x_LAW(opaque);
    RegDef32 reg = regdef_find(t102x_law_regs, offset);

    if (reg.index < 0) {
        ERR(LCC, "Bad write offset %#" HWADDR_PRIx, offset);
        return;
    }

    DBG(LAW, "Write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    if (!!(value & ~reg.write_mask)) {
        ERR(LAW, "Maybe write to a read only bit %#" PRIx64,
            (value & ~reg.write_mask));
    }

    t102x_law_write_value(s, &reg, (uint32_t)value);
}

static void t102x_law_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = t102x_law_read,
        .write      = t102x_law_write,
        .endianness = DEVICE_BIG_ENDIAN,
    };

    T102xLAWState *s = T102x_LAW(dev);


    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_T102x_LAW, LAW_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void t102x_law_reset(DeviceState *dev)
{
    T102xLAWState *s = T102x_LAW(dev);

    for (int i = 0; i < ARRAY_SIZE(s->regs); ++i) {
        s->regs[i] = t102x_law_regs[i].reset_value;
    }
}

static void t102x_law_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = t102x_law_realize;
    dc->reset   = t102x_law_reset;
}

enum {
    DDR_CS0_BNDS,
    DDR_CS1_BNDS,
    DDR_CS2_BNDS,
    DDR_CS3_BNDS,
    DDR_CS0_CONFIG,
    DDR_CS1_CONFIG,
    DDR_CS2_CONFIG,
    DDR_CS3_CONFIG,
    DDR_CS0_CONFIG_2,
    DDR_CS1_CONFIG_2,
    DDR_CS2_CONFIG_2,
    DDR_CS3_CONFIG_2,
    DDR_TIMING_CFG_3,
    DDR_TIMING_CFG_0,
    DDR_TIMING_CFG_1,
    DDR_TIMING_CFG_2,
    DDR_DDR_SDRAM_CFG,
    DDR_DDR_SDRAM_CFG_2,
    DDR_DDR_SDRAM_MODE,
    DDR_DDR_SDRAM_MODE_2,
    DDR_DDR_SDRAM_MD_CNTL,
    DDR_DDR_SDRAM_INTERVAL,
    DDR_DDR_DATA_INIT,
    DDR_DDR_SDRAM_CLK_CNTL,
    DDR_DDR_INIT_ADDR,
    DDR_DDR_INIT_EXT_ADDRESS,
    DDR_TIMING_CFG_4,
    DDR_TIMING_CFG_5,
    DDR_TIMING_CFG_6,
    DDR_TIMING_CFG_7,
    DDR_DDR_ZQ_CNTL,
    DDR_DDR_WRLVL_CNTL,
    DDR_DDR_SR_CNTR,
    DDR_DDR_SDRAM_RCW_1,
    DDR_DDR_SDRAM_RCW_2,
    DDR_DDR_WRLVL_CNTL_2,
    DDR_DDR_WRLVL_CNTL_3,
    DDR_DDR_SDRAM_RCW_3,
    DDR_DDR_SDRAM_RCW_4,
    DDR_DDR_SDRAM_RCW_5,
    DDR_DDR_SDRAM_RCW_6,
    DDR_DDR_SDRAM_MODE_3,
    DDR_DDR_SDRAM_MODE_4,
    DDR_DDR_SDRAM_MODE_5,
    DDR_DDR_SDRAM_MODE_6,
    DDR_DDR_SDRAM_MODE_7,
    DDR_DDR_SDRAM_MODE_8,
    DDR_DDRCDR_1,
    DDR_DDRCDR_2,
    DDR_DDR_IP_REV1,
    DDR_DDR_IP_REV2,
    DDR_ERR_DISABLE,
    DDR_ERR_INT_EN,
};

static const RegDef32 t102x_ddr_regs[] = {
    REG_ITEM(DDR_CS0_BNDS,              0x0000, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DDR_CS1_BNDS,              0x0008, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DDR_CS2_BNDS,              0x0010, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DDR_CS3_BNDS,              0x0018, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DDR_CS0_CONFIG,            0x0080, 0x00000000, 0x80F7C737),
    REG_ITEM(DDR_CS1_CONFIG,            0x0084, 0x00000000, 0x80F7C737),
    REG_ITEM(DDR_CS2_CONFIG,            0x0088, 0x00000000, 0x80F7C737),
    REG_ITEM(DDR_CS3_CONFIG,            0x008C, 0x00000000, 0x80F7C737),
    REG_ITEM(DDR_CS0_CONFIG_2,          0x00C0, 0x00000000, 0x83000000),
    REG_ITEM(DDR_CS1_CONFIG_2,          0x00C4, 0x00000000, 0x83000000),
    REG_ITEM(DDR_CS2_CONFIG_2,          0x00C8, 0x00000000, 0x83000000),
    REG_ITEM(DDR_CS3_CONFIG_2,          0x00CC, 0x00000000, 0x83000000),
    REG_ITEM(DDR_TIMING_CFG_3,          0x0100, 0x00000000, 0x137F3507),
    REG_ITEM(DDR_TIMING_CFG_0,          0x0104, 0x00110005, 0xFFFFC01F),
    REG_ITEM(DDR_TIMING_CFG_1,          0x0108, 0x00000000, 0xFFFEFFFF),
    REG_ITEM(DDR_TIMING_CFG_2,          0x010C, 0x00000000, 0xF07DFFFF),
    REG_ITEM(DDR_DDR_SDRAM_CFG,         0x0110, 0x87000000, 0xF73DFF0F),
    REG_ITEM(DDR_DDR_SDRAM_CFG_2,       0x0114, 0x00000000, 0xC060FB77),
    REG_ITEM(DDR_DDR_SDRAM_MODE,        0x0118, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DDR_DDR_SDRAM_MODE_2,      0x011C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DDR_DDR_SDRAM_MD_CNTL,     0x0120, 0x00000000, 0xFFFBFFFF),
    REG_ITEM(DDR_DDR_SDRAM_INTERVAL,    0x0124, 0x00000000, 0xFFFF3FFF),
    REG_ITEM(DDR_DDR_DATA_INIT,         0x0128, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DDR_DDR_SDRAM_CLK_CNTL,    0x0130, 0x02000000, 0x07C00000),
    REG_ITEM(DDR_DDR_INIT_ADDR,         0x0148, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DDR_DDR_INIT_EXT_ADDRESS,  0x014C, 0x00000000, 0x800000FF),
    REG_ITEM(DDR_TIMING_CFG_4,          0x0160, 0x00000000, 0xFFFFD513),
    REG_ITEM(DDR_TIMING_CFG_5,          0x0164, 0x00000000, 0x1F71F700),
    REG_ITEM(DDR_TIMING_CFG_6,          0x0168, 0x00000000, 0x1FF9F000),
    REG_ITEM(DDR_TIMING_CFG_7,          0x016C, 0x00000000, 0x3FFF00F0),
    REG_ITEM(DDR_DDR_ZQ_CNTL,           0x0170, 0x00000000, 0x8F0F0F0F),
    REG_ITEM(DDR_DDR_WRLVL_CNTL,        0x0174, 0x00000000, 0x8777F71F),
    REG_ITEM(DDR_DDR_SR_CNTR,           0x017C, 0x00000000, 0x000F0000),
    REG_ITEM(DDR_DDR_SDRAM_RCW_1,       0x0180, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DDR_DDR_SDRAM_RCW_2,       0x0184, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DDR_DDR_WRLVL_CNTL_2,      0x0190, 0x00000000, 0x1F1F1F1F),
    REG_ITEM(DDR_DDR_WRLVL_CNTL_3,      0x0194, 0x00000000, 0x1F1F1F1F),
    REG_ITEM(DDR_DDR_SDRAM_RCW_3,       0x01A0, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DDR_DDR_SDRAM_RCW_4,       0x01A4, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DDR_DDR_SDRAM_RCW_5,       0x01A8, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DDR_DDR_SDRAM_RCW_6,       0x01AC, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DDR_DDR_SDRAM_MODE_3,      0x0200, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DDR_DDR_SDRAM_MODE_4,      0x0204, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DDR_DDR_SDRAM_MODE_5,      0x0208, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DDR_DDR_SDRAM_MODE_6,      0x020C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DDR_DDR_SDRAM_MODE_7,      0x0210, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DDR_DDR_SDRAM_MODE_8,      0x0214, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DDR_DDRCDR_1,              0x0B28, 0x00008080, 0x800F8080),
    REG_ITEM(DDR_DDRCDR_2,              0x0B2C, 0x08000000, 0x8800DFC1),
    REG_ITEM(DDR_DDR_IP_REV1,           0x0BF8, 0x00020500, 0x00000000),
    REG_ITEM(DDR_DDR_IP_REV2,           0x0BFC, 0x00000000, 0x00000000),
    REG_ITEM(DDR_ERR_DISABLE,           0x0E44, 0x00000000, 0x0000119D),
    REG_ITEM(DDR_ERR_INT_EN,            0x0E48, 0x00000000, 0x0000119D),
};

enum {
    DDR_DDR_SDRAM_CFG_2__D_INIT_BIT = 27,
    DDR_DDR_SDRAM_CFG_2__D_INIT_MASK = 0x00000010,
};

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[ARRAY_SIZE(t102x_ddr_regs)];
} T102xDDRState;

static uint64_t t102x_ddr_read(void *opaque, hwaddr offset, unsigned size)
{
    T102xDDRState *s = T102x_DDR(opaque);
    RegDef32 reg = regdef_find(t102x_ddr_regs, offset);

    if (reg.index < 0) {
        ERR(DDR, "Bad read offset %#" HWADDR_PRIx, offset);
        return 0;
    }

    uint64_t value = s->regs[reg.index];

    DBG(DDR, "Read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    return value;
}

static void t102x_ddr_write(void *opaque, hwaddr offset, uint64_t value,
                            unsigned size)
{
    T102xDDRState *s = T102x_DDR(opaque);
    RegDef32 reg = regdef_find(t102x_ddr_regs, offset);

    if (reg.index < 0) {
        ERR(DDR, "Bad write offset %#" HWADDR_PRIx, offset);
        return;
    }

    DBG(DDR, "Write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    if (!!(value & ~reg.write_mask)) {
        ERR(DDR, "Maybe write to a read only bit %#" PRIx64,
            (value & ~reg.write_mask));
    }

    switch (reg.index) {
    case DDR_DDR_SDRAM_CFG_2:
        if (GET_FIELD(DDR_DDR_SDRAM_CFG_2, D_INIT, value) == 1) {
            DBG(DDR, "DRAM data initialization, and cleared");
            CLEAR_FIELD(DDR_DDR_SDRAM_CFG_2, D_INIT, value);
        }
        s->regs[reg.index] = value;
        break;
    default:
        s->regs[reg.index] = value;
        break;
    }
}

static void t102x_ddr_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = t102x_ddr_read,
        .write      = t102x_ddr_write,
        .endianness = DEVICE_BIG_ENDIAN,
    };

    T102xDDRState *s = T102x_DDR(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_T102x_DDR, DDR_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void t102x_ddr_reset(DeviceState *dev)
{
    T102xDDRState *s = T102x_DDR(dev);

    for (int i = 0; i < ARRAY_SIZE(s->regs); ++i) {
        s->regs[i] = t102x_ddr_regs[i].reset_value;
    }
}

static void t102x_ddr_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = t102x_ddr_realize;
    dc->reset   = t102x_ddr_reset;
}

enum {
    CPC_CPCCSR0,
    CPC_CPCCFG0,
    CPC_CPCEWCR0,
    CPC_CPCEWBAR0,
    CPC_CPCEWCR1,
    CPC_CPCEWBAR1,
    CPC_CPCSRCR1,
    CPC_CPCSRCR0,
    CPC_CPCERRINJHI,
    CPC_CPCERRINJLO,
    CPC_CPCERRINJCTL,
    CPC_CPCCAPDATAHI,
    CPC_CPCCAPDATALO,
    CPC_CPCCAPTECC,
    CPC_CPCERRDET,
    CPC_CPCERRDIS,
    CPC_CPCERRINTEN,
    CPC_CPCERREADDR,
    CPC_CPCERRADDR,
    CPC_CPCERRCTL,
    CPC_CPCHDBCR0,
};

static const RegDef32 t102x_cpc_regs[] = {
    REG_ITEM(CPC_CPCCSR0,       0x0000, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(CPC_CPCCFG0,       0x0008, 0x50B1C004, 0x00000000),
    REG_ITEM(CPC_CPCEWCR0,      0x0010, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(CPC_CPCEWBAR0,     0x0014, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(CPC_CPCEWCR1,      0x0020, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(CPC_CPCEWBAR1,     0x0024, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(CPC_CPCSRCR1,      0x0100, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(CPC_CPCSRCR0,      0x0104, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(CPC_CPCERRINJHI,   0x0E00, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(CPC_CPCERRINJLO,   0x0E04, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(CPC_CPCERRINJCTL,  0x0E08, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(CPC_CPCCAPDATAHI,  0x0E20, 0x00000000, 0x00000000),
    REG_ITEM(CPC_CPCCAPDATALO,  0x0E24, 0x00000000, 0x00000000),
    REG_ITEM(CPC_CPCCAPTECC,    0x0E28, 0x00000000, 0x00000000),
    REG_ITEM(CPC_CPCERRDET,     0x0E40, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(CPC_CPCERRDIS,     0x0E44, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(CPC_CPCERRINTEN,   0x0E48, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(CPC_CPCERREADDR,   0x0E50, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(CPC_CPCERRADDR,    0x0E54, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(CPC_CPCERRCTL,     0x0E58, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(CPC_CPCHDBCR0,     0x0F00, 0x00000000, 0xFFFFFFFF),
};

enum {
    CPC_CPCCSR0__CPCFI_BIT = 10,
    CPC_CPCCSR0__CPCFI_MASK = 0x00200000,
    CPC_CPCCSR0__CPCLFC_BIT = 21,
    CPC_CPCCSR0__CPCLFC_MASK = 0x00000400,
};

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[ARRAY_SIZE(t102x_cpc_regs)];
} T102xCPCState;

static uint64_t t102x_cpc_read(void *opaque, hwaddr offset, unsigned size)
{
    T102xCPCState *s = T102x_CPC(opaque);
    RegDef32 reg = regdef_find(t102x_cpc_regs, offset);

    if (reg.index < 0) {
        ERR(CPC, "Bad read offset %#" HWADDR_PRIx, offset);
        return 0;
    }

    uint64_t value = s->regs[reg.index];

    DBG(CPC, "Read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    return value;
}

static void t102x_cpc_write(void *opaque, hwaddr offset, uint64_t value,
                            unsigned size)
{
    T102xCPCState *s = T102x_CPC(opaque);
    RegDef32 reg = regdef_find(t102x_cpc_regs, offset);

    if (reg.index < 0) {
        ERR(CPC, "Bad write offset %#" HWADDR_PRIx, offset);
        return;
    }

    DBG(CPC, "Write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    if (!!(value & ~reg.write_mask)) {
        ERR(CPC, "Maybe write to a read only bit %#" PRIx64,
            (value & ~reg.write_mask));
    }

    switch (reg.index) {
    case CPC_CPCCSR0:
        if (GET_FIELD(CPC_CPCCSR0, CPCFI, value) == 1) {
            DBG(CPC, "Cache flash invalidate, and cleared");
            CLEAR_FIELD(CPC_CPCCSR0, CPCFI, value);
        }
        if (GET_FIELD(CPC_CPCCSR0, CPCLFC, value) == 1) {
            DBG(CPC, "Cache flash lock clear operation, and cleared");
            CLEAR_FIELD(CPC_CPCCSR0, CPCLFC, value);
        }
        s->regs[reg.index] = value;
        break;
    default:
        s->regs[reg.index] = value;
        break;
    }
}

static void t102x_cpc_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = t102x_cpc_read,
        .write      = t102x_cpc_write,
        .endianness = DEVICE_BIG_ENDIAN,
    };

    T102xCPCState *s = T102x_CPC(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_T102x_CPC, CPC_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void t102x_cpc_reset(DeviceState *dev)
{
    T102xCPCState *s = T102x_CPC(dev);

    for (int i = 0; i < ARRAY_SIZE(s->regs); ++i) {
        s->regs[i] = t102x_cpc_regs[i].reset_value;
    }
}

static void t102x_cpc_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = t102x_cpc_realize;
    dc->reset   = t102x_cpc_reset;
}

enum {
    CLKING_CLKC0CSR,
    CLKING_CLKCG1HWACSR,
    CLKING_CLKC1CSR,
    CLKING_CLKCG2HWACSR,
    CLKING_PLLC1GSR,
    CLKING_PLLC2GSR,
    CLKING_PLLC3GSR,
    CLKING_PLLC4GSR,
    CLKING_PLLC5GSR,
    CLKING_PLLC6GSR,
    CLKING_CLKPCSR,
    CLKING_PLLPGSR,
    CLKING_PLLDGSR,
};

static const RegDef32 t102x_clking_regs[] = {
    REG_ITEM(CLKING_CLKC0CSR,       0x0000, 0x00000000, 0x70000000),
    REG_ITEM(CLKING_CLKCG1HWACSR,   0x0010, 0x00000000, 0x70000000), // FIXME: Reset value
    REG_ITEM(CLKING_CLKC1CSR,       0x0020, 0x00000000, 0x70000000),
    REG_ITEM(CLKING_CLKCG2HWACSR,   0x0030, 0x00000000, 0x70000000), // FIXME: Reset value
    REG_ITEM(CLKING_PLLC1GSR,       0x0800, 0x00000018, 0x80000000),
    REG_ITEM(CLKING_PLLC2GSR,       0x0820, 0x00000000, 0x80000000),
    REG_ITEM(CLKING_PLLC3GSR,       0x0840, 0x00000000, 0x80000000),
    REG_ITEM(CLKING_PLLC4GSR,       0x0860, 0x00000000, 0x80000000),
    REG_ITEM(CLKING_PLLC5GSR,       0x0880, 0x00000000, 0x80000000),
    REG_ITEM(CLKING_PLLC6GSR,       0x08A0, 0x00000000, 0x80000000),
    REG_ITEM(CLKING_CLKPCSR,        0x0A00, 0x0000F800, 0x0001FE00), // FIXME: Reset value
    REG_ITEM(CLKING_PLLPGSR,        0x0C00, 0x00000008, 0x00000000),
    REG_ITEM(CLKING_PLLDGSR,        0x0C20, 0x00000020, 0x80000000),
};

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[ARRAY_SIZE(t102x_clking_regs)];
} T102xCLKingState;

static uint64_t t102x_clking_read(void *opaque, hwaddr offset, unsigned size)
{
    T102xCLKingState *s = T102x_CLKING(opaque);
    RegDef32 reg = regdef_find(t102x_clking_regs, offset);

    if (reg.index < 0) {
        ERR(CLKING, "Bad read offset %#" HWADDR_PRIx, offset);
        return 0;
    }

    uint64_t value = s->regs[reg.index];

    DBG(CLKING, "Read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    return value;
}

static void t102x_clking_write(void *opaque, hwaddr offset, uint64_t value,
                            unsigned size)
{
    T102xCLKingState *s = T102x_CLKING(opaque);
    RegDef32 reg = regdef_find(t102x_clking_regs, offset);

    if (reg.index < 0) {
        ERR(CLKING, "Bad write offset %#" HWADDR_PRIx, offset);
        return;
    }

    DBG(CLKING, "Write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    if (!!(value & ~reg.write_mask)) {
        ERR(CLKING, "Maybe write to a read only bit %#" PRIx64,
            (value & ~reg.write_mask));
    }

    switch (reg.index) {
    default:
        s->regs[reg.index] = value;
        break;
    }
}

static void t102x_clking_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = t102x_clking_read,
        .write      = t102x_clking_write,
        .endianness = DEVICE_BIG_ENDIAN,
    };

    T102xCLKingState *s = T102x_CLKING(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_T102x_CLKING, CLKING_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void t102x_clking_reset(DeviceState *dev)
{
    T102xCLKingState *s = T102x_CLKING(dev);

    for (int i = 0; i < ARRAY_SIZE(s->regs); ++i) {
        s->regs[i] = t102x_clking_regs[i].reset_value;
    }
}

static void t102x_clking_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = t102x_clking_realize;
    dc->reset   = t102x_clking_reset;
}

enum {
    DCFG_CCSR_PORSR1,
    DCFG_CCSR_PORSR2,
    DCFG_CCSR_DEVDISR1,
    DCFG_CCSR_DEVDISR2,
    DCFG_CCSR_DEVDISR3,
    DCFG_CCSR_DEVDISR4,
    DCFG_CCSR_DEVDISR5,
    DCFG_CCSR_BRR,
    DCFG_CCSR_RCWSR1,
    DCFG_CCSR_RCWSR2,
    DCFG_CCSR_RCWSR3,
    DCFG_CCSR_RCWSR4,
    DCFG_CCSR_RCWSR5,
    DCFG_CCSR_RCWSR6,
    DCFG_CCSR_RCWSR7,
    DCFG_CCSR_RCWSR8,
    DCFG_CCSR_RCWSR9,
    DCFG_CCSR_RCWSR10,
    DCFG_CCSR_RCWSR11,
    DCFG_CCSR_RCWSR12,
    DCFG_CCSR_RCWSR13,
    DCFG_CCSR_RCWSR14,
    DCFG_CCSR_RCWSR15,
    DCFG_CCSR_RCWSR16,
    DCFG_CCSR_CRSTSR0,
    DCFG_CCSR_CRSTSR1,
    DCFG_CCSR_USB1LIODNR,
    DCFG_CCSR_USB2LIODNR,
    DCFG_CCSR_SDMMCLIODNR,
    DCFG_CCSR_SATALIODNR,
    DCFG_CCSR_DIULIODNR,
    DCFG_CCSR_TDMDMALIODNR,
    DCFG_CCSR_QELIODNR,
    DCFG_CCSR_DMA1LIODNR,
    DCFG_CCSR_DMA2LIODNR,
    DCFG_CCSR_TP_ITYP0,
    DCFG_CCSR_TP_ITYP1,
    DCFG_CCSR_TP_ITYP2,
    DCFG_CCSR_TP_ITYP3,
    DCFG_CCSR_TP_ITYP4,
    DCFG_CCSR_TP_ITYP5,
    DCFG_CCSR_TP_ITYP6,
    DCFG_CCSR_TP_ITYP7,
    DCFG_CCSR_TP_ITYP8,
    DCFG_CCSR_TP_ITYP9,
    DCFG_CCSR_TP_ITYP10,
    DCFG_CCSR_TP_ITYP11,
    DCFG_CCSR_TP_ITYP12,
    DCFG_CCSR_TP_ITYP13,
    DCFG_CCSR_TP_ITYP14,
    DCFG_CCSR_TP_ITYP15,
    DCFG_CCSR_TP_ITYP16,
    DCFG_CCSR_TP_ITYP17,
    DCFG_CCSR_TP_ITYP18,
    DCFG_CCSR_TP_ITYP19,
    DCFG_CCSR_TP_ITYP20,
    DCFG_CCSR_TP_ITYP21,
    DCFG_CCSR_TP_ITYP22,
    DCFG_CCSR_TP_ITYP23,
    DCFG_CCSR_TP_ITYP24,
    DCFG_CCSR_TP_ITYP25,
    DCFG_CCSR_TP_ITYP26,
    DCFG_CCSR_TP_ITYP27,
    DCFG_CCSR_TP_ITYP28,
    DCFG_CCSR_TP_ITYP29,
    DCFG_CCSR_TP_ITYP30,
    DCFG_CCSR_TP_ITYP31,
    DCFG_CCSR_TP_ITYP32,
    DCFG_CCSR_TP_ITYP33,
    DCFG_CCSR_TP_ITYP34,
    DCFG_CCSR_TP_ITYP35,
    DCFG_CCSR_TP_ITYP36,
    DCFG_CCSR_TP_ITYP37,
    DCFG_CCSR_TP_ITYP38,
    DCFG_CCSR_TP_ITYP39,
    DCFG_CCSR_TP_ITYP40,
    DCFG_CCSR_TP_ITYP41,
    DCFG_CCSR_TP_ITYP42,
    DCFG_CCSR_TP_ITYP43,
    DCFG_CCSR_TP_ITYP44,
    DCFG_CCSR_TP_ITYP45,
    DCFG_CCSR_TP_ITYP46,
    DCFG_CCSR_TP_ITYP47,
    DCFG_CCSR_TP_ITYP48,
    DCFG_CCSR_TP_ITYP49,
    DCFG_CCSR_TP_ITYP50,
    DCFG_CCSR_TP_ITYP51,
    DCFG_CCSR_TP_ITYP52,
    DCFG_CCSR_TP_ITYP53,
    DCFG_CCSR_TP_ITYP54,
    DCFG_CCSR_TP_ITYP55,
    DCFG_CCSR_TP_ITYP56,
    DCFG_CCSR_TP_ITYP57,
    DCFG_CCSR_TP_ITYP58,
    DCFG_CCSR_TP_ITYP59,
    DCFG_CCSR_TP_ITYP60,
    DCFG_CCSR_TP_ITYP61,
    DCFG_CCSR_TP_ITYP62,
    DCFG_CCSR_TP_ITYP63,
    DCFG_CCSR_TP_CLUSTER1,
};

static const RegDef32 t102x_dcfg_regs[] = {
    REG_ITEM(DCFG_CCSR_PORSR1,          0x0000, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_PORSR2,          0x0004, 0x20000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_DEVDISR1,        0x0070, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DCFG_CCSR_DEVDISR2,        0x0074, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DCFG_CCSR_DEVDISR3,        0x0078, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DCFG_CCSR_DEVDISR4,        0x007C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DCFG_CCSR_DEVDISR5,        0x0080, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DCFG_CCSR_BRR,             0x00E4, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DCFG_CCSR_RCWSR1,          0x0100, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_RCWSR2,          0x0104, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_RCWSR3,          0x0108, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_RCWSR4,          0x010C, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_RCWSR5,          0x0110, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_RCWSR6,          0x0114, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_RCWSR7,          0x0118, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_RCWSR8,          0x011C, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_RCWSR9,          0x0120, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_RCWSR10,         0x0124, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_RCWSR11,         0x0128, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_RCWSR12,         0x012C, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_RCWSR13,         0x0130, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_RCWSR14,         0x0134, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_RCWSR15,         0x0138, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_RCWSR16,         0x013C, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_CRSTSR0,         0x0400, 0x00000004, 0xFFFFFFFF),
    REG_ITEM(DCFG_CCSR_CRSTSR1,         0x0404, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DCFG_CCSR_USB1LIODNR,      0x0520, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DCFG_CCSR_USB2LIODNR,      0x0524, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DCFG_CCSR_SDMMCLIODNR,     0x0530, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DCFG_CCSR_SATALIODNR,      0x0550, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DCFG_CCSR_DIULIODNR,       0x0570, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DCFG_CCSR_TDMDMALIODNR,    0x0574, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DCFG_CCSR_QELIODNR,        0x0578, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DCFG_CCSR_DMA1LIODNR,      0x0580, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DCFG_CCSR_DMA2LIODNR,      0x0584, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DCFG_CCSR_TP_ITYP0,        0x0740, 0x00000003, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP1,        0x0744, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP2,        0x0748, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP3,        0x074C, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP4,        0x0750, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP5,        0x0754, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP6,        0x0758, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP7,        0x075C, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP8,        0x0760, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP9,        0x0764, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP10,       0x0768, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP11,       0x076C, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP12,       0x0770, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP13,       0x0774, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP14,       0x0778, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP15,       0x077C, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP16,       0x0780, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP17,       0x0784, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP18,       0x0788, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP19,       0x078C, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP20,       0x0790, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP21,       0x0794, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP22,       0x0798, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP23,       0x079C, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP24,       0x07A0, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP25,       0x07A4, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP26,       0x07A8, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP27,       0x07AC, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP28,       0x07B0, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP29,       0x07B4, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP30,       0x07B8, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP31,       0x07BC, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP32,       0x07C0, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP33,       0x07C4, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP34,       0x07C8, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP35,       0x07CC, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP36,       0x07D0, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP37,       0x07D4, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP38,       0x07D8, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP39,       0x07DC, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP40,       0x07E0, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP41,       0x07E4, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP42,       0x07E8, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP43,       0x07EC, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP44,       0x07F0, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP45,       0x07F4, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP46,       0x07F8, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP47,       0x07FC, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP48,       0x0800, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP49,       0x0804, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP50,       0x0808, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP51,       0x080C, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP52,       0x0810, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP53,       0x0814, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP54,       0x0818, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP55,       0x081C, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP56,       0x0820, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP57,       0x0824, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP58,       0x0828, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP59,       0x082C, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP60,       0x0830, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP61,       0x0834, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP62,       0x0838, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_ITYP63,       0x083C, 0x00000000, 0x00000000),
    REG_ITEM(DCFG_CCSR_TP_CLUSTER1,     0x0844, 0xC1010100, 0x00000000),
};

enum {
    DCFG_CCSR_CRSTSRn__RST_WRT_BIT = 7,
    DCFG_CCSR_CRSTSRn__RST_WRT_MASK = 0x03000000,
    DCFG_CCSR_CRSTSRn__RST_MPIC_BIT = 15,
    DCFG_CCSR_CRSTSRn__RST_MPIC_MASK = 0x00030000,
    DCFG_CCSR_CRSTSRn__RST_CORE_BIT = 23,
    DCFG_CCSR_CRSTSRn__RST_CORE_MASK = 0x00000300,
    DCFG_CCSR_CRSTSRn__RST_HRST_BIT = 29,
    DCFG_CCSR_CRSTSRn__RST_HRST_MASK = 0x00000002,
    DCFG_CCSR_CRSTSRn__RST_PORST_BIT = 31,
    DCFG_CCSR_CRSTSRn__RST_PORST_MASK = 0x00000001,
};

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[ARRAY_SIZE(t102x_dcfg_regs)];
    void *rcw;
} T102xDCFGState;

static uint64_t t102x_dcfg_read(void *opaque, hwaddr offset, unsigned size)
{
    T102xDCFGState *s = T102x_DCFG(opaque);
    RegDef32 reg = regdef_find(t102x_dcfg_regs, offset);

    if (reg.index < 0) {
        ERR(DCFG, "Bad read offset %#" HWADDR_PRIx, offset);
        return 0;
    }

    uint64_t value;
    switch (reg.index) {
    case DCFG_CCSR_RCWSR1 ... DCFG_CCSR_RCWSR16:
        if (s->rcw != NULL) {
            value = ((uint32_t *)s->rcw)[reg.index - DCFG_CCSR_RCWSR1];
        } else {
            value = 0;
        }
        break;
    default:
        value = s->regs[reg.index];
        break;
    }

    DBG(DCFG, "Read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    return value;
}

static void t102x_dcfg_write(void *opaque, hwaddr offset, uint64_t value,
                            unsigned size)
{
    T102xDCFGState *s = T102x_DCFG(opaque);
    RegDef32 reg = regdef_find(t102x_dcfg_regs, offset);

    if (reg.index < 0) {
        ERR(DCFG, "Bad write offset %#" HWADDR_PRIx, offset);
        return;
    }

    DBG(DCFG, "Write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    if (!!(value & ~reg.write_mask)) {
        ERR(DCFG, "Maybe write to a read only bit %#" PRIx64,
            (value & ~reg.write_mask));
    }

    switch (reg.index) {
    case DCFG_CCSR_CRSTSR0:
        if (GET_FIELD(DCFG_CCSR_CRSTSRn, RST_WRT, value) == 1) {
            DBG(DCFG, "RST_WRT cleared");
            CLEAR_FIELD(DCFG_CCSR_CRSTSRn, RST_WRT, value);
        }
        if (GET_FIELD(DCFG_CCSR_CRSTSRn, RST_MPIC, value) == 1) {
            DBG(DCFG, "RST_MPIC cleared");
            CLEAR_FIELD(DCFG_CCSR_CRSTSRn, RST_MPIC, value);
        }
        if (GET_FIELD(DCFG_CCSR_CRSTSRn, RST_CORE, value) == 1) {
            DBG(DCFG, "RST_CORE cleared");
            CLEAR_FIELD(DCFG_CCSR_CRSTSRn, RST_CORE, value);
        }
        if (GET_FIELD(DCFG_CCSR_CRSTSRn, RST_HRST, value) == 1) {
            DBG(DCFG, "RST_HRST cleared");
            CLEAR_FIELD(DCFG_CCSR_CRSTSRn, RST_HRST, value);
        }
        if (GET_FIELD(DCFG_CCSR_CRSTSRn, RST_PORST, value) == 1) {
            DBG(DCFG, "RST_PORST cleared");
            CLEAR_FIELD(DCFG_CCSR_CRSTSRn, RST_PORST, value);
        }
        s->regs[reg.index] = value;
        break;
    default:
        s->regs[reg.index] = value;
        break;
    }
}

static void t102x_dcfg_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = t102x_dcfg_read,
        .write      = t102x_dcfg_write,
        .endianness = DEVICE_BIG_ENDIAN,
    };

    T102xDCFGState *s = T102x_DCFG(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_T102x_DCFG, DCFG_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void t102x_dcfg_reset(DeviceState *dev)
{
    T102xDCFGState *s = T102x_DCFG(dev);

    for (int i = 0; i < ARRAY_SIZE(s->regs); ++i) {
        s->regs[i] = t102x_dcfg_regs[i].reset_value;
    }
}

static void t102x_dcfg_class_init(ObjectClass *oc, void *data)
{
    static Property props[] = {
        DEFINE_PROP_PTR("rcw", T102xDCFGState, rcw),
        DEFINE_PROP_END_OF_LIST(),
    };

    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = t102x_dcfg_realize;
    dc->reset   = t102x_dcfg_reset;
    dc->props   = props;
}

enum {
    RCPM_TPH10SR0,
    RCPM_TPH10SETR0,
    RCPM_TPH10CLRR,
    RCPM_TPH10PSR0,
    RCPM_TWAITSR,
    RCPM_PCPH15SR,
    RCPM_PCPH15SETR,
    RCPM_PCPH15CLRR,
    RCPM_PCPH15PSR,
    RCPM_POWMGTCSR,
    RCPM_IPPDEXPCRn,
    RCPM_TPMIMR0,
    RCPM_TPMCIMR0,
    RCPM_TPMMCMR0,
    RCPM_TPMNMIMR0,
    RCPM_TMCPMASKCR0,
    RCPM_PCTBENR,
    RCPM_PCTBCKSELR,
    RCPM_TBCLKDIVR,
    RCPM_TTBHLTCR0,
};

static const RegDef32 t102x_rcpm_regs[] = {
    REG_ITEM(RCPM_TPH10SR0,     0x000C, 0x00000000, 0x00000000),
    REG_ITEM(RCPM_TPH10SETR0,   0x001C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(RCPM_TPH10CLRR,    0x002C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(RCPM_TPH10PSR0,    0x003C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(RCPM_TWAITSR,      0x004C, 0x00000000, 0x00000000),
    REG_ITEM(RCPM_PCPH15SR,     0x00B0, 0x00000000, 0x00000000),
    REG_ITEM(RCPM_PCPH15SETR,   0x00B4, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(RCPM_PCPH15CLRR,   0x00B8, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(RCPM_PCPH15PSR,    0x00BC, 0x00000000, 0x00000000),
    REG_ITEM(RCPM_POWMGTCSR,    0x0130, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(RCPM_IPPDEXPCRn,   0x0140, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(RCPM_TPMIMR0,      0x015C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(RCPM_TPMCIMR0,     0x016C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(RCPM_TPMMCMR0,     0x017C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(RCPM_TPMNMIMR0,    0x018C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(RCPM_TMCPMASKCR0,  0x019C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(RCPM_PCTBENR,      0x01A0, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(RCPM_PCTBCKSELR,   0x01A4, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(RCPM_TBCLKDIVR,    0x01A8, 0x00000000, 0x00000000),
    REG_ITEM(RCPM_TTBHLTCR0,    0x01BC, 0x00000000, 0xFFFFFFFF),
};

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[ARRAY_SIZE(t102x_rcpm_regs)];
} T102xRCPMState;

static uint64_t t102x_rcpm_read(void *opaque, hwaddr offset, unsigned size)
{
    T102xRCPMState *s = T102x_RCPM(opaque);
    RegDef32 reg = regdef_find(t102x_rcpm_regs, offset);

    if (reg.index < 0) {
        ERR(RCPM, "Bad read offset %#" HWADDR_PRIx, offset);
        return 0;
    }

    uint64_t value = s->regs[reg.index];

    DBG(RCPM, "Read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    return value;
}

static void t102x_rcpm_write(void *opaque, hwaddr offset, uint64_t value,
                            unsigned size)
{
    T102xRCPMState *s = T102x_RCPM(opaque);
    RegDef32 reg = regdef_find(t102x_rcpm_regs, offset);

    if (reg.index < 0) {
        ERR(RCPM, "Bad write offset %#" HWADDR_PRIx, offset);
        return;
    }

    DBG(RCPM, "Write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    if (!!(value & ~reg.write_mask)) {
        ERR(RCPM, "Maybe write to a read only bit %#" PRIx64,
            (value & ~reg.write_mask));
    }

    switch (reg.index) {
    default:
        s->regs[reg.index] = value;
        break;
    }
}

static void t102x_rcpm_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = t102x_rcpm_read,
        .write      = t102x_rcpm_write,
        .endianness = DEVICE_BIG_ENDIAN,
    };

    T102xRCPMState *s = T102x_RCPM(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_T102x_RCPM, RCPM_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void t102x_rcpm_reset(DeviceState *dev)
{
    T102xRCPMState *s = T102x_RCPM(dev);

    for (int i = 0; i < ARRAY_SIZE(s->regs); ++i) {
        s->regs[i] = t102x_rcpm_regs[i].reset_value;
    }
}

static void t102x_rcpm_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = t102x_rcpm_realize;
    dc->reset   = t102x_rcpm_reset;
}

enum {
    USB_PHY1_ID,
    USB_PHY1_CTRL_PHY1,
    USB_PHY1_DRVVBUSCFG_PHY1,
    USB_PHY1_PWRFLTCFG_PHY1,
    USB_PHY1_STS_PHY1,
    USB_PHY1_XCVRPRG_PHY1,
    USB_PHY1_TVR,
    USB_PHY1_PLLPRG1,
    USB_PHY1_PLLPRG2,
    USB_PHY1_CTRL_PHY2,
    USB_PHY1_DRVVBUSCFG_PHY2,
    USB_PHY1_PWRFLTCFG_PHY2,
    USB_PHY1_STS_PHY2,
    USB_PHY1_XCVRPRG_PHY2,
};

static const RegDef32 t102x_usb_phy_regs[] = {
    REG_ITEM(USB_PHY1_ID,               0x0000, 0x00000200, 0xFFFFFFFF),
    REG_ITEM(USB_PHY1_CTRL_PHY1,        0x0004, 0x00000086, 0xFFFFFFFF),
    REG_ITEM(USB_PHY1_DRVVBUSCFG_PHY1,  0x0008, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(USB_PHY1_PWRFLTCFG_PHY1,   0x000C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(USB_PHY1_STS_PHY1,         0x0010, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(USB_PHY1_XCVRPRG_PHY1,     0x0040, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(USB_PHY1_TVR,              0x005C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(USB_PHY1_PLLPRG1,          0x0060, 0x00000010, 0xFFFFFFFF),
    REG_ITEM(USB_PHY1_PLLPRG2,          0x0064, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(USB_PHY1_CTRL_PHY2,        0x0080, 0x00000086, 0xFFFFFFFF),
    REG_ITEM(USB_PHY1_DRVVBUSCFG_PHY2,  0x0084, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(USB_PHY1_PWRFLTCFG_PHY2,   0x0088, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(USB_PHY1_STS_PHY2,         0x008C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(USB_PHY1_XCVRPRG_PHY2,     0x00BC, 0x00000000, 0xFFFFFFFF),
};

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[ARRAY_SIZE(t102x_usb_phy_regs)];
} T102xUSBPHYState;

static uint64_t t102x_usb_phy_read(void *opaque, hwaddr offset, unsigned size)
{
    T102xUSBPHYState *s = T102x_USB_PHY(opaque);
    RegDef32 reg = regdef_find(t102x_usb_phy_regs, offset);

    if (reg.index < 0) {
        ERR(USB_PHY, "Bad read offset %#" HWADDR_PRIx, offset);
        return 0;
    }

    uint64_t value = s->regs[reg.index];

    DBG(USB_PHY, "Read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    return value;
}

static void t102x_usb_phy_write(void *opaque, hwaddr offset, uint64_t value,
                            unsigned size)
{
    T102xUSBPHYState *s = T102x_USB_PHY(opaque);
    RegDef32 reg = regdef_find(t102x_usb_phy_regs, offset);

    if (reg.index < 0) {
        ERR(USB_PHY, "Bad write offset %#" HWADDR_PRIx, offset);
        return;
    }

    DBG(USB_PHY, "Write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    if (!!(value & ~reg.write_mask)) {
        ERR(USB_PHY, "Maybe write to a read only bit %#" PRIx64,
            (value & ~reg.write_mask));
    }

    switch (reg.index) {
    default:
        s->regs[reg.index] = value;
        break;
    }
}

static void t102x_usb_phy_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = t102x_usb_phy_read,
        .write      = t102x_usb_phy_write,
        .endianness = DEVICE_BIG_ENDIAN,
    };

    T102xUSBPHYState *s = T102x_USB_PHY(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_T102x_USB_PHY, USB_PHY_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void t102x_usb_phy_reset(DeviceState *dev)
{
    T102xUSBPHYState *s = T102x_USB_PHY(dev);

    for (int i = 0; i < ARRAY_SIZE(s->regs); ++i) {
        s->regs[i] = t102x_usb_phy_regs[i].reset_value;
    }
}

static void t102x_usb_phy_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = t102x_usb_phy_realize;
    dc->reset   = t102x_usb_phy_reset;
}

enum {
    PEX_PEX_CONFIG_ADDR,
    PEX_PEX_CONFIG_DATA,
    PEX_PEX_LBR,
};

static const RegDef32 t102x_pex_regs[] = {
    REG_ITEM(PEX_PEX_CONFIG_ADDR,   0x0000, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(PEX_PEX_CONFIG_DATA,   0x0004, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(PEX_PEX_LBR,           0x0040, 0x00000000, 0xFFFFFFFF),
};

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[ARRAY_SIZE(t102x_pex_regs)];
} T102xPEXState;

static uint64_t t102x_pex_read(void *opaque, hwaddr offset, unsigned size)
{
    T102xPEXState *s = T102x_PEX(opaque);
    RegDef32 reg = regdef_find(t102x_pex_regs, offset);

    if (reg.index < 0) {
        ERR(PEX, "Bad read offset %#" HWADDR_PRIx, offset);
        return 0;
    }

    uint64_t value = s->regs[reg.index];

    DBG(PEX, "Read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    return value;
}

static void t102x_pex_write(void *opaque, hwaddr offset, uint64_t value,
                            unsigned size)
{
    T102xPEXState *s = T102x_PEX(opaque);
    RegDef32 reg = regdef_find(t102x_pex_regs, offset);

    if (reg.index < 0) {
        ERR(PEX, "Bad write offset %#" HWADDR_PRIx, offset);
        return;
    }

    DBG(PEX, "Write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    if (!!(value & ~reg.write_mask)) {
        ERR(PEX, "Maybe write to a read only bit %#" PRIx64,
            (value & ~reg.write_mask));
    }

    switch (reg.index) {
    default:
        s->regs[reg.index] = value;
        break;
    }
}

static void t102x_pex_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = t102x_pex_read,
        .write      = t102x_pex_write,
        .endianness = DEVICE_BIG_ENDIAN,
    };

    T102xPEXState *s = T102x_PEX(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_T102x_PEX, PEX_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void t102x_pex_reset(DeviceState *dev)
{
    T102xPEXState *s = T102x_PEX(dev);

    for (int i = 0; i < ARRAY_SIZE(s->regs); ++i) {
        s->regs[i] = t102x_pex_regs[i].reset_value;
    }
}

static void t102x_pex_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = t102x_pex_realize;
    dc->reset   = t102x_pex_reset;
}

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
    T102xSECState *s = T102x_SEC(opaque);
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
    T102xSECState *s = T102x_SEC(opaque);
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

    T102xSECState *s = T102x_SEC(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_T102x_SEC, SEC_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void t102x_sec_reset(DeviceState *dev)
{
    T102xSECState *s = T102x_SEC(dev);

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
    T102xQMANState *s = T102x_QMAN(opaque);
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
    T102xQMANState *s = T102x_QMAN(opaque);
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

    T102xQMANState *s = T102x_QMAN(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_T102x_QMAN, QMAN_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void t102x_qman_reset(DeviceState *dev)
{
    T102xQMANState *s = T102x_QMAN(dev);

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
    T102xBMANState *s = T102x_BMAN(opaque);
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
    T102xBMANState *s = T102x_BMAN(opaque);
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

    T102xBMANState *s = T102x_BMAN(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_T102x_BMAN, BMAN_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void t102x_bman_reset(DeviceState *dev)
{
    T102xBMANState *s = T102x_BMAN(dev);

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
    T102xFMANState *s = T102x_FMAN(opaque);
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
    T102xFMANState *s = T102x_FMAN(opaque);
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

    T102xFMANState *s = T102x_FMAN(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_T102x_FMAN, FMAN_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void t102x_fman_reset(DeviceState *dev)
{
    T102xFMANState *s = T102x_FMAN(dev);

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

static void t102x_ccsr_register_types(void)
{
    static const TypeInfo lcc_info = {
        .name = TYPE_T102x_LCC,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(T102xLCCState),
        .class_init = t102x_lcc_class_init,
    };
    static const TypeInfo law_info = {
        .name = TYPE_T102x_LAW,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(T102xLAWState),
        .class_init = t102x_law_class_init,
    };
    static const TypeInfo ddr_info = {
        .name = TYPE_T102x_DDR,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(T102xDDRState),
        .class_init = t102x_ddr_class_init,
    };
    static const TypeInfo cpc_info = {
        .name = TYPE_T102x_CPC,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(T102xCPCState),
        .class_init = t102x_cpc_class_init,
    };
    static const TypeInfo dcfg_info = {
        .name = TYPE_T102x_DCFG,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(T102xDCFGState),
        .class_init = t102x_dcfg_class_init,
    };
    static const TypeInfo clking_info = {
        .name = TYPE_T102x_CLKING,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(T102xCLKingState),
        .class_init = t102x_clking_class_init,
    };
    static const TypeInfo rcpm_info = {
        .name = TYPE_T102x_RCPM,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(T102xRCPMState),
        .class_init = t102x_rcpm_class_init,
    };
    static const TypeInfo usb_phy_info = {
        .name = TYPE_T102x_USB_PHY,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(T102xUSBPHYState),
        .class_init = t102x_usb_phy_class_init,
    };
    static const TypeInfo pex_info = {
        .name = TYPE_T102x_PEX,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(T102xPEXState),
        .class_init = t102x_pex_class_init,
    };
    static const TypeInfo sec_info = {
        .name = TYPE_T102x_SEC,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(T102xSECState),
        .class_init = t102x_sec_class_init,
    };
    static const TypeInfo qman_info = {
        .name = TYPE_T102x_QMAN,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(T102xQMANState),
        .class_init = t102x_qman_class_init,
    };
    static const TypeInfo bman_info = {
        .name = TYPE_T102x_BMAN,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(T102xBMANState),
        .class_init = t102x_bman_class_init,
    };
    static const TypeInfo fman_info = {
        .name = TYPE_T102x_FMAN,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(T102xFMANState),
        .class_init = t102x_fman_class_init,
    };

    type_register_static(&lcc_info);
    type_register_static(&law_info);
    type_register_static(&ddr_info);
    type_register_static(&cpc_info);
    type_register_static(&dcfg_info);
    type_register_static(&clking_info);
    type_register_static(&rcpm_info);
    type_register_static(&usb_phy_info);
    type_register_static(&pex_info);
    type_register_static(&sec_info);
    type_register_static(&qman_info);
    type_register_static(&bman_info);
    type_register_static(&fman_info);
}

type_init(t102x_ccsr_register_types)
