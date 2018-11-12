/*
 * CSR Quatro 5530 Evaluation board emulation
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

#include "hw/arm/csr-quatro.h"

typedef struct {
    CsrQuatroState soc;
    MemoryRegion ram;
} CsrQuatro5530;

void quatro5530_sdhci_init(CsrQuatroState *s, int port_num);
void quatro5530_fcspi_init(CsrQuatroState *s);
void quatro5530_stmmac_init(CsrQuatroState *s);
