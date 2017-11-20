/*
 * Copyright (c) 2017 t-kenji <protect.2501@gmail.com>
 *
 * QorIQ LS1046A Configuration, Control, and Status Register pseudo-device
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
 * file name "LS1046ARM.pdf". You can easily find it on the web.
 *
 */

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "cpu.h"
#include "hw/hw.h"
#include "sysemu/sysemu.h"
#include "arm-powerctl.h"
#include "hw/sysbus.h"
#include "hw/misc/ls1_ccsr.h"
#include "qemu/log.h"


#define LS1_CCSR_DEBUG


#define DDR_MMIO_SIZE (0x10000)
#define DDR_ADDR_TIMING_CFG_3 (0x0100)
#define DDR_ADDR_TIMING_CFG_0 (0x0104)
#define DDR_ADDR_TIMING_CFG_1 (0x0108)
#define DDR_ADDR_TIMING_CFG_2 (0x010C)
#define DDR_ADDR_SDRAM_CFG (0x0110)
#define DDR_ADDR_SDRAM_CFG_2 (0x0114)
#define DDR_ADDR_IP_REV1 (0x0BF8)
#define DDR_ADDR_IP_REV2 (0x0BFC)
#define DDR_ADDR_DEBUG_10 (0x0F24)
#define DDR_ADDR_DEBUG_11 (0x0F28)
#define DDR_ADDR_DEBUG_29 (0x0F70)

/*
 * DDR SDRAM timing configuration 3 (TIMING_CFG_3)
 *  EXT_PRETOACT: 0 clocks
 *  EXT_ACTTOPRE: 0 clocks
 *  EXT_ACTTORW: 0
 *  EXT_REFREC: 0 clocks
 *  EXT_CASLAT: 0 clocks
 *  EXT_ADD_LAT: 0 clocks
 *  EXT_WRREC: 0 clocks
 *  CNTL_ADJ: MODTn, MSCn_B, and MCKEn will be launched aligned with the other DRAM address and control signals.
 */
#define DDR_VAL_TIMING_CFG_3 (0x00000000)

/*
 * DDR SDRAM timing configuration 1 (TIMING_CFG_1)
 *  PRETOACT: 1 clock
 *  ACTTOPRE: 16 clocks
 *  ACTTORW: 1 clock
 *  CASLAT: 1 clock
 *  RSRVD: Reserved.
 *  REFREC: 8 clocks
 *  RWREC: 1 clock
 *  ACTTOACT: 1 clock
 *  WRTORD: 1 clock
 */
#define DDR_VAL_TIMING_CFG_1 (0x10100111)

/*
 * DDR SDRAM control configuration (DDR_SDRAM_CFG)
 *  MEM_EN: SDRAM interface logic is enabled.
 *  SREN: SDRAM self refresh is disabled during sleep.
 *  ECC_EN: No ECC errors are reported.
 *  SDRAM_TYPE: DDR4 SDRAM
 *  DYN_PWR: Dynamic power management mode is disabled.
 *  DBW: 32-bit bus is used.
 *  BE_8: 8-beat bursts are used on the DRAM interface.
 *  T3_EN: 1T timing is enabled.
 *  T2_EN: 1T timing is enabled.
 *  BA_INTLV_CTL: No external memory banks are interleaved
 *  HSE: I/O driver impedance will be calibrated to full strength.
 *  ACC_ECC_EN: Accumulated ECC is disabled
 *  MEM_HALT: DDR controller will accept transactions.
 *  BI: DDR controller will cycle through initialization routine based on SDRAM_TYPE
 */
#define DDR_VAL_SDRAM_CFG (0x850C0000)

/*
 * DDR IP block revision 1 (DDR_IP_REV1)
 *  IP_ID: IP block ID. (For the DDR controller, this value is 0x0002)
 *  IP_MJ: Major revision. (This is currently set to 0x05)
 *  IP_MN: Minor revision. (This is currently set to 8'd1)
 */
#define DDR_VAL_IP_REV1 (0x00020501)

/*
 * DDR IP block revision 2 (DDR_IP_REV2)
 *  IP_INT: IP Block integration Options.
 *  IP_CFG: IP Block Configuration Options.
 */
#define DDR_VAL_IP_REV2 (0x00000000)


