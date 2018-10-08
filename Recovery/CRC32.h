#pragma once

#include <inttypes.h>

extern void crc32init();
extern void crc32buf(uint8_t *buf, size_t len);
extern uint32_t crc32finish();
