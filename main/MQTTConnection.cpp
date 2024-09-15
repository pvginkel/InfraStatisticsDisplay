#include "includes.h"

#include "MQTTConnection.h"

static const char *TAG = "MQTTConnection";
static const char *TOPIC_PREFIX = "pieter";
static const char *DEVICE_MANUFACTURER = "Pieter";
static const char *DEVICE_MODEL = "Thermostat Display";
constexpr auto QOS_MAX_ONE = 0;      // Send at most one.
constexpr auto QOS_MIN_ONE = 1;      // Send at least one.
constexpr auto QOS_EXACTLY_ONE = 2;  // Send exactly one.

MQTTConnection::MQTTConnection(Queue *synchronizationQueue, const DeviceConfiguration &configuration)
    : _configuration(configuration),
      _synchronizationQueue(synchronizationQueue),
      _deviceId(getDeviceId()),
      _modeTopic(format("%s/%s/set/mode", TOPIC_PREFIX, configuration.getDeviceName().c_str())),
      _localTemperatureTopic(
          format("%s/%s/set/local_temperature", TOPIC_PREFIX, configuration.getDeviceName().c_str())),
      _humidityTopic(format("%s/%s/set/local_humidity", TOPIC_PREFIX, configuration.getDeviceName().c_str())),
      _setpointTopic(format("%s/%s/set/setpoint", TOPIC_PREFIX, configuration.getDeviceName().c_str())),
      _heatingTopic(format("%s/%s/set/heating", TOPIC_PREFIX, configuration.getDeviceName().c_str())),
      _entityTopic(format("%s/%s", TOPIC_PREFIX, configuration.getDeviceName().c_str())),
      _stateTopic(format("%s/%s/state", TOPIC_PREFIX, configuration.getDeviceName().c_str())),
      _logTopic(format("%s/%s/log", TOPIC_PREFIX, configuration.getDeviceName().c_str())) {}

void MQTTConnection::begin() {
    esp_mqtt5_connection_property_config_t connect_property = {
        .session_expiry_interval = 10,
        .maximum_packet_size = 1024,
        .receive_maximum = 65535,
        .topic_alias_maximum = 2,
        .request_resp_info = true,
        .request_problem_info = true,
        .will_delay_interval = 10,
        .message_expiry_interval = 10,
        .payload_format_indicator = true,
    };

    initializeState();

    const auto lastWillMessage = "offline";

    esp_mqtt_client_config_t configuration = {
        .broker =
            {
                .address =
                    {
                        .uri = CONFIG_MQTT_BROKER_URL,
                    },
            },
        .credentials =
            {
                .username = CONFIG_MQTT_USER_ID,
                .authentication =
                    {
                        .password = CONFIG_MQTT_PASSWORD,
                    },
            },
        .session =
            {
                .last_will =
                    {
                        .topic = _stateTopic.c_str(),
                        .msg = lastWillMessage,
                        .msg_len = strlen(lastWillMessage),
                        .qos = QOS_MIN_ONE,
                        .retain = true,
                    },
                .protocol_ver = MQTT_PROTOCOL_V_5,
            },
        .network =
            {
                .disable_auto_reconnect = false,
            },
    };

    _client = esp_mqtt_client_init(&configuration);

    esp_mqtt5_client_set_connect_property(_client, &connect_property);

    esp_mqtt_client_register_event(
        _client, MQTT_EVENT_ANY,
        [](auto eventHandlerArg, auto eventBase, auto eventId, auto eventData) {
            ((MQTTConnection *)eventHandlerArg)->eventHandler(eventBase, eventId, eventData);
        },
        this);

    esp_mqtt_client_start(_client);
}

void MQTTConnection::initializeState() {
    _state.setpoint = DEFAULT_SETPOINT;

    nvs_handle_t handle;
    auto err = nvs_open("storage", NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return;
    }
    ESP_ERROR_CHECK(err);

    int32_t setpoint;
    err = nvs_get_i32(handle, "setpoint", &setpoint);
    if (err == ESP_OK) {
        _state.setpoint = double(setpoint) / 10;
    }

    int8_t mode;
    err = nvs_get_i8(handle, "mode", &mode);
    if (err == ESP_OK) {
        _state.mode = (ThermostatMode)mode;
    }

    nvs_close(handle);
}

