/*
 * Copyright (c) 2019 t-kenji <protect.2501@gmail.com>
 *
 * QorIQ LayerScape1 CPLD pseudo-device
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
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * *****************************************************************
 *
 * The documentation for this device is noted in the LS1046A documentation,
 * file name "LS1046ARDBRM.pdf". You can easily find it on the web.
 *
 */

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qemu/log.h"

#define ENABLE_DEBUG

#define TYPE_LS1_CPLD "ls1.cpld"
#define LS1_CPLD(obj) OBJECT_CHECK(LS1CPLDState, (obj), TYPE_LS1_CPLD)

#if defined(ENABLE_DEBUG)
#define DEBUG(format, ...) qemu_log("%s: " format "\n", TYPE_LS1_CPLD, ##__VA_ARGS__)
#else
#define DEBUG(format, ...)
#endif

enum {
    LS1_CPLD_MMIO_SIZE = 0x00001000,
};

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
} LS1CPLDState;

static const VMStateDescription ls1_cpld_vmstate = {
    .name = TYPE_LS1_CPLD,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_END_OF_LIST()
    },
};

static uint64_t ls1_cpld_read(void *opaque, hwaddr offset, unsigned size)
{
    DEBUG("Bad read offset %#" HWADDR_PRIx, offset);
    return 0;
}

static void ls1_cpld_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    DEBUG("Bad write %#" PRIx64 " to offset %#" HWADDR_PRIx, value, offset);
}

static void ls1_cpld_reset(DeviceState *dev)
{
}

static void ls1_cpld_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = ls1_cpld_read,
        .write      = ls1_cpld_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
    };

    LS1CPLDState *s = LS1_CPLD(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_LS1_CPLD, LS1_CPLD_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void ls1_cpld_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = ls1_cpld_realize;
    dc->reset   = ls1_cpld_reset;
    dc->vmsd    = &ls1_cpld_vmstate;
}

static void ls1_cpld_register_types(void)
{
    static const TypeInfo cpld_info = {
        .name = TYPE_LS1_CPLD,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(LS1CPLDState),
        .class_init = ls1_cpld_class_init,
    };

    type_register_static(&cpld_info);
}

type_init(ls1_cpld_register_types)