#define SCFG_MMIO_SIZE (0x10000)
#define SCFG_ADDR_USB1PRM1CR (0x00070)
#define SCFG_ADDR_USB1PRM2CR (0x00074)
#define SCFG_ADDR_USB1PRM3CR (0x00078)
#define SCFG_ADDR_USB2PRM1CR (0x0007C)
#define SCFG_ADDR_USB2PRM2CR (0x00080)
#define SCFG_ADDR_USB2PRM3CR (0x00084)
#define SCFG_ADDR_USB3PRM1CR (0x00088)
#define SCFG_ADDR_USB3PRM2CR (0x0008C)
#define SCFG_ADDR_USB3PRM3CR (0x00090)
#define SCFG_ADDR_USB2_ICID (0x00100)
#define SCFG_ADDR_USB3_ICID (0x00104)
#define SCFG_ADDR_SATA_ICID (0x00118)
#define SCFG_ADDR_USB1_ICID (0x0011C)
#define SCFG_ADDR_SDHC_ICID (0x00124)
#define SCFG_ADDR_EDMA_ICID (0x00128)
#define SCFG_ADDR_ETR_ICID (0x0012C)
#define SCFG_ADDR_CORE0_SFT_RST (0x00130)
#define SCFG_ADDR_CORE1_SFT_RST (0x00134)
#define SCFG_ADDR_CORE2_SFT_RST (0x00138)
#define SCFG_ADDR_CORE3_SFT_RST (0x0013C)
#define SCFG_ADDR_FTM_CHAIN_CONFIG (0x00154)
#define SCFG_ADDR_ALTCBAR (0x00158)
#define SCFG_ADDR_QSPI_CFG (0x0015C)
#define SCFG_ADDR_QOS1 (0x0016C)
#define SCFG_ADDR_QOS2 (0x00170)
#define SCFG_ADDR_DEBUG_ICID (0x0018C)
#define SCFG_ADDR_SNPCNFGCR (0x001A4)
#define SCFG_ADDR_INTPCR (0x001AC)
#define SCFG_ADDR_CORESRENCR (0x00204)
#define SCFG_ADDR_RVBAR0_0 (0x00220)
#define SCFG_ADDR_RVBAR0_1 (0x00224)
#define SCFG_ADDR_RVBAR1_0 (0x00228)
#define SCFG_ADDR_RVBAR1_1 (0x0022C)
#define SCFG_ADDR_RVBAR2_0 (0x00230)
#define SCFG_ADDR_RVBAR2_1 (0x00234)
#define SCFG_ADDR_RVBAR3_0 (0x00238)
#define SCFG_ADDR_RVBAR3_1 (0x0023C)
#define SCFG_ADDR_LPMCSR (0x00240)
#define SCFG_ADDR_ECGTXCMCR (0x00404)
#define SCFG_ADDR_SDHCIOVSELCR (0x00408)
#define SCFG_ADDR_RCWPMUXCR0 (0x0040C)
#define SCFG_ADDR_USBDRVVBUS_SELCR (0x00410)
#define SCFG_ADDR_USBPWRFAULT_SELCR (0x00414)
#define SCFG_ADDR_USB_REFCLK_SELCR1 (0x00418)
#define SCFG_ADDR_USB_REFCLK_SELCR2 (0x0041C)
#define SCFG_ADDR_USB_REFCLK_SELCR3 (0x00420)
#define SCFG_ADDR_RETREQCR (0x00424)
#define SCFG_ADDR_COREPMCR (0x0042C)
#define SCFG_ADDR_SCRATCHRW(n) (0x00600 + ((n) * 4))
#define SCFG_ADDR_COREBCR (0x00680)
#define SCFG_ADDR_G0MSIIR(n) (0x11000 + ((n) * 4))
#define SCFG_ADDR_G1MSIIR(n) (0x22000 + ((n) * 4))
#define SCFG_ADDR_G2MSIIR(n) (0x33000 + ((n) * 4))

/*
 * USB1 Parameter 1 Control Register (SCFG_USB1PRM1CR)
 */
#define SCFG_RST_USB1PRM1CR (0x27672B2A)
#define SCFG_MSK_USB1PRM1CR(v) (uint32_t)((v) & 0x00000000FFFFFFFF)

/*
 * USB1 Parameter 2 Control Register (SCFG_USB1PRM2CR)
 */
#define SCFG_RST_USB1PRM2CR (0x17C1FF48)
#define SCFG_MSK_USB1PRM2CR(v) (uint32_t)((v) & 0x00000000FFFFFFF8)

/*
 * USB1 Parameter 3 Control Register (SCFG_USB1PRM3CR)
 */
#define SCFG_RST_USB1PRM3CR (0x00000000)
#define SCFG_MSK_USB1PRM3CR(v) (uint32_t)((v) & 0x00000000FFFF0000)

/*
 * USB2 Parameter 1 Control Register (SCFG_USB2PRM1CR)
 */
#define SCFG_RST_USB2PRM1CR (0x27672B2A)
#define SCFG_MSK_USB2PRM1CR(v) (uint32_t)((v) & 0x00000000FFFFFFFF)

/*
 * USB2 Parameter 2 Control Register (SCFG_USB2PRM2CR)
 */
#define SCFG_RST_USB2PRM2CR (0x17C1FF48)
#define SCFG_MSK_USB2PRM2CR(v) (uint32_t)((v) & 0x00000000FFFFFFF8)

/*
 * USB2 Parameter 3 Control Register (SCFG_USB2PRM3CR)
 */
#define SCFG_RST_USB2PRM3CR (0x00000000)
#define SCFG_MSK_USB2PRM3CR(v) (uint32_t)((v) & 0x00000000FF7F0000)

/*
 * USB3 Parameter 1 Control Register (SCFG_USB3PRM1CR)
 */
#define SCFG_RST_USB3PRM1CR (0x27672B2A)
#define SCFG_MSK_USB3PRM1CR(v) (uint32_t)((v) & 0x00000000FFFFFFFF)

/*
 * USB3 Parameter 2 Control Register (SCFG_USB3PRM2CR)
 */
#define SCFG_RST_USB3PRM2CR (0x17C1FF48)
#define SCFG_MSK_USB3PRM2CR(v) (uint32_t)((v) & 0x00000000FFFFFFF8)

/*
 * USB3 Parameter 3 Control Register (SCFG_USB3PRM3CR)
 */
#define SCFG_RST_USB3PRM3CR (0x00000000)
#define SCFG_MSK_USB3PRM3CR(v) (uint32_t)((v) & 0x00000000FF7F0000)

/*
 * USB2 ICID Register (SCFG_USB2_ICID)
 */
#define SCFG_RST_USB2_ICID (0x00000000)
#define SCFG_MSK_USB2_ICID(v) (uint32_t)((v) & 0x00000000FF800000)

/*
 * USB3 ICID Register (SCFG_USB3_ICID)
 */
#define SCFG_RST_USB3_ICID (0x00000000)
#define SCFG_MSK_USB3_ICID(v) (uint32_t)((v) & 0x00000000FF800000)

/*
 * SATA ICID Register (SCFG_SATA_ICID)
 */
#define SCFG_RST_SATA_ICID (0x00000000)
#define SCFG_MSK_SATA_ICID(v) (uint32_t)((v) & 0x00000000FF800000)

/*
 * USB1 ICID Register (SCFG_USB1_ICID)
 */
#define SCFG_RST_USB1_ICID (0x00000000)
#define SCFG_MSK_USB1_ICID(v) (uint32_t)((v) & 0x00000000FF800000)

/*
 * eSDHC ICID Register (SCFG_SDHC_ICID)
 */
#define SCFG_RST_SDHC_ICID (0x00000000)
#define SCFG_MSK_SDHC_ICID(v) (uint32_t)((v) & 0x00000000FF800000)

