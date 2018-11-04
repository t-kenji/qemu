/*
 * CSR Quatro 5500 SoC emulation
 *
 * Copyright (C) 2018 t-kenji <protect.2501@gmail.com>
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
#include "qapi/error.h"
#include "cpu.h"
#include "hw/arm/csr-quatro.h"
#include "hw/char/serial.h"
#include "hw/ide/ahci.h"
#include "hw/usb/hcd-xhci.h"
#include "exec/address-spaces.h"
#include "sysemu/kvm.h"
#include "kvm_arm.h"
#include "qemu/log.h"

static void csr_quatro_init(Object *obj)
{
    CsrQuatroState *ms = CSR_QUATRO(obj);
    int num_cpu = AP_CPUS;

    object_initialize_child(obj, "ap-cpu[*]",
                            &ms->ap_cpus[0], sizeof(ms->ap_cpus[0]),
                            ARM_CPU_TYPE_NAME("cortex-a7"),
                            &error_abort, NULL);
    for (int i = 1; i < num_cpu; ++i) {
        object_initialize_child(obj, "ap-cpu[*]",
                                &ms->ap_cpus[i], sizeof(ms->ap_cpus[0]),
                                ARM_CPU_TYPE_NAME("cortex-a15"),
                                &error_abort, NULL);
    }

    /* Cortex-A7 MP Core.
     */
    sysbus_init_child_obj(obj, "a7mpcore",
                          &ms->a7mpcore, sizeof(ms->a7mpcore),
                          TYPE_A15MPCORE_PRIV);
}

