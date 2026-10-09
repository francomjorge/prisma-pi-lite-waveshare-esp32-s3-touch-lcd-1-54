#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
power = (root / "main/power_button.c").read_text(encoding="utf-8")
wrapper = (root / "main/app36.c").read_text(encoding="utf-8")
cmake = (root / "main/CMakeLists.txt").read_text(encoding="utf-8")
root_cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
version = (root / "VERSION_1.0.txt").read_text(encoding="utf-8")

# Waveshare schematic: KEY_PWR=GPIO5, BAT_EN=GPIO2. KEY_PWR is active LOW.
assert "#define POWER_KEY_PIN GPIO_NUM_5" in power
assert "#define BAT_EN_PIN GPIO_NUM_2" in power
assert "#define LCD_BL_PIN GPIO_NUM_46" in power
assert "gpio_get_level(POWER_KEY_PIN) == 0" in power

# Gesture contract.
assert "#define POWER_REBOOT_MS 2000" in power
assert "#define POWER_OFF_MS 3000" in power
assert "#define POWER_BOOT_HOLD_MS 3000" in power
assert "esp_restart();" in power
assert "battery_latch_set(false);" in power
assert "battery_latch_set(true);" in power
assert "esp_sleep_enable_ext0_wakeup(POWER_KEY_PIN, 0)" in power

# Startup gate must run before the normal appliance boot path.
prepare = wrapper.index("prisma_power_prepare_boot();")
storage = wrapper.index("prisma_storage_init();")
monitor = wrapper.index("prisma_power_start_monitor();")
legacy = wrapper.index("prisma_legacy_main();")
assert prepare < storage < monitor < legacy
assert '"power_button.c"' in cmake

# Release identity.
assert 'set(PROJECT_VER "1.0.0")' in root_cmake
assert "PRISMA-PI LITE 1.0" in version
assert "STABLE" in version

print("v1.0 OK: stable baseline + GPIO5 power gestures")
