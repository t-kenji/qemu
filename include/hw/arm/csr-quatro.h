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

#define TYPE_CSR_QUATRO "csr,quatro-5500"
#define CSR_QUATRO(obj) OBJECT_CHECK(CsrQuatroState, (obj), TYPE_CSR_QUATRO)

#define DEFAULT_CPUS (1 + CSR_QUATRO_NUM_MP_CPUS)
#define MAX_CPUS (CSR_QUATRO_NUM_AP_CPUS + CSR_QUATRO_NUM_MP_CPUS)
#define AP_CPUS MIN(smp_cpus - CSR_QUATRO_NUM_MP_CPUS, CSR_QUATRO_NUM_AP_CPUS)

#define MiB(n) ((n) * 1024UL * 1024UL)
#define GiB(n) ((n) * 1024UL * 1024UL * 1024UL)

enum CsrQuatroConfiguration {
    CSR_QUATRO_NUM_AP_CPUS = 3,
    CSR_QUATRO_NUM_MP_CPUS = 2,

    CSR_QUATRO_NUM_UARTS = 3,
    CSR_QUATRO_NUM_SDHCIS = 3,
};

enum CsrQuatroMemoryMap {
    CSR_QUATRO_DDR_RAM_ADDR  = 0x80000000,
    CSR_QUATRO_DDR_RAM_SIZE  = GiB(2),

    CSR_QUATRO_RSTGEN_ADDR   = 0x04010000,
    CSR_QUATRO_CLK_ADDR      = 0x04020000,
    CSR_QUATRO_RTC_ADDR      = 0x04030000,
    CSR_QUATRO_HRT0_ADDR     = 0x04040010,
    CSR_QUATRO_HRT1_ADDR     = 0x04040020,
    CSR_QUATRO_SDMCLK_ADDR   = 0x04050000,

    CSR_QUATRO_UART0_ADDR    = 0x040B0010,
    CSR_QUATRO_UART1_ADDR    = 0x04160010,
    CSR_QUATRO_UART2_ADDR    = 0x052C0010,

    CSR_QUATRO_FCSPI_ADDR    = 0x04110000,
    CSR_QUATRO_A7MPCORE_ADDR = 0x04300000,
    CSR_QUATRO_DDRMC_ADDR    = 0x04310000,
    CSR_QUATRO_A15GPF_ADDR   = 0x043B0000,
    CSR_QUATRO_SDIO0_ADDR    = 0x04440000,
    CSR_QUATRO_SDHCI0_ADDR   = 0x04440100,
    CSR_QUATRO_SDIO1_ADDR    = 0x04450000,
    CSR_QUATRO_SDHCI1_ADDR   = 0x04450100,
    CSR_QUATRO_SDHCI2_ADDR   = 0x04450200,
    CSR_QUATRO_USBD_ADDR     = 0x04500000,
    CSR_QUATRO_USBH_ADDR     = 0x04600000,
    CSR_QUATRO_ETHERNET_ADDR = 0x04410000,
    CSR_QUATRO_GPDMA0_ADDR   = 0x04150000,
    CSR_QUATRO_GPDMA1_ADDR   = 0x04940000,
    CSR_QUATRO_TTC0_ADDR     = 0x04980000,
    CSR_QUATRO_TTC1_ADDR     = 0x049A0000,
    CSR_QUATRO_SATA_ADDR     = 0x04A30000,
    CSR_QUATRO_SBE0_ADDR     = 0x05040000,
    CSR_QUATRO_SBE1_ADDR     = 0x05050000,
    CSR_QUATRO_FIR0_ADDR     = 0x05060000,
    CSR_QUATRO_FIR1_ADDR     = 0x05070000,
    CSR_QUATRO_SCAL0_ADDR    = 0x05080000,
    CSR_QUATRO_SCAL1_ADDR    = 0x05090000,
    CSR_QUATRO_SCRN0_ADDR    = 0x050A0000,
    CSR_QUATRO_SCRN1_ADDR    = 0x050B0000,
    CSR_QUATRO_LPRI0_ADDR    = 0x05120000,
    CSR_QUATRO_JBIG0_ADDR    = 0x05110000,
    CSR_QUATRO_JBIG1_ADDR    = 0x05150000,
    CSR_QUATRO_LCDC_ADDR     = 0x052A0000,
    CSR_QUATRO_DSP0_ADDR     = 0x05700000,
    CSR_QUATRO_DSP1_ADDR     = 0x05780000,
    CSR_QUATRO_CM30_ADDR     = 0x05340000,
    CSR_QUATRO_CM31_ADDR     = 0x05360000,
    CSR_QUATRO_SRAM_ADDR     = 0x05400000,
    CSR_QUATRO_SRAM_SIZE     = MiB(2),
};

enum CsrQuatroInterrupts {
    CSR_QUATRO_UART0_IRQ = 18,
    CSR_QUATRO_UART1_IRQ = 29,
    CSR_QUATRO_UART2_IRQ = 137,
    CSR_QUATRO_FCSPI_IRQ = 21,
    CSR_QUATRO_XHCI_IRQ = 97,
    CSR_QUATRO_SDIO0_IRQ = 99,
    CSR_QUATRO_SDIO1_IRQ = 101,
    CSR_QUATRO_STMMAC_IRQ = 94,

    CSR_QUATRO_GIC_NUM_SPI_INTR = 192
};

typedef struct {
    /*< private >*/
    DeviceState parent_obj;

    /*< public >*/
    ARMCPU ap_cpus[CSR_QUATRO_NUM_AP_CPUS];
    A15MPPrivState a7mpcore;
    MemoryRegion sram;
} CsrQuatroState;

#endif /* __CSR_QUATRO_5500_H__ */
