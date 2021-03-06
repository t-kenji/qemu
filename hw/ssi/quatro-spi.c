/*
 * CSR Quatro 5500 fcspi emulation
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
#include "hw/ssi/ssi.h"
#include "hw/misc/gen-reg.h"
#include "sysemu/dma.h"
#include "qemu/fifo8.h"
#include "qemu/log.h"

//#define ENABLE_DEBUG

#define TYPE_QUATRO_FCSPI "quatro5500-fcspi"
#define QUATRO_FCSPI(obj) OBJECT_CHECK(QuatroFCSPIState, (obj), TYPE_QUATRO_FCSPI)

#define TYPE_QUATRO_SPI "quatro5500-spi"
#define QUATRO_SPI(obj) OBJECT_CHECK(QuatroSPIState, (obj), TYPE_QUATRO_SPI)

#if defined(ENABLE_DEBUG)
#define DEBUG(type, format, ...)                     \
    do {                                             \
        qemu_log("%s: " format "\n",                 \
                 TYPE_QUATRO_##type, ##__VA_ARGS__); \
    } while (0)
#else
#define DEBUG(type, format, ...) \
    do {} while (0)
#endif
#define ERROR(type, format, ...)                           \
    do {                                                   \
        qemu_log_mask(LOG_GUEST_ERROR, "%s: " format "\n", \
                      TYPE_QUATRO_##type, ##__VA_ARGS__);  \
    } while (0)

#define FIFO_CAPACITY (256)

enum QuatroSPIMemoryMap {
    QUATRO_FCSPI_MMIO_SIZE = 0x10000,
    QUATRO_SPI_MMIO_SIZE = 0x10000,
};

enum QuatroFCSPIBits {
    FCSPI_DMA_CST__DIR_BIT = 0,
    FCSPI_DMA_CST__TRANS_BIT = 4,
    FCSPI_DMA_CST__RESET_BIT = 24,
};

enum Quatro_FCSPIVals {
    FCSPI_ACCRR1__ERASE_SECTOR = 0,
    FCSPI_ACCRR1__ERASE_BLOCK = 1,
    FCSPI_ACCRR1__ERASE_CHIP =2,
};

enum QuatroSPIBits {
    SPICMD__DA = 14,
    SPICMD__START = 31,
    SPIDCTL__RB = 10,
    SPIDCTL__NW = 9,
    SPIDCTL__NA = 8,
};

enum QuatroFCSPIRegs {
    CTRL,
    STAT,
    ACCRR0,
    ACCRR1,
    ACCRR2,
    DDPM,
    RWDATA,
    FFSTAT,
    DEFMEM,
    EXADDR,
    MEMSPEC,
    DMA_SADDR,
    DMA_FADDR,
    DMA_LEN,
    DMA_CST,
    DMA_DEBUG,
    DMA_SPARE,
};

static const RegDef32 quatro_fcspi_regs[] = {
    REG_ITEM(CTRL,      0x0000, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(STAT,      0x0004, 0x00000008, 0xFFFFFFFF),
    REG_ITEM(ACCRR0,    0x0008, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(ACCRR1,    0x000C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(ACCRR2,    0x0010, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DDPM,      0x0014, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(RWDATA,    0x0018, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(FFSTAT,    0x001C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DEFMEM,    0x0020, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(EXADDR,    0x0024, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(MEMSPEC,   0x0028, 0x0020BA20, 0xFFFFFFFF),
    REG_ITEM(DMA_SADDR, 0x0800, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DMA_FADDR, 0x0804, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DMA_LEN,   0x0808, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DMA_CST,   0x080C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DMA_DEBUG, 0x0810, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(DMA_SPARE, 0x0814, 0x00000000, 0xFFFFFFFF),
};

enum QuatroSPIRegs {
    SPIERR,
    SPICLK0,
    SPICLK1,
    SPIXCFG,
    SPICST,
    SPICFG0,
    SPICFG1,
    SPICFG2,
    SPICMD0,
    SPICMD1,
    SPICMD2,
    SPIDCTL,
    SPIDCMD,
    SPIDADDR,
    SPIRADDR,
    SPIDFIFO,
    SPIINT,
};

static const RegDef32 quatro_spi_regs[] = {
    REG_ITEM(SPIERR,    0x0000, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SPICLK0,   0x0010, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SPICLK1,   0x0014, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SPIXCFG,   0x0018, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SPICST,    0x001C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SPICFG0,   0x0020, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SPICFG1,   0x0024, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SPICFG2,   0x0028, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SPICMD0,   0x0040, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SPICMD1,   0x0044, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SPICMD2,   0x0048, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SPIDCTL,   0x0080, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SPIDCMD,   0x0084, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SPIDADDR,  0x0088, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SPIRADDR,  0x008C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SPIDFIFO,  0x0090, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(SPIINT,    0x00A0, 0x00000000, 0xFFFFFFFF),
};

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem;
    uint32_t regs[ARRAY_SIZE(quatro_fcspi_regs)];
    qemu_irq irq;
    bool irqstat;
    qemu_irq cs_line;
    SSIBus *spi;
    Fifo8 tx_fifo;
    Fifo8 rx_fifo;
} QuatroFCSPIState;

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem;
    uint32_t regs[ARRAY_SIZE(quatro_spi_regs)];
    qemu_irq irq;
    qemu_irq cs_line;
    SSIBus *spi;
    Fifo8 tx_fifo;
    Fifo8 rx_fifo;
} QuatroSPIState;

static const VMStateDescription quatro_fcspi_vmstate = {
    .name = TYPE_QUATRO_FCSPI,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_UINT32_ARRAY(regs, QuatroFCSPIState,
                             ARRAY_SIZE(quatro_fcspi_regs)),
        VMSTATE_BOOL(irqstat, QuatroFCSPIState),
        VMSTATE_FIFO8(tx_fifo, QuatroFCSPIState),
        VMSTATE_FIFO8(rx_fifo, QuatroFCSPIState),
        VMSTATE_END_OF_LIST()
    },
};

static const VMStateDescription quatro_spi_vmstate = {
    .name = TYPE_QUATRO_SPI,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_UINT32_ARRAY(regs, QuatroSPIState, ARRAY_SIZE(quatro_spi_regs)),
        VMSTATE_FIFO8(tx_fifo, QuatroSPIState),
        VMSTATE_FIFO8(rx_fifo, QuatroSPIState),
        VMSTATE_END_OF_LIST()
    },
};

static void quatro_fcspi_int_update(QuatroFCSPIState *s)
{
    qemu_set_irq(s->irq, s->irqstat);
}

static void quatro_fcspi_dma_transfer(QuatroFCSPIState *s)
{
    uint32_t cst = s->regs[DMA_CST];
    uint32_t len = s->regs[DMA_LEN];
    uint32_t faddr = s->regs[DMA_FADDR];
    dma_addr_t phys_addr = s->regs[DMA_SADDR];
    uint32_t data_size;

    if ((cst & (1 << FCSPI_DMA_CST__DIR_BIT)) != 0) {
        /* Switch to write enable. */
        qemu_set_irq(s->cs_line, 0);
        ssi_transfer(s->spi, 0x06);
        qemu_set_irq(s->cs_line, 1);

        /* write */
        qemu_set_irq(s->cs_line, 0);
        ssi_transfer(s->spi, 0x02);     /* FIXME: may be data alignment. */
        ssi_transfer(s->spi, (faddr & 0x00FF0000) >> 16);
        ssi_transfer(s->spi, (faddr & 0x0000FF00) >>  8);
        ssi_transfer(s->spi, (faddr & 0x000000FF) >>  0);

        do {
            static uint8_t buf[FIFO_CAPACITY];
            data_size = MIN(sizeof(buf), len);
            dma_memory_read(&address_space_memory, phys_addr,
                            buf, data_size);
            for (uint32_t cur = 0; cur < data_size && len > 0; ++cur, --len) {
                ssi_transfer(s->spi, buf[cur]);
            }
            phys_addr += data_size;
        } while (len > 0);

        qemu_set_irq(s->cs_line, 1);

        /* Switch to write disable. */
        qemu_set_irq(s->cs_line, 0);
        ssi_transfer(s->spi, 0x04);
        qemu_set_irq(s->cs_line, 1);
    } else {
        /* read */
        qemu_set_irq(s->cs_line, 0);
        ssi_transfer(s->spi, 0x03);     /* FIXME: may be data alignment. */
        ssi_transfer(s->spi, (faddr & 0x00FF0000) >> 16);
        ssi_transfer(s->spi, (faddr & 0x0000FF00) >>  8);
        ssi_transfer(s->spi, (faddr & 0x000000FF) >>  0);

        fifo8_reset(&s->rx_fifo);
        do {
            data_size = len;
            while (!fifo8_is_full(&s->rx_fifo) && (len > 0)) {
                fifo8_push(&s->rx_fifo, (uint8_t)ssi_transfer(s->spi, 0));
                --len;
            }
            if (data_size > fifo8_num_used(&s->rx_fifo)) {
                data_size = fifo8_num_used(&s->rx_fifo);
            }
            const uint8_t *buf = fifo8_pop_buf(&s->rx_fifo,
                                               data_size,
                                               &data_size);
            dma_memory_write(&address_space_memory, phys_addr,
                             buf, data_size);
            phys_addr += data_size;
        } while (len > 0);

        qemu_set_irq(s->cs_line, 1);
    }

    s->irqstat = true;
}

