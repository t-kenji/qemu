/*
 * CSR Quatro 5500 General Purpose DMA emulation
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

#define ENABLE_DEBUG

#define TYPE_QUATRO_GPDMA "quatro5500-gpdma"
#define QUATRO_GPDMA(obj) OBJECT_CHECK(QuatroGPDMAState, (obj), TYPE_QUATRO_GPDMA)

#if defined(ENABLE_DEBUG)
#define DEBUG(format, ...)                                              \
    do {                                                                \
        qemu_log("%s: " format "\n", TYPE_QUATRO_GPDMA, ##__VA_ARGS__); \
    } while (0)
#else
#define DEBUG(format, ...) \
    do {} while (0)
#endif
#define ERROR(format, ...)                                 \
    do {                                                   \
        qemu_log_mask(LOG_GUEST_ERROR, "%s: " format "\n", \
                      TYPE_QUATRO_GPDMA, ##__VA_ARGS__);   \
    } while (0)

enum {
    QUATRO_GPDMA_MMIO_SIZE = 0x10000,
};

typedef struct {
    /*< parent >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem;
} QuatroGPDMAState;

static const VMStateDescription quatro_gpdma_vmstate = {
    .name = TYPE_QUATRO_GPDMA,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_END_OF_LIST()
    },
};

static uint64_t quatro_gpdma_read(void *opaque, hwaddr offset, unsigned size)
{
    DEBUG("Bad read offset %#" HWADDR_PRIx, offset);
    return 0;
}

static void quatro_gpdma_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    DEBUG("Bad write %" PRIx64 " to offset %#" HWADDR_PRIx, value, offset);
    return;
}

static void quatro_gpdma_reset(DeviceState *dev)
{
}

static void quatro_gpdma_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = quatro_gpdma_read,
        .write      = quatro_gpdma_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
    };

    QuatroGPDMAState *s = QUATRO_GPDMA(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_QUATRO_GPDMA, QUATRO_GPDMA_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void quatro_gpdma_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = quatro_gpdma_realize;
    dc->reset   = quatro_gpdma_reset;
    dc->vmsd    = &quatro_gpdma_vmstate;
}

static void quatro_gpdma_register_type(void)
{
    static const TypeInfo tinfo = {
        .name = TYPE_QUATRO_GPDMA,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroGPDMAState),
        .class_init = quatro_gpdma_class_init,
    };

    type_register_static(&tinfo);
}

type_init(quatro_gpdma_register_type)
