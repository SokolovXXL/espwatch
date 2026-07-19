#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <lvgl.h>
#include "esp_lcd_touch_axs5106l.h"
#include "ui.h"
#include "screens.h"
#include "eez_shim.h"
#include "vars.h"
#include <WiFi.h>
#include <time.h>
#include <SD.h>
#include <SPI.h>
#include <vector>
using namespace eez;
using namespace eez::flow; // if you also need defs_v3

struct WifiCred {
  String ssid;
  String pass;
};

// ---------- ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ДЛЯ WI-FI ----------
bool wifi_scanning = false;
bool wifi_connecting = false;
uint32_t wifi_connect_start_ms = 0;
String wifi_password = "";
bool sdCardReady = false;                 // true, если SD-карта успешно инициализирована
static const char *WIFI_CREDS_FILE = "/wifi.txt";
// Обработчик выбора сети из списка (объявлен ниже, в extern "C" блоке)
extern "C" void wifi_network_selected(lv_event_t *e);

// Функция обновления списка сетей в переменной WIFI_NETWORKS
void update_wifi_networks_list() {
    int n = WiFi.scanComplete();
    if (n <= 0) {
        eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_WIFI_NETWORKS,
            eez::Value(0, VALUE_TYPE_NULL)); // пустой массив
        eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_WIFI_ON_OFF,
            eez::Value(n == 0 ? "No networks" : "Scan failed", VALUE_TYPE_STRING));
        lv_obj_clean(objects.wifi_networks); // список тоже очищаем
        return;
    }
    // Создаём массив строк
    auto arrayValue = eez::Value::makeArrayRef(n, defs_v3::ARRAY_TYPE_STRING, 0);
    auto array = arrayValue.getArray();
    for (int i = 0; i < n; i++) {
        array->values[i] = eez::Value::makeStringRef(WiFi.SSID(i).c_str(), -1, 0);
    }
    eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_WIFI_NETWORKS, arrayValue);
    eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_WIFI_ON_OFF,
        eez::Value("Scan complete", VALUE_TYPE_STRING));
    WiFi.scanDelete(); // очищаем результаты после использования

    // Заполняем видимый виджет списка (lv_list) и вешаем клик-обработчик на каждую кнопку
    eez_update_wifi_networks_list(objects.wifi_networks, wifi_network_selected);
}

// ---------- ОБРАБОТЧИКИ СОБЫТИЙ (вызываются из UI) ----------
extern "C" {
  void wifi_scan_clicked(lv_event_t *e) {
      if (WiFi.scanNetworks(true) == WIFI_SCAN_RUNNING) {
          wifi_scanning = true;
          eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_WIFI_ON_OFF,
              eez::Value("Scanning...", VALUE_TYPE_STRING));
      } else {
          eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_WIFI_ON_OFF,
              eez::Value("Scan failed", VALUE_TYPE_STRING));
      }
  }

  // Клик по сети в списке: запоминаем SSID и открываем панель "Wifi/Password/Connect"
  void wifi_network_selected(lv_event_t *e) {
      lv_obj_t *btn = lv_event_get_target(e);
      lv_obj_t *list = lv_obj_get_parent(btn);
      const char *ssid = lv_list_get_btn_text(list, btn);
      if (!ssid) return;

      eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_SELECTED_SSID,
          eez::Value::makeStringRef(ssid, -1, 0));

      lv_textarea_set_text(objects.obj2, ""); // очищаем поле пароля от предыдущего ввода

      // Выезжает панель подключения поверх wifimenu (сетка X:0 Y:58, как задумано в screens.c)
      lv_obj_set_pos(objects.wifi_connect, 0, 58);
  }

  void wifi_connect_clicked(lv_event_t *e) {
      // SSID берём из глобальной переменной (её выставил wifi_network_selected),
      // пароль читаем прямо из текстового поля панели подключения
      Value ssidVal = eez::flow::getGlobalVariable(FLOW_GLOBAL_VARIABLE_SELECTED_SSID);
      const char* ssid = ssidVal.getString();
      const char* pass = lv_textarea_get_text(objects.obj2);

      if (!ssid || strlen(ssid) == 0) {
          eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_WIFI_ON_OFF,
              eez::Value("Select a network", VALUE_TYPE_STRING));
          return;
      }

      wifi_password = pass;

      WiFi.disconnect(true);
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, pass);
      wifi_connecting = true;
      wifi_connect_start_ms = ::millis();
      eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_WIFI_ON_OFF,
          eez::Value("Connecting...", VALUE_TYPE_STRING));

      // Прячем панель подключения обратно, статус будет виден в wifimenu (wifi_on_off)
      lv_obj_set_pos(objects.wifi_connect, -600, 58);
  }

  // Кнопка Cancel на панели подключения: просто прячем панель, ничего не меняя
  void wifi_cancel_clicked(lv_event_t *e) {
      lv_obj_set_pos(objects.wifi_connect, -600, 58);
  }

  void wifi_toggle_switch(lv_event_t *e) {
      bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
      if (on) {
          WiFi.mode(WIFI_STA);
          eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_WIFI_ON_OFF,
              eez::Value("Wi-Fi on", VALUE_TYPE_STRING));
          // Можно автоматически запустить сканирование
          wifi_scan_clicked(e);
      } else {
          WiFi.disconnect(true);
          WiFi.mode(WIFI_OFF);
          wifi_connecting = false;
          wifi_scanning = false;
          eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_WIFI_ON_OFF,
              eez::Value("Wi-Fi off", VALUE_TYPE_STRING));
          // Очищаем список сетей
          eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_WIFI_NETWORKS,
              eez::Value(0, VALUE_TYPE_NULL));
          eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_SELECTED_SSID,
              eez::Value("", VALUE_TYPE_STRING));
          lv_obj_clean(objects.wifi_networks);
          lv_obj_set_pos(objects.wifi_connect, -600, 58);
      }
  }
}
// (обработчики для menubtn и exit_wifimenu будут в screens.c)
// ---------- НАСТРОЙКИ Wi-Fi ----------
//const char* ssid     = "Na_rasslabone";
//const char* password = "9118451195";

