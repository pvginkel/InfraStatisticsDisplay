#pragma once

#include "ApplicationBase.h"
#include "LoadingUI.h"
#include "StatsDto.h"
#include "StatsUI.h"

class Application : public ApplicationBase {
    Device* _device;
    LoadingUI* _loading_ui{};
    StatsUI* _stats_ui{};

public:
    Application(Device* device);

protected:
    void do_begin() override;
    void do_ready() override;
    void do_network_connection_failed() override;
    void do_process() override;

private:
    void run();
    void state_changed();
    void register_mqtt_callbacks();
};
