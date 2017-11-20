/*
 * Copyright (c) 2017 t-kenji <protect.2501@gmail.com>
 *
 * LS1046A MultiMediaCard/SD/SDIO Controller emulation.
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
#include "sysemu/dma.h"
#include "hw/sd/ls1_mmci.h"
#include "qemu/log.h"


#define LS1_MMCI_DEBUG


#define TYPE_LS1_MMCI_BUS "ls1-mmci-bus"
#define LS1_MMCI_BUS(obj) OBJECT_CHECK(SDBus, (obj), TYPE_LS1_MMCI_BUS)

#define REG_DSADDR_BLKATTR2 (0x000)
#define REG_BLKATTR (0x004)
#define REG_CMDARG (0x008)
#define REG_XFERTYP (0x00C)
#define REG_CMDRSP0 (0x010)
#define REG_CMDRSP1 (0x014)
#define REG_CMDRSP2 (0x018)
#define REG_CMDRSP3 (0x01C)
#define REG_DATPORT (0x020)
#define REG_PRSSTAT (0x024)
#define REG_PROCTL (0x028)
#define REG_SYSCTL (0x02C)
#define REG_IRQSTAT (0x030)
#define REG_IRQSTATEN (0x034)
#define REG_IRQSIGEN (0x038)
#define REG_AUTOCERR_SYSCTL2 (0x03C)
#define REG_HOSTCAPBLT (0x040)
#define REG_WML (0x044)
#define REG_FEVT (0x050)
#define REG_ADMAES (0x054)
#define REG_ADSADDR (0x058)
#define REG_HOSTVER (0x0FC)
#define REG_DMAERRADDR (0x104)
#define REG_DMAERRATTR (0x10C)
#define REG_HOSTCAPBLT2 (0x114)
#define REG_TBCTL (0x120)
#define REG_TBPTR (0x128)
#define REG_SDDIRCTL (0x140)
#define REG_SDCLKCTL (0x144)
#define REG_ESDHCCTL (0x40C)

/*
 * SDMA system address register / Block attribute 2 (DSADDR_BLKATTR2)
 *  DS_ADDR: SDMA system address or Block attribute 2
 */
#define RST_DSADDR_BLKATTR2 (0x00000000)
#define MSK_DSADDR_BLKATTR2 (0xFFFFFFFF)

/*
 * Block attributes register (BLKATTR)
 *  BLKCNT: Stop count
 *  BLKSIZE: No data transfer
 */
#define RST_BLKATTR (0x00000000)
#define MSK_BLKATTR (0xFFFF0FFF)
#define BIT_BLKATTR_BLKCNT (16)
#define BIT_BLKATTR_BLKSIZE (0)
#define PICK_BLKATTR_BLKCNT(v) (((v) >> BIT_BLKATTR_BLKCNT) & 0xFFFF)
#define PICK_BLKATTR_BLKSIZE(v) (((v) >> BIT_BLKATTR_BLKSIZE) & 0x0FFF)
#define SUB_BLKATTR_BLKCNT(v, n) ((v) - ((n) << BIT_BLKATTR_BLKCNT))

/*
 * Command argument register (CMDARG)
 */
#define RST_CMDARG (0x00000000)
#define MSK_CMDARG (0xFFFFFFFF)

/*
 * Transfer type register (XFERTYP)
 *  CMDINX: 0
 *  CMDTYP: Normal other commands
 *  DPSEL: No data present
 *  CICEN: Disable
 *  CCCEN: Disable
 *  RSPTYP: No response
 *  MSBSEL: Single block
 *  DTDSEL: Write (host to card)
 *  ACEN: Auto CMD disable
 *  BCEN: Disable
 *  DMAEN: Disable
 */
#define RST_XFERTYP (0x00000000)
#define MSK_XFERTYP (0x3FFb003F)
#define BIT_XFERTYP_CMDINX (24)
#define BIT_XFERTYP_CMDTYP (22)
#define BIT_XFERTYP_DPSEL (21)
#define BIT_XFERTYP_CICEN (20)
#define BIT_XFERTYP_CCCEN (19)
#define BIT_XFERTYP_RSPTYP (16)
#define BIT_XFERTYP_MSBSEL (5)
#define BIT_XFERTYP_DTDSEL (4)
#define BIT_XFERTYP_ACEN (2)
#define BIT_XFERTYP_BCEN (1)
#define BIT_XFERTYP_DMAEN (0)
#define PICK_XFERTYP_CMDINX(v) (((v) >> BIT_XFERTYP_CMDINX) & 0x3F)
#define PICK_XFERTYP_DPSEL(v) (((v) >> BIT_XFERTYP_DPSEL) & 0x01)
#define PICK_XFERTYP_RSPTYP(v) (((v) >> BIT_XFERTYP_RSPTYP) & 0x03)
#define PICK_XFERTYP_MSBSEL(v) (((v) >> BIT_XFERTYP_MSBSEL) & 0x01)
#define PICK_XFERTYP_DTDSEL(v) (((v) >> BIT_XFERTYP_DTDSEL) & 0x01)
#define PICK_XFERTYP_ACEN(v) (((v) >> BIT_XFERTYP_ACEN) & 0x03)
#define PICK_XFERTYP_BCEN(v) (((v) >> BIT_XFERTYP_BCEN) & 0x01)
#define PICK_XFERTYP_DMAEN(v) (((v) >> BIT_XFERTYP_DMAEN) & 0x01)
#define VAL_XFERTYP_ACEN_ACMD12 (0x01)
#define VAL_XFERTYP_ACEN_ACMD23 (0x02)

/*
 * Present state register (PRSSTAT)
 *  DLSL: DAT[7:0] line signal level
 *  CLSL: CMD line signal level
 *  WPS: Write enable (SDHC_WP=0)
 *  CDS: Card present (SDHC_CD_B=0)
 *  CINS: Card inserted
 *  BREN: Buffer read disable
 *  BWEN: Buffer write disable
 *  RTA: No valid data
 *  WTA: No valid data
 *  SDOFF: SD clock is active
 *  SDSTB: Clock is stable
 *  DLA: DAT line inactive
 *  CDIHB: Can issue command which uses the DAT line
 *  CIHB: Can issue command using only CMD line
 */
#define RST_PRSSTAT (0xFF8D0088)
#define BIT_PRSSTAT_BREN (11)
#define BIT_PRSSTAT_BWEN (10)
#define BIT_PRSSTAT_RTA (9)
#define BIT_PRSSTAT_WTA (8)
#define BIT_PRSSTAT_DLA (2)
#define BIT_PRSSTAT_CDIHB (1)
#define BIT_PRSSTAT_CIHB (0)