/*
 * eDMA ICID Register (SCFG_eDMA_ICID)
 */
#define SCFG_RST_EDMA_ICID (0x00000000)
#define SCFG_MSK_EDMA_ICID(v) (uint32_t)((v) & 0x00000000FF800000)

/*
 * ETR ICID Register (SCFG_ETR_ICID)
 */
#define SCFG_RST_ETR_ICID (0x00000000)
#define SCFG_MSK_ETR_ICID(v) (uint32_t)((v) & 0x00000000FF800000)

/*
 * Core 0 soft reset Register (SCFG_CORE0_SFT_RST)
 */
#define SCFG_RST_CORE0_SFT_RST (0x00000000)
#define SCFG_MSK_CORE0_SFT_RST(v) (uint32_t)((v) & 0x0000000080000000)

/*
 * Core 1 soft reset Register (SCFG_CORE1_SFT_RST)
 */
#define SCFG_RST_CORE1_SFT_RST (0x00000000)
#define SCFG_MSK_CORE1_SFT_RST(v) (uint32_t)((v) & 0x0000000080000000)

/*
 * Core 2 soft reset Register (SCFG_CORE2_SFT_RST)
 */
#define SCFG_RST_CORE2_SFT_RST (0x00000000)
#define SCFG_MSK_CORE2_SFT_RST(v) (uint32_t)((v) & 0x0000000080000000)

/*
 * Core 3 soft reset Register (SCFG_CORE3_SFT_RST)
 */
#define SCFG_RST_CORE3_SFT_RST (0x00000000)
#define SCFG_MSK_CORE3_SFT_RST(v) (uint32_t)((v) & 0x0000000080000000)

/*
 * FTM chain configuration (SCFG_FTM_CHAIN_CONFIG)
 */
#define SCFG_RST_FTM_CHAIN_CONFIG (0x00000000)
#define SCFG_MSK_FTM_CHAIN_CONFIG(v) (uint32_t)((v) & 0x00000000F000F000)

/*
 * ALTCBAR Register (SCFG_ALTCBAR)
 */
#define SCFG_RST_ALTCBAR (0x0000000000)
#define SCFG_MSK_ALTCBAR(v) (uint32_t)((v) & 0x00000000FFFFFE00)

/*
 * QSPI CONFIG Register (SCFG_QSPI_CFG)
 *  CLK_SEL: Divide by 64
 *  CLKDIS: Not gated
 *  CLKSRC: Core PLL
 */
#define SCFG_RST_QSPI_CFG (0x10100000)
#define SCFG_MSK_QSPI_CFG(v) (uint32_t)((v) & 0x00000000F0000090)

#define SCFG_RST_QOS1 (0x0016C)
#define SCFG_RST_QOS2 (0x00170)
#define SCFG_RST_DEBUG_ICID (0x0018C)

/*
 * Snoop Configuration Register (SCFG_SNPCNFGCR)
 *  SATARDSNP: SATA reads are not snoopable
 *  SATAWRSNP: SATA writes are not snoopable
 *  USB1RDSNP: USB1 reads are not snoopable
 *  USB1WRSNP: USB1 write are not snoopable
 *  DBGRDSNP: Debug reads are not snoopable
 *  DBGWRSNP: Debug write are not snoopable
 *  USB2WRSNP: USB2 writes are not snoopable
 *  USB2RDSNP: USB2 reads are not snoopable
 *  USB3WRSNP: USB3 writes are not snoopable
 *  USB3RDSNP: USB3 reads are not snoopable
 */
#define SCFG_RST_SNPCNFGCR (0x00000000)
#define SCFG_MSK_SNPCNFGCR(v) (uint32_t)((v) & 0x0000000000FD7000)

/*
 * Interrupt Polarity Register (SCFG_INTPCR)
 */
#define SCFG_RST_INTPCR (0x00000000)
#define SCFG_MSK_INTPCR(v) (uint32_t)((v) & 0x00000000FFF00000)

#define SCFG_RST_CORESRENCR (0x00204)
#define SCFG_RST_RVBAR0_0 (0x00220)
#define SCFG_RST_RVBAR0_1 (0x00224)
#define SCFG_RST_RVBAR1_0 (0x00228)
#define SCFG_RST_RVBAR1_1 (0x0022C)
#define SCFG_RST_RVBAR2_0 (0x00230)
#define SCFG_RST_RVBAR2_1 (0x00234)
#define SCFG_RST_RVBAR3_0 (0x00238)
#define SCFG_RST_RVBAR3_1 (0x0023C)
#define SCFG_RST_LPMCSR (0x00240)
#define SCFG_RST_ECGTXCMCR (0x00000000)
#define SCFG_RST_SDHCIOVSELCR (0x00408)

/*
 * Extended RCW PinMux Control Register (SCFG_RCWPMUXCR0)
 */
#define SCFG_RST_RCWPMUXCR0 (0x0040C)
#define SCFG_MSK_RCWPMUXCR0(v) (uint32_t)((v) & 0x0000000000007777)

/*
 * USB DRVVBUS Control Register (SCFG_USBDRVVBUS_SELCR)
 */
#define SCFG_RST_USBDRVVBUS_SELCR (0x00000000)
#define SCFG_MSK_USBDRVVBUS_SELCR(v) (uint32_t)((v) & 0x0000000000000003)

/*
 * USB PWRFAULT Control Register (SCFG_USBPWRFAULT_SELCR)
 */
#define SCFG_RST_USBPWRFAULT_SELCR (0x00000000)
#define SCFG_MSK_USBPWRFAULT_SELCR(v) (uint32_t)((v) & 0x000000000000003F);

#define SCFG_RST_USB_REFCLK_SELCR1 (0x00418)
#define SCFG_RST_USB_REFCLK_SELCR2 (0x0041C)
#define SCFG_RST_USB_REFCLK_SELCR3 (0x00420)
#define SCFG_RST_RETREQCR (0x00424)
#define SCFG_RST_COREPMCR (0x0042C)

/*
 * Scratch Read Write Registers (SCFG_SCRATCHRWn)
 *  VAL: 32-bit scratch contents
 */
