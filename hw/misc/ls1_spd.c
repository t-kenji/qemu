/*
 * QorIQ LS1046A Serial Presence Detect EEPROM for DDR
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

#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/i2c/i2c.h"
#include "hw/i2c/smbus.h"
#include "hw/misc/ls1_spd.h"


//#define LS1_SPD_DEBUG


#define SPD_EEPROM_NUM  (1)

#define SPD_SPA0_ADDR   (0x36)
#define SPD_SPA1_ADDR   (0x37)
#define SPD_EEPROM_ADDR (0x51)


#if defined(LS1_SPD_DEBUG)
#define dprintf(...) fprintf(stderr, __VA_ARGS__)
#else
#define dprintf(...)
#endif

struct DDR4SPDDevice {
    SMBusDevice smbusdev;
    void *data;
    void *alias;
    uint8_t offset;
};

/* From JEEC Standard No. 21-C release 23A */
struct ddr4_spd_eeprom_s {
    /* General Section: Bytes 0-127 */
    uint8_t info_size_crc;      /*  0 # bytes */
    uint8_t spd_rev;            /*  1 Total # bytes of SPD */
    uint8_t mem_type;           /*  2 Key Byte / mem type */
    uint8_t module_type;        /*  3 Key Byte / Module Type */
    uint8_t density_banks;      /*  4 Density and Banks    */
    uint8_t addressing;         /*  5 Addressing */
    uint8_t package_type;       /*  6 Package type */
    uint8_t opt_feature;        /*  7 Optional features */
    uint8_t thermal_ref;        /*  8 Thermal and refresh */
    uint8_t oth_opt_features;   /*  9 Other optional features */
    uint8_t res_10;             /* 10 Reserved */
    uint8_t module_vdd;         /* 11 Module nominal voltage */
    uint8_t organization;       /* 12 Module Organization */
    uint8_t bus_width;          /* 13 Module Memory Bus Width */
    uint8_t therm_sensor;       /* 14 Module Thermal Sensor */
    uint8_t ext_type;           /* 15 Extended module type */
    uint8_t res_16;
    uint8_t timebases;          /* 17 MTb and FTB */
    uint8_t tck_min;            /* 18 tCKAVGmin */
    uint8_t tck_max;            /* 19 TCKAVGmax */
    uint8_t caslat_b1;          /* 20 CAS latencies, 1st byte */
    uint8_t caslat_b2;          /* 21 CAS latencies, 2nd byte */
    uint8_t caslat_b3;          /* 22 CAS latencies, 3rd byte */
    uint8_t caslat_b4;          /* 23 CAS latencies, 4th byte */
    uint8_t taa_min;            /* 24 Min CAS Latency Time */
    uint8_t trcd_min;           /* 25 Min RAS# to CAS# Delay Time */
    uint8_t trp_min;            /* 26 Min Row Precharge Delay Time */
    uint8_t tras_trc_ext;       /* 27 Upper Nibbles for tRAS and tRC */
    uint8_t tras_min_lsb;       /* 28 tRASmin, lsb */
    uint8_t trc_min_lsb;        /* 29 tRCmin, lsb */
    uint8_t trfc1_min_lsb;      /* 30 Min Refresh Recovery Delay Time */
    uint8_t trfc1_min_msb;      /* 31 Min Refresh Recovery Delay Time */
    uint8_t trfc2_min_lsb;      /* 32 Min Refresh Recovery Delay Time */
    uint8_t trfc2_min_msb;      /* 33 Min Refresh Recovery Delay Time */
    uint8_t trfc4_min_lsb;      /* 34 Min Refresh Recovery Delay Time */
    uint8_t trfc4_min_msb;      /* 35 Min Refresh Recovery Delay Time */
    uint8_t tfaw_msb;           /* 36 Upper Nibble for tFAW */
    uint8_t tfaw_min;           /* 37 tFAW, lsb */
    uint8_t trrds_min;          /* 38 tRRD_Smin, MTB */
    uint8_t trrdl_min;          /* 39 tRRD_Lmin, MTB */
    uint8_t tccdl_min;          /* 40 tCCS_Lmin, MTB */
    uint8_t res_41[60-41];      /* 41 Rserved */
    uint8_t mapping[78-60];     /* 60~77 Connector to SDRAM bit map */
    uint8_t res_78[117-78];     /* 78~116, Reserved */
    int8_t fine_tccdl_min;      /* 117 Fine offset for tCCD_Lmin */
    int8_t fine_trrdl_min;      /* 118 Fine offset for tRRD_Lmin */
    int8_t fine_trrds_min;      /* 119 Fine offset for tRRD_Smin */
    int8_t fine_trc_min;        /* 120 Fine offset for tRCmin */
    int8_t fine_trp_min;        /* 121 Fine offset for tRPmin */
    int8_t fine_trcd_min;       /* 122 Fine offset for tRCDmin */
    int8_t fine_taa_min;        /* 123 Fine offset for tAAmin */
    int8_t fine_tck_max;        /* 124 Fine offset for tCKAVGmax */
    int8_t fine_tck_min;        /* 125 Fine offset for tCKAVGmin */
    /* CRC: Bytes 126-127 */
    uint8_t crc[2];            /* 126-127 SPD CRC */

