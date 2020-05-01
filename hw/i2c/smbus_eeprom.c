/*
 * QEMU SMBus EEPROM device
 *
 * Copyright (c) 2007 Arastra, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qemu/crc16.h"
#include "qapi/error.h"
#include "hw/boards.h"
#include "hw/i2c/i2c.h"
#include "hw/i2c/smbus_slave.h"
#include "hw/qdev-properties.h"
#include "migration/vmstate.h"
#include "hw/i2c/smbus_eeprom.h"

//#define DEBUG

#define TYPE_SMBUS_EEPROM "smbus-eeprom"

#define SMBUS_EEPROM(obj) \
    OBJECT_CHECK(SMBusEEPROMDevice, (obj), TYPE_SMBUS_EEPROM)

#define SMBUS_EEPROM_SIZE 256

typedef struct SMBusEEPROMDevice {
    SMBusDevice smbusdev;
    uint8_t data[SMBUS_EEPROM_SIZE];
    void *init_data;
    uint8_t offset;
    bool accessed;
} SMBusEEPROMDevice;

static uint8_t eeprom_receive_byte(SMBusDevice *dev)
{
    SMBusEEPROMDevice *eeprom = SMBUS_EEPROM(dev);
    uint8_t *data = eeprom->data;
    uint8_t val = data[eeprom->offset++];

    eeprom->accessed = true;
#ifdef DEBUG
    printf("eeprom_receive_byte: addr=0x%02x val=0x%02x\n",
           dev->i2c.address, val);
#endif
    return val;
}

static int eeprom_write_data(SMBusDevice *dev, uint8_t *buf, uint8_t len)
{
    SMBusEEPROMDevice *eeprom = SMBUS_EEPROM(dev);
    uint8_t *data = eeprom->data;

    eeprom->accessed = true;
#ifdef DEBUG
    printf("eeprom_write_byte: addr=0x%02x cmd=0x%02x val=0x%02x\n",
           dev->i2c.address, buf[0], buf[1]);
#endif
    /* len is guaranteed to be > 0 */
    eeprom->offset = buf[0];
    buf++;
    len--;

    for (; len > 0; len--) {
        data[eeprom->offset] = *buf++;
        eeprom->offset = (eeprom->offset + 1) % SMBUS_EEPROM_SIZE;
    }

    return 0;
}

static bool smbus_eeprom_vmstate_needed(void *opaque)
{
    MachineClass *mc = MACHINE_GET_CLASS(qdev_get_machine());
    SMBusEEPROMDevice *eeprom = opaque;

    return (eeprom->accessed || smbus_vmstate_needed(&eeprom->smbusdev)) &&
        !mc->smbus_no_migration_support;
}

static const VMStateDescription vmstate_smbus_eeprom = {
    .name = "smbus-eeprom",
    .version_id = 1,
    .minimum_version_id = 1,
    .needed = smbus_eeprom_vmstate_needed,
    .fields      = (VMStateField[]) {
        VMSTATE_SMBUS_DEVICE(smbusdev, SMBusEEPROMDevice),
        VMSTATE_UINT8_ARRAY(data, SMBusEEPROMDevice, SMBUS_EEPROM_SIZE),
        VMSTATE_UINT8(offset, SMBusEEPROMDevice),
        VMSTATE_BOOL(accessed, SMBusEEPROMDevice),
        VMSTATE_END_OF_LIST()
    }
};

/*
 * Reset the EEPROM contents to the initial state on a reset.  This
 * isn't really how an EEPROM works, of course, but the general
 * principle of QEMU is to restore function on reset to what it would
 * be if QEMU was stopped and started.
 *
 * The proper thing to do would be to have a backing blockdev to hold
 * the contents and restore that on startup, and not do this on reset.
 * But until that time, act as if we had been stopped and restarted.
 */
static void smbus_eeprom_reset(DeviceState *dev)
{
    SMBusEEPROMDevice *eeprom = SMBUS_EEPROM(dev);

    memcpy(eeprom->data, eeprom->init_data, SMBUS_EEPROM_SIZE);
    eeprom->offset = 0;
}

static void smbus_eeprom_realize(DeviceState *dev, Error **errp)
{
    smbus_eeprom_reset(dev);
}

static Property smbus_eeprom_properties[] = {
    DEFINE_PROP_PTR("data", SMBusEEPROMDevice, init_data),
    DEFINE_PROP_END_OF_LIST(),
};

static void smbus_eeprom_class_initfn(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SMBusDeviceClass *sc = SMBUS_DEVICE_CLASS(klass);

    dc->realize = smbus_eeprom_realize;
    dc->reset = smbus_eeprom_reset;
    sc->receive_byte = eeprom_receive_byte;
    sc->write_data = eeprom_write_data;
    dc->props = smbus_eeprom_properties;
    dc->vmsd = &vmstate_smbus_eeprom;
    /* Reason: pointer property "data" */
    dc->user_creatable = false;
}

