/*
 * USB xHCI controller emulation
 *
 * Copyright (c) 2011 Securiforest
 * Date: 2011-05-11 ;  Author: Hector Martin <hector@marcansoft.com>
 * Based on usb-ohci.c, emulates Renesas NEC USB 3.0
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#include "qemu/osdep.h"
#include "hw/hw.h"
#include "trace.h"
#include "qapi/error.h"

#include "hcd-xhci.h"

static void usb_xhci_sysbus_realize(DeviceState *dev, Error **errp)
{
    XHCISysBusState *s = SYS_BUS_XHCI(dev);
    XHCIState *xhci = &s->xhci;

    if (xhci->numintrs > MAXINTRS) {
        xhci->numintrs = MAXINTRS;
    }
    while (xhci->numintrs & (xhci->numintrs - 1)) {   /* ! power of 2 */
        xhci->numintrs++;
    }
    if (xhci->numintrs < 1) {
        xhci->numintrs = 1;
    }
    if (xhci->numslots > MAXSLOTS) {
        xhci->numslots = MAXSLOTS;
    }
    if (xhci->numslots < 1) {
        xhci->numslots = 1;
    }
    if (xhci_get_flag(xhci, XHCI_FLAG_ENABLE_STREAMS)) {
        xhci->max_pstreams_mask = 7; /* == 256 primary streams */
    } else {
        xhci->max_pstreams_mask = 0;
    }

    xhci->as = &address_space_memory;
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &xhci->irq);

    usb_xhci_realize(xhci, dev, errp);
}

static void usb_xhci_sysbus_reset(DeviceState *dev)
{
    XHCISysBusState *s = SYS_BUS_XHCI(dev);
    XHCIState *xhci = &s->xhci;

    usb_xhci_reset(xhci);
}

static const VMStateDescription vmstate_xhci_sysbus = {
    .name = "xhci-sysbus",
    .version_id = 2,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_STRUCT(xhci, XHCISysBusState, 2, vmstate_xhci, XHCIState),
        VMSTATE_END_OF_LIST()
    }
};

static Property xhci_sysbus_properties[] = {
    DEFINE_PROP_BIT("streams", XHCISysBusState, xhci.flags,
                    XHCI_FLAG_ENABLE_STREAMS, true),
    DEFINE_PROP_UINT32("p2",    XHCISysBusState, xhci.numports_2, 4),
    DEFINE_PROP_UINT32("p3",    XHCISysBusState, xhci.numports_3, 4),
    DEFINE_PROP_END_OF_LIST(),
};

static void xhci_sysbus_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    XHCISysBusState *s = SYS_BUS_XHCI(obj);
    XHCISysBusClass *xsc = SYS_BUS_XHCI_GET_CLASS(obj);
    XHCIState *xhci = &s->xhci;

    xhci->numports_2 = xsc->numports_2;
    if (xhci->numports_2 > MAXPORTS_2) {
        xhci->numports_2 = MAXPORTS_2;
    }
    xhci->numports_3 = xsc->numports_3;
    if (xhci->numports_3 > MAXPORTS_3) {
        xhci->numports_3 = MAXPORTS_3;
    }

    xhci->numports = xhci->numports_2 + xhci->numports_3;
    xhci->numintrs = MAXINTRS;
    xhci->numslots = MAXSLOTS;

    usb_xhci_init(xhci, DEVICE(obj));
    sysbus_init_mmio(sbd, &xhci->mem);
}

static void xhci_sysbus_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = usb_xhci_sysbus_realize;
    dc->reset   = usb_xhci_sysbus_reset;
    dc->props   = xhci_sysbus_properties;
    dc->vmsd    = &vmstate_xhci_sysbus;
    set_bit(DEVICE_CATEGORY_USB, dc->categories);
}

static const TypeInfo xhci_sysbus_info = {
    .name          = TYPE_SYS_BUS_XHCI,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(XHCISysBusState),
    .instance_init = xhci_sysbus_init,
    .abstract      = true,
    .class_size    = sizeof(XHCISysBusClass),
    .class_init    = xhci_sysbus_class_init,
};

static void quatro5500_xhci_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    XHCISysBusClass *xsc = SYS_BUS_XHCI_CLASS(oc);

    set_bit(DEVICE_CATEGORY_USB, dc->categories);
    xsc->numports_2 = 1;
    xsc->numports_3 = 0;
}

static void quatro5500_xhci_instance_init(Object *obj)
{
    XHCISysBusState *s = SYS_BUS_XHCI(obj);
    XHCIState *xhci = &s->xhci;

    xhci->numintrs = MAXINTRS;
    xhci->numslots = MAXSLOTS;
    xhci_set_flag(xhci, XHCI_FLAG_SS_FIRST);
}

static const TypeInfo quatro5500_xhci_info = {
    .name       = TYPE_QUATRO5500_XHCI,
    .parent     = TYPE_SYS_BUS_XHCI,
    .class_init = quatro5500_xhci_class_init,
    .instance_init = quatro5500_xhci_instance_init,
};

static void xhci_sysbus_register_types(void)
{
    type_register_static(&xhci_sysbus_info);
    type_register_static(&quatro5500_xhci_info);
}

type_init(xhci_sysbus_register_types)
