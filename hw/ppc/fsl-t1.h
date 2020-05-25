/*
 * Freescale T1 SoC definitions.
 *
 * Copyright (c) 2020 t-kenji <protect.2501@gmail.com>
 */

#ifndef __FSL_T1_H__
#define __FSL_T1_H__

#define TYPE_FSL_T102X "fsl,t102x"
#define FSL_T102X(obj) \
    OBJECT_CHECK(FslT102xState, (obj), TYPE_FSL_T102X)
#define FSL_T102X_GET_CLASS(obj) \
    OBJECT_GET_CLASS(FslT102xClass, (obj), TYPE_FSL_T102X)
#define FSL_T102X_CLASS(cls) \
    OBJECT_CLASS_CHECK(FslT102xClass, (cls), TYPE_FSL_T102X)

#define SVR_T1024 (0x85400000)
#define SVR_T1023 (0x85410000)
#define SVR_T1014 (0x85440000)
#define SVR_T1013 (0x85450000)

enum {
    FSL_T102X__NUM_CPUS = 2,
    FSL_T102X__UBOOT_ENTRY = 0x30000000,
    FSL_T102X__UBOOT_SPL_ENTRY = 0xFFFD8000,
    FSL_T102X__RESET_VECTOR_ADDRESS = 0xFFFFFFFC,
};

typedef struct {
    /*< private >*/
    DeviceState parent_obj;

    /*< public >*/
    PowerPCCPU cpus[FSL_T102X__NUM_CPUS];
    IrqLines irqs[FSL_T102X__NUM_CPUS];
    uint32_t svr;
    void *rcw;
} FslT102xState;

typedef struct {
    /*< private >*/
    DeviceClass parent_cls;

    /*< public >*/
    int mpic_version;
    hwaddr ccsrbar_base;
} FslT102xClass;

typedef struct {
    uint32_t dt_base;
    uint32_t dt_size;
    uint32_t entry;
} PPCBootInfo;

#endif /* __FSL_T1_H__ */
