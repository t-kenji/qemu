/*
 * Freescale LS1046A SoC emulation
 *
 * Copyright (C) 2017 t-kenji <protect.2501@gmail.com>
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

#ifndef FSL_LS1_H
#define FSL_LS1_H

#include "hw/arm/arm.h"
#include "hw/intc/arm_gic.h"
#include "hw/sysbus.h"
#include "hw/char/serial.h"
#include "hw/i2c/imx_i2c.h"
#include "hw/sd/ls1_mmci.h"
#include "hw/misc/ls1_ccsr.h"
#include "hw/misc/ls1_dpaa.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "cpu.h"

#define TYPE_FSL_LS1046A "fsl,ls1046a"
#define FSL_LS1046A(obj) OBJECT_CHECK(FslLS1046AState, (obj), TYPE_FSL_LS1046A)

#define FSL_LS1046A_NUM_CPUS  (4)
#define FSL_LS1046A_NUM_UARTS (2)
#define FSL_LS1046A_NUM_IRQ   (256 - GIC_INTERNAL)
#define FSL_LS1046A_NUM_I2CS  (4)

typedef struct FslLS1046AState {
    /*< private >*/
    DeviceState parent_obj;

    /*< public >*/
    ARMCPU        cpus[FSL_LS1046A_NUM_CPUS];
    GICState      gic;
    qemu_irq      irqs[FSL_LS1046A_NUM_IRQ];
    CCSRDDRState  ddr;
    CCSRSCfgState scfg;
    CCSRGUtsState guts;
    CCSRClkState  clk;
    DPAAQMSPState qmsp;
    DPAABMSPState bmsp;
    DPAASecState  sec;
    DPAAQManState qman;
    DPAABManState bman;
    DPAAFManState fman;
    IMXI2CState   i2cs[FSL_LS1046A_NUM_I2CS];
    LS1MMCIState  esdhc;
    MemoryRegion  rom;
    MemoryRegion  ccsr;
    MemoryRegion  ocram0;
    MemoryRegion  ocram1;
} FslLS1046AState;


#define FSL_LS1046A_ROM_ADDR    (0x000000000)
#define FSL_LS1046A_ROM_SIZE    (0x000100000)
#define FSL_LS1046A_CCSR_ADDR   (0x001000000)
#define FSL_LS1046A_CCSR_SIZE   (0x00F000000)
#define FSL_LS1046A_OCRAM0_ADDR (0x010000000)
#define FSL_LS1046A_OCRAM0_SIZE (0x000010000)
#define FSL_LS1046A_OCRAM1_ADDR (0x010010000)
#define FSL_LS1046A_OCRAM1_SIZE (0x000010000)
#define FSL_LS1046A_MMDC_ADDR   (0x080000000)
#define FSL_LS1046A_MMDC_SIZE   (0x080000000)
#define FSL_LS1046A_QMSP_ADDR   (0x500000000)
#define FSL_LS1046A_QMSP_SIZE   (0x008000000)
#define FSL_LS1046A_BMSP_ADDR   (0x508000000)
#define FSL_LS1046A_BMSP_SIZE   (0x008000000)

#define ARCH_TIMER_VIRT_IRQ    (11)
#define ARCH_TIMER_S_EL1_IRQ   (13)
#define ARCH_TIMER_NS_EL1_IRQ  (14)
#define ARCH_TIMER_NS_EL2_IRQ  (10)

/* see LS1046ARM.pdf: 5.2 Internal interrupt sources */
#define FSL_LS1046A_DUART1_IRQ (86 - GIC_INTERNAL)
#define FSL_LS1046A_DUART2_IRQ (87 - GIC_INTERNAL)
#define FSL_LS1046A_I2C1_IRQ   (88)
#define FSL_LS1046A_I2C2_IRQ   (89)
#define FSL_LS1046A_I2C3_IRQ   (90)
#define FSL_LS1046A_I2C4_IRQ   (91)
#define FSL_LS1046A_USB1_IRQ   (92)
#define FSL_LS1046A_USB2_IRQ   (93)
#define FSL_LS1046A_ESDHC_IRQ  (94 - GIC_INTERNAL)
#define FSL_LS1046A_USB3_IRQ   (95)
#define FSL_LS1046A_GPIO1_IRQ  (98)
#define FSL_LS1046A_GPIO2_IRQ  (99)
#define FSL_LS1046A_GPIO3_IRQ  (100)
#define FSL_LS1046A_QSPI_IRQ   (131)

#endif /* FSL_LS1_H */