// ---------- ПИНЫ ----------
#define Touch_I2C_SDA 18
#define Touch_I2C_SCL 19
#define Touch_RST    20
#define Touch_INT    21
#define GFX_BL       23
#define ROTATION     0

#define SD_CS_PIN   4
#define SD_SCK_PIN  1
#define SD_MOSI_PIN 2
#define SD_MISO_PIN 3
// ---------- ДИСПЛЕЙ ----------
// MISO передан явно (5-й параметр) на случай, если gfx->begin() внутри себя
// повторно вызовет SPI.begin() - тогда MISO не потеряется повторно.
Arduino_DataBus *bus = new Arduino_HWSPI(15 /* DC */, 14 /* CS */, 1 /* SCK */, 2 /* MOSI */, SD_MISO_PIN /* MISO */);
Arduino_GFX *gfx = new Arduino_ST7789(
  bus, 22 /* RST */, 0 /* rotation */, false /* IPS */,
  172 /* width */, 320 /* height */,
  34 /* col_offset1 */, 0 /* row_offset1 */,
  34 /* col_offset2 */, 0 /* row_offset2 */
);

// ---------- LVGL БУФЕР ----------
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[172 * 20];

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

// ---------- ИНИЦИАЛИЗАЦИЯ РЕГИСТРОВ ДИСПЛЕЯ ----------
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

// ---------- ПОДКЛЮЧЕНИЕ К WI-FI И ПОЛУЧЕНИЕ ВРЕМЕНИ ----------

void syncTime() {
    configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    Serial.print("Waiting for NTP time");
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    while (timeinfo.tm_year < (2020 - 1900) && ++retry < 30) {
        delay(1000);
        time(&now);
        localtime_r(&now, &timeinfo);
        Serial.print(".");
    }
    if (timeinfo.tm_year >= (2020 - 1900)) {
        Serial.println(" OK");
        char buf[9];
        strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
        eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_HMS,
        eez::Value::makeStringRef(buf, -1, 0x12345678));
    } else {
        Serial.println(" FAILED");
    }
}
/*bool syncTime() {
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

  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
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
}*/

// =================================================================
// WI-FI CREDENTIALS STORAGE НА SD-КАРТЕ (/wifi.txt)
// Формат файла, одна сеть на строку:
//   SSID1;PASSWORD1
//   SSID2;PASSWORD2
// =================================================================



// Читает и парсит /wifi.txt целиком в вектор структур WifiCred.
// Корректно обрабатывает CRLF/LF, пустые строки и строки без ';'.
static std::vector<WifiCred> wifiCreds_load() {
  std::vector<WifiCred> creds;

  if (!sdCardReady) {
    return creds;
  }

  if (!SD.exists(WIFI_CREDS_FILE)) {
    Serial.println("wifi.txt does not exist yet on SD card.");
    return creds;
  }

  File f = SD.open(WIFI_CREDS_FILE, FILE_READ);
  if (!f) {
    Serial.println("Failed to open wifi.txt for reading!");
    return creds;
  }
  Serial.println("wifi.txt opened");

  while (f.available()) {
    String line = f.readStringUntil('\n');
    // readStringUntil('\n') оставляет '\r' в конце при CRLF-файлах — trim() уберёт его,
    // а заодно и пробелы по краям.
    line.trim();
    if (line.length() == 0) {
      continue; // пустая строка / хвост файла
    }

    int sep = line.indexOf(';');
    if (sep <= 0) {
      Serial.printf("Skipping malformed line in wifi.txt: \"%s\"\n", line.c_str());
      continue;
    }

    WifiCred c;
    c.ssid = line.substring(0, sep);
    c.pass = line.substring(sep + 1);
    c.ssid.trim();
    c.pass.trim();

    if (c.ssid.length() == 0) {
      continue;
    }

    creds.push_back(c);
  }

  f.close();
  Serial.printf("Loaded %d networks\n", (int)creds.size());
  return creds;
}

