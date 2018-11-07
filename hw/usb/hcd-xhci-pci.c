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
#include "hw/pci/pci.h"
#include "hw/pci/msi.h"
#include "hw/pci/msix.h"
#include "trace.h"
#include "qapi/error.h"

#include "hcd-xhci.h"

#define OFF_MSIX_TABLE  0x3000
#define OFF_MSIX_PBA    0x3800

static void usb_xhci_pci_reset(DeviceState *dev)
{
    XHCIPCIState *s = PCI_XHCI(dev);
    XHCIState *xhci = &s->xhci;

    usb_xhci_reset(xhci);
}

static void usb_xhci_pci_realize(struct PCIDevice *dev, Error **errp)
{
    XHCIPCIState *s = PCI_XHCI(dev);
    XHCIState *xhci = &s->xhci;
    Error *err = NULL;
    int ret;

    dev->config[PCI_CLASS_PROG] = 0x30;    /* xHCI */
    dev->config[PCI_INTERRUPT_PIN] = 0x01; /* interrupt pin 1 */
    dev->config[PCI_CACHE_LINE_SIZE] = 0x10;
    dev->config[0x60] = 0x30; /* release number */

    if (strcmp(object_get_typename(OBJECT(dev)), TYPE_NEC_XHCI) == 0) {
        xhci->nec_quirks = true;
    }
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

    if (s->msi != ON_OFF_AUTO_OFF) {
        ret = msi_init(dev, 0x70, xhci->numintrs, true, false, &err);
        /* Any error other than -ENOTSUP(board's MSI support is broken)
         * is a programming error */
        assert(!ret || ret == -ENOTSUP);
        if (ret && s->msi == ON_OFF_AUTO_ON) {
            /* Can't satisfy user's explicit msi=on request, fail */
            error_append_hint(&err, "You have to use msi=auto (default) or "
                    "msi=off with this machine type.\n");
            error_propagate(errp, err);
            return;
        }
        assert(!err || s->msi == ON_OFF_AUTO_AUTO);
        /* With msi=auto, we fall back to MSI off silently */
        error_free(err);
    }

    xhci->as = pci_get_address_space(dev);
    xhci->irq = pci_allocate_irq(dev);
    xhci->pci = dev;

    usb_xhci_realize(xhci, DEVICE(dev), NULL);

    pci_register_bar(dev, 0,
                     PCI_BASE_ADDRESS_SPACE_MEMORY | PCI_BASE_ADDRESS_MEM_TYPE_64,
                     &xhci->mem);

    if (pci_bus_is_express(pci_get_bus(dev)) ||
        xhci_get_flag(xhci, XHCI_FLAG_FORCE_PCIE_ENDCAP)) {
        ret = pcie_endpoint_cap_init(dev, 0xa0);
        assert(ret > 0);
    }

    if (s->msix != ON_OFF_AUTO_OFF) {
        /* TODO check for errors, and should fail when msix=on */
        msix_init(dev, xhci->numintrs,
                  &xhci->mem, 0, OFF_MSIX_TABLE,
                  &xhci->mem, 0, OFF_MSIX_PBA,
                  0x90, NULL);
    }
}

static void usb_xhci_pci_exit(PCIDevice *dev)
{
    XHCIPCIState *s = PCI_XHCI(dev);
    XHCIState *xhci = &s->xhci;

    usb_xhci_unrealize(xhci, DEVICE(dev), NULL);

    /* destroy msix memory region */
    if (dev->msix_table && dev->msix_pba
        && dev->msix_entry_used) {
        msix_uninit(dev, &xhci->mem, &xhci->mem);
    }
}

static int xhci_pci_post_load(void *opaque, int version_id)
{
    XHCIPCIState *s = PCI_XHCI(opaque);
    XHCIState *xhci = &s->xhci;
    PCIDevice *pci_dev = PCI_DEVICE(xhci);

    for (int intr = 0; intr < xhci->numintrs; intr++) {
        if (xhci->intr[intr].msix_used) {
            msix_vector_use(pci_dev, intr);
        } else {
            msix_vector_unuse(pci_dev, intr);
        }
    }

    return 0;
}

