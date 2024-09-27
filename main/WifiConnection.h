#pragma once

struct WifiConnectionState {
    bool connected;
    uint8_t errorReason;
};

class WifiConnection {
    Queue *_synchronization_queue;
    EventGroupHandle_t _wifiEventGroup;
    Callback<WifiConnectionState> _state_changed;
    int _attempt;

    void event_handler(esp_event_base_t eventBase, int32_t eventId, void *eventData);

public:
    WifiConnection(Queue *synchronizationQueue);

    void begin();
    void on_state_changed(function<void(WifiConnectionState)> func) { _state_changed.add(func); }
};
