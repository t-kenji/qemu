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
#include "qemu/log.h"

#define TYPE_STMMAC "stmmaceth"
#define STMMAC(obj) OBJECT_CHECK(STMMACState, (obj), TYPE_STMMAC)

enum STMMACMemoryMap {
    STMMAC_MMIO_SIZE = 0x9000,
};

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
} STMMACState;

static const VMStateDescription stmmac_vmstate = {
    .name               = TYPE_STMMAC,
    .version_id         = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_END_OF_LIST()
    },
};

static uint64_t stmmac_read(void *opaque, hwaddr offset, unsigned size)
{
    qemu_log("%s: Bad read offset 0x%" HWADDR_PRIx "\n",
             TYPE_STMMAC, offset);

    return 0;
}

static void stmmac_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    qemu_log("%s: Bad write offset 0x%" HWADDR_PRIx
             " (value is 0x%" PRIx64 ")\n",
             TYPE_STMMAC, offset, value);
}

static void stmmac_reset(DeviceState *dev)
{
}

static void stmmac_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = stmmac_read,
        .write      = stmmac_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
    };

    STMMACState *s = STMMAC(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_STMMAC, STMMAC_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void stmmac_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = stmmac_realize;
    dc->reset   = stmmac_reset;
    dc->vmsd    = &stmmac_vmstate;
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