    /* Module-Specific Section: Bytes 128-255 */
    union {
        struct {
            /* 128 (Unbuffered) Module Nominal Height */
            uint8_t mod_height;
            /* 129 (Unbuffered) Module Maximum Thickness */
            uint8_t mod_thickness;
            /* 130 (Unbuffered) Reference Raw Card Used */
            uint8_t ref_raw_card;
            /* 131 (Unbuffered) Address Mapping from
                  Edge Connector to DRAM */
            uint8_t addr_mapping;
            /* 132~253 (Unbuffered) Reserved */
            uint8_t res_132[254-132];
            /* 254~255 CRC */
            uint8_t crc[2];
        } unbuffered;
        struct {
            /* 128 (Registered) Module Nominal Height */
            uint8_t mod_height;
            /* 129 (Registered) Module Maximum Thickness */
            uint8_t mod_thickness;
            /* 130 (Registered) Reference Raw Card Used */
            uint8_t ref_raw_card;
            /* 131 DIMM Module Attributes */
            uint8_t modu_attr;
            /* 132 RDIMM Thermal Heat Spreader Solution */
            uint8_t thermal;
            /* 133 Register Manufacturer ID Code, LSB */
            uint8_t reg_id_lo;
            /* 134 Register Manufacturer ID Code, MSB */
            uint8_t reg_id_hi;
            /* 135 Register Revision Number */
            uint8_t reg_rev;
            /* 136 Address mapping from register to DRAM */
            uint8_t reg_map;
            /* 137~253 Reserved */
            uint8_t res_137[254-137];
            /* 254~255 CRC */
            uint8_t crc[2];
        } registered;
        struct {
            /* 128 (Loadreduced) Module Nominal Height */
            uint8_t mod_height;
            /* 129 (Loadreduced) Module Maximum Thickness */
            uint8_t mod_thickness;
            /* 130 (Loadreduced) Reference Raw Card Used */
            uint8_t ref_raw_card;
            /* 131 DIMM Module Attributes */
            uint8_t modu_attr;
            /* 132 RDIMM Thermal Heat Spreader Solution */
            uint8_t thermal;
            /* 133 Register Manufacturer ID Code, LSB */
            uint8_t reg_id_lo;
            /* 134 Register Manufacturer ID Code, MSB */
            uint8_t reg_id_hi;
            /* 135 Register Revision Number */
            uint8_t reg_rev;
            /* 136 Address mapping from register to DRAM */
            uint8_t reg_map;
            /* 137 Register Output Drive Strength for CMD/Add*/
            uint8_t reg_drv;
            /* 138 Register Output Drive Strength for CK */
            uint8_t reg_drv_ck;
            /* 139 Data Buffer Revision Number */
            uint8_t data_buf_rev;
            /* 140 DRAM VrefDQ for Package Rank 0 */
            uint8_t vrefqe_r0;
            /* 141 DRAM VrefDQ for Package Rank 1 */
            uint8_t vrefqe_r1;
            /* 142 DRAM VrefDQ for Package Rank 2 */
            uint8_t vrefqe_r2;
            /* 143 DRAM VrefDQ for Package Rank 3 */
            uint8_t vrefqe_r3;
            /* 144 Data Buffer VrefDQ for DRAM Interface */
            uint8_t data_intf;
            /*
             * 145 Data Buffer MDQ Drive Strength and RTT
             * for data rate <= 1866
             */
            uint8_t data_drv_1866;
            /*
             * 146 Data Buffer MDQ Drive Strength and RTT
             * for 1866 < data rate <= 2400
             */
            uint8_t data_drv_2400;
            /*
             * 147 Data Buffer MDQ Drive Strength and RTT
             * for 2400 < data rate <= 3200
             */
            uint8_t data_drv_3200;
            /* 148 DRAM Drive Strength */
            uint8_t dram_drv;
            /*
             * 149 DRAM ODT (RTT_WR, RTT_NOM)
             * for data rate <= 1866
             */
            uint8_t dram_odt_1866;
            /*
             * 150 DRAM ODT (RTT_WR, RTT_NOM)
             * for 1866 < data rate <= 2400
             */
            uint8_t dram_odt_2400;
            /*
             * 151 DRAM ODT (RTT_WR, RTT_NOM)
             * for 2400 < data rate <= 3200
             */
            uint8_t dram_odt_3200;
            /*
             * 152 DRAM ODT (RTT_PARK)
             * for data rate <= 1866
             */
            uint8_t dram_odt_park_1866;
            /*
             * 153 DRAM ODT (RTT_PARK)
             * for 1866 < data rate <= 2400
             */
            uint8_t dram_odt_park_2400;
            /*
             * 154 DRAM ODT (RTT_PARK)
             * for 2400 < data rate <= 3200
             */
            uint8_t dram_odt_park_3200;
            uint8_t res_155[254-155];    /* Reserved */
            /* 254~255 CRC */
            uint8_t crc[2];
        } loadreduced;
        uint8_t uc[128]; /* 128-255 Module-Specific Section */
    } mod_section;

