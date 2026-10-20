#!/usr/bin/env python3
from pathlib import Path

root=Path(__file__).resolve().parents[1]
cmake=(root/'main/CMakeLists.txt').read_text()
app=(root/'main/app_factory_v1.c').read_text()
midi=(root/'main/midi_service.c').read_text()
admin=(root/'main/admin_api_factory.c').read_text()
html=(root/'main/web/admin.html').read_text()
sdk=(root/'sdkconfig.defaults').read_text()

assert '"midi_service.c"' in cmake and ' usb' in cmake
assert '#define PAGE_COUNT 2' in app
assert 'draw_midi' in app and 'MIDI CTRL' in app
assert 'prisma_midi_init();' in app
assert 'midi_connected' in app and 'DISCONNECTED' in app
assert 'play_sound' in app and 'sound_file' in app

assert 'usb_host_install' in midi
assert 'USB_HOST_CLIENT_EVENT_NEW_DEV' in midi
assert 'USB_HOST_CLIENT_EVENT_DEV_GONE' in midi
assert 'bInterfaceClass == 0x01' in midi and 'bInterfaceSubClass == 0x03' in midi
assert 'nvs_set_u8' in midi and 'nvs_set_str' in midi
assert 'prisma_midi_set_config' in midi

assert '/api/midi/status' in admin
assert '/api/midi/config' in admin
assert 'prisma_midi_on_file_deleted' in admin
assert 'FACTORY V1.1 MIDI' in admin

assert 'data-tab="midi"' in html
assert 'Play sound on MIDI event' in html
assert 'soundFile' in html
assert "filter(f=>/\\.wav$/i.test(f.name))" in html
assert 'setInterval' in html

assert 'CONFIG_ESP_CONSOLE_USB_CDC=n' in sdk
assert 'CONFIG_ESP_CONSOLE_UART_DEFAULT=y' in sdk

print('Factory V1.1 OK: 2 screens + live USB MIDI status + persistent sound selection')