#define SCFG_RST_SCRATCHRWn (0x00000000)
#define SCFG_MSK_SCRATCHRW(v) (uint32_t)((v) & 0x00000000FFFFFFFF)

/*
 * Core Boot Control Register (SCFG_COREBCR)
 */
#define SCFG_RST_COREBCR (0x00000000)
#define SCFG_MSK_COREBCR(v) (uint32_t)((v) & 0x000000000000000F)

#define SCFG_RST_G0MSIIR(n) (0x11000 + ((n) * 4))
#define SCFG_RST_G1MSIIR(n) (0x22000 + ((n) * 4))
#define SCFG_RST_G2MSIIR(n) (0x33000 + ((n) * 4))


#define GUTS_MMIO_SIZE (0x1000)
#define GUTS_RSTCR_RESET (0x02)
#define GUTS_ADDR_FUSESR (0x028)
#define GUTS_ADDR_DEVDISR2 (0x074)
#define GUTS_ADDR_SVR (0x0A4)
#define GUTS_ADDR_BRR (0x0E4)
#define GUTS_ADDR_RCWSRA(n) (0x100 + ((n) * 4)) /* range is 0x100-0x13C (n is 0-15) */
#define GUTS_ADDR_TP_ITYPA(n) (0x740 + ((n) * 4)) /* range is 0x740-0x83C (n is 0-63) */
#define GUTS_ADDR_TP_CLUSTER1 (0x844)

/*
 * Fuse Status Register (FUSESR)
 *  DA_V: 1V
 */
#define GUTS_VAL_FUSESR (0x00000000)

/*
 * Device Disable Register 2 (DEVDISR2)
 *  FMAN1_MAC1: Module is disabled
 *  FMAN1_MAC2: Module is disabled
 *  FMAN1_MAC3: Module is disabled
 *  FMAN1_MAC4: Module is disabled
 *  FMAN1_MAC5: Module is disabled
 *  FMAN1_MAC6: Module is disabled
 *  FMAN1_MAC9: Module is disabled
 *  FMAN1_MAC10: Module is disabled
 *  FMAN1: Module is disabled
 */
#define GUTS_VAL_DEVDISR2 (0xFCC00080)

/*
 * System Version Register (SVR)
 *  MFR_ID: 8
 *  SOC_DEV_ID: 0x707
 *  VAR_PER: LS1046A (with security)
 *  MAJOR_REV: 1
 *  MINOR_REV: 0
 */
#define GUTS_VAL_SVR (0x87070010)

/*
 * Reset Configuration Word Status n (RCWSRa)
 *  see u-boot/board/freescale/ls1046ardb/ls1046ardb_rcw_emmc.cfg
 */
static const uint32_t RCW_DATA[] = {
    0x0c150010, 0x0e000000, 0x00000000, 0x00000000,
    0x11335559, 0x40000012, 0x60040000, 0xc1000000,
    0x00000000, 0x00000000, 0x00000000, 0x00238800,
    0x20124000, 0x00003000, 0x00000096, 0x00000001
};

/*
 * Topology Initiator Type n (TP_ITYPa)
 *  INIT_TYPE: ARM Cortex A72 (disable)
 */
#define GUTS_VAL_TP_ITYPA       (0x00000081)

/*
 * Core Cluster n Topology Register (TP_CLUSTER1)
 *  EOC: Last cluster in the chip
 */
#define GUTS_VAL_TP_CLUSTER1    (0xC3020100)


#define CLK_MMIO_SIZE (0x0C20)
#define CLK_ADDR_CLKCCSR (0x000)
#define CLK_ADDR_CL1KCGHWACSR (0x010)
#define CLK_ADDR_CL2KCGHWACSR (0x030)
#define CLK_ADDR_PLLC1GSR (0x800)
#define CLK_ADDR_PLLC2GSR (0x820)
#define CLK_ADDR_CLKPCSR (0xA00)
#define CLK_ADDR_PLLPGSR (0xC00)
#define CLK_ADDR_PLLDGSR (0xC20)

/*
 * Core cluster n clock control/status register (Clocking_CLKCCSR)
 *  CLKSEL: Platform Frequency. The FMAN core clock will remain Async.
 */
#define CLK_RST_CLKCCSR (0x28000000)
#define CLK_MSK_CLKCCSR(v) (uint32_t)((v) & 0x78000000)

/*
 * Clock generator n hardware accelerator control / status register (Clocking_CLnKCGHWACSR)
 *  HWACLKSEL: Async mode (Cluster Group A PLL 2/2 is clock)
 */
#define CLK_VAL_CL1KCGHWACSR (0x30000000)

/*
 * Clock generator n hardware accelerator control / status register (Clocking_CLnKCGHWACSR)
 *  HWACLKSEL: Async mode (Cluster Group A PLL 2/1 is clock)
 */
#define CLK_VAL_CL2KCGHWACSR (0x08000000)

/*
 * PLL cluster n general status register (Clocking_PLLCnGSR)
 *  KILL: PLL is active.
 *  CFG: RCW[CGA_PLL1_RAT] is 16:1
 *       RCW[CGA_PLL2_RAT] is 14:1
 */
#define CLK_VAL_PLLC1GSR (0x00000020)
#define CLK_VAL_PLLC2GSR (0x0000001C)

/*
 * Platform clock domain control/status register (Clocking_CLKPCSR)
 *  CLKOEN: Release CLK_OUT pad to high impedance.
 *  CLKOSEL: SYSCLK.
 *  CLKODIV: No division. (divide-by-1)
 */
#define CLK_VAL_CLKPCSR (0x0000F800)

/*
 * Platform PLL general status register (Clocking_PLLPGSR)
 *  CFG: RCW[SYS_PLL_RAT] is 6:1
 */
#define CLK_VAL_PLLPGSR (0x0000000C)

/*
 * DDR PLL general status register (Clocking_PLLDGSR)
 *  KILL: PLL is active.
 *  CFG: RCW[MEM_PLL_RAT] is 21:1
 */
