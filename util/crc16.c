#include "qemu/osdep.h"
#include "qemu/crc16.h"

uint16_t crc16(const uint8_t *data, unsigned int length)
{
    int count = length, crc, i;
    crc = 0;
    while (--count >= 0) {
        crc = crc ^ (int)*data++ << 8;
        for (i = 0; i < 8; ++i) {
            if (crc & 0x8000) {
                crc = crc << 1 ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    return (crc & 0xFFFF);
}
