#include "includes.h"

#include "StatsUI.h"

#include <chrono>
#include <ctime>

#include "Messages.h"
#include "lv_support.h"

LOG_TAG(StatsUI);

void StatsUI::do_begin() { LvglUI::do_begin(); }

#ifndef LV_SIMULATOR

void StatsUI::do_update() {
    auto current_time = time(nullptr);

    if (_next_update == 0 || current_time >= _next_update) {
        if (_next_update == 0) {
            // Get the time_t for the start of the hour.
            auto start_hour_time_info = *localtime(&current_time);
            start_hour_time_info.tm_min = 0;
            start_hour_time_info.tm_sec = 0;
            auto start_hour_time = mktime(&start_hour_time_info);

            // Calculate the number of seconds from the current time to
            // the next update interval.
            auto diff = current_time - start_hour_time;
            auto rounded_up =
                ((diff + CONFIG_INFRA_STATISTICS_UPDATE_INTERVAL - 1) / CONFIG_INFRA_STATISTICS_UPDATE_INTERVAL) *
                CONFIG_INFRA_STATISTICS_UPDATE_INTERVAL;

            // Set the next update to the correct time. After the initial
            // calculation, every other update will be calculated simply
            // by adding the update interval.
            _next_update = start_hour_time + rounded_up;

            // Subtract 10 seconds to better time the update on the interval.
            // The update completes in roughly 15 seconds.
            _next_update -= 10;
        } else {
            _next_update += CONFIG_INFRA_STATISTICS_UPDATE_INTERVAL;
        }

        update_stats();
    }
}

void StatsUI::update_stats() {
    esp_http_client_config_t config = {
        .url = CONFIG_INFRA_STATISTICS_ENDPOINT,
        .timeout_ms = CONFIG_INFRA_STATISTICS_ENDPOINT_RECV_TIMEOUT,
    };

    ESP_LOGI(TAG, "Downloading statistics from %s", config.url);

    string json;
    auto err = esp_http_download_string(config, json, 128 * 1024);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to download statistics");
        return;
    }

    if (!StatsDto::from_json(json.c_str(), _stats)) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return;
    }

    ESP_LOGI(TAG, "Updating screen");

    render();
}

#endif

