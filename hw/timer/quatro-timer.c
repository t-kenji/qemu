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

#define TYPE_QUATRO_RTC "quatro5500.rtc"
#define QUATRO_RTC(obj) OBJECT_CHECK(QuatroRTCState, (obj), TYPE_QUATRO_RTC)

#define TYPE_QUATRO_HRT0 "quatro5500.hrt0"
#define QUATRO_HRT0(obj) OBJECT_CHECK(QuatroHRT0State, (obj), TYPE_QUATRO_HRT0)

typedef struct {
    const char *name;
    hwaddr offset;
    uint32_t reset_value;
} QuatroTimerReg;

#define REG_ITEM(index, offset, reset_value) \
    [index] = {#index, (offset), (reset_value)}

enum QuatroRTCRegs {
    CNT,
    CTL,
    QUATRO_RTC_NUM_REGS
};

static const QuatroTimerReg quatro_rtc_regs[] = {
    REG_ITEM(CNT, 0x0010, 0x00000000),
    REG_ITEM(CTL, 0x001C, 0x00000000),
};

enum QuatroHRT0State {
    HRTPRE0,
    HRTCNT0H,
    HRTCNT0L,
    HRTCTL0,
    QUATRO_HRT0_NUM_REGS
};

static const QuatroTimerReg quatro_hrt0_regs[] = {
    REG_ITEM(HRTPRE0,  0x0000, 0x00000000),
    REG_ITEM(HRTCNT0H, 0x0004, 0x00000000),
    REG_ITEM(HRTCNT0L, 0x0008, 0x00000000),
    REG_ITEM(HRTCTL0,  0x000C, 0x00000000),
};

#undef REG_ITEM

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[QUATRO_RTC_NUM_REGS];
} QuatroRTCState;

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    ptimer_state *timer;
    uint32_t regs[QUATRO_HRT0_NUM_REGS];
} QuatroHRT0State;

static const VMStateDescription quatro_rtc_vmstate = {
    .name = TYPE_QUATRO_RTC,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_UINT32_ARRAY(regs, QuatroRTCState, QUATRO_RTC_NUM_REGS),
        VMSTATE_END_OF_LIST()
    },
};

static const VMStateDescription quatro_hrt0_vmstate = {
    .name = TYPE_QUATRO_HRT0,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_PTIMER(timer, QuatroHRT0State),
        VMSTATE_UINT32_ARRAY(regs, QuatroHRT0State, QUATRO_HRT0_NUM_REGS),
        VMSTATE_END_OF_LIST()
    },
};

static int quatro_timer_offset_to_index(const QuatroTimerReg *regs,
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

static uint64_t quatro_rtc_read(void *opaque, hwaddr offset, unsigned size)
{
    QuatroRTCState *s = QUATRO_RTC(opaque);
    int index = quatro_timer_offset_to_index(quatro_rtc_regs,
                                             QUATRO_RTC_NUM_REGS,
                                             offset);

    if (index < 0) {
        qemu_log("%s: Bad read offset 0x%" HWADDR_PRIx "\n",
                 TYPE_QUATRO_RTC, offset);
        return 0;
    }

    uint64_t value = s->regs[index];
    qemu_log("%s: read 0x%" PRIx64 " from offset 0x%" HWADDR_PRIx "\n",
             TYPE_QUATRO_RTC, value, offset);
    return value;
}

static void quatro_rtc_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    QuatroRTCState *s = QUATRO_RTC(opaque);
    int index = quatro_timer_offset_to_index(quatro_rtc_regs,
                                             QUATRO_RTC_NUM_REGS,
                                             offset);

    switch (index) {
    case CNT:
        s->regs[CNT] = (uint32_t)value;
        break;
    case CTL:
        s->regs[CTL] = (uint32_t)value;
        break;
    default:
        qemu_log("%s: Bad write offset 0x%" HWADDR_PRIx "\n",
                 TYPE_QUATRO_RTC, offset);
        return;
    }
    qemu_log("%s: write 0x%" PRIx64 " to offset 0x%" HWADDR_PRIx "\n",
             TYPE_QUATRO_RTC, value, offset);
}

