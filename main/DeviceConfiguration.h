#pragma once

class DeviceConfiguration {
private:
    static constexpr auto DEFAULT_ENABLE_OTA = true;

    string _device_name;
    string _device_entity_id;
    string _endpoint;
    bool _enable_ota;

public:
    DeviceConfiguration();
    DeviceConfiguration(const DeviceConfiguration&) = delete;
    DeviceConfiguration& operator=(const DeviceConfiguration&) = delete;
    DeviceConfiguration(DeviceConfiguration&&) = delete;
    DeviceConfiguration& operator=(DeviceConfiguration&&) = delete;

    esp_err_t load();

    const string& get_endpoint() const { return _endpoint; }
    const string& get_device_name() const { return _device_name; }
    const string& get_device_entity_id() const { return _device_entity_id; }
    bool get_enable_ota() const { return _enable_ota; }
};
