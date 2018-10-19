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

//#define USING_RAM

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
    ARMCPU cpu;
    NVICState nvic;
#if defined(USING_RAM)
    MemoryRegion container;
#endif
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
    QuatroCM3State *s = QUATRO_CM3(dev);

    s->cpu.env.nvic = &s->nvic;
    s->nvic.cpu = &s->cpu;

#if defined(USING_RAM)
    object_property_set_link(OBJECT(&s->cpu), OBJECT(&s->container),
                             "memory", &error_abort);
#endif
    object_property_set_bool(OBJECT(&s->cpu), true,
                             "start-powered-off", &error_abort);
    object_property_set_bool(OBJECT(&s->cpu), true,
                             "realized", &error_abort);
    object_property_set_bool(OBJECT(&s->nvic), true,
                             "realized", &error_abort);

    qdev_pass_gpios(DEVICE(&s->nvic), dev, NULL);
    qdev_pass_gpios(DEVICE(&s->nvic), dev, "SYSRESETREQ");
    SysBusDevice *sbd = SYS_BUS_DEVICE(&s->nvic);
    sysbus_connect_irq(sbd, 0, qdev_get_gpio_in(DEVICE(&s->cpu), ARM_CPU_IRQ));

#if defined(USING_RAM)
    memory_region_add_subregion(&s->container, 0xE000E000,
                                sysbus_mmio_get_region(sbd, 0));
#endif
}

static void quatro_cm3_init(Object *obj)
{
    static const MemoryRegionOps ops = {
        .read       = quatro_cm3_read,
        .write      = quatro_cm3_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
    };

    QuatroCM3State *s = QUATRO_CM3(obj);

#if defined(USING_RAM)
    memory_region_init_ram(&s->container, obj, "quatro-cm3.container",
                           QUATRO_CM3_MEM_SIZE, &error_abort);
#endif
    memory_region_init_io(&s->iomem, obj, &ops, s,
                          TYPE_QUATRO_CM3, QUATRO_CM3_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->iomem);
    object_initialize(&s->cpu, sizeof(s->cpu), ARM_CPU_TYPE_NAME("cortex-m3"));

    sysbus_init_child_obj(obj, "nvic", &s->nvic, sizeof(s->nvic), TYPE_NVIC);
    object_property_add_alias(obj, "num-irq",
                              OBJECT(&s->nvic), "num-irq", &error_abort);
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