static void quatro_rtc_reset(DeviceState *dev)
{
    QuatroRTCState *s = QUATRO_RTC(dev);

    for (int i = 0; i < QUATRO_RTC_NUM_REGS; ++i) {
        s->regs[i] = quatro_rtc_regs[i].reset_value;
    }
}

static void quatro_rtc_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = quatro_rtc_read,
        .write      = quatro_rtc_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
    };

    QuatroRTCState *s = QUATRO_RTC(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_QUATRO_RTC, 0x20);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void quatro_rtc_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->desc    = "CSR Quatro 5500 Real-time clock";
    dc->realize = quatro_rtc_realize;
    dc->reset   = quatro_rtc_reset;
    dc->vmsd    = &quatro_rtc_vmstate;
}

static uint64_t quatro_hrt0_read(void *opaque, hwaddr offset, unsigned size)
{
    QuatroHRT0State *s = QUATRO_HRT0(opaque);
    int index = quatro_timer_offset_to_index(quatro_hrt0_regs,
                                             QUATRO_HRT0_NUM_REGS,
                                             offset);

    if (index < 0) {
        qemu_log("%s: Bad read offset 0x%" HWADDR_PRIx "\n",
                 TYPE_QUATRO_HRT0, offset);
        return 0;
    }

    uint64_t value = s->regs[index];
    //qemu_log("%s: read offset 0x%" HWADDR_PRIx ", value: 0x%" PRIx32 "\n", TYPE_QUATRO_HRT0, offset, s->regs[i]);
    return value;
}

static void quatro_hrt0_write(void *opaque,
                             hwaddr offset,
                             uint64_t value,
                             unsigned size)
{
    QuatroHRT0State *s = QUATRO_HRT0(opaque);
    int index = quatro_timer_offset_to_index(quatro_hrt0_regs,
                                             QUATRO_HRT0_NUM_REGS,
                                             offset);

    switch (index) {
    case HRTPRE0:
        s->regs[HRTPRE0] = (uint32_t)value;
        break;
    case HRTCNT0H:
        s->regs[HRTCNT0H] = (uint32_t)value;
        break;
    case HRTCNT0L:
        s->regs[HRTCNT0L] = (uint32_t)value;
        break;
    case HRTCTL0:
        s->regs[HRTCTL0] = (uint32_t)value;
        break;
    default:
        qemu_log("%s: Bad write offset 0x%" HWADDR_PRIx "\n",
                 TYPE_QUATRO_HRT0, offset);
        return;
    }
    qemu_log("%s: write 0x%" PRIx64 " to offset 0x%" HWADDR_PRIx "\n",
             TYPE_QUATRO_HRT0, value, offset);
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

    QuatroHRT0State *s = QUATRO_HRT0(dev);

    s->timer = ptimer_init(NULL, PTIMER_POLICY_DEFAULT);
    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_QUATRO_HRT0, 0x10);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void quatro_hrt0_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->desc    = "CSR Quatro 5500 High-resolution timer";
    dc->realize = quatro_hrt0_realize;
    dc->reset   = quatro_hrt0_reset;
    dc->vmsd    = &quatro_hrt0_vmstate;
}

static void quatro_hrt0_register_types(void)
{
    static const TypeInfo rtc_tinfo = {
        .name          = TYPE_QUATRO_RTC,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroRTCState),
        .class_init    = quatro_rtc_class_init,
    };
    static const TypeInfo hrt0_tinfo = {
        .name          = TYPE_QUATRO_HRT0,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroHRT0State),
        .class_init    = quatro_hrt0_class_init,
    };

    type_register_static(&rtc_tinfo);
    type_register_static(&hrt0_tinfo);
}

type_init(quatro_hrt0_register_types)