static void quatro_fcspi_erase(QuatroFCSPIState *s)
{
    switch (s->regs[ACCRR1]) {
    case FCSPI_ACCRR1__ERASE_SECTOR:
        qemu_set_irq(s->cs_line, 0);
        ssi_transfer(s->spi, 0x20);
        ssi_transfer(s->spi, (s->regs[ACCRR0] & 0x00FF0000) >> 16);
        ssi_transfer(s->spi, (s->regs[ACCRR0] & 0x0000FF00) >>  8);
        ssi_transfer(s->spi, (s->regs[ACCRR0] & 0x000000FF) >>  0);
        qemu_set_irq(s->cs_line, 1);
        break;
    case FCSPI_ACCRR1__ERASE_BLOCK:
        ERROR(FCSPI, "Block erase not supported");
        break;
    case FCSPI_ACCRR1__ERASE_CHIP:
        ERROR(FCSPI, "Chip erase not supported");
        break;
    default:
        ERROR(FCSPI, "Unknown erase type %d", s->regs[ACCRR1]);
        return;
    }
}

static uint64_t quatro_fcspi_read(void *opaque, hwaddr offset, unsigned size)
{
    QuatroFCSPIState *s = QUATRO_FCSPI(opaque);
    RegDef32 reg = regdef_find(quatro_fcspi_regs, offset);

    if (reg.index < 0) {
        ERROR(FCSPI, "Bad read offset %#" HWADDR_PRIx, offset);
        return 0;
    }

    uint64_t value = s->regs[reg.index];
    DEBUG(FCSPI, "Read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")",
          value, reg.name, offset);
    return value;
}

