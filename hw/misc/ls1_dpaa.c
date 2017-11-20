/*
 * Copyright (c) 2017 t-kenji <protect.2501@gmail.com>
 *
 * QorIQ LS1046A Data Path Acceleration Architecture  pseudo-device
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
 * file name "LS1046ADPAARM.pdf" and "LS1046ASECRM.pdf". You can easily find
 * it on the web.
 *
 */

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "cpu.h"
#include "hw/hw.h"
#include "sysemu/sysemu.h"
#include "arm-powerctl.h"
#include "hw/sysbus.h"
#include "hw/arm/fsl-ls1.h"
#include "hw/misc/ls1_dpaa.h"
#include "qemu/log.h"


#define LS1_DPAA_DEBUG

#define QMSP_MMIO_SIZE (0x8000000)
#define BMSP_MMIO_SIZE (0x8000000)
#define SEC_MMIO_SIZE  (0x0100000)
#define QMAN_MMIO_SIZE (0x0010000)
#define BMAN_MMIO_SIZE (0x0010000)
#define FMAN_MMIO_SIZE (0x0100000)

#define QMSP_ADDR_QCSP_EQCR_CI_CINH (0x00003040)
#define QMSP_ADDR_QCSP_ISDR         (0x00003680)
#define QMSP_ADDR_QCSP_CR           (0x00003800)
#define QMSP_ADDR_QCSP_RR0          (0x00003900)
#define QMSP_ADDR_QCSP_RR1          (0x00003940)

/*
 * QCSP Management Command Registers (QCSPi_CR)
 *  VERB: 
 *  COMMAND_DATA: 
 */
#define QMSP_RST_QCSP_CR (0x00000000)
#define QMSP_MSK_QCSP_CR(v) (uint32_t)((v) & 0xFFFFFFFF)

#define QMSP_RST_QCSP_RR (0x00000000)
#define QMSP_MSK_QCSP_RR(v) (uint32_t)((v) & 0xFFFFFFFF)

/*
 * QCSP EQCR Consumer Index Cache-Inhibited Registers (QCSPi_EQCR_CI_CINH)
 *  VP: 1
 *  PB: 1
 *  VC: 1
 *  CI: 0
 */
#define QMSP_RST_QCSP_EQCR_CI_CINH (0x00008808)
#define QMSP_MSK_QCSP_EQCR_CI_CINH(v) (uint32_t)((v) & 0x0000880F)

/*
 * QCSP Interrupt Status Disable Registers (QCSPi_ISDR)
 *  CCSCI: enable
 *  CSCI: enable
 *  EQCI: enable
 *  EQRI: enable
 *  DQRI: enable
 *  MRI: enable
 *  DQ_AVAIL: enable
 */
#define QMSP_RST_QCSP_ISDR (0x00000000)
#define QMSP_MSK_QCSP_ISDR(v) (uint32_t)((v) & 0x003FFFFF)


#define REG_FMT TARGET_FMT_plx

#if defined(LS1_DPAA_DEBUG)
#define dprintf(...) qemu_log(__VA_ARGS__)
#else
#define dprintf(...)
#endif


static uint64_t dpaa_qmsp_read(void *opaque, hwaddr addr, unsigned size)
{
    DPAAQMSPState *s = opaque;
    uint64_t value = 0;
    int offset;
    int i;

    i = (addr & 0xF0000) >> 16;
    offset = (addr & 0x3F);
    addr &= (QMSP_MMIO_SIZE - 1) & ~0x40F003F;
    switch (addr) {
    case QMSP_ADDR_QCSP_EQCR_CI_CINH:
        value = s->qcsp_eqcr_ci_cinh[i];
        break;
    case QMSP_ADDR_QCSP_ISDR:
        value = s->qcsp_isdr[i];
        break;
    case QMSP_ADDR_QCSP_CR:
        value = (s->qcsp_cr[i][offset / 4] >> (8 * (offset % 4))) & (0xFFFFFFFF >> (8 * (8 - size)));
        break;
    case QMSP_ADDR_QCSP_RR0:
        value = (s->qcsp_rr0[i][offset / 4] >> (8 * (offset % 4))) & (0xFFFFFFFF >> (8 * (8 - size)));
        break;
    case QMSP_ADDR_QCSP_RR1:
        value = (s->qcsp_rr1[i][offset / 4] >> (8 * (offset % 4))) & (0xFFFFFFFF >> (8 * (8 - size)));
        break;
    default:
        break;
    }
    dprintf("%s: " REG_FMT "+%d+%d > %"  PRIx64 "\n", __func__, addr, offset, i, value);

    return value;
}

