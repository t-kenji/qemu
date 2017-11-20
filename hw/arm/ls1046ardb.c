/*
 * LS1046A Reference Design Board System emulation.
 *
 * Copyright (c) 2017 t-kenji <protect.2501@gmail.com>
 *
 * This code is licensed under the GPL, version 2 or later.
 * See the file `COPYING' in the top level directory.
 *
 * It (partially) emulates a ls1046a reference design board, 
 * with a Freescale LS1046A SoC
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "hw/arm/fsl-ls1.h"
#include "hw/boards.h"
#include "hw/char/serial.h"
#include "hw/i2c/smbus.h"
#include "hw/misc/ls1_spd.h"
#include "hw/misc/ls1_ccsr.h"
#include "sysemu/sysemu.h"
#include "qemu/error-report.h"
#include "sysemu/qtest.h"


#define MAX_NUM_PORTS (16)

struct __attribute__ ((packed)) ls1046ardb_eeprom {
    uint8_t id[4];                  /* 0x00 - 0x03 EEPROM Tag 'NXID' */
    uint8_t sn[12];                 /* 0x04 - 0x0F Serial Number */
    uint8_t errata[5];              /* 0x10 - 0x14 Errata Level */
    uint8_t date[6];                /* 0x15 - 0x1a Build Date */
    uint8_t res_0;                  /* 0x1b        Reserved */
    uint32_t version;               /* 0x1c - 0x1f NXID Version */
    uint8_t tempcal[8];             /* 0x20 - 0x27 Temperature Calibration Factors */
    uint8_t tempcalsys[2];          /* 0x28 - 0x29 System Temperature Calibration Factors */
    uint8_t tempcalflags;           /* 0x2a        Temperature Calibration Flags */
    uint8_t res_1[21];              /* 0x2b - 0x3f Reserved */
    int8_t mac_count;               /* 0x40        Number of MAC addresses */
    uint8_t mac_flag;               /* 0x41        MAC table flags */
    uint8_t mac[MAX_NUM_PORTS][6];  /* 0x42 - 0xa1 MAC addresses */
    uint8_t res_2[90];              /* 0xa2 - 0xfb Reserved */
    uint32_t crc;                   /* 0xfc - 0xff CRC32 checksum */
};

struct LS1046ARDB {
    FslLS1046AState soc;
    MemoryRegion ram;
};

/*
 * EEPROM CAT24C05 emulation.
 *  CAT24C05 - 4 kbit (512 x 8)
 */
static uint8_t eeprom_data[] = {
    [255] = 0x00,

    /* 0100: :*/  'N',  'X',  'I',  'D', 0x00, 0x00, 0x00, 0x00,
    /* 0108: :*/ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0110: :*/ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0118: :*/ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    /* 0120: :*/ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0128: :*/ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0130: :*/ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0138: :*/ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0140: :*/ 0x01, 0x00, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc
};

static uint32_t ls1046ardb_crc32(uint32_t crc, const uint8_t *data, size_t length)
{
    /* generated using the AUTODIN II polynomial
     *  x^32 + x^26 + x^23 + x^22 + x^16 +
     *  x^12 + x^11 + x^10 + x^8 + x^7 + x^5 + x^4 + x^2 + x^1 + 1
     */
    static const uint32_t crc32_table[] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
        0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
        0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
        0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
        0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
        0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
        0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
        0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
        0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
        0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,
        0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,
        0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
        0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
        0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
        0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a,
        0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,
        0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
        0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
        0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
        0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,
        0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
        0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
        0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
        0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
        0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086,
        0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,
        0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
        0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
        0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
        0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
        0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,
        0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
        0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
        0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
        0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,
        0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60,
        0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
        0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
        0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,
        0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
        0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
        0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
        0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
        0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,
        0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
        0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
        0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
        0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
        0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,
        0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,
        0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
        0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
    };

    while (length--) {
        crc = crc32_table[(crc ^ *data++) & 0xFFL] ^ (crc >> 8);
    }

    return crc ^ 0xFFFFFFFF;
}

static void ls1046ardb_eeprom_init(I2CBus *smbus, int start_addr,
                                   uint8_t *pre_data, size_t pre_data_size)
{
    int i;
    uint32_t crc;
    uint8_t *eeprom_buf = g_malloc0(2 * 256);

    memset(eeprom_buf, 0, 2 * 256);
    if (pre_data_size > 0) {
        memcpy(eeprom_buf, pre_data, pre_data_size);
    }
    crc = ls1046ardb_crc32(~0, &eeprom_buf[256], 0xFC);
    eeprom_buf[256 + 0xFC + 0] = (crc >> 24) & 0xFF;
    eeprom_buf[256 + 0xFC + 1] = (crc >> 16) & 0xFF;
    eeprom_buf[256 + 0xFC + 2] = (crc >>  8) & 0xFF;
    eeprom_buf[256 + 0xFC + 3] = (crc >>  0) & 0xFF;

    for (i = 0; i < 2; ++i) {
        DeviceState *eeprom = qdev_create((BusState *)smbus, "smbus-eeprom");
        qdev_prop_set_uint8(eeprom, "address", start_addr + i);
        qdev_prop_set_ptr(eeprom, "data", eeprom_buf + (i * 256));
        qdev_init_nofail(eeprom);
    }
}

