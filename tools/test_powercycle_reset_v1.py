#!/usr/bin/env python3
from pathlib import Path

root=Path(__file__).resolve().parents[1]
app=(root/'main/app_checkpoint_a.c').read_text(encoding='utf-8')

assert 'static void cp_reset_performance(void)' in app
reset=app.split('static void cp_reset_performance(void)',1)[1].split('bool prisma_appliance_get_on',1)[0]

# Power-up reset target: BYPASS, intensity 0, static/motion FX cleared.
assert 'prisma_shared_mode=1;' in reset
assert 'prisma_shared_intensity=0;' in reset
assert 'intensity_button=0;' in reset
assert 'intensity_level=0;' in reset
assert 'for(int i=0;i<5;i++)' in reset
assert 'effect_enabled[i]=false;' in reset
assert 'effect_value[i]=0;' in reset
assert 'prisma_control_set_effect(i,false);' in reset
assert 'effect_enabled[5]=true;' in reset
assert 'effect_value[5]=0;' in reset
assert 'prisma_control_set_effect(5,true);' in reset
assert 'for(int i=0;i<MOTION_COUNT;i++)' in reset
assert 'motion_enabled[i]=false;' in reset
assert 'motion_value[i]=0;' in reset

# Reset happens only on OFF -> ON from the appliance power button.
power_touch=app.split('else if(page==1&&startx>=170&&startx<=232&&starty>=5&&starty<=42)',1)[1].split('else if(page==1&&startx>=12&&startx<=165',1)[0]
assert 'bool was_on=prisma_shared_on;' in power_touch
assert 'toggle_power();' in power_touch
assert 'if(!was_on&&prisma_shared_on)cp_reset_performance();' in power_touch

# BYPASS/RANDOM remains a pure performance-mode toggle; it must not reset FX.
mode_touch=app.split('else if(page==1&&prisma_shared_on&&startx>=82&&startx<=232&&starty>=139&&starty<=181)',1)[1].split('else if(page==3',1)[0]
assert 'prisma_shared_mode=prisma_shared_mode?0:1;' in mode_touch
assert 'cp_reset_performance' not in mode_touch

# Source selection is preserved and independent from the reset.
assert 'prisma_storage_next()' in app
assert 'prisma_control_set_filename(src);' in app
assert 'prisma_storage_next()' not in reset
assert 'prisma_control_set_filename' not in reset

# Pixel-tuned preset table from the parent branch must remain intact.
for token in (
    '/* 1G */ {{false,false,false,true,false}, { 0, 6,15,12,50,45}',
    '/* 2Y */ {{true,true,true,true,false},    {22,33,42,52,50,62}',
    '/* 2R */ {{true,true,true,true,true},     {25,43,50,64,50,68}',
    '/* 3G */ {{false,true,true,true,true},    { 0,54,58,74,50,75}',
    '/* 3Y */ {{true,true,true,true,true},     {53,68,68,85,50,84}',
    '/* 3R */ {{true,true,true,true,true},     {62,86,80,96,50,95}',
):
    assert token in app

print('Power-cycle reset V1 protected: OFF->ON resets performance, BYPASS/RANDOM does not, source preserved')
