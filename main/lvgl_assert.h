#pragma once

#include "esp_system.h"

// There's a CONFIG_LV_ASSERT_HANDLER, but that can't be processed
// by Kconfig because there's no "literal" or "verbatim" option.
// Instead, we just define the assert header to be this file and
// redefine the LV_ASSERT_HANDLER macro.

#undef LV_ASSERT_HANDLER
#define LV_ASSERT_HANDLER esp_restart();