/*
 * Protocol control register (PROCTL)
 *  WECRM: Disable
 *  WECINS: Disable
 *  WECINT: Disable
 *  IABG: Disable
 *  RWCTL: Disable read wait control, and stop SD clock at block gap when
 *         SABGREQ bit is set
 *  CREQ: No effect
 *  SABGREG: Transfer
 *  VOLT_SEL: Change the SD bus Supply voltage to high voltage range,
 *            around 3.0V
 *  DMAS: Single DMA is selected
 *  CDSS: SD CD pin is selected (for normal purposes)
 *  CDTL: Card detect the level is 0, no card inserted
 *  EMODE: Little endian mode
 *  DTW: 1-bit mode
 */
#define RST_PROCTL (0x00000020)
#define MSK_PROCTL (0x070F07F6)
#define BIT_PROCTL_CREQ (17)
#define BIT_PROCTL_SABGREG (16)
#define BIT_PROCTL_DMAS (8)
#define PICK_PROCTL_CREQ(v) (((v) >> BIT_PROCTL_CREQ) & 0x01)
#define PICK_PROCTL_SABGREG(v) (((v) >> BIT_PROCTL_SABGREG) & 0x01)
#define PICK_PROCTL_DMAS(v) (((v) >> BIT_PROCTL_DMAS) & 0x03)
#define VAL_PROCTL_DMAS_SDMA (0)
#define VAL_PROCTL_DMAS_ADMA1 (1)
#define VAL_PROCTL_DMAS_ADMA2_32 (2)

/*
 * System Control Register when ESDHCCTL[CRS=0] (SYSCTL_ESDHCCTL_CRS_0)
 *  INITA: Initialization deactive (cleared)
 *  RSTD: No reset
 *  RSTC: No reset
 *  RSTA: No reset
 *  DTOCV: SDCLK x 2^13
 *  SDCLKFS: Base clock divided by 256
 *  DVS: Divisor by 3
 *  SDCLKEN: SD clock enable
 */
#define RST_SYSCTL (0x00008038)
#define MSK_SYSCTL (0x0F0FFFF8)
#define BIT_SYSCTL_INITA (27)
#define BIT_SYSCTL_RSTD (26)
#define BIT_SYSCTL_RSTC (25)
#define BIT_SYSCTL_RSTA (24)
#define PICK_SYSCTL_INITA(v) (((v) >> BIT_SYSCTL_INITA) & 0x01)
#define PICK_SYSCTL_RSTD(v) (((v) >> BIT_SYSCTL_RSTD) & 0x01)
#define PICK_SYSCTL_RSTC(v) (((v) >> BIT_SYSCTL_RSTC) & 0x01)
#define PICK_SYSCTL_RSTA(v) (((v) >> BIT_SYSCTL_RSTA) & 0x01)

/*
 * Interrupt status register (IRQSTAT)
 *  RTOE: No timeout error
 *  DMAE: No error
 *  TNE: No error
 *  ADMAE: No error
 *  AC12E: No error
 *  DEBE: No error
 *  DCE: No error
 *  DTOE: No error
 *  CIE: No error
 *  CEBE: No error
 *  CCE: No error
 *  CTOE: No error
 *  RTE: Re-tuning is not required
 *  CINT: No card interrupt
 *  CRM: Card state unstable or inserted
 *  CINS: Card state unstable or removed
 *  BRR: Not ready to read buffer
 *  BWR: Not ready to write buffer
 *  DINT: No DMA interrupt
 *  BGE: No block gap event
 *  TC: Transfer not complete
 *  CC: Command not complete
 */
#define RST_IRQSTAT (0x00000000)
#define MSK_IRQSTAT (0x377F11FF)
#define BIT_IRQSTAT_RTOE (29)
#define BIT_IRQSTAT_DMAE (28)
#define BIT_IRQSTAT_TNE (26)
#define BIT_IRQSTAT_ADMAE (25)
#define BIT_IRQSTAT_AC12E (24)
#define BIT_IRQSTAT_DEBE (22)
#define BIT_IRQSTAT_DCE (21)
#define BIT_IRQSTAT_DTOE (20)
#define BIT_IRQSTAT_CIE (19)
#define BIT_IRQSTAT_CEBE (18)
#define BIT_IRQSTAT_CCE (17)
#define BIT_IRQSTAT_CTOE (16)
#define BIT_IRQSTAT_RTE (12)
#define BIT_IRQSTAT_CINT (8)
#define BIT_IRQSTAT_CRM (7)
#define BIT_IRQSTAT_CINS (6)
#define BIT_IRQSTAT_BRR (5)
#define BIT_IRQSTAT_BWR (4)
#define BIT_IRQSTAT_DINT (3)
#define BIT_IRQSTAT_BGE (2)
#define BIT_IRQSTAT_TC (1)
#define BIT_IRQSTAT_CC (0)

/*
 * Interrupt status enable register (IRQSTATEN)
 *  RTOESEN: Enabled
 *  DMAESEN: Enabled
 *  TNESEN: Enabled
 *  ADMAESEN: Enabled
 *  AC12ESEN: Enabled
 *  DEBESEN: Enabled
 *  DCESEN: Enabled
 *  DTOESEN: Enabled
 *  CIESEN: Enabled
 *  CEBESEN: Enabled
 *  CCESEN: Enabled
 *  CTOESEN: Enabled
 *  RTESEN: Enabled
 *  CINTSEN: Enabled
 *  CRMSEN: Enabled
 *  CINSEN: Enabled
 *  BRRSEN: Enabled
 *  BWRSEN: Enabled
 *  DINTSEN: Enabled
 *  BGESEN: Enabled
 *  TCSEN: Enabled
 *  CCSEN: Enabled
 */
#define RST_IRQSTATEN (0x377F11FF)
#define MSK_IRQSTATEN (0x377F11FF)
#define BIT_IRQSTATEN_RTOE (29)
#define BIT_IRQSTATEN_DMAE (28)
#define BIT_IRQSTATEN_TNE (26)
#define BIT_IRQSTATEN_ADMAE (25)
#define BIT_IRQSTATEN_AC12E (24)
#define BIT_IRQSTATEN_DEBE (22)
#define BIT_IRQSTATEN_DCE (21)
#define BIT_IRQSTATEN_DTOE (20)
#define BIT_IRQSTATEN_CIE (19)
#define BIT_IRQSTATEN_CEBE (18)
#define BIT_IRQSTATEN_CCE (17)
#define BIT_IRQSTATEN_CTOE (16)
#define BIT_IRQSTATEN_RTE (12)
#define BIT_IRQSTATEN_CINT (8)
#define BIT_IRQSTATEN_CRM (7)
#define BIT_IRQSTATEN_CINS (6)
#define BIT_IRQSTATEN_BRR (5)
#define BIT_IRQSTATEN_BWR (4)
#define BIT_IRQSTATEN_DINT (3)
#define BIT_IRQSTATEN_BGE (2)
#define BIT_IRQSTATEN_TC (1)
#define BIT_IRQSTATEN_CC (0)
#define PICK_IRQSTATEN_ADMAE(v) (((v) >> BIT_IRQSTAT_ADMAE) & 0x01)
#define PICK_IRQSTATEN_DTOE(v) (((v) >> BIT_IRQSTAT_DTOE) & 0x01)
#define PICK_IRQSTATEN_CTOE(v) (((v) >> BIT_IRQSTATEN_CTOE) & 0x01)
#define PICK_IRQSTATEN_DINT(v) (((v) >> BIT_IRQSTATEN_DINT) & 0x01)
#define PICK_IRQSTATEN_TC(v) (((v) >> BIT_IRQSTATEN_TC) & 0x01)
#define PICK_IRQSTATEN_CC(v) (((v) >> BIT_IRQSTATEN_CC) & 0x01)

