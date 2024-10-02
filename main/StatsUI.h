#pragma once

#include "Device.h"
#include "LvglUI.h"
#include "StatsDto.h"

class StatsUI : public LvglUI {
    struct Job {
        Job(const char* icon, const char* status_icon, string&& name, time_t time)
            : icon(icon), status_icon(status_icon), name(move(name)), time(time) {}

        const char* icon;
        const char* status_icon;
        string name;
        time_t time;
    };

    StatsDto _stats;
#ifndef LV_SIMULATOR
    time_t _next_update = 0;
#endif

public:
#ifdef LV_SIMULATOR
    StatsDto& get_stats() { return _stats; }
#endif

protected:
    void do_begin() override;
    void do_render(lv_obj_t* parent) override;

#ifndef LV_SIMULATOR
    void do_update() override;
    void update_stats();
#endif

    void create_kubernetes_nodes(lv_obj_t* parent, uint8_t col, uint8_t row);
    void create_kubernetes_node(lv_obj_t* parent, KubernetesNodeDto& node, uint8_t col, uint8_t row);
    void create_statistics(lv_obj_t* parent, uint8_t col, uint8_t row);
    void create_container_starts_cell(lv_obj_t* parent, int value, const char* icon, uint8_t col, uint8_t row);
    void create_last_builds(lv_obj_t* parent, uint8_t col, uint8_t row);
    void create_failed_jobs(lv_obj_t* parent, uint8_t col, uint8_t row);
    void create_job(lv_obj_t* parent, Job& job, uint8_t row);
    void create_jobs(lv_obj_t* parent, vector<Job>& jobs, uint8_t col, uint8_t row);
};