static const VMStateDescription vmstate_xhci_pci = {
    .name = "xhci",
    .version_id = 2,
    .minimum_version_id = 1,
    .post_load = xhci_pci_post_load,
    .fields = (VMStateField[]) {
        VMSTATE_PCI_DEVICE(pci_dev, XHCIPCIState),
        VMSTATE_MSIX(pci_dev, XHCIPCIState),
        VMSTATE_STRUCT(xhci, XHCIPCIState, 2, vmstate_xhci, XHCIState),
        VMSTATE_END_OF_LIST()
    }
};

static Property xhci_pci_properties[] = {
    DEFINE_PROP_BIT("streams", XHCIPCIState, xhci.flags,
                    XHCI_FLAG_ENABLE_STREAMS, true),
    DEFINE_PROP_UINT32("p2",    XHCIPCIState, xhci.numports_2, 4),
    DEFINE_PROP_UINT32("p3",    XHCIPCIState, xhci.numports_3, 4),
    DEFINE_PROP_END_OF_LIST(),
};

static void xhci_pci_init(Object *obj)
{
    XHCIPCIState *s = PCI_XHCI(obj);
    XHCIState *xhci = &s->xhci;

    /* QEMU_PCI_CAP_EXPRESS initialization does not depend on QEMU command
     * line, therefore, no need to wait to realize like other devices */
    PCI_DEVICE(obj)->cap_present |= QEMU_PCI_CAP_EXPRESS;

    if (xhci->numports_2 > MAXPORTS_2) {
        xhci->numports_2 = MAXPORTS_2;
    }
    if (xhci->numports_3 > MAXPORTS_3) {
        xhci->numports_3 = MAXPORTS_3;
    }

    xhci->numports = xhci->numports_2 + xhci->numports_3;

    usb_xhci_init(xhci, DEVICE(obj));
}

static void xhci_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    dc->vmsd    = &vmstate_xhci_pci;
    dc->props   = xhci_pci_properties;
    dc->reset   = usb_xhci_pci_reset;
    set_bit(DEVICE_CATEGORY_USB, dc->categories);
    k->realize      = usb_xhci_pci_realize;
    k->exit         = usb_xhci_pci_exit;
    k->class_id     = PCI_CLASS_SERIAL_USB;
}

static const TypeInfo xhci_pci_info = {
    .name          = TYPE_PCI_XHCI,
    .parent        = TYPE_PCI_DEVICE,
    .class_init    = xhci_class_init,
    .instance_size = sizeof(XHCIPCIState),
    .instance_init = xhci_pci_init,
    .abstract      = true,
    .interfaces = (InterfaceInfo[]) {
        { INTERFACE_PCIE_DEVICE },
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { }
    },
};

static void qemu_xhci_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->vendor_id    = PCI_VENDOR_ID_REDHAT;
    k->device_id    = PCI_DEVICE_ID_REDHAT_XHCI;
    k->revision     = 0x01;
}

static void qemu_xhci_instance_init(Object *obj)
{
    XHCIPCIState *s = PCI_XHCI(obj);
    XHCIState *xhci = &s->xhci;

    s->msi      = ON_OFF_AUTO_OFF;
    s->msix     = ON_OFF_AUTO_AUTO;
    xhci->numintrs = MAXINTRS;
    xhci->numslots = MAXSLOTS;
    xhci_set_flag(xhci, XHCI_FLAG_SS_FIRST);
}

static const TypeInfo qemu_xhci_info = {
    .name          = TYPE_QEMU_XHCI,
    .parent        = TYPE_PCI_XHCI,
    .class_init    = qemu_xhci_class_init,
    .instance_init = qemu_xhci_instance_init,
};

static void xhci_pci_register_types(void)
{
    type_register_static(&xhci_pci_info);
    type_register_static(&qemu_xhci_info);
}

type_init(xhci_pci_register_types)