/*
 * Interrupt signal enable register (IRQSIGEN)
 *  RTOEIEN: Masked
 *  DMAEIEN: Masked
 *  TNEIEN: Masked
 *  ADMAEIEN: ADMA Error Interrupt Enable
 *  AC12EIEN: Masked
 *  DEBEIEN: Masked
 *  DCEIEN: Masked
 *  DTOEIEN: Masked
 *  CIEIEN: Masked
 *  CEBEIEN: Masked
 *  CCEIEN: Masked
 *  CTOEIEN: Masked
 *  RTEIEN: Enabled
 *  CINTIEN: Masked
 *  CRMIEN: Masked
 *  CINSIEN: Masked
 *  BRRIEN: Masked
 *  BWRIEN: Masked
 *  DINTIEN: Masked
 *  BGEIEN: Masked
 *  TCIEN: Masked
 *  CCIEN: Masked
 */
#define RST_IRQSIGEN (0x04001000)
#define MSK_IRQSIGEN (0x377F11FF)

/*
 * Auto CMD Error Status Register / System Control 2 Register (AUTOCERR_SYSCTL2)
 *  AIE: Disabled
 *  SMPCLKSEL: Tuning procedure unsuccessful
 *  EXTN: Not tuned or tuning not compelted
 *  UHSM: SDR12 for SD, or max 52 MHz mode for SD 2.0 / eMMC 4.2 or older spec
 *  CNIBAC12E: No error
 *  AC12IE: No error
 *  AC12EBE: No error
 *  AC12CE: No CRC error
 *  AC12TOE: No error
 *  AC12NE: Executed
 */
#define RST_AUTOCERR_SYSCTL2 (0x00000000)
#define MSK_AUTOCERR_SYSCTL2 (0x40C70000)

/*
 * Host controller capabilities register (HOSTCAPBLT)
 *  AIS: Asynchronous interrupt supported
 *  SBS64B: 64-bit system bus not supported
 *  VS18: 1.8V supported
 *  VS30: 3.0V not supported
 *  VS33: 3.3V not supported
 *  SRS: Supported
 *  DMAS: DMA supported
 *  HSS: High speed supported
 *  ADMAS: Advanced DMA supported
 *  MBL: 2048 bytes
 */
#define RST_HOSTCAPBLT (0x24F20000)
#define BIT_HOSTCAPBLT_ADMAS (20)
#define PICK_HOSTCAPBLT_ADMAS(v) (((v) >> BIT_HOSTCAPBLT_ADMAS) & 0x01)

/*
 * Watermark level register (WML)
 *  WR_BRST_LEN: 16 transfers in a single burst
 *  WR_WML: 16 words
 *  RD_BRST_LEN: 16 transfers in a single burst
 *  RD_WML: 16 words
 */
#define RST_WML (0x00100010)
#define MSK_WML (0x0F7F0F7F)

#define RST_FEVT (0)

/*
 * ADMA error status register (ADMAES)
 *  ADMABE: No error
 *  ADMADCE: No error
 *  ADMALME: No error
 *  ADMAES: idle
 */
#define RST_ADMAES (0x00000000)
#define BIT_ADMAES_ADMABE (4)
#define BIT_ADMAES_ADMADCE (3)
#define BIT_ADMAES_ADMALME (2)
#define BIT_ADMAES_ADMAES (0)
#define MSK_ADMAES_ADMAES (0x00000003)
#define VAL_ADMAES_ADMAES_IDLE (0x0)
#define VAL_ADMAES_ADMAES_FETCH_DESC (0x1)
#define VAL_ADMAES_ADMAES_DATA_XFER (0x2)
#define VAL_ADMAES_ADMAES_WAIT_STOP (0x3)

/*
 * ADMA system address register (ADSADDR)
 *  ADS_ADDR: ADMA system address
 */
#define RST_ADSADDR (0x00000000)
#define MSK_ADSADDR (0xFFFFFFFF)

/*
 * Host controller version register (HOSTVER)
 *  VVN: eSDHC Version 3.2
 *  SVN: SD Host Specification Version 3.0
 */
#define RST_HOSTVER  (0x00002202)

#define RST_DMAERRADDR (0)
#define RST_DMAERRATTR (0)

/*
 * Host controller capabilitiesregister 2 (HOSTCAPBLT2)
 *  RTM: Mode 3 - Software Timer, and Auto Re-tuning during data transfer
 *  UTSDR50: Tuning for SDR50 mode supported
 *  TCRT: Get timer infomation fromother source
 *  DTDS: Driver Type D not supported
 *  DTCS: Driver Type C not supported
 *  DTAS: Driver Type A not supported
 *  DDR50: DDR mode supported
 *  SDR104: SDR104 supported
 *  SDR50: SDR50 supported
 */
#define RST_HOSTCAPBLT2 (0x0000CF07)

#define RST_TBCTL (0)
#define RST_TBPTR (0)
#define RST_SDDIRCTL (0)
#define RST_SDCLKCTL (0)

/*
 * eSDHC control register (ESDHCCTL)
 *  PTOCV: Timeout count value for access is 2^10 clocks
 *  PCS: Platform clock is used
 *  FAF: No Flush
 *  RTR: No re-tuning request
 *  CRS: SDCLKFS is defined as 8-bit field, and DVS is active in system control
 *       register
 *  RD_PRFTCH_BLKCNT: No prefetch
 *  PAD_DIS: DMA will pad data at the end each block transfer
 *  SNOOP: DMA transactions are not snooped by the CPU data cache
 *  WR_BUF: Non-last write transactions are not bufferable
 *  RD_SAFE: It is not safe to read morebytes that ware intended
 */
#define RST_ESDHCCTL (0x00000000)
#define MSK_ESDHCCTL (0x003B1FCC)

