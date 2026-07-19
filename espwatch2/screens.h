#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_MAIN = 1,
    _SCREEN_ID_LAST = 1
};

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *bg;
    lv_obj_t *obj0;
    lv_obj_t *menubtn;
    lv_obj_t *menu;
    lv_obj_t *obj1;
    lv_obj_t *wifimenu;
    lv_obj_t *exit_wifimenu_cross;
    lv_obj_t *wifi_networks;
    lv_obj_t *wifi_on_off;
    lv_obj_t *wifi_on_off_switch;
    lv_obj_t *auto_connect;
    lv_obj_t *scan_button;
    lv_obj_t *wifi_connect;
    lv_obj_t *wifi_text;
    lv_obj_t *wifi_name;
    lv_obj_t *wifi_text_1;
    lv_obj_t *obj2;
    lv_obj_t *connect_button;
    lv_obj_t *cancel_button;
} objects_t;

extern objects_t objects;

void create_screen_main();
void tick_screen_main();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/