static const TypeInfo smbus_eeprom_info = {
    .name          = TYPE_SMBUS_EEPROM,
    .parent        = TYPE_SMBUS_DEVICE,
    .instance_size = sizeof(SMBusEEPROMDevice),
    .class_init    = smbus_eeprom_class_initfn,
};

static void smbus_eeprom_register_types(void)
{
    type_register_static(&smbus_eeprom_info);
}

type_init(smbus_eeprom_register_types)

void smbus_eeprom_init_one(I2CBus *smbus, uint8_t address, uint8_t *eeprom_buf)
{
    DeviceState *dev;

    dev = qdev_create((BusState *) smbus, TYPE_SMBUS_EEPROM);
    qdev_prop_set_uint8(dev, "address", address);
    qdev_prop_set_ptr(dev, "data", eeprom_buf);
    qdev_init_nofail(dev);
}

void smbus_eeprom_init(I2CBus *smbus, int nb_eeprom,
                       const uint8_t *eeprom_spd, int eeprom_spd_size)
{
    int i;
     /* XXX: make this persistent */

    assert(nb_eeprom <= 8);
    uint8_t *eeprom_buf = g_malloc0(8 * SMBUS_EEPROM_SIZE);
    if (eeprom_spd_size > 0) {
        memcpy(eeprom_buf, eeprom_spd, eeprom_spd_size);
    }

    for (i = 0; i < nb_eeprom; i++) {
        smbus_eeprom_init_one(smbus, 0x50 + i,
                              eeprom_buf + (i * SMBUS_EEPROM_SIZE));
    }
}

static uint8_t *spd_data_generate_ddr(enum sdram_type type,
                                      ram_addr_t ram_size,
                                      Error **errp)
{
    uint8_t *spd;
    uint8_t nbanks;
    uint16_t density;
    uint32_t size;
    int min_log2, max_log2, sz_log2;
    int i;

    switch (type) {
    case SDR:
        min_log2 = 2;
        max_log2 = 9;
        break;
    case DDR:
        min_log2 = 5;
        max_log2 = 12;
        break;
    case DDR2:
        min_log2 = 7;
        max_log2 = 14;
        break;
    default:
        g_assert_not_reached();
    }
    size = ram_size >> 20; /* work in terms of megabytes */
    if (size < 4) {
        error_setg(errp, "SDRAM size is too small");
        return NULL;
    }
    sz_log2 = 31 - clz32(size);
    size = 1U << sz_log2;
    if (ram_size > size * MiB) {
        error_setg(errp, "SDRAM size 0x"RAM_ADDR_FMT" is not a power of 2, "
                   "truncating to %u MB", ram_size, size);
    }
    if (sz_log2 < min_log2) {
        error_setg(errp,
                   "Memory size is too small for SDRAM type, adjusting type");
        if (size >= 32) {
            type = DDR;
            min_log2 = 5;
            max_log2 = 12;
        } else {
            type = SDR;
            min_log2 = 2;
            max_log2 = 9;
        }
    }

    nbanks = 1;
    while (sz_log2 > max_log2 && nbanks < 8) {
        sz_log2--;
        nbanks++;
    }

    if (size > (1ULL << sz_log2) * nbanks) {
        error_setg(errp, "Memory size is too big for SDRAM, truncating");
    }

    /* split to 2 banks if possible to avoid a bug in MIPS Malta firmware */
    if (nbanks == 1 && sz_log2 > min_log2) {
        sz_log2--;
        nbanks++;
    }

    density = 1ULL << (sz_log2 - 2);
    switch (type) {
    case DDR2:
        density = (density & 0xe0) | (density >> 8 & 0x1f);
        break;
    case DDR:
        density = (density & 0xf8) | (density >> 8 & 0x07);
        break;
    case SDR:
    default:
        density &= 0xff;
        break;
    }

    spd = g_malloc0(256);
    spd[0] = 128;   /* data bytes in EEPROM */
    spd[1] = 8;     /* log2 size of EEPROM */
    spd[2] = type;
    spd[3] = 13;    /* row address bits */
    spd[4] = 10;    /* column address bits */
    spd[5] = (type == DDR2 ? nbanks - 1 : nbanks);
    spd[6] = 64;    /* module data width */
                    /* reserved / data width high */
    spd[8] = 4;     /* interface voltage level */
    spd[9] = 0x25;  /* highest CAS latency */
    spd[10] = 1;    /* access time */
                    /* DIMM configuration 0 = non-ECC */
    spd[12] = 0x82; /* refresh requirements */
    spd[13] = 8;    /* primary SDRAM width */
                    /* ECC SDRAM width */
    spd[15] = (type == DDR2 ? 0 : 1); /* reserved / delay for random col rd */
    spd[16] = 12;   /* burst lengths supported */
    spd[17] = 4;    /* banks per SDRAM device */
    spd[18] = 12;   /* ~CAS latencies supported */
    spd[19] = (type == DDR2 ? 0 : 1); /* reserved / ~CS latencies supported */
    spd[20] = 2;    /* DIMM type / ~WE latencies */
                    /* module features */
                    /* memory chip features */
    spd[23] = 0x12; /* clock cycle time @ medium CAS latency */
                    /* data access time */
                    /* clock cycle time @ short CAS latency */
                    /* data access time */
    spd[27] = 20;   /* min. row precharge time */
    spd[28] = 15;   /* min. row active row delay */
    spd[29] = 20;   /* min. ~RAS to ~CAS delay */
    spd[30] = 45;   /* min. active to precharge time */
    spd[31] = density;
    spd[32] = 20;   /* addr/cmd setup time */
    spd[33] = 8;    /* addr/cmd hold time */
    spd[34] = 20;   /* data input setup time */
    spd[35] = 8;    /* data input hold time */

    /* checksum */
    for (i = 0; i < 63; i++) {
        spd[63] += spd[i];
    }
    return spd;
}

static uint8_t *spd_data_generate_ddr3(ram_addr_t ram_size, Error **errp)
{
    uint8_t *spd;
    uint8_t nbanks;
    uint16_t density;
    uint32_t size;
    int min_log2 = 9, sz_log2;

    size = ram_size >> 20; /* work in terms of megabytes */
    if (size < 4) {
        error_setg(errp, "SDRAM size is too small");
        return NULL;
    }
    sz_log2 = 31 - clz32(size);
    size = 1U << sz_log2;
    if (ram_size > size * MiB) {
        error_setg(errp, "SDRAM size 0x"RAM_ADDR_FMT" is not a power of 2, "
                   "truncating to %u MB", ram_size, size);
    }
    if (sz_log2 < min_log2) {
        error_setg(errp, "Memory size is too small for SDRAM type,");
        return NULL;
    }

    nbanks = 2;
    sz_log2 -= 1;
    density = sz_log2 - 8;

    spd = g_malloc0(256);
    spd[0] = 0x92; /* Number of Serial PD Bytes Written / SPD Device Size / CRC Coverage */
    spd[1] = 0x10; /* SPD Revision */
    spd[2] = 0x0B; /* Key Byte / DRAM Device Type */
    spd[3] = 0x02; /* Key Byte / Module type */
    spd[4] = density & 0x0F; /* SDRAM Density and Banks */
    spd[5] = 0x12; /* SDRAM Addressing */
    spd[6] = 0x00; /* Module Nominal Voltage, VDD */
    spd[7] = ((nbanks == 8) ? 0x40 : nbanks - 1) << 3 | 0x01; /* Module Organization */
    spd[8] = 0x0B; /* Module Memory Bus Width */
    spd[9] = 0x52; /* Fine Timebase (FTB) Dividend / Divisor */
    spd[10] = 0x01; /* Medium Timebase (MTB) Dividend */
    spd[11] = 0x08; /* Medium Timebase (MTB) Divisor */
    spd[12] = 0x0C; /* SDRAM Minimum Cycle Time (tCKmin) */
    spd[14] = 0x7C; /* CAS Latencles Supported, LSB */
    spd[15] = 0x00; /* CAS Latencles Supported, MSB */
    spd[16] = 0x6C; /* Minimum CAS Latency Time (tAAmin) */
    spd[17] = 0x78; /* Minimum Write Recovery Time (tWRmin) */
    spd[18] = 0x6C; /* Minimum RAS# to CAS# Delay Time (tRCDmin) */
    spd[19] = 0x30; /* Minimum Row Active to Row Active Delay Time (tRRDmin) */
    spd[20] = 0x6C; /* Minimum Row Precharge Delay Time (tRPmin) */
    spd[21] = 0x11; /* Upper Nibbles for tRAS and tRC */
    spd[22] = 0x20; /* Minimum Active to Precharge Delay Time (tRASmin), LSB */
    spd[23] = 0x8C; /* Minimum Active to Active / Refresh Delay Time (tRCmin), LSB */
    spd[24] = 0x70; /* Minimum Refresh Recovery Delay Time (tRFCmin), LSB */
    spd[25] = 0x03; /* Minimum Refresh Recovery Delay Time (tRFCmin), MSB */
    spd[26] = 0x3C; /* Minimum Internal Write to Read Command Delay Time (tWTRmin) */
    spd[27] = 0x3C; /* Minimum Internal Read to Precharge Command Delay Time (tRTPmin) */
    spd[28] = 0x00; /* Upper Nibble for tFAW */
    spd[29] = 0xF0; /* Minimum Four Activate Window Delay Time (tFAWmin) */
    spd[30] = 0x82; /* SDRAM Optional Feature */
    spd[31] = 0x05; /* SDRAM Thermal and Refresh Options */
    spd[32] = 0x80; /* Module Thermal Sensor */
    spd[33] = 0x00; /* SDRAM Device Type */
    spd[34] = 0x00; /* Fine Offset for SDRAM Minimum Cycle Time (tCKmin) */
    spd[35] = 0x00; /* Fine Offset for Minimum CAS Latency Time (tAAmin) */
    spd[36] = 0x00; /* Fine Offset for Minimum RAS# to CAS# Delay Time (tRCDmin) */
    spd[37] = 0x00; /* Fine Offset for Minimum Row Precharge Delay Time (tRPmin) */
    spd[38] = 0x00; /* Fine Offset for Minimum Active to Active / Refresh Delay Time (tRCmin) */
    spd[41] = 0x06; /* SDRAM Maximum Activate Count (MAC) Value */
    spd[60] = 0x03; /* Raw Card Extension, Module Nominal Height */
    spd[61] = 0x11; /* Module Maximum Thickness */
    spd[62] = 0x0B; /* Reference Raw Card Used */
    spd[63] = 0x00; /* DIMM Module Attributes */
    spd[64] = 0x00; /* RDIMM Thermal Heat Spreader Solution */
    spd[65] = 0x04; /* Register Manufacturer ID Code, LSB */
    spd[66] = 0xB3; /* Register Manufacturer ID Code, MSB */
    spd[67] = 0x03; /* Register Revision Number */
    spd[68] = 0x00; /* Register Type */
    spd[69] = 0x00; /* RC1 (MS Nibble) / RC0 (LS Nibble) */
    spd[70] = 0x50; /* RC3 (MS Nibble) / RC2 (LS Nibble) - Drive Strength, Command / Address */
    spd[71] = 0x55; /* RC5 (MS Nibble) / RC4 (LS Nibble) - Drive Strength, Control and Clock */
    spd[72] = 0x00; /* RC7 (MS Nibble) / RC6 (LS Nibble) */
    spd[73] = 0x00; /* RC9 (MS Nibble) / RC8 (LS Nibble) */
    spd[74] = 0x00; /* RC11 (MS Nibble) / RC10 (LS Nibble) */
    spd[75] = 0x00; /* RC13 (MS Nibble) / RC12 (LS Nibble) */
    spd[76] = 0x00; /* RC15 (MS Nibble) / RC14 (LS Nibble) */
    uint16_t crc = crc16(spd, (spd[0] & 0x80) ? 117 : 125);
    spd[117] = 0x80; /* Module ID: Module Manufacturer's JEDEC ID Code LSB */
    spd[118] = 0x2C; /* Module ID: Module Manufacturer's JEDEC ID Code MSB */
    spd[119] = 0x00; /* Module ID: Module Manufacturing Location */
    spd[120] = 0x00; /* Module ID: Module Manufacturing Date (Year) */
    spd[121] = 0x00; /* Module ID: Module Manufacturing Date (Week) */
    spd[122] = 0x12; /* Module ID: Module Serial Number */
    spd[123] = 0x34; /* Module ID: Module Serial Number */
    spd[124] = 0x56; /* Module ID: Module Serial Number */
    spd[125] = 0x78; /* Module ID: Module Serial Number */
    spd[126] = (uint8_t)(crc & 0xFF); /* Cyclical Redundancy Code LSB */
    spd[127] = (uint8_t)(crc >> 8); /* Cyclical Redundancy Code MSB */
    spd[128] = 'Q';
    spd[129] = 'E';
    spd[130] = 'M';
    spd[131] = 'U';
    spd[132] = '-';
    spd[133] = 'A';
    spd[134] = 'B';
    spd[135] = 'C';
    spd[136] = 'D';
    spd[137] = 'E';
    spd[138] = 'F';
    spd[139] = 'G';
    spd[140] = 'H';
    spd[141] = 'I';
    spd[142] = 'J';
    spd[143] = 'K';
    spd[144] = 'L';
    spd[145] = 'M';
    spd[146] = 0x44;
    spd[147] = 0x5A;
    spd[148] = 0x80;
    spd[149] = 0x2C;
    return spd;
}

/* Generate SDRAM SPD EEPROM data describing a module of type and size */
uint8_t *spd_data_generate(enum sdram_type type, ram_addr_t ram_size,
                           Error **errp)
{
    switch (type) {
    case SDR:
    case DDR:
    case DDR2:
        return spd_data_generate_ddr(type, ram_size, errp);
    case DDR3:
        return spd_data_generate_ddr3(ram_size, errp);
    default:
        g_assert_not_reached();
    }
}
