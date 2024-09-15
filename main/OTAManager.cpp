#include "includes.h"

#include "OTAManager.h"

constexpr auto OTA_INITIAL_CHECK_INTERVAL = 5;
constexpr auto HASH_LENGTH = 32;  // SHA-256 hash length
constexpr auto BUFFER_SIZE = 1024;

static const char *TAG = "OTAManager";

OTAManager::OTAManager() : _updateTimer(nullptr) {}

void OTAManager::begin() {
    const esp_timer_create_args_t displayOffTimerArgs = {
        .callback = [](void *arg) { ((OTAManager *)arg)->updateCheck(); },
        .arg = this,
        .name = "updateTimer",
    };

    ESP_ERROR_CHECK(esp_timer_create(&displayOffTimerArgs, &_updateTimer));
    ESP_ERROR_CHECK(esp_timer_start_once(_updateTimer, ESP_TIMER_SECONDS(OTA_INITIAL_CHECK_INTERVAL)));
    ESP_LOGI(TAG, "Started OTA timer");
}

void OTAManager::updateCheck() {
    if (installUpdate()) {
        ESP_LOGI(TAG, "Firmware installed successfully; restarting system");

        esp_restart();
        return;
    }

    ESP_ERROR_CHECK(esp_timer_start_once(_updateTimer, ESP_TIMER_SECONDS(CONFIG_OTA_CHECK_INTERVAL)));
}

bool OTAManager::installUpdate() {
    auto firmwareInstalled = false;
    auto otaBusy = false;

    auto updatePartition = esp_ota_get_next_update_partition(nullptr);
    auto runningPartition = esp_ota_get_running_partition();

    if (!updatePartition || !runningPartition) {
        ESP_LOGE(TAG, "Failed to get partitions");
        return false;
    }

    auto buffer = new char[BUFFER_SIZE];
    auto firmwareSize = 0;
    esp_ota_handle_t updateHandle = 0;

    esp_http_client_config_t config = {
        .url = CONFIG_OTA_ENDPOINT,
        .timeout_ms = CONFIG_OTA_RECV_TIMEOUT,
    };

    ESP_LOGI(TAG, "Getting firmware from %s", config.url);

    auto client = esp_http_client_init(&config);

    ESP_ERROR_CHECK_JUMP(esp_http_client_open(client, 0), end);

    esp_http_client_fetch_headers(client);

    while (true) {
        auto read = esp_http_client_read(client, buffer, BUFFER_SIZE);

        if (read < 0) {
            ESP_LOGE(TAG, "Error while reading from HTTP stream");
            goto end;
        }

        if (read == 0) {
            // As esp_http_client_read never returns negative error code, we rely on
            // `errno` to check for underlying transport connectivity closure if any.

            if (errno == ECONNRESET || errno == ENOTCONN) {
                ESP_LOGE(TAG, "Connection closed unexpectedly, errno = %d", errno);
                goto end;
            } else if (esp_http_client_is_complete_data_received(client)) {
                ESP_LOGI(TAG, "Connection closed");
                break;
            } else {
                ESP_LOGE(TAG, "Stream not completely read");
                goto end;
            }
        }

        // If this is the first block we've read, parse the header.
        if (firmwareSize == 0) {
            if (read < sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
                ESP_LOGE(TAG, "Did not receive enough data to parse the firmware header");
                goto end;
            }

            // check current version with downloading
            esp_app_desc_t newAppInfo;
            memcpy(&newAppInfo, &buffer[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)],
                   sizeof(esp_app_desc_t));

            esp_app_desc_t runningAppInfo;
            ESP_ERROR_CHECK_JUMP(esp_ota_get_partition_description(runningPartition, &runningAppInfo), end);

            ESP_LOGI(TAG, "New firmware version: %s, current %s", newAppInfo.version, runningAppInfo.version);

            if (strcmp(newAppInfo.version, runningAppInfo.version) == 0) {
                ESP_LOGI(TAG, "Firmware already up to date.");
                goto end;
            }

            auto lastInvalidApp = esp_ota_get_last_invalid_partition();

            if (lastInvalidApp != nullptr) {
                esp_app_desc_t invalidAppInfo;
                ESP_ERROR_CHECK_JUMP(esp_ota_get_partition_description(lastInvalidApp, &invalidAppInfo), end);

                ESP_LOGI(TAG, "Last invalid firmware version: %s", invalidAppInfo.version);

                // Check current version with last invalid partition.
                if (strcmp(invalidAppInfo.version, newAppInfo.version) == 0) {
                    ESP_LOGW(TAG, "Refusing to update to invalid firmware version.");
                    goto end;
                }
            }

            ESP_ERROR_CHECK_JUMP(esp_ota_begin(updatePartition, OTA_WITH_SEQUENTIAL_WRITES, &updateHandle), end);

            otaBusy = true;

            ESP_LOGI(TAG, "Downloading new firmware");
        }

        ESP_ERROR_CHECK_JUMP(esp_ota_write(updateHandle, (const void *)buffer, read), end);

        firmwareSize += read;

        ESP_LOGI(TAG, "Written %d bytes, total %d", read, firmwareSize);
    }

    if (!esp_http_client_is_complete_data_received(client)) {
        ESP_LOGE(TAG, "Stream not fully read");
        goto end;
    }

    ESP_ERROR_CHECK_JUMP(esp_ota_end(updateHandle), end);

    otaBusy = false;

    ESP_ERROR_CHECK_JUMP(esp_ota_set_boot_partition(updatePartition), end);

    firmwareInstalled = true;

end:
    if (otaBusy) {
        esp_ota_abort(updateHandle);
    }

    delete[] buffer;

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    return firmwareInstalled;
}

bool OTAManager::parseHash(char *buffer, uint8_t *hash) {
    for (auto i = 0; i < HASH_LENGTH; i++) {
        auto h = hextoi(buffer[i * 2]);
        auto l = hextoi(buffer[i * 2 + 1]);

        if (h == -1 || l == -1) {
            return false;
        }

        hash[i] = h << 4 | l;
    }

    return true;
}