// Полностью перезаписывает /wifi.txt содержимым вектора creds.
// Файл сначала удаляется, затем создаётся заново — так гарантированно
// не остаётся "хвостов" от старого содержимого (в отличие от открытия
// в FILE_WRITE поверх существующего файла, который на ESP32 не усекается).
static bool wifiCreds_saveAll(const std::vector<WifiCred> &creds) {
  if (!sdCardReady) {
    Serial.println("Cannot save wifi.txt: SD card not ready.");
    return false;
  }

  if (SD.exists(WIFI_CREDS_FILE)) {
    SD.remove(WIFI_CREDS_FILE);
  }

  File f = SD.open(WIFI_CREDS_FILE, FILE_WRITE);
  if (!f) {
    Serial.println("Failed to open wifi.txt for writing!");
    return false;
  }

  for (const auto &c : creds) {
    f.print(c.ssid);
    f.print(';');
    f.println(c.pass);
  }

  f.flush();
  f.close();

  Serial.printf("wifi.txt saved (%d networks)\n", (int)creds.size());
  return true;
}

// Добавляет новую сеть или обновляет пароль для уже существующего SSID.
// Дубликаты по SSID не допускаются.
static void wifiCreds_upsert(const String &ssid, const String &pass) {
  if (!sdCardReady) {
    Serial.println("Cannot save credentials: SD card not ready.");
    return;
  }
  if (ssid.length() == 0) {
    return;
  }

  std::vector<WifiCred> creds = wifiCreds_load();

  bool found = false;
  for (auto &c : creds) {
    if (c.ssid == ssid) {
      c.pass = pass;
      found = true;
      break;
    }
  }
  if (!found) {
    WifiCred nc;
    nc.ssid = ssid;
    nc.pass = pass;
    creds.push_back(nc);
  }

  wifiCreds_saveAll(creds);
  Serial.printf("Credentials for \"%s\" %s in wifi.txt\n",
                ssid.c_str(), found ? "updated" : "added");
}

// Сканирует эфир, сверяет найденные сети со списком, сохранённым на SD,
// и последовательно пытается подключиться, пока одна из попыток не увенчается
// успехом (или пока не закончатся кандидаты). Полностью синхронная/блокирующая
// функция — вызывается один раз при старте, до открытия основного UI.
static bool tryConnectSavedNetworks() {
  std::vector<WifiCred> creds = wifiCreds_load();
  if (creds.empty()) {
    Serial.println("No saved networks available.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);

  Serial.println("Scanning WiFi...");
  int n = WiFi.scanNetworks();
  if (n <= 0) {
    Serial.println("Scan found no networks.");
    Serial.println("No saved network available.");
    return false;
  }

  Serial.println("Found:");
  for (int i = 0; i < n; i++) {
    Serial.println(WiFi.SSID(i));
  }

  bool connected = false;
  for (const auto &cred : creds) {
    // Пробуем подключиться, только если сеть реально видна в эфире.
    bool visible = false;
    for (int i = 0; i < n; i++) {
      if (WiFi.SSID(i) == cred.ssid) {
        visible = true;
        break;
      }
    }
    if (!visible) {
      continue;
    }

    Serial.printf("Trying %s...\n", cred.ssid.c_str());
    WiFi.disconnect(true);
    delay(50);
    WiFi.begin(cred.ssid.c_str(), cred.pass.c_str());

    uint32_t start = ::millis();
    while (WiFi.status() != WL_CONNECTED && ::millis() - start < 10000) {
      delay(250);
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());

      wifi_password = cred.pass;
      eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_SELECTED_SSID,
          eez::Value(cred.ssid.c_str(), VALUE_TYPE_STRING));
      eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_WIFI_ON_OFF,
          eez::Value("Connected", VALUE_TYPE_STRING));

      connected = true;
      break;
    } else {
      Serial.println("Failed");
    }
  }

  WiFi.scanDelete();

  if (!connected) {
    Serial.println("No saved network available.");
  }

  return connected;
}

