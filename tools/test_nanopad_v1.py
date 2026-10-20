#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
app = (root / 'main/app_checkpoint_a.c').read_text(encoding='utf-8')
power = (root / 'main/prisma_power.c').read_text(encoding='utf-8')
storage = (root / 'main/storage.c').read_text(encoding='utf-8')
storage_h = (root / 'main/storage.h').read_text(encoding='utf-8')
portal = (root / 'main/wifi_portal_checkpoint_a.c').read_text(encoding='utf-8')
cmake = (root / 'main/CMakeLists.txt').read_text(encoding='utf-8')
partitions = (root / 'partitions.csv').read_text(encoding='utf-8')

# V1 appliance: home only, but generic page/swipe router remains.
assert '#define CP_PAGE_COUNT 1' in app
assert 'const int total_w = CP_PAGE_COUNT * dot_w' in app
assert 'page<CP_PAGE_COUNT-1' in app
assert 'page>0' in app
assert 'cp_page_intro' in app
assert 'glitch(startx,starty)' in app
assert 'home_cooldown=HOME_COOLDOWN_TICKS' in app

# Power contract: 5 s OFF, 3 s ON, 2 s + release restart.
assert '#define RESTART_HOLD_MS 2000' in power
assert '#define POWER_ON_HOLD_MS 3000' in power
assert '#define POWER_OFF_HOLD_MS 5000' in power
assert 'held_ms(started) >= POWER_OFF_HOLD_MS' in power
assert 'duration >= RESTART_HOLD_MS && duration < POWER_OFF_HOLD_MS' in power
assert 'PWR 5 s -> POWER OFF' in power

# Four logical media slots over the proven SPIFFS filesystem.
assert '#define PRISMA_STORAGE_SLOT_COUNT 4' in storage_h
assert 'media_slots[PRISMA_STORAGE_SLOT_COUNT]' in storage
assert 'build_media_slots();' in storage
assert 'prisma_storage_slot_present' in storage
assert 'prisma_storage_slot_name' in storage
assert 'storage,  data, spiffs' in partitions
assert '0x3E0000' in partitions

# Existing web/admin infrastructure remains live in V1.
assert 'prisma_wifi_start();' in app
assert 'prisma_admin_register' in portal
assert '"/api/state"' in portal
assert '"/source/*"' in portal
assert '"app_checkpoint_a.c"' in cmake
assert '"wifi_portal_checkpoint_a.c"' in cmake

print('nanoPAD V1 protected: home UI + swipe router + 5s power-off + 4 media slots + Wi-Fi/admin')
