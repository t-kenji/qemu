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
#include "qemu/timer.h"
#include "qemu/bitops.h"
#include "qemu/log.h"

#define TYPE_QUATRO_CLK "quatro5500.clk"
#define QUATRO_CLK(obj) OBJECT_CHECK(QuatroClkState, (obj), TYPE_QUATRO_CLK)

#define TYPE_QUATRO_RTC "quatro5500.rtc"
#define QUATRO_RTC(obj) OBJECT_CHECK(QuatroRTCState, (obj), TYPE_QUATRO_RTC)

#define TYPE_QUATRO_HRT0 "quatro5500.hrt0"
#define QUATRO_HRT0(obj) OBJECT_CHECK(QuatroHRT0State, (obj), TYPE_QUATRO_HRT0)

enum QuatroTimerMemoryMap {
    QUATRO_CLK_MMIO_SIZE  = 0x10000,
    QUATRO_RTC_MMIO_SIZE  = 0x20,
    QUATRO_HRT0_MMIO_SIZE = 0x10,
};

enum QuatroClkSysPLLValues {
    SYSPLL_OUTPUT_FREQ = 2400000000,
    HED_CLOCK          = 300000000,
    SYSPLL_DIVIDER     = (SYSPLL_OUTPUT_FREQ / HED_CLOCK) - 1,
};

enum QuatroClkSysCGValues {
    CLKSTATSW1_SYS_IS_MUX_CLK  = 0x00040000,
    CLKSTATSW1_SYS_IS_LP_CLK   = 0x00020000,
    CLKSTATSW1_SYS_IS_XIN0_CLK = 0x00010000,
};

enum QuatroRTCValues {
    RTC_BUSY = 0x01,
};

enum QuatroHRT0Values {
    HRT_STOP  = 0x00000000,
    HRT_CLEAR = 0x00000001,
    HRT_START = 0x00000002,
};

typedef struct {
    union {
        uint32_t dword;
        uint16_t word[2];
        uint8_t byte[4];
    };
} Reg;

typedef struct {
    const char *name;
    hwaddr offset;
    uint32_t reset_value;
} QuatroTimerReg;

#define REG_ITEM(index, offset, reset_value) \
    [index] = {#index, (offset), (reset_value)}

enum QuatroClkRegs {
    SYSPLL_DIV12_0,
    SYSPLL_DIV12_1,
    SYSPLL_DIV12_2,
    SYSPLL_DIV12_3,
    SYSCG_CLKSTATSW1,
    SYSCG_CLKMUXCTRL1,
    SYSCG_CLKDIVCTRL0,
    SYSCG_CLKDIVCTRL1,

    QUATRO_CLK_NUM_REGS
};

static const QuatroTimerReg quatro_clk_regs[] = {
    REG_ITEM(SYSPLL_DIV12_0,    0x0018, 0x00000000),
    REG_ITEM(SYSPLL_DIV12_1,    0x001C, 0x00000000),
    REG_ITEM(SYSPLL_DIV12_2,    0x0020, 0x00000000),
    REG_ITEM(SYSPLL_DIV12_3,    0x0024, SYSPLL_DIVIDER),
    REG_ITEM(SYSCG_CLKSTATSW1,  0x0414, CLKSTATSW1_SYS_IS_MUX_CLK),
    REG_ITEM(SYSCG_CLKMUXCTRL1, 0x0430, 0x00000003),
    REG_ITEM(SYSCG_CLKDIVCTRL0, 0x0458, 0x00000001),
    REG_ITEM(SYSCG_CLKDIVCTRL1, 0x045C, 0x00000001),
};

enum QuatroRTCRegs {
    RTC_CNT,
    RTC_CTL,
    QUATRO_RTC_NUM_REGS
};

static const QuatroTimerReg quatro_rtc_regs[] = {
    REG_ITEM(RTC_CNT, 0x0010, 0x00000000),
    REG_ITEM(RTC_CTL, 0x001C, 0x00000004),
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
    uint32_t regs[QUATRO_CLK_NUM_REGS];
} QuatroClkState;

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
    uint64_t counter_offset;
    uint32_t regs[QUATRO_HRT0_NUM_REGS];
} QuatroHRT0State;

static const VMStateDescription quatro_clk_vmstate = {
    .name = TYPE_QUATRO_CLK,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_UINT32_ARRAY(regs, QuatroClkState, QUATRO_CLK_NUM_REGS),
        VMSTATE_END_OF_LIST()
    },
};

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
        VMSTATE_UINT64(counter_offset, QuatroHRT0State),
        VMSTATE_UINT32_ARRAY(regs, QuatroHRT0State, QUATRO_HRT0_NUM_REGS),
        VMSTATE_END_OF_LIST()
    },
};

static inline int64_t quatro_sec_to_nsec(int64_t sec)
{
    return sec * 1000000000;
}

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

static uint64_t quatro_hrt_get_count(QuatroHRT0State *s)
{
    return qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL)
           / (quatro_sec_to_nsec(1) / HED_CLOCK);
}