static void dpaa_qmsp_write(void *opaque, hwaddr addr,
                            uint64_t value, unsigned size)
{
    DPAAQMSPState *s = opaque;
    int offset;
    int i;

    i = (addr & 0xF0000) >> 16;
    offset = (addr & 0x3F);
    addr &= (QMSP_MMIO_SIZE - 1) & ~0x40F003F;
    switch (addr) {
    case QMSP_ADDR_QCSP_EQCR_CI_CINH:
        s->qcsp_eqcr_ci_cinh[i] = QMSP_MSK_QCSP_EQCR_CI_CINH(value);
        break;
    case QMSP_ADDR_QCSP_ISDR:
        s->qcsp_isdr[i] = QMSP_MSK_QCSP_ISDR(value);
        break;
    case QMSP_ADDR_QCSP_CR:
        s->qcsp_cr[i][offset / 4] &= (0xFFFFFFFFFFFFFFFF >> (8 * (8 - size))) << (8 * (offset % 4));
        s->qcsp_cr[i][offset / 4] |= value << (8 * (offset % 4));

        if (offset == 0 && size == 1) {
            s->qcsp_rr0[i][0] = value | (0xF0 << 8);
            s->qcsp_rr1[i][0] = value | (0xF0 << 8);
        }
        break;
    default:
        break;
    }
    dprintf("%s: " REG_FMT "+%d+%d < %" PRIx64 "\n", __func__, addr, offset, i, value);
}

static uint64_t dpaa_bmsp_read(void *opaque, hwaddr addr, unsigned size)
{
    uint64_t value = 0;

    addr &= BMSP_MMIO_SIZE - 1;

    dprintf("%s: " REG_FMT " > %"  PRIx64 "\n", __func__, addr, value);

    return value;
}

static void dpaa_bmsp_write(void *opaque, hwaddr addr,
                            uint64_t value, unsigned size)
{
    addr &= BMSP_MMIO_SIZE - 1;

    dprintf("%s: " REG_FMT " < %" PRIx64 "\n", __func__, addr, value);
}

static uint64_t dpaa_sec_read(void *opaque, hwaddr addr, unsigned size)
{
    uint64_t value = 0;

    addr &= SEC_MMIO_SIZE - 1;

    dprintf("%s: " REG_FMT " > %"  PRIx64 "\n", __func__, addr, value);

    return value;
}

static void dpaa_sec_write(void *opaque, hwaddr addr,
                           uint64_t value, unsigned size)
{
    addr &= SEC_MMIO_SIZE - 1;

    dprintf("%s: " REG_FMT " < %" PRIx64 "\n", __func__, addr, value);
}

static uint64_t dpaa_qman_read(void *opaque, hwaddr addr, unsigned size)
{
    uint64_t value = 0;

    addr &= QMAN_MMIO_SIZE - 1;

    dprintf("%s: " REG_FMT " > %"  PRIx64 "\n", __func__, addr, value);

    return value;
}

static void dpaa_qman_write(void *opaque, hwaddr addr,
                             uint64_t value, unsigned size)
{
    addr &= QMAN_MMIO_SIZE - 1;

    dprintf("%s: " REG_FMT " < %" PRIx64 "\n", __func__, addr, value);
}

