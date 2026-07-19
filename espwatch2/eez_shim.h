#ifndef EEZ_SHIM_H
#define EEZ_SHIM_H

#include <stdbool.h>
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Global variable accessors
bool eez_get_global_bool(int varIndex);
void eez_set_global_bool(int varIndex, bool value);
void eez_set_global_string(int varIndex, const char* value);

// Flow state
void* eez_get_flow_state(int pageIndex, int unused);

// Evaluate a text property – uses macro to pass __FILE__ and __LINE__
// We'll use a macro to call the underlying _evalTextProperty
#define eez_eval_text_property(flowState, comp, prop, msg) \
    _evalTextProperty(flowState, comp, prop, msg, __FILE__, __LINE__)

// Propagate LVGL event
void eez_flow_propagate_value_lvgl_event(void* flowState, unsigned componentIndex,
                                         unsigned outputIndex, lv_event_t* event);

// Update Wi-Fi networks list widget
void eez_update_wifi_networks_list(lv_obj_t* list, lv_event_cb_t selected_cb);

#ifdef __cplusplus
}
#endif

#endif // EEZ_SHIM_H