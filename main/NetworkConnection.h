#pragma once

struct NetworkConnectionState {
    bool connected;
    uint8_t errorReason;
};

class NetworkConnection {
    static NetworkConnection *_instance;
    Queue *_synchronization_queue;
    EventGroupHandle_t _wifi_event_group;
    Callback<NetworkConnectionState> _state_changed;
    int _attempt;
    bool _have_sntp_synced;

public:
    NetworkConnection(Queue *synchronizationQueue);

    void begin();
    void on_state_changed(function<void(NetworkConnectionState)> func) { _state_changed.add(func); }

private:
    void event_handler(esp_event_base_t eventBase, int32_t eventId, void *eventData);
    void setup_sntp();
};
