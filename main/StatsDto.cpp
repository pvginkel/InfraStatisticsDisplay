// ReSharper disable CppClangTidyMiscUseAnonymousNamespace

#include "includes.h"

#include "StatsDto.h"

#include "cJSON.h"

static bool parse_jenkins_build_status(const char* statusStr, JenkinsBuildStatus& status) {
    if (strcmp(statusStr, "ABORTED") == 0) {
        status = JenkinsBuildStatus::Aborted;
    } else if (strcmp(statusStr, "FAILURE") == 0) {
        status = JenkinsBuildStatus::Failure;
    } else if (strcmp(statusStr, "NOT_BUILT") == 0) {
        status = JenkinsBuildStatus::NotBuilt;
    } else if (strcmp(statusStr, "SUCCESS") == 0) {
        status = JenkinsBuildStatus::Success;
    } else if (strcmp(statusStr, "UNSTABLE") == 0) {
        status = JenkinsBuildStatus::Unstable;
    } else {
        return false;
    }

    return true;
}

// Helper function to parse JenkinsBuildDto
static bool parse_jenkins_build(const cJSON* item, JenkinsBuildDto& build) {
    if (!cJSON_IsObject(item))
        return false;

    const cJSON* name = cJSON_GetObjectItemCaseSensitive(item, "name");
    const cJSON* number = cJSON_GetObjectItemCaseSensitive(item, "number");
    const cJSON* execution = cJSON_GetObjectItemCaseSensitive(item, "execution");
    const cJSON* status = cJSON_GetObjectItemCaseSensitive(item, "status");

    if (!cJSON_IsString(name) || !cJSON_IsNumber(number) || !cJSON_IsNumber(execution) || !cJSON_IsString(status)) {
        return false;
    }

    build.name = name->valuestring;
    build.number = number->valueint;
    build.execution = static_cast<time_t>(execution->valuedouble);
    if (!parse_jenkins_build_status(status->valuestring, build.status)) {
        return false;
    }

    return true;
}

// Helper function to parse KubernetesNodeDto
static bool parse_kubernetes_node(const cJSON* item, KubernetesNodeDto& node) {
    if (!cJSON_IsObject(item))
        return false;

    const cJSON* name = cJSON_GetObjectItemCaseSensitive(item, "name");
    const cJSON* created = cJSON_GetObjectItemCaseSensitive(item, "created");
    const cJSON* allocated_pods = cJSON_GetObjectItemCaseSensitive(item, "allocated_pods");
    const cJSON* allocated_containers = cJSON_GetObjectItemCaseSensitive(item, "allocated_containers");
    const cJSON* cpu_capacity = cJSON_GetObjectItemCaseSensitive(item, "cpu_capacity");
    const cJSON* cpu_usage = cJSON_GetObjectItemCaseSensitive(item, "cpu_usage");
    const cJSON* memory_capacity = cJSON_GetObjectItemCaseSensitive(item, "memory_capacity");
    const cJSON* memory_usage = cJSON_GetObjectItemCaseSensitive(item, "memory_usage");

    if (!cJSON_IsString(name) || !cJSON_IsNumber(created) || !cJSON_IsNumber(allocated_pods) ||
        !cJSON_IsNumber(allocated_containers) || !cJSON_IsNumber(cpu_capacity) || !cJSON_IsNumber(cpu_usage) ||
        !cJSON_IsNumber(memory_capacity) || !cJSON_IsNumber(memory_usage)) {
        return false;
    }

    node.name = name->valuestring;
    node.created = static_cast<time_t>(created->valuedouble);
    node.allocated_pods = allocated_pods->valueint;
    node.allocated_containers = allocated_containers->valueint;
    node.cpu_capacity = static_cast<int64_t>(cpu_capacity->valuedouble);
    node.cpu_usage = static_cast<int64_t>(cpu_usage->valuedouble);
    node.memory_capacity = static_cast<int64_t>(memory_capacity->valuedouble);
    node.memory_usage = static_cast<int64_t>(memory_usage->valuedouble);

    return true;
}

