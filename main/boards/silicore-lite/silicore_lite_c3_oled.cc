#include "silicore_lite_c3_oled.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>

#define TAG "SilicoreC3Oled"

// Memory-optimized OLED for ESP32-C3
SilicoreC3Oled::SilicoreC3Oled(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, 
                                int width, int height, bool mirror_x, bool mirror_y,
                                DisplayFonts fonts)
    : OledDisplay(panel_io, panel, width, height, mirror_x, mirror_y, fonts) {
    
    // Apply C3-specific memory optimizations immediately
    ESP_LOGI(TAG, "Applying ESP32-C3 specific memory optimizations");
    
    // Apply HIGH memory optimization for ESP32-C3
    // MEDIUM level still causes watchdog timeouts
    SetMemoryOptimizationLevel(MemoryOptimizationLevel::HIGH);
    
    // Extreme refresh rate reduction to save even more memory and CPU
    SetRefreshRate(10);  // 1/10 rate - extremely slow refresh rate for stability
    
    // Log initial memory status
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    ESP_LOGI(TAG, "Available heap after display init: %d bytes", free_heap);
} 