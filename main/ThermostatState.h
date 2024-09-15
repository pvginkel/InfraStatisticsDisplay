#pragma once

enum class ThermostatRunningState { Unknown, True, False };
enum class ThermostatMode { Off, Heat };

struct ThermostatState {
    double localTemperature;
    double localHumidity;
    double setpoint;
    ThermostatMode mode;
    ThermostatRunningState state;

    ThermostatState() : localTemperature(NAN), localHumidity(NAN), setpoint(NAN), mode(), state() {}

    bool equals(ThermostatState &other) {
        return localTemperature == other.localTemperature && localHumidity == other.localHumidity &&
               setpoint == other.setpoint && mode == other.mode && state == other.state;
    }

    bool valid() {
        return !isnan(localTemperature) && !isnan(localHumidity) && !isnan(setpoint) &&
               state != ThermostatRunningState::Unknown;
    }
};
