/*
 * CSR Quatro 5500 peripherals emulation
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

//#define ENABLE_DEBUG

#define TYPE_QUATRO_A15GPF "quatro5500.a15gpf"
#define QUATRO_A15GPF(obj) OBJECT_CHECK(QuatroA15GPFState, (obj), TYPE_QUATRO_A15GPF)

#define TYPE_QUATRO_RSTGEN "quatro5500.rstgen"
#define QUATRO_RSTGEN(obj) OBJECT_CHECK(QuatroRstGenState, (obj), TYPE_QUATRO_RSTGEN)

#define TYPE_QUATRO_DDRMC "quatro5500.ddrmc"
#define QUATRO_DDRMC(obj) OBJECT_CHECK(QuatroDDRMCState, (obj), TYPE_QUATRO_DDRMC)

#define TYPE_QUATRO_SDIOCORE "quatro5500.sdiocore"
#define QUATRO_SDIOCORE(obj) OBJECT_CHECK(QuatroSDIOCoreState, (obj), TYPE_QUATRO_SDIOCORE)

#define TYPE_QUATRO_SDMCLK "quatro5500.sdmclk"
#define QUATRO_SDMCLK(obj) OBJECT_CHECK(QuatroSDMClkState, (obj), TYPE_QUATRO_SDMCLK)

#define TYPE_QUATRO_TTC "quatro5500.ttc"
#define QUATRO_TTC(obj) OBJECT_CHECK(QuatroTTCState, (obj), TYPE_QUATRO_TTC)

#define TYPE_QUATRO_SBE "quatro5500.sbe"
#define QUATRO_SBE(obj) OBJECT_CHECK(QuatroSBEState, (obj), TYPE_QUATRO_SBE)

#define TYPE_QUATRO_FIR "quatro5500.fir"
#define QUATRO_FIR(obj) OBJECT_CHECK(QuatroFIRState, (obj), TYPE_QUATRO_FIR)

#define TYPE_QUATRO_SCAL "quatro5500.scal"
#define QUATRO_SCAL(obj) OBJECT_CHECK(QuatroSCALState, (obj), TYPE_QUATRO_SCAL)

#define TYPE_QUATRO_SCRN "quatro5500.scrn"
#define QUATRO_SCRN(obj) OBJECT_CHECK(QuatroSCRNState, (obj), TYPE_QUATRO_SCRN)

#define TYPE_QUATRO_JBIG "quatro5500.jbig"
#define QUATRO_JBIG(obj) OBJECT_CHECK(QuatroJBIGState, (obj), TYPE_QUATRO_JBIG)

#define TYPE_QUATRO_LCDC "quatro5500.lcdc"
#define QUATRO_LCDC(obj) OBJECT_CHECK(QuatroLCDCState, (obj), TYPE_QUATRO_LCDC)

#define TYPE_QUATRO_DSP "quatro5500.dsp"
#define QUATRO_DSP(obj) OBJECT_CHECK(QuatroDSPState, (obj), TYPE_QUATRO_DSP)

enum QuatroPeripheralMemoryMap {
    QUATRO_PERI_A15GPF_MMIO_SIZE   = 0x10000,
    QUATRO_PERI_RSTGEN_MMIO_SIZE   = 0x10000,
    QUATRO_PERI_DDRMC_MMIO_SIZE    = 0x10000,
    QUATRO_PERI_SDIOCORE_MMIO_SIZE = 0x100,
    QUATRO_PERI_SDMCLK_MMIO_SIZE   = 0x10000,
    QUATRO_PERI_TTC_MMIO_SIZE      = 0x20000,
    QUATRO_PERI_SBE_MMIO_SIZE      = 0x10000,
    QUATRO_PERI_FIR_MMIO_SIZE      = 0x10000,
    QUATRO_PERI_SCAL_MMIO_SIZE     = 0x10000,
    QUATRO_PERI_SCRN_MMIO_SIZE     = 0x10000,
    QUATRO_PERI_JBIG_MMIO_SIZE     = 0x10000,
    QUATRO_PERI_LCDC_MMIO_SIZE     = 0x10000,
    QUATRO_PERI_DSP_MMIO_SIZE      = 0x100000,
};

typedef struct {
    const char *name;
    hwaddr offset;
    uint32_t reset_value;
} QuatroPeriReg;

#define REG_ITEM(index, offset, reset_value) \
    [index] = {#index, (offset), (reset_value)}

enum QuatroA15GPFRegs {
    A15RST,
    A15EVA0,
    A15EVA1,
    QUATRO_A15GPF_NUM_REGS
};

static const QuatroPeriReg quatro_a15gpf_regs[] = {
    REG_ITEM(A15RST,  0x0024, 0x00000000),
    REG_ITEM(A15EVA0, 0x0040, 0x00000000),
    REG_ITEM(A15EVA1, 0x0044, 0x00000000),
};

enum QuatroRstGenRegs {
    PAD_INTERNAL,
    POWER_CTRL,
    POWER_STAT,
    POWER_ISO,
    QUATRO_RSTGEN_NUM_REGS
};

static const QuatroPeriReg quatro_rstgen_regs[] = {
    REG_ITEM(PAD_INTERNAL, 0x0024, 0x00000040),
    REG_ITEM(POWER_ISO,    0x0160, 0x00000000),
    REG_ITEM(POWER_CTRL,   0x0164, 0x00000000),
    REG_ITEM(POWER_STAT,   0x0168, 0x00000000),
};

enum QuatroDDRMCRegs {
    EXT_ADDR_MODE,
    QUATRO_DDRMC_NUM_REGS
};

static const QuatroPeriReg quatro_ddrmc_regs[] = {
    REG_ITEM(EXT_ADDR_MODE, 0x4880, 0x00000000),
};

enum QuatroSDIOCoreRegs {
    SDIO0_HRS0,
    SDIO0_HRS1,
    SDIO0_HRS2,
    SDIO0_HRS44_0,
    SDIO0_HRS44_1,
    QUATRO_SDIOCORE_NUM_REGS
};

static const QuatroPeriReg quatro_sdiocore_regs[] = {
    REG_ITEM(SDIO0_HRS0,    0x0000, 0x00000000),
    REG_ITEM(SDIO0_HRS1,    0x0004, 0x00000000),
    REG_ITEM(SDIO0_HRS2,    0x0008, 0x00000000),
    REG_ITEM(SDIO0_HRS44_0, 0x00B0, 0x00000000),
    REG_ITEM(SDIO0_HRS44_1, 0x00B4, 0x00000000),
};

enum QuatroSDMClkRegs {
    CLKDISCTRL,
    CLKDISSTAT,
    SDIO0_EXTCTL,
    SDIO1_EXTCTL,
    QUATRO_SDMCLK_NUM_REGS
};

static const QuatroPeriReg quatro_sdmclk_regs[] = {
    REG_ITEM(CLKDISCTRL,   0x01D8, 0x00000000),
    REG_ITEM(CLKDISSTAT,   0x01DC, 0x00000003),
    REG_ITEM(SDIO0_EXTCTL, 0x0280, 0x00002000),
    REG_ITEM(SDIO1_EXTCTL, 0x0284, 0x00002000),
};

#undef REG_ITEM

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[QUATRO_A15GPF_NUM_REGS];
} QuatroA15GPFState;

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[QUATRO_RSTGEN_NUM_REGS];
} QuatroRstGenState;

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[QUATRO_DDRMC_NUM_REGS];
} QuatroDDRMCState;

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[QUATRO_SDIOCORE_NUM_REGS];
} QuatroSDIOCoreState;

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t regs[QUATRO_SDMCLK_NUM_REGS];
} QuatroSDMClkState;

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
} QuatroTTCState;

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
} QuatroSBEState;

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
} QuatroFIRState;

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
} QuatroSCALState;

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
} QuatroSCRNState;

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
} QuatroJBIGState;

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
} QuatroLCDCState;

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
} QuatroDSPState;

static const VMStateDescription quatro_a15gpf_vmstate = {
    .name = TYPE_QUATRO_A15GPF,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_UINT32_ARRAY(regs, QuatroA15GPFState, QUATRO_A15GPF_NUM_REGS),
        VMSTATE_END_OF_LIST()
    },
};

static const VMStateDescription quatro_rstgen_vmstate = {
    .name = TYPE_QUATRO_RSTGEN,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_UINT32_ARRAY(regs, QuatroRstGenState, QUATRO_RSTGEN_NUM_REGS),
        VMSTATE_END_OF_LIST()
    },
};

static const VMStateDescription quatro_ddrmc_vmstate = {
    .name = TYPE_QUATRO_DDRMC,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_UINT32_ARRAY(regs, QuatroDDRMCState, QUATRO_DDRMC_NUM_REGS),
        VMSTATE_END_OF_LIST()
    },
};

static const VMStateDescription quatro_sdiocore_vmstate = {
    .name = TYPE_QUATRO_SDIOCORE,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_UINT32_ARRAY(regs, QuatroSDIOCoreState, QUATRO_SDIOCORE_NUM_REGS),
        VMSTATE_END_OF_LIST()
    },
};

static const VMStateDescription quatro_sdmclk_vmstate = {
    .name = TYPE_QUATRO_SDMCLK,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_UINT32_ARRAY(regs, QuatroSDMClkState, QUATRO_SDMCLK_NUM_REGS),
        VMSTATE_END_OF_LIST()
    },
};

static const VMStateDescription quatro_ttc_vmstate = {
    .name = TYPE_QUATRO_TTC,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_END_OF_LIST()
    },
};

static const VMStateDescription quatro_sbe_vmstate = {
    .name = TYPE_QUATRO_SBE,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_END_OF_LIST()
    },
};

static const VMStateDescription quatro_fir_vmstate = {
    .name = TYPE_QUATRO_FIR,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_END_OF_LIST()
    },
};

static const VMStateDescription quatro_scal_vmstate = {
    .name = TYPE_QUATRO_SCAL,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_END_OF_LIST()
    },
};

static const VMStateDescription quatro_scrn_vmstate = {
    .name = TYPE_QUATRO_SCRN,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_END_OF_LIST()
    },
};

static const VMStateDescription quatro_jbig_vmstate = {
    .name = TYPE_QUATRO_JBIG,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_END_OF_LIST()
    },
};

static const VMStateDescription quatro_lcdc_vmstate = {
    .name = TYPE_QUATRO_LCDC,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_END_OF_LIST()
    },
};

static const VMStateDescription quatro_dsp_vmstate = {
    .name = TYPE_QUATRO_DSP,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_END_OF_LIST()
    },
};

#if defined(ENABLE_DEBUG)
#define DEBUGLOG(format, ...)                 \
    do {                                      \
        qemu_log(format "\n", ##__VA_ARGS__); \
    } while (0)
#else
#define DEBUGLOG(format, ...)
#endif
#define ERRORLOG(format, ...)                                       \
    do {                                                            \
        qemu_log_mask(LOG_GUEST_ERROR, format "\n", ##__VA_ARGS__); \
    } while (0)

static int quatro_peripheral_offset_to_index(const QuatroPeriReg *regs,
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

static uint64_t quatro_a15gpf_read(void *opaque, hwaddr offset, unsigned size)
{
    QuatroA15GPFState *s = QUATRO_A15GPF(opaque);
    int index = quatro_peripheral_offset_to_index(quatro_a15gpf_regs,
                                                  QUATRO_A15GPF_NUM_REGS,
                                                  offset);

    if (index < 0) {
        ERRORLOG("%s: Bad read offset %#" HWADDR_PRIx,
                 TYPE_QUATRO_A15GPF, offset);
        return 0;
    }

    uint64_t value = s->regs[index];
    DEBUGLOG("%s: read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")",
             TYPE_QUATRO_A15GPF, value, quatro_a15gpf_regs[index].name, offset);
    return value;
}

static void quatro_a15gpf_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    QuatroA15GPFState *s = QUATRO_A15GPF(opaque);
    int index = quatro_peripheral_offset_to_index(quatro_a15gpf_regs,
                                                  QUATRO_A15GPF_NUM_REGS,
                                                  offset);

    switch (index) {
    case A15RST:
        s->regs[A15RST] = (uint32_t)value;
        break;
    case A15EVA0:
        s->regs[A15EVA0] = (uint32_t)value;
        break;
    case A15EVA1:
        s->regs[A15EVA1] = (uint32_t)value;
        break;
    default:
        ERRORLOG("%s: Bad write offset %#" HWADDR_PRIx,
                 TYPE_QUATRO_A15GPF, offset);
        return;
    }
    DEBUGLOG("%s: write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
             TYPE_QUATRO_A15GPF, value, quatro_a15gpf_regs[index].name, offset);
}

static void quatro_a15gpf_reset(DeviceState *dev)
{
    QuatroA15GPFState *s = QUATRO_A15GPF(dev);

    for (int i = 0; i < QUATRO_A15GPF_NUM_REGS; ++i) {
        s->regs[i] = quatro_a15gpf_regs[i].reset_value;
    }
}

static void quatro_a15gpf_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = quatro_a15gpf_read,
        .write      = quatro_a15gpf_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
    };

    QuatroA15GPFState *s = QUATRO_A15GPF(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_QUATRO_A15GPF, QUATRO_PERI_A15GPF_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void quatro_a15gpf_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = quatro_a15gpf_realize;
    dc->reset   = quatro_a15gpf_reset;
    dc->vmsd    = &quatro_a15gpf_vmstate;
}

static uint64_t quatro_rstgen_read(void *opaque, hwaddr offset, unsigned size)
{
    QuatroRstGenState *s = QUATRO_RSTGEN(opaque);
    int index = quatro_peripheral_offset_to_index(quatro_rstgen_regs,
                                                  QUATRO_RSTGEN_NUM_REGS,
                                                  offset);

    if (index < 0) {
        ERRORLOG("%s: Bad read offset %#" HWADDR_PRIx,
                 TYPE_QUATRO_RSTGEN, offset);
        return 0;
    }

    uint64_t value = s->regs[index];
    DEBUGLOG("%s: read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")",
             TYPE_QUATRO_RSTGEN, value, quatro_rstgen_regs[index].name, offset);
    return value;
}

static void quatro_rstgen_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    QuatroRstGenState *s = QUATRO_RSTGEN(opaque);
    int index = quatro_peripheral_offset_to_index(quatro_rstgen_regs,
                                                  QUATRO_RSTGEN_NUM_REGS,
                                                  offset);

    switch (index) {
    case PAD_INTERNAL:
        s->regs[PAD_INTERNAL] = (uint32_t)value;
        break;
    case POWER_ISO:
        s->regs[POWER_ISO] = (uint32_t)value;
        break;
    case POWER_CTRL:
        s->regs[POWER_CTRL] = (uint32_t)value;
        break;
    case POWER_STAT:
        s->regs[POWER_STAT] = (uint32_t)value;
        break;
    default:
        ERRORLOG("%s: Bad write offset %#" HWADDR_PRIx,
                 TYPE_QUATRO_RSTGEN, offset);
        return;
    }
    DEBUGLOG("%s: write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
             TYPE_QUATRO_RSTGEN, value, quatro_rstgen_regs[index].name, offset);
}

static void quatro_rstgen_reset(DeviceState *dev)
{
    QuatroRstGenState *s = QUATRO_RSTGEN(dev);

    for (int i = 0; i < QUATRO_RSTGEN_NUM_REGS; ++i) {
        s->regs[i] = quatro_rstgen_regs[i].reset_value;
    }
}

static void quatro_rstgen_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = quatro_rstgen_read,
        .write      = quatro_rstgen_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
    };

    QuatroRstGenState *s = QUATRO_RSTGEN(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_QUATRO_RSTGEN, QUATRO_PERI_RSTGEN_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void quatro_rstgen_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = quatro_rstgen_realize;
    dc->reset   = quatro_rstgen_reset;
    dc->vmsd    = &quatro_rstgen_vmstate;
}

static uint64_t quatro_ddrmc_read(void *opaque, hwaddr offset, unsigned size)
{
    QuatroDDRMCState *s = QUATRO_DDRMC(opaque);
    int index = quatro_peripheral_offset_to_index(quatro_ddrmc_regs,
                                                  QUATRO_DDRMC_NUM_REGS,
                                                  offset);

    if (index < 0) {
        DEBUGLOG("%s: Bad read offset %#" HWADDR_PRIx,
                 TYPE_QUATRO_DDRMC, offset);
        return 0;
    }

    uint64_t value = s->regs[index];
    DEBUGLOG("%s: read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")",
             TYPE_QUATRO_DDRMC, value, quatro_ddrmc_regs[index].name, offset);
    return value;
}

static void quatro_ddrmc_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    QuatroDDRMCState *s = QUATRO_DDRMC(opaque);
    int index = quatro_peripheral_offset_to_index(quatro_ddrmc_regs,
                                                  QUATRO_DDRMC_NUM_REGS,
                                                  offset);

    switch (index) {
    case EXT_ADDR_MODE:
        s->regs[EXT_ADDR_MODE] = (uint32_t)value;
        break;
    default:
        ERRORLOG("%s: Bad write offset %#" HWADDR_PRIx,
                 TYPE_QUATRO_DDRMC, offset);
        return;
    }
    DEBUGLOG("%s: write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
             TYPE_QUATRO_DDRMC, value, quatro_ddrmc_regs[index].name, offset);
}

static void quatro_ddrmc_reset(DeviceState *dev)
{
    QuatroDDRMCState *s = QUATRO_DDRMC(dev);

    for (int i = 0; i < QUATRO_DDRMC_NUM_REGS; ++i) {
        s->regs[i] = quatro_ddrmc_regs[i].reset_value;
    }
}

static void quatro_ddrmc_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = quatro_ddrmc_read,
        .write      = quatro_ddrmc_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
    };

    QuatroDDRMCState *s = QUATRO_DDRMC(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_QUATRO_DDRMC, QUATRO_PERI_DDRMC_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void quatro_ddrmc_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = quatro_ddrmc_realize;
    dc->reset   = quatro_ddrmc_reset;
    dc->vmsd    = &quatro_ddrmc_vmstate;
}

static uint64_t quatro_sdiocore_read(void *opaque, hwaddr offset, unsigned size)
{
    QuatroSDIOCoreState *s = QUATRO_SDIOCORE(opaque);
    int index = quatro_peripheral_offset_to_index(quatro_sdiocore_regs,
                                                  QUATRO_SDIOCORE_NUM_REGS,
                                                  offset);

    if (index < 0) {
        ERRORLOG("%s: Bad read offset %#" HWADDR_PRIx,
                 TYPE_QUATRO_SDIOCORE, offset);
        return 0;
    }

    uint64_t value = s->regs[index];
    switch (index) {
    case SDIO0_HRS0:
        s->regs[SDIO0_HRS0] &= 0xFFFFFFFE;
        break;
    }

    DEBUGLOG("%s: read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")",
             TYPE_QUATRO_SDIOCORE, value, quatro_sdiocore_regs[index].name, offset);
    return value;
}

static void quatro_sdiocore_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    QuatroSDIOCoreState *s = QUATRO_SDIOCORE(opaque);
    int index = quatro_peripheral_offset_to_index(quatro_sdiocore_regs,
                                                  QUATRO_SDIOCORE_NUM_REGS,
                                                  offset);

    switch (index) {
    case SDIO0_HRS0:
        s->regs[SDIO0_HRS0] = (uint32_t)value;
        break;
    case SDIO0_HRS1:
        s->regs[SDIO0_HRS1] = (uint32_t)value;
        break;
    case SDIO0_HRS2:
        s->regs[SDIO0_HRS2] = (uint32_t)value;
        break;
    case SDIO0_HRS44_0:
        s->regs[SDIO0_HRS44_0] = (uint32_t)value | 0x04000000;
        if (s->regs[SDIO0_HRS44_0] & 0x01000000) {
            s->regs[SDIO0_HRS44_0] |= 0x04000000;
        } else {
            s->regs[SDIO0_HRS44_0] &= ~0x04000000;
        }
        break;
    case SDIO0_HRS44_1:
        s->regs[SDIO0_HRS44_1] = (uint32_t)value | 0x04000000;
        if (s->regs[SDIO0_HRS44_1] & 0x01000000) {
            s->regs[SDIO0_HRS44_1] |= 0x04000000;
        } else {
            s->regs[SDIO0_HRS44_1] &= ~0x04000000;
        }
        break;
    default:
        ERRORLOG("%s: Bad read offset %#" HWADDR_PRIx,
                 TYPE_QUATRO_SDIOCORE, offset);
        return;
    }
    DEBUGLOG("%s: write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
             TYPE_QUATRO_SDIOCORE, value, quatro_sdiocore_regs[index].name, offset);
}

static void quatro_sdiocore_reset(DeviceState *dev)
{
    QuatroSDIOCoreState *s = QUATRO_SDIOCORE(dev);

    for (int i = 0; i < QUATRO_SDIOCORE_NUM_REGS; ++i) {
        s->regs[i] = quatro_sdiocore_regs[i].reset_value;
    }
}

static void quatro_sdiocore_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read = quatro_sdiocore_read,
        .write = quatro_sdiocore_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
    };

    QuatroSDIOCoreState *s = QUATRO_SDIOCORE(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s, TYPE_QUATRO_SDIOCORE,
                          QUATRO_PERI_SDIOCORE_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void quatro_sdiocore_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = quatro_sdiocore_realize;
    dc->reset = quatro_sdiocore_reset;
    dc->vmsd = &quatro_sdiocore_vmstate;
}

static uint64_t quatro_sdmclk_read(void *opaque, hwaddr offset, unsigned size)
{
    QuatroSDMClkState *s = QUATRO_SDMCLK(opaque);
    int index = quatro_peripheral_offset_to_index(quatro_sdmclk_regs,
                                                  QUATRO_SDMCLK_NUM_REGS,
                                                  offset);

    if (index < 0) {
        ERRORLOG("%s: Bad read offset %#" HWADDR_PRIx,
                 TYPE_QUATRO_SDMCLK, offset);
        return 0;
    }

    uint64_t value = s->regs[index];
    DEBUGLOG("%s: read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")",
             TYPE_QUATRO_SDMCLK, value, quatro_sdmclk_regs[index].name, offset);
    return value;
}

static void quatro_sdmclk_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    QuatroSDMClkState *s = QUATRO_SDMCLK(opaque);
    int index = quatro_peripheral_offset_to_index(quatro_sdmclk_regs,
                                                  QUATRO_SDMCLK_NUM_REGS,
                                                  offset);

    switch (index) {
    case CLKDISCTRL:
        s->regs[CLKDISCTRL] = (uint32_t)value;
        s->regs[CLKDISSTAT] |= 0x00000003;
        s->regs[CLKDISSTAT] &= 0xFFFFFFFC | (0x00000003 & s->regs[CLKDISCTRL]);
        break;
    case SDIO0_EXTCTL:
        s->regs[SDIO0_EXTCTL] = (uint32_t)value;
        break;
    case SDIO1_EXTCTL:
        s->regs[SDIO1_EXTCTL] = (uint32_t)value;
        break;
    default:
        ERRORLOG("%s: Bad write offset %#" HWADDR_PRIx,
                 TYPE_QUATRO_SDMCLK, offset);
        return;
    }
    DEBUGLOG("%s: write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
             TYPE_QUATRO_SDMCLK, value, quatro_sdmclk_regs[index].name, offset);
}

static void quatro_sdmclk_reset(DeviceState *dev)
{
    QuatroSDMClkState *s = QUATRO_SDMCLK(dev);

    for (int i = 0; i < QUATRO_SDMCLK_NUM_REGS; ++i) {
        s->regs[i] = quatro_sdmclk_regs[i].reset_value;
    }
}

static void quatro_sdmclk_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read = quatro_sdmclk_read,
        .write = quatro_sdmclk_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
    };

    QuatroSDMClkState *s = QUATRO_SDMCLK(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s, TYPE_QUATRO_SDMCLK,
                          QUATRO_PERI_SDMCLK_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void quatro_sdmclk_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = quatro_sdmclk_realize;
    dc->reset = quatro_sdmclk_reset;
    dc->vmsd = &quatro_sdmclk_vmstate;
}

static uint64_t quatro_ttc_read(void *opaque, hwaddr offset, unsigned size)
{
    DEBUGLOG("%s: Bad read offset %#" HWADDR_PRIx,
             TYPE_QUATRO_TTC, offset);
    return 0;
}

static void quatro_ttc_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    DEBUGLOG("%s: Bad write %#" PRIx64 " to offset %#" HWADDR_PRIx,
             TYPE_QUATRO_TTC, value, offset);
}

static void quatro_ttc_reset(DeviceState *dev)
{
}

static void quatro_ttc_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read = quatro_ttc_read,
        .write = quatro_ttc_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
    };

    QuatroTTCState *s = QUATRO_TTC(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_QUATRO_TTC, QUATRO_PERI_TTC_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void quatro_ttc_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = quatro_ttc_realize;
    dc->reset   = quatro_ttc_reset;
    dc->vmsd    = &quatro_ttc_vmstate;
}

static uint64_t quatro_sbe_read(void *opaque, hwaddr offset, unsigned size)
{
    DEBUGLOG("%s: Bad read offset %#" HWADDR_PRIx,
             TYPE_QUATRO_SBE, offset);
    return 0;
}

static void quatro_sbe_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    DEBUGLOG("%s: Bad write %#" PRIx64 " to offset %#" HWADDR_PRIx,
             TYPE_QUATRO_SBE, value, offset);
}

static void quatro_sbe_reset(DeviceState *dev)
{
}

static void quatro_sbe_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read = quatro_sbe_read,
        .write = quatro_sbe_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
    };

    QuatroSBEState *s = QUATRO_SBE(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_QUATRO_SBE, QUATRO_PERI_SBE_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void quatro_sbe_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = quatro_sbe_realize;
    dc->reset   = quatro_sbe_reset;
    dc->vmsd    = &quatro_sbe_vmstate;
}

static uint64_t quatro_fir_read(void *opaque, hwaddr offset, unsigned size)
{
    DEBUGLOG("%s: Bad read offset %#" HWADDR_PRIx,
             TYPE_QUATRO_FIR, offset);
    return 0;
}

static void quatro_fir_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    DEBUGLOG("%s: Bad write %#" PRIx64 " to offset %#" HWADDR_PRIx,
             TYPE_QUATRO_FIR, value, offset);
}

static void quatro_fir_reset(DeviceState *dev)
{
}

static void quatro_fir_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read = quatro_fir_read,
        .write = quatro_fir_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
    };

    QuatroFIRState *s = QUATRO_FIR(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_QUATRO_FIR, QUATRO_PERI_FIR_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void quatro_fir_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = quatro_fir_realize;
    dc->reset   = quatro_fir_reset;
    dc->vmsd    = &quatro_fir_vmstate;
}

static uint64_t quatro_scal_read(void *opaque, hwaddr offset, unsigned size)
{
    DEBUGLOG("%s: Bad read offset %#" HWADDR_PRIx,
             TYPE_QUATRO_SCAL, offset);
    return 0;
}

static void quatro_scal_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    DEBUGLOG("%s: Bad write %#" PRIx64 " to offset %#" HWADDR_PRIx,
             TYPE_QUATRO_SCAL, value, offset);
}

static void quatro_scal_reset(DeviceState *dev)
{
}

static void quatro_scal_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read = quatro_scal_read,
        .write = quatro_scal_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
    };

    QuatroSCALState *s = QUATRO_SCAL(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_QUATRO_SCAL, QUATRO_PERI_SCAL_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void quatro_scal_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = quatro_scal_realize;
    dc->reset   = quatro_scal_reset;
    dc->vmsd    = &quatro_scal_vmstate;
}

static uint64_t quatro_scrn_read(void *opaque, hwaddr offset, unsigned size)
{
    DEBUGLOG("%s: Bad read offset %#" HWADDR_PRIx,
             TYPE_QUATRO_SCRN, offset);
    return 0;
}

static void quatro_scrn_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    DEBUGLOG("%s: Bad write %#" PRIx64 " to offset %#" HWADDR_PRIx,
             TYPE_QUATRO_SCRN, value, offset);
}

static void quatro_scrn_reset(DeviceState *dev)
{
}

static void quatro_scrn_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read = quatro_scrn_read,
        .write = quatro_scrn_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
    };

    QuatroSCRNState *s = QUATRO_SCRN(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_QUATRO_SCRN, QUATRO_PERI_SCRN_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void quatro_scrn_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = quatro_scrn_realize;
    dc->reset   = quatro_scrn_reset;
    dc->vmsd    = &quatro_scrn_vmstate;
}

static uint64_t quatro_jbig_read(void *opaque, hwaddr offset, unsigned size)
{
    DEBUGLOG("%s: Bad read offset %#" HWADDR_PRIx,
             TYPE_QUATRO_JBIG, offset);
    return 0;
}

static void quatro_jbig_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    DEBUGLOG("%s: Bad write %#" PRIx64 " to offset %#" HWADDR_PRIx,
             TYPE_QUATRO_JBIG, value, offset);
}

static void quatro_jbig_reset(DeviceState *dev)
{
}

static void quatro_jbig_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read = quatro_jbig_read,
        .write = quatro_jbig_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
    };

    QuatroJBIGState *s = QUATRO_JBIG(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_QUATRO_JBIG, QUATRO_PERI_JBIG_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void quatro_jbig_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = quatro_jbig_realize;
    dc->reset   = quatro_jbig_reset;
    dc->vmsd    = &quatro_jbig_vmstate;
}

static uint64_t quatro_lcdc_read(void *opaque, hwaddr offset, unsigned size)
{
    DEBUGLOG("%s: Bad read offset %#" HWADDR_PRIx,
             TYPE_QUATRO_LCDC, offset);
    return 0;
}

static void quatro_lcdc_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    DEBUGLOG("%s: Bad write %#" PRIx64 " to offset %#" HWADDR_PRIx,
             TYPE_QUATRO_LCDC, value, offset);
}

static void quatro_lcdc_reset(DeviceState *dev)
{
}

static void quatro_lcdc_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read = quatro_lcdc_read,
        .write = quatro_lcdc_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
    };

    QuatroLCDCState *s = QUATRO_LCDC(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_QUATRO_LCDC, QUATRO_PERI_LCDC_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void quatro_lcdc_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = quatro_lcdc_realize;
    dc->reset   = quatro_lcdc_reset;
    dc->vmsd    = &quatro_lcdc_vmstate;
}

static uint64_t quatro_dsp_read(void *opaque, hwaddr offset, unsigned size)
{
    DEBUGLOG("%s: Bad read offset %#" HWADDR_PRIx,
             TYPE_QUATRO_DSP, offset);
    return 0;
}

static void quatro_dsp_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    DEBUGLOG("%s: Bad write %#" PRIx64 " to offset %#" HWADDR_PRIx,
             TYPE_QUATRO_DSP, value, offset);
}

static void quatro_dsp_reset(DeviceState *dev)
{
}

static void quatro_dsp_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read = quatro_dsp_read,
        .write = quatro_dsp_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
    };

    QuatroDSPState *s = QUATRO_DSP(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_QUATRO_DSP, QUATRO_PERI_DSP_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void quatro_dsp_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = quatro_dsp_realize;
    dc->reset   = quatro_dsp_reset;
    dc->vmsd    = &quatro_dsp_vmstate;
}

static void quatro_peripherals_register_types(void)
{
    static const TypeInfo a15gpf_tinfo = {
        .name = TYPE_QUATRO_A15GPF,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroA15GPFState),
        .class_init = quatro_a15gpf_class_init,
    };
    static const TypeInfo rstgen_tinfo = {
        .name = TYPE_QUATRO_RSTGEN,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroRstGenState),
        .class_init = quatro_rstgen_class_init,
    };
    static const TypeInfo ddrmc_tinfo = {
        .name = TYPE_QUATRO_DDRMC,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroDDRMCState),
        .class_init = quatro_ddrmc_class_init,
    };
    static const TypeInfo sdiocore_tinfo = {
        .name = TYPE_QUATRO_SDIOCORE,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroSDIOCoreState),
        .class_init = quatro_sdiocore_class_init,
    };
    static const TypeInfo sdmclk_tinfo = {
        .name = TYPE_QUATRO_SDMCLK,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroSDMClkState),
        .class_init = quatro_sdmclk_class_init,
    };
    static const TypeInfo ttc_tinfo = {
        .name = TYPE_QUATRO_TTC,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroTTCState),
        .class_init = quatro_ttc_class_init,
    };
    static const TypeInfo sbe_tinfo = {
        .name = TYPE_QUATRO_SBE,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroSBEState),
        .class_init = quatro_sbe_class_init,
    };
    static const TypeInfo fir_tinfo = {
        .name = TYPE_QUATRO_FIR,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroFIRState),
        .class_init = quatro_fir_class_init,
    };
    static const TypeInfo scal_tinfo = {
        .name = TYPE_QUATRO_SCAL,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroSCALState),
        .class_init = quatro_scal_class_init,
    };
    static const TypeInfo scrn_tinfo = {
        .name = TYPE_QUATRO_SCRN,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroSCRNState),
        .class_init = quatro_scrn_class_init,
    };
    static const TypeInfo jbig_tinfo = {
        .name = TYPE_QUATRO_JBIG,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroJBIGState),
        .class_init = quatro_jbig_class_init,
    };
    static const TypeInfo lcdc_tinfo = {
        .name = TYPE_QUATRO_LCDC,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroLCDCState),
        .class_init = quatro_lcdc_class_init,
    };
    static const TypeInfo dsp_tinfo = {
        .name = TYPE_QUATRO_DSP,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(QuatroDSPState),
        .class_init = quatro_dsp_class_init,
    };

    type_register_static(&a15gpf_tinfo);
    type_register_static(&rstgen_tinfo);
    type_register_static(&ddrmc_tinfo);
    type_register_static(&sdiocore_tinfo);
    type_register_static(&sdmclk_tinfo);
    type_register_static(&ttc_tinfo);
    type_register_static(&sbe_tinfo);
    type_register_static(&fir_tinfo);
    type_register_static(&scal_tinfo);
    type_register_static(&scrn_tinfo);
    type_register_static(&jbig_tinfo);
    type_register_static(&lcdc_tinfo);
    type_register_static(&dsp_tinfo);
}

type_init(quatro_peripherals_register_types)
