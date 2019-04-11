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
#ifndef LS1_MMCI_H
#define LS1_MMCI_H

#include "hw/sysbus.h"
#include "hw/sd/sd.h"


#define TYPE_LS1_MMCI "ls1-mmci"
#define LS1_MMCI(obj) OBJECT_CHECK(LS1MMCIState, (obj), TYPE_LS1_MMCI)

#define LS1_MMCI_FIFO_SIZE (128)

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    SDBus sdbus;
    QEMUTimer *transfer_timer;
    qemu_irq irq;

    uint32_t ds_addr;
    uint32_t blkattr;
    uint32_t cmdarg;
    uint32_t xfertyp;
    uint32_t cmdrsp[4];
    uint32_t prsstat;
    uint32_t proctl;
    uint32_t sysctl;
    uint32_t irqstat;
    uint32_t irqstaten;
    uint32_t irqsigen;
    uint32_t autocerr_sysctl2;
    uint32_t hostcapblt;
    uint32_t wml;
    uint32_t fevt;
    uint32_t admaes;
    uint32_t adsaddr;
    uint32_t hostver;
    uint32_t dmaerraddr;
    uint32_t dmaerrattr;
    uint32_t hostcapblt2;
    uint32_t tbctl;
    uint32_t tbptr;
    uint32_t sddirctl;
    uint32_t sdclkctl;
    uint32_t esdhcctl;

    uint32_t tx_fifo[LS1_MMCI_FIFO_SIZE];
    uint32_t tx_start;
    uint32_t tx_len;
    uint32_t rx_fifo[LS1_MMCI_FIFO_SIZE];
    uint32_t rx_start;
    uint32_t rx_len;
    uint32_t data_left;
} LS1MMCIState;

#endif /* LS1_MMCI_H */
