/*
 * Copyright (c) 2017 t-kenji <protect.2501@gmail.com>
 *
 * LS1046A SOC emulation.
 *
 * Based on hw/arm/fsl-imx6.c
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
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "qemu/error-report.h"
#include "hw/arm/fsl-ls1.h"
#include "hw/intc/arm_gic_common.h"
#include "hw/misc/ls1_ccsr.h"
#include "hw/misc/ls1_dpaa.h"
#include "chardev/char.h"
#include "sysemu/sysemu.h"
#include "sysemu/kvm.h"
#include "kvm_arm.h"


#define NAME_SIZE (20)


static void fsl_ls1046a_init(Object *obj)
{
    FslLS1046AState *s = FSL_LS1046A(obj);
    char name[NAME_SIZE];
    int i;

    if (smp_cpus > FSL_LS1046A_NUM_CPUS) {
        error_report("%s: Only %d CPUs are supported (%d requested)",
                     TYPE_FSL_LS1046A, FSL_LS1046A_NUM_CPUS, smp_cpus);
        exit(1);
    }

    for (i = 0; i < smp_cpus; ++i) {
        object_initialize(&s->cpus[i], sizeof(s->cpus[i]),
                          "cortex-a57-" TYPE_ARM_CPU);
        snprintf(name, NAME_SIZE, "cpu%d", i);
        object_property_add_child(obj, name, OBJECT(&s->cpus[i]), NULL);
    }

    object_initialize(&s->gic, sizeof(s->gic), gic_class_name());
    qdev_set_parent_bus(DEVICE(&s->gic), sysbus_get_default());
    object_property_add_child(obj, "gic", OBJECT(&s->gic), NULL);

    object_initialize(&s->ddr, sizeof(s->ddr), TYPE_CCSR_DDR);
    qdev_set_parent_bus(DEVICE(&s->ddr), sysbus_get_default());
    object_property_add_child(obj, "ddr", OBJECT(&s->ddr), NULL);

    object_initialize(&s->scfg, sizeof(s->scfg), TYPE_CCSR_SCFG);
    qdev_set_parent_bus(DEVICE(&s->scfg), sysbus_get_default());
    object_property_add_child(obj, "scfg", OBJECT(&s->scfg), NULL);

    object_initialize(&s->sec, sizeof(s->sec), TYPE_DPAA_SEC);
    qdev_set_parent_bus(DEVICE(&s->sec), sysbus_get_default());
    object_property_add_child(obj, "sec", OBJECT(&s->sec), NULL);

    object_initialize(&s->qman, sizeof(s->qman), TYPE_DPAA_QMAN);
    qdev_set_parent_bus(DEVICE(&s->qman), sysbus_get_default());
    object_property_add_child(obj, "qman", OBJECT(&s->qman), NULL);

    object_initialize(&s->bman, sizeof(s->bman), TYPE_DPAA_BMAN);
    qdev_set_parent_bus(DEVICE(&s->bman), sysbus_get_default());
    object_property_add_child(obj, "bman", OBJECT(&s->bman), NULL);

    object_initialize(&s->fman, sizeof(s->fman), TYPE_DPAA_FMAN);
    qdev_set_parent_bus(DEVICE(&s->fman), sysbus_get_default());
    object_property_add_child(obj, "fman", OBJECT(&s->fman), NULL);

    object_initialize(&s->guts, sizeof(s->guts), TYPE_CCSR_GUTS);
    qdev_set_parent_bus(DEVICE(&s->guts), sysbus_get_default());
    object_property_add_child(obj, "guts", OBJECT(&s->guts), NULL);

    object_initialize(&s->clk, sizeof(s->clk), TYPE_CCSR_CLK);
    qdev_set_parent_bus(DEVICE(&s->clk), sysbus_get_default());
    object_property_add_child(obj, "clk", OBJECT(&s->clk), NULL);

#if 0
    object_initialize(&s->a9mpcore, sizeof(s->a9mpcore), TYPE_A9MPCORE_PRIV);
    qdev_set_parent_bus(DEVICE(&s->a9mpcore), sysbus_get_default());
    object_property_add_child(obj, "a9mpcore", OBJECT(&s->a9mpcore), NULL);

    object_initialize(&s->ccm, sizeof(s->ccm), TYPE_IMX6_CCM);
    qdev_set_parent_bus(DEVICE(&s->ccm), sysbus_get_default());
    object_property_add_child(obj, "ccm", OBJECT(&s->ccm), NULL);

    object_initialize(&s->src, sizeof(s->src), TYPE_IMX6_SRC);
    qdev_set_parent_bus(DEVICE(&s->src), sysbus_get_default());
    object_property_add_child(obj, "src", OBJECT(&s->src), NULL);

    for (i = 0; i < FSL_IMX6_NUM_UARTS; i++) {
        object_initialize(&s->uart[i], sizeof(s->uart[i]), TYPE_IMX_SERIAL);
        qdev_set_parent_bus(DEVICE(&s->uart[i]), sysbus_get_default());
        snprintf(name, NAME_SIZE, "uart%d", i + 1);
        object_property_add_child(obj, name, OBJECT(&s->uart[i]), NULL);
    }

    object_initialize(&s->gpt, sizeof(s->gpt), TYPE_IMX6_GPT);
    qdev_set_parent_bus(DEVICE(&s->gpt), sysbus_get_default());
    object_property_add_child(obj, "gpt", OBJECT(&s->gpt), NULL);

    for (i = 0; i < FSL_IMX6_NUM_EPITS; i++) {
        object_initialize(&s->epit[i], sizeof(s->epit[i]), TYPE_IMX_EPIT);
        qdev_set_parent_bus(DEVICE(&s->epit[i]), sysbus_get_default());
        snprintf(name, NAME_SIZE, "epit%d", i + 1);
        object_property_add_child(obj, name, OBJECT(&s->epit[i]), NULL);
    }
#endif

    for (i = 0; i < FSL_LS1046A_NUM_I2CS; ++i) {
        object_initialize(&s->i2cs[i], sizeof(s->i2cs[i]), TYPE_VF610_I2C);
        qdev_set_parent_bus(DEVICE(&s->i2cs[i]), sysbus_get_default());
        snprintf(name, NAME_SIZE, "i2c%d", i + 1);
        object_property_add_child(obj, name, OBJECT(&s->i2cs[i]), NULL);
    }
#if 0
    for (i = 0; i < FSL_IMX6_NUM_GPIOS; i++) {
        object_initialize(&s->gpio[i], sizeof(s->gpio[i]), TYPE_IMX_GPIO);
        qdev_set_parent_bus(DEVICE(&s->gpio[i]), sysbus_get_default());
        snprintf(name, NAME_SIZE, "gpio%d", i + 1);
        object_property_add_child(obj, name, OBJECT(&s->gpio[i]), NULL);
    }
#endif

    object_initialize(&s->esdhc, sizeof(s->esdhc), TYPE_LS1_MMCI);
    qdev_set_parent_bus(DEVICE(&s->esdhc), sysbus_get_default());
    object_property_add_child(obj, "esdhc", OBJECT(&s->esdhc), NULL);
    object_property_add_alias(obj, "sd-bus", OBJECT(&s->esdhc),
                              "sd-bus", NULL);
#if 0
    for (i = 0; i < FSL_IMX6_NUM_ECSPIS; i++) {
        object_initialize(&s->spi[i], sizeof(s->spi[i]), TYPE_IMX_SPI);
        qdev_set_parent_bus(DEVICE(&s->spi[i]), sysbus_get_default());
        snprintf(name, NAME_SIZE, "spi%d", i + 1);
        object_property_add_child(obj, name, OBJECT(&s->spi[i]), NULL);
    }

    object_initialize(&s->eth, sizeof(s->eth), TYPE_IMX_ENET);
    qdev_set_parent_bus(DEVICE(&s->eth), sysbus_get_default());
    object_property_add_child(obj, "eth", OBJECT(&s->eth), NULL);
#endif

    object_initialize(&s->qmsp, sizeof(s->qmsp), TYPE_DPAA_QMSP);
    qdev_set_parent_bus(DEVICE(&s->qmsp), sysbus_get_default());
    object_property_add_child(obj, "qmsp", OBJECT(&s->qmsp), NULL);

    object_initialize(&s->bmsp, sizeof(s->bmsp), TYPE_DPAA_BMSP);
    qdev_set_parent_bus(DEVICE(&s->bmsp), sysbus_get_default());
    object_property_add_child(obj, "bmsp", OBJECT(&s->bmsp), NULL);
}

static Error *setup_gic(FslLS1046AState *s)
{
    Object *gicobj = OBJECT(&s->gic);
    DeviceState *gicdev = DEVICE(&s->gic);
    SysBusDevice *gicbus = SYS_BUS_DEVICE(&s->gic);
    int cpu, irq;
    Error *err = NULL;

    object_property_set_int(gicobj, smp_cpus, "num-cpu",
                            &error_abort);

    object_property_set_int(gicobj,
                            FSL_LS1046A_NUM_IRQ + GIC_INTERNAL, "num-irq",
                            &error_abort);

    object_property_set_bool(gicobj, true, "realized", &err);
    if (err) {
        return err;
    }

    for (cpu = 0; cpu < smp_cpus; ++cpu) {
        DeviceState *cpudev = DEVICE(qemu_get_cpu(cpu));
        int ppibase = FSL_LS1046A_NUM_IRQ + cpu * GIC_INTERNAL + GIC_NR_SGIS;
        const int timer_irq[] = {
            [GTIMER_PHYS] = ARCH_TIMER_NS_EL1_IRQ,
            [GTIMER_VIRT] = ARCH_TIMER_VIRT_IRQ,
            [GTIMER_HYP] = ARCH_TIMER_NS_EL2_IRQ,
            [GTIMER_SEC] = ARCH_TIMER_S_EL1_IRQ
        };
        for (irq = 0; irq < ARRAY_SIZE(timer_irq); ++irq) {
            qdev_connect_gpio_out(cpudev, irq,
                                  qdev_get_gpio_in(gicdev,
                                                   ppibase + timer_irq[irq]));
        }

        sysbus_connect_irq(gicbus, cpu,
                           qdev_get_gpio_in(cpudev, ARM_CPU_IRQ));
        sysbus_connect_irq(gicbus, cpu + smp_cpus,
                           qdev_get_gpio_in(cpudev, ARM_CPU_FIQ));
        sysbus_connect_irq(gicbus, cpu + 2 * smp_cpus,
                           qdev_get_gpio_in(cpudev, ARM_CPU_VIRQ));
        sysbus_connect_irq(gicbus, cpu + 3 * smp_cpus,
                           qdev_get_gpio_in(cpudev, ARM_CPU_VFIQ));
    }

    for (irq = 0; irq < FSL_LS1046A_NUM_IRQ; ++irq) {
        s->irqs[irq] = qdev_get_gpio_in(gicdev, irq);
    }

    return NULL;
}

static void fsl_ls1046a_realize(DeviceState *dev, Error **errp)
{
    FslLS1046AState *s = FSL_LS1046A(dev);
    MemoryRegion *mr;
    uint16_t i;
    Error *err = NULL;

    for (i = 0; i < smp_cpus; ++i) {

        /* On uniprocessor, the CBAR is set to 0 */
        if (smp_cpus > 1) {
            object_property_set_int(OBJECT(&s->cpus[i]),
                                    FSL_LS1046A_CCSR_ADDR + LS1046A_CCSR_GIC_BASE_OFFSET,
                                    "reset-cbar", &error_abort);
        }

        /* All CPU but CPU 0 start in power off mode */
        if (i) {
            object_property_set_bool(OBJECT(&s->cpus[i]), true,
                                     "start-powered-off", &error_abort);
        }

        object_property_set_bool(OBJECT(&s->cpus[i]),
                                 true, "has_el3", NULL);
        object_property_set_bool(OBJECT(&s->cpus[i]),
                                 true, "has_el2", NULL);
        object_property_set_bool(OBJECT(&s->cpus[i]), true, "realized", &err);
        if (err) {
            error_propagate(errp, err);
            return;
        }
    }

    /* CCSR memory */
    memory_region_init_ram(&s->ccsr, NULL, "ls1046a.ccsr",
                           FSL_LS1046A_CCSR_SIZE, &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    memory_region_add_subregion(get_system_memory(), FSL_LS1046A_CCSR_ADDR,
                                &s->ccsr);

    object_property_set_bool(OBJECT(&s->ddr), true, "realized", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->ddr), 0);
    memory_region_add_subregion(&s->ccsr, LS1046A_CCSR_DDR_OFFSET, mr);

    err = setup_gic(s);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->gic), 0);
    memory_region_add_subregion(&s->ccsr, LS1046A_CCSR_GIC_DIST_OFFSET, mr);
    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->gic), 1);
    memory_region_add_subregion(&s->ccsr, LS1046A_CCSR_GIC_CPU_OFFSET, mr);

    object_property_set_bool(OBJECT(&s->scfg), true, "realized", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->scfg), 0);
    memory_region_add_subregion(&s->ccsr, LS1046A_CCSR_SCFG_OFFSET, mr);

    object_property_set_bool(OBJECT(&s->sec), true, "realized", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->sec), 0);
    memory_region_add_subregion(&s->ccsr, LS1046A_CCSR_SEC_OFFSET, mr);

    object_property_set_bool(OBJECT(&s->qman), true, "realized", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->qman), 0);
    memory_region_add_subregion(&s->ccsr, LS1046A_CCSR_QMAN_OFFSET, mr);

    object_property_set_bool(OBJECT(&s->bman), true, "realized", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->bman), 0);
    memory_region_add_subregion(&s->ccsr, LS1046A_CCSR_BMAN_OFFSET, mr);

    object_property_set_bool(OBJECT(&s->fman), true, "realized", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->fman), 0);
    memory_region_add_subregion(&s->ccsr, LS1046A_CCSR_FMAN_OFFSET, mr);

    object_property_set_bool(OBJECT(&s->guts), true, "realized", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->guts), 0);
    memory_region_add_subregion(&s->ccsr, LS1046A_CCSR_GUTS_OFFSET, mr);

    object_property_set_bool(OBJECT(&s->clk), true, "realized", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->clk), 0);
    memory_region_add_subregion(&s->ccsr, LS1046A_CCSR_CLK_OFFSET, mr);

    /* Initialize all I2C */
    for (i = 0; i < FSL_LS1046A_NUM_I2CS; ++i) {
        static const struct {
            hwaddr addr;
            unsigned int irq;
        } i2c_table[] = {
            { LS1046A_CCSR_I2C1_OFFSET, FSL_LS1046A_I2C1_IRQ },
            { LS1046A_CCSR_I2C2_OFFSET, FSL_LS1046A_I2C2_IRQ },
            { LS1046A_CCSR_I2C3_OFFSET, FSL_LS1046A_I2C3_IRQ },
            { LS1046A_CCSR_I2C4_OFFSET, FSL_LS1046A_I2C4_IRQ }
        };

        object_property_set_bool(OBJECT(&s->i2cs[i]), true, "realized", &err);
        if (err) {
            error_propagate(errp, err);
            return;
        }

        mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->i2cs[i]), 0);
        memory_region_add_subregion(&s->ccsr, i2c_table[i].addr, mr);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->i2cs[i]), 0,
                           s->irqs[i2c_table[i].irq]);
    }

    /* Initialize eSDHC */
    object_property_set_bool(OBJECT(&s->esdhc), true, "realized", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->esdhc), 0);
    memory_region_add_subregion(&s->ccsr, LS1046A_CCSR_ESDHC_OFFSET, mr);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->esdhc), 0,
                       s->irqs[FSL_LS1046A_ESDHC_IRQ]);

    /* DUART1 */
    serial_mm_init(&s->ccsr, LS1046A_CCSR_DUART1_OFFSET, 0,
                   s->irqs[FSL_LS1046A_DUART1_IRQ], 115200, serial_hds[0],
                   DEVICE_LITTLE_ENDIAN);