static void csr_quatro_realize(DeviceState *dev, Error **errp)
{
    CsrQuatroState *ms = CSR_QUATRO(dev);
    int num_cpu = AP_CPUS;

    for (int i = 0; i < num_cpu; ++i) {
        Object *cpu = OBJECT(&ms->ap_cpus[i]);

        /* On uniprocessor, the CBAR is set to 0 */
        if (num_cpu > 1) {
            object_property_set_int(cpu, CSR_QUATRO_A7MPCORE_ADDR,
                                    "reset-cbar", &error_abort);
        }

        /* All CPU but CPU 0 start in power off mode */
        if (i != 0) {
            object_property_set_bool(cpu, true,
                                     "start-powered-off", &error_abort);
        }

        object_property_set_bool(cpu, true, "realized", &error_abort);
    }

    /* Cortex-A7 MP Core.
     */
    SysBusDevice *sbd = SYS_BUS_DEVICE(&ms->a7mpcore);
    object_property_set_int(OBJECT(&ms->a7mpcore),
                            num_cpu, "num-cpu",
                            &error_abort);
    object_property_set_int(OBJECT(&ms->a7mpcore),
                            CSR_QUATRO_GIC_NUM_SPI_INTR + GIC_INTERNAL, "num-irq",
                            &error_abort);
    object_property_set_bool(OBJECT(&ms->a7mpcore),
                             true, "realized",
                             &error_abort);
    sysbus_mmio_map(sbd, 0, CSR_QUATRO_A7MPCORE_ADDR);

    memory_region_init_ram(&ms->sram, OBJECT(dev), "quatro5500.sram",
                           CSR_QUATRO_SRAM_SIZE, &error_abort);
    memory_region_add_subregion(get_system_memory(), CSR_QUATRO_SRAM_ADDR,
                                &ms->sram);

    /* Connect the CPUs to the GIC. */
    for (int i = 0; i < num_cpu; ++i) {
        DeviceState *cpu = DEVICE(qemu_get_cpu(i));

        sysbus_connect_irq(sbd, i,
                           qdev_get_gpio_in(cpu, ARM_CPU_IRQ));
        sysbus_connect_irq(sbd, i + num_cpu,
                           qdev_get_gpio_in(cpu, ARM_CPU_FIQ));
    }

    sysbus_create_simple("quatro5500.rstgen", CSR_QUATRO_RSTGEN_ADDR, NULL);
    sysbus_create_simple("quatro5500.clk", CSR_QUATRO_CLK_ADDR, NULL);
    sysbus_create_simple("quatro5500.rtc", CSR_QUATRO_RTC_ADDR, NULL);
    sysbus_create_simple("quatro5500.hrt0", CSR_QUATRO_HRT0_ADDR, NULL);
    sysbus_create_simple("quatro5500.sdmclk", CSR_QUATRO_SDMCLK_ADDR, NULL);
    sysbus_create_simple("quatro5500.ddrmc", CSR_QUATRO_DDRMC_ADDR, NULL);
    sysbus_create_simple("quatro5500.a15gpf", CSR_QUATRO_A15GPF_ADDR, NULL);
    sysbus_create_simple("quatro5500.sdiocore", CSR_QUATRO_SDIO0_ADDR, NULL);
    sysbus_create_simple("quatro5500.sdiocore", CSR_QUATRO_SDIO1_ADDR, NULL);
    sysbus_create_simple(TYPE_QUATRO5500_XHCI, CSR_QUATRO_USBH_ADDR, NULL);
    sysbus_create_simple("quatro5500.gpdma", CSR_QUATRO_GPDMA0_ADDR, NULL);
    sysbus_create_simple("quatro5500.gpdma", CSR_QUATRO_GPDMA1_ADDR, NULL);
    sysbus_create_simple("quatro5500.ttc", CSR_QUATRO_TTC0_ADDR, NULL);
    sysbus_create_simple("quatro5500.ttc", CSR_QUATRO_TTC1_ADDR, NULL);
    sysbus_create_simple(TYPE_SYSBUS_AHCI, CSR_QUATRO_SATA_ADDR, NULL);
    sysbus_create_simple("quatro5500.sbe", CSR_QUATRO_SBE0_ADDR, NULL);
    sysbus_create_simple("quatro5500.sbe", CSR_QUATRO_SBE1_ADDR, NULL);
    sysbus_create_simple("quatro5500.fir", CSR_QUATRO_FIR0_ADDR, NULL);
    sysbus_create_simple("quatro5500.fir", CSR_QUATRO_FIR1_ADDR, NULL);
    sysbus_create_simple("quatro5500.scal", CSR_QUATRO_SCAL0_ADDR, NULL);
    sysbus_create_simple("quatro5500.scal", CSR_QUATRO_SCAL1_ADDR, NULL);
    sysbus_create_simple("quatro5500.scrn", CSR_QUATRO_SCRN0_ADDR, NULL);
    sysbus_create_simple("quatro5500.scrn", CSR_QUATRO_SCRN1_ADDR, NULL);
    sysbus_create_simple("quatro5500.jbig", CSR_QUATRO_JBIG0_ADDR, NULL);
    sysbus_create_simple("quatro5500.lpri", CSR_QUATRO_LPRI0_ADDR, NULL);
    sysbus_create_simple("quatro5500.jbig", CSR_QUATRO_JBIG1_ADDR, NULL);
    sysbus_create_simple("quatro5500.lcdc", CSR_QUATRO_LCDC_ADDR, NULL);
    sysbus_create_simple("quatro5500.dsp", CSR_QUATRO_DSP0_ADDR, NULL);
    sysbus_create_simple("quatro5500.dsp", CSR_QUATRO_DSP1_ADDR, NULL);
    sysbus_create_simple("quatro5500.cm3", CSR_QUATRO_CM30_ADDR, NULL);
    sysbus_create_simple("quatro5500.cm3", CSR_QUATRO_CM31_ADDR, NULL);

    /* UARTs.
     */
    for (int i = 0; i < CSR_QUATRO_NUM_UARTS; ++i) {
        static const struct {
            hwaddr addr;
            int irq;
        } uarts[] = {
            {CSR_QUATRO_UART0_ADDR, CSR_QUATRO_UART0_IRQ},
            {CSR_QUATRO_UART1_ADDR, CSR_QUATRO_UART1_IRQ},
            {CSR_QUATRO_UART2_ADDR, CSR_QUATRO_UART2_IRQ},
        };
        if (serial_hd(i)) {
            serial_mm_init(get_system_memory(),
                           uarts[i].addr, 0,
                           qdev_get_gpio_in(DEVICE(&ms->a7mpcore), uarts[i].irq),
                           115200, serial_hd(i), DEVICE_LITTLE_ENDIAN);
        }
    }
}

static void csr_quatro_class_init(ObjectClass *oc, void *data)
{
    static Property props[] = {
        DEFINE_PROP_END_OF_LIST()
    };

    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->props   = props;
    dc->realize = csr_quatro_realize;

    /* Reason: Uses serial_hds in realize function, thus can't be used twice */
    dc->user_creatable = false;
}

static void csr_quatro_register_types(void)
{
    static const TypeInfo tinfo = {
        .name = TYPE_CSR_QUATRO,
        .parent = TYPE_DEVICE,
        .instance_size = sizeof(CsrQuatroState),
        .instance_init = csr_quatro_init,
        .class_init = csr_quatro_class_init,
    };

    type_register_static(&tinfo);
}

type_init(csr_quatro_register_types)