static uint64_t dpaa_bman_read(void *opaque, hwaddr addr, unsigned size)
{
    uint64_t value = 0;

    addr &= BMAN_MMIO_SIZE - 1;

    dprintf("%s: " REG_FMT " > %"  PRIx64 "\n", __func__, addr, value);

    return value;
}

static void dpaa_bman_write(void *opaque, hwaddr addr,
                             uint64_t value, unsigned size)
{
    addr &= BMAN_MMIO_SIZE - 1;

    dprintf("%s: " REG_FMT " < %" PRIx64 "\n", __func__, addr, value);
}

static uint64_t dpaa_fman_read(void *opaque, hwaddr addr, unsigned size)
{
    uint64_t value = 0;

    addr &= FMAN_MMIO_SIZE - 1;

    dprintf("%s: " REG_FMT " > %"  PRIx64 "\n", __func__, addr, value);

    return value;
}

static void dpaa_fman_write(void *opaque, hwaddr addr,
                             uint64_t value, unsigned size)
{
    addr &= FMAN_MMIO_SIZE - 1;

    dprintf("%s: " REG_FMT " < %" PRIx64 "\n", __func__, addr, value);
}

static void dpaa_qmsp_initfn(Object *obj)
{
    static const MemoryRegionOps dpaa_qmsp_ops = {
        .read = dpaa_qmsp_read,
        .write = dpaa_qmsp_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
        .valid = {
            .min_access_size = 1,
            .max_access_size = 4,
        }
    };

    SysBusDevice *d = SYS_BUS_DEVICE(obj);
    DPAAQMSPState *s = DPAA_QMSP(obj);
    int i, j;

    memory_region_init_io(&s->iomem, OBJECT(s), &dpaa_qmsp_ops, s,
                          "dpaa.qmsp", QMSP_MMIO_SIZE);
    sysbus_init_mmio(d, &s->iomem);

    for (i = 0; i < ARRAY_SIZE(s->qcsp_eqcr_ci_cinh); ++i) {
        s->qcsp_eqcr_ci_cinh[i] = QMSP_RST_QCSP_EQCR_CI_CINH;
    }
    for (i = 0; i < ARRAY_SIZE(s->qcsp_isdr); ++i) {
        s->qcsp_isdr[i] = QMSP_RST_QCSP_ISDR;
    }
    for (i = 0; i < ARRAY_SIZE(s->qcsp_cr); ++i) {
        for (j = 0; j < ARRAY_SIZE(s->qcsp_cr[0]); ++j) {
            s->qcsp_cr[i][j] = QMSP_RST_QCSP_CR;
        }
    }
    for (i = 0; i < ARRAY_SIZE(s->qcsp_rr0); ++i) {
        for (j = 0; j < ARRAY_SIZE(s->qcsp_rr0[0]); ++j) {
            s->qcsp_rr0[i][j] = QMSP_RST_QCSP_RR;
        }
    }
    for (i = 0; i < ARRAY_SIZE(s->qcsp_rr1); ++i) {
        for (j = 0; j < ARRAY_SIZE(s->qcsp_rr1[0]); ++j) {
            s->qcsp_rr1[i][j] = QMSP_RST_QCSP_RR;
        }
    }
}

static void dpaa_bmsp_initfn(Object *obj)
{
    static const MemoryRegionOps dpaa_bmsp_ops = {
        .read = dpaa_bmsp_read,
        .write = dpaa_bmsp_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
        .valid = {
            .min_access_size = 4,
            .max_access_size = 4,
        }
    };

    SysBusDevice *d = SYS_BUS_DEVICE(obj);
    DPAABMSPState *s = DPAA_BMSP(obj);

    memory_region_init_io(&s->iomem, OBJECT(s), &dpaa_bmsp_ops, s,
                          "dpaa.bmsp", BMSP_MMIO_SIZE);
    sysbus_init_mmio(d, &s->iomem);
}