// Helper function to parse KubernetesJobDto
static bool parse_kubernetes_job(const cJSON* item, KubernetesJobDto& job) {
    if (!cJSON_IsObject(item))
        return false;

    const cJSON* name = cJSON_GetObjectItemCaseSensitive(item, "name");
    const cJSON* ns = cJSON_GetObjectItemCaseSensitive(item, "namespace");
    const cJSON* created = cJSON_GetObjectItemCaseSensitive(item, "created");
    const cJSON* completed = cJSON_GetObjectItemCaseSensitive(item, "completed");
    const cJSON* succeeded = cJSON_GetObjectItemCaseSensitive(item, "succeeded");
    const cJSON* failed = cJSON_GetObjectItemCaseSensitive(item, "failed");

    if (!cJSON_IsString(name) || !cJSON_IsString(ns) || !cJSON_IsNumber(created) ||
        !(cJSON_IsNumber(completed) || cJSON_IsNull(completed)) || !cJSON_IsNumber(succeeded) ||
        !cJSON_IsNumber(failed)) {
        return false;
    }

    job.name = name->valuestring;
    job.ns = ns->valuestring;
    job.created = static_cast<time_t>(created->valuedouble);

    if (cJSON_IsNumber(completed)) {
        job.completed = static_cast<time_t>(completed->valuedouble);
    } else {
        job.completed = 0;
    }

    job.is_completed = cJSON_IsNumber(completed);
    job.succeeded = succeeded->valueint;
    job.failed = failed->valueint;

    return true;
}

// Helper function to parse ContainerStartsStatsDto
static bool parse_container_starts(const cJSON* item, ContainerStartsStatsDto& containerStarts) {
    if (!cJSON_IsObject(item))
        return false;

    const cJSON* day = cJSON_GetObjectItemCaseSensitive(item, "day");
    const cJSON* week = cJSON_GetObjectItemCaseSensitive(item, "week");

    if (!cJSON_IsNumber(day) || !cJSON_IsNumber(week)) {
        return false;
    }

    containerStarts.day = day->valueint;
    containerStarts.week = week->valueint;

    return true;
}

void StatsDto::clear() {
    last_builds.clear();
    last_failed_builds.clear();
    nodes.clear();
    last_failed_jobs.clear();
    container_starts = {};
}

bool StatsDto::from_json(const char* json_string, StatsDto& stats) {
    stats.clear();

    cJSON_Data root = {cJSON_Parse(json_string)};
    if (*root == nullptr) {
        // Parsing failed
        return false;
    }

    // Parse container_starts
    const cJSON* container_starts = cJSON_GetObjectItemCaseSensitive(*root, "container_starts");
    if (container_starts && !parse_container_starts(container_starts, stats.container_starts)) {
        return false;
    }

    // Parse last_builds
    const cJSON* last_builds = cJSON_GetObjectItemCaseSensitive(*root, "last_builds");
    if (last_builds && cJSON_IsArray(last_builds)) {
        cJSON* build;
        cJSON_ArrayForEach(build, last_builds) {
            JenkinsBuildDto dto;
            if (!parse_jenkins_build(build, dto)) {
                return false;
            }
            stats.last_builds.push_back(dto);
        }
    }

    // Parse last_failed_builds
    const cJSON* last_failed_builds = cJSON_GetObjectItemCaseSensitive(*root, "last_failed_builds");
    if (last_failed_builds && cJSON_IsArray(last_failed_builds)) {
        cJSON* build;
        cJSON_ArrayForEach(build, last_failed_builds) {
            JenkinsBuildDto dto;
            if (!parse_jenkins_build(build, dto)) {
                return false;
            }
            stats.last_failed_builds.push_back(dto);
        }
    }

    // Parse nodes
    const cJSON* nodes = cJSON_GetObjectItemCaseSensitive(*root, "nodes");
    if (nodes && cJSON_IsArray(nodes)) {
        cJSON* node;
        cJSON_ArrayForEach(node, nodes) {
            KubernetesNodeDto dto;
            if (!parse_kubernetes_node(node, dto)) {
                return false;
            }
            stats.nodes.push_back(dto);
        }
    }

    // Parse last_failed_jobs
    const cJSON* last_failed_jobs = cJSON_GetObjectItemCaseSensitive(*root, "last_failed_jobs");
    if (last_failed_jobs && cJSON_IsArray(last_failed_jobs)) {
        cJSON* job;
        cJSON_ArrayForEach(job, last_failed_jobs) {
            KubernetesJobDto dto;
            if (!parse_kubernetes_job(job, dto)) {
                return false;
            }
            stats.last_failed_jobs.push_back(dto);
        }
    }

    // If all parsing steps are successful
    return true;
}
