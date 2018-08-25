/*
 * CSR Quatro 5500 SoC emulation
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

#ifndef __CSR_QUATRO_5500_H__
#define __CSR_QUATRO_5500_H__

#include "qemu-common.h"
#include "hw/arm/arm.h"
#include "hw/cpu/a15mpcore.h"
#if 0
#include "hw/intc/arm_gic.h"
#include "hw/net/cadence_gem.h"
#include "hw/char/cadence_uart.h"
#include "hw/ide/pci.h"
#include "hw/ide/ahci.h"
#include "hw/sd/sdhci.h"
#include "hw/ssi/xilinx_spips.h"
#include "hw/dma/xlnx_dpdma.h"
#include "hw/dma/xlnx-zdma.h"
#include "hw/display/xlnx_dp.h"
#include "hw/intc/xlnx-zynqmp-ipi.h"
#include "hw/timer/xlnx-zynqmp-rtc.h"
#endif

#define TYPE_CSR_QUATRO "csr,quatro-5500"
#define CSR_QUATRO(obj) OBJECT_CHECK(struct CsrQuatroState, \
                                     (obj), TYPE_CSR_QUATRO)

enum CsrQuatroConfiguration {
    CSR_QUATRO_NUM_MPU_CPUS = 3,
    CSR_QUATRO_NUM_MCU_CPUS = 2,

    CSR_QUATRO_NUM_UARTS = 3,
};

enum CsrQuatroMemoryMap {
    CSR_QUATRO_DDR_RAM_ADDR = 0x80000000,
    CSR_QUATRO_MAX_RAM_SIZE = (2 * 1024UL * 1024UL * 1024UL),

    CSR_QUATRO_UART0_ADDR = 0x040B0010,
    CSR_QUATRO_UART1_ADDR = 0x04160010,
    CSR_QUATRO_UART2_ADDR = 0x052C0010,

    CSR_QUATRO_A7MPCORE_ADDR = 0x04300000,
};

enum CsrQuatroInterrupts {
    CSR_QUATRO_UART0_IRQ = 18,
    CSR_QUATRO_UART1_IRQ = 29,
    CSR_QUATRO_UART2_IRQ = 137,

    CSR_QUATRO_GIC_NUM_SPI_INTR = 160
};

struct CsrQuatroState {
    /*< private >*/
    DeviceState parent_obj;

    /*< public >*/
    ARMCPU mpu_cpus[CSR_QUATRO_NUM_MPU_CPUS];
    ARMCPU mcu_cpus[CSR_QUATRO_NUM_MCU_CPUS];
    A15MPPrivState a7mpcore;
};

#endif /* __CSR_QUATRO_5500_H__ */
