#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "lcd_bsp.h"
#include "FT3168.h"
#include <lvgl.h>

// =====================================================
// WLAN
// =====================================================
const char* WIFI_SSID = "---";
const char* WIFI_PASSWORD = "---";

// =====================================================
// OpenWeatherMap
// =====================================================
const char* API_KEY = "---";

// Standorte
float current_lat = 46.8499; // Chur Standard
float current_lon = 9.5329;

bool fetch_weather_flag = false;

// =====================================================
// LVGL Elemente
// =====================================================
lv_obj_t *titleLabel;
lv_obj_t *temperatureLabel;
lv_obj_t *descriptionLabel;
lv_obj_t *detailsLabel;
lv_obj_t *statusLabel;
lv_obj_t *btn_chur;
lv_obj_t *btn_rankweil;
lv_obj_t *temp_meter;
lv_meter_indicator_t * temp_indic;
lv_obj_t *day_night_led;
lv_obj_t *dayNightLabel;
lv_obj_t *weather_icon_label;

// =====================================================
// Wetterdaten
// =====================================================
float temperature = 0;
float feelsLike = 0;
int humidity = 0;
int pressure = 0;
float windSpeed = 0;
String description = "";
String city = "";
bool is_day = true;
String weather_icon = "";

// =====================================================
// Hilfsfunktion: Status aktualisieren (Thread-Safe)
// =====================================================
void setStatus(String text) {
  if (example_lvgl_lock(-1)) {
    if (statusLabel != NULL) {
      lv_label_set_text(statusLabel, text.c_str());
    }
    example_lvgl_unlock();
  }
}

// =====================================================
// Button Callback
// =====================================================
static void btn_event_cb(lv_event_t * e) {
  lv_obj_t * btn = lv_event_get_target(e);
  if (btn == btn_chur) {
    current_lat = 46.8499;
    current_lon = 9.5329;
  } else if (btn == btn_rankweil) {
    current_lat = 47.2714; // Rankweil
    current_lon = 9.6409;
  }
  
  // Set flag to fetch in main loop
  fetch_weather_flag = true;
}

// =====================================================
// Swipe Callback
// =====================================================
static void swipe_event_cb(lv_event_t * e) {
  lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
  
  if (dir == LV_DIR_LEFT || dir == LV_DIR_RIGHT) {
    // Wenn wir in Chur sind, gehe zu Rankweil. Ansonsten zurück zu Chur.
    if (current_lat < 47.0) { // Chur ist bei ~46.8, Rankweil bei ~47.2
      current_lat = 47.2714;
      current_lon = 9.6409;
    } else {
      current_lat = 46.8499;
      current_lon = 9.5329;
    }
    fetch_weather_flag = true;
  }
}

// =====================================================
// Display Update
// =====================================================
void updateWeatherDisplay() {
  if (example_lvgl_lock(-1)) {
    char buffer[64];

    // Stadt
    lv_label_set_text(titleLabel, city.c_str());

    // Temperatur Text
    snprintf(buffer, sizeof(buffer), "%.1f °C", temperature);
    lv_label_set_text(temperatureLabel, buffer);

    // Temperatur Meter aktualisieren
    lv_meter_set_indicator_end_value(temp_meter, temp_indic, (int32_t)temperature);

    // Wetterbeschreibung & Icon
    String icon_text = "";
    if (weather_icon.indexOf("01") >= 0) icon_text = "KLAR";
    else if (weather_icon.indexOf("02") >= 0 || weather_icon.indexOf("03") >= 0 || weather_icon.indexOf("04") >= 0) icon_text = "WOLKIG";
    else if (weather_icon.indexOf("09") >= 0 || weather_icon.indexOf("10") >= 0) icon_text = "REGEN";
    else if (weather_icon.indexOf("11") >= 0) icon_text = "GEWITTER";
    else if (weather_icon.indexOf("13") >= 0) icon_text = "SCHNEE";
    else if (weather_icon.indexOf("50") >= 0) icon_text = "NEBEL";
    else icon_text = weather_icon;

    lv_label_set_text(weather_icon_label, icon_text.c_str());
    lv_label_set_text(descriptionLabel, description.c_str());

    // Details (fix für ue)
    String details =
      "Gefuehlt: " + String(feelsLike, 1) + " °C\n"
      "Luftfeuchte: " + String(humidity) + " %\n"
      "Wind: " + String(windSpeed, 1) + " m/s";

    lv_label_set_text(detailsLabel, details.c_str());

    // Tag / Nacht LED
    if (is_day) {
      lv_led_set_color(day_night_led, lv_color_hex(0xFFDD00)); // Gelb für Tag
      lv_led_set_brightness(day_night_led, 255);
      lv_led_on(day_night_led);
      lv_label_set_text(dayNightLabel, "Tag");
    } else {
      lv_led_set_color(day_night_led, lv_color_hex(0x1E3A8A)); // Dunkelblau für Nacht
      lv_led_set_brightness(day_night_led, 150);
      lv_led_on(day_night_led);
      lv_label_set_text(dayNightLabel, "Nacht");
    }

    lv_label_set_text(statusLabel, "Aktualisiert");
    example_lvgl_unlock();
  }
}

