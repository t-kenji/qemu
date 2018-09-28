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
#include "qemu/log.h"

#define TYPE_QUATRO_SPI "quatro5500.fcspi"
#define QUATRO_SPI(obj) OBJECT_CHECK(QuatroSPIState, (obj), TYPE_QUATRO_SPI)

enum QuatroSPIMemoryMap {
    QUATRO_SPI_MMIO_SIZE = 0x10000,
};

typedef struct {
    const char *name;
    hwaddr offset;
    uint32_t reset_value;
} QuatroSPIReg;

#define REG_ITEM(index, offset, reset_value) \
    [index] = {#index, (offset), (reset_value)}

enum QuatroSPIRegs {
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
    QUATRO_SPI_NUM_REGS
};

static const QuatroSPIReg quatro_spi_regs[] = {
    REG_ITEM(CTRL,      0x0000, 0x00000000),
    REG_ITEM(STAT,      0x0004, 0x00000000),
    REG_ITEM(ACCRR0,    0x0008, 0x00000000),
    REG_ITEM(ACCRR1,    0x000C, 0x00000000),
    REG_ITEM(ACCRR2,    0x0010, 0x00000000),
    REG_ITEM(DDPM,      0x0014, 0x00000000),
    REG_ITEM(RWDATA,    0x0018, 0x00000000),
    REG_ITEM(FFSTAT,    0x001C, 0x00000000),
    REG_ITEM(DEFMEM,    0x0020, 0x00000000),
    REG_ITEM(EXADDR,    0x0024, 0x00000000),
    REG_ITEM(MEMSPEC,   0x0028, 0x00000000),
    REG_ITEM(DMA_SADDR, 0x0200, 0x00000000),
    REG_ITEM(DMA_FADDR, 0x0204, 0x00000000),
    REG_ITEM(DMA_LEN,   0x0208, 0x00000000),
    REG_ITEM(DMA_CST,   0x020C, 0x00000000),
    REG_ITEM(DMA_DEBUG, 0x0210, 0x00000000),
    REG_ITEM(DMA_SPARE, 0x0214, 0x00000000),
};

#undef REG_ITEM

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[QUATRO_SPI_NUM_REGS];
} QuatroSPIState;

static const VMStateDescription quatro_spi_vmstate = {
    .name = TYPE_QUATRO_SPI,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_UINT32_ARRAY(regs, QuatroSPIState, QUATRO_SPI_NUM_REGS),
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

static uint64_t quatro_spi_read(void *opaque, hwaddr offset, unsigned size)
{
    QuatroSPIState *s = QUATRO_SPI(opaque);
    int index = quatro_spi_offset_to_index(quatro_spi_regs,
                                           QUATRO_SPI_NUM_REGS,
                                           offset);

    if (index < 0) {
        qemu_log("%s: Bad read offset 0x%" HWADDR_PRIx "\n",
                 TYPE_QUATRO_SPI, offset);
        return 0;
    }

    uint64_t value = s->regs[index];
    qemu_log("%s: read 0x%" PRIx64 " from %s (offset 0x%" HWADDR_PRIx ")\n",
             TYPE_QUATRO_SPI, value, quatro_spi_regs[index].name, offset);
    return value;
}

static void quatro_spi_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    int index = quatro_spi_offset_to_index(quatro_spi_regs,
                                           QUATRO_SPI_NUM_REGS,
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
    case DMA_FADDR:
    case DMA_LEN:
    case DMA_CST:
    case DMA_DEBUG:
    case DMA_SPARE:
        break;
    default:
        qemu_log("%s: Bad write offset 0x%" HWADDR_PRIx "\n",
                 TYPE_QUATRO_SPI, offset);
        return;
    }
    qemu_log("%s: write 0x%" PRIx64 " to %s (offset 0x%" HWADDR_PRIx ")\n",
             TYPE_QUATRO_SPI, value, quatro_spi_regs[index].name, offset);
}

static void quatro_spi_reset(DeviceState *dev)
{
    QuatroSPIState *s = QUATRO_SPI(dev);

    for (int i = 0; i < QUATRO_SPI_NUM_REGS; ++i) {
        s->regs[i] = quatro_spi_regs[i].reset_value;
    }
}

static void quatro_spi_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = quatro_spi_read,
        .write      = quatro_spi_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
    };

    QuatroSPIState *s = QUATRO_SPI(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_QUATRO_SPI, QUATRO_SPI_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void quatro_spi_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = quatro_spi_realize;
    dc->reset   = quatro_spi_reset;
    dc->vmsd    = &quatro_spi_vmstate;
}

static void quatro_spi_register_type(void)
{
    static const TypeInfo tinfo = {
        .name = TYPE_QUATRO_SPI,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroSPIState),
        .class_init = quatro_spi_class_init,
    };

    type_register_static(&tinfo);
}

type_init(quatro_spi_register_type)