static void quatro_fcspi_write(void *opaque,
                               hwaddr offset,
                               uint64_t value,
                               unsigned size)
{
    QuatroFCSPIState *s = QUATRO_FCSPI(opaque);
    RegDef32 reg = regdef_find(quatro_fcspi_regs, offset);

    if (reg.index < 0) {
        ERROR(FCSPI, "Bad write offset %#" HWADDR_PRIx, offset);
        return;
    }

    DEBUG(FCSPI, "Write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
          value, reg.name, offset);
    if (!!(value & ~reg.write_mask)) {
        ERROR(FCSPI, "Maybe write to a read only bit %#" PRIx64,
              (value & ~reg.write_mask));
    }

    switch (reg.index) {
    case ACCRR2:
        s->regs[ACCRR2] = (uint32_t)value;
        if (s->regs[ACCRR2] & 2) {
            quatro_fcspi_erase(s);
        }
        break;
    case DMA_CST:
        s->regs[DMA_CST] = (uint32_t)value;
        if (s->regs[DMA_CST] & (1 << FCSPI_DMA_CST__TRANS_BIT)) {
            quatro_fcspi_dma_transfer(s);
        }
        if (s->regs[DMA_CST] & (1 << FCSPI_DMA_CST__RESET_BIT)) {
            s->regs[DMA_CST] &= ~ (1 << FCSPI_DMA_CST__RESET_BIT);
            s->irqstat = false;
        }
        quatro_fcspi_int_update(s);
        break;
    default:
        s->regs[reg.index] = (uint32_t)value;
        break;
    }
}