// =====================================================
// Wetter laden
// =====================================================
void getWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    setStatus("WiFi nicht verbunden");
    return;
  }

  setStatus("Lade Wetter...");
  Serial.println("Hole Wetterdaten...");

  String url =
    "https://api.openweathermap.org/data/2.5/weather?"
    "lat=" + String(current_lat, 4) +
    "&lon=" + String(current_lon, 4) +
    "&units=metric"
    "&lang=de"
    "&appid=" + String(API_KEY);

  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      setStatus("JSON Fehler");
      http.end();
      return;
    }

    temperature = doc["main"]["temp"] | 0.0;
    feelsLike = doc["main"]["feels_like"] | 0.0;
    humidity = doc["main"]["humidity"] | 0;
    pressure = doc["main"]["pressure"] | 0;
    windSpeed = doc["wind"]["speed"] | 0.0;

    const char* weatherDescription = doc["weather"][0]["description"] | "";
    const char* weatherCity = doc["name"] | "";
    const char* weatherIconStr = doc["weather"][0]["icon"] | "01d";

    description = String(weatherDescription);
    city = String(weatherCity);
    weather_icon = String(weatherIconStr);
    is_day = weather_icon.endsWith("d");

    updateWeatherDisplay();
  }
  else {
    String errorText = "HTTP Fehler: " + String(httpCode);
    setStatus(errorText);
  }
  http.end();
}

