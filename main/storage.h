#pragma once

#include <stdbool.h>
#include <stddef.h>

void prisma_storage_init(void);
bool prisma_storage_ready(void);
bool prisma_storage_info(size_t *total, size_t *used);
int prisma_storage_file_count(void);
int prisma_storage_max_files(void);