// =================================================================
// SETUP
// =================================================================
void setup(void) {
  Serial.begin(115200);
  Serial.println("LVGL + EEZ Studio UI (with NTP)");

  // --- Инициализация SPI, дисплея, тачскрина, LVGL ---
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN);
  if (!gfx->begin()) Serial.println("gfx->begin() failed!");
  lcd_reg_init();
  gfx->setRotation(ROTATION);
  #ifdef GFX_BL
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);
  #endif

  Wire.begin(Touch_I2C_SDA, Touch_I2C_SCL);
  bsp_touch_init(&Wire, Touch_RST, Touch_INT,
                 gfx->getRotation(), gfx->width(), gfx->height());

  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, sizeof(buf)/sizeof(lv_color_t));
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

  timer = timerBegin(1000);
  timerAttachInterrupt(timer, &onTimer);
  timerAlarm(timer, 1, true, 0);

  // --- !!! ИНИЦИАЛИЗАЦИЯ UI ДО ВСЕХ ОПЕРАЦИЙ С ГЛОБАЛЬНЫМИ ПЕРЕМЕННЫМИ !!! ---
  ui_init();

  // --- Теперь можно инициализировать SD и работать с Wi-Fi ---
  if (!SD.begin(SD_CS_PIN, SPI)) {
      Serial.println("SD card initialization failed!");
      sdCardReady = false;
  } else {
      sdCardReady = true;
      Serial.println("SD initialized");
      uint8_t cardType = SD.cardType();
      if (cardType == CARD_NONE) {
          Serial.println("No SD card attached.");
          sdCardReady = false;
      } else {
          Serial.print("SD Card Type: ");
          if (cardType == CARD_MMC) Serial.println("MMC");
          else if (cardType == CARD_SD) Serial.println("SDSC");
          else if (cardType == CARD_SDHC) Serial.println("SDHC");
          else Serial.println("UNKNOWN");
          Serial.printf("Card size: %llu MB\n", SD.cardSize() / (1024 * 1024));
      }
  }

  if (sdCardReady) {
      // Создать файл, если отсутствует
      if (!SD.exists(WIFI_CREDS_FILE)) {
          File f = SD.open(WIFI_CREDS_FILE, FILE_WRITE);
          if (f) { f.flush(); f.close(); }
      }
      // Попытка автоподключения (теперь после ui_init)
      if (tryConnectSavedNetworks()) {
          syncTime();
      } else {
          // Статус можно устанавливать, т.к. ui уже инициализирован
          WiFi.mode(WIFI_STA);
          eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_WIFI_ON_OFF,
              eez::Value("Not connected", VALUE_TYPE_STRING));
      }
  } else {
      Serial.println("Skipping auto Wi-Fi connect: SD card not available.");
      WiFi.mode(WIFI_STA);
      eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_WIFI_ON_OFF,
          eez::Value("No SD card", VALUE_TYPE_STRING));
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

    // Обновление времени каждую секунду
    static uint32_t last_second = 0;
    if (now - last_second >= 1000) {
      last_second = now;
      time_t rawtime = time(nullptr);
      struct tm* tm_info = localtime(&rawtime);
      // Проверяем, что время действительно установлено (год > 2020)
      if (tm_info->tm_year >= (2020 - 1900)) {
        char buf[9];
        strftime(buf, sizeof(buf), "%H:%M:%S", tm_info);
        // Обновляем глобальную переменную EEZ
        eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_HMS, eez::Value::makeStringRef(buf, -1, 0x12345678));
      }
    }

    // --- WI-FI СКАНИРОВАНИЕ ---
    if (wifi_scanning) {
        int scanResult = WiFi.scanComplete();
        if (scanResult >= 0) {
            update_wifi_networks_list();
            wifi_scanning = false;
        }
    }

    // --- WI-FI ПОДКЛЮЧЕНИЕ ---
    if (wifi_connecting) {
        if (WiFi.status() == WL_CONNECTED) {
            wifi_connecting = false;
            eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_WIFI_ON_OFF,
                eez::Value("Connected", VALUE_TYPE_STRING));
            // Сохраняем имя сети в переменную SELECTED_SSID (если нужно)
            eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_SELECTED_SSID,
                eez::Value(WiFi.SSID().c_str(), VALUE_TYPE_STRING));
            // Сохраняем/обновляем учётные данные на SD (без дублей, с заменой пароля)
            if (sdCardReady) {
              wifiCreds_upsert(WiFi.SSID(), wifi_password);
            } else {
              Serial.println("Cannot save WiFi credentials: SD card not ready.");
            }
            syncTime();
        } else if (::millis() - wifi_connect_start_ms > 15000) { // таймаут 15 сек
            wifi_connecting = false;
            eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_WIFI_ON_OFF,
                eez::Value("Connection timeout", VALUE_TYPE_STRING));
        }
    }

    ui_tick();
    lv_timer_handler();
    delay(5);
}

