/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef ES_STORAGE_PROBE_H
#define ES_STORAGE_PROBE_H
#include <stdint.h>
int store_open(const char *path, int cache_kib);
const unsigned char *store_get(uint32_t id, uint32_t *length);
void store_close(void);
uint64_t store_reads(void);
uint64_t store_bytes(void);
uint64_t store_memory_peak(void);
const char *store_memory_label(void);
const char *store_error(void);
int store_check_readonly(void);
#endif
