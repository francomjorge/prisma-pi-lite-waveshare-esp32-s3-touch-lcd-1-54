#include "prisma_sensors.h"

#include <math.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define QMI8658_ADDR       0x6B
#define QMI8658_WHO_AM_I   0x00
#define QMI8658_CTRL1      0x02
#define QMI8658_CTRL2      0x03
#define QMI8658_CTRL3      0x04
#define QMI8658_CTRL7      0x08
#define QMI8658_STATUS0    0x2E
#define QMI8658_AX_L       0x35
#define QMI8658_RESET      0x60

static const char *TAG = "PRISMA_SENSORS";
static i2c_master_dev_handle_t qmi_dev;
static bool qmi_ready;

static esp_err_t reg_read(uint8_t reg, uint8_t *data, size_t len)
{
    if (!qmi_dev) return ESP_ERR_INVALID_STATE;
    return i2c_master_transmit_receive(qmi_dev, &reg, 1, data, len, pdMS_TO_TICKS(100));
}

static esp_err_t reg_write(uint8_t reg, uint8_t value)
{
    if (!qmi_dev) return ESP_ERR_INVALID_STATE;
    uint8_t b[2] = {reg, value};
    return i2c_master_transmit(qmi_dev, b, sizeof(b), pdMS_TO_TICKS(100));
}

bool prisma_sensors_init(i2c_master_bus_handle_t bus)
{
    if (qmi_ready) return true;
    if (!bus) {
        ESP_LOGW(TAG, "Shared I2C bus unavailable");
        return false;
    }

    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = QMI8658_ADDR,
        .scl_speed_hz = 400000,
    };
    esp_err_t err = i2c_master_bus_add_device(bus, &cfg, &qmi_dev);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "QMI8658 add failed: %s", esp_err_to_name(err));
        qmi_dev = NULL;
        return false;
    }

    uint8_t id = 0;
    err = reg_read(QMI8658_WHO_AM_I, &id, 1);
    if (err != ESP_OK || id != 0x05) {
        ESP_LOGW(TAG, "QMI8658 not detected (err=%s id=0x%02X)", esp_err_to_name(err), id);
        return false;
    }

    /* Same register sequence used by Waveshare's ESP-IDF 5.5.1 factory BSP. */
    if (reg_write(QMI8658_RESET, 0xB0) != ESP_OK) return false;
    vTaskDelay(pdMS_TO_TICKS(100));
    if (reg_write(QMI8658_CTRL1, 0x40) != ESP_OK) return false;
    if (reg_write(QMI8658_CTRL7, 0x03) != ESP_OK) return false;
    if (reg_write(QMI8658_CTRL2, 0x95) != ESP_OK) return false;
    if (reg_write(QMI8658_CTRL3, 0xD5) != ESP_OK) return false;

    qmi_ready = true;
    ESP_LOGI(TAG, "QMI8658 ready on shared I2C bus, id=0x%02X", id);
    return true;
}

bool prisma_sensors_imu_ready(void)
{
    return qmi_ready;
}

static int16_t le_i16(const uint8_t *p)
{
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

bool prisma_sensors_read_imu(prisma_imu_data_t *data)
{
    if (!data || !qmi_ready) return false;

    uint8_t status = 0;
    if (reg_read(QMI8658_STATUS0, &status, 1) != ESP_OK || !(status & 0x03)) return false;

    uint8_t b[12] = {0};
    if (reg_read(QMI8658_AX_L, b, sizeof(b)) != ESP_OK) return false;

    memset(data, 0, sizeof(*data));
    data->acc_x = le_i16(&b[0]);
    data->acc_y = le_i16(&b[2]);
    data->acc_z = le_i16(&b[4]);
    data->gyr_x = le_i16(&b[6]);
    data->gyr_y = le_i16(&b[8]);
    data->gyr_z = le_i16(&b[10]);

    float ax = (float)data->acc_x;
    float ay = (float)data->acc_y;
    float az = (float)data->acc_z;
    const float rad_to_deg = 57.2957795f;
    data->angle_x = atan2f(ax, sqrtf(ay * ay + az * az)) * rad_to_deg;
    data->angle_y = atan2f(ay, sqrtf(ax * ax + az * az)) * rad_to_deg;
    data->angle_z = atan2f(sqrtf(ax * ax + ay * ay), az) * rad_to_deg;
    return true;
}
