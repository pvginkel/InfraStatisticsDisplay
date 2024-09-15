#pragma once

enum class JenkinsBuildStatus: int8_t {
    Aborted,
    Failure,
    NotBuilt,
    Success,
    Unstable
};

struct JenkinsBuildDto {
    string name;
    int number;
    time_t execution;
    JenkinsBuildStatus status;
};

struct KubernetesNodeDto {
    string name;
    time_t created;
    int allocatedPods;
    int allocatedContainers;
    int64_t cpuCapacity;
    int64_t cpuUsage;
    int64_t memoryCapacity;
    int64_t memoryUsage;
};

struct KubernetesJobDto {
    string name;
    string ns;
    time_t created;
    time_t completed;
    bool isCompleted;
    int succeeded;
    int failed;
};

struct ContainerStartsStatsDto {
    int day;
    int week;
};

struct StatsDto {
    vector<JenkinsBuildDto> lastBuilds;
    vector<JenkinsBuildDto> lastFailedBuilds;
    vector<KubernetesNodeDto> nodes;
    vector<KubernetesJobDto> lastFailedJobs;
    ContainerStartsStatsDto containerStarts;

    static bool fromJson(const char* jsonString, StatsDto& stats);
};

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
