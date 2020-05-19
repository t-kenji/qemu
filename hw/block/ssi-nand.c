#include "qemu/osdep.h"
#include "qemu/units.h"
#include "sysemu/block-backend.h"
#include "hw/qdev-properties.h"
#include "hw/ssi/ssi.h"
#include "qemu/bitops.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/error-report.h"
#include "qapi/error.h"

#define ENABLE_DEBUG
#define DEBUG_SPINAND 0

#if defined(ENABLE_DEBUG)
#define DBG(type, format, ...)                          \
    do {                                                \
        if (DEBUG_##type) {                             \
            qemu_log("%s: " format "\n",                \
                     TYPE_##type, ##__VA_ARGS__); \
        }                                               \
    } while (0)
#else
#define DBG(type, format, ...) do {} while (0)
#endif
#define ERR(type, format, ...)                              \
    do {                                                    \
        qemu_log_mask(LOG_GUEST_ERROR, "%s: " format "\n",  \
                      TYPE_##type, ##__VA_ARGS__);    \
    } while (0)

#define TYPE_SPINAND "spi-nand"
#define SPINAND(obj) OBJECT_CHECK(SPINANDFlashState, (obj), TYPE_SPINAND)

typedef enum {
    NAND_CMD__RESET = 0xFF,
    NAND_CMD__GET_FEATURE = 0x0F,
    NAND_CMD__SET_FEATURE = 0x1F,
    NAND_CMD__READ_ID = 0x9F,
    NAND_CMD__PAGE_READ = 0x13,
    NAND_CMD__READ_PAGE_CACHE_RANDOM = 0x30,
    NAND_CMD__READ_PAGE_CACHE_LAST = 0x3F,
    NAND_CMD__READ_FROM_CACHE_x1 = 0x0B,
    NAND_CMD__WRITE_ENABLE = 0x06,
    NAND_CMD__WRITE_DISABLE = 0x04,
    NAND_CMD__PROGRAM_EXECUTE = 0x10,
    NAND_CMD__PROGRAM_LOAD_x1 = 0x02,
} NANDCMD;

typedef enum {
    CMD_STATE__IDLE,
    CMD_STATE__PAGE_PROGRAM,
    CMD_STATE__READ,
    CMD_STATE__COLLECTING_DATA,
    CMD_STATE__READING_DATA,
} CMDState;

typedef enum {
    FEAT_ADDR__BLOCK_LOCK = 0xA0,
    FEAT_ADDR__CONFIG = 0xB0,
    FEAT_ADDR__STATUS = 0xC0,
    FEAT_ADDR__DIE_SELECT = 0xD0,
} FeatAddr;

enum {
    MAX_PAGESIZE = 1 << 12,
    MAX_OOBSIZE = 1 << 8,
};

#define INTERNAL_DATA_BUFFER_BYTES (16)
#define CACHE_BYTES (MAX_PAGESIZE + MAX_OOBSIZE)

enum {
    STATUS__CRBSY = 1 << 7,
    STATUS__ECCS2 = 1 << 6,
    STATUS__ECCS1 = 1 << 5,
    STATUS__ECCS0 = 1 << 4,
    STATUS__P_FAIL = 1 << 3,
    STATUS__E_FAIL = 1 << 2,
    STATUS__WEL = 1 << 1,
    STATUS__OIP = 1 << 0,
};

typedef struct {
    SSISlave parent_obj;

    BlockBackend *blk;
    uint8_t *storage;
    bool mem_oob;

    uint8_t mfr_id;
    uint8_t dev_id;
    uint8_t bus_width;
    uint32_t size_mib;
    uint32_t page_shift;
    uint32_t oob_shift;
    int pages;

    NANDCMD cip;
    CMDState state;
    uint8_t data[INTERNAL_DATA_BUFFER_BYTES];
    uint8_t cache[CACHE_BYTES];
    uint32_t length;
    uint32_t position;
    bool data_read_loop;
    size_t needed_bytes;
    uint32_t cur_addr;
    uint8_t block_lock;
    uint8_t config;
    uint8_t status;
    uint8_t die_select;
    bool write_enable;
} SPINANDFlashState;

#define PAGE_SIZE(s) (1 << (s)->page_shift)
#define PAGE_SECTORS(s) (1 << ((s)->page_shift - BDRV_SECTOR_BITS))
#define OOB_SIZE(s) (1 << (s)->oob_shift)
#define PAGE(s, a) ((a) >> (s)->page_shift)
#define PAGE_START(s, p) ((p) * (PAGE_SIZE(s) + OOB_SIZE(s)))
#define PAGE_OFFSET(s, a) ((a) & ((1 << (s)->page_shift) - 1))
#define SECTOR(s, a) ((a) >> BDRV_SECTOR_BITS)
#define SECTOR_OFFSET(s, a) ((a) & ((1 << BDRV_SECTOR_BITS) - 1))

__attribute__((unused))
static void nand_update_status(SPINANDFlashState *s)
{
    s->status = 0;
}

static inline size_t nand_get_addr_bytes(SPINANDFlashState *s)
{
    switch (s->cip) {
    case NAND_CMD__RESET:
    case NAND_CMD__READ_ID:
    case NAND_CMD__READ_PAGE_CACHE_LAST:
    case NAND_CMD__WRITE_ENABLE:
    case NAND_CMD__WRITE_DISABLE:
        return 0;
    case NAND_CMD__GET_FEATURE:
    case NAND_CMD__SET_FEATURE:
        return 1;
    case NAND_CMD__READ_FROM_CACHE_x1:
    case NAND_CMD__PROGRAM_LOAD_x1:
        return 2;
    default:
        return 3;
    }
}

static inline size_t nand_get_dummy_bytes(SPINANDFlashState *s)
{
    switch (s->cip) {
    case NAND_CMD__READ_ID:
    case NAND_CMD__READ_FROM_CACHE_x1:
        return 1;
    default:
        return 0;
    }
}

static void complete_collecting_data(SPINANDFlashState *s)
{
    size_t n = nand_get_addr_bytes(s);

    s->cur_addr = 0;
    for (size_t i = 0; i < n; ++i) {
        s->cur_addr <<= 8;
        s->cur_addr |= s->data[i];
    }

    s->cur_addr &= ((s->size_mib << 20) - 1);

    switch (s->cip) {
    case NAND_CMD__GET_FEATURE:
        switch (s->cur_addr) {
        case FEAT_ADDR__BLOCK_LOCK:
            s->data[0] = s->block_lock;
            break;
        case FEAT_ADDR__CONFIG:
            s->data[0] = s->config;
            break;
        case FEAT_ADDR__STATUS:
            s->data[0] = s->status;
            break;
        case FEAT_ADDR__DIE_SELECT:
            s->data[0] = s->die_select;
        default:
            ERR(SPINAND, "Invalid GET FEATURE address 0x%02x", s->cur_addr);
            s->data[0] = 0;
            break;
        }
        s->position = 0;
        s->length = 1;
        s->state = CMD_STATE__READING_DATA;
        break;
    case NAND_CMD__SET_FEATURE:
        switch (s->cur_addr) {
        case FEAT_ADDR__BLOCK_LOCK:
            s->block_lock = s->data[0];
            break;
        case FEAT_ADDR__CONFIG:
            s->config = s->data[0];
            break;
        case FEAT_ADDR__STATUS:
            s->status = s->data[0];
            break;
        case FEAT_ADDR__DIE_SELECT:
            s->die_select = s->data[0];
        default:
            ERR(SPINAND, "Invalid SET FEATURE address 0x%02x", s->cur_addr);
            break;
        }
        s->state = CMD_STATE__IDLE;
        break;
    case NAND_CMD__READ_ID:
        s->data[0] = s->mfr_id;
        s->data[1] = s->dev_id;
        s->position = 0;
        s->length = 2;
        s->data_read_loop = false;
        s->state = CMD_STATE__READING_DATA;
        break;
    case NAND_CMD__PAGE_READ:
        DBG(SPINAND, "PAGE READ at page %u", s->cur_addr);
        if (s->blk) {
            if (s->mem_oob) {
                off_t off = s->cur_addr << s->page_shift;
                if (blk_pread(s->blk, off, s->cache, PAGE_SIZE(s)) < 0) {
                    ERR(SPINAND, "read error in page %d", s->cur_addr);
                }
                memcpy(s->cache + PAGE_SIZE(s),
                       s->storage + (s->cur_addr << s->oob_shift), OOB_SIZE(s));
            } else {
                if (blk_pread(s->blk, PAGE_START(s, s->cur_addr), s->cache,
                              PAGE_SIZE(s) + OOB_SIZE(s)) < 0) {
                    ERR(SPINAND, "read error in page %d", s->cur_addr);
                }
            }
        } else {
            memcpy(s->cache, s->storage + PAGE_START(s, s->cur_addr),
                   PAGE_SIZE(s) + OOB_SIZE(s));
        }
        s->state = CMD_STATE__IDLE;
        break;
    case NAND_CMD__READ_FROM_CACHE_x1:
        s->state = CMD_STATE__READ;
        break;
    case NAND_CMD__PROGRAM_EXECUTE:
        DBG(SPINAND, "PROGRAM EXECUTE at page %u", s->cur_addr);
        if (s->blk) {
            if (s->mem_oob) {
                off_t off = s->cur_addr << s->page_shift;
                if (blk_pwrite(s->blk, off, s->cache, PAGE_SIZE(s), 0) < 0) {
                    ERR(SPINAND, "write error in page %d", s->cur_addr);
                }
                memcpy(s->storage + (s->cur_addr << s->oob_shift),
                       s->cache + PAGE_SIZE(s), OOB_SIZE(s));
            } else {
                if (blk_pwrite(s->blk, PAGE_START(s, s->cur_addr), s->cache,
                               PAGE_SIZE(s) + OOB_SIZE(s), 0) < 0) {
                    ERR(SPINAND, "write error in page %d", s->cur_addr);
                }
            }
        } else {
            memcpy(s->storage + PAGE_START(s, s->cur_addr), s->cache,
                   PAGE_SIZE(s) + OOB_SIZE(s));
        }
        s->state = CMD_STATE__IDLE;
        break;
    case NAND_CMD__PROGRAM_LOAD_x1:
        s->state = CMD_STATE__PAGE_PROGRAM;
        break;
    default:
        s->state = CMD_STATE__IDLE;
        break;
    }
}

static void nand_reset(SPINANDFlashState *s)
{
    s->cip            = NAND_CMD__RESET;
    s->state          = CMD_STATE__IDLE;
    s->length         = 0;
    s->position       = 0;
    s->data_read_loop = false;
    s->needed_bytes   = 0;
    s->block_lock     = 0;
    s->config         = 0;
    s->status         = 0;
    s->die_select     = 0;
    s->write_enable   = 0;
}

static void decode_new_cmd(SPINANDFlashState *s, NANDCMD cmd)
{
    DBG(SPINAND, "Decode new command: 0x%02x", cmd);
    s->cip = cmd;

    switch (s->cip) {
    case NAND_CMD__RESET:
        nand_reset(s);
        break;
    case NAND_CMD__GET_FEATURE:
    case NAND_CMD__READ_ID:
    case NAND_CMD__PAGE_READ:
    case NAND_CMD__READ_FROM_CACHE_x1:
    case NAND_CMD__PROGRAM_EXECUTE:
    case NAND_CMD__PROGRAM_LOAD_x1:
        s->needed_bytes = nand_get_addr_bytes(s) + nand_get_dummy_bytes(s);
        s->position = 0;
        s->length = 0;
        s->state = CMD_STATE__COLLECTING_DATA;
        break;
    case NAND_CMD__SET_FEATURE:
        s->needed_bytes = nand_get_addr_bytes(s) + nand_get_dummy_bytes(s) + 1;
        s->position = 0;
        s->length = 0;
        s->state = CMD_STATE__COLLECTING_DATA;
        break;
    case NAND_CMD__WRITE_ENABLE:
        s->write_enable = true;
        break;
    case NAND_CMD__WRITE_DISABLE:
        s->write_enable = false;
        break;
    default:
        DBG(SPINAND, "Unsupported command 0x%02x", cmd);
        break;
    }
}

static int spinand_cs(SSISlave *ss, bool select)
{
    SPINANDFlashState *s = SPINAND(ss);

    if (select) {
        s->length = 0;
        s->position = 0;
        s->state = CMD_STATE__IDLE;
        s->data_read_loop = false;
    }

    return 0;
}

static uint32_t spinand_transfer8(SSISlave *ss, uint32_t tx)
{
    SPINANDFlashState *s = SPINAND(ss);
    uint32_t ret = 0;

    switch (s->state) {
    case CMD_STATE__PAGE_PROGRAM:
        if (!s->write_enable) {
            ERR(SPINAND, "write with write protect!");
        }
        s->cache[s->cur_addr] = (uint8_t)tx;
        s->cur_addr = (s->cur_addr + 1) % sizeof(s->cache);
        break;
    case CMD_STATE__READ:
        ret = s->cache[s->cur_addr];
        s->cur_addr = (s->cur_addr + 1) % sizeof(s->cache);
        break;
    case CMD_STATE__COLLECTING_DATA:
        if (s->length >= INTERNAL_DATA_BUFFER_BYTES) {
            ERR(SPINAND, "Write overrun internal data buffer. "
                         "SPI controller (QEMU emulator or guest driver) "
                         "is misbehaving");
            s->length = s->position = 0;
            s->state = CMD_STATE__IDLE;
            break;
        }

        s->data[s->length] = (uint8_t)tx;
        ++s->length;

        if (s->length == s->needed_bytes) {
            complete_collecting_data(s);
        }
        break;
    case CMD_STATE__READING_DATA:
        if (s->position >= INTERNAL_DATA_BUFFER_BYTES) {
            ERR(SPINAND, "Read overrun internal data buffer. "
                         "SPI controller (QEMU emulator or guest driver) "
                         "is misbehaving");
            s->length = s->position = 0;
            s->state = CMD_STATE__IDLE;
            break;
        }

        ret = s->data[s->position];
        ++s->position;
        if (s->position == s->length) {
            s->position = 0;
            if (!s->data_read_loop) {
                s->state = CMD_STATE__IDLE;
            }
        }
        break;
    case CMD_STATE__IDLE:
    default:
        decode_new_cmd(s, (uint8_t)tx);
        break;
    }

    return ret;
}

static void spinand_realize(SSISlave *ss, Error **errp)
{
    SPINANDFlashState *s = SPINAND(ss);
    size_t page_size;
    int ret;

    s->pages = (s->size_mib << 20) >> s->page_shift;
    page_size = 1 << s->oob_shift;
    s->mem_oob = true;

    if (s->blk != NULL) {
        if (blk_is_read_only(s->blk)) {
            error_setg(errp, "Can't use a read-only drive");
            return;
        }
        ret = blk_set_perm(s->blk, BLK_PERM_CONSISTENT_READ | BLK_PERM_WRITE,
                           BLK_PERM_ALL, errp);
        if (ret < 0) {
            return;
        }
        if (blk_getlength(s->blk) >=
            ((s->pages << s->page_shift) + (s->pages << s->oob_shift))) {
            page_size = 0;
            s->mem_oob = false;
        }
    } else {
        page_size += 1 << s->page_shift;
    }

    if (page_size > 0) {
        s->storage = blk_blockalign(s->blk, s->pages * page_size);
        memset(s->storage, 0xFF, s->pages * page_size);
    }
}

static void spinand_reset(DeviceState *dev)
{
    SPINANDFlashState *s = SPINAND(dev);

    nand_reset(s);
}

static void spinand_class_init(ObjectClass *oc, void *data)
{
    static Property spinand_props[] = {
        DEFINE_PROP_DRIVE("drive", SPINANDFlashState, blk),
        DEFINE_PROP_UINT8("manufacturer_id", SPINANDFlashState, mfr_id, 0),
        DEFINE_PROP_UINT8("device_id", SPINANDFlashState, dev_id, 0),
        DEFINE_PROP_UINT8("bus_width", SPINANDFlashState, bus_width, 8),
        DEFINE_PROP_UINT32("size_mib", SPINANDFlashState, size_mib, 512),
        DEFINE_PROP_UINT32("page_shift", SPINANDFlashState, page_shift, 12),
        DEFINE_PROP_UINT32("oob_shift", SPINANDFlashState, oob_shift, (12 - 4)),
        DEFINE_PROP_END_OF_LIST(),
    };

    DeviceClass *dc = DEVICE_CLASS(oc);
    SSISlaveClass *ssc = SSI_SLAVE_CLASS(oc);

    dc->desc         = "SPI NAND Flash Memory";
    ssc->realize     = spinand_realize;
    ssc->transfer    = spinand_transfer8;
    ssc->set_cs      = spinand_cs;
    ssc->cs_polarity = SSI_CS_LOW;
    dc->reset        = spinand_reset;
    dc->props        = spinand_props;
}

static void spinand_register_types(void)
{
    static const TypeInfo spinand_info = {
        .name           = TYPE_SPINAND,
        .parent         = TYPE_SSI_SLAVE,
        .instance_size  = sizeof(SPINANDFlashState),
        .class_init     = spinand_class_init,
    };

    type_register_static(&spinand_info);
}

type_init(spinand_register_types)
