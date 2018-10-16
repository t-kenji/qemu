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
#include "sysemu/dma.h"
#include "qemu/fifo8.h"
#include "qemu/log.h"

#define TYPE_QUATRO_FCSPI "quatro5500.fcspi"
#define QUATRO_FCSPI(obj) OBJECT_CHECK(QuatroFCSPIState, (obj), TYPE_QUATRO_FCSPI)

#define FIFO_CAPACITY (256)

enum QuatroSPIMemoryMap {
    QUATRO_FCSPI_MMIO_SIZE = 0x10000,
};

enum QuatroFCSPIBits {
    FCSPI_DMA_CST__DIR_BIT = 0,
    FCSPI_DMA_CST__TRANS_BIT = 4,
    FCSPI_DMA_CST__RESET_BIT = 24,
};

typedef struct {
    const char *name;
    hwaddr offset;
    uint32_t reset_value;
} QuatroSPIReg;

#define REG_ITEM(index, offset, reset_value) \
    [index] = {#index, (offset), (reset_value)}

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
    QUATRO_FCSPI_NUM_REGS
};

static const QuatroSPIReg quatro_fcspi_regs[] = {
    REG_ITEM(CTRL,      0x0000, 0x00000000),
    REG_ITEM(STAT,      0x0004, 0x00000008),
    REG_ITEM(ACCRR0,    0x0008, 0x00000000),
    REG_ITEM(ACCRR1,    0x000C, 0x00000000),
    REG_ITEM(ACCRR2,    0x0010, 0x00000000),
    REG_ITEM(DDPM,      0x0014, 0x00000000),
    REG_ITEM(RWDATA,    0x0018, 0x00000000),
    REG_ITEM(FFSTAT,    0x001C, 0x00000000),
    REG_ITEM(DEFMEM,    0x0020, 0x00000000),
    REG_ITEM(EXADDR,    0x0024, 0x00000000),
    REG_ITEM(MEMSPEC,   0x0028, 0x0020BA20),
    REG_ITEM(DMA_SADDR, 0x0800, 0x00000000),
    REG_ITEM(DMA_FADDR, 0x0804, 0x00000000),
    REG_ITEM(DMA_LEN,   0x0808, 0x00000000),
    REG_ITEM(DMA_CST,   0x080C, 0x00000000),
    REG_ITEM(DMA_DEBUG, 0x0810, 0x00000000),
    REG_ITEM(DMA_SPARE, 0x0814, 0x00000000),
};

#undef REG_ITEM

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[QUATRO_FCSPI_NUM_REGS];
    qemu_irq irq;
    bool irqstat;
    qemu_irq cs_line;
    SSIBus *spi;
    Fifo8 tx_fifo;
    Fifo8 rx_fifo;
} QuatroFCSPIState;

static const VMStateDescription quatro_fcspi_vmstate = {
    .name = TYPE_QUATRO_FCSPI,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_UINT32_ARRAY(regs, QuatroFCSPIState, QUATRO_FCSPI_NUM_REGS),
        VMSTATE_BOOL(irqstat, QuatroFCSPIState),
        VMSTATE_FIFO8(tx_fifo, QuatroFCSPIState),
        VMSTATE_FIFO8(rx_fifo, QuatroFCSPIState),
        VMSTATE_END_OF_LIST()
    },
};

static int quatro_spi_offset_to_index(const QuatroSPIReg *regs,
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

static void quatro_fcspi_int_update(QuatroFCSPIState *s)
{
    qemu_set_irq(s->irq, s->irqstat);
}

static void quatro_fcspi_dma_transfer(QuatroFCSPIState *s)
{
    uint32_t cst = s->regs[DMA_CST];
    uint32_t len = s->regs[DMA_LEN];
    uint32_t faddr = s->regs[DMA_FADDR];
    uint32_t phys_addr = s->regs[DMA_SADDR];
    uint32_t data_size;

    if ((cst & (1 << FCSPI_DMA_CST__DIR_BIT)) != 0) {
        /* write */
        qemu_set_irq(s->cs_line, 0);
        ssi_transfer(s->spi, 0x02);
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
    } else {
        /* read */
        qemu_set_irq(s->cs_line, 0);
        ssi_transfer(s->spi, 0x03);
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
            const uint8_t *buf = fifo8_pop_buf(&s->rx_fifo, data_size, &data_size);
            dma_memory_write(&address_space_memory, phys_addr,
                             buf, data_size);
            phys_addr += data_size;
        } while (len > 0);

        qemu_set_irq(s->cs_line, 1);
    }

    s->irqstat = true;
}