static uint64_t quatro_clk_read(void *opaque, hwaddr offset, unsigned size)
{
    const QuatroClkState *s = QUATRO_CLK(opaque);
    int index = quatro_timer_offset_to_index(quatro_clk_regs,
                                             QUATRO_CLK_NUM_REGS,
                                             offset);

    if (index < 0) {
        qemu_log("%s: Bad read offset 0x%" HWADDR_PRIx "\n",
                 TYPE_QUATRO_CLK, offset);
        return 0;
    }

    uint64_t value = s->regs[index];
    qemu_log("%s: read 0x%" PRIx64 " from offset 0x%" HWADDR_PRIx "\n",
             TYPE_QUATRO_CLK, value, offset);
    return value;
}

static void quatro_clk_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    QuatroClkState *s = QUATRO_CLK(opaque);
    int index = quatro_timer_offset_to_index(quatro_clk_regs,
                                             QUATRO_CLK_NUM_REGS,
                                             offset);

    switch (index) {
    case SYSPLL_DIV12_0:
        s->regs[SYSPLL_DIV12_0] = (uint32_t)value;
        break;
    case SYSPLL_DIV12_1:
        s->regs[SYSPLL_DIV12_1] = (uint32_t)value;
        break;
    case SYSPLL_DIV12_2:
        s->regs[SYSPLL_DIV12_2] = (uint32_t)value;
        break;
    case SYSPLL_DIV12_3:
        s->regs[SYSPLL_DIV12_3] = (uint32_t)value;
        break;
    case SYSCG_CLKSTATSW1:
        s->regs[SYSCG_CLKSTATSW1] = (uint32_t)value;
        break;
    case SYSCG_CLKMUXCTRL1:
        s->regs[SYSCG_CLKMUXCTRL1] = (uint32_t)value;
        break;
    case SYSCG_CLKDIVCTRL0:
        s->regs[SYSCG_CLKDIVCTRL0] = (uint32_t)value;
        break;
    case SYSCG_CLKDIVCTRL1:
        s->regs[SYSCG_CLKDIVCTRL1] = (uint32_t)value;
        break;
    default:
        qemu_log("%s: Bad write offset 0x%" HWADDR_PRIx "\n",
                 TYPE_QUATRO_CLK, offset);
        return;
    }
    qemu_log("%s: write 0x%" PRIx64 " to offset 0x%" HWADDR_PRIx "\n",
             TYPE_QUATRO_CLK, value, offset);
}

static void quatro_clk_reset(DeviceState *dev)
{
    QuatroClkState *s = QUATRO_CLK(dev);

    for (int i = 0; i < QUATRO_CLK_NUM_REGS; ++i) {
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
    int index = quatro_timer_offset_to_index(quatro_rtc_regs,
                                             QUATRO_RTC_NUM_REGS,
                                             offset);

    if (index < 0) {
        qemu_log("%s: Bad read offset 0x%" HWADDR_PRIx "\n",
                 TYPE_QUATRO_RTC, offset);
        return 0;
    }

    uint64_t value = s->regs[index];
    switch (index) {
    case RTC_CTL: {
            s->regs[RTC_CTL] &= ~RTC_BUSY;
        }
        break;
    };
    qemu_log("%s: read 0x%" PRIx64 " from offset 0x%" HWADDR_PRIx "\n",
             TYPE_QUATRO_RTC, value, offset);
    return value;
}

static void quatro_rtc_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    QuatroRTCState *s = QUATRO_RTC(opaque);
    int byte = offset % sizeof(uint32_t);
    offset -= byte;
    int index = quatro_timer_offset_to_index(quatro_rtc_regs,
                                             QUATRO_RTC_NUM_REGS,
                                             offset);

    switch (index) {
    case RTC_CNT:
        s->regs[RTC_CNT] = (uint32_t)value;
        break;
    case RTC_CTL: {
            Reg *reg = (Reg *)&s->regs[RTC_CTL];
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
            }
            qemu_log("%s: CTL: byte %d, size %d, 0x%08" PRIx32 "\n",
                     TYPE_QUATRO_RTC, byte, size, s->regs[RTC_CTL]);
        }
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
    int index = quatro_timer_offset_to_index(quatro_hrt0_regs,
                                             QUATRO_HRT0_NUM_REGS,
                                             offset);

    if (index < 0) {
        qemu_log("%s: Bad read offset 0x%" HWADDR_PRIx "\n",
                 TYPE_QUATRO_HRT0, offset);
        return 0;
    }

    switch (index) {
    case HRTCNT0H ... HRTCNT0L: {
            uint64_t counter = quatro_hrt_get_count(s) - s->counter_offset;
            s->regs[HRTCNT0H] = (uint32_t)extract64(counter, 32, 32);
            s->regs[HRTCNT0L] = (uint32_t)extract64(counter, 0, 32);
        }
        break;
    }
    uint64_t value = s->regs[index];
    //qemu_log("%s: read 0x%" PRIX64 " from offset 0x%" HWADDR_PRIx "\n",
    //         TYPE_QUATRO_HRT0, value, offset);
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
        switch (value) {
        case HRT_STOP:
            break;
        case HRT_CLEAR:
            s->regs[HRTCNT0H] = s->regs[HRTCNT0L] = 0;
            break;
        case HRT_START:
            s->counter_offset = quatro_hrt_get_count(s);
            break;
        default:
            qemu_log("%s: Bad write 0x%" PRIx64 " to offset 0x%" HWADDR_PRIx "\n",
                     TYPE_QUATRO_HRT0, value, offset);
            return;
        }
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

    s->counter_offset = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
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

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s, TYPE_QUATRO_HRT0,
                          QUATRO_HRT0_MMIO_SIZE);
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