#define SDHC_ADMA_ATTR_SET_LEN         (1 << 4)
#define SDHC_ADMA_ATTR_ACT_TRAN        (1 << 5)
#define SDHC_ADMA_ATTR_ACT_LINK        (3 << 4)
#define SDHC_ADMA_ATTR_INT             (1 << 2)
#define SDHC_ADMA_ATTR_END             (1 << 1)
#define SDHC_ADMA_ATTR_VALID           (1 << 0)
#define SDHC_ADMA_ATTR_ACT_MASK        ((1 << 4)|(1 << 5))
#define SDHC_TRANSFER_DELAY             100
#define SDHC_ADMA_DESCS_PER_DELAY       5

#define REG_FMT TARGET_FMT_plx

#if defined(LS1_MMCI_DEBUG)
#define dprintf(...) qemu_log(__VA_ARGS__)
#else
#define dprintf(...)
#endif


static bool ls1_mmci_vmstate_validate(void *opaque, int version_id)
{
    LS1MMCIState *s = opaque;

    return (s->tx_start < ARRAY_SIZE(s->tx_fifo))
           && (s->rx_start < ARRAY_SIZE(s->rx_fifo))
           && (s->tx_len <= ARRAY_SIZE(s->tx_fifo))
           && (s->rx_len <= ARRAY_SIZE(s->rx_fifo));
}

static const VMStateDescription vmstate_ls1_mmci = {
    .name = "ls1-mmci",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(ds_addr, LS1MMCIState),
        VMSTATE_UINT32(cmdarg, LS1MMCIState),
        VMSTATE_UINT32(xfertyp, LS1MMCIState),
        VMSTATE_UINT32(proctl, LS1MMCIState),
        VMSTATE_UINT32(sysctl, LS1MMCIState),
        VMSTATE_UINT32(irqstat, LS1MMCIState),
        VMSTATE_UINT32(irqstaten, LS1MMCIState),
        VMSTATE_UINT32(irqsigen, LS1MMCIState),
        VMSTATE_UINT32(autocerr_sysctl2, LS1MMCIState),
        VMSTATE_UINT32(hostcapblt, LS1MMCIState),
        VMSTATE_UINT32(wml, LS1MMCIState),
        VMSTATE_UINT32(admaes, LS1MMCIState),
        VMSTATE_UINT32(adsaddr, LS1MMCIState),
        VMSTATE_UINT32(hostver, LS1MMCIState),
        VMSTATE_UINT32(hostcapblt2, LS1MMCIState),
        VMSTATE_UINT32(esdhcctl, LS1MMCIState),
        VMSTATE_UINT32(tx_start, LS1MMCIState),
        VMSTATE_UINT32(tx_len, LS1MMCIState),
        VMSTATE_UINT32(rx_start, LS1MMCIState),
        VMSTATE_UINT32(rx_len, LS1MMCIState),
        VMSTATE_VALIDATE("fifo size incorrect", ls1_mmci_vmstate_validate),
        VMSTATE_UINT32_ARRAY(cmdrsp, LS1MMCIState, 4),
        VMSTATE_UINT32_ARRAY(tx_fifo, LS1MMCIState, LS1_MMCI_FIFO_SIZE),
        VMSTATE_UINT32_ARRAY(rx_fifo, LS1MMCIState, LS1_MMCI_FIFO_SIZE),
        VMSTATE_END_OF_LIST()
    }
};

static const char *get_reg_name(hwaddr offset)
{
    switch (offset) {
    case REG_DSADDR_BLKATTR2:
        return "DSADDR_BLKATTR2";
    case REG_BLKATTR:
        return "BLKATTR";
    case REG_CMDARG:
        return "CMDARG";
    case REG_XFERTYP:
        return "XFERTYP";
    case REG_CMDRSP0:
        return "CMDRSP0";
    case REG_CMDRSP1:
        return "CMDRSP1";
    case REG_CMDRSP2:
        return "CMDRSP2";
    case REG_CMDRSP3:
        return "CMDRSP3";
    case REG_DATPORT:
        return "DATPORT";
    case REG_PRSSTAT:
        return "PRSSTAT";
    case REG_PROCTL:
        return "PROCTL";
    case REG_SYSCTL:
        return "SYSCTL";
    case REG_IRQSTAT:
        return "IRQSTAT";
    case REG_IRQSTATEN:
        return "IRQSTATEN";
    case REG_IRQSIGEN:
        return "IRQSIGEN";
    case REG_AUTOCERR_SYSCTL2:
        return "AUTOCERR_SYSCTL2";
    case REG_HOSTCAPBLT:
        return "HOSTCAPBLT";
    case REG_WML:
        return "WML";
    case REG_FEVT:
        return "FEVT";
    case REG_ADMAES:
        return "ADMAES";
    case REG_ADSADDR:
        return "ADSADDR";
    case REG_HOSTVER:
        return "HOSTVER";
    case REG_DMAERRADDR:
        return "DMAERRADDR";
    case REG_DMAERRATTR:
        return "DMAERRATTR";
    case REG_HOSTCAPBLT2:
        return "HOSTCAPBLT2";
    case REG_TBCTL:
        return "TBCTL";
    case REG_TBPTR:
        return "TBPTR";
    case REG_SDDIRCTL:
        return "SDDIRCTL";
    case REG_SDCLKCTL:
        return "SDCLKCTL";
    case REG_ESDHCCTL:
        return "ESDHCCTL";
    default:
        return "UNKNOWN";
    }
}

static void ls1_mmci_reset_fifo(LS1MMCIState *s)
{
    s->data_left = 0;
    s->tx_len = 0;
    s->rx_len = 0;
    memset(s->tx_fifo, 0, sizeof(s->tx_fifo));
    memset(s->rx_fifo, 0, sizeof(s->rx_fifo));
}

static uint32_t ls1_mmci_sdbus_read(LS1MMCIState *s)
{
    return (uint32_t)(sdbus_read_data(&s->sdbus)
                      | (sdbus_read_data(&s->sdbus) << 8)
                      | (sdbus_read_data(&s->sdbus) << 16)
                      | (sdbus_read_data(&s->sdbus) << 24));
}

static void ls1_mmci_sdbus_write(LS1MMCIState *s, uint32_t value)
{
    sdbus_write_data(&s->sdbus, value & 0xFF);
    sdbus_write_data(&s->sdbus, (value >> 8) & 0xFF);
    sdbus_write_data(&s->sdbus, (value >> 16) & 0xFF);
    sdbus_write_data(&s->sdbus, (value >> 24) & 0xFF);
}

static void ls1_mmci_int_update(LS1MMCIState *s)
{
    uint32_t mask = s->irqstaten;
#if 0
    if (s->cmdat & CMDAT_DMA_EN) {
        mask |= INT_RXFIFO_REQ | INT_TXFIFO_REQ;

        qemu_set_irq(s->rx_dma, !!(s->intreq & INT_RXFIFO_REQ));
        qemu_set_irq(s->tx_dma, !!(s->intreq & INT_TXFIFO_REQ));
    }
#endif

    qemu_set_irq(s->irq, !!(s->irqstat & mask));
}