#define CLK_VAL_PLLDGSR (0x0000002A)


#define REG_FMT TARGET_FMT_plx

#if defined(LS1_CCSR_DEBUG)
#define dprintf(...) qemu_log(__VA_ARGS__)
#else
#define dprintf(...)
#endif


static uint64_t bootlocptr = 0;


static uint64_t ccsr_ddr_read(void *opaque, hwaddr addr,
                                 unsigned size)
{
    uint64_t value = 0;

    addr &= DDR_MMIO_SIZE - 1;
    switch (addr) {
    case DDR_ADDR_TIMING_CFG_3:
        value = DDR_VAL_TIMING_CFG_3;
        break;
    case DDR_ADDR_TIMING_CFG_1:
        value = DDR_VAL_TIMING_CFG_1;
        break;
    case DDR_ADDR_SDRAM_CFG:
        value = DDR_VAL_SDRAM_CFG;
        break;
    case DDR_ADDR_IP_REV1:
        value = DDR_VAL_IP_REV1;
        break;
    case DDR_ADDR_IP_REV2:
        value = DDR_VAL_IP_REV2;
        break;
    case DDR_ADDR_DEBUG_10:
        /* TODO: check the actual board value */
        value = 0;
        break;
    case DDR_ADDR_DEBUG_11:
        /* TODO: check the actual board value */
        value = 0;
        break;
    case DDR_ADDR_DEBUG_29:
        /* TODO: check the actual board value */
        value = 0;
        break;
    default:
        hw_error("%s: Unknown register read: " REG_FMT "\n", __func__, addr);
        break;
    }
    dprintf("ddr: " REG_FMT " > %"  PRIx64 "\n", addr, value);

    return value;
}

static void ccsr_ddr_write(void *opaque, hwaddr addr,
                              uint64_t value, unsigned size)
{
    addr &= DDR_MMIO_SIZE - 1;

    dprintf("ddr: Unknown register write: " REG_FMT " = %" PRIx64 "\n",
            addr, value);
}

static uint64_t ccsr_scfg_read(void *opaque, hwaddr addr,
                                  unsigned size)
{
    CCSRSCfgState *s = opaque;
    uint64_t value = 0;

    addr &= SCFG_MMIO_SIZE - 1;
    switch (addr) {
    case SCFG_ADDR_USB1PRM1CR:
        value = s->usb1prm1cr;
        break;
    case SCFG_ADDR_USB1PRM2CR:
        value = s->usb1prm2cr;
        break;
    case SCFG_ADDR_USB1PRM3CR:
        value = s->usb1prm3cr;
        break;
    case SCFG_ADDR_USB2PRM1CR:
        value = s->usb2prm1cr;
        break;
    case SCFG_ADDR_USB2PRM2CR:
        value = s->usb2prm2cr;
        break;
    case SCFG_ADDR_USB2PRM3CR:
        value = s->usb2prm3cr;
        break;
    case SCFG_ADDR_USB3PRM1CR:
        value = s->usb3prm1cr;
        break;
    case SCFG_ADDR_USB3PRM2CR:
        value = s->usb3prm2cr;
        break;
    case SCFG_ADDR_USB3PRM3CR:
        value = s->usb3prm3cr;
        break;
    case SCFG_ADDR_USB2_ICID:
    case SCFG_ADDR_USB3_ICID:
    case SCFG_ADDR_SATA_ICID:
    case SCFG_ADDR_USB1_ICID:
    case SCFG_ADDR_SDHC_ICID:
    case SCFG_ADDR_EDMA_ICID:
    case SCFG_ADDR_ETR_ICID:
    case SCFG_ADDR_CORE0_SFT_RST:
    case SCFG_ADDR_CORE1_SFT_RST:
    case SCFG_ADDR_CORE2_SFT_RST:
    case SCFG_ADDR_CORE3_SFT_RST:
    case SCFG_ADDR_FTM_CHAIN_CONFIG:
    case SCFG_ADDR_ALTCBAR:
        hw_error("%s: register "  REG_FMT " is not implemented\n", __func__, addr);
        break;
    case SCFG_ADDR_QSPI_CFG:
        value = s->qspi_cfg;
        break;
    case SCFG_ADDR_SNPCNFGCR:
        value = s->snpcnfgcr;
        break;
    case SCFG_ADDR_INTPCR:
        value = s->intpcr;
        break;
    case SCFG_ADDR_RCWPMUXCR0:
        value = s->rcwpmuxcr0;
        break;
    case SCFG_ADDR_USBDRVVBUS_SELCR:
        value = s->usbdrvvbus_selcr;
        break;
    case SCFG_ADDR_USBPWRFAULT_SELCR:
        value = s->usbpwrfault_selcr;
        break;
    case SCFG_ADDR_SCRATCHRW(0):
        value = s->scratchrw[0];
        break;
    case SCFG_ADDR_SCRATCHRW(1):
        value = s->scratchrw[1];
        break;
    case SCFG_ADDR_SCRATCHRW(2):
        value = s->scratchrw[2];
        break;
    case SCFG_ADDR_SCRATCHRW(3):
        value = s->scratchrw[3];
        break;
    case SCFG_ADDR_COREBCR:
        value = s->corebcr;
        break;
    default:
        hw_error("%s: Unknown register read: " REG_FMT "\n", __func__, addr);
        break;
    }
    dprintf("scfg: " REG_FMT " > %"  PRIx64 "\n", addr, value);

    return value;
}

