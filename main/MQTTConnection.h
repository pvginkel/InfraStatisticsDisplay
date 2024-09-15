#pragma once

#include "DeviceConfiguration.h"
#include "ThermostatState.h"

struct MQTTConnectionState {
    bool connected;
};

class MQTTConnection {
    static constexpr double DEFAULT_SETPOINT = 19;

    static string getDeviceId();

    const DeviceConfiguration &_configuration;
    Queue *_synchronizationQueue;
    string _deviceId;
    esp_mqtt_client_handle_t _client;
    string _modeTopic;
    string _localTemperatureTopic;
    string _humidityTopic;
    string _setpointTopic;
    string _heatingTopic;
    string _entityTopic;
    string _stateTopic;
    string _logTopic;
    ThermostatState _state;
    Callback<MQTTConnectionState> _stateChanged;
    Callback<void> _thermostatStateChanged;

public:
    MQTTConnection(Queue *synchronizationQueue, const DeviceConfiguration &configuration);

    void begin();
    void onStateChanged(function<void(MQTTConnectionState)> func) { _stateChanged.add(func); }
    void onThermostatStateChanged(function<void()> func) { _thermostatStateChanged.add(func); }
    ThermostatState getState() { return _state; }
    void setState(ThermostatState &state, bool force = false);

private:
    void initializeState();
    void saveState(ThermostatState &state);
    void eventHandler(esp_event_base_t eventBase, int32_t eventId, void *eventData);
    void handleConnected();
    void handleData(esp_mqtt_event_handle_t event);
    void subscribe(string &topic);
    void publishDiscovery();
    void setOnline();
    const char *serializeMode(ThermostatMode value);
    ThermostatMode deserializeMode(string &value);
};
