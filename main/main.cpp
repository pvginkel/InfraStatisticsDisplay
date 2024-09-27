#include "includes.h"

#include "Application.h"

extern "C" {
void app_main(void) {
    // If we've restarted because of a brownout or watchdog reset,
    // perform a silent startup.
    const auto resetReason = esp_reset_reason();
    const auto silent = resetReason == ESP_RST_BROWNOUT || resetReason == ESP_RST_WDT;

    Device device;

    device.begin();

    Application application(&device);

    application.begin(silent);

    while (1) {
        application.process();
    }
}
}