#if 0
    object_property_set_bool(OBJECT(&s->ccm), true, "realized", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->ccm), 0, FSL_IMX6_CCM_ADDR);

    object_property_set_bool(OBJECT(&s->src), true, "realized", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->src), 0, FSL_IMX6_SRC_ADDR);

    /* Initialize all UARTs */
    for (i = 0; i < FSL_IMX6_NUM_UARTS; i++) {
        static const struct {
            hwaddr addr;
            unsigned int irq;
        } serial_table[FSL_IMX6_NUM_UARTS] = {
            { FSL_IMX6_UART1_ADDR, FSL_IMX6_UART1_IRQ },
            { FSL_IMX6_UART2_ADDR, FSL_IMX6_UART2_IRQ },
            { FSL_IMX6_UART3_ADDR, FSL_IMX6_UART3_IRQ },
            { FSL_IMX6_UART4_ADDR, FSL_IMX6_UART4_IRQ },
            { FSL_IMX6_UART5_ADDR, FSL_IMX6_UART5_IRQ },
        };

        if (i < MAX_SERIAL_PORTS) {
            Chardev *chr;

            chr = serial_hds[i];

            if (!chr) {
                char *label = g_strdup_printf("imx6.uart%d", i + 1);
                chr = qemu_chr_new(label, "null");
                g_free(label);
                serial_hds[i] = chr;
            }

            qdev_prop_set_chr(DEVICE(&s->uart[i]), "chardev", chr);
        }

        object_property_set_bool(OBJECT(&s->uart[i]), true, "realized", &err);
        if (err) {
            error_propagate(errp, err);
            return;
        }

        sysbus_mmio_map(SYS_BUS_DEVICE(&s->uart[i]), 0, serial_table[i].addr);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->uart[i]), 0,
                           qdev_get_gpio_in(DEVICE(&s->a9mpcore),
                                            serial_table[i].irq));
    }

    s->gpt.ccm = IMX_CCM(&s->ccm);

    object_property_set_bool(OBJECT(&s->gpt), true, "realized", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }

    sysbus_mmio_map(SYS_BUS_DEVICE(&s->gpt), 0, FSL_IMX6_GPT_ADDR);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->gpt), 0,
                       qdev_get_gpio_in(DEVICE(&s->a9mpcore),
                                        FSL_IMX6_GPT_IRQ));

    /* Initialize all EPIT timers */
    for (i = 0; i < FSL_IMX6_NUM_EPITS; i++) {
        static const struct {
            hwaddr addr;
            unsigned int irq;
        } epit_table[FSL_IMX6_NUM_EPITS] = {
            { FSL_IMX6_EPIT1_ADDR, FSL_IMX6_EPIT1_IRQ },
            { FSL_IMX6_EPIT2_ADDR, FSL_IMX6_EPIT2_IRQ },
        };

        s->epit[i].ccm = IMX_CCM(&s->ccm);

        object_property_set_bool(OBJECT(&s->epit[i]), true, "realized", &err);
        if (err) {
            error_propagate(errp, err);
            return;
        }

        sysbus_mmio_map(SYS_BUS_DEVICE(&s->epit[i]), 0, epit_table[i].addr);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->epit[i]), 0,
                           qdev_get_gpio_in(DEVICE(&s->a9mpcore),
                                            epit_table[i].irq));
    }
