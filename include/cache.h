#ifndef _CACHE_H
#define _CACHE_H

#include <stdint.h>
#include "support.h"

enum CacheType
{
    ICACHE,
    DCACHE
};

void cache_setup();
void cache_invalidate_all(enum CacheType cache);
void cache_invalidate_line(enum CacheType type, uint32_t address);
uint8_t cache_read_8(enum CacheType type, uint32_t address);
uint16_t cache_read_16(enum CacheType type, uint32_t address);
uint32_t cache_read_32(enum CacheType type, uint32_t address);
uint64_t cache_read_64(enum CacheType type, uint32_t address);
uint128_t cache_read_128(enum CacheType type, uint32_t address);

int cache_write_8(enum CacheType type, uint32_t address, uint8_t data, uint8_t write_back);
int cache_write_16(enum CacheType type, uint32_t address, uint16_t data, uint8_t write_back);
int cache_write_32(enum CacheType type, uint32_t address, uint32_t data, uint8_t write_back);
int cache_write_64(enum CacheType type, uint32_t address, uint64_t data, uint8_t write_back);
int cache_write_128(enum CacheType type, uint32_t address, uint128_t data, uint8_t write_back);

#endif /* CACHE_H */