static void ls1_mmci_end_transfer(LS1MMCIState *s)
{
    /* Automatically send CMD12 / CMD23 to stop transfer if Auto CMD12 / CMD23 enabled */
    if (PICK_XFERTYP_ACEN(s->xfertyp)) {
        SDRequest req;
        uint8_t rsp[16];

        req.cmd = 0x0C;
        req.arg = 0;
        dprintf("%s:%d$ Automatically issue CMD%d %08x\n", __func__, __LINE__, req.cmd, req.arg);
        sdbus_do_command(&s->sdbus, &req, rsp);
        /* Auto CMD12 respose goes to the upper Response register */
        s->cmdrsp[0] = (rsp[0] << 24) | (rsp[1] << 16) | (rsp[2] << 8) | rsp[3];
    }

    s->prsstat &= ~((1 << BIT_PRSSTAT_RTA) | (1 << BIT_PRSSTAT_WTA)
                    | (1 << BIT_PRSSTAT_DLA) | (1 << BIT_PRSSTAT_CDIHB));
    if (PICK_IRQSTATEN_TC(s->irqstaten)) {
        s->irqstat |= (1 << BIT_IRQSTAT_TC);
    }
    if (PICK_IRQSTATEN_DINT(s->irqstaten) && PICK_XFERTYP_DMAEN(s->xfertyp)) {
        s->irqstat |= (1 << BIT_IRQSTAT_DINT);
    }

    ls1_mmci_int_update(s);
}

static void ls1_mmci_sdma_transfer_single_block(LS1MMCIState *s)
{
    uint32_t data_cnt = PICK_BLKATTR_BLKSIZE(s->blkattr) / sizeof(uint32_t);
    uint32_t phys_addr = s->ds_addr;
    uint32_t data_size;

    if (PICK_XFERTYP_DTDSEL(s->xfertyp)) {
        do {
            for (s->rx_len = 0;
                 (s->rx_len < LS1_MMCI_FIFO_SIZE) && (data_cnt > 0);
                ++s->rx_len, --data_cnt) {

                s->rx_fifo[s->rx_len] = ls1_mmci_sdbus_read(s);
            }
            data_size = s->rx_len * sizeof(uint32_t);
            dma_memory_write(&address_space_memory, phys_addr,
                             s->rx_fifo, data_size);
            phys_addr += data_size;
        } while (data_cnt > 0);
    } else {
        do {
            s->tx_len = (data_cnt < LS1_MMCI_FIFO_SIZE) ? data_cnt : LS1_MMCI_FIFO_SIZE;
            data_size = s->tx_len * sizeof(uint32_t);
            dma_memory_read(&address_space_memory, phys_addr,
                            s->tx_fifo, data_size);
            for (s->tx_start = 0; s->tx_start < s->tx_len; ++s->tx_start) {
                ls1_mmci_sdbus_write(s, s->tx_fifo[s->tx_start]);
            }
            phys_addr += data_size;
            data_cnt -= s->tx_len;
        } while (data_cnt > 0);
    }

    s->data_left -= PICK_BLKATTR_BLKSIZE(s->blkattr) / sizeof(uint32_t);

    ls1_mmci_end_transfer(s);
}

static void ls1_mmci_sdma_transfer_multi_block(LS1MMCIState *s)
{
    uint32_t phys_addr = s->ds_addr;
    uint32_t data_size;

    if (PICK_XFERTYP_DTDSEL(s->xfertyp)) {
        s->prsstat |= (1 << BIT_PRSSTAT_RTA) | (1 << BIT_PRSSTAT_DLA)
                      | (1 << BIT_PRSSTAT_CDIHB);
        do {
            for (s->rx_len = 0; s->rx_len < LS1_MMCI_FIFO_SIZE; ++s->rx_len) {
                s->rx_fifo[s->rx_len] = ls1_mmci_sdbus_read(s);
            }
            data_size = s->rx_len * sizeof(s->rx_fifo[0]);
            dma_memory_write(&address_space_memory, phys_addr,
                             s->rx_fifo, data_size);
            phys_addr += data_size;
            s->data_left -= s->rx_len;
        } while (s->data_left > 0);
    } else {
        s->prsstat |= (1 << BIT_PRSSTAT_WTA) | (1 << BIT_PRSSTAT_DLA)
                      | (1 << BIT_PRSSTAT_CDIHB);
        do {
            s->tx_len = LS1_MMCI_FIFO_SIZE;
            data_size = s->tx_len * sizeof(s->tx_fifo[0]);
            dma_memory_read(&address_space_memory, phys_addr,
                            s->tx_fifo, data_size);
            phys_addr += data_size;
            for (s->tx_start = 0; s->tx_start < s->tx_len; ++s->tx_start) {
                ls1_mmci_sdbus_write(s, s->tx_fifo[s->tx_start]);
            }
            s->data_left -= s->tx_len;
        } while (s->data_left > 0);
    }

    if (s->data_left == 0) {
        ls1_mmci_end_transfer(s);
    }
}

typedef struct ADMADesc {
    hwaddr addr;
    uint16_t length;
    uint8_t attr;
    uint8_t incr;
} ADMADesc;

static void ls1_mmci_adma_description(LS1MMCIState *s, ADMADesc *desc)
{
    uint32_t adma1 = 0;
    uint64_t adma2 = 0;
    hwaddr entry_addr = (hwaddr)s->adsaddr;

    switch (PICK_PROCTL_DMAS(s->proctl)) {
    case VAL_PROCTL_DMAS_ADMA1:
        dma_memory_read(&address_space_memory, entry_addr, (uint8_t *)&adma1,
                        sizeof(adma1));
        adma1 = le32_to_cpu(adma1);
        desc->addr = (hwaddr)(adma1 & 0xFFFFF000);
        desc->attr = (uint8_t)extract32(adma1, 0, 7);
        desc->incr = 4;
        if ((desc->attr & SDHC_ADMA_ATTR_ACT_MASK) == SDHC_ADMA_ATTR_SET_LEN) {
            desc->length = (uint16_t)extract32(adma1, 12, 16);
        } else {
            desc->length = 4096;
        }
        break;
    case VAL_PROCTL_DMAS_ADMA2_32:
        dma_memory_read(&address_space_memory, entry_addr, (uint8_t *)&adma2,
                        sizeof(adma2));
        adma2 = le64_to_cpu(adma2);
        /* The spec does not specify endianness of descriptor table.
         * We currently assume that it is LE.
         */
        desc->addr = (hwaddr)extract64(adma2, 32, 32) & ~0x3ULL;
        desc->length = (uint16_t)extract64(adma2, 16, 16);
        desc->attr = (uint8_t)extract64(adma2, 0, 7);
        desc->incr = 8;
        break;
    }
}