void MQTTConnection::saveState(ThermostatState &state) {
    if (_state.setpoint == state.setpoint && _state.mode == state.mode) {
        return;
    }

    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &handle));

    if (!isnan(state.setpoint)) {
        ESP_ERROR_CHECK(nvs_set_i32(handle, "setpoint", int32_t(state.setpoint * 10)));
    }
    ESP_ERROR_CHECK(nvs_set_i8(handle, "mode", int8_t(state.mode)));

    nvs_close(handle);
}

string MQTTConnection::getDeviceId() {
    uint8_t mac[6];

    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));

    return format("0x%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void MQTTConnection::eventHandler(esp_event_base_t eventBase, int32_t eventId, void *eventData) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, eventBase, eventId);
    auto event = (esp_mqtt_event_handle_t)eventData;

    ESP_LOGD(TAG, "Free heap size is %" PRIu32 ", minimum %" PRIu32, esp_get_free_heap_size(),
             esp_get_minimum_free_heap_size());

    switch ((esp_mqtt_event_id_t)eventId) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            handleConnected();
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT disconnected");

            _stateChanged.queue(_synchronizationQueue, {.connected = false});
            break;

        case MQTT_EVENT_SUBSCRIBED:
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            break;

        case MQTT_EVENT_PUBLISHED:
            break;

        case MQTT_EVENT_DATA:
            handleData(event);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT return code is %d", event->error_handle->connect_return_code);
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                if (event->error_handle->esp_tls_last_esp_err) {
                    ESP_LOGI(TAG, "reported from esp-tls");
                }
                if (event->error_handle->esp_tls_stack_err) {
                    ESP_LOGI(TAG, "reported from tls stack");
                }
                if (event->error_handle->esp_transport_sock_errno) {
                    ESP_LOGI(TAG, "captured as transport's socket errno");
                }
                ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
            }
            break;

        default:
            ESP_LOGD(TAG, "Other event id: %d", event->event_id);
            break;
    }
}

void MQTTConnection::handleConnected() {
    subscribe(_modeTopic);
    subscribe(_localTemperatureTopic);
    subscribe(_humidityTopic);
    subscribe(_setpointTopic);
    subscribe(_heatingTopic);

    publishDiscovery();
    setOnline();
    setState(_state, true);

    _stateChanged.queue(_synchronizationQueue, {.connected = true});
}

void MQTTConnection::handleData(esp_mqtt_event_handle_t event) {
    auto topic = string(event->topic, event->topic_len);
    auto data = string(event->data, event->data_len);

    ESP_LOGI(TAG, "Received data topic %s data '%s'", topic.c_str(), data.c_str());

    auto state = _state;

    if (topic == _modeTopic) {
        state.mode = deserializeMode(data);
    } else if (topic == _setpointTopic) {
        state.setpoint = atof(data.c_str());
    } else if (topic == _localTemperatureTopic) {
        state.localTemperature = atof(data.c_str());
    } else if (topic == _humidityTopic) {
        state.localHumidity = atof(data.c_str());
    } else if (topic == _heatingTopic) {
        state.state = iequals(data, "true") ? ThermostatRunningState::True : ThermostatRunningState::False;
    }

    setState(state);
}

void MQTTConnection::subscribe(string &topic) {
    ESP_LOGI(TAG, "Subscribing to topic %s", topic.c_str());

    ESP_ERROR_ASSERT(esp_mqtt_client_subscribe(_client, topic.c_str(), 0) >= 0);
}

