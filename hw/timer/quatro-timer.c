/*
 *  CSR Quatro 5500 High-resolution timer
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
#include "hw/ptimer.h"
#include "qemu/main-loop.h"
#include "qemu/log.h"

#define TYPE_QUATRO_HRT0 "quatro5500.hrt0"
#define QUATRO_HRT0(obj) OBJECT_CHECK(QuatroHRT0State, (obj), TYPE_QUATRO_HRT0)

typedef struct {
    const char *name;
    hwaddr offset;
    uint32_t reset_value;
} QuatroTimerReg;

static const QuatroTimerReg quatro_hrt0_regs[] = {
    {"HRTPRE0",         0x0000, 0x00000000},
    {"HRTCNT0H",        0x0004, 0x00000000},
    {"HRTCNT0L",        0x0008, 0x00000000},
    {"HRTCTL0",         0x000C, 0x00000000},
};

#define QUATRO_HRT0_NUM_REGS ARRAY_SIZE(quatro_hrt0_regs)

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    ptimer_state *timer;

    uint32_t regs[QUATRO_HRT0_NUM_REGS];
} QuatroHRT0State;


static const VMStateDescription quatro_hrt0_vmstate= {
    .name = TYPE_QUATRO_HRT0,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_PTIMER(timer, QuatroHRT0State),
        VMSTATE_UINT32_ARRAY(regs, QuatroHRT0State, QUATRO_HRT0_NUM_REGS),
        VMSTATE_END_OF_LIST()
    },
};

static uint64_t quatro_hrt0_read(void *opaque, hwaddr offset, unsigned size)
{
    const QuatroHRT0State *s = QUATRO_HRT0(opaque);

    for (int i = 0; i < QUATRO_HRT0_NUM_REGS; ++i ) {
        const QuatroTimerReg *reg = &quatro_hrt0_regs[i];
        if (reg->offset == offset) {
            //qemu_log("%s: read offset 0x%" HWADDR_PRIx ", value: 0x%" PRIx32 "\n", TYPE_QUATRO_HRT0, offset, s->regs[i]);
            return s->regs[i];
        }
    }

    qemu_log("%s: Bad read offset 0x%" HWADDR_PRIx "\n", TYPE_QUATRO_HRT0, offset);

    return 0;
}

static void quatro_hrt0_write(void *opaque,
                             hwaddr offset,
                             uint64_t value,
                             unsigned size)
{
    qemu_log("%s: Bad write offset 0x%" HWADDR_PRIx "\n", TYPE_QUATRO_HRT0, offset);
}

static void quatro_hrt0_init(Object *obj)
{
    //QuatroHRTState *s = QUATRO_HRT0(obj);
    qemu_log("%s:%d:%s$ trace\n", __FILE__, __LINE__, __func__);
}

static void quatro_hrt0_reset(DeviceState *dev)
{
    QuatroHRT0State *s = QUATRO_HRT0(dev);

    ptimer_stop(s->timer);
    for (int i = 0; i < QUATRO_HRT0_NUM_REGS; ++i) {
        s->regs[i] = quatro_hrt0_regs[i].reset_value;
    }
}

static void quatro_hrt0_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = quatro_hrt0_read,
        .write      = quatro_hrt0_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
    };

    qemu_log("%s:%d:%s$ trace\n", __FILE__, __LINE__, __func__);
    QuatroHRT0State *s = QUATRO_HRT0(dev);

    s->timer = ptimer_init(NULL, PTIMER_POLICY_DEFAULT);
    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_QUATRO_HRT0, 0x10);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void quatro_hrt0_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    qemu_log("%s:%d:%s$ trace\n", __FILE__, __LINE__, __func__);
    dc->desc    = "CSR Quatro 5500 High-resolution timer";
    dc->realize = quatro_hrt0_realize;
    dc->reset   = quatro_hrt0_reset;
    dc->vmsd    = &quatro_hrt0_vmstate;
}

static void quatro_hrt0_register_type(void)
{
    static const TypeInfo tinfo = {
        .name          = TYPE_QUATRO_HRT0,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroHRT0State),
        .instance_init = quatro_hrt0_init,
        .class_init    = quatro_hrt0_class_init,
    };

    type_register_static(&tinfo);
}

type_init(quatro_hrt0_register_type)
