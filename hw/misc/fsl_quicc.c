#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "hw/misc/gen-reg.h"
#include "qemu/log.h"

#define ENABLE_DEBUG
#define DEBUG_REG 0
#define DEBUG_DEBUG 0
#define DEBUG_RAM 0

enum QUICCMemorySize {
    QUICC_MMIO_SIZE = 0x100000,
    REG_MMIO_SIZE = 0x4080,
    DEBUG_MMIO_SIZE = 0x3F80,
    RAM_MMIO_SIZE = 0xE000,
};

#define TYPE_FSL_QUICC "fsl-quicc"
#define FSL_QUICC(obj) OBJECT_CHECK(FSLQUICCState, (obj), TYPE_FSL_QUICC)
#define TYPE_QUICC_REG "quicc-reg"
#define QUICC_REG(obj) OBJECT_CHECK(QUICCRegState, (obj), TYPE_QUICC_REG)
#define TYPE_QUICC_DEBUG "quicc-debug"
#define QUICC_DEBUG(obj) OBJECT_CHECK(QUICCDebugState, (obj), TYPE_QUICC_DEBUG)
#define TYPE_QUICC_RAM "quicc-ram"
#define QUICC_RAM(obj) OBJECT_CHECK(QUICCRAMState, (obj), TYPE_QUICC_RAM)

#if defined(ENABLE_DEBUG)
#define DBG(type, format, ...)                          \
    do {                                                \
        if (DEBUG_##type) {                             \
            qemu_log("%s: " format "\n",                \
                     TYPE_QUICC_##type, ##__VA_ARGS__); \
        }                                               \
    } while (0)
#else
#define DBG(type, format, ...) do {} while (0)
#endif
#define ERR(type, format, ...)                              \
    do {                                                    \
        qemu_log_mask(LOG_GUEST_ERROR, "%s: " format "\n",  \
                      TYPE_QUICC_##type, ##__VA_ARGS__);    \
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
    REG_CECR,
    REG_SDSR,
    REG_SDMR,
    REG_SDAQR,
    REG_SDAQMR,
    REG_SDEBCR,
};

static const RegDef32 quicc_reg_regs[] = {
    REG_ITEM(REG_CECR,      0x0100, 0x00000000, 0x83FF7FFF),
    REG_ITEM(REG_SDSR,      0x4000, 0x00000000, 0x03000000),
    REG_ITEM(REG_SDMR,      0x4004, 0x0000A000, 0xA38CEBC8),
    REG_ITEM(REG_SDAQR,     0x4038, 0x00000000, 0xFFFF0001),
    REG_ITEM(REG_SDAQMR,    0x403C, 0x00000000, 0xFFFF0000),
    REG_ITEM(REG_SDEBCR,    0x4044, 0x00000000, 0x01FFFFFF),
};

enum {
    REG_CECR__FLG_BIT = 15,
    REG_CECR__FLG_MASK = 0x00010000,
};

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[ARRAY_SIZE(quicc_reg_regs)];
} QUICCRegState;

static uint64_t quicc_reg_read(void *opaque, hwaddr offset, unsigned size)
{
    QUICCRegState *s = QUICC_REG(opaque);
    RegDef32 reg = regdef_find(quicc_reg_regs, offset);

    if (reg.index < 0) {
        ERR(REG, "Bad read offset %#" HWADDR_PRIx, offset);
        return 0;
    }

    uint64_t value = s->regs[reg.index];

    DBG(REG, "Read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    return value;
}

static void quicc_reg_write(void *opaque, hwaddr offset, uint64_t value,
                            unsigned size)
{
    QUICCRegState *s = QUICC_REG(opaque);
    RegDef32 reg = regdef_find(quicc_reg_regs, offset);

    if (reg.index < 0) {
        ERR(REG, "Bad write offset %#" HWADDR_PRIx, offset);
        return;
    }

    DBG(REG, "Write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    if (!!(value & ~reg.write_mask)) {
        ERR(REG, "Maybe write to a read only bit %#" PRIx64,
            (value & ~reg.write_mask));
    }

    switch (reg.index) {
    case REG_CECR:
        if (GET_FIELD(REG_CECR, FLG, value) == 1) {
            DBG(REG, "Command semaphoe flag, and cleared");
            CLEAR_FIELD(REG_CECR, FLG, value);
        }
        s->regs[reg.index] = value;
        break;
    default:
        s->regs[reg.index] = value;
        break;
    }
}

static void quicc_reg_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = quicc_reg_read,
        .write      = quicc_reg_write,
        .endianness = DEVICE_BIG_ENDIAN,
    };

    QUICCRegState *s = QUICC_REG(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_QUICC_REG, REG_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void quicc_reg_reset(DeviceState *dev)
{
    QUICCRegState *s = QUICC_REG(dev);

    for (int i = 0; i < ARRAY_SIZE(s->regs); ++i) {
        s->regs[i] = quicc_reg_regs[i].reset_value;
    }
}

static void quicc_reg_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = quicc_reg_realize;
    dc->reset   = quicc_reg_reset;
}

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion muram;
} QUICCRAMState;

