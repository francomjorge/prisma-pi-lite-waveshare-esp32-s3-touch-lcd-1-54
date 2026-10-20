#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define PRISMA_SECURITY_USERNAME "admin"
#define PRISMA_SECURITY_SESSION_HEX 64

typedef struct {
    bool password_set;
    bool recovery_available;
    uint32_t session_generation;
    uint8_t active_sessions;
} prisma_security_status_t;

esp_err_t prisma_security_init(void);
void prisma_security_get_status(prisma_security_status_t *out);
bool prisma_security_verify_password(const char *password);
bool prisma_security_set_password(const char *current_password, const char *new_password);
bool prisma_security_remove_password(const char *current_password);
void prisma_security_force_clear_password(void);
void prisma_security_invalidate_sessions(void);
bool prisma_security_create_session(const char *password, char out_token[PRISMA_SECURITY_SESSION_HEX + 1]);
bool prisma_security_validate_session(const char *token);
void prisma_security_logout_token(const char *token);