static uint64_t quatro_fcspi_read(void *opaque, hwaddr offset, unsigned size)
{
    QuatroFCSPIState *s = QUATRO_FCSPI(opaque);
    int index = quatro_spi_offset_to_index(quatro_fcspi_regs,
                                           QUATRO_FCSPI_NUM_REGS,
                                           offset);

    if (index < 0) {
        qemu_log("%s: Bad read offset 0x%" HWADDR_PRIx "\n",
                 TYPE_QUATRO_FCSPI, offset);
        return 0;
    }

    uint64_t value = s->regs[index];
#if 0
    qemu_log("%s: read 0x%" PRIx64 " from %s (offset 0x%" HWADDR_PRIx ")\n",
             TYPE_QUATRO_FCSPI, value, quatro_fcspi_regs[index].name, offset);
#endif
    return value;
}

static void quatro_fcspi_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    QuatroFCSPIState *s = QUATRO_FCSPI(opaque);
    int index = quatro_spi_offset_to_index(quatro_fcspi_regs,
                                           QUATRO_FCSPI_NUM_REGS,
                                           offset);

    switch (index) {
    case CTRL:
    case STAT:
    case ACCRR0:
    case ACCRR1:
    case ACCRR2:
    case DDPM:
    case RWDATA:
    case FFSTAT:
    case DEFMEM:
    case EXADDR:
    case MEMSPEC:
    case DMA_SADDR:
        s->regs[DMA_SADDR] = (uint32_t)value;
        break;
    case DMA_FADDR:
        s->regs[DMA_FADDR] = (uint32_t)value;
        break;
    case DMA_LEN:
        s->regs[DMA_LEN] = (uint32_t)value;
        break;
    case DMA_CST:
        s->regs[DMA_CST] = (uint32_t)value;
        if (s->regs[DMA_CST] & (1 << FCSPI_DMA_CST__TRANS_BIT)) {
            qemu_set_irq(s->cs_line, 0);
            ssi_transfer(s->spi, 0x06);
            qemu_set_irq(s->cs_line, 1);

            quatro_fcspi_dma_transfer(s);
        }
        if (s->regs[DMA_CST] & (1 << FCSPI_DMA_CST__RESET_BIT)) {
            s->regs[DMA_CST] &= ~ (1 << FCSPI_DMA_CST__RESET_BIT);
            s->irqstat = false;
        }
        quatro_fcspi_int_update(s);
        break;
    case DMA_DEBUG:
    case DMA_SPARE:
        break;
    default:
        qemu_log("%s: Bad write offset 0x%" HWADDR_PRIx "\n",
                 TYPE_QUATRO_FCSPI, offset);
        return;
    }
#if 0
    qemu_log("%s: write 0x%" PRIx64 " to %s (offset 0x%" HWADDR_PRIx ")\n",
             TYPE_QUATRO_FCSPI, value, quatro_fcspi_regs[index].name, offset);
#endif
}

static void quatro_fcspi_reset(DeviceState *dev)
{
    QuatroFCSPIState *s = QUATRO_FCSPI(dev);

    for (int i = 0; i < QUATRO_FCSPI_NUM_REGS; ++i) {
        s->regs[i] = quatro_fcspi_regs[i].reset_value;
    }

    fifo8_reset(&s->tx_fifo);
    fifo8_reset(&s->rx_fifo);
    qemu_set_irq(s->irq, 0);
    qemu_set_irq(s->cs_line, 0);
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

static void quatro_spi_register_types(void)
{
    static const TypeInfo fcspi_tinfo = {
        .name = TYPE_QUATRO_FCSPI,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroFCSPIState),
        .class_init = quatro_fcspi_class_init,
    };

    type_register_static(&fcspi_tinfo);
}

type_init(quatro_spi_register_types)