static void ccsr_scfg_write(void *opaque, hwaddr addr,
                               uint64_t value, unsigned size)
{
    CCSRSCfgState *s = opaque;

    addr &= SCFG_MMIO_SIZE - 1;
    switch (addr) {
    case SCFG_ADDR_USB1PRM1CR:
        s->usb1prm1cr = SCFG_MSK_USB1PRM1CR(value);
        break;
    case SCFG_ADDR_USB1PRM2CR:
        s->usb1prm2cr = SCFG_MSK_USB1PRM2CR(value);
        break;
    case SCFG_ADDR_USB1PRM3CR:
        s->usb1prm3cr = SCFG_MSK_USB1PRM2CR(value);
        break;
    case SCFG_ADDR_USB2PRM1CR:
        s->usb2prm1cr = SCFG_MSK_USB1PRM3CR(value);
        break;
    case SCFG_ADDR_USB2PRM2CR:
        s->usb2prm2cr = SCFG_MSK_USB2PRM1CR(value);
        break;
    case SCFG_ADDR_USB2PRM3CR:
        s->usb2prm3cr = SCFG_MSK_USB2PRM3CR(value);
        break;
    case SCFG_ADDR_USB3PRM1CR:
        s->usb3prm1cr = SCFG_MSK_USB3PRM1CR(value);
        break;
    case SCFG_ADDR_USB3PRM2CR:
        s->usb3prm2cr = SCFG_MSK_USB3PRM2CR(value);
        break;
    case SCFG_ADDR_USB3PRM3CR:
        s->usb3prm3cr = SCFG_MSK_USB3PRM3CR(value);
        break;
    case SCFG_ADDR_USB2_ICID:
    case SCFG_ADDR_USB3_ICID:
    case SCFG_ADDR_SATA_ICID:
    case SCFG_ADDR_USB1_ICID:
    case SCFG_ADDR_SDHC_ICID:
    case SCFG_ADDR_EDMA_ICID:
    case SCFG_ADDR_ETR_ICID:
    case SCFG_ADDR_CORE0_SFT_RST:
    case SCFG_ADDR_CORE1_SFT_RST:
    case SCFG_ADDR_CORE2_SFT_RST:
    case SCFG_ADDR_CORE3_SFT_RST:
    case SCFG_ADDR_FTM_CHAIN_CONFIG:
    case SCFG_ADDR_ALTCBAR:
        hw_error("%s: register " REG_FMT " is not implemented\n", __func__, addr);
        break;
    case SCFG_ADDR_QSPI_CFG:
        s->qspi_cfg = SCFG_MSK_QSPI_CFG(value);
        break;
    case SCFG_ADDR_SNPCNFGCR:
        s->snpcnfgcr = SCFG_MSK_SNPCNFGCR(value);
        break;
    case SCFG_ADDR_INTPCR:
        s->intpcr = SCFG_MSK_INTPCR(value);
        break;
    case SCFG_ADDR_RCWPMUXCR0:
        s->rcwpmuxcr0 = SCFG_MSK_RCWPMUXCR0(value);
        break;
    case SCFG_ADDR_USBDRVVBUS_SELCR:
        s->usbdrvvbus_selcr = SCFG_MSK_USBDRVVBUS_SELCR(value);
        break;
    case SCFG_ADDR_USBPWRFAULT_SELCR:
        s->usbpwrfault_selcr = SCFG_MSK_USBPWRFAULT_SELCR(value);
        break;
    case SCFG_ADDR_SCRATCHRW(0):
        s->scratchrw[0] = SCFG_MSK_SCRATCHRW(value);
        bootlocptr = ((uint64_t)s->scratchrw[0] << 32) | (uint64_t)s->scratchrw[1];
        break;
    case SCFG_ADDR_SCRATCHRW(1):
        s->scratchrw[1] = SCFG_MSK_SCRATCHRW(value);
        bootlocptr = ((uint64_t)s->scratchrw[0] << 32) | (uint64_t)s->scratchrw[1];
        break;
    case SCFG_ADDR_SCRATCHRW(2):
        s->scratchrw[2] = SCFG_MSK_SCRATCHRW(value);
        break;
    case SCFG_ADDR_SCRATCHRW(3):
        s->scratchrw[3] = SCFG_MSK_SCRATCHRW(value);
        break;
    case SCFG_ADDR_COREBCR:
        s->corebcr = SCFG_MSK_COREBCR(value);
        break;
    default:
        hw_error("%s: Unknown register write: " REG_FMT " < %" PRIx64 "\n",
                 __func__, addr, value);
        break;
    };
}

