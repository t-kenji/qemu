/*
 *  Generic Register Definition.
 *
 *  Copyright (C) 2019 t-kenji <protect.2501@gmail.com>
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
 */
#ifndef __GEN_REG_H__
#define __GEN_REG_H__

typedef struct {
    const char *name;
    int index;
    hwaddr offset;
    uint8_t reset_value;
    uint8_t write_mask;
} RegDef8;

typedef struct {
    const char *name;
    int index;
    hwaddr offset;
    uint16_t reset_value;
    uint16_t write_mask;
} RegDef16;

typedef struct {
    const char *name;
    int index;
    hwaddr offset;
    uint32_t reset_value;
    uint32_t write_mask;
} RegDef32;

typedef struct {
    const char *name;
    int index;
    hwaddr offset;
    uint64_t reset_value;
    uint64_t write_mask;
} RegDef64;

#define REG_DEF(n, i, o, r, m) \
    {.name=(n), .index=(i), .offset=(o), .reset_value=(r), .write_mask=(m)}

#define REG_ITEM(i, o, r, m) \
    [i] = REG_DEF(#i, i, o, r, m)


static inline RegDef8 internal_reg8_find(const RegDef8 *regs,
                                         size_t length,
                                         hwaddr offset)
{
    static RegDef8 nullreg = REG_DEF("null", -1, -1, 0, 0);

    for (size_t i = 0; i < length; ++i) {
        if (regs[i].offset == offset) {
            return regs[i];
        }
    }
    return nullreg;
}

static inline RegDef16 internal_reg16_find(const RegDef16 *regs,
                                           size_t length,
                                           hwaddr offset)
{
    static RegDef16 nullreg = REG_DEF("null", -1, -1, 0, 0);

    for (size_t i = 0; i < length; ++i) {
        if (regs[i].offset == offset) {
            return regs[i];
        }
    }
    return nullreg;
}

static inline RegDef32 internal_reg32_find(const RegDef32 *regs,
                                           size_t length,
                                           hwaddr offset)
{
    static RegDef32 nullreg = REG_DEF("null", -1, -1, 0, 0);

    for (size_t i = 0; i < length; ++i) {
        if (regs[i].offset == offset) {
            return regs[i];
        }
    }
    return nullreg;
}

static inline RegDef64 internal_reg64_find(const RegDef64 *regs,
                                           size_t length,
                                           hwaddr offset)
{
    static RegDef64 nullreg = REG_DEF("null", -1, -1, 0, 0);

    for (size_t i = 0; i < length; ++i) {
        if (regs[i].offset == offset) {
            return regs[i];
        }
    }
    return nullreg;
}

#define regdef_find(r, o) \
    _Generic((r), \
             const RegDef8 *: internal_reg8_find, \
             const RegDef16 *: internal_reg16_find, \
             const RegDef32 *: internal_reg32_find, \
             const RegDef64 *: internal_reg64_find \
    )((r), ARRAY_SIZE(r), (o))

#endif /* __GEN_REG_H__ */