void MQTTConnection::publishDiscovery() {
    ESP_LOGI(TAG, "Publishing discovery information");

    auto uniqueIdentifier = format("%s_%s", TOPIC_PREFIX, _deviceId.c_str());

    auto root = cJSON_CreateObject();

    auto availabilityTopic = cJSON_CreateObject();
    cJSON_AddStringToObject(availabilityTopic, "topic", _stateTopic.c_str());

    auto availability = cJSON_AddArrayToObject(root, "availability");
    cJSON_AddItemToArray(availability, availabilityTopic);

    cJSON_AddStringToObject(root, "action_template", "{{ value_json.state }}");
    cJSON_AddStringToObject(root, "action_topic", _entityTopic.c_str());
    cJSON_AddStringToObject(root, "current_temperature_template", "{{ value_json.local_temperature }}");
    cJSON_AddStringToObject(root, "current_temperature_topic", _entityTopic.c_str());
    cJSON_AddStringToObject(root, "current_humidity_template", "{{ value_json.local_humidity }}");
    cJSON_AddStringToObject(root, "current_humidity_topic", _entityTopic.c_str());

    auto device = cJSON_AddObjectToObject(root, "device");

    auto identifiers = cJSON_AddArrayToObject(device, "identifiers");
    cJSON_AddItemToArray(identifiers, cJSON_CreateString(uniqueIdentifier.c_str()));

    cJSON_AddStringToObject(device, "manufacturer", DEVICE_MANUFACTURER);
    cJSON_AddStringToObject(device, "model", DEVICE_MODEL);
    cJSON_AddStringToObject(device, "name", _configuration.getDeviceName().c_str());

    cJSON_AddStringToObject(root, "device_class", "temperature");
    cJSON_AddNumberToObject(root, "max_temp", 35);
    cJSON_AddNumberToObject(root, "min_temp", 4);
    cJSON_AddStringToObject(root, "mode_command_topic", _modeTopic.c_str());
    cJSON_AddStringToObject(root, "mode_state_template", "{{ value_json.mode }}");
    cJSON_AddStringToObject(root, "mode_state_topic", _entityTopic.c_str());

    auto modes = cJSON_AddArrayToObject(root, "modes");
    cJSON_AddItemToArray(modes, cJSON_CreateString("off"));
    cJSON_AddItemToArray(modes, cJSON_CreateString("heat"));

    cJSON_AddNullToObject(root, "name");
    cJSON_AddStringToObject(root, "object_id", _configuration.getDeviceEntityId().c_str());
    cJSON_AddNumberToObject(root, "temp_step", 0.5);
    cJSON_AddStringToObject(root, "temperature_command_topic", _setpointTopic.c_str());
    cJSON_AddStringToObject(root, "temperature_state_template", "{{ value_json.setpoint }}");
    cJSON_AddStringToObject(root, "temperature_state_topic", _entityTopic.c_str());
    cJSON_AddStringToObject(root, "temperature_unit", "C");
    cJSON_AddStringToObject(root, "unique_id", uniqueIdentifier.c_str());

    auto json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    auto publishTopic = format("homeassistant/climate/%s/climate/config", _deviceId.c_str());

    ESP_ERROR_ASSERT(esp_mqtt_client_publish(_client, publishTopic.c_str(), json, 0, QOS_MIN_ONE, true) >= 0);

    cJSON_free(json);
}

void MQTTConnection::setOnline() {
    ESP_ERROR_ASSERT(esp_mqtt_client_publish(_client, _stateTopic.c_str(), "online", 0, QOS_MIN_ONE, true) >= 0);
}

void MQTTConnection::setState(ThermostatState &state, bool force) {
    if (!force && state.equals(_state)) {
        ESP_LOGD(TAG, "Ignoring state change, nothing changed");
    }

    ESP_LOGI(TAG, "Received new state");

    saveState(state);

    _state = state;

    if (!state.valid()) {
        ESP_LOGI(TAG, "Skipping publishing of new state until it's valid");
        return;
    }

    _thermostatStateChanged.call();

    auto root = cJSON_CreateObject();

    cJSON_AddNumberToObject(root, "local_temperature", state.localTemperature);
    cJSON_AddNumberToObject(root, "local_humidity", state.localHumidity);
    cJSON_AddNumberToObject(root, "setpoint", state.setpoint);
    cJSON_AddStringToObject(root, "mode", serializeMode(state.mode));
    cJSON_AddStringToObject(root, "state", state.state == ThermostatRunningState::True ? "heating" : "idle");

    auto json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    auto result = esp_mqtt_client_publish(_client, _entityTopic.c_str(), json, 0, QOS_MIN_ONE, true);
    if (result < 0) {
        ESP_LOGE(TAG, "Sending status update message failed with error %d", result);
    }

    cJSON_free(json);
}

const char *MQTTConnection::serializeMode(ThermostatMode value) {
    switch (value) {
        case ThermostatMode::Heat:
            return "heat";
        default:
            return "off";
    }
}

ThermostatMode MQTTConnection::deserializeMode(string &value) {
    if (value == "heat") {
        return ThermostatMode::Heat;
    } else if (value == "off") {
        return ThermostatMode::Off;
    } else {
        ESP_LOGE(TAG, "Received invalid mode %s", value.c_str());
        return ThermostatMode::Off;
    }
}