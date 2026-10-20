#pragma once

#include <stdbool.h>

#define PRISMA_STORAGE_SLOT_COUNT 4

void prisma_storage_init(void);
int prisma_storage_count(void);
const char *prisma_storage_current(void);
const char *prisma_storage_next(void);

/*
 * Stable four-slot logical registry used by the appliance/web diagnostics.
 * V1 fills it from the boot-time media scan. Empty slots remain reserved for
 * later image/sample assets without changing the filesystem contract.
 */
int prisma_storage_slot_count(void);
bool prisma_storage_slot_present(int slot);
const char *prisma_storage_slot_name(int slot);