static void quicc_ram_realize(DeviceState *dev, Error **errp)
{
    QUICCRAMState *s = QUICC_RAM(dev);

    memory_region_init_ram(&s->muram, OBJECT(s), TYPE_QUICC_RAM,
                           RAM_MMIO_SIZE, &error_fatal);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->muram);
}

static void quicc_ram_reset(DeviceState *dev)
{
}

static void quicc_ram_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = quicc_ram_realize;
    dc->reset   = quicc_ram_reset;
}

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion as;
    QUICCRegState reg;
    QUICCRAMState ram;
} FSLQUICCState;

static uint64_t fsl_quicc_read(void *opaque, hwaddr offset, unsigned size)
{
    ERR(REG, "Bad read offset %#" HWADDR_PRIx, offset);
    return 0;
}

static void fsl_quicc_write(void *opaque, hwaddr offset, uint64_t value,
                        unsigned size)
{
    ERR(REG, "Bad write %#" PRIx64 " to offset %#" HWADDR_PRIx, value, offset);
    return;
}

static void fsl_quicc_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = fsl_quicc_read,
        .write      = fsl_quicc_write,
        .endianness = DEVICE_BIG_ENDIAN,
    };

    FSLQUICCState *s = FSL_QUICC(dev);

    memory_region_init_io(&s->as, OBJECT(s), &ops, s,
                          TYPE_FSL_QUICC, QUICC_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->as);

    Error *err = NULL;
    SysBusDevice *bus;

    /* Register Space */
    object_property_set_bool(OBJECT(&s->reg), true, "realized", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    bus = SYS_BUS_DEVICE(&s->reg);
    memory_region_add_subregion(&s->as, 0x00000,
                                sysbus_mmio_get_region(bus, 0));

    /* RAM Space */
    object_property_set_bool(OBJECT(&s->ram), true, "realized", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    bus = SYS_BUS_DEVICE(&s->ram);
    memory_region_add_subregion(&s->as, 0x08000,
                                sysbus_mmio_get_region(bus, 0));
}

static void fsl_quicc_reset(DeviceState *dev)
{
}

static void fsl_quicc_init(Object *obj)
{
    FSLQUICCState *s = FSL_QUICC(obj);

    sysbus_init_child_obj(obj, "reg", &s->reg, sizeof(s->reg),
                          TYPE_QUICC_REG);
    sysbus_init_child_obj(obj, "ram", &s->ram, sizeof(s->ram),
                          TYPE_QUICC_RAM);
}

static void fsl_quicc_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = fsl_quicc_realize;
    dc->reset   = fsl_quicc_reset;
}

static void fsl_quicc_register_types(void)
{
    static const TypeInfo quicc_info = {
        .name = TYPE_FSL_QUICC,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(FSLQUICCState),
        .instance_init = fsl_quicc_init,
        .class_init = fsl_quicc_class_init,
    };
    static const TypeInfo reg_info = {
        .name = TYPE_QUICC_REG,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QUICCRegState),
        .class_init = quicc_reg_class_init,
    };
    static const TypeInfo ram_info = {
        .name = TYPE_QUICC_RAM,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QUICCRAMState),
        .class_init = quicc_ram_class_init,
    };

    type_register_static(&quicc_info);
    type_register_static(&reg_info);
    type_register_static(&ram_info);
}

type_init(fsl_quicc_register_types)
