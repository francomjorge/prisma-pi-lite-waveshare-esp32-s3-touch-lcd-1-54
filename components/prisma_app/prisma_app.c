#include "prisma_app.h"

#include <string.h>

#include "freertos/FreeRTOS.h"

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_initialized = false;
static prisma_synth_state_t s_state;

static const char *FILES[] = {
    "F1 MOON.JPG",
    "F2 TANGO.JPG",
};

static uint8_t clamp100(uint8_t value)
{
    return value > 100 ? 100 : value;
}

static void bump_revision(void)
{
    ++s_state.revision;
    if (s_state.revision == 0) s_state.revision = 1;
}

bool prisma_app_init(void)
{
    if (s_initialized) return true;

    portENTER_CRITICAL(&s_lock);
    memset(&s_state, 0, sizeof(s_state));
    s_state.revision = 1;
    s_state.active_page = 0;
    s_state.file_index = 0;
    strncpy(s_state.file_name, FILES[0], sizeof(s_state.file_name) - 1);
    s_state.intensity = 61;
    s_state.bypass = true;

    for (int i = 0; i < PRISMA_EFFECT_COUNT; ++i) {
        s_state.effects[i].enabled = true;
        s_state.effects[i].value = 50;
    }
    s_state.effects[PRISMA_EFFECT_MIRROR].value = 100;
    portEXIT_CRITICAL(&s_lock);

    s_initialized = true;
    return true;
}

prisma_synth_state_t prisma_app_get_state(void)
{
    prisma_synth_state_t snapshot;
    portENTER_CRITICAL(&s_lock);
    snapshot = s_state;
    portEXIT_CRITICAL(&s_lock);
    return snapshot;
}

void prisma_app_set_page(uint8_t page)
{
    if (page > 3U) return;
    portENTER_CRITICAL(&s_lock);
    if (s_state.active_page != page) {
        s_state.active_page = page;
        bump_revision();
    }
    portEXIT_CRITICAL(&s_lock);
}

void prisma_app_cycle_file(void)
{
    portENTER_CRITICAL(&s_lock);
    s_state.file_index = (uint8_t)((s_state.file_index + 1U) % 2U);
    strncpy(s_state.file_name, FILES[s_state.file_index], sizeof(s_state.file_name) - 1);
    s_state.file_name[sizeof(s_state.file_name) - 1] = '\0';
    bump_revision();
    portEXIT_CRITICAL(&s_lock);
}

void prisma_app_set_intensity(uint8_t value)
{
    portENTER_CRITICAL(&s_lock);
    value = clamp100(value);
    if (s_state.intensity != value) {
        s_state.intensity = value;
        bump_revision();
    }
    portEXIT_CRITICAL(&s_lock);
}

void prisma_app_toggle_bypass(void)
{
    portENTER_CRITICAL(&s_lock);
    s_state.bypass = !s_state.bypass;
    bump_revision();
    portEXIT_CRITICAL(&s_lock);
}

void prisma_app_set_bypass(bool bypass)
{
    portENTER_CRITICAL(&s_lock);
    if (s_state.bypass != bypass) {
        s_state.bypass = bypass;
        bump_revision();
    }
    portEXIT_CRITICAL(&s_lock);
}

void prisma_app_toggle_effect(prisma_effect_id_t effect)
{
    if (effect < 0 || effect >= PRISMA_EFFECT_COUNT) return;
    portENTER_CRITICAL(&s_lock);
    s_state.effects[effect].enabled = !s_state.effects[effect].enabled;
    bump_revision();
    portEXIT_CRITICAL(&s_lock);
}

void prisma_app_set_effect_enabled(prisma_effect_id_t effect, bool enabled)
{
    if (effect < 0 || effect >= PRISMA_EFFECT_COUNT) return;
    portENTER_CRITICAL(&s_lock);
    if (s_state.effects[effect].enabled != enabled) {
        s_state.effects[effect].enabled = enabled;
        bump_revision();
    }
    portEXIT_CRITICAL(&s_lock);
}

void prisma_app_set_effect_value(prisma_effect_id_t effect, uint8_t value)
{
    if (effect < 0 || effect >= PRISMA_EFFECT_COUNT) return;
    portENTER_CRITICAL(&s_lock);
    value = clamp100(value);
    if (s_state.effects[effect].value != value) {
        s_state.effects[effect].value = value;
        bump_revision();
    }
    portEXIT_CRITICAL(&s_lock);
}
