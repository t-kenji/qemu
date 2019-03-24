/*
 * Copyright (c) 2017-2019 t-kenji <protect.2501@gmail.com>
 *
 * QorIQ LS1046A Data Path Acceleration Architecture  pseudo-device
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

#ifndef LS1_DPAA_H
#define LS1_DPAA_H


#define TYPE_LS1_DPAA_QMSP "ls1.dpaa-qmsp"
#define LS1_DPAA_QMSP(obj) OBJECT_CHECK(DPAAQMSPState, (obj), TYPE_LS1_DPAA_QMSP)

#define TYPE_LS1_DPAA_BMSP "ls1.dpaa-bmsp"
#define LS1_DPAA_BMSP(obj) OBJECT_CHECK(DPAABMSPState, (obj), TYPE_LS1_DPAA_BMSP)

#define TYPE_LS1_DPAA_SEC "ls1.dpaa-sec"
#define LS1_DPAA_SEC(obj) OBJECT_CHECK(DPAASecState, (obj), TYPE_LS1_DPAA_SEC)

#define TYPE_LS1_DPAA_QMAN "ls1.dpaa-qman"
#define LS1_DPAA_QMAN(obj) OBJECT_CHECK(DPAAQManState, (obj), TYPE_LS1_DPAA_QMAN)

#define TYPE_LS1_DPAA_BMAN "ls1.dpaa-bman"
#define LS1_DPAA_BMAN(obj) OBJECT_CHECK(DPAABManState, (obj), TYPE_LS1_DPAA_BMAN)

#define TYPE_LS1_DPAA_FMAN "ls1.dpaa-fman"
#define LS1_DPAA_FMAN(obj) OBJECT_CHECK(DPAAFManState, (obj), TYPE_LS1_DPAA_FMAN)


typedef struct DPAAQMSPState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;

    uint32_t qcsp_rr0[10][16];
    uint32_t qcsp_rr1[10][16];
    uint32_t qcsp_cr[10][16];
    uint32_t qcsp_eqcr_ci_cinh[10];
    uint32_t qcsp_isdr[10];
} DPAAQMSPState;

typedef struct DPAABMSPState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
} DPAABMSPState;

typedef struct DPAASecState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
} DPAASecState;

typedef struct DPAAQManState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
} DPAAQManState;

typedef struct DPAABManState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
} DPAABManState;

typedef struct DPAAFManState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
} DPAAFManState;


#endif /* LS1_DPAA_H */
