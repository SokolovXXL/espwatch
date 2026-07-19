#ifndef EEZ_LVGL_UI_EVENTS_H
#define EEZ_LVGL_UI_EVENTS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Native handlers implemented in espwatch2.ino
void wifi_network_selected(lv_event_t *e);
void wifi_connect_clicked(lv_event_t *e);
void wifi_cancel_clicked(lv_event_t *e);
void wifi_scan_clicked(lv_event_t *e);
void wifi_toggle_switch(lv_event_t *e);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_EVENTS_H*/