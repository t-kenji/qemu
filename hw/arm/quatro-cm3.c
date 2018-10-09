/*
 * CSR Quatro 5500 Cortex-M3 emulation
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
#include "qapi/error.h"
#include "cpu.h"
#include "hw/arm/csr-quatro.h"
#include "hw/sysbus.h"
#include "hw/intc/armv7m_nvic.h"
#include "exec/address-spaces.h"
#include "qemu/log.h"

#define TYPE_QUATRO_CM3 "quatro5500.cm3"
#define QUATRO_CM3(obj) OBJECT_CHECK(QuatroCM3State, (obj), TYPE_QUATRO_CM3)

enum QuatroCM3MemoryMap {
    QUATRO_CM3_MEM_SIZE = 0x1400000,
    QUATRO_CM3_MMIO_SIZE = 0x10000,
};

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
} QuatroCM3State;

static const VMStateDescription quatro_cm3_vmstate = {
    .name = TYPE_QUATRO_CM3,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_END_OF_LIST()
    },
};

static uint64_t quatro_cm3_read(void *opaque, hwaddr offset, unsigned size)
{
    qemu_log("%s: Bad read offset %#" HWADDR_PRIx "\n",
             TYPE_QUATRO_CM3, offset);
    return 0;
}

static void quatro_cm3_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    qemu_log("%s: Bad write offset %#" HWADDR_PRIx "\n",
             TYPE_QUATRO_CM3, offset);
}

static void quatro_cm3_reset(DeviceState *dev)
{
}

static void quatro_cm3_realize(DeviceState *dev, Error **errp)
{
}

static void quatro_cm3_init(Object *obj)
{
    static const MemoryRegionOps ops = {
        .read       = quatro_cm3_read,
        .write      = quatro_cm3_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
    };

    QuatroCM3State *s = QUATRO_CM3(obj);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_QUATRO_CM3, QUATRO_CM3_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->iomem);
}

static void quatro_cm3_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = quatro_cm3_realize;
    dc->reset   = quatro_cm3_reset;
    dc->vmsd    = &quatro_cm3_vmstate;
}

static void quatro_cm3_register_type(void)
{
    static const TypeInfo tinfo = {
        .name = TYPE_QUATRO_CM3,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroCM3State),
        .instance_init = quatro_cm3_init,
        .class_init = quatro_cm3_class_init,
    };

    type_register_static(&tinfo);
}

type_init(quatro_cm3_register_type)
