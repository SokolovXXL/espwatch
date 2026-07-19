#ifndef EEZ_LVGL_UI_VARS_H
#define EEZ_LVGL_UI_VARS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// enum declarations

// Flow global variables

enum FlowGlobalVariables {
    FLOW_GLOBAL_VARIABLE_HMS = 0,
    FLOW_GLOBAL_VARIABLE_MENUOPEN = 1,
    FLOW_GLOBAL_VARIABLE_WIFIMENUOPEN = 2,
    FLOW_GLOBAL_VARIABLE_WIFI_ON_OFF = 3,
    FLOW_GLOBAL_VARIABLE_SELECTED_SSID = 4,
    FLOW_GLOBAL_VARIABLE_WIFI_NETWORKS = 5,
    FLOW_GLOBAL_VARIABLE_PASSWORD = 6
};

// Native global variables

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_VARS_H*/