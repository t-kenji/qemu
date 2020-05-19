#include "qemu/osdep.h"
#include "qemu/fifo32.h"
#include "qemu/bswap.h"
#include "qapi/error.h"
#include "exec/address-spaces.h"
#include "hw/hw.h"
#include "hw/sysbus.h"
#include "hw/ssi/ssi.h"
#include "hw/irq.h"
#include "hw/misc/gen-reg.h"
#include "qemu/log.h"

#define ENABLE_DEBUG
#define DEBUG_ESPI 0

enum eSPIMemorySize {
    ESPI_MMIO_SIZE = 0x100,
    ESPI_FIFO_SIZE = 32 / 4,
};

#define TYPE_FSL_ESPI "fsl-espi"
#define FSL_ESPI(obj) OBJECT_CHECK(FSLeSPIState, (obj), TYPE_FSL_ESPI)

#if defined(ENABLE_DEBUG)
#define DBG(type, format, ...)                          \
    do {                                                \
        if (DEBUG_##type) {                             \
            qemu_log("%s: " format "\n",                \
                     TYPE_FSL_##type, ##__VA_ARGS__); \
        }                                               \
    } while (0)
#else
#define DBG(type, format, ...) do {} while (0)
#endif
#define ERR(type, format, ...)                              \
    do {                                                    \
        qemu_log_mask(LOG_GUEST_ERROR, "%s: " format "\n",  \
                      TYPE_FSL_##type, ##__VA_ARGS__);    \
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
    ESPI_SPMODE,
    ESPI_SPIE,
    ESPI_SPIM,
    ESPI_SPCOM,
    ESPI_SPITF,
    ESPI_SPIRF,
    ESPI_SPMODE0,
    ESPI_SPMODE1,
    ESPI_SPMODE2,
    ESPI_SPMODE3,
};

static const RegDef32 fsl_espi_regs[] = {
    REG_ITEM(ESPI_SPMODE,   0x0000, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(ESPI_SPIE,     0x0004, 0x00208900, 0xFFFFFFFF),
    REG_ITEM(ESPI_SPIM,     0x0008, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(ESPI_SPCOM,    0x000C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(ESPI_SPITF,    0x0010, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(ESPI_SPIRF,    0x0014, 0x00000000, 0x00000000),
    REG_ITEM(ESPI_SPMODE0,  0x0020, 0x00100000, 0xFFFFFFFF),
    REG_ITEM(ESPI_SPMODE1,  0x0024, 0x00100000, 0xFFFFFFFF),
    REG_ITEM(ESPI_SPMODE2,  0x0028, 0x00100000, 0xFFFFFFFF),
    REG_ITEM(ESPI_SPMODE3,  0x002C, 0x00100000, 0xFFFFFFFF),
};

enum {
    ESPI_SPMODE__EN_BIT = 0,
    ESPI_SPMODE__EN_MASK = 0x80000000,

    ESPI_SPIE__RXCNT_BIT = 7,
    ESPI_SPIE__RXCNT_MASK = 0x3F000000,
    ESPI_SPIE__TXCNT_BIT = 15,
    ESPI_SPIE__TXCNT_MASK = 0x003F0000,
    ESPI_SPIE__TXE_BIT = 16,
    ESPI_SPIE__TXE_MASK = 0x00008000,
    ESPI_SPIE__DON_BIT = 17,
    ESPI_SPIE__DON_MASK = 0x00004000,
    ESPI_SPIE__RXT_BIT = 18,
    ESPI_SPIE__RXT_MASK = 0x00002000,
    ESPI_SPIE__RXF_BIT = 19,
    ESPI_SPIE__RXF_MASK = 0x00001000,
    ESPI_SPIE__TXT_BIT = 20,
    ESPI_SPIE__TXT_MASK = 0x00000800,
    ESPI_SPIE__RNE_BIT = 22,
    ESPI_SPIE__RNE_MASK = 0x00000200,
    ESPI_SPIE__TNF_BIT = 23,
    ESPI_SPIE__TNF_MASK = 0x00000100,

    ESPI_SPIM__TXE_BIT = 16,
    ESPI_SPIM__TXE_MASK = 0x00008000,
    ESPI_SPIM__DON_BIT = 17,
    ESPI_SPIM__DON_MASK = 0x00004000,
    ESPI_SPIM__RXT_BIT = 18,
    ESPI_SPIM__RXT_MASK = 0x00002000,
    ESPI_SPIM__RXF_BIT = 19,
    ESPI_SPIM__RXF_MASK = 0x00001000,
    ESPI_SPIM__TXT_BIT = 20,
    ESPI_SPIM__TXT_MASK = 0x00000800,
    ESPI_SPIM__RNE_BIT = 22,
    ESPI_SPIM__RNE_MASK = 0x00000200,
    ESPI_SPIM__TNF_BIT = 23,
    ESPI_SPIM__TNF_MASK = 0x00000100,

    ESPI_SPCOM__CS_BIT = 1,
    ESPI_SPCOM__CS_MASK = 0xC0000000,
};

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[ARRAY_SIZE(fsl_espi_regs)];

    SSIBus *bus;
    qemu_irq irq;
    qemu_irq cs_lines[4];
    Fifo32 rx_fifo;
    Fifo32 tx_fifo;
    uint16_t rx_cnt;
} FSLeSPIState;

static inline bool fsl_espi_is_enabled(FSLeSPIState *s)
{
    return !!GET_FIELD(ESPI_SPMODE, EN, s->regs[ESPI_SPMODE]);
}

static inline uint8_t fsl_espi_selected_chip(FSLeSPIState *s)
{
    return GET_FIELD(ESPI_SPCOM, CS, s->regs[ESPI_SPCOM]);
}

static void fsl_espi_update_event(FSLeSPIState *s)
{
    uint32_t ev = s->regs[ESPI_SPIE];

    SET_FIELD(ESPI_SPIE, RXCNT, ev, s->rx_cnt);
    SET_FIELD(ESPI_SPIE, TXE, ev, fifo32_is_empty(&s->tx_fifo));
    SET_FIELD(ESPI_SPIE, RNE, ev, !fifo32_is_empty(&s->rx_fifo));
    SET_FIELD(ESPI_SPIE, TNF, ev, !fifo32_is_full(&s->tx_fifo));

    s->regs[ESPI_SPIE] = ev;
}

static void fsl_espi_flush_txfifo(FSLeSPIState *s)
{
    DBG(ESPI, "Begin: TX Fifo Size = %d, RX Fifo Size = %d",
        fifo32_num_used(&s->tx_fifo), fifo32_num_used(&s->rx_fifo));

    while (!fifo32_is_empty(&s->tx_fifo)) {
        uint32_t tx = bswap32(fifo32_pop(&s->tx_fifo)),
                 rx = 0,
                 tx_burst = 32;

        while (tx_burst > 0) {
            uint8_t byte = tx & 0xFF;

            byte = ssi_transfer(s->bus, byte);

            tx >>= 8;
            tx_burst -= 8;
            rx <<= 8;
            rx |= byte;

            ++s->rx_cnt;
        }

        if (fifo32_is_full(&s->rx_fifo)) {
            SET_FIELD(ESPI_SPIE, RXF, s->regs[ESPI_SPIE], 1);
        } else {
            fifo32_push(&s->rx_fifo, rx);
        }
    }

    SET_FIELD(ESPI_SPIE, DON, s->regs[ESPI_SPIE], 1);
}

static void fsl_espi_reset(DeviceState *dev)
{
    FSLeSPIState *s = FSL_ESPI(dev);

    for (int i = 0; i < ARRAY_SIZE(s->regs); ++i) {
        s->regs[i] = fsl_espi_regs[i].reset_value;
    }

    fifo32_reset(&s->rx_fifo);
    fifo32_reset(&s->tx_fifo);
    s->rx_cnt = 0;
    fsl_espi_update_event(s);

    for (int i = 0; i < ARRAY_SIZE(s->cs_lines); ++i) {
        qemu_irq_raise(s->cs_lines[i]);
    }
}

static uint64_t fsl_espi_read(void *opaque, hwaddr offset, unsigned size)
{
    FSLeSPIState *s = FSL_ESPI(opaque);
    RegDef32 reg = regdef_find(fsl_espi_regs, offset);

    if (reg.index < 0) {
        ERR(ESPI, "Bad read offset %#" HWADDR_PRIx, offset);
        return 0;
    }

    uint64_t value;
    switch (reg.index) {
    case ESPI_SPIRF:
        if (fifo32_is_empty(&s->rx_fifo)) {
            value = 0xDEADBEEF;
        } else {
            value = fifo32_pop(&s->rx_fifo);
            if (s->rx_cnt > 4) {
                s->rx_cnt -= 4;
            } else {
                s->rx_cnt = 0;
            }
        }
        break;
    default:
        value = s->regs[reg.index];
        break;
    }

    DBG(ESPI, "Read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    fsl_espi_update_event(s);
    return value;
}

static void fsl_espi_write(void *opaque, hwaddr offset, uint64_t value,
                           unsigned size)
{
    FSLeSPIState *s = FSL_ESPI(opaque);
    RegDef32 reg = regdef_find(fsl_espi_regs, offset);

    if (reg.index < 0) {
        ERR(ESPI, "Bad write offset %#" HWADDR_PRIx, offset);
        return;
    }

    DBG(ESPI, "Write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    if (!!(value & ~reg.write_mask)) {
        ERR(ESPI, "Maybe write to a read only bit %#" PRIx64,
            (value & ~reg.write_mask));
    }

    switch (reg.index) {
    case ESPI_SPMODE:
        s->regs[reg.index] = value;

        if (!fsl_espi_is_enabled(s)) {
            /* device is disabled, so this is a reset */
            fsl_espi_reset(DEVICE(s));
        } else {
            for (int i = 0; i < ARRAY_SIZE(s->cs_lines); ++i) {
                qemu_set_irq(s->cs_lines[i], !(i == fsl_espi_selected_chip(s)));
            }
        }
        break;
    case ESPI_SPMODE0:
        s->regs[reg.index] = value;
        break;
    case ESPI_SPIE:
        if (GET_FIELD(ESPI_SPIE, TNF, value) == 1) {
//            DBG(ESPI, "TNF cleared");
            CLEAR_FIELD(ESPI_SPIE, TNF, value);
        }
        if (GET_FIELD(ESPI_SPIE, RNE, value) == 1) {
//            DBG(ESPI, "RNE cleared");
            CLEAR_FIELD(ESPI_SPIE, RNE, value);
        }
        if (GET_FIELD(ESPI_SPIE, TXT, value) == 1) {
//            DBG(ESPI, "TXT cleared");
            CLEAR_FIELD(ESPI_SPIE, TXT, value);
        }
        if (GET_FIELD(ESPI_SPIE, RXF, value) == 1) {
//            DBG(ESPI, "RXF cleared");
            CLEAR_FIELD(ESPI_SPIE, RXF, value);
        }
        if (GET_FIELD(ESPI_SPIE, RXT, value) == 1) {
//            DBG(ESPI, "RXT cleared");
            CLEAR_FIELD(ESPI_SPIE, RXT, value);
        }
        if (GET_FIELD(ESPI_SPIE, DON, value) == 1) {
//            DBG(ESPI, "DON cleared");
            CLEAR_FIELD(ESPI_SPIE, DON, value);
        }
        if (GET_FIELD(ESPI_SPIE, TXE, value) == 1) {
//            DBG(ESPI, "TXE cleared");
            CLEAR_FIELD(ESPI_SPIE, TXE, value);
        }
        if (GET_FIELD(ESPI_SPIE, TXCNT, value) == 1) {
//            DBG(ESPI, "TXCNT cleared");
            CLEAR_FIELD(ESPI_SPIE, TXCNT, value);
        }
        if (GET_FIELD(ESPI_SPIE, RXCNT, value) == 1) {
//            DBG(ESPI, "RXCNT cleared");
            CLEAR_FIELD(ESPI_SPIE, RXCNT, value);
        }
        s->regs[reg.index] = value;
        break;
    case ESPI_SPITF:
        if (fifo32_is_full(&s->tx_fifo)) {
            ERR(ESPI, "Tx FIFO is full");
            break;
        }

        fifo32_push(&s->tx_fifo, (uint32_t)value);

        fsl_espi_flush_txfifo(s);
        break;
    default:
        s->regs[reg.index] = value;
        break;
    }

    fsl_espi_update_event(s);
}

static void fsl_espi_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = fsl_espi_read,
        .write      = fsl_espi_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
        .valid = {
            .min_access_size = 4,
            .max_access_size = 4,
            .unaligned = false,
        },
    };

    FSLeSPIState *s = FSL_ESPI(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_FSL_ESPI, ESPI_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq);

    s->bus = ssi_create_bus(dev, "spi");
    ssi_auto_connect_slaves(dev, s->cs_lines, s->bus);

    for (int i = 0; i < ARRAY_SIZE(s->cs_lines); ++i) {
        sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->cs_lines[i]);
    }

    fifo32_create(&s->rx_fifo, ESPI_FIFO_SIZE);
    fifo32_create(&s->tx_fifo, ESPI_FIFO_SIZE);
}

static void fsl_espi_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->desc    = "Freescale eSPI Controller";
    dc->realize = fsl_espi_realize;
    dc->reset   = fsl_espi_reset;
}

static void fsl_espi_register_types(void)
{
    static const TypeInfo espi_info = {
        .name = TYPE_FSL_ESPI,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(FSLeSPIState),
        .class_init = fsl_espi_class_init,
    };

    type_register_static(&espi_info);
}

type_init(fsl_espi_register_types)