#endif

#if 0
    /* Initialize all GPIOs */
    for (i = 0; i < FSL_IMX6_NUM_GPIOS; i++) {
        static const struct {
            hwaddr addr;
            unsigned int irq_low;
            unsigned int irq_high;
        } gpio_table[FSL_IMX6_NUM_GPIOS] = {
            {
                FSL_IMX6_GPIO1_ADDR,
                FSL_IMX6_GPIO1_LOW_IRQ,
                FSL_IMX6_GPIO1_HIGH_IRQ
            },
            {
                FSL_IMX6_GPIO2_ADDR,
                FSL_IMX6_GPIO2_LOW_IRQ,
                FSL_IMX6_GPIO2_HIGH_IRQ
            },
            {
                FSL_IMX6_GPIO3_ADDR,
                FSL_IMX6_GPIO3_LOW_IRQ,
                FSL_IMX6_GPIO3_HIGH_IRQ
            },
            {
                FSL_IMX6_GPIO4_ADDR,
                FSL_IMX6_GPIO4_LOW_IRQ,
                FSL_IMX6_GPIO4_HIGH_IRQ
            },
            {
                FSL_IMX6_GPIO5_ADDR,
                FSL_IMX6_GPIO5_LOW_IRQ,
                FSL_IMX6_GPIO5_HIGH_IRQ
            },
            {
                FSL_IMX6_GPIO6_ADDR,
                FSL_IMX6_GPIO6_LOW_IRQ,
                FSL_IMX6_GPIO6_HIGH_IRQ
            },
            {
                FSL_IMX6_GPIO7_ADDR,
                FSL_IMX6_GPIO7_LOW_IRQ,
                FSL_IMX6_GPIO7_HIGH_IRQ
            },
        };

        object_property_set_bool(OBJECT(&s->gpio[i]), true, "has-edge-sel",
                                 &error_abort);
        object_property_set_bool(OBJECT(&s->gpio[i]), true, "has-upper-pin-irq",
                                 &error_abort);
        object_property_set_bool(OBJECT(&s->gpio[i]), true, "realized", &err);
        if (err) {
            error_propagate(errp, err);
            return;
        }

        sysbus_mmio_map(SYS_BUS_DEVICE(&s->gpio[i]), 0, gpio_table[i].addr);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->gpio[i]), 0,
                           qdev_get_gpio_in(DEVICE(&s->a9mpcore),
                                            gpio_table[i].irq_low));
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->gpio[i]), 1,
                           qdev_get_gpio_in(DEVICE(&s->a9mpcore),
                                            gpio_table[i].irq_high));
    }

    /* Initialize all SDHC */
    for (i = 0; i < FSL_IMX6_NUM_ESDHCS; i++) {
        static const struct {
            hwaddr addr;
            unsigned int irq;
        } esdhc_table[FSL_IMX6_NUM_ESDHCS] = {
            { FSL_IMX6_uSDHC1_ADDR, FSL_IMX6_uSDHC1_IRQ },
            { FSL_IMX6_uSDHC2_ADDR, FSL_IMX6_uSDHC2_IRQ },
            { FSL_IMX6_uSDHC3_ADDR, FSL_IMX6_uSDHC3_IRQ },
            { FSL_IMX6_uSDHC4_ADDR, FSL_IMX6_uSDHC4_IRQ },
        };

        object_property_set_bool(OBJECT(&s->esdhc[i]), true, "realized", &err);
        if (err) {
            error_propagate(errp, err);
            return;
        }
        sysbus_mmio_map(SYS_BUS_DEVICE(&s->esdhc[i]), 0, esdhc_table[i].addr);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->esdhc[i]), 0,
                           qdev_get_gpio_in(DEVICE(&s->a9mpcore),
                                            esdhc_table[i].irq));
    }

    /* Initialize all ECSPI */
    for (i = 0; i < FSL_IMX6_NUM_ECSPIS; i++) {
        static const struct {
            hwaddr addr;
            unsigned int irq;
        } spi_table[FSL_IMX6_NUM_ECSPIS] = {
            { FSL_IMX6_eCSPI1_ADDR, FSL_IMX6_ECSPI1_IRQ },
            { FSL_IMX6_eCSPI2_ADDR, FSL_IMX6_ECSPI2_IRQ },
            { FSL_IMX6_eCSPI3_ADDR, FSL_IMX6_ECSPI3_IRQ },
            { FSL_IMX6_eCSPI4_ADDR, FSL_IMX6_ECSPI4_IRQ },
            { FSL_IMX6_eCSPI5_ADDR, FSL_IMX6_ECSPI5_IRQ },
        };

        /* Initialize the SPI */
        object_property_set_bool(OBJECT(&s->spi[i]), true, "realized", &err);
        if (err) {
            error_propagate(errp, err);
            return;
        }

        sysbus_mmio_map(SYS_BUS_DEVICE(&s->spi[i]), 0, spi_table[i].addr);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->spi[i]), 0,
                           qdev_get_gpio_in(DEVICE(&s->a9mpcore),
                                            spi_table[i].irq));
    }

    object_property_set_bool(OBJECT(&s->eth), true, "realized", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->eth), 0, FSL_IMX6_ENET_ADDR);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->eth), 0,
                       qdev_get_gpio_in(DEVICE(&s->a9mpcore),
                                        FSL_IMX6_ENET_MAC_IRQ));
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->eth), 1,
                       qdev_get_gpio_in(DEVICE(&s->a9mpcore),
                                        FSL_IMX6_ENET_MAC_1588_IRQ));
