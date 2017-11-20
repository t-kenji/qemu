/*
 * Copyright (c) 2017 t-kenji <protect.2501@gmail.com>
 *
 * QorIQ LS1046A Configuration, Control, and Status Register pseudo-device
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

#ifndef LS1_CCSR_H
#define LS1_CCSR_H


#define LS1046A_CCSR_SIZE              (0x0F000000)
#define LS1046A_CCSR_GIC_BASE_OFFSET   (0x00400000)
#define LS1046A_CCSR_GIC_DIST_OFFSET   (0x00410000)
#define LS1046A_CCSR_GIC_CPU_OFFSET    (0x00420000)
#define LS1046A_CCSR_DDR_OFFSET        (0x00080000)
#define LS1046A_CCSR_ESDHC_OFFSET      (0x00560000)
#define LS1046A_CCSR_SCFG_OFFSET       (0x00570000)
#define LS1046A_CCSR_SEC_OFFSET        (0x00700000)
#define LS1046A_CCSR_QMAN_OFFSET       (0x00880000)
#define LS1046A_CCSR_BMAN_OFFSET       (0x00890000)
#define LS1046A_CCSR_FMAN_OFFSET       (0x00A00000)
#define LS1046A_CCSR_GUTS_OFFSET       (0x00EE0000)
#define LS1046A_CCSR_CLK_OFFSET        (0x00EE1000)
#define LS1046A_CCSR_I2C1_OFFSET       (0x01180000)
#define LS1046A_CCSR_I2C1_SIZE         (0x00010000)
#define LS1046A_CCSR_I2C2_OFFSET       (0x01190000)
#define LS1046A_CCSR_I2C2_SIZE         (0x00010000)
#define LS1046A_CCSR_I2C3_OFFSET       (0x011A0000)
#define LS1046A_CCSR_I2C3_SIZE         (0x00010000)
#define LS1046A_CCSR_I2C4_OFFSET       (0x011B0000)
#define LS1046A_CCSR_I2C4_SIZE         (0x00010000)
#define LS1046A_CCSR_DUART1_OFFSET     (0x011C0500)
#define LS1046A_CCSR_DUART2_OFFSET     (0x011C0600)
#define LS1046A_CCSR_USB3C1_OFFSET     (0x01F00000)
#define LS1046A_CCSR_USB3C1_SIZE       (0x00100000)
#define LS1046A_CCSR_USB3C2_OFFSET     (0x02000000)
#define LS1046A_CCSR_USB3C2_SIZE       (0x00100000)
#define LS1046A_CCSR_USB3C3_OFFSET     (0x02100000)
#define LS1046A_CCSR_USB3C3_SIZE       (0x00100000)
#define LS1046A_CCSR_PCIE1SHR_OFFSET   (0x02400000)
#define LS1046A_CCSR_PCIE1SHR_SIZE     (0x00010000)
#define LS1046A_CCSR_PCIE1LUT_OFFSET   (0x02480000)
#define LS1046A_CCSR_PCIE1LUT_SIZE     (0x00010000)
#define LS1046A_CCSR_PCIE1CTL_OFFSET   (0x024C0000)
#define LS1046A_CCSR_PCIE1CTL_SIZE     (0x00010000)
#define LS1046A_CCSR_PCIE2SHR_OFFSET   (0x02500000)
#define LS1046A_CCSR_PCIE2SHR_SIZE     (0x00010000)
#define LS1046A_CCSR_PCIE2LUT_OFFSET   (0x02580000)
#define LS1046A_CCSR_PCIE2LUT_SIZE     (0x00010000)
#define LS1046A_CCSR_PCIE2CTL_OFFSET   (0x025C0000)
#define LS1046A_CCSR_PCIE2CTL_SIZE     (0x00010000)
#define LS1046A_CCSR_PCIE3SHR_OFFSET   (0x02600000)
#define LS1046A_CCSR_PCIE3SHR_SIZE     (0x00010000)
#define LS1046A_CCSR_PCIE3LUT_OFFSET   (0x02680000)
#define LS1046A_CCSR_PCIE3LUT_SIZE     (0x00010000)
#define LS1046A_CCSR_PCIE3CTL_OFFSET   (0x026C0000)
#define LS1046A_CCSR_PCIE3CTL_SIZE     (0x00010000)

#define TYPE_CCSR_DDR "ccsr-ddr"
#define CCSR_DDR(obj) OBJECT_CHECK(CCSRDDRState, (obj), TYPE_CCSR_DDR)

#define TYPE_CCSR_SCFG "ccsr-scfg"
#define CCSR_SCFG(obj) OBJECT_CHECK(CCSRSCfgState, (obj), TYPE_CCSR_SCFG)

#define TYPE_CCSR_GUTS "ccsr-guts"
#define CCSR_GUTS(obj) OBJECT_CHECK(CCSRGUtsState, (obj), TYPE_CCSR_GUTS)

#define TYPE_CCSR_CLK "ccsr-clk"
#define CCSR_CLK(obj) OBJECT_CHECK(CCSRClkState, (obj), TYPE_CCSR_CLK)


typedef struct CCSRDDRState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
} CCSRDDRState;

typedef struct CCSRSCfgState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t usb1prm1cr;
    uint32_t usb1prm2cr;
    uint32_t usb1prm3cr;
    uint32_t usb2prm1cr;
    uint32_t usb2prm2cr;
    uint32_t usb2prm3cr;
    uint32_t usb3prm1cr;
    uint32_t usb3prm2cr;
    uint32_t usb3prm3cr;
    uint32_t qspi_cfg;
    uint32_t snpcnfgcr;
    uint32_t intpcr;
    uint32_t rcwpmuxcr0;
    uint32_t usbdrvvbus_selcr;
    uint32_t usbpwrfault_selcr;
    uint32_t scratchrw[4];
    uint32_t corebcr;
} CCSRSCfgState;

typedef struct CCSRGUtsState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
} CCSRGUtsState;

typedef struct CCSRClkState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t clkccsr;
} CCSRClkState;


#endif /* LS1_CCSR_H */
