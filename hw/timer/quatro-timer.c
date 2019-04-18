/*
 *  CSR Quatro 5500 Clocks emulation
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
#include "hw/misc/gen-reg.h"
#include "qemu/timer.h"
#include "qemu/bitops.h"
#include "qemu/log.h"

#define ENABLE_DEBUG

#define TYPE_QUATRO_CLK "quatro5500-clk"
#define QUATRO_CLK(obj) OBJECT_CHECK(QuatroClkState, (obj), TYPE_QUATRO_CLK)

#define TYPE_QUATRO_RTC "quatro5500-rtc"
#define QUATRO_RTC(obj) OBJECT_CHECK(QuatroRTCState, (obj), TYPE_QUATRO_RTC)

#define TYPE_QUATRO_HRT0 "quatro5500-hrt0"
#define QUATRO_HRT0(obj) OBJECT_CHECK(QuatroHRT0State, (obj), TYPE_QUATRO_HRT0)

#if defined(ENABLE_DEBUG)
#define DEBUG(type, format, ...)                     \
    do {                                             \
        qemu_log("%s: " format "\n",                 \
                 TYPE_QUATRO_##type, ##__VA_ARGS__); \
    } while (0)
#else
#define DEBUG(type, format, ...) \
    do {} while (0)
#endif
#define ERROR(type, format, ...)                           \
    do {                                                   \
        qemu_log_mask(LOG_GUEST_ERROR, "%s: " format "\n", \
                      TYPE_QUATRO_##type, ##__VA_ARGS__);  \
    } while (0)

enum QuatroTimerMemoryMap {
    QUATRO_CLK_MMIO_SIZE  = 0x10000,
    QUATRO_RTC_MMIO_SIZE  = 0x20,
    QUATRO_HRT0_MMIO_SIZE = 0x10,
};

enum {
    SYSPLL_OUTPUT_FREQ = 2400000000,
    SYSPLL_HED_CLOCK   = 300000000,
    SYSPLL_DIVIDER     = (SYSPLL_OUTPUT_FREQ / SYSPLL_HED_CLOCK) - 1,

    SYSCG_CLKSTATSW1__SYS_IS_MUX_CLK  = 0x00040000,
    SYSCG_CLKSTATSW1__SYS_IS_LP_CLK   = 0x00020000,
    SYSCG_CLKSTATSW1__SYS_IS_XIN0_CLK = 0x00010000,

    RTC_CTL__RTC_BUSY = 0x01,

    HRTCTL__HRT_STOP  = 0x00000000,
    HRTCTL__HRT_CLEAR = 0x00000001,
    HRTCTL__HRT_START = 0x00000002,
};

typedef struct {
    union {
        uint32_t dword;
        uint16_t word[2];
        uint8_t byte[4];
    };
} UniReg;

enum {
    SYSPLL_DIV12_0,
    SYSPLL_DIV12_1,
    SYSPLL_DIV12_2,
    SYSPLL_DIV12_3,
    SYSCG_CLKSTATSW1,
    SYSCG_CLKMUXCTRL1,
    SYSCG_CLKDIVCTRL0,
    SYSCG_CLKDIVCTRL1,
};

static const RegDef32 quatro_clk_regs[] = {
    REG_ITEM(SYSPLL_DIV12_0,    0x0018, 0x00000000,                       0xFFFFFFFF),
    REG_ITEM(SYSPLL_DIV12_1,    0x001C, 0x00000000,                       0xFFFFFFFF),
    REG_ITEM(SYSPLL_DIV12_2,    0x0020, 0x00000000,                       0xFFFFFFFF),
    REG_ITEM(SYSPLL_DIV12_3,    0x0024, SYSPLL_DIVIDER,                   0xFFFFFFFF),
    REG_ITEM(SYSCG_CLKSTATSW1,  0x0414, SYSCG_CLKSTATSW1__SYS_IS_MUX_CLK, 0xFFFFFFFF),
    REG_ITEM(SYSCG_CLKMUXCTRL1, 0x0430, 0x00000003,                       0xFFFFFFFF),
    REG_ITEM(SYSCG_CLKDIVCTRL0, 0x0458, 0x00000001,                       0xFFFFFFFF),
    REG_ITEM(SYSCG_CLKDIVCTRL1, 0x045C, 0x00000001,                       0xFFFFFFFF),
};

enum {
    RTC_CNT,
    RTC_CTL,
};

static const RegDef32 quatro_rtc_regs[] = {
    REG_ITEM(RTC_CNT, 0x0010, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(RTC_CTL, 0x001C, 0x00000004, 0xFFFFFFFF),
};

enum {
    HRTPRE0,
    HRTCNT0H,
    HRTCNT0L,
    HRTCTL0,
};

static const RegDef32 quatro_hrt0_regs[] = {
    REG_ITEM(HRTPRE0,  0x0000, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(HRTCNT0H, 0x0004, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(HRTCNT0L, 0x0008, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(HRTCTL0,  0x000C, 0x00000000, 0xFFFFFFFF),
};

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem;
    uint32_t regs[ARRAY_SIZE(quatro_clk_regs)];
} QuatroClkState;

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem;
    uint32_t regs[ARRAY_SIZE(quatro_rtc_regs)];
} QuatroRTCState;

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem;
    uint64_t counter_offset;
    uint32_t regs[ARRAY_SIZE(quatro_hrt0_regs)];
} QuatroHRT0State;

static const VMStateDescription quatro_clk_vmstate = {
    .name = TYPE_QUATRO_CLK,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_UINT32_ARRAY(regs, QuatroClkState, ARRAY_SIZE(quatro_clk_regs)),
        VMSTATE_END_OF_LIST()
    },
};

static const VMStateDescription quatro_rtc_vmstate = {
    .name = TYPE_QUATRO_RTC,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_UINT32_ARRAY(regs, QuatroRTCState, ARRAY_SIZE(quatro_rtc_regs)),
        VMSTATE_END_OF_LIST()
    },
};

static const VMStateDescription quatro_hrt0_vmstate = {
    .name = TYPE_QUATRO_HRT0,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_UINT64(counter_offset, QuatroHRT0State),
        VMSTATE_UINT32_ARRAY(regs, QuatroHRT0State,
                             ARRAY_SIZE(quatro_hrt0_regs)),
        VMSTATE_END_OF_LIST()
    },
};

static uint64_t hrt_get_count(void)
{
    return qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) / 1000;
}

static uint64_t quatro_clk_read(void *opaque, hwaddr offset, unsigned size)
{
    const QuatroClkState *s = QUATRO_CLK(opaque);
    RegDef32 reg = regdef_find(quatro_clk_regs, offset);

    if (reg.index < 0) {
        ERROR(CLK, "Bad read offset %#" HWADDR_PRIx, offset);
        return 0;
    }

    uint64_t value = s->regs[reg.index];
    DEBUG(CLK, "Read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")",
          value, reg.name, offset);
    return value;
}

static void quatro_clk_write(void *opaque,
                             hwaddr offset,
                             uint64_t value,
                             unsigned size)
{
    QuatroClkState *s = QUATRO_CLK(opaque);
    RegDef32 reg = regdef_find(quatro_clk_regs, offset);

    if (reg.index < 0) {
        ERROR(CLK, "Bad write offset %#" HWADDR_PRIx, offset);
        return;
    }

    DEBUG(CLK, "Write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
          value, reg.name, offset);
    if (!!(value & ~reg.write_mask)) {
        ERROR(CLK, "Maybe write to a read only bit %#" PRIx64,
              (value & ~reg.write_mask));
    }

    switch (reg.index) {
    default:
        s->regs[reg.index] = (uint32_t)value;
        break;
    }
}

static void quatro_clk_reset(DeviceState *dev)
{
    QuatroClkState *s = QUATRO_CLK(dev);

    for (int i = 0; i < ARRAY_SIZE(s->regs); ++i) {
        s->regs[i] = quatro_clk_regs[i].reset_value;
    }
}

static void quatro_clk_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read = quatro_clk_read,
        .write = quatro_clk_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
    };

    QuatroClkState *s = QUATRO_CLK(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s, TYPE_QUATRO_CLK,
                          QUATRO_CLK_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void quatro_clk_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->desc = "CSR Quatro 5500 Clock Control register";
    dc->realize = quatro_clk_realize;
    dc->reset = quatro_clk_reset;
    dc->vmsd = &quatro_clk_vmstate;
}

static uint64_t quatro_rtc_read(void *opaque, hwaddr offset, unsigned size)
{
    QuatroRTCState *s = QUATRO_RTC(opaque);
    int byte = offset % sizeof(uint32_t);
    offset -= byte;
    RegDef32 reg = regdef_find(quatro_rtc_regs, offset);

    if (reg.index < 0) {
        ERROR(RTC, "Bad read offset %#" HWADDR_PRIx, offset);
        return 0;
    }

    uint64_t value = s->regs[reg.index];
    switch (reg.index) {
    case RTC_CNT:
        value = time(NULL);
        break;
    case RTC_CTL:
        s->regs[RTC_CTL] &= ~RTC_CTL__RTC_BUSY;
        break;
    };
    DEBUG(RTC, "Read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")",
          value, reg.name, offset);
    return value;
}

static void quatro_rtc_write(void *opaque,
                             hwaddr offset,
                             uint64_t value,
                             unsigned size)
{
    QuatroRTCState *s = QUATRO_RTC(opaque);
    int byte = offset % sizeof(uint32_t);
    offset -= byte;
    RegDef32 reg = regdef_find(quatro_rtc_regs, offset);

    if (reg.index < 0) {
        ERROR(RTC, "Bad write offset %#" HWADDR_PRIx, offset);
        return;
    }

    DEBUG(RTC, "Write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
          value, reg.name, offset);
    if (!!(value & ~reg.write_mask)) {
        ERROR(RTC, "Maybe write to a read only bit %#" PRIx64,
              (value & ~reg.write_mask));
    }

    switch (reg.index) {
    case RTC_CTL: {
            UniReg *reg = (UniReg *)&s->regs[RTC_CTL];
            switch (size) {
            case 1:
                reg->byte[byte] = (uint8_t)value;
                break;
            case 2:
                reg->word[byte / 2] = (uint16_t)value;
                break;
            case 4:
                reg->dword = (uint32_t)value;
                break;
            default:
                break;
            }
        }
        break;
    default:
        s->regs[reg.index] = (uint32_t)value;
        break;
    }
}

static void quatro_rtc_reset(DeviceState *dev)
{
    QuatroRTCState *s = QUATRO_RTC(dev);

    for (int i = 0; i < ARRAY_SIZE(s->regs); ++i) {
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

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s, TYPE_QUATRO_RTC,
                          QUATRO_RTC_MMIO_SIZE);
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
    RegDef32 reg = regdef_find(quatro_hrt0_regs, offset);

    if (reg.index < 0) {
        ERROR(HRT0, "Bad read offset %#" HWADDR_PRIx, offset);
        return 0;
    }

    switch (reg.index) {
    case HRTCNT0H ... HRTCNT0L:
        {
            uint64_t counter = hrt_get_count() - s->counter_offset;
            s->regs[HRTCNT0H] = (uint32_t)extract64(counter, 32, 32);
            s->regs[HRTCNT0L] = (uint32_t)extract64(counter, 0, 32);
        }
        break;
    }
    uint64_t value = s->regs[reg.index];
    return value;
}

static void quatro_hrt0_write(void *opaque,
                             hwaddr offset,
                             uint64_t value,
                             unsigned size)
{
    QuatroHRT0State *s = QUATRO_HRT0(opaque);
    RegDef32 reg = regdef_find(quatro_hrt0_regs, offset);

    if (reg.index < 0) {
        ERROR(HRT0, "Bad write offset %#" HWADDR_PRIx, offset);
        return;
    }

    DEBUG(HRT0, "Write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
          value, reg.name, offset);
    if (!!(value & ~reg.write_mask)) {
        ERROR(HRT0, "Maybe write to a read only bit %#" PRIx64,
              (value & ~reg.write_mask));
    }

    switch (reg.index) {
    case HRTCTL0:
        s->regs[HRTCTL0] = (uint32_t)value;
        switch (value) {
        case HRTCTL__HRT_STOP:
            break;
        case HRTCTL__HRT_CLEAR:
            s->regs[HRTCNT0H] = s->regs[HRTCNT0L] = 0;
            break;
        case HRTCTL__HRT_START:
            s->counter_offset = hrt_get_count();
            break;
        default:
            ERROR(HRT0, "Bad control value %#" PRIx64, value);
            return;
        }
        break;
    default:
        s->regs[reg.index] = (uint32_t)value;
        break;
    }
}

static void quatro_hrt0_reset(DeviceState *dev)
{
    QuatroHRT0State *s = QUATRO_HRT0(dev);

    s->counter_offset = 0;
    for (int i = 0; i < ARRAY_SIZE(s->regs); ++i) {
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

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s, TYPE_QUATRO_HRT0,
                          QUATRO_HRT0_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void quatro_hrt0_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->desc    = "CSR Quatro 5500 High-resolution timer 0";
    dc->realize = quatro_hrt0_realize;
    dc->reset   = quatro_hrt0_reset;
    dc->vmsd    = &quatro_hrt0_vmstate;
}

static void quatro_timer_register_types(void)
{
    static const TypeInfo clk_tinfo = {
        .name = TYPE_QUATRO_CLK,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroClkState),
        .class_init = quatro_clk_class_init,
    };
    static const TypeInfo rtc_tinfo = {
        .name = TYPE_QUATRO_RTC,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroRTCState),
        .class_init = quatro_rtc_class_init,
    };
    static const TypeInfo hrt0_tinfo = {
        .name = TYPE_QUATRO_HRT0,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroHRT0State),
        .class_init = quatro_hrt0_class_init,
    };

    type_register_static(&clk_tinfo);
    type_register_static(&rtc_tinfo);
    type_register_static(&hrt0_tinfo);
}

type_init(quatro_timer_register_types)
