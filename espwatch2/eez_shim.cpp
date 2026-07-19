#include "eez-flow.h"
#include "screens.h"      // for create_screen_main and other UI objects
#include "vars.h"
#include "eez_shim.h"
#include <lvgl.h>

using namespace eez;
using namespace eez::flow;

extern "C" {

// ----------------------------------------------------------------------------
// Global variable accessors
// ----------------------------------------------------------------------------
bool eez_get_global_bool(int varIndex) {
    Value v = getGlobalVariable(varIndex);
    return v.toBool();
}

void eez_set_global_bool(int varIndex, bool value) {
    setGlobalVariable(varIndex, Value(value, VALUE_TYPE_BOOLEAN));
}

void eez_set_global_string(int varIndex, const char* value) {
    Value strVal = Value::makeStringRef(value, -1, 0);
    setGlobalVariable(varIndex, strVal);
}

// ----------------------------------------------------------------------------
// Flow state
// ----------------------------------------------------------------------------
void* eez_get_flow_state(int pageIndex, int unused) {
    (void)unused;
    return getFlowState(NULL, (unsigned)pageIndex);
}

// ----------------------------------------------------------------------------
// Evaluate a text property – this is now a macro in eez_shim.h
// but we keep the underlying implementation if needed, though not used directly.
// ----------------------------------------------------------------------------
// (No need to define eez_eval_text_property; the macro uses _evalTextProperty)

// ----------------------------------------------------------------------------
// Propagate LVGL event
// ----------------------------------------------------------------------------
void eez_flow_propagate_value_lvgl_event(void* flowState, unsigned componentIndex,
                                         unsigned outputIndex, lv_event_t* event) {
    flowPropagateValueLVGLEvent(flowState, componentIndex, outputIndex, event);
}

// ----------------------------------------------------------------------------
// Update Wi-Fi networks list widget
// ----------------------------------------------------------------------------
void eez_update_wifi_networks_list(lv_obj_t* list, lv_event_cb_t selected_cb) {
    lv_obj_clean(list);
    Value networksVal = getGlobalVariable(FLOW_GLOBAL_VARIABLE_WIFI_NETWORKS);
    if (!networksVal.isArray()) return;

    auto array = networksVal.getArray();
    for (uint32_t i = 0; i < array->arraySize; i++) {
        const char* ssid = array->values[i].getString();
        if (ssid) {
            lv_obj_t* btn = lv_list_add_btn(list, NULL, ssid);
            lv_obj_add_event_cb(btn, selected_cb, LV_EVENT_CLICKED, NULL);
        }
    }
}

// ----------------------------------------------------------------------------
// Create all screens – this is required by eez-flow.cpp


} // extern "C"