#pragma once

class OTAManager {
    esp_timer_handle_t _update_timer;
    Callback<void> _ota_start;

public:
    OTAManager();

    void begin();
    void on_ota_start(function<void()> func) { _ota_start.add(func); }

private:
    void update_check();
    bool install_update();
    bool parse_hash(char* buffer, uint8_t* hash);
};