    uint8_t res_256[320-256];    /* 256~319 Reserved */

    /* Module supplier's data: Byte 320~383 */
    uint8_t mmid_lsb;           /* 320 Module MfgID Code LSB */
    uint8_t mmid_msb;           /* 321 Module MfgID Code MSB */
    uint8_t mloc;               /* 322 Mfg Location */
    uint8_t mdate[2];           /* 323~324 Mfg Date */
    uint8_t sernum[4];          /* 325~328 Module Serial Number */
    uint8_t mpart[20];          /* 329~348 Mfg's Module Part Number */
    uint8_t mrev;               /* 349 Module Revision Code */
    uint8_t dmid_lsb;           /* 350 DRAM MfgID Code LSB */
    uint8_t dmid_msb;           /* 351 DRAM MfgID Code MSB */
    uint8_t stepping;           /* 352 DRAM stepping */
    uint8_t msd[29];            /* 353~381 Mfg's Specific Data */
    uint8_t res_382[2];         /* 382~383 Reserved */

    uint8_t user[512-384];      /* 384~511 End User Programmable */
};

static struct ddr4_spd_eeprom_s spd_eeprom[SPD_EEPROM_NUM] = {
    [0] = {
        .info_size_crc = 0x23,      /* #0 SPD Bytes Total: 512, SPD Bytes Used:384 */
        .spd_rev = 0x11,            /* #1 Production Revision 1.1 */
        .mem_type = 0x0C,           /* #2 DDR4 SDRAM */
        .module_type = 0x03,        /* #3 SO DIMM  */
        .density_banks = 0x52,      /* #4 2 bank groups, 4 banks, 1Gb (2GB) */
        .addressing = 0x21,         /* #5 Row bits: 16, Column bits: 10 */
        .package_type = 0x00,       /* #6 Monolithic single die DRAM */
        .opt_feature = 0x08,        /* #7 8192 * tREFI, Unlimited MAC */
        .thermal_ref = 0xE4,        /* #8 ??? */
        .oth_opt_features = 0x00,   /* #9 All PPR not supported  */
        .module_vdd = 0x03,         /* #11 Normal DRAM VDD=1.2V only */
        .organization = 0x09,       /* #12 2 Rank module using x8 chips */
        .bus_width = 0x03,          /* #13 64 bit primary bus, no parity or ECC */
        .therm_sensor = 0x00,       /* #14 Thermal sensor not incorporated onto this assembly */
        .ext_type = 0x00,           /* #15 None */
        .timebases = 0x00,          /* #17 MTB: 125ps, FTB: 1ps */
        .tck_min = 0x07,            /* #18 DDR4-2400 (1200 MHz clock) */
        .tck_max = 0x0D,            /* #19 DDR4-2400 (1200 MHz clock) */
        .caslat_b1 = 0xF8,          /* #20 CL supported:16-10 */
        .caslat_b2 = 0x03,          /* #21 see #20 */
        .caslat_b3 = 0x00,          /* #22 see #20 */
        .caslat_b4 = 0x00,          /* #23 see #20 */
        .taa_min = 0x6E,            /* #24 DDR4-1600K (800 MHz clock) */
        .trcd_min = 0x6E,           /* #25 DDR4-1600K (800 MHz clock) */
        .trp_min = 0x6E,            /* #26 DDR4-1600K (800 MHz clock) */
        .tras_trc_ext = 0x11,       /* #27 see #28 and #29 */
        .tras_min_lsb = 0x00,       /* #28 DDR4-2400 (1200 MHz clock) */
        .trc_min_lsb = 0x6E,        /* #29 DDR4-2400 (1200 MHz clock) */
        .trfc1_min_lsb = 0x30,      /* #30 16 Gb DDR4 SDRAM */
        .trfc1_min_msb = 0x11,      /* #31 see #30 */
        .trfc2_min_lsb = 0x20,      /* #32 16 Gb DDR4 SDRAM */
        .trfc2_min_msb = 0x08,      /* #33 see #32 */
        .trfc4_min_lsb = 0x20,      /* #34 16 Gb DDR4 SDRAM */
        .trfc4_min_msb = 0x08,      /* #35 see #34 */
        .tfaw_msb = 0x00,           /* #36 see #37 */
        .tfaw_min = 0x68,           /* #37 DDR4-2400, 1/2 KB page size */
        .trrds_min = 0x1B,          /* #38 DDR4-2400, 1/2 KB page size */
        .trrdl_min = 0x28,          /* #39 DDR4-2400, 1/2 KB page size */
        .tccdl_min = 0x28,          /* #40 DDR4-2400 */
        .mapping = {                /* #60-77  */
            0x0c, 0x2c, 0x15, 0x35, 0x15, 0x35, 0x0b, 0x2c, 0x15,
            0x35, 0x0b, 0x35, 0x0b, 0x2c, 0x0b, 0x35, 0x15, 0x36
        },
        .fine_tccdl_min = 0x00,     /* #117  */
        .fine_trrdl_min = 0x9C,     /* #118  */
        .fine_trrds_min = 0xB4,     /* #119  */
        .fine_trc_min = 0x00,       /* #120  */
        .fine_trp_min = 0x00,       /* #121  */
        .fine_trcd_min = 0x00,      /* #122  */
        .fine_taa_min = 0x00,       /* #123  */
        .fine_tck_max = 0xE7,       /* #124  */
        .fine_tck_min = 0xD6,       /* #125  */
        .crc = { 0x00, 0x00 },      /* #126-127 CRC1 */

        .mod_section = {
            .unbuffered = {
                .mod_height = 0x00,     /* #128  */ //0x11
                .mod_thickness = 0x00,  /* #129  */ //0x11
                .ref_raw_card = 0x4     /* #130  */ //0x20
            }
        }
    }
};

