#pragma once

#include "LoadingUI.h"
#include "LogManager.h"
#include "OTAManager.h"
#include "Queue.h"
#include "StatsDto.h"
#include "StatsUI.h"
#include "WifiConnection.h"

class Application {
    static Application* _instance;
    Device* _device;
    WifiConnection _wifi_connection;
    OTAManager _ota_manager;
    LoadingUI* _loading_ui;
    StatsUI* _stats_ui;
    Queue _queue;
    DeviceConfiguration _configuration;
    LogManager _log_manager;

public:
    Application(Device* device);

    void begin(bool silent);
    void process();

private:
    void setup_flash();
    void do_begin(bool silent);
    void begin_wifi();
    void begin_wifi_available();
    void begin_sntp_synced();
    void begin_after_initialization();
    void begin_ui();
};
