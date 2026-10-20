#include "storage.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_log.h"
#include "esp_spiffs.h"

#define PRISMA_MAX_FILES 4

static const char *TAG = "PRISMA_STORAGE";
static bool mounted = false;

static bool is_user_file_name(const char *name)
{
    return name && name[0] && name[0] != '.' && strcmp(name, "upload.tmp") != 0;
}

void prisma_storage_init(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/storage",
        .partition_label = "storage",
        .max_files = 8,
        .format_if_mount_failed = false,
    };

    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(err));
        mounted = false;
        return;
    }

    mounted = true;
    remove("/storage/upload.tmp");

    size_t total = 0, used = 0;
    if (esp_spiffs_info("storage", &total, &used) == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS mounted: total=%u used=%u files=%d/%d",
                 (unsigned)total, (unsigned)used, prisma_storage_file_count(), PRISMA_MAX_FILES);
    }
}

bool prisma_storage_ready(void)
{
    return mounted;
}

bool prisma_storage_info(size_t *total, size_t *used)
{
    if (!mounted) return false;
    size_t t = 0, u = 0;
    if (esp_spiffs_info("storage", &t, &u) != ESP_OK) return false;
    if (total) *total = t;
    if (used) *used = u;
    return true;
}

int prisma_storage_file_count(void)
{
    if (!mounted) return 0;
    DIR *dir = opendir("/storage");
    if (!dir) return 0;
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!is_user_file_name(entry->d_name)) continue;
        char path[96];
        if (snprintf(path, sizeof(path), "/storage/%s", entry->d_name) >= (int)sizeof(path)) continue;
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) count++;
    }
    closedir(dir);
    return count;
}

int prisma_storage_max_files(void)
{
    return PRISMA_MAX_FILES;
}
