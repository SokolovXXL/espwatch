#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <lvgl.h>
#include "esp_lcd_touch_axs5106l.h"
#include "ui.h"

#include <WiFi.h>
#include <time.h>

// ---------- НАСТРОЙКИ Wi-Fi ----------
const char* ssid     = "Xiaomi_0F6D";
const char* password = "krokodil-11";

// ---------- ПИНЫ (настройте под свой проект) ----------
#define Touch_I2C_SDA 18
#define Touch_I2C_SCL 19
#define Touch_RST    20
#define Touch_INT    21

#define GFX_BL       23

#define ROTATION     0   // 0..3

// ---------- ДИСПЛЕЙ (ST7789, 172x320) ----------
Arduino_DataBus *bus = new Arduino_HWSPI(15 /* DC */, 14 /* CS */, 1 /* SCK */, 2 /* MOSI */);
Arduino_GFX *gfx = new Arduino_ST7789(
  bus, 22 /* RST */, 0 /* rotation */, false /* IPS */,
  172 /* width */, 320 /* height */,
  34 /* col_offset1 */, 0 /* row_offset1 */,
  34 /* col_offset2 */, 0 /* row_offset2 */
);

// ---------- LVGL БУФЕР ----------
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[172 * 20];          // буфер 1/16 экрана

// ---------- ТАЙМЕР ДЛЯ LVGL ----------
volatile uint32_t lv_millis = 0;
hw_timer_t *timer = NULL;

void IRAM_ATTR onTimer() {
  lv_millis++;
}

// ---------- КОЛБЭКИ LVGL ----------
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint16_t w = area->x2 - area->x1 + 1;
  uint16_t h = area->y2 - area->y1 + 1;
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)color_p, w, h);
  lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  bsp_touch_read();
  touch_data_t touch;
  if (bsp_touch_get_coordinates(&touch)) {
    data->point.x = touch.coords[0].x;
    data->point.y = touch.coords[0].y;
    data->state   = LV_INDEV_STATE_PR;
  } else {
    data->state   = LV_INDEV_STATE_REL;
  }
}

// ---------- ИНИЦИАЛИЗАЦИЯ РЕГИСТРОВ СТОКОВОГО ДИСПЛЕЯ ----------
void lcd_reg_init(void) {
  static const uint8_t init_operations[] = {
    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x11,
    END_WRITE,
    DELAY, 120,

    BEGIN_WRITE,
    WRITE_C8_D16, 0xDF, 0x98, 0x53,
    WRITE_C8_D8, 0xB2, 0x23,

    WRITE_COMMAND_8, 0xB7,
    WRITE_BYTES, 4,
    0x00, 0x47, 0x00, 0x6F,

    WRITE_COMMAND_8, 0xBB,
    WRITE_BYTES, 6,
    0x1C, 0x1A, 0x55, 0x73, 0x63, 0xF0,

    WRITE_C8_D16, 0xC0, 0x44, 0xA4,
    WRITE_C8_D8, 0xC1, 0x16,

    WRITE_COMMAND_8, 0xC3,
    WRITE_BYTES, 8,
    0x7D, 0x07, 0x14, 0x06, 0xCF, 0x71, 0x72, 0x77,

    WRITE_COMMAND_8, 0xC4,
    WRITE_BYTES, 12,
    0x00, 0x00, 0xA0, 0x79, 0x0B, 0x0A, 0x16, 0x79, 0x0B, 0x0A, 0x16, 0x82,

    WRITE_COMMAND_8, 0xC8,
    WRITE_BYTES, 32,
    0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
    0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,

    WRITE_COMMAND_8, 0xD0,
    WRITE_BYTES, 5,
    0x04, 0x06, 0x6B, 0x0F, 0x00,

    WRITE_C8_D16, 0xD7, 0x00, 0x30,
    WRITE_C8_D8, 0xE6, 0x14,
    WRITE_C8_D8, 0xDE, 0x01,

    WRITE_COMMAND_8, 0xB7,
    WRITE_BYTES, 5,
    0x03, 0x13, 0xEF, 0x35, 0x35,

    WRITE_COMMAND_8, 0xC1,
    WRITE_BYTES, 3,
    0x14, 0x15, 0xC0,

    WRITE_C8_D16, 0xC2, 0x06, 0x3A,
    WRITE_C8_D16, 0xC4, 0x72, 0x12,
    WRITE_C8_D8, 0xBE, 0x00,
    WRITE_C8_D8, 0xDE, 0x02,

    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 3,
    0x00, 0x02, 0x00,

    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 3,
    0x01, 0x02, 0x00,

    WRITE_C8_D8, 0xDE, 0x00,
    WRITE_C8_D8, 0x35, 0x00,
    WRITE_C8_D8, 0x3A, 0x05,

    WRITE_COMMAND_8, 0x2A,
    WRITE_BYTES, 4,
    0x00, 0x22, 0x00, 0xCD,

    WRITE_COMMAND_8, 0x2B,
    WRITE_BYTES, 4,
    0x00, 0x00, 0x01, 0x3F,

    WRITE_C8_D8, 0xDE, 0x02,

    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 3,
    0x00, 0x02, 0x00,

    WRITE_C8_D8, 0xDE, 0x00,
    WRITE_C8_D8, 0x36, 0x00,
    WRITE_COMMAND_8, 0x21,
    END_WRITE,

    DELAY, 10,

    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x29,
    END_WRITE
  };
  bus->batchOperation(init_operations, sizeof(init_operations));
}

