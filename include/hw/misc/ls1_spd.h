/*
 * QorIQ LS1046A Serial Presence Detect EEPROM for DDR
 *
 * Copyright (C) 2017 t-kenji <protect.2501@gmail.com>
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

#ifndef LS1_SPD_H
#define LS1_SPD_H

#include "hw/i2c/i2c.h"

void ls1_spd_init(I2CBus *smbus);

#endif /* LS1_SPD_H */
