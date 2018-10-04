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
#include "hw/usb/hcd-ehci.h"
#include "exec/address-spaces.h"
#include "sysemu/kvm.h"
#include "kvm_arm.h"
#include "qemu/log.h"

//#define CM3_ENABLE

#if defined(CM3_ENABLE)
static void csr_quatro_create_cm3x(CsrQuatroState *ms)
{
    for (int i = 0; i < CSR_QUATRO_NUM_MP_CPUS; ++i) {
        object_initialize_child(OBJECT(ms), "mp-cpu[*]",
                                &ms->mp_cpus[i], sizeof(ms->mp_cpus[0]),
                                ARM_CPU_TYPE_NAME("cortex-m3"),
                                &error_abort, NULL);
        char *name = object_get_canonical_path_component(OBJECT(&ms->mp_cpus[i]));
        qemu_log("cm3x: name: '%s' (%p), nvic: %p, mr: %p\n",
                 name, &ms->mp_cpus[i], &ms->nvics[i], &ms->cm3mem[i]);
        g_free(name);

        ms->mp_cpus[i].env.nvic = &ms->nvics[i];
        ms->nvics[i].cpu = &ms->mp_cpus[i];
        object_property_set_bool(OBJECT(&ms->mp_cpus[i]),
                                 true, "realized", &error_abort);
        object_property_set_bool(OBJECT(&ms->nvics[i]),
                                 true, "realized", &error_abort);

        memory_region_init(&ms->cm3mem[i], OBJECT(&ms->mp_cpus[i]), "cm3mem[*]", UINT64_MAX);
        SysBusDevice *sbd = SYS_BUS_DEVICE(&ms->nvics[i]);
        sysbus_connect_irq(sbd, 0,
                           qdev_get_gpio_in(DEVICE(&ms->mp_cpus[i]), ARM_CPU_IRQ));

        memory_region_add_subregion(&ms->cm3mem[i], 0xe000e000,
                                    sysbus_mmio_get_region(sbd, 0));
    }
}
#endif

static void csr_quatro_init(Object *obj)
{
    CsrQuatroState *ms = CSR_QUATRO(obj);
    int num_aps = MIN(smp_cpus - CSR_QUATRO_NUM_MP_CPUS, CSR_QUATRO_NUM_AP_CPUS);

    object_initialize_child(obj, "ap-cpu[*]",
                            &ms->ap_cpus[0], sizeof(ms->ap_cpus[0]),
                            ARM_CPU_TYPE_NAME("cortex-a7"),
                            &error_abort, NULL);
    for (int i = 1; i < num_aps; ++i) {
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

#if defined(CM3_ENABLE)
    for (int i = 0; i < CSR_QUATRO_NUM_MP_CPUS; ++i) {
        sysbus_init_child_obj(obj, "nvic[*]",
                              &ms->nvics[i], sizeof(ms->nvics[0]),
                              TYPE_NVIC);
    }
#endif
}

static void csr_quatro_realize(DeviceState *dev, Error **errp)
{
    CsrQuatroState *ms = CSR_QUATRO(dev);
    int num_aps = MIN(smp_cpus - CSR_QUATRO_NUM_MP_CPUS, CSR_QUATRO_NUM_AP_CPUS);

    for (int i = 0; i < num_aps; ++i) {
        Object *cpu = OBJECT(&ms->ap_cpus[i]);

        /* On uniprocessor, the CBAR is set to 0 */
        if (num_aps > 1) {
            object_property_set_int(cpu, CSR_QUATRO_A7MPCORE_ADDR,
                                    "reset-cbar", &error_abort);
        }

        object_property_set_bool(cpu, true, "realized", &error_abort);
    }

    /* Cortex-A7 MP Core.
     */
    SysBusDevice *sbd = SYS_BUS_DEVICE(&ms->a7mpcore);
    object_property_set_int(OBJECT(&ms->a7mpcore),
                            num_aps, "num-cpu",
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
    for (int i = 0; i < num_aps; ++i) {
        DeviceState *cpu = DEVICE(qemu_get_cpu(i));

        sysbus_connect_irq(sbd, i,
                           qdev_get_gpio_in(cpu, ARM_CPU_IRQ));
        sysbus_connect_irq(sbd, i + num_aps,
                           qdev_get_gpio_in(cpu, ARM_CPU_FIQ));
    }

#if defined(CM3_ENABLE)
    csr_quatro_create_cm3x(ms);
#endif
    sysbus_create_simple("quatro5500.rstgen", CSR_QUATRO_RSTGEN_ADDR, NULL);
    sysbus_create_simple("quatro5500.clk", CSR_QUATRO_CLK_ADDR, NULL);
    sysbus_create_simple("quatro5500.rtc", CSR_QUATRO_RTC_ADDR, NULL);
    sysbus_create_simple("quatro5500.hrt0", CSR_QUATRO_HRT0_ADDR, NULL);
    sysbus_create_simple("quatro5500.sdmclk", CSR_QUATRO_SDMCLK_ADDR, NULL);
    sysbus_create_simple("quatro5500.fcspi", CSR_QUATRO_FCSPI_ADDR, NULL);
    sysbus_create_simple("quatro5500.ddrmc", CSR_QUATRO_DDRMC_ADDR, NULL);
    sysbus_create_simple("quatro5500.a15gpf", CSR_QUATRO_A15GPF_ADDR, NULL);
    sysbus_create_simple("quatro5500.sdiocore", CSR_QUATRO_SDIO0_ADDR, NULL);
    sysbus_create_simple("quatro5500.sdiocore", CSR_QUATRO_SDIO1_ADDR, NULL);
    sysbus_create_simple(TYPE_QUATRO5500_EHCI, CSR_QUATRO_USBH_ADDR, NULL);
    sysbus_create_simple("stmmaceth", CSR_QUATRO_ETHERNET_ADDR, NULL);
    sysbus_create_simple(TYPE_SYSBUS_AHCI, CSR_QUATRO_SATA_ADDR, NULL);

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
