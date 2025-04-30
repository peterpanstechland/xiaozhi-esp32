#ifndef SILICORE_LITE_C3_OLED_H
#define SILICORE_LITE_C3_OLED_H

#include "display/oled_display.h"

// A memory-optimized OLED display for ESP32-C3
class SilicoreC3Oled : public OledDisplay {
public:
    SilicoreC3Oled(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, 
                      int width, int height, bool mirror_x, bool mirror_y,
                      DisplayFonts fonts);
};

#endif // SILICORE_LITE_C3_OLED_H 