static void dpaa_sec_initfn(Object *obj)
{
    static const MemoryRegionOps dpaa_sec_ops = {
        .read = dpaa_sec_read,
        .write = dpaa_sec_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
        .valid = {
            .min_access_size = 1,
            .max_access_size = 8,
        }
    };

    SysBusDevice *d = SYS_BUS_DEVICE(obj);
    DPAASecState *s = DPAA_SEC(obj);

    memory_region_init_io(&s->iomem, OBJECT(s), &dpaa_sec_ops, s,
                          "dpaa.sec", SEC_MMIO_SIZE);
    sysbus_init_mmio(d, &s->iomem);
}

static void dpaa_qman_initfn(Object *obj)
{
    static const MemoryRegionOps dpaa_qman_ops = {
        .read = dpaa_qman_read,
        .write = dpaa_qman_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
        .valid = {
            .min_access_size = 4,
            .max_access_size = 4,
        }
    };

    SysBusDevice *d = SYS_BUS_DEVICE(obj);
    DPAAQManState *s = DPAA_QMAN(obj);

    memory_region_init_io(&s->iomem, OBJECT(s), &dpaa_qman_ops, s,
                          "dpaa.qman", QMAN_MMIO_SIZE);
    sysbus_init_mmio(d, &s->iomem);
}

static void dpaa_bman_initfn(Object *obj)
{
    static const MemoryRegionOps dpaa_bman_ops = {
        .read = dpaa_bman_read,
        .write = dpaa_bman_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
        .valid = {
            .min_access_size = 4,
            .max_access_size = 4,
        }
    };

    SysBusDevice *d = SYS_BUS_DEVICE(obj);
    DPAABManState *s = DPAA_BMAN(obj);

    memory_region_init_io(&s->iomem, OBJECT(s), &dpaa_bman_ops, s,
                          "dpaa.bman", BMAN_MMIO_SIZE);
    sysbus_init_mmio(d, &s->iomem);
}

static void dpaa_fman_initfn(Object *obj)
{
    static const MemoryRegionOps dpaa_fman_ops = {
        .read = dpaa_fman_read,
        .write = dpaa_fman_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
        .valid = {
            .min_access_size = 4,
            .max_access_size = 4,
        }
    };

    SysBusDevice *d = SYS_BUS_DEVICE(obj);
    DPAAFManState *s = DPAA_FMAN(obj);

    memory_region_init_io(&s->iomem, OBJECT(s), &dpaa_fman_ops, s,
                          "dpaa.fman", FMAN_MMIO_SIZE);
    sysbus_init_mmio(d, &s->iomem);
}


static const TypeInfo dpaa_qmsp_info = {
    .name          = TYPE_DPAA_QMSP,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(DPAAQMSPState),
    .instance_init = dpaa_qmsp_initfn
};

static const TypeInfo dpaa_bmsp_info = {
    .name          = TYPE_DPAA_BMSP,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(DPAABMSPState),
    .instance_init = dpaa_bmsp_initfn
};

static const TypeInfo dpaa_sec_info = {
    .name          = TYPE_DPAA_SEC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(DPAASecState),
    .instance_init = dpaa_sec_initfn
};

static const TypeInfo dpaa_qman_info = {
    .name          = TYPE_DPAA_QMAN,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(DPAAQManState),
    .instance_init = dpaa_qman_initfn
};

static const TypeInfo dpaa_bman_info = {
    .name          = TYPE_DPAA_BMAN,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(DPAABManState),
    .instance_init = dpaa_bman_initfn
};

static const TypeInfo dpaa_fman_info = {
    .name          = TYPE_DPAA_FMAN,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(DPAAFManState),
    .instance_init = dpaa_fman_initfn
};

static void dpaa_register_types(void)
{
    type_register_static(&dpaa_qmsp_info);
    type_register_static(&dpaa_bmsp_info);
    type_register_static(&dpaa_sec_info);
    type_register_static(&dpaa_qman_info);
    type_register_static(&dpaa_bman_info);
    type_register_static(&dpaa_fman_info);
}

type_init(dpaa_register_types)