/* No need to do any particular setup for secondary boot */
static void ls1046ardb_write_secondary(ARMCPU *cpu,
                                       const struct arm_boot_info *info)
{
}

/* Secondary cores are reset through SRC device */
static void ls1046ardb_reset_secondary(ARMCPU *cpu,
                                       const struct arm_boot_info *info)
{
}

static void ls1046ardb_init(MachineState *machine)
{
    static struct arm_boot_info ls1_binfo = {
        /* DDR memory start */
        .loader_start = FSL_LS1046A_MMDC_ADDR,
        /* No board ID, we boot from DT tree */
        .board_id = -1,
        /* EL2 / EL3 not support */
        .secure_boot = true
    };

    struct LS1046ARDB *vms = g_new0(struct LS1046ARDB, 1);
    MemoryRegion *address_space_mem = get_system_memory();
    I2CBus *smbus;
    DriveInfo *di;
    BlockBackend *blk;
    BusState *bus;
    DeviceState *carddev;
    Error *err = NULL;

    /* Check the amount of memory is compatible with the SOC */
    if (machine->ram_size > FSL_LS1046A_MMDC_SIZE) {
        error_report("RAM size " RAM_ADDR_FMT " above max supported (%08x)",
                     machine->ram_size, FSL_LS1046A_MMDC_SIZE);
        exit(1);
    }

    object_initialize(&vms->soc, sizeof(vms->soc), TYPE_FSL_LS1046A);
    object_property_add_child(OBJECT(machine), "soc", OBJECT(&vms->soc),
                              &error_abort);

    object_property_set_bool(OBJECT(&vms->soc), true, "realized", &err);
    if (err != NULL) {
        error_report("%s", error_get_pretty(err));
        exit(1);
    }

    memory_region_allocate_system_memory(&vms->ram, NULL, "ls1046ardb.ram",
                                         machine->ram_size);
    memory_region_add_subregion(address_space_mem, FSL_LS1046A_MMDC_ADDR,
                                &vms->ram);

    /* Initialize EEPROMs */
    smbus = (I2CBus *)qdev_get_child_bus(DEVICE(&vms->soc.i2cs[0]), "i2c-bus.0");
    ls1_spd_init(smbus);
    ls1046ardb_eeprom_init(smbus, 0x52, eeprom_data, sizeof(eeprom_data));

    /* Create and plug in the SD cards */
    di = drive_get_next(IF_SD);
    blk = di ? blk_by_legacy_dinfo(di) : NULL;
    bus = qdev_get_child_bus(DEVICE(&vms->soc), "sd-bus");
    if (bus == NULL) {
        error_report("No SD bus found in SOC object");
        exit(1);
    }
    carddev = qdev_create(bus, TYPE_SD_CARD);
    qdev_prop_set_drive(carddev, "drive", blk, &error_fatal);
    object_property_set_bool(OBJECT(carddev), true, "realized", &error_fatal);

#if 0
    {
        /*
         * TODO: Ideally we would expose the chip select and spi bus on the
         * SoC object using alias properties; then we would not need to
         * directly access the underlying spi device object.
         */
        /* Add the sst25vf016b NOR FLASH memory to first SPI */
        Object *spi_dev;

        spi_dev = object_resolve_path_component(OBJECT(&s->soc), "spi1");
        if (spi_dev) {
            SSIBus *spi_bus;

            spi_bus = (SSIBus *)qdev_get_child_bus(DEVICE(spi_dev), "spi");
            if (spi_bus) {
                DeviceState *flash_dev;
                qemu_irq cs_line;
                DriveInfo *dinfo = drive_get_next(IF_MTD);

                flash_dev = ssi_create_slave_no_init(spi_bus, "sst25vf016b");
                if (dinfo) {
                    qdev_prop_set_drive(flash_dev, "drive",
                                        blk_by_legacy_dinfo(dinfo),
                                        &error_fatal);
                }
                qdev_init_nofail(flash_dev);

                cs_line = qdev_get_gpio_in_named(flash_dev, SSI_GPIO_CS, 0);
                sysbus_connect_irq(SYS_BUS_DEVICE(spi_dev), 1, cs_line);
            }
        }
    }
#endif

    ls1_binfo.ram_size = machine->ram_size;
    ls1_binfo.kernel_filename = machine->kernel_filename;
    ls1_binfo.kernel_cmdline = machine->kernel_cmdline;
    ls1_binfo.initrd_filename = machine->initrd_filename;
    ls1_binfo.nb_cpus = smp_cpus;
    ls1_binfo.write_secondary_boot = ls1046ardb_write_secondary;
    ls1_binfo.secondary_cpu_reset_hook = ls1046ardb_reset_secondary;

    if (!qtest_enabled()) {
        arm_load_kernel(&vms->soc.cpus[0], &ls1_binfo);
    }
}

static void ls1046ardb_machine_init(MachineClass *mc)
{
    mc->desc = "Freescale LS1046A Reference Design Board (Cortex A72)";
    mc->init = ls1046ardb_init;
    mc->max_cpus = FSL_LS1046A_NUM_CPUS;
}

DEFINE_MACHINE("ls1046ardb", ls1046ardb_machine_init)
