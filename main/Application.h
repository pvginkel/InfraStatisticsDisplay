#pragma once

#include "LoadingUI.h"
#include "LogManager.h"
#include "NetworkConnection.h"
#include "OTAManager.h"
#include "Queue.h"
#include "StatsDto.h"
#include "StatsUI.h"

class Application {
    Device* _device;
    NetworkConnection _network_connection;
    OTAManager _ota_manager;
    LoadingUI* _loading_ui;
    StatsUI* _stats_ui;
    Queue _queue;
    DeviceConfiguration _configuration;
    LogManager _log_manager;
    bool _have_sntp_synced;

public:
    Application(Device* device);

    void begin(bool silent);
    void process();

private:
    void setup_flash();
    void do_begin(bool silent);
    void begin_network();
    void begin_network_available();
    void begin_after_initialization();
    void begin_ui();
};