// =====================================================
// Display erstellen
// =====================================================
void createWeatherUI() {
  if (example_lvgl_lock(-1)) {
    // Hintergrund
    lv_obj_t * scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0B1021), LV_PART_MAIN);
    
    // Style fuer ein schickes Panel (Rounded Card)
    static lv_style_t style_card;
    lv_style_init(&style_card);
    lv_style_set_radius(&style_card, 24);
    lv_style_set_bg_color(&style_card, lv_color_hex(0x1A2235));
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_border_color(&style_card, lv_color_hex(0x2C3954));
    lv_style_set_pad_all(&style_card, 10);

    // Main Card
    lv_obj_t * main_card = lv_obj_create(scr);
    lv_obj_add_style(main_card, &style_card, 0);
    lv_obj_set_size(main_card, 260, 360);
    lv_obj_align(main_card, LV_ALIGN_CENTER, 0, 30);
    lv_obj_clear_flag(main_card, LV_OBJ_FLAG_SCROLLABLE);

    // Gesten aktivieren (Wischen / Swipen)
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scr, swipe_event_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(main_card, swipe_event_cb, LV_EVENT_GESTURE, NULL);

    // Buttons
    btn_chur = lv_btn_create(scr);
    lv_obj_set_size(btn_chur, 110, 40);
    lv_obj_align(btn_chur, LV_ALIGN_TOP_LEFT, 15, 10);
    lv_obj_t * lbl_chur = lv_label_create(btn_chur);
    lv_label_set_text(lbl_chur, "Chur");
    lv_obj_center(lbl_chur);
    lv_obj_add_event_cb(btn_chur, btn_event_cb, LV_EVENT_CLICKED, NULL);

    btn_rankweil = lv_btn_create(scr);
    lv_obj_set_size(btn_rankweil, 110, 40);
    lv_obj_align(btn_rankweil, LV_ALIGN_TOP_RIGHT, -15, 10);
    lv_obj_t * lbl_rank = lv_label_create(btn_rankweil);
    lv_label_set_text(lbl_rank, "Rankweil");
    lv_obj_center(lbl_rank);
    lv_obj_add_event_cb(btn_rankweil, btn_event_cb, LV_EVENT_CLICKED, NULL);

    // Stadt (Title)
    titleLabel = lv_label_create(main_card);
    lv_label_set_text(titleLabel, "Wetter");
    lv_obj_set_style_text_color(titleLabel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(titleLabel, LV_ALIGN_TOP_MID, 0, 5);

    // Temperatur Meter (Gauge)
    temp_meter = lv_meter_create(main_card);
    lv_obj_set_size(temp_meter, 150, 150);
    lv_obj_align(temp_meter, LV_ALIGN_TOP_MID, 0, 35);
    
    // Style clean up
    lv_obj_set_style_pad_all(temp_meter, 10, 0);

    lv_meter_scale_t * scale = lv_meter_add_scale(temp_meter);
    lv_meter_set_scale_ticks(temp_meter, scale, 31, 2, 8, lv_color_hex(0x64748B));
    lv_meter_set_scale_major_ticks(temp_meter, scale, 10, 4, 15, lv_color_hex(0xFFFFFF), 15);
    lv_meter_set_scale_range(temp_meter, scale, -20, 40, 270, 135);

    temp_indic = lv_meter_add_arc(temp_meter, scale, 8, lv_color_hex(0x4DB8FF), 0);
    
    // Temperatur Label unterhalb des Meters
    temperatureLabel = lv_label_create(main_card);
    lv_label_set_text(temperatureLabel, "--.-");
    lv_obj_set_style_text_color(temperatureLabel, lv_color_hex(0x4DB8FF), 0);
    lv_obj_align(temperatureLabel, LV_ALIGN_TOP_MID, 0, 185);

    // Tag / Nacht LED
    day_night_led = lv_led_create(main_card);
    lv_obj_set_size(day_night_led, 15, 15);
    lv_obj_align(day_night_led, LV_ALIGN_TOP_RIGHT, -5, 5);
    lv_led_off(day_night_led);

    // Tag / Nacht Text
    dayNightLabel = lv_label_create(main_card);
    lv_label_set_text(dayNightLabel, "Tag");
    lv_obj_set_style_text_color(dayNightLabel, lv_color_hex(0xA0ABC0), 0);
    lv_obj_align_to(dayNightLabel, day_night_led, LV_ALIGN_OUT_LEFT_MID, -8, 0);

    // Icon & Wetterbeschreibung
    weather_icon_label = lv_label_create(main_card);
    lv_label_set_text(weather_icon_label, "");
    lv_obj_set_style_text_color(weather_icon_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(weather_icon_label, LV_ALIGN_TOP_MID, 0, 210);

    descriptionLabel = lv_label_create(main_card);
    lv_label_set_text(descriptionLabel, "Lade Wetter...");
    lv_obj_set_style_text_color(descriptionLabel, lv_color_hex(0xA0ABC0), 0);
    lv_obj_align(descriptionLabel, LV_ALIGN_TOP_MID, 0, 230);

    // Trennlinie
    lv_obj_t * line = lv_line_create(main_card);
    static lv_point_t line_points[] = { {0, 0}, {200, 0} };
    lv_line_set_points(line, line_points, 2);
    lv_obj_set_style_line_color(line, lv_color_hex(0x2C3954), 0);
    lv_obj_set_style_line_width(line, 2, 0);
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 255);

    // Details Label
    detailsLabel = lv_label_create(main_card);
    lv_label_set_text(detailsLabel, "Lade Daten...");
    lv_obj_set_style_text_color(detailsLabel, lv_color_hex(0xE2E8F0), 0);
    lv_obj_set_style_text_line_space(detailsLabel, 10, 0);
    lv_obj_align(detailsLabel, LV_ALIGN_TOP_LEFT, 15, 275);

    // Status
    statusLabel = lv_label_create(scr);
    lv_label_set_text(statusLabel, "Starte...");
    lv_obj_set_style_text_color(statusLabel, lv_color_hex(0x64748B), 0);
    lv_obj_align(statusLabel, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    example_lvgl_unlock();
  }
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Touch_Init();
  lcd_lvgl_Init();
  set_amoled_backlight(0xff);

  createWeatherUI();

  Serial.println("\nConnecting to WiFi...");
  setStatus("Verbinde WiFi...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  setStatus("WiFi verbunden");

  getWeather();
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  if (fetch_weather_flag) {
    fetch_weather_flag = false;
    getWeather();
  }
  
  lv_timer_handler();
  delay(5);
}