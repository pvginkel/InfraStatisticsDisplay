#include "includes.h"

#include "DeviceConfiguration.h"

static const char* TAG = "DeviceConfiguration";

DeviceConfiguration::DeviceConfiguration() : _enable_ota(DEFAULT_ENABLE_OTA) {
    uint8_t mac[6];

    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));

    auto formattedMac = format("%02x-%02x-%02x-%02x-%02x-%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    _endpoint = format(CONFIG_DEVICE_CONFIG_ENDPOINT, formattedMac.c_str());
}

esp_err_t DeviceConfiguration::load() {
    esp_http_client_config_t config = {
        .url = get_endpoint().c_str(),
        .timeout_ms = CONFIG_OTA_RECV_TIMEOUT,
    };

    ESP_LOGI(TAG, "Getting device configuration from %s", config.url);

    string json;
    auto err = esp_http_download_string(config, json, 128 * 1024);
    if (err != ESP_OK) {
        return err;
    }

    cJSON_Data data = {cJSON_Parse(json.c_str())};
    if (!*data) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        err = ESP_ERR_INVALID_ARG;
        return err;
    }

    auto deviceNameItem = cJSON_GetObjectItemCaseSensitive(*data, "deviceName");
    if (!cJSON_IsString(deviceNameItem) || !deviceNameItem->valuestring) {
        ESP_LOGE(TAG, "Cannot get deviceName property");
        return ESP_ERR_INVALID_ARG;
    }

    _device_name = deviceNameItem->valuestring;

    ESP_LOGI(TAG, "Device name: %s", _device_name.c_str());

    auto deviceEntityIdItem = cJSON_GetObjectItemCaseSensitive(*data, "deviceEntityId");
    if (!cJSON_IsString(deviceEntityIdItem) || !deviceEntityIdItem->valuestring) {
        ESP_LOGE(TAG, "Cannot get deviceEntityIdItem property");
        return ESP_ERR_INVALID_ARG;
    }

    _device_entity_id = deviceEntityIdItem->valuestring;

    ESP_LOGI(TAG, "Device entity ID: %s", _device_entity_id.c_str());

    auto enableOTAItem = cJSON_GetObjectItemCaseSensitive(*data, "enableOTA");
    if (enableOTAItem != nullptr) {
        if (!cJSON_IsBool(enableOTAItem)) {
            ESP_LOGE(TAG, "Cannot get enableOTAItem property");
            return ESP_ERR_INVALID_ARG;
        }

        _enable_ota = cJSON_IsTrue(enableOTAItem);
    }

    ESP_LOGI(TAG, "Enable OTA: %s", _enable_ota ? "yes" : "no");

    return ERR_OK;
}