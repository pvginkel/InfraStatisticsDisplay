#include "includes.h"

#include "DeviceConfiguration.h"

static const char* TAG = "DeviceConfiguration";

DeviceConfiguration::DeviceConfiguration() : _enableOTA(DEFAULT_ENABLE_OTA) {
    uint8_t mac[6];

    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));

    auto formattedMac = format("%02x-%02x-%02x-%02x-%02x-%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    _endpoint = format(CONFIG_DEVICE_CONFIG_ENDPOINT, formattedMac.c_str());
}

esp_err_t DeviceConfiguration::load() {
    auto err = ESP_OK;
    cJSON* root = nullptr;
    cJSON* deviceNameItem = nullptr;
    cJSON* deviceEntityIdItem = nullptr;
    cJSON* enableOTAItem = nullptr;

    esp_http_client_config_t config = {
        .url = getEndpoint().c_str(),
        .timeout_ms = CONFIG_OTA_RECV_TIMEOUT,
    };

    ESP_LOGI(TAG, "Getting device configuration from %s", config.url);

    string json;
    err = esp_http_download_string(config, json, 128 * 1024);
    if (err != ESP_OK) {
        goto end;
    }

    root = cJSON_Parse(json.c_str());
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        err = ESP_ERR_INVALID_ARG;
        goto end;
    }

    deviceNameItem = cJSON_GetObjectItemCaseSensitive(root, "deviceName");
    if (!cJSON_IsString(deviceNameItem) || !deviceNameItem->valuestring) {
        ESP_LOGE(TAG, "Cannot get deviceName property");
        err = ESP_ERR_INVALID_ARG;
        goto end;
    }

    _deviceName = deviceNameItem->valuestring;

    ESP_LOGI(TAG, "Device name: %s", _deviceName.c_str());

    deviceEntityIdItem = cJSON_GetObjectItemCaseSensitive(root, "deviceEntityId");
    if (!cJSON_IsString(deviceEntityIdItem) || !deviceEntityIdItem->valuestring) {
        ESP_LOGE(TAG, "Cannot get deviceEntityIdItem property");
        err = ESP_ERR_INVALID_ARG;
        goto end;
    }

    _deviceEntityId = deviceEntityIdItem->valuestring;

    enableOTAItem = cJSON_GetObjectItemCaseSensitive(root, "enableOTA");
    if (enableOTAItem != nullptr) {
        if (!cJSON_IsBool(enableOTAItem)) {
            ESP_LOGE(TAG, "Cannot get enableOTAItem property");
            err = ESP_ERR_INVALID_ARG;
            goto end;
        }

        _enableOTA = cJSON_IsTrue(enableOTAItem);
    }

    ESP_LOGI(TAG, "Device entity ID: %s", _deviceEntityId.c_str());

end:
    if (root) {
        cJSON_Delete(root);
    }

    return err;
}