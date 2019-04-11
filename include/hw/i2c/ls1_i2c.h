/*
 *  LS1 I2C Bus Serial Interface registers definition
 *
 *  Copyright (C) 2017 t-kenji <protect.2501@gmail.com>
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
 */

#ifndef LS1_I2C_H
#define LS1_I2C_H

#include "hw/sysbus.h"

#define TYPE_LS1_I2C "ls1.i2c"
#define LS1_I2C(obj) OBJECT_CHECK(LS1I2CState, (obj), TYPE_LS1_I2C)

#define LS1_I2C_MEM_SIZE        (0x7)

/* LS1 I2C memory map */
#define IBAD_ADDR               (0x00)  /* address register */
#define IBFD_ADDR               (0x01)  /* frequency divider register */
#define IBCR_ADDR               (0x02)  /* control register */
#define IBSR_ADDR               (0x03)  /* status register */
#define IBDR_ADDR               (0x04)  /* data register */
#define IBIC_ADDR               (0x05)  /* interrupt register */
#define IBDBG_ADDR              (0x06)  /* debug register */

#define IBAD_MASK               (0xFE)
#define IBAD_RESET              (0x00)

#define IBFD_MASK               (0xFF)
#define IBFD_RESET              (0x00)

#define IBCR_MDIS               (1 << 7)
#define IBCR_IBIE               (1 << 6)
#define IBCR_MSSL               (1 << 5)
#define IBCR_TXRX               (1 << 4)
#define IBCR_NOACK              (1 << 3)
#define IBCR_RSTA               (1 << 2)
#define IBCR_DMAEN              (1 << 1)
#define IBCR_IBDOZE             (1 << 0)
#define IBCR_MASK               (0xFF)
#define IBCR_RESET              (0x80)

#define IBSR_TCF                (1 << 7)
#define IBSR_IAAF               (1 << 6)
#define IBSR_IBB                (1 << 5)
#define IBSR_IBAL               (1 << 4)
#define IBSR_SRW                (1 << 2)
#define IBSR_IBIF               (1 << 1)
#define IBSR_RXAK               (1 << 0)
#define IBSR_MASK               (0xF7)
#define IBSR_RESET              (0x80)

#define IBDR_MASK               (0xFF)
#define IBDR_RESET              (0x00)

#define IBIC_BIIE               (1 << 7)
#define IBIC_BYTERXIE           (1 << 6)
#define IBIC_MASK               (0xC0)
#define IBIC_RESET              (0x00)

#define IBDBG_GLFLT_EN          (1 << 3)
#define IBDBG_IPG_DEBUG_HALTED  (1 << 1)
#define IBDBG_IPG_DEBUG_EN      (1 << 0)
#define IBDBG_MASK              (0x0F)
#define IBDBG_RESET             (0x00)

#define ADDR_RESET              (0xFF00)

typedef struct LS1I2CState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    I2CBus *bus;
    qemu_irq irq;

    uint16_t  address;

    uint16_t ibad;
    uint16_t ibfd;
    uint16_t ibcr;
    uint16_t ibsr;
    uint16_t ibdr_read;
    uint16_t ibdr_write;
    uint16_t ibic;
    uint16_t ibdbg;
} LS1I2CState;

#endif /* LS1_I2C_H */
