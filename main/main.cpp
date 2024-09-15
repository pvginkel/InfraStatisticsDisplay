#include "includes.h"

#include "Application.h"
#include "ESP_Panel.h"

extern "C" {
void app_main(void) {
    // If we've restarted because of a brownout or watchdog reset,
    // perform a silent startup.
    const auto resetReason = esp_reset_reason();
    const auto silent = resetReason == ESP_RST_BROWNOUT || resetReason == ESP_RST_WDT;

    ESP_Panel panel;

    auto display = panel.begin(silent);

    Application application(panel);

    application.begin(display, silent);

    while (1) {
        application.process();

        panel.process();
    }
}
}
