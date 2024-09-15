#pragma once

struct WifiConnectionState {
    bool connected;
    uint8_t errorReason;
};

class WifiConnection {
    Queue *_synchronizationQueue;
    EventGroupHandle_t _wifiEventGroup;
    Callback<WifiConnectionState> _stateChanged;
    int _attempt;

    void eventHandler(esp_event_base_t eventBase, int32_t eventId, void *eventData);

public:
    WifiConnection(Queue *synchronizationQueue);

    void begin();
    void onStateChanged(function<void(WifiConnectionState)> func) { _stateChanged.add(func); }
};
