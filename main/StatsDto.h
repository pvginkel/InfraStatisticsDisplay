#pragma once

enum class JenkinsBuildStatus : int8_t { Aborted, Failure, NotBuilt, Success, Unstable };

struct JenkinsBuildDto {
    string name;
    int number;
    time_t execution;
    JenkinsBuildStatus status;
};

struct KubernetesNodeDto {
    string name;
    time_t created;
    int allocated_pods;
    int allocated_containers;
    int64_t cpu_capacity;
    int64_t cpu_usage;
    int64_t memory_capacity;
    int64_t memory_usage;
};

struct KubernetesJobDto {
    string name;
    string ns;
    time_t created;
    time_t completed;
    bool is_completed;
    int succeeded;
    int failed;
};

struct ContainerStartsStatsDto {
    int day;
    int week;
};

struct StatsDto {
    vector<JenkinsBuildDto> last_builds;
    vector<JenkinsBuildDto> last_failed_builds;
    vector<KubernetesNodeDto> nodes;
    vector<KubernetesJobDto> last_failed_jobs;
    ContainerStartsStatsDto container_starts;

    StatsDto() {}
    StatsDto(const StatsDto&) = delete;
    StatsDto& operator=(const StatsDto&) = delete;
    StatsDto(StatsDto&&) = delete;
    StatsDto& operator=(StatsDto&&) = delete;

    static bool from_json(const char* json_string, StatsDto& stats);
};