static void quatro_fcspi_reset(DeviceState *dev)
{
    QuatroFCSPIState *s = QUATRO_FCSPI(dev);

    for (int i = 0; i < ARRAY_SIZE(s->regs); ++i) {
        s->regs[i] = quatro_fcspi_regs[i].reset_value;
    }

    qemu_set_irq(s->irq, 0);
    qemu_set_irq(s->cs_line, 0);
    fifo8_reset(&s->tx_fifo);
    fifo8_reset(&s->rx_fifo);
}

static void quatro_fcspi_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = quatro_fcspi_read,
        .write      = quatro_fcspi_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
    };

    QuatroFCSPIState *s = QUATRO_FCSPI(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    s->spi = ssi_create_bus(dev, "spi");

    sysbus_init_irq(sbd, &s->irq);
    ssi_auto_connect_slaves(dev, &s->cs_line, s->spi);
    sysbus_init_irq(sbd, &s->cs_line);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_QUATRO_FCSPI, QUATRO_FCSPI_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);

    fifo8_create(&s->tx_fifo, FIFO_CAPACITY);
    fifo8_create(&s->rx_fifo, FIFO_CAPACITY);
}

static void quatro_fcspi_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = quatro_fcspi_realize;
    dc->reset   = quatro_fcspi_reset;
    dc->vmsd    = &quatro_fcspi_vmstate;
}

static uint64_t quatro_spi_read(void *opaque, hwaddr offset, unsigned size)
{
    QuatroSPIState *s = QUATRO_SPI(opaque);
    RegDef32 reg = regdef_find(quatro_spi_regs, offset);

    if (reg.index < 0) {
        ERROR(SPI, "Bad read offset %#" HWADDR_PRIx, offset);
        return 0;
    }

    uint64_t value = s->regs[reg.index];
    switch (reg.index) {
    case SPIDCTL:
        value |= fifo8_num_used(&s->rx_fifo);
        break;
    case SPIDFIFO:
        value = fifo8_pop(&s->rx_fifo);
        break;
    }
    DEBUG(SPI, "Read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")",
          value, reg.name, offset);
    return value;
}

