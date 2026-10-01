import re

file_path = r'c:\Users\flori\esp32\HelloWorld_AMOLED\HelloWorld_AMOLED.ino'

with open(file_path, 'r', encoding='utf-8') as f:
    code = f.read()

# Add setStatus helper before updateWeatherDisplay
status_helper = """
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
"""

# Replace the direct lv_label_set_text(statusLabel, ...) calls with setStatus
code = re.sub(r'lv_label_set_text\s*\(\s*statusLabel\s*,\s*"([^"]+)"\s*\);', r'setStatus("\1");', code)
code = re.sub(r'lv_label_set_text\s*\(\s*statusLabel\s*,\s*([^\)]+)\s*\);', r'setStatus(\1);', code)

# Wrap updateWeatherDisplay with lock
update_weather_pattern = r'(void updateWeatherDisplay\(\) \{)(.*?)(^\})'
def update_weather_repl(m):
    body = m.group(2)
    # Remove the setStatus from the body since we'll wrap the rest in a lock anyway, or just wrap the whole thing
    return m.group(1) + "\n  if (example_lvgl_lock(-1)) {" + body + "    example_lvgl_unlock();\n  }\n" + m.group(3)

code = re.sub(update_weather_pattern, update_weather_repl, code, flags=re.DOTALL | re.MULTILINE)

# Rewrite createWeatherUI
beautiful_ui = """
void createWeatherUI() {
  if (example_lvgl_lock(-1)) {
    // Hintergrund (Deep Dark Blue)
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
    lv_style_set_shadow_width(&style_card, 30);
    lv_style_set_shadow_color(&style_card, lv_color_hex(0x000000));
    lv_style_set_shadow_opa(&style_card, LV_OPA_60);
    lv_style_set_shadow_ofs_y(&style_card, 10);
    lv_style_set_pad_all(&style_card, 20);

    // Main Card
    lv_obj_t * main_card = lv_obj_create(scr);
    lv_obj_add_style(main_card, &style_card, 0);
    lv_obj_set_size(main_card, 240, 380);
    lv_obj_align(main_card, LV_ALIGN_CENTER, 0, -10);
    lv_obj_clear_flag(main_card, LV_OBJ_FLAG_SCROLLABLE);

    // Stadt (Title)
    titleLabel = lv_label_create(main_card);
    lv_label_set_text(titleLabel, "Wetter");
    lv_obj_set_style_text_color(titleLabel, lv_color_hex(0xFFFFFF), 0);
    // Skalierung nutzen, falls keine grossen Fonts geladen sind
    // lv_obj_set_style_transform_zoom(titleLabel, 300, 0); 
    lv_obj_align(titleLabel, LV_ALIGN_TOP_MID, 0, 10);

    // Temperatur
    temperatureLabel = lv_label_create(main_card);
    lv_label_set_text(temperatureLabel, "--.- °C");
    lv_obj_set_style_text_color(temperatureLabel, lv_color_hex(0x4DB8FF), 0);
    lv_obj_align(temperatureLabel, LV_ALIGN_TOP_MID, 0, 60);

    // Wetterbeschreibung
    descriptionLabel = lv_label_create(main_card);
    lv_label_set_text(descriptionLabel, "Lade Wetter...");
    lv_obj_set_style_text_color(descriptionLabel, lv_color_hex(0xA0ABC0), 0);
    lv_obj_align(descriptionLabel, LV_ALIGN_TOP_MID, 0, 100);

    // Trennlinie
    lv_obj_t * line = lv_line_create(main_card);
    static lv_point_t line_points[] = { {0, 0}, {200, 0} };
    lv_line_set_points(line, line_points, 2);
    lv_obj_set_style_line_color(line, lv_color_hex(0x2C3954), 0);
    lv_obj_set_style_line_width(line, 2, 0);
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 140);

    // Details Label
    detailsLabel = lv_label_create(main_card);
    lv_label_set_text(detailsLabel, "Lade Daten...");
    lv_obj_set_style_text_color(detailsLabel, lv_color_hex(0xE2E8F0), 0);
    lv_obj_set_style_text_line_space(detailsLabel, 12, 0);
    lv_obj_align(detailsLabel, LV_ALIGN_TOP_LEFT, 10, 170);

    // Status
    statusLabel = lv_label_create(scr);
    lv_label_set_text(statusLabel, "Starte...");
    lv_obj_set_style_text_color(statusLabel, lv_color_hex(0x64748B), 0);
    lv_obj_align(statusLabel, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    example_lvgl_unlock();
  }
}
"""
code = re.sub(r'void createWeatherUI\(\) \{.*?(?=\n// =====================================================\n// SETUP)', beautiful_ui, code, flags=re.DOTALL | re.MULTILINE)

# Insert setStatus helper before updateWeatherDisplay
code = code.replace("// =====================================================\n// Hilfsfunktion: Text auf Display anzeigen\n// =====================================================", status_helper + "\n// =====================================================\n// Hilfsfunktion: Text auf Display anzeigen\n// =====================================================")

with open(file_path, 'w', encoding='utf-8') as f:
    f.write(code)