static void ls1_mmci_adma_transfer(LS1MMCIState *s)
{
    uint32_t length, data_size;
    ADMADesc desc;
    int i;

    for (i = 0; i < SDHC_ADMA_DESCS_PER_DELAY; ++i) {
        s->admaes &= ~(1 << BIT_ADMAES_ADMALME);

        ls1_mmci_adma_description(s, &desc);
        dprintf("ADMA loop: addr=" REG_FMT ", len=%d, attr=%x\n",
                desc.addr, desc.length, desc.attr);

        if ((desc.attr & SDHC_ADMA_ATTR_VALID) == 0) {
            s->admaes &= ~MSK_ADMAES_ADMAES;
            s->admaes |= VAL_ADMAES_ADMAES_FETCH_DESC;

            if (PICK_IRQSTATEN_ADMAE(s->irqstaten)) {
                s->irqstat |= (1 << BIT_IRQSTAT_ADMAE);
            }

            ls1_mmci_int_update(s);
            return;
        }

        length = desc.length ? desc.length : 65536;

        switch (desc.attr & SDHC_ADMA_ATTR_ACT_MASK) {
        case SDHC_ADMA_ATTR_ACT_TRAN:
            if (PICK_XFERTYP_DTDSEL(s->xfertyp)) {
                do {
                    for (s->rx_len = 0;
                         (s->data_left > 0) && (s->rx_len < LS1_MMCI_FIFO_SIZE);
                         ++s->rx_len) {

                        s->rx_fifo[s->rx_len] = ls1_mmci_sdbus_read(s);
                        --s->data_left;
                    }
                    data_size = s->rx_len * sizeof(s->rx_fifo[0]);
                    if (length < s->rx_len) {
                        length = 0;
                    } else {
                        length -= s->rx_len;
                    }
                    dma_memory_write(&address_space_memory, desc.addr,
                                     s->rx_fifo, data_size);
                    desc.addr += data_size;
                } while (s->data_left > 0);
            } else {
                do {
                    s->tx_len = LS1_MMCI_FIFO_SIZE;
                    data_size = s->tx_len * sizeof(s->tx_fifo[0]);
                    if (length < s->tx_len) {
                        length = 0;
                    } else {
                        length -= s->tx_len;
                    }
                    dma_memory_read(&address_space_memory, desc.addr,
                                    s->tx_fifo, data_size);
                    desc.addr += data_size;
                    for (s->tx_start = 0;
                         (s->data_left > 0) && (s->tx_start < s->tx_len);
                         ++s->tx_start) {

                        ls1_mmci_sdbus_write(s, s->tx_fifo[s->tx_start]);
                        --s->data_left;
                    }
                } while (s->data_left > 0);
            }
            s->adsaddr += desc.incr;
            break;
        case SDHC_ADMA_ATTR_ACT_LINK:
            s->adsaddr = desc.addr;
            dprintf("ADMA link: adsaddr=0x%08x\n", s->adsaddr);
            break;
        default:
            s->adsaddr += desc.incr;
            break;
        }

        if (desc.attr & SDHC_ADMA_ATTR_INT) {
            dprintf("ADMA interrupt: adsaddr=0x%08x\n", s->adsaddr);
            if (PICK_IRQSTATEN_DINT(s->irqstaten)) {
                s->irqstat |= (1 << BIT_IRQSTAT_DINT);
            }

            ls1_mmci_int_update(s);
        }

        /* ADMA transfer terminates if blkcnt == 0 or by END attribute */
        if ((PICK_XFERTYP_BCEN(s->xfertyp)
             && (PICK_BLKATTR_BLKCNT(s->blkattr) == 0))
            || (desc.attr & SDHC_ADMA_ATTR_END)) {

            dprintf("ADMA transfer completed\n");
            if ((length != 0) || ((desc.attr & SDHC_ADMA_ATTR_END) &&
                                  PICK_XFERTYP_BCEN(s->xfertyp) &&
                                  (PICK_BLKATTR_BLKCNT(s->blkattr) != 0))) {

                dprintf("SD/MMC host ADMA length mismatch\n");
                //s->admaes |= (1 << BIT_ADMAES_ADMALME) | VAL_ADMAES_ADMAES_DATA_XFER;
                if (PICK_IRQSTATEN_ADMAE(s->irqstaten)) {
                    dprintf("Set ADMA error flag\n");
                    //s->irqstat |= (1 << BIT_IRQSTAT_ADMAE);
                }

                //ls1_mmci_int_update(s);
            }
            ls1_mmci_end_transfer(s);
            return;
        }
    }

    /* we have unfinished business - reschedule to continue ADMA */
    timer_mod(s->transfer_timer,
              qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + SDHC_TRANSFER_DELAY);
}

static void ls1_mmci_read_block_from_card(LS1MMCIState *s)
{
    if (!sdbus_data_ready(&s->sdbus)) {
        dprintf("%s: data not ready", __func__);
        return;
    }

    while ((s->data_left > 0) && (s->rx_len < LS1_MMCI_FIFO_SIZE)) {
        uint32_t offset = (s->rx_start + (s->rx_len++))
                          & (LS1_MMCI_FIFO_SIZE - 1);
        s->rx_fifo[offset] = ls1_mmci_sdbus_read(s);
        --s->data_left;
    }

    if (s->data_left == 0) {
        ls1_mmci_end_transfer(s);
    }

    ls1_mmci_int_update(s);
}

static void ls1_mmci_write_block_to_card(LS1MMCIState *s)
{
    while ((s->data_left > 0) && (s->tx_len > 0)) {
        ls1_mmci_sdbus_write(s, s->tx_fifo[s->tx_start++]);
        s->tx_start &= LS1_MMCI_FIFO_SIZE - 1;
        --s->tx_len;
        --s->data_left;
    }

    if (s->data_left == 0) {
        ls1_mmci_end_transfer(s);
    }

    ls1_mmci_int_update(s);
}