#endif

    /* ROM memory */
    memory_region_init_rom(&s->rom, NULL, "ls1046a.rom",
                           FSL_LS1046A_ROM_SIZE, &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    memory_region_add_subregion(get_system_memory(), FSL_LS1046A_ROM_ADDR,
                                &s->rom);

#if 0
    /* CAAM memory */
    memory_region_init_rom(&s->caam, NULL, "imx6.caam",
                           FSL_IMX6_CAAM_MEM_SIZE, &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    memory_region_add_subregion(get_system_memory(), FSL_IMX6_CAAM_MEM_ADDR,
                                &s->caam);
#endif

    /* OCRAM memory */
    memory_region_init_ram(&s->ocram0, NULL, "ls1046a.ocram0",
                           FSL_LS1046A_OCRAM0_SIZE, &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    memory_region_add_subregion(get_system_memory(), FSL_LS1046A_OCRAM0_ADDR,
                                &s->ocram0);
    memory_region_init_ram(&s->ocram1, NULL, "ls1046a.ocram1",
                           FSL_LS1046A_OCRAM1_SIZE, &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    memory_region_add_subregion(get_system_memory(), FSL_LS1046A_OCRAM1_ADDR,
                                &s->ocram1);

    object_property_set_bool(OBJECT(&s->qmsp), true, "realized", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->qmsp), 0);
    memory_region_add_subregion(get_system_memory(), FSL_LS1046A_QMSP_ADDR, mr);

    object_property_set_bool(OBJECT(&s->bmsp), true, "realized", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->bmsp), 0);
    memory_region_add_subregion(get_system_memory(), FSL_LS1046A_BMSP_ADDR, mr);
}

static void fsl_ls1046a_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = fsl_ls1046a_realize;

    dc->desc = "Freescale QorIQ LS1046A SOC";
}

static const TypeInfo fsl_ls1046a_type_info = {
    .name = TYPE_FSL_LS1046A,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(FslLS1046AState),
    .instance_init = fsl_ls1046a_init,
    .class_init = fsl_ls1046a_class_init,
};

static void fsl_ls1046a_register_types(void)
{
    type_register_static(&fsl_ls1046a_type_info);
}

type_init(fsl_ls1046a_register_types)
