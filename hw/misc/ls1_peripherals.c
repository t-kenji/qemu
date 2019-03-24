/*
 * Copyright (c) 2019 t-kenji <protect.2501@gmail.com>
 *
 * QorIQ LayerScape1 Peripherals pseudo-device
 *
 * - Debug configuration register address space
 * - Integrated Flash Controller
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
 * file name "LS1046ARM.pdf". You can easily find it on the web.
 *
 */

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qemu/log.h"

//#define ENABLE_DEBUG

#define TYPE_LS1_DCSR "ls1.dcsr"
#define LS1_DCSR(obj) OBJECT_CHECK(LS1DCSRState, (obj), TYPE_LS1_DCSR)

#define TYPE_LS1_IFC "ls1.ifc"
#define LS1_IFC(obj) OBJECT_CHECK(LS1IFCState, (obj), TYPE_LS1_IFC)

#if defined(ENABLE_DEBUG)
#define DEBUG(type, format, ...) qemu_log("%s: " format "\n", TYPE_LS1_##type, ##__VA_ARGS__)
#else
#define DEBUG(type, format, ...)
#endif

enum {
    LS1_DCSR_MMIO_SIZE = 0x04000000,
    LS1_IFC_MMIO_SIZE =  0x20000000,
};

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
} LS1DCSRState;

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
} LS1IFCState;

static const VMStateDescription ls1_dcsr_vmstate = {
    .name = TYPE_LS1_DCSR,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_END_OF_LIST()
    },
};

static const VMStateDescription ls1_ifc_vmstate = {
    .name = TYPE_LS1_IFC,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_END_OF_LIST()
    },
};

static uint64_t ls1_dcsr_read(void *opaque, hwaddr offset, unsigned size)
{
    DEBUG(DCSR, "Bad read offset %#" HWADDR_PRIx, offset);
    return 0;
}

static void ls1_dcsr_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    DEBUG(DCSR, "Bad write %#" PRIx64 " to offset %#" HWADDR_PRIx, value, offset);
}

static void ls1_dcsr_reset(DeviceState *dev)
{
}

static void ls1_dcsr_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = ls1_dcsr_read,
        .write      = ls1_dcsr_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
    };

    LS1DCSRState *s = LS1_DCSR(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_LS1_DCSR, LS1_DCSR_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void ls1_dcsr_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = ls1_dcsr_realize;
    dc->reset   = ls1_dcsr_reset;
    dc->vmsd    = &ls1_dcsr_vmstate;
}

static uint64_t ls1_ifc_read(void *opaque, hwaddr offset, unsigned size)
{
    DEBUG(IFC, "Bad read offset %#" HWADDR_PRIx, offset);
    return 0;
}

static void ls1_ifc_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    DEBUG(IFC, "Bad write %#" PRIx64 " to offset %#" HWADDR_PRIx, value, offset);
}

static void ls1_ifc_reset(DeviceState *dev)
{
}

static void ls1_ifc_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = ls1_ifc_read,
        .write      = ls1_ifc_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
    };

    LS1IFCState *s = LS1_IFC(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_LS1_IFC, LS1_IFC_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void ls1_ifc_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = ls1_ifc_realize;
    dc->reset   = ls1_ifc_reset;
    dc->vmsd    = &ls1_ifc_vmstate;
}

static void ls1_peripherals_register_types(void)
{
    static const TypeInfo dcsr_info = {
        .name = TYPE_LS1_DCSR,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(LS1DCSRState),
        .class_init = ls1_dcsr_class_init,
    };
    static const TypeInfo ifc_info = {
        .name = TYPE_LS1_IFC,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(LS1IFCState),
        .class_init = ls1_ifc_class_init,
    };

    type_register_static(&dcsr_info);
    type_register_static(&ifc_info);
}

type_init(ls1_peripherals_register_types)