static void ls1_mmci_data_transfer(void *opaque)
{
    LS1MMCIState *s = (LS1MMCIState *)opaque;

    if (PICK_XFERTYP_DMAEN(s->xfertyp)) {
        switch (PICK_PROCTL_DMAS(s->proctl)) {
        case VAL_PROCTL_DMAS_SDMA:
        {
            uint32_t data_cnt = PICK_BLKATTR_BLKCNT(s->blkattr)
                                / sizeof(uint32_t);
            if ((s->data_left <= data_cnt)
                || !PICK_XFERTYP_MSBSEL(s->xfertyp)) {

                ls1_mmci_sdma_transfer_single_block(s);
            } else {
                ls1_mmci_sdma_transfer_multi_block(s);
            }
            break;
        }
        case VAL_PROCTL_DMAS_ADMA1:
            if (!PICK_HOSTCAPBLT_ADMAS(s->hostcapblt)) {
                hw_error("%s: ADMA1 not supported\n", __func__);
            }
            ls1_mmci_adma_transfer(s);
            break;
        case VAL_PROCTL_DMAS_ADMA2_32:
            if (!PICK_HOSTCAPBLT_ADMAS(s->hostcapblt)) {
                hw_error("%s: ADMA2 not supported\n", __func__);
            }
            ls1_mmci_adma_transfer(s);
            break;
        default:
            hw_error("%s: Unsupported DMA type %d\n", __func__,
                     PICK_PROCTL_DMAS(s->proctl));
            break;
        }
    } else {
        s->prsstat |= (1 << BIT_PRSSTAT_DLA) | (1 << BIT_PRSSTAT_CDIHB);
        if (PICK_XFERTYP_DTDSEL(s->xfertyp)) {
            ls1_mmci_read_block_from_card(s);
        } else {
            ls1_mmci_write_block_to_card(s);
        }
    }
}

static void ls1_mmci_wakequeues(LS1MMCIState *s)
{
    int rsplen, i;
    SDRequest req;
    uint8_t rsp[16];

    req.cmd = PICK_XFERTYP_CMDINX(s->xfertyp);
    req.arg = s->cmdarg;
    req.crc = 0;	/* FIXME */

    rsplen = sdbus_do_command(&s->sdbus, &req, rsp);
    s->prsstat |= (1 << BIT_PRSSTAT_CIHB);

    switch (PICK_XFERTYP_RSPTYP(s->xfertyp)) {
    case 0: /* No response */
        goto complete;
    case 1: /* R2 */
    {
        static int reloc_idx[] = {
            15, 2, 13, 0, 11, 6, 9, 4, 7, 10, 5, 8, 3, 14, 1, 12
        };
        if (rsplen < 16) {
            printf("%s: rsplen %d (< 16)\n", __func__, rsplen);
            goto timeout;
        }
        for (i = 0; i < rsplen - 1; ++i) {
            rsp[reloc_idx[i]] = rsp[reloc_idx[i + 1]];
        }
        goto complete;
    }
    case 2: /* R1, R4, R5 or R6 */
        if (rsplen < 4) {
            printf("%s: rsplen %d (< 4)\n", __func__, rsplen);
            goto timeout;
        }
        goto complete;
    case 3:	/* R3 */
        if (rsplen < 4) {
            printf("%s: rsplen %d (< 4)\n", __func__, rsplen);
            goto timeout;
        }
        goto complete;
    complete:
        for (i = 0; rsplen > 0; ++i, rsplen -= 4) {
            s->cmdrsp[i] =   (rsp[i * 4 + 0] << 24)
                           + (rsp[i * 4 + 1] << 16)
                           + (rsp[i * 4 + 2] <<  8)
                           + (rsp[i * 4 + 3] <<  0);
        }
        s->prsstat &= ~(1 << BIT_PRSSTAT_CIHB);
        if (PICK_IRQSTATEN_CC(s->irqstaten)) {
            s->irqstat |= (1 << BIT_IRQSTAT_CC);
        }
        break;
    timeout:
        if (PICK_IRQSTATEN_CTOE(s->irqstaten)) {
            s->irqstat |= (1 << BIT_IRQSTAT_CTOE);
        }
        break;
    }

    ls1_mmci_int_update(s);

    if ((s->blkattr != 0) && PICK_XFERTYP_DPSEL(s->xfertyp)) {
        s->data_left = (PICK_BLKATTR_BLKSIZE(s->blkattr)
                        * PICK_BLKATTR_BLKCNT(s->blkattr)) / sizeof(uint32_t);
        ls1_mmci_data_transfer(s);
    }
}

static uint64_t ls1_mmci_read(void *opaque, hwaddr offset, unsigned size)
{
    LS1MMCIState *s = opaque;
    uint64_t value = 0;

    switch (offset) {
    case REG_DSADDR_BLKATTR2:
        value = s->ds_addr;
        break;
    case REG_BLKATTR:
        value = s->blkattr;
        break;
    case REG_CMDARG:
        value = s->cmdarg;
        break;
    case REG_XFERTYP:
        value = s->xfertyp;
        break;
    case REG_CMDRSP0:
        value = s->cmdrsp[0];
        break;
    case REG_CMDRSP1:
        value = s->cmdrsp[1];
        break;
    case REG_CMDRSP2:
        value = s->cmdrsp[2];
        break;
    case REG_CMDRSP3:
        value = s->cmdrsp[3];
        break;
    case REG_DATPORT:
        value = s->rx_fifo[s->rx_start++];
        s->rx_start &= LS1_MMCI_FIFO_SIZE - 1;
        --s->rx_len;
        ls1_mmci_data_transfer(s);
        break;
    case REG_PRSSTAT:
        value = s->prsstat;
        break;
    case REG_PROCTL:
        value = s->proctl;
        break;
    case REG_SYSCTL:
        value = s->sysctl;

        /* reset bit clear */
        s->sysctl &= 0x000FFFF8;
        break;
    case REG_IRQSTAT:
        value = s->irqstat;
        break;
    case REG_IRQSTATEN:
        value = s->irqstaten;
        break;
    case REG_IRQSIGEN:
        value = s->irqsigen;
        break;
    case REG_AUTOCERR_SYSCTL2:
        value = s->autocerr_sysctl2;
        break;
    case REG_HOSTCAPBLT:
        value = s->hostcapblt;
        break;
    case REG_WML:
        value = s->wml;
        break;
    case REG_FEVT:
        hw_error("%s: %s is not implemented\n", __func__, get_reg_name(offset));
        break;
    case REG_ADMAES:
        value = s->admaes;
        break;
    case REG_ADSADDR:
        value = s->adsaddr;
        break;
    case REG_HOSTVER:
        value = s->hostver;
        break;
    case REG_DMAERRADDR:
    case REG_DMAERRATTR:
        hw_error("%s: %s is not implemented\n", __func__, get_reg_name(offset));
        break;
    case REG_HOSTCAPBLT2:
        value = s->hostcapblt2;
        break;
    case REG_TBCTL:
    case REG_TBPTR:
    case REG_SDDIRCTL:
    case REG_SDCLKCTL:
        hw_error("%s: %s is not implemented\n", __func__, get_reg_name(offset));
        break;
    case REG_ESDHCCTL:
        value = s->esdhcctl;
        break;
    default:
        //hw_error("%s: Bad offset " REG_FMT "\n", __func__, offset);
        break;
    }
    dprintf("%s: %s > %" PRIx64 "\n", __func__, get_reg_name(offset), value);

    return value;
}