void StatsUI::do_render(lv_obj_t* parent) {
    auto outer_cont = lv_obj_create(parent);
    reset_outer_container_styles(outer_cont);
    static lv_coord_t outer_cont_col_desc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t outer_cont_row_desc[] = {LV_GRID_FR(1),   LV_GRID_CONTENT, LV_GRID_CONTENT,
                                               LV_GRID_CONTENT, LV_GRID_FR(1),   LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(outer_cont, outer_cont_col_desc, outer_cont_row_desc);

    create_statistics(outer_cont, 0, 1);
    create_kubernetes_nodes(outer_cont, 0, 2);

    auto bottom_outer_cont = lv_obj_create(outer_cont);
    reset_layout_container_styles(bottom_outer_cont);
    static lv_coord_t bottom_outer_cont_col_desc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t bottom_outer_cont_row_desc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(bottom_outer_cont, bottom_outer_cont_col_desc, bottom_outer_cont_row_desc);
    lv_obj_set_grid_cell(bottom_outer_cont, LV_GRID_ALIGN_STRETCH, 0, LV_GRID_ALIGN_STRETCH, 3);

    create_last_builds(bottom_outer_cont, 0, 0);
    create_failed_jobs(bottom_outer_cont, 1, 0);
}

void StatsUI::create_kubernetes_nodes(lv_obj_t* parent, uint8_t col, uint8_t row) {
    auto node_count = _stats.nodes.size();

    auto nodes_cont = lv_obj_create(parent);
    reset_layout_container_styles(nodes_cont);
    static lv_coord_t* top_outer_cont_col_desc = nullptr;
    delete[] top_outer_cont_col_desc;
    top_outer_cont_col_desc = new lv_coord_t[node_count * 2];
    for (size_t i = 0; i < node_count; i++) {
        top_outer_cont_col_desc[i * 2] = LV_GRID_CONTENT;
        top_outer_cont_col_desc[i * 2 + 1] = LV_GRID_FR(1);
    }
    top_outer_cont_col_desc[node_count * 2 - 1] = LV_GRID_TEMPLATE_LAST;
    static lv_coord_t top_outer_cont_row_desc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(nodes_cont, top_outer_cont_col_desc, top_outer_cont_row_desc);
    lv_obj_set_grid_cell(nodes_cont, LV_GRID_ALIGN_STRETCH, col, LV_GRID_ALIGN_START, row);
    lv_obj_set_style_pad_top(nodes_cont, lv_dpx(18), LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(nodes_cont, lv_dpx(26), LV_PART_MAIN);

    for (size_t i = 0; i < node_count; i++) {
        create_kubernetes_node(nodes_cont, _stats.nodes[i], i * 2, 0);
    }
}

void StatsUI::create_kubernetes_node(lv_obj_t* parent, KubernetesNodeDto& node, uint8_t col, uint8_t row) {
    auto circle_cont = lv_obj_create(parent);
    reset_layout_container_styles(circle_cont);
    lv_obj_set_grid_cell(circle_cont, LV_GRID_ALIGN_CENTER, col, LV_GRID_ALIGN_START, row);
    static lv_coord_t cont_col_desc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t cont_row_desc[] = {LV_GRID_FR(1),   LV_GRID_CONTENT, LV_GRID_CONTENT,
                                         LV_GRID_CONTENT, LV_GRID_FR(1),   LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(circle_cont, cont_col_desc, cont_row_desc);
    lv_obj_set_size(circle_cont, lv_dpx(230), lv_dpx(230));
    lv_obj_set_style_radius(circle_cont, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(circle_cont, lv_dpx(6), LV_PART_MAIN);
    lv_obj_set_style_border_color(circle_cont, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(circle_cont, 0, LV_PART_MAIN);

    auto name_label = lv_label_create(circle_cont);
    lv_obj_set_grid_cell(name_label, LV_GRID_ALIGN_CENTER, 0, LV_GRID_ALIGN_START, 1);
    lv_label_set_text(name_label, node.name.c_str());
    lv_obj_set_style_text_font(name_label, SMALL_FONT, LV_PART_MAIN);

    auto resources_row = lv_obj_create(circle_cont);
    reset_layout_container_styles(resources_row);
    lv_obj_set_grid_cell(resources_row, LV_GRID_ALIGN_CENTER, 0, LV_GRID_ALIGN_START, 2);
    static lv_coord_t row1_cont_col_desc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT,
                                              LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row1_cont_row_desc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(resources_row, row1_cont_col_desc, row1_cont_row_desc);
    lv_obj_set_style_pad_ver(resources_row, lv_dpx(7), LV_PART_MAIN);

    auto cpu_icon_label = lv_label_create(resources_row);
    lv_label_set_text(cpu_icon_label, FA_MICROCHIP);
    lv_obj_set_style_text_font(cpu_icon_label, XSMALL_ICONS_FONT, LV_PART_MAIN);
    lv_obj_set_grid_cell(cpu_icon_label, LV_GRID_ALIGN_START, 0, LV_GRID_ALIGN_CENTER, 0);

    auto cpu_label = lv_label_create(resources_row);
    lv_label_set_text(cpu_label, format("%d%%", (int)(node.cpu_usage * 100.0f / node.cpu_capacity)).c_str());
    lv_obj_set_style_text_font(cpu_label, SMALL_FONT, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(cpu_label, lv_dpx(5), LV_PART_MAIN);
    lv_obj_set_grid_cell(cpu_label, LV_GRID_ALIGN_START, 1, LV_GRID_ALIGN_CENTER, 0);

    auto memory_icon_label = lv_label_create(resources_row);
    lv_label_set_text(memory_icon_label, FA_MEMORY);
    lv_obj_set_style_text_font(memory_icon_label, XSMALL_ICONS_FONT, LV_PART_MAIN);
    lv_obj_set_style_pad_left(memory_icon_label, lv_dpx(5), LV_PART_MAIN);
    lv_obj_set_grid_cell(memory_icon_label, LV_GRID_ALIGN_START, 2, LV_GRID_ALIGN_CENTER, 0);

    auto memory_label = lv_label_create(resources_row);
    lv_label_set_text(memory_label, format("%d%%", (int)(node.memory_usage * 100.0f / node.memory_capacity)).c_str());
    lv_obj_set_style_text_font(memory_label, SMALL_FONT, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(memory_label, lv_dpx(5), LV_PART_MAIN);
    lv_obj_set_grid_cell(memory_label, LV_GRID_ALIGN_START, 3, LV_GRID_ALIGN_CENTER, 0);

    auto containers_row = lv_obj_create(circle_cont);
    reset_layout_container_styles(containers_row);
    lv_obj_set_grid_cell(containers_row, LV_GRID_ALIGN_CENTER, 0, LV_GRID_ALIGN_START, 3);
    static lv_coord_t row2_cont_col_desc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT,
                                              LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row2_cont_row_desc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(containers_row, row2_cont_col_desc, row2_cont_row_desc);

    auto pods_icon_label = lv_label_create(containers_row);
    lv_label_set_text(pods_icon_label, FA_CUBES);
    lv_obj_set_style_text_font(pods_icon_label, XSMALL_ICONS_FONT, LV_PART_MAIN);
    lv_obj_set_grid_cell(pods_icon_label, LV_GRID_ALIGN_START, 0, LV_GRID_ALIGN_CENTER, 0);

    auto pods_label = lv_label_create(containers_row);
    lv_label_set_text(pods_label, format("%d", node.allocated_pods).c_str());
    lv_obj_set_style_text_font(pods_label, SMALL_FONT, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(pods_label, lv_dpx(5), LV_PART_MAIN);
    lv_obj_set_grid_cell(pods_label, LV_GRID_ALIGN_START, 1, LV_GRID_ALIGN_CENTER, 0);

    auto containers_icon_label = lv_label_create(containers_row);
    lv_label_set_text(containers_icon_label, FA_CUBE);
    lv_obj_set_style_text_font(containers_icon_label, XSMALL_ICONS_FONT, LV_PART_MAIN);
    lv_obj_set_style_pad_left(containers_icon_label, lv_dpx(5), LV_PART_MAIN);
    lv_obj_set_grid_cell(containers_icon_label, LV_GRID_ALIGN_START, 2, LV_GRID_ALIGN_CENTER, 0);

    auto containers_label = lv_label_create(containers_row);
    lv_label_set_text(containers_label, format("%d", node.allocated_containers).c_str());
    lv_obj_set_style_text_font(containers_label, SMALL_FONT, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(containers_label, lv_dpx(5), LV_PART_MAIN);
    lv_obj_set_grid_cell(containers_label, LV_GRID_ALIGN_START, 3, LV_GRID_ALIGN_CENTER, 0);
}

void StatsUI::create_statistics(lv_obj_t* parent, uint8_t col, uint8_t row) {
    auto cont = lv_obj_create(parent);
    reset_layout_container_styles(cont);
    lv_obj_set_grid_cell(cont, LV_GRID_ALIGN_STRETCH, col, LV_GRID_ALIGN_CENTER, row);
    static lv_coord_t cont_col_desc[] = {LV_GRID_FR(1),   LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_CONTENT,
                                         LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t cont_row_desc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(cont, cont_col_desc, cont_row_desc);

    auto total_containers = 0;
    auto total_pods = 0;

    for (auto& node : _stats.nodes) {
        total_containers += node.allocated_containers;
        total_pods += node.allocated_pods;
    }

    create_container_starts_cell(cont, total_pods, FA_CUBES, 1, 0);
    create_container_starts_cell(cont, total_containers, FA_CUBE, 3, 0);
    create_container_starts_cell(cont, _stats.container_starts.week, FA_CALENDAR_WEEK, 4, 0);
    create_container_starts_cell(cont, _stats.container_starts.day, FA_CALENDAR_DAY, 5, 0);
}

void StatsUI::create_container_starts_cell(lv_obj_t* parent, int value, const char* icon, uint8_t col, uint8_t row) {
    auto cont = lv_obj_create(parent);
    reset_layout_container_styles(cont);
    lv_obj_set_grid_cell(cont, LV_GRID_ALIGN_START, col, LV_GRID_ALIGN_CENTER, row);
    static lv_coord_t cont_col_desc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    static lv_coord_t cont_row_desc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(cont, cont_col_desc, cont_row_desc);
    lv_obj_set_style_pad_hor(cont, lv_dpx(24), LV_PART_MAIN);

    auto icon_label = lv_label_create(cont);
    lv_label_set_text(icon_label, icon);
    lv_obj_set_style_text_font(icon_label, SMALL_ICONS_FONT, LV_PART_MAIN);
    lv_obj_set_grid_cell(icon_label, LV_GRID_ALIGN_START, 0, LV_GRID_ALIGN_CENTER, 0);

    auto label = lv_label_create(cont);
    lv_label_set_text(label, format_number(value).c_str());
    lv_obj_set_style_text_font(label, NORMAL_FONT, LV_PART_MAIN);
    lv_obj_set_grid_cell(label, LV_GRID_ALIGN_START, 1, LV_GRID_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(label, lv_dpx(10), LV_PART_MAIN);
}

void StatsUI::create_last_builds(lv_obj_t* parent, uint8_t col, uint8_t row) {
    vector<Job> jobs;

    jobs.reserve(_stats.last_builds.size());

    for (auto& build : _stats.last_builds) {
        jobs.emplace_back(FA_GEARS, nullptr, move(format("#%d %s", build.number, build.name.c_str())), build.execution);
    }

    create_jobs(parent, jobs, col, row);
}

void StatsUI::create_failed_jobs(lv_obj_t* parent, uint8_t col, uint8_t row) {
    vector<Job> jobs;

    jobs.reserve(_stats.last_failed_builds.size() + _stats.last_failed_jobs.size());

    for (auto& build : _stats.last_failed_builds) {
        jobs.emplace_back(FA_GEARS, FA_CIRCLE_EXCLAMATION, move(format("#%d %s", build.number, build.name.c_str())),
                          build.execution);
    }

    for (auto& job : _stats.last_failed_jobs) {
        jobs.emplace_back(FA_CIRCLE_PLAY, FA_CIRCLE_EXCLAMATION,
                          move(format("%s (%s)", job.name.c_str(), job.ns.c_str())), job.created);
    }

    sort(jobs.begin(), jobs.end(), [](const Job& a, const Job& b) { return a.time > b.time; });

    create_jobs(parent, jobs, col, row);
}

void StatsUI::create_jobs(lv_obj_t* parent, vector<Job>& jobs, uint8_t col, uint8_t row) {
    auto cont = lv_obj_create(parent);
    reset_layout_container_styles(cont);
    lv_obj_set_grid_cell(cont, LV_GRID_ALIGN_STRETCH, col, LV_GRID_ALIGN_START, row);
    static lv_coord_t cont_col_desc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t cont_row_desc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT,
                                         LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(cont, cont_col_desc, cont_row_desc);

    auto job_count = min((int)jobs.size(), 5);

    for (auto i = 0; i < job_count; i++) {
        create_job(cont, jobs[i], 0, i);
    }
}

void StatsUI::create_job(lv_obj_t* parent, Job& job, uint8_t col, uint8_t row) {
    auto cont = lv_obj_create(parent);
    reset_layout_container_styles(cont);
    lv_obj_set_grid_cell(cont, LV_GRID_ALIGN_STRETCH, col, LV_GRID_ALIGN_START, row);
    static lv_coord_t cont_col_desc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t cont_row_desc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(cont, cont_col_desc, cont_row_desc);

    if (job.status_icon) {
        auto status_icon_label = lv_label_create(cont);
        lv_label_set_text(status_icon_label, job.status_icon);
        lv_obj_set_style_text_font(status_icon_label, XSMALL_ICONS_FONT, LV_PART_MAIN);
        lv_obj_set_style_pad_left(status_icon_label, lv_dpx(5), LV_PART_MAIN);
        lv_obj_set_grid_cell(status_icon_label, LV_GRID_ALIGN_START, 0, LV_GRID_ALIGN_CENTER, 0);
    }

    auto icon_label = lv_label_create(cont);
    lv_label_set_text(icon_label, job.icon);
    lv_obj_set_style_text_font(icon_label, XSMALL_ICONS_FONT, LV_PART_MAIN);
    lv_obj_set_style_pad_left(icon_label, lv_dpx(5), LV_PART_MAIN);
    lv_obj_set_grid_cell(icon_label, LV_GRID_ALIGN_START, 1, LV_GRID_ALIGN_CENTER, 0);

    tm job_time_info;
    localtime_r(&job.time, &job_time_info);

    time_t now;
    tm now_time_info;
    time(&now);
    localtime_r(&now, &now_time_info);

    string time_str;

    if (job_time_info.tm_year != now_time_info.tm_year) {
        time_str = format("%d", job_time_info.tm_year);
    } else if (!(job_time_info.tm_mon == now_time_info.tm_mon && job_time_info.tm_mday == now_time_info.tm_mday)) {
        time_str = format("%d-%d", job_time_info.tm_mday, job_time_info.tm_mon + 1);
    } else {
        time_str = format("%d:%02d", job_time_info.tm_hour, job_time_info.tm_min);
    }

    auto label = lv_label_create(cont);
    lv_label_set_text(label, format("%s: %s", time_str.c_str(), job.name.c_str()).c_str());
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(label, SMALL_FONT, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(label, lv_dpx(5), LV_PART_MAIN);
    lv_obj_set_grid_cell(label, LV_GRID_ALIGN_STRETCH, 2, LV_GRID_ALIGN_CENTER, 0);
}