static uint64_t ccsr_guts_read(void *opaque, hwaddr addr,
                                  unsigned size)
{
    uint64_t value = 0;
    //ARMCPU *cpu = ARM_CPU(current_cpu);
    //CPUARMState *env = &cpu->env;

    addr &= GUTS_MMIO_SIZE - 1;
    switch (addr) {
    case GUTS_ADDR_FUSESR:
        value = GUTS_VAL_FUSESR;
        break;
    case GUTS_ADDR_DEVDISR2:
        value = GUTS_VAL_DEVDISR2;
        break;
    case GUTS_ADDR_SVR:
        value = GUTS_VAL_SVR;
        break;
    case GUTS_ADDR_RCWSRA(0):
    case GUTS_ADDR_RCWSRA(1):
    case GUTS_ADDR_RCWSRA(2):
    case GUTS_ADDR_RCWSRA(3):
    case GUTS_ADDR_RCWSRA(4):
    case GUTS_ADDR_RCWSRA(5):
    case GUTS_ADDR_RCWSRA(6):
    case GUTS_ADDR_RCWSRA(7):
    case GUTS_ADDR_RCWSRA(8):
    case GUTS_ADDR_RCWSRA(9):
    case GUTS_ADDR_RCWSRA(10):
    case GUTS_ADDR_RCWSRA(11):
    case GUTS_ADDR_RCWSRA(12):
    case GUTS_ADDR_RCWSRA(13):
    case GUTS_ADDR_RCWSRA(14):
    case GUTS_ADDR_RCWSRA(15):
    {
        uint32_t offset = (addr - GUTS_ADDR_RCWSRA(0)) / sizeof(uint32_t);
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        value = bswap32(RCW_DATA[offset]);
#else
        value = RCW_DATA[offset];
#endif
        break;
    }
    case GUTS_ADDR_TP_ITYPA(0):
    case GUTS_ADDR_TP_ITYPA(1):
    case GUTS_ADDR_TP_ITYPA(2):
    case GUTS_ADDR_TP_ITYPA(3):
    case GUTS_ADDR_TP_ITYPA(4):
    case GUTS_ADDR_TP_ITYPA(5):
    case GUTS_ADDR_TP_ITYPA(6):
    case GUTS_ADDR_TP_ITYPA(7):
    case GUTS_ADDR_TP_ITYPA(8):
    case GUTS_ADDR_TP_ITYPA(9):
    case GUTS_ADDR_TP_ITYPA(10):
    case GUTS_ADDR_TP_ITYPA(11):
    case GUTS_ADDR_TP_ITYPA(12):
    case GUTS_ADDR_TP_ITYPA(13):
    case GUTS_ADDR_TP_ITYPA(14):
    case GUTS_ADDR_TP_ITYPA(15):
    case GUTS_ADDR_TP_ITYPA(16):
    case GUTS_ADDR_TP_ITYPA(17):
    case GUTS_ADDR_TP_ITYPA(18):
    case GUTS_ADDR_TP_ITYPA(19):
    case GUTS_ADDR_TP_ITYPA(20):
    case GUTS_ADDR_TP_ITYPA(21):
    case GUTS_ADDR_TP_ITYPA(22):
    case GUTS_ADDR_TP_ITYPA(23):
    case GUTS_ADDR_TP_ITYPA(24):
    case GUTS_ADDR_TP_ITYPA(25):
    case GUTS_ADDR_TP_ITYPA(26):
    case GUTS_ADDR_TP_ITYPA(27):
    case GUTS_ADDR_TP_ITYPA(28):
    case GUTS_ADDR_TP_ITYPA(29):
    case GUTS_ADDR_TP_ITYPA(30):
    case GUTS_ADDR_TP_ITYPA(31):
    case GUTS_ADDR_TP_ITYPA(32):
    case GUTS_ADDR_TP_ITYPA(33):
    case GUTS_ADDR_TP_ITYPA(34):
    case GUTS_ADDR_TP_ITYPA(35):
    case GUTS_ADDR_TP_ITYPA(36):
    case GUTS_ADDR_TP_ITYPA(37):
    case GUTS_ADDR_TP_ITYPA(38):
    case GUTS_ADDR_TP_ITYPA(39):
    case GUTS_ADDR_TP_ITYPA(40):
    case GUTS_ADDR_TP_ITYPA(41):
    case GUTS_ADDR_TP_ITYPA(42):
    case GUTS_ADDR_TP_ITYPA(43):
    case GUTS_ADDR_TP_ITYPA(44):
    case GUTS_ADDR_TP_ITYPA(45):
    case GUTS_ADDR_TP_ITYPA(46):
    case GUTS_ADDR_TP_ITYPA(47):
    case GUTS_ADDR_TP_ITYPA(48):
    case GUTS_ADDR_TP_ITYPA(49):
    case GUTS_ADDR_TP_ITYPA(50):
    case GUTS_ADDR_TP_ITYPA(51):
    case GUTS_ADDR_TP_ITYPA(52):
    case GUTS_ADDR_TP_ITYPA(53):
    case GUTS_ADDR_TP_ITYPA(54):
    case GUTS_ADDR_TP_ITYPA(55):
    case GUTS_ADDR_TP_ITYPA(56):
    case GUTS_ADDR_TP_ITYPA(57):
    case GUTS_ADDR_TP_ITYPA(58):
    case GUTS_ADDR_TP_ITYPA(59):
    case GUTS_ADDR_TP_ITYPA(60):
    case GUTS_ADDR_TP_ITYPA(61):
    case GUTS_ADDR_TP_ITYPA(62):
    case GUTS_ADDR_TP_ITYPA(63):
        value = GUTS_VAL_TP_ITYPA;
        break;
    case GUTS_ADDR_TP_CLUSTER1:
        value = GUTS_VAL_TP_CLUSTER1;
        break;
    default:
        hw_error("%s: Unknown register read: " REG_FMT "\n", __func__, addr);
        break;
    }
    dprintf("guts: " REG_FMT " > %"  PRIx64 "\n", addr, value);

    return value;
}

static void ccsr_guts_write(void *opaque, hwaddr addr,
                               uint64_t value, unsigned size)
{
    ARMCPU *cpu = ARM_CPU(current_cpu);
    CPUARMState *env = &cpu->env;
    int cur_el = arm_current_el(&cpu->env);
    int i;

    addr &= GUTS_MMIO_SIZE - 1;
    switch (addr) {
    case GUTS_ADDR_BRR:
        for (i = 0; i < smp_cpus; ++i) {
            if (value & (1 << i)) {
                arm_set_cpu_on(i, bootlocptr, env->xregs[0], cur_el, env->aarch64);
            }
        }
        break;
    default:
        hw_error("%s: Unknown register write: "REG_FMT " < %" PRIx64 "\n",
                 __func__, addr, value);
        break;
    }
}

static uint64_t ccsr_clk_read(void *opaque, hwaddr addr,
                                 unsigned size)
{
    CCSRClkState *s = opaque;
    uint64_t value = 0;

    addr &= CLK_MMIO_SIZE - 1;
    switch (addr) {
    case CLK_ADDR_CLKCCSR:
        value = s->clkccsr;
        break;
    case CLK_ADDR_CL1KCGHWACSR:
        value = CLK_VAL_CL1KCGHWACSR;
        break;
    case CLK_ADDR_CL2KCGHWACSR:
        value = CLK_VAL_CL2KCGHWACSR;
        break;
    case CLK_ADDR_PLLC1GSR:
        value = CLK_VAL_PLLC1GSR;
        break;
    case CLK_ADDR_PLLC2GSR:
        value = CLK_VAL_PLLC2GSR;
        break;
    case CLK_ADDR_CLKPCSR:
        value = CLK_VAL_CLKPCSR;
        break;
    case CLK_ADDR_PLLPGSR:
        value = CLK_VAL_PLLPGSR;
        break;
    case CLK_ADDR_PLLDGSR:
        value = CLK_VAL_PLLDGSR;
        break;
    default:
        hw_error("%s: Unknown register read: " REG_FMT "\n", __func__, addr);
        break;
    }
    dprintf("clk: " REG_FMT " > %"  PRIx64 "\n", addr, value);

    return value;
}