static void ls1_mmci_write(void *opaque,
                           hwaddr offset, uint64_t value, unsigned size)
{
    LS1MMCIState *s = opaque;

    dprintf("%s: %s < %" PRIx64 "\n", __func__, get_reg_name(offset), value);
    switch (offset) {
    case REG_DSADDR_BLKATTR2:
        s->ds_addr = value & MSK_DSADDR_BLKATTR2;
        break;
    case REG_BLKATTR:
        s->blkattr = value & MSK_BLKATTR;
        break;
    case REG_CMDARG:
        s->cmdarg = value & MSK_CMDARG;
        break;
    case REG_XFERTYP:
        if (!(s->prsstat & (1 << BIT_PRSSTAT_CDIHB))
            && !(s->prsstat & (1 << BIT_PRSSTAT_CIHB))) {

            s->xfertyp = value & MSK_XFERTYP;
            ls1_mmci_wakequeues(s);
        }
        break;
    case REG_DATPORT:
        if (s->tx_len < LS1_MMCI_FIFO_SIZE) {
            uint32_t tx_cur = (s->tx_start + (s->tx_len++))
                              & (LS1_MMCI_FIFO_SIZE - 1);
            s->tx_fifo[tx_cur] = value;
        }
        ls1_mmci_data_transfer(s);
        break;
    case REG_PROCTL:
        s->proctl = value & MSK_PROCTL;
        break;
    case REG_SYSCTL:
        s->sysctl = value & MSK_SYSCTL;

        if (PICK_SYSCTL_RSTD(s->sysctl)) {
            ls1_mmci_reset_fifo(s);
            s->prsstat &= ~((1 << BIT_PRSSTAT_BREN)
                            | (1 << BIT_PRSSTAT_BWEN)
                            | (1 << BIT_PRSSTAT_RTA)
                            | (1 << BIT_PRSSTAT_WTA)
                            | (1 << BIT_PRSSTAT_DLA)
                            | (1 << BIT_PRSSTAT_CDIHB)
                            | (1 << BIT_PRSSTAT_CIHB));
            s->proctl &= ~((1 << BIT_PROCTL_CREQ)
                           | (1 << BIT_PROCTL_SABGREG));
            s->irqstat &= ~((1 << BIT_IRQSTAT_BRR)
                            | (1 << BIT_IRQSTAT_BWR)
                            | (1 << BIT_IRQSTAT_DINT)
                            | (1 << BIT_IRQSTAT_BGE)
                            | (1 << BIT_IRQSTAT_TC));
        }
        if (PICK_SYSCTL_RSTC(s->sysctl)) {
            s->prsstat &= ~(1 << BIT_PRSSTAT_CIHB);
            s->irqstat &= ~(1 << BIT_IRQSTAT_CC);
        }
        ls1_mmci_int_update(s);
        break;
    case REG_IRQSTAT:
        s->irqstat &= 0xFFFFFFFF ^ (uint32_t)value;
        ls1_mmci_int_update(s);
        break;
    case REG_IRQSTATEN:
        s->irqstaten = value & MSK_IRQSTATEN;
        break;
    case REG_IRQSIGEN:
        s->irqsigen = value & MSK_IRQSIGEN;
        break;
    case REG_AUTOCERR_SYSCTL2:
        s->autocerr_sysctl2 = value & MSK_AUTOCERR_SYSCTL2;
        break;
    case REG_WML:
        s->wml = value & MSK_WML;
        break;
    case REG_ADSADDR:
        s->adsaddr = value & MSK_ADSADDR;
        break;
    case REG_ESDHCCTL:
        s->esdhcctl = value & MSK_ESDHCCTL;
        break;
    default:
        hw_error("%s: Bad offset " REG_FMT " (value %" PRIx64 ")\n",
                 __func__, offset, value);
        break;
    }
}

static const MemoryRegionOps ls1_mmci_ops = {
    .read = ls1_mmci_read,
    .write = ls1_mmci_write,
    .endianness = DEVICE_BIG_ENDIAN
};

static void ls1_mmci_reset(DeviceState *d)
{
    LS1MMCIState *s = LS1_MMCI(d);

    s->ds_addr = RST_DSADDR_BLKATTR2;
    s->blkattr = RST_BLKATTR;
    s->cmdarg = RST_CMDARG;
    s->xfertyp = RST_XFERTYP;
    s->prsstat = RST_PRSSTAT;
    s->proctl = RST_PROCTL;
    s->sysctl = RST_SYSCTL;
    s->irqstat = RST_IRQSTAT;
    s->irqstaten = RST_IRQSTATEN;
    s->irqsigen = RST_IRQSIGEN;
    s->autocerr_sysctl2 = RST_AUTOCERR_SYSCTL2;
    s->hostcapblt = RST_HOSTCAPBLT;
    s->wml = RST_WML;
    s->admaes = RST_ADMAES;
    s->adsaddr = RST_ADSADDR;
    s->hostver = RST_HOSTVER;
    s->esdhcctl = RST_ESDHCCTL;

    memset(s->cmdrsp, 0, sizeof(s->cmdrsp));
    ls1_mmci_reset_fifo(s);
}

static void ls1_mmci_instance_init(Object *obj)
{
    LS1MMCIState *s = LS1_MMCI(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->iomem, obj, &ls1_mmci_ops, s,
                          TYPE_LS1_MMCI, 0x10000);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);

    qbus_create_inplace(&s->sdbus, sizeof(s->sdbus),
                        TYPE_LS1_MMCI_BUS, DEVICE(obj), TYPE_SD_BUS);

    s->transfer_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, ls1_mmci_data_transfer, s);
}

static void ls1_mmci_instance_finalize(Object *obj)
{
    LS1MMCIState *s = LS1_MMCI(obj);

    timer_del(s->transfer_timer);
    timer_free(s->transfer_timer);
}

static void ls1_mmci_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->vmsd = &vmstate_ls1_mmci;
    dc->reset = ls1_mmci_reset;
}

static const TypeInfo ls1_mmci_info = {
    .name = TYPE_LS1_MMCI,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(LS1MMCIState),
    .instance_init = ls1_mmci_instance_init,
    .instance_finalize = ls1_mmci_instance_finalize,
    .class_init = ls1_mmci_class_init
};

static const TypeInfo ls1_mmci_bus_info = {
    .name = TYPE_LS1_MMCI_BUS,
    .parent = TYPE_SD_BUS,
    .instance_size = sizeof(SDBus),
};

static void ls1_mmci_register_types(void)
{
    type_register_static(&ls1_mmci_info);
    type_register_static(&ls1_mmci_bus_info);
}

type_init(ls1_mmci_register_types)