static void quatro_spi_write(void *opaque,
                             hwaddr offset,
                             uint64_t value,
                             unsigned size)
{
    QuatroSPIState *s = QUATRO_SPI(opaque);
    RegDef32 reg = regdef_find(quatro_spi_regs, offset);

    if (reg.index < 0) {
        ERROR(SPI, "Bad write offset %#" HWADDR_PRIx, offset);
        return;
    }

    DEBUG(SPI, "Write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
          value, reg.name, offset);
    if (!!(value & ~reg.write_mask)) {
        ERROR(SPI, "Maybe write to a read only bit %#" PRIx64,
              (value & ~reg.write_mask));
    }

    switch (reg.index) {
    case SPICMD0:
        if (value & (1 << SPICMD__START)) {
            /* TODO: Cut out as a function. */
            uint32_t write_bytes = ((((uint32_t)value >> 16) & 0x1FFF) + 1) >> 3;
            uint32_t read_bytes = (((uint32_t)value & 0x1FFF) + 1) >> 3;

            qemu_set_irq(s->cs_line, 0);
            if (write_bytes > 0) {
                if (s->regs[SPIDCTL] & (1 << SPIDCTL__NW)) {
                    fifo8_reset(&s->rx_fifo);
                    do {
                        uint8_t tx = fifo8_pop(&s->tx_fifo);
                        uint32_t rx = ssi_transfer(s->spi, tx);
                        fifo8_push(&s->rx_fifo, rx);
                        --write_bytes;
                    } while (write_bytes > 0);
                    fifo8_reset(&s->tx_fifo);
                } else {
                    static uint8_t buf[FIFO_CAPACITY];
                    dma_addr_t phys_addr = s->regs[SPIDADDR];

                    memset(buf, 0, sizeof(buf));

                    if (!(value & (1 << SPICMD__DA))) {
                        ssi_transfer(s->spi, 0);
                    }
                    do {
                        uint32_t data_size = MIN(sizeof(buf), write_bytes);
                        dma_memory_read(&address_space_memory, phys_addr, buf, data_size);
                        for (uint32_t i = 0; i < data_size; ++i) {
                            ssi_transfer(s->spi, buf[i]);
                        }
                        write_bytes -= data_size;
                        phys_addr += data_size;
                    } while (write_bytes > 0);
                    if (value & (1 << SPICMD__DA)) {
                        ssi_transfer(s->spi, 0);
                    }

                    if (read_bytes > 0) {
                        phys_addr = (s->regs[SPIDCTL] & (1 << SPIDCTL__RB)) ? s->regs[SPIRADDR] : s->regs[SPIDADDR];
                        fifo8_reset(&s->rx_fifo);
                        do {
                            uint32_t data_size = read_bytes;
                            while (!fifo8_is_full(&s->rx_fifo) && (read_bytes > 0)) {
                                uint32_t rx = ssi_transfer(s->spi, 0);
                                fifo8_push(&s->rx_fifo, (uint8_t)rx);
                                --read_bytes;
                            }
                            if (data_size > fifo8_num_used(&s->rx_fifo)) {
                                data_size = fifo8_num_used(&s->rx_fifo);
                            }
                            const uint8_t *read_buf = fifo8_pop_buf(&s->rx_fifo, data_size, &data_size);
                            dma_memory_write(&address_space_memory, phys_addr, read_buf, data_size);
                            phys_addr += data_size;
                        } while (read_bytes > 0);
                    }
                }
            }
            qemu_set_irq(s->cs_line, 1);

            s->regs[SPIINT] |= 0x2;
            qemu_irq_raise(s->irq);
        }
        break;
    case SPIDFIFO:
        DEBUG(SPI, "Write size: %d", size);
        for (int i = 0; i < sizeof(s->regs[0]); ++i) {
            fifo8_push(&s->tx_fifo, (uint8_t)((value >>  (8 * i)) & 0xFF));
        }
        break;
    case SPIINT:
        if (((value & (1 << 1)) != 0) && ((s->regs[SPIINT] & (1 << 1)) != 0)) {
            s->regs[SPIINT] &= ~(1 << 1);
            qemu_irq_lower(s->irq);
        }
        break;
    default:
        s->regs[reg.index] = (uint32_t)value;
        break;
    }
}

static void quatro_spi_reset(DeviceState *dev)
{
    QuatroSPIState *s = QUATRO_SPI(dev);

    for (int i = 0; i < ARRAY_SIZE(s->regs); ++i) {
        s->regs[i] = quatro_spi_regs[i].reset_value;
    }

    qemu_set_irq(s->irq, 0);
    qemu_set_irq(s->cs_line, 0);
    fifo8_reset(&s->tx_fifo);
    fifo8_reset(&s->rx_fifo);
}

static void quatro_spi_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = quatro_spi_read,
        .write      = quatro_spi_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
    };

    QuatroSPIState *s = QUATRO_SPI(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    s->spi = ssi_create_bus(dev, "spi");

    sysbus_init_irq(sbd, &s->irq);
    ssi_auto_connect_slaves(dev, &s->cs_line, s->spi);
    sysbus_init_irq(sbd, &s->cs_line);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_QUATRO_SPI, QUATRO_SPI_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);

    fifo8_create(&s->tx_fifo, FIFO_CAPACITY);
    fifo8_create(&s->rx_fifo, FIFO_CAPACITY);
}

static void quatro_spi_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = quatro_spi_realize;
    dc->reset   = quatro_spi_reset;
    dc->vmsd    = &quatro_spi_vmstate;
}

static void quatro_spi_register_types(void)
{
    static const TypeInfo fcspi_tinfo = {
        .name = TYPE_QUATRO_FCSPI,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroFCSPIState),
        .class_init = quatro_fcspi_class_init,
    };
    static const TypeInfo spi_tinfo = {
        .name = TYPE_QUATRO_SPI,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroSPIState),
        .class_init = quatro_spi_class_init,
    };

    type_register_static(&fcspi_tinfo);
    type_register_static(&spi_tinfo);
}

type_init(quatro_spi_register_types)
