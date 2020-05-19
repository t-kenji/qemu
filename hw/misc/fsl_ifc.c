#include "qemu/osdep.h"
#include "qapi/error.h"
#include "exec/address-spaces.h"
#include "sysemu/block-backend.h"
#include "hw/hw.h"
#include "hw/sysbus.h"
#include "hw/block/flash.h"
#include "hw/misc/gen-reg.h"
#include "qemu/log.h"

#define ENABLE_DEBUG
#define DEBUG_IFC 1

enum IFCMemorySize {
    IFC_MMIO_SIZE = 0x2000,
    IFC_SRAM_SIZE = 0x2400,
};

#define TYPE_FSL_IFC "fsl-ifc"
#define FSL_IFC(obj) OBJECT_CHECK(FSLIFCState, (obj), TYPE_FSL_IFC)

#if defined(ENABLE_DEBUG)
#define DBG(type, format, ...)                          \
    do {                                                \
        if (DEBUG_##type) {                             \
            qemu_log("%s: " format "\n",                \
                     TYPE_FSL_##type, ##__VA_ARGS__); \
        }                                               \
    } while (0)
#else
#define DBG(type, format, ...) do {} while (0)
#endif
#define ERR(type, format, ...)                              \
    do {                                                    \
        qemu_log_mask(LOG_GUEST_ERROR, "%s: " format "\n",  \
                      TYPE_FSL_##type, ##__VA_ARGS__);    \
    } while (0)

#define GET_FIELD(reg, fld, val) \
    (((val) & reg##__##fld##_MASK) >> (31 - reg##__##fld##_BIT))

#define CLEAR_FIELD(reg, fld, var) \
    (var) &= ~reg##__##fld##_MASK

#define SET_FIELD(reg, fld, var, val)                                        \
    ({                                                                       \
        CLEAR_FIELD(reg, fld, var);                                          \
        (var) |= ((val) << (31 - reg##__##fld##_BIT)) & reg##__##fld##_MASK; \
    })

enum {
    MSEL_NOR = 0x00,
    MSEL_NAND = 0x01,
    MSEL_GPCM = 0x02,
};

enum {
    IFC_REV,
    IFC_CSPR0_EXT,
    IFC_CSPR0,
    IFC_CSPR1_EXT,
    IFC_CSPR1,
    IFC_CSPR2_EXT,
    IFC_CSPR2,
    IFC_CSPR3_EXT,
    IFC_CSPR3,
    IFC_CSPR4_EXT,
    IFC_CSPR4,
    IFC_CSPR5_EXT,
    IFC_CSPR5,
    IFC_CSPR6_EXT,
    IFC_CSPR6,
    IFC_AMASK0,
    IFC_AMASK1,
    IFC_AMASK2,
    IFC_AMASK3,
    IFC_AMASK4,
    IFC_AMASK5,
    IFC_AMASK6,
    IFC_CSOR0,
    IFC_CSOR0_EXT,
    IFC_CSOR1,
    IFC_CSOR1_EXT,
    IFC_CSOR2,
    IFC_CSOR2_EXT,
    IFC_CSOR3,
    IFC_CSOR3_EXT,
    IFC_CSOR4,
    IFC_CSOR4_EXT,
    IFC_CSOR5,
    IFC_CSOR5_EXT,
    IFC_CSOR6,
    IFC_CSOR6_EXT,
    IFC_FTIM0_CS0,
    IFC_FTIM1_CS0,
    IFC_FTIM2_CS0,
    IFC_FTIM3_CS0,
    IFC_FTIM0_CS1,
    IFC_FTIM1_CS1,
    IFC_FTIM2_CS1,
    IFC_FTIM3_CS1,
    IFC_FTIM0_CS2,
    IFC_FTIM1_CS2,
    IFC_FTIM2_CS2,
    IFC_FTIM3_CS2,
    IFC_FTIM0_CS3,
    IFC_FTIM1_CS3,
    IFC_FTIM2_CS3,
    IFC_FTIM3_CS3,
    IFC_FTIM0_CS4,
    IFC_FTIM1_CS4,
    IFC_FTIM2_CS4,
    IFC_FTIM3_CS4,
    IFC_FTIM0_CS5,
    IFC_FTIM1_CS5,
    IFC_FTIM2_CS5,
    IFC_FTIM3_CS5,
    IFC_FTIM0_CS6,
    IFC_FTIM1_CS6,
    IFC_FTIM2_CS6,
    IFC_FTIM3_CS6,
    IFC_RB_STAT,
    IFC_GCR,
    IFC_CM_EVTER_STAT,
    IFC_CM_EVTER_EN,
    IFC_CM_EVTER_INTR_EN,
    IFC_CM_ERATTR0,
    IFC_CM_ERATTR1,
    IFC_CCR,
    IFC_CSR,
    IFC_DDR_CCR,

    IFC_NCFGR,
    IFC_NAND_FCR0,
    IFC_NAND_FCR1,
    IFC_ROW0,
    IFC_COL0,
    IFC_ROW1,
    IFC_COL1,
    IFC_ROW2,
    IFC_COL2,
    IFC_ROW3,
    IFC_COL3,
    IFC_NAND_BC,
    IFC_NAND_FIR0,
    IFC_NAND_FIR1,
    IFC_NAND_FIR2,
    IFC_NAND_CSEL,
    IFC_NANDSEQ_STRT,
    IFC_NAND_EVTER_STAT,
    IFC_PGRDCMPL_EVT_STAT,
    IFC_NAND_EVTER_EN,
};

static const RegDef32 fsl_ifc_regs[] = {
    REG_ITEM(IFC_REV,               0x0000, 0x01010000, 0x00000000),
    REG_ITEM(IFC_CSPR0_EXT,         0x000C, 0x00000000, 0x000000FF),
    REG_ITEM(IFC_CSPR0,             0x0010, 0x00000000, 0xFFFF01D7),
    REG_ITEM(IFC_CSPR1_EXT,         0x0018, 0x00000000, 0x000000FF),
    REG_ITEM(IFC_CSPR1,             0x001C, 0x00000000, 0xFFFF01D7),
    REG_ITEM(IFC_CSPR2_EXT,         0x0024, 0x00000000, 0x000000FF),
    REG_ITEM(IFC_CSPR2,             0x0028, 0x00000000, 0xFFFF01D7),
    REG_ITEM(IFC_CSPR3_EXT,         0x0030, 0x00000000, 0x000000FF),
    REG_ITEM(IFC_CSPR3,             0x0034, 0x00000000, 0xFFFF01D7),
    REG_ITEM(IFC_CSPR4_EXT,         0x003C, 0x00000000, 0x000000FF),
    REG_ITEM(IFC_CSPR4,             0x0040, 0x00000000, 0xFFFF01D7),
    REG_ITEM(IFC_CSPR5_EXT,         0x0048, 0x00000000, 0x000000FF),
    REG_ITEM(IFC_CSPR5,             0x004C, 0x00000000, 0xFFFF01D7),
    REG_ITEM(IFC_CSPR6_EXT,         0x0054, 0x00000000, 0x000000FF),
    REG_ITEM(IFC_CSPR6,             0x0058, 0x00000000, 0xFFFF01D7),
    REG_ITEM(IFC_AMASK0,            0x00A0, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_AMASK1,            0x00AC, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_AMASK2,            0x00B8, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_AMASK3,            0x00C4, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_AMASK4,            0x00D0, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_AMASK5,            0x00DC, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_AMASK6,            0x00E8, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_CSOR0,             0x0130, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_CSOR0_EXT,         0x0134, 0x00100000, 0xFFFFFFFF),
    REG_ITEM(IFC_CSOR1,             0x013C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_CSOR1_EXT,         0x0140, 0x00100000, 0xFFFFFFFF),
    REG_ITEM(IFC_CSOR2,             0x0148, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_CSOR2_EXT,         0x014C, 0x00100000, 0xFFFFFFFF),
    REG_ITEM(IFC_CSOR3,             0x0154, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_CSOR3_EXT,         0x0158, 0x00100000, 0xFFFFFFFF),
    REG_ITEM(IFC_CSOR4,             0x0160, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_CSOR4_EXT,         0x0164, 0x00100000, 0xFFFFFFFF),
    REG_ITEM(IFC_CSOR5,             0x016C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_CSOR5_EXT,         0x0170, 0x00100000, 0xFFFFFFFF),
    REG_ITEM(IFC_CSOR6,             0x0178, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_CSOR6_EXT,         0x017C, 0x00100000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM0_CS0,         0x01C0, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM1_CS0,         0x01C4, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM2_CS0,         0x01C8, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM3_CS0,         0x01CC, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM0_CS1,         0x01F0, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM1_CS1,         0x01F4, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM2_CS1,         0x01F8, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM3_CS1,         0x01FC, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM0_CS2,         0x0220, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM1_CS2,         0x0224, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM2_CS2,         0x0228, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM3_CS2,         0x022C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM0_CS3,         0x0250, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM1_CS3,         0x0254, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM2_CS3,         0x0258, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM3_CS3,         0x025C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM0_CS4,         0x0280, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM1_CS4,         0x0284, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM2_CS4,         0x0288, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM3_CS4,         0x028C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM0_CS5,         0x02B0, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM1_CS5,         0x02B4, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM2_CS5,         0x02B8, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM3_CS5,         0x02BC, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM0_CS6,         0x02E0, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM1_CS6,         0x02E4, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM2_CS6,         0x02E8, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_FTIM3_CS6,         0x02EC, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_RB_STAT,           0x0400, 0x00000000, 0x00000000),
    REG_ITEM(IFC_GCR,               0x040C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_CM_EVTER_STAT,     0x0418, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_CM_EVTER_EN,       0x0424, 0x80000000, 0xFFFFFFFF),
    REG_ITEM(IFC_CM_EVTER_INTR_EN,  0x0430, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_CM_ERATTR0,        0x043C, 0x00000000, 0x00000000),
    REG_ITEM(IFC_CM_ERATTR1,        0x0440, 0x00000000, 0x00000000),
    REG_ITEM(IFC_CCR,               0x044C, 0x03008000, 0xFFFFFFFF),
    REG_ITEM(IFC_CSR,               0x0450, 0x00000000, 0x00000000),
    REG_ITEM(IFC_DDR_CCR,           0x0454, 0x00800000, 0xFFFFFFFF),

    REG_ITEM(IFC_NCFGR,             0x1000, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_NAND_FCR0,         0x1014, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_NAND_FCR1,         0x1018, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_ROW0,              0x103C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_COL0,              0x1044, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_ROW1,              0x104C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_COL1,              0x1054, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_ROW2,              0x105C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_COL2,              0x1064, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_ROW3,              0x106C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_COL3,              0x1074, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_NAND_BC,           0x1108, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_NAND_FIR0,         0x1110, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_NAND_FIR1,         0x1114, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_NAND_FIR2,         0x1118, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_NAND_CSEL,         0x115C, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_NANDSEQ_STRT,      0x1164, 0x00000000, 0xFFFFFFFF),
    REG_ITEM(IFC_NAND_EVTER_STAT,   0x116C, 0x00000000, 0x8E00C800),
    REG_ITEM(IFC_PGRDCMPL_EVT_STAT, 0x1174, 0x00000000, 0xFFFF0000),
    REG_ITEM(IFC_NAND_EVTER_EN,     0x1180, 0xAE000000, 0xFFFFFFFF),
};

enum {
    IFC_CSPRn__BA_BIT = 15,
    IFC_CSPRn__BA_MASK = 0xFFFF0000,
    IFC_CSPRn__PS_BIT = 24,
    IFC_CSPRn__PS_MASK = 0x00000180,
    IFC_CSPRn__WP_BIT = 25,
    IFC_CSPRn__WP_MASK = 0x00000040,
    IFC_CSPRn__TE_BIT = 27,
    IFC_CSPRn__TE_MASK = 0x00000010,
    IFC_CSPRn__MSEL_BIT = 30,
    IFC_CSPRn__MSEL_MASK = 0x00000006,
    IFC_CSPRn__V_BIT = 31,
    IFC_CSPRn__V_MASK = 0x00000001,

    IFC_NANDSEQ_STRT__NAND_FIR_START_BIT = 0,
    IFC_NANDSEQ_STRT__NAND_FIR_START_MASK = 0x80000000,

    IFC_NAND_EVTER_STAT__OPC_BIT = 0,
    IFC_NAND_EVTER_STAT__OPC_MASK = 0x80000000,
    IFC_NAND_EVTER_STAT__FTOER_BIT = 4,
    IFC_NAND_EVTER_STAT__FTOER_MASK = 0x08000000,
    IFC_NAND_EVTER_STAT__WPER_BIT = 5,
    IFC_NAND_EVTER_STAT__WPER_MASK = 0x04000000,
    IFC_NAND_EVTER_STAT__ECCER_BIT = 6,
    IFC_NAND_EVTER_STAT__ECCER_MASK = 0x02000000,
    IFC_NAND_EVTER_STAT__RCW_DN_BIT = 16,
    IFC_NAND_EVTER_STAT__RCW_DN_MASK = 0x00008000,
    IFC_NAND_EVTER_STAT__BOOT_DN_BIT = 17,
    IFC_NAND_EVTER_STAT__BOOT_DN_MASK = 0x00004000,
    IFC_NAND_EVTER_STAT__BBI_SRCH_SEL_BIT = 20,
    IFC_NAND_EVTER_STAT__BBI_SRCH_SEL_MASK = 0x00000800,

    IFC_PGRDCMPL_EVT_STAT__SEC_DONE_BIT = 15,
    IFC_PGRDCMPL_EVT_STAT__SEC_DONE_MASK = 0xFFFF0000,
};

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    MemoryRegion sram;
    uint8_t *sram_ptr;
    DeviceState *nand;
    uint32_t regs[ARRAY_SIZE(fsl_ifc_regs)];
} FSLIFCState;

static void fsl_ifc_nand_command(FSLIFCState *s, uint8_t cmd)
{
    int buswidth = onfi_getbuswidth(s->nand);
    hwaddr ba = s->regs[IFC_CSPR0] & 0xFFFF0000;
    ba |= (hwaddr)s->regs[IFC_CSPR0_EXT] << 32;

    memory_region_set_address(&s->sram, ba);

printf("%s: cmd=%02x\n", __func__, cmd);
    onfi_setpins(s->nand, 1, 0, 0, 0, 0); /* CLE active */
    onfi_setio(s->nand, cmd);
    onfi_setpins(s->nand, 0, 1, 0, 0, 0); /* ALE active */
    onfi_setio(s->nand, (uint8_t)s->regs[IFC_ROW3]);

    switch (cmd) {
    case 0x90:
        onfi_setpins(s->nand, 0, 0, 0, 0, 0);
        if (s->regs[IFC_NAND_BC] == 0) {
            switch (buswidth) {
            case 8:
                s->sram_ptr[0] = onfi_getio(s->nand);
                s->sram_ptr[1] = onfi_getio(s->nand);
                break;
            default:
                hw_error("Unsupported buswidth %d bits", buswidth);
                break;
            }
        } else {
            switch (buswidth) {
            case 8:
                for (int i = 0; i < s->regs[IFC_NAND_BC]; i++) {
                    s->sram_ptr[i] = onfi_getio(s->nand);
                }
                break;
            default:
                hw_error("Unsupported buswidth %d bits", buswidth);
                break;
            }
        }
        break;
    case 0xEC:
        onfi_setpins(s->nand, 0, 0, 0, 0, 0);
        switch (buswidth) {
        case 8:
            for (int i = 0; i < s->regs[IFC_NAND_BC]; i++) {
                s->sram_ptr[i] = onfi_getio(s->nand);
            }
            break;
        default:
            hw_error("Unsupported buswidth %d bits", buswidth);
            break;
        }
        break;
    default:
        ERR(IFC, "Unsupported command %#02x", cmd);
        break;
    }
}

static void fsl_ifc_nand_seq_start(FSLIFCState *s)
{
    enum {
        OP_NOOP = 0x00,
        OP_CA0 = 0x01,
        OP_CA1 = 0x02,
        OP_CA2 = 0x03,
        OP_CA3 = 0x04,
        OP_RA0 = 0x05,
        OP_RA1 = 0x06,
        OP_RA2 = 0x07,
        OP_RA3 = 0x08,
        OP_CMD0 = 0x09,
        OP_CMD1 = 0x0A,
        OP_CMD2 = 0x0B,
        OP_CMD3 = 0x0C,
        OP_CMD4 = 0x0D,
        OP_CMD5 = 0x0E,
        OP_CMD6 = 0x0F,
        OP_CMD7 = 0x10,
        OP_CW0 = 0x11,
        OP_CW1 = 0x12,
        OP_CW2 = 0x13,
        OP_CW3 = 0x14,
        OP_CW4 = 0x15,
        OP_CW5 = 0x16,
        OP_CW6 = 0x17,
        OP_CW7 = 0x18,
        OP_WBCD = 0x19,
        OP_RBCD = 0x1A,
        OP_BTRD = 0x1B,
        OP_RDSTAT = 0x1C,
        OP_NWAIT = 0x1D,
        OP_WFR = 0x1E,
        OP_SBRD = 0x1F,
        OP_UA = 0x20,
        OP_RB = 0x21,
    };

    SET_FIELD(IFC_NAND_EVTER_STAT, OPC, s->regs[IFC_NAND_EVTER_STAT], 1);
    uint8_t ops[] = {
        ((s->regs[IFC_NAND_FIR0] >> 26) & 0x3F),
        ((s->regs[IFC_NAND_FIR0] >> 20) & 0x3F),
        ((s->regs[IFC_NAND_FIR0] >> 14) & 0x3F),
        ((s->regs[IFC_NAND_FIR0] >>  8) & 0x3F),
        ((s->regs[IFC_NAND_FIR0] >>  2) & 0x3F),
        ((s->regs[IFC_NAND_FIR1] >> 26) & 0x3F),
        ((s->regs[IFC_NAND_FIR1] >> 20) & 0x3F),
        ((s->regs[IFC_NAND_FIR1] >> 14) & 0x3F),
        ((s->regs[IFC_NAND_FIR1] >>  8) & 0x3F),
        ((s->regs[IFC_NAND_FIR1] >>  2) & 0x3F),
        ((s->regs[IFC_NAND_FIR2] >> 26) & 0x3F),
        ((s->regs[IFC_NAND_FIR2] >> 20) & 0x3F),
        ((s->regs[IFC_NAND_FIR2] >> 14) & 0x3F),
        ((s->regs[IFC_NAND_FIR2] >>  8) & 0x3F),
        ((s->regs[IFC_NAND_FIR2] >>  2) & 0x3F),
    };
    for (int i = 0; i < ARRAY_SIZE(ops); i++) {
        if (ops[i] > 0) {
            uint32_t fcr;
            int n;

            switch (ops[i]) {
            case OP_CMD0 ... OP_CMD3:
                fcr = s->regs[IFC_NAND_FCR0];
                n = ops[i] - OP_CMD0;
                fsl_ifc_nand_command(s, (fcr >> ((3 - n) << 3)) & 0xFF);
                break;
            case OP_CMD4 ... OP_CMD7:
                fcr = s->regs[IFC_NAND_FCR1];
                n = ops[i] - OP_CMD4;
                fsl_ifc_nand_command(s, (fcr >> ((3 - n) << 3)) & 0xFF);
                break;
            case OP_CW0 ... OP_CW3:
                fcr = s->regs[IFC_NAND_FCR0];
                n = ops[i] - OP_CW0;
                fsl_ifc_nand_command(s, (fcr >> ((3 - n) << 3)) & 0xFF);
                break;
            case OP_CW4 ... OP_CW7:
                fcr = s->regs[IFC_NAND_FCR1];
                n = ops[i] - OP_CW4;
                fsl_ifc_nand_command(s, (fcr >> ((3 - n) << 3)) & 0xFF);
                break;
            default:
                ERR(IFC, "Unknown opcode %#x", ops[i]);
                break;
            }
        }
    }
}

static uint64_t fsl_ifc_read(void *opaque, hwaddr offset, unsigned size)
{
    FSLIFCState *s = FSL_IFC(opaque);
    RegDef32 reg = regdef_find(fsl_ifc_regs, offset);

    if (reg.index < 0) {
        ERR(IFC, "Bad read offset %#" HWADDR_PRIx, offset);
        return 0;
    }

    uint64_t value = s->regs[reg.index];

    DBG(IFC, "Read %#" PRIx64 " from %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    return value;
}

static void fsl_ifc_write(void *opaque, hwaddr offset, uint64_t value,
                          unsigned size)
{
    FSLIFCState *s = FSL_IFC(opaque);
    RegDef32 reg = regdef_find(fsl_ifc_regs, offset);

    if (reg.index < 0) {
        ERR(IFC, "Bad write offset %#" HWADDR_PRIx, offset);
        return;
    }

    DBG(IFC, "Write %#" PRIx64 " to %s (offset %#" HWADDR_PRIx ")",
        value, reg.name, offset);
    if (!!(value & ~reg.write_mask)) {
        ERR(IFC, "Maybe write to a read only bit %#" PRIx64,
            (value & ~reg.write_mask));
    }

    switch (reg.index) {
    case IFC_CSPR0:
    case IFC_CSPR1:
    case IFC_CSPR2:
    case IFC_CSPR3:
    case IFC_CSPR4:
    case IFC_CSPR5:
    case IFC_CSPR6:
        if (GET_FIELD(IFC_CSPRn, V, value) == 1) {
            int machine = GET_FIELD(IFC_CSPRn, MSEL, value);
            DBG(IFC, "machine is %x", machine);
        }
        s->regs[reg.index] = value;
        break;
    case IFC_NANDSEQ_STRT:
        if (GET_FIELD(IFC_NANDSEQ_STRT, NAND_FIR_START, value) == 1) {
            fsl_ifc_nand_seq_start(s);
        }
        s->regs[reg.index] = value;
        break;
    case IFC_NAND_EVTER_STAT:
        if (GET_FIELD(IFC_NAND_EVTER_STAT, OPC, value) == 1) {
            DBG(IFC, "OPC cleared");
            CLEAR_FIELD(IFC_NAND_EVTER_STAT, OPC, value);
        }
        if (GET_FIELD(IFC_NAND_EVTER_STAT, FTOER, value) == 1) {
            DBG(IFC, "FTOER cleared");
            CLEAR_FIELD(IFC_NAND_EVTER_STAT, FTOER, value);
        }
        if (GET_FIELD(IFC_NAND_EVTER_STAT, WPER, value) == 1) {
            DBG(IFC, "WPER cleared");
            CLEAR_FIELD(IFC_NAND_EVTER_STAT, WPER, value);
        }
        if (GET_FIELD(IFC_NAND_EVTER_STAT, ECCER, value) == 1) {
            DBG(IFC, "ECCER cleared");
            CLEAR_FIELD(IFC_NAND_EVTER_STAT, ECCER, value);
        }
        if (GET_FIELD(IFC_NAND_EVTER_STAT, RCW_DN, value) == 1) {
            DBG(IFC, "RCW_DN cleared");
            CLEAR_FIELD(IFC_NAND_EVTER_STAT, RCW_DN, value);
        }
        if (GET_FIELD(IFC_NAND_EVTER_STAT, BOOT_DN, value) == 1) {
            DBG(IFC, "BOOT_DN cleared");
            CLEAR_FIELD(IFC_NAND_EVTER_STAT, BOOT_DN, value);
        }
        if (GET_FIELD(IFC_NAND_EVTER_STAT, BBI_SRCH_SEL, value) == 1) {
            DBG(IFC, "BBI_SRCH_SEL cleared");
            CLEAR_FIELD(IFC_NAND_EVTER_STAT, BBI_SRCH_SEL, value);
        }
        s->regs[reg.index] = value & reg.write_mask;
        break;
    case IFC_PGRDCMPL_EVT_STAT:
        if (GET_FIELD(IFC_PGRDCMPL_EVT_STAT, SEC_DONE, value) > 0) {
            DBG(IFC, "SEC_DONE cleared");
            CLEAR_FIELD(IFC_PGRDCMPL_EVT_STAT, SEC_DONE, value);
        }
        s->regs[reg.index] = value & reg.write_mask;
        break;
    default:
        s->regs[reg.index] = value;
        break;
    }
}

static void fsl_ifc_realize(DeviceState *dev, Error **errp)
{
    static const MemoryRegionOps ops = {
        .read       = fsl_ifc_read,
        .write      = fsl_ifc_write,
        .endianness = DEVICE_BIG_ENDIAN,
    };

    FSLIFCState *s = FSL_IFC(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ops, s,
                          TYPE_FSL_IFC, IFC_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);

#if 0
    /* NAND device */
    s->nand = onfi_init(blk_by_name("nand"), NAND_MFR_MICRON, 0xAC);
#endif
    memory_region_init_ram(&s->sram, OBJECT(s), "sram",
                           IFC_SRAM_SIZE, &error_fatal);
    s->sram_ptr = memory_region_get_ram_ptr(&s->sram);

    /* FIXME: unnecessary mapping */
    memory_region_add_subregion(get_system_memory(),
                                0xFFF800000, &s->sram);
}

static void fsl_ifc_reset(DeviceState *dev)
{
    FSLIFCState *s = FSL_IFC(dev);

    for (int i = 0; i < ARRAY_SIZE(s->regs); ++i) {
        s->regs[i] = fsl_ifc_regs[i].reset_value;
    }

    memset(s->sram_ptr, 0x00, IFC_SRAM_SIZE);
}

static void fsl_ifc_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->desc    = "Freescale Integrated Flash Controller";
    dc->realize = fsl_ifc_realize;
    dc->reset   = fsl_ifc_reset;
}

static void fsl_ifc_register_types(void)
{
    static const TypeInfo ifc_info = {
        .name = TYPE_FSL_IFC,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(FSLIFCState),
        .class_init = fsl_ifc_class_init,
    };

    type_register_static(&ifc_info);
}

type_init(fsl_ifc_register_types)