// ---------- ПЕРЕМЕННАЯ ДЛЯ HMS (глобальная, доступна из EEZ) ----------
static char hms_str[9] = "00:00:00";   // "HH:MM:SS" + '\0'

// Реализация функций, объявленных в vars.h
extern "C" {
  const char* get_var_hms() {
    return hms_str;
  }

  void set_var_hms(const char* value) {
    strncpy(hms_str, value, sizeof(hms_str) - 1);
    hms_str[sizeof(hms_str) - 1] = '\0';
  }
}

// ---------- ПОДКЛЮЧЕНИЕ К WI-FI И ПОЛУЧЕНИЕ ВРЕМЕНИ ----------
bool syncTime() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(" FAILED");
    return false;
  }
  Serial.println(" OK");

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Waiting for NTP time");
  time_t now = 0;
  struct tm timeinfo = {0};
  int retry = 0;
  while (timeinfo.tm_year < (2020 - 1900) && ++retry < 20) {
    delay(1000);
    time(&now);
    localtime_r(&now, &timeinfo);
    Serial.print(".");
  }
  if (timeinfo.tm_year < (2020 - 1900)) {
    Serial.println(" FAILED");
    return false;
  }
  Serial.println(" OK");
  return true;
}

// =================================================================
// SETUP
// =================================================================
void setup(void) {
  Serial.begin(115200);
  Serial.println("LVGL + EEZ Studio UI (with NTP)");

  // --- Дисплей ---
  if (!gfx->begin()) {
    Serial.println("gfx->begin() failed!");
  }
  lcd_reg_init();
  gfx->setRotation(ROTATION);

#ifdef GFX_BL
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);
#endif

  // --- Тачскрин ---
  Wire.begin(Touch_I2C_SDA, Touch_I2C_SCL);
  bsp_touch_init(&Wire, Touch_RST, Touch_INT,
                 gfx->getRotation(),
                 gfx->width(), gfx->height());

  // --- LVGL ---
  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, sizeof(buf) / sizeof(lv_color_t));

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = gfx->width();
  disp_drv.ver_res = gfx->height();
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.rotated  = LV_DISP_ROT_NONE;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type    = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  // Таймер для LVGL (1 мс)
  timer = timerBegin(1000);
  timerAttachInterrupt(timer, &onTimer);
  timerAlarm(timer, 1, true, 0);

  // --- ИНИЦИАЛИЗАЦИЯ ИНТЕРФЕЙСА EEZ STUDIO ---
  ui_init();   // создаёт все экраны и загружает главный
  setenv("TZ", "UTC-3", 1);
  tzset();
  // --- ПОДКЛЮЧЕНИЕ К WI-FI И СИНХРОНИЗАЦИЯ ВРЕМЕНИ ---
  if (syncTime()) {
    // Первое обновление строки hms
    time_t now = time(nullptr);
    struct tm* tm_info = localtime(&now);
    char buf[9];
    strftime(buf, sizeof(buf), "%H:%M:%S", tm_info);
    set_var_hms(buf);
    Serial.printf("Initial time set: %s\n", buf);
  } else {
    Serial.println("Could not sync time, display will show 00:00:00");
  }

  Serial.println("Setup done.");
}

// =================================================================
// LOOP
// =================================================================
void loop() {
  static uint32_t last_tick = 0;
  uint32_t now = lv_millis;
  if (now != last_tick) {
    lv_tick_inc(now - last_tick);
    last_tick = now;
  }

  // Обновление времени каждую секунду (если время получено)
  static uint32_t last_second = 0;
  if (now - last_second >= 1000) {
    last_second = now;
    time_t rawtime = time(nullptr);
    struct tm* tm_info = localtime(&rawtime);
    // Проверяем, что время действительно установлено (год > 2020)
    if (tm_info->tm_year >= (2020 - 1900)) {
      char buf[9];
      strftime(buf, sizeof(buf), "%H:%M:%S", tm_info);
      set_var_hms(buf);   // обновляем глобальную переменную
    }
  }

  ui_tick();          // обновление переменных/виджетов (если нужно)
  lv_timer_handler(); // обработка событий LVGL
  delay(5);
}