static uint16_t spd_crc16(uint8_t *ptr, int count)
{
    int crc, i;

    crc = 0;
    while (--count >= 0) {
        crc = crc ^ (int)*ptr++ << 8;
        for (i = 0; i < 8; ++i)
            if (crc & 0x8000)
                crc = crc << 1 ^ 0x1021;
            else
                crc = crc << 1;
    }
    return (uint16_t)(crc & 0xffff);
}

static void byte_copy(uint8_t *dest, const uint8_t *src, size_t n)
{
    size_t i;
    for (i = 0; i < n; ++i) {
        *(dest++) = *(src++);
    }
}

static void spd_quick_cmd(SMBusDevice *dev, uint8_t read)
{
    dprintf("%s: addr=0x%02x read=%d\n", __func__, dev->i2c.address, read);
}

static void spd_send_byte(SMBusDevice *dev, uint8_t val)
{
    struct DDR4SPDDevice *spd = (struct DDR4SPDDevice *)dev;

    dprintf("%s: addr=0x%02x val=0x%02x\n", __func__, dev->i2c.address, val);
    spd->offset = val;
}

static uint8_t spd_receive_byte(SMBusDevice *dev)
{
    struct DDR4SPDDevice *spd = (struct DDR4SPDDevice *)dev;
    uint8_t *data = spd->data;
    uint8_t val = data[spd->offset++];

    dprintf("%s: addr=0x%02x:%u val=0x%02x\n",
           __func__, dev->i2c.address, spd->offset - 1, val);
    return val;
}

static void spd_write_data(SMBusDevice *dev, uint8_t cmd, uint8_t *buf, int len)
{
    struct DDR4SPDDevice *spd = (struct DDR4SPDDevice *)dev;
    int n;

    dprintf("%s: addr=0x%02x cmd=0x%02x val=0x%02x len=%d\n",
           __func__, dev->i2c.address, cmd, buf[0], len);
    switch (dev->i2c.address) {
    case SPD_SPA0_ADDR:
        *(uintptr_t**)spd->alias = spd->data;
        break;
    case SPD_SPA1_ADDR:
        *(uintptr_t**)spd->alias = spd->data;
        break;
    default:
        /* A page write operation is not a valid SMBus command.
           It is a block write without a length byte.  Fortunately we
           get the full block anyway.  */
        /* TODO: Should this set the current location?  */
        if (cmd + len > 256) {
            n = 256 - cmd;
        } else {
            n = len;
        }
        byte_copy(spd->data + cmd, buf, n);
        len -= n;
        if (len) {
            byte_copy(spd->data, buf + n, len);
        }
        break;
    }
}

static uint8_t spd_read_data(SMBusDevice *dev, uint8_t cmd, int n)
{
    struct DDR4SPDDevice *spd = (struct DDR4SPDDevice *)dev;
    /* If this is the first byte then set the current position.  */
    if (n == 0)
        spd->offset = cmd;
    /* As with writes, we implement block reads without the
       SMBus length byte.  */
    return spd_receive_byte(dev);
}

static int spd_initfn(SMBusDevice *dev)
{
    struct DDR4SPDDevice *spd = (struct DDR4SPDDevice *)dev;

    spd->offset = 0;
    return 0;
}

static Property spd_properties[] = {
    DEFINE_PROP_PTR("data", struct DDR4SPDDevice, data),
    DEFINE_PROP_PTR("alias", struct DDR4SPDDevice, alias),
    DEFINE_PROP_END_OF_LIST(),
};

static void spd_class_initfn(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SMBusDeviceClass *sc = SMBUS_DEVICE_CLASS(klass);

    sc->init = spd_initfn;
    sc->quick_cmd = spd_quick_cmd;
    sc->send_byte = spd_send_byte;
    sc->receive_byte = spd_receive_byte;
    sc->write_data = spd_write_data;
    sc->read_data = spd_read_data;
    dc->props = spd_properties;
    /* Reason: pointer property "data" */
    dc->user_creatable = false;
}

static void spd_register_types(void)
{
    static const TypeInfo spd_info = {
        .name          = "ls1-spd",
        .parent        = TYPE_SMBUS_DEVICE,
        .instance_size = sizeof(struct DDR4SPDDevice),
        .class_init    = spd_class_initfn,
    };

    type_register_static(&spd_info);
}

type_init(spd_register_types)

void ls1_spd_init(I2CBus *smbus)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(spd_eeprom); ++i) {
        static const struct {
            uint8_t address;
            uint8_t lower_alias;
            uint8_t upper_alias;
        } slave_addrs[] = {
            { SPD_EEPROM_ADDR, SPD_SPA0_ADDR, SPD_SPA1_ADDR }
        };

        /* calculate crc for DDR4 */
        uint16_t csum;
        csum = spd_crc16((uint8_t *)&spd_eeprom[i], 126);
        spd_eeprom[i].crc[0] = (uint8_t)(csum & 0xFF);
        spd_eeprom[i].crc[1] = (uint8_t)(csum >> 8);

        csum = spd_crc16((uint8_t *)((uintptr_t)&spd_eeprom[i] + 128), 126);
        spd_eeprom[i].mod_section.uc[126] = (uint8_t)(csum & 0xFF);
        spd_eeprom[i].mod_section.uc[127] = (uint8_t)(csum >> 8);

        /* setup device */
        DeviceState *spd, *alias;
        spd = qdev_create((BusState *)smbus, "ls1-spd");
        qdev_prop_set_uint8(spd, "address", slave_addrs[i].address);
        qdev_prop_set_ptr(spd, "data", NULL);
        qdev_prop_set_ptr(spd, "alias", NULL);
        qdev_init_nofail(spd);
        alias = qdev_create((BusState *)smbus, "ls1-spd");
        qdev_prop_set_uint8(alias, "address", slave_addrs[i].lower_alias);
        qdev_prop_set_ptr(alias, "data", (uint8_t *)((uintptr_t)&spd_eeprom[i] + 0));
        qdev_prop_set_ptr(alias, "alias", &((struct DDR4SPDDevice *)spd)->data);
        qdev_init_nofail(alias);
        alias = qdev_create((BusState *)smbus, "ls1-spd");
        qdev_prop_set_uint8(alias, "address", slave_addrs[i].upper_alias);
        qdev_prop_set_ptr(alias, "data", (uint8_t *)((uintptr_t)&spd_eeprom[i] + 256));
        qdev_prop_set_ptr(alias, "alias", &((struct DDR4SPDDevice *)spd)->data);
        qdev_init_nofail(alias);
    }
}
