#pragma once

class OTAManager {
    esp_timer_handle_t _updateTimer;
    Callback<void> _otaStart;

public:
    OTAManager();

    void begin();
    void onOTAStart(function<void()> func) { _otaStart.add(func); }

private:
    void updateCheck();
    bool installUpdate();
    bool parseHash(char* buffer, uint8_t* hash);
};