static void ccsr_clk_write(void *opaque, hwaddr addr,
                              uint64_t value, unsigned size)
{
    CCSRClkState *s = opaque;

    addr &= CLK_MMIO_SIZE - 1;
    switch (addr) {
    case CLK_ADDR_CLKCCSR:
        s->clkccsr = CLK_MSK_CLKCCSR(value);
        break;
    default:
        hw_error("%s: Unknown register write: " REG_FMT " < %" PRIx64 "\n",
                 __func__, addr, value);
        break;
    };
}

static void ccsr_ddr_initfn(Object *obj)
{
    static const MemoryRegionOps ddr_ops = {
        .read = ccsr_ddr_read,
        .write = ccsr_ddr_write,
        .endianness = DEVICE_BIG_ENDIAN,
        .valid = {
            .min_access_size = 4,
            .max_access_size = 4,
        }
    };

    SysBusDevice *d = SYS_BUS_DEVICE(obj);
    CCSRDDRState *s = CCSR_DDR(obj);

    memory_region_init_io(&s->iomem, OBJECT(s), &ddr_ops, s,
                          "ccsr.ddr", DDR_MMIO_SIZE);
    sysbus_init_mmio(d, &s->iomem);
}

static void ccsr_scfg_initfn(Object *obj)
{
    static const MemoryRegionOps scfg_ops = {
        .read = ccsr_scfg_read,
        .write = ccsr_scfg_write,
        .endianness = DEVICE_BIG_ENDIAN,
        .valid = {
            .min_access_size = 4,
            .max_access_size = 4,
        }
    };

    SysBusDevice *d = SYS_BUS_DEVICE(obj);
    CCSRSCfgState *s = CCSR_SCFG(obj);

    memory_region_init_io(&s->iomem, OBJECT(s), &scfg_ops, s,
                          "ccsr.scfg", SCFG_MMIO_SIZE);
    sysbus_init_mmio(d, &s->iomem);

    s->usb1prm1cr = SCFG_RST_USB1PRM1CR;
    s->usb1prm2cr = SCFG_RST_USB1PRM2CR;
    s->usb1prm3cr = SCFG_RST_USB1PRM3CR;
    s->usb2prm1cr = SCFG_RST_USB2PRM1CR;
    s->usb2prm2cr = SCFG_RST_USB2PRM2CR;
    s->usb2prm3cr = SCFG_RST_USB2PRM3CR;
    s->usb3prm1cr = SCFG_RST_USB3PRM1CR;
    s->usb3prm2cr = SCFG_RST_USB3PRM2CR;
    s->usb3prm3cr = SCFG_RST_USB3PRM3CR;
    s->qspi_cfg = SCFG_RST_QSPI_CFG;
    s->snpcnfgcr = SCFG_RST_SNPCNFGCR;
    s->intpcr = SCFG_RST_INTPCR;
    s->rcwpmuxcr0 = SCFG_RST_RCWPMUXCR0;
    s->usbdrvvbus_selcr = SCFG_RST_USBDRVVBUS_SELCR;
    s->usbpwrfault_selcr = SCFG_RST_USBPWRFAULT_SELCR;
    s->scratchrw[0] = SCFG_RST_SCRATCHRWn;
    s->scratchrw[1] = SCFG_RST_SCRATCHRWn;
    s->scratchrw[2] = SCFG_RST_SCRATCHRWn;
    s->scratchrw[3] = SCFG_RST_SCRATCHRWn;
    s->corebcr = SCFG_RST_COREBCR;
}

static void ccsr_guts_initfn(Object *obj)
{
    static const MemoryRegionOps guts_ops = {
        .read = ccsr_guts_read,
        .write = ccsr_guts_write,
        .endianness = DEVICE_BIG_ENDIAN,
        .valid = {
            .min_access_size = 4,
            .max_access_size = 4,
        }
    };

    SysBusDevice *d = SYS_BUS_DEVICE(obj);
    CCSRGUtsState *s = CCSR_GUTS(obj);

    memory_region_init_io(&s->iomem, OBJECT(s), &guts_ops, s,
                          "ccsr.guts", GUTS_MMIO_SIZE);
    sysbus_init_mmio(d, &s->iomem);
}

static void ccsr_clk_initfn(Object *obj)
{
    static const MemoryRegionOps clk_ops = {
        .read = ccsr_clk_read,
        .write = ccsr_clk_write,
        .endianness = DEVICE_BIG_ENDIAN,
        .valid = {
            .min_access_size = 4,
            .max_access_size = 4,
        }
    };

    SysBusDevice *d = SYS_BUS_DEVICE(obj);
    CCSRClkState *s = CCSR_CLK(obj);

    memory_region_init_io(&s->iomem, OBJECT(s), &clk_ops, s,
                          "ccsr.clk", CLK_MMIO_SIZE);
    sysbus_init_mmio(d, &s->iomem);

    s->clkccsr = CLK_RST_CLKCCSR;
}

static const TypeInfo ccsr_ddr_info = {
    .name          = TYPE_CCSR_DDR,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(CCSRDDRState),
    .instance_init = ccsr_ddr_initfn
};

static const TypeInfo ccsr_scfg_info = {
    .name          = TYPE_CCSR_SCFG,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(CCSRSCfgState),
    .instance_init = ccsr_scfg_initfn
};

static const TypeInfo ccsr_guts_info = {
    .name          = TYPE_CCSR_GUTS,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(CCSRGUtsState),
    .instance_init = ccsr_guts_initfn
};

static const TypeInfo ccsr_clk_info = {
    .name          = TYPE_CCSR_CLK,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(CCSRClkState),
    .instance_init = ccsr_clk_initfn
};

static void ls1046a_ccsr_register_types(void)
{
    type_register_static(&ccsr_ddr_info);
    type_register_static(&ccsr_scfg_info);
    type_register_static(&ccsr_guts_info);
    type_register_static(&ccsr_clk_info);
}

type_init(ls1046a_ccsr_register_types)
