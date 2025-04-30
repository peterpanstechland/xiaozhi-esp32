#include "oled_display.h"
#include "font_awesome_symbols.h"
#include "assets/lang_config.h"

#include <string>
#include <algorithm>
#include <cstring>  // For strcmp

#include <esp_log.h>
#include <esp_err.h>
#include <esp_lvgl_port.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>  // For vTaskDelay
#include <esp_timer.h>
#include <lvgl.h>  // This includes all LVGL functionality including area operations

#define TAG "OledDisplay"

LV_FONT_DECLARE(font_awesome_30_1);

OledDisplay::OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
    int width, int height, bool mirror_x, bool mirror_y, DisplayFonts fonts)
    : panel_io_(panel_io), panel_(panel), fonts_(fonts) {
    width_ = width;
    height_ = height;

    ESP_LOGI(TAG, "Initialize LVGL");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD screen");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * height_),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = true,
        .rotation = {
            .swap_xy = false,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    if (height_ == 64) {
        SetupUI_128x64();
    } else {
        SetupUI_128x32();
    }
    
    // 强制初始化网络图标为信号图标
    if (network_label_ != nullptr) {
        network_icon_ = FONT_AWESOME_SIGNAL_1;
        lv_label_set_text(network_label_, network_icon_);
        ESP_LOGI(TAG, "Network icon initialized");
    }
}

OledDisplay::~OledDisplay() {
    if (content_ != nullptr) {
        lv_obj_del(content_);
    }
    if (status_bar_ != nullptr) {
        lv_obj_del(status_bar_);
    }
    if (side_bar_ != nullptr) {
        lv_obj_del(side_bar_);
    }
    if (container_ != nullptr) {
        lv_obj_del(container_);
    }

    if (panel_ != nullptr) {
        esp_lcd_panel_del(panel_);
    }
    if (panel_io_ != nullptr) {
        esp_lcd_panel_io_del(panel_io_);
    }
    lvgl_port_deinit();
}

bool OledDisplay::Lock(int timeout_ms) {
    return lvgl_port_lock(timeout_ms);
}

void OledDisplay::Unlock() {
    lvgl_port_unlock();
}

void OledDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr) {
        return;
    }

    // 在HIGH优化级别下采用极简处理方式
    if (memory_optimization_level_ == MemoryOptimizationLevel::HIGH) {
        // 处理空内容
        if (content == nullptr || content[0] == '\0') {
            if (content_right_ != nullptr) {
                lv_obj_add_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
            }
            return;
        }
        
        // 使用静态字符缓冲区避免动态内存分配
        static char static_buffer[40]; // 非常小的静态缓冲区
        int max_chars = sizeof(static_buffer) - 4; // 留出空间给省略号和结束符
        
        // 简单复制字符，避免使用std::string
        int i = 0;
        for (; i < max_chars && content[i] != '\0'; i++) {
            static_buffer[i] = (content[i] == '\n') ? ' ' : content[i];
        }
        
        // 添加省略号和字符串结束符
        if (content[i] != '\0') {
            static_buffer[i++] = '.';
            static_buffer[i++] = '.';
            static_buffer[i++] = '.';
        }
        static_buffer[i] = '\0';
        
        // 极简滚动模式，只滚动一次而非循环滚动
        lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL);
        
        // 设置文本并显示
        lv_label_set_text(chat_message_label_, static_buffer);
        
        if (content_right_ != nullptr) {
            lv_obj_clear_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
        }
        
        // 使用超慢滚动速度，只设置动画时间
        lv_obj_set_style_anim_time(chat_message_label_, 8000, 0);
        
        // 让出CPU时间
        vTaskDelay(1);
        return;
    }

    // MEDIUM优化级别下使用中等复杂度处理
    if (memory_optimization_level_ == MemoryOptimizationLevel::MEDIUM) {
        // 处理空内容
        if (content == nullptr || content[0] == '\0') {
            if (content_right_ != nullptr) {
                lv_obj_add_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
            }
            return;
        }
        
        // 使用静态缓冲区
        static char medium_buffer[80];
        int max_chars = sizeof(medium_buffer) - 4;
        
        // 复制字符
        int i = 0;
        for (; i < max_chars && content[i] != '\0'; i++) {
            medium_buffer[i] = (content[i] == '\n') ? ' ' : content[i];
        }
        
        // 添加省略号和结束符
        if (content[i] != '\0') {
            medium_buffer[i++] = '.';
            medium_buffer[i++] = '.';
            medium_buffer[i++] = '.';
        }
        medium_buffer[i] = '\0';
        
        // 使用简单滚动，而非循环滚动
        lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL);
        
        // 设置文本
        lv_label_set_text(chat_message_label_, medium_buffer);
        
        if (content_right_ != nullptr) {
            lv_obj_clear_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
        }
        
        // 中速滚动，只设置动画时间
        lv_obj_set_style_anim_time(chat_message_label_, 5000, 0);
        
        vTaskDelay(1);
        return;
    }

    // 低/无优化级别使用标准处理
    std::string content_str = content;
    std::replace(content_str.begin(), content_str.end(), '\n', ' ');
    
    // 允许更长的文本和循环滚动
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    
    if (content_right_ == nullptr) {
        lv_label_set_text(chat_message_label_, content_str.c_str());
    } else {
        if (content == nullptr || content[0] == '\0') {
            lv_obj_add_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_label_set_text(chat_message_label_, content_str.c_str());
            lv_obj_clear_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void OledDisplay::SetupUI_128x64() {
    DisplayLockGuard lock(this);

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, fonts_.text_font, 0);
    lv_obj_set_style_text_color(screen, lv_color_black(), 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0);

    /* 状态栏 - 简化版 */
    status_bar_ = lv_obj_create(container_);
    lv_obj_set_size(status_bar_, LV_HOR_RES, 16);
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 2, 0);

    // 添加一个明显的黑色背景块来显示网络图标位置
    lv_obj_t* network_container = lv_obj_create(status_bar_);
    lv_obj_set_size(network_container, 16, 16);  // 稍微减小容器尺寸
    lv_obj_set_style_bg_color(network_container, lv_color_black(), 0);
    lv_obj_set_style_border_width(network_container, 0, 0);

    // 创建白色网络图标在黑色背景上 - 使用更小的字体大小
    network_label_ = lv_label_create(network_container);
    lv_obj_set_style_text_font(network_label_, fonts_.icon_font, 0);
    lv_obj_set_style_text_color(network_label_, lv_color_white(), 0);
    // 测试使用不同的图标，确认图标字体正常
    lv_label_set_text(network_label_, FONT_AWESOME_BATTERY_FULL);  // 使用电池图标来测试
    lv_obj_set_style_transform_scale(network_label_, 80, 0);  // 缩小图标到80%
    lv_obj_center(network_label_);
    network_icon_ = FONT_AWESOME_BATTERY_FULL;  // 保存测试图标
    ESP_LOGI(TAG, "Network icon changed to battery for testing");

    // 状态文本 (中间)
    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);

    // 通知标签 (中间，与状态重叠)
    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_label_set_text(notification_label_, "");
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    // 静音图标 (右侧)
    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, fonts_.icon_font, 0);

    // 电池图标 (最右侧)
    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, fonts_.icon_font, 0);
    lv_obj_set_style_text_opa(battery_label_, 180, 0);

    // 创建但隐藏百分比标签
    battery_percentage_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_percentage_label_, "");
    lv_obj_add_flag(battery_percentage_label_, LV_OBJ_FLAG_HIDDEN);

    low_battery_popup_ = lv_obj_create(screen);
    lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, fonts_.text_font->line_height * 2);
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(low_battery_popup_, lv_color_black(), 0);
    lv_obj_set_style_radius(low_battery_popup_, 10, 0);
    low_battery_label_ = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label_, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_color(low_battery_label_, lv_color_white(), 0);
    lv_obj_center(low_battery_label_);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);

    /* Content */
    content_ = lv_obj_create(container_);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_width(content_, LV_HOR_RES);
    lv_obj_set_flex_grow(content_, 1);
    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_flex_main_place(content_, LV_FLEX_ALIGN_CENTER, 0);

    // 创建左侧固定宽度的容器
    content_left_ = lv_obj_create(content_);
    lv_obj_set_size(content_left_, 32, LV_SIZE_CONTENT);  // 固定宽度32像素
    lv_obj_set_style_pad_all(content_left_, 0, 0);
    lv_obj_set_style_border_width(content_left_, 0, 0);

    emotion_label_ = lv_label_create(content_left_);
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_1, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_AI_CHIP);
    lv_obj_center(emotion_label_);
    lv_obj_set_style_pad_top(emotion_label_, 8, 0);

    // 创建右侧可扩展的容器
    content_right_ = lv_obj_create(content_);
    lv_obj_set_size(content_right_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(content_right_, 0, 0);
    lv_obj_set_style_border_width(content_right_, 0, 0);
    lv_obj_set_flex_grow(content_right_, 1);
    lv_obj_add_flag(content_right_, LV_OBJ_FLAG_HIDDEN);

    chat_message_label_ = lv_label_create(content_right_);
    lv_label_set_text(chat_message_label_, "");
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_width(chat_message_label_, width_ - 32);
    lv_obj_set_style_pad_top(chat_message_label_, 14, 0);

    // Only set up animations if they're enabled
    if (animations_enabled_) {
        // Setup animation for scrolling text
        static lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_delay(&a, 1000);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_obj_set_style_anim(chat_message_label_, &a, LV_PART_MAIN);
        lv_obj_set_style_anim_duration(chat_message_label_, lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);
    }
    
    // 全屏刷新
    if (panel_) {
        esp_lcd_panel_draw_bitmap(panel_, 0, 0, width_, height_, nullptr);
        ESP_LOGI(TAG, "Full screen refresh after UI setup");
    }
}

void OledDisplay::SetupUI_128x32() {
    DisplayLockGuard lock(this);

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, fonts_.text_font, 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_column(container_, 0, 0);

    /* Emotion label on the left side */
    content_ = lv_obj_create(container_);
    lv_obj_set_size(content_, 32, 32);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_style_border_width(content_, 0, 0);
    lv_obj_set_style_radius(content_, 0, 0);

    emotion_label_ = lv_label_create(content_);
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_1, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_AI_CHIP);
    lv_obj_center(emotion_label_);

    /* Right side */
    side_bar_ = lv_obj_create(container_);
    lv_obj_set_size(side_bar_, width_ - 32, 32);
    lv_obj_set_flex_flow(side_bar_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(side_bar_, 0, 0);
    lv_obj_set_style_border_width(side_bar_, 0, 0);
    lv_obj_set_style_radius(side_bar_, 0, 0);
    lv_obj_set_style_pad_row(side_bar_, 0, 0);

    /* 状态栏 - 简化版 */
    status_bar_ = lv_obj_create(side_bar_);
    lv_obj_set_size(status_bar_, width_ - 32, 16);
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 2, 0);

    // 添加一个明显的黑色背景块来显示网络图标位置
    lv_obj_t* network_container = lv_obj_create(status_bar_);
    lv_obj_set_size(network_container, 16, 16);  // 稍微减小容器尺寸
    lv_obj_set_style_bg_color(network_container, lv_color_black(), 0);
    lv_obj_set_style_border_width(network_container, 0, 0);

    // 创建白色网络图标在黑色背景上 - 使用更小的字体大小
    network_label_ = lv_label_create(network_container);
    lv_obj_set_style_text_font(network_label_, fonts_.icon_font, 0);
    lv_obj_set_style_text_color(network_label_, lv_color_white(), 0);
    // 测试使用不同的图标，确认图标字体正常
    lv_label_set_text(network_label_, FONT_AWESOME_BATTERY_FULL);  // 使用电池图标来测试
    lv_obj_set_style_transform_scale(network_label_, 80, 0);  // 缩小图标到80%
    lv_obj_center(network_label_);
    network_icon_ = FONT_AWESOME_BATTERY_FULL;  // 保存测试图标
    ESP_LOGI(TAG, "Network icon changed to battery for testing");

    // 状态文本 (中间)
    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);

    // 通知标签 (中间，与状态重叠)
    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_label_set_text(notification_label_, "");
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    // 静音图标 (右侧)
    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, fonts_.icon_font, 0);

    // 电池图标 (最右侧)
    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, fonts_.icon_font, 0);
    lv_obj_set_style_text_opa(battery_label_, 180, 0);

    // 创建但隐藏百分比标签
    battery_percentage_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_percentage_label_, "");
    lv_obj_add_flag(battery_percentage_label_, LV_OBJ_FLAG_HIDDEN);

    low_battery_popup_ = lv_obj_create(screen);
    lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, fonts_.text_font->line_height * 2);
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(low_battery_popup_, lv_color_black(), 0);
    lv_obj_set_style_radius(low_battery_popup_, 10, 0);
    low_battery_label_ = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label_, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_color(low_battery_label_, lv_color_white(), 0);
    lv_obj_center(low_battery_label_);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);

    chat_message_label_ = lv_label_create(side_bar_);
    lv_obj_set_size(chat_message_label_, width_ - 32, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_left(chat_message_label_, 2, 0);
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(chat_message_label_, "");

    // Only set up animations if they're enabled
    if (animations_enabled_) {
        // Setup animation for scrolling text
        static lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_delay(&a, 1000);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_obj_set_style_anim(chat_message_label_, &a, LV_PART_MAIN);
        lv_obj_set_style_anim_duration(chat_message_label_, lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);
    }
    
    // 全屏刷新
    if (panel_) {
        esp_lcd_panel_draw_bitmap(panel_, 0, 0, width_, height_, nullptr);
        ESP_LOGI(TAG, "Full screen refresh after UI setup");
    }
}

void OledDisplay::Refresh() {
    if (!locked_) {
        ESP_LOGW(TAG, "Refresh called without lock");
        return;
    }

    // Skip refresh based on refresh rate
    refresh_counter_++;
    if (refresh_counter_ < refresh_rate_) {
        return;
    }
    refresh_counter_ = 0;

    dirty_ = false;
    std::lock_guard<std::mutex> guard(mutex_);

    // For HIGH optimization level, process LVGL tasks with a timeout to avoid watchdog triggers
    if (memory_optimization_level_ == MemoryOptimizationLevel::HIGH) {
        // Set a start time
        int64_t start_time = esp_timer_get_time();
        
        // Maximum time to spend in LVGL (2ms - more aggressive)
        const int64_t max_process_time = 2000; // microseconds
        
        // Process LVGL tasks with a time limit
        lv_timer_handler();
        
        // Check if we've spent too much time
        int64_t elapsed = esp_timer_get_time() - start_time;
        if (elapsed > max_process_time) {
            ESP_LOGW(TAG, "LVGL processing took %lld μs, may cause watchdog issues", elapsed);
        }
        
        // Explicitly yield to other tasks to prevent watchdog timeouts
        vTaskDelay(1);
    } else {
        // Normal processing for other optimization levels
        lv_timer_handler();
        
        // Still yield occasionally for medium optimization level
        if (memory_optimization_level_ == MemoryOptimizationLevel::MEDIUM) {
            vTaskDelay(1);
        }
    }
    
    // Apply simplified emotion mode if enabled
    if (simplified_emotion_mode_ && emotion_label_ != nullptr) {
        // Use simpler icon/animation when in simplified mode
        lv_obj_set_style_opa(emotion_label_, LV_OPA_100, 0);
    }
    
    // 确保网络图标总是可见
    if (network_label_ != nullptr) {
        lv_obj_clear_flag(network_label_, LV_OBJ_FLAG_HIDDEN);
        if (network_icon_ == nullptr || network_icon_[0] == '\0') {
            lv_label_set_text(network_label_, FONT_AWESOME_SIGNAL_1);
        } else {
            lv_label_set_text(network_label_, network_icon_);
        }
    }
}

// Implement memory optimization controls
void OledDisplay::SetRefreshRate(uint8_t rate) {
    if (rate == 0) rate = 1; // Prevent division by zero
    refresh_rate_ = rate;
    ESP_LOGI(TAG, "Display refresh rate set to 1/%d", rate);
}

void OledDisplay::EnableAnimations(bool enable) {
    animations_enabled_ = enable;
    ESP_LOGI(TAG, "Display animations %s", enable ? "enabled" : "disabled");
}

void OledDisplay::SetSimplifiedEmotionMode(bool simplified) {
    simplified_emotion_mode_ = simplified;
    ESP_LOGI(TAG, "Simplified emotion mode %s", simplified ? "enabled" : "disabled");
}

void OledDisplay::SetMemoryOptimizationLevel(MemoryOptimizationLevel level) {
    std::lock_guard<std::mutex> guard(mutex_);
    memory_optimization_level_ = level;
    
    // Apply memory optimization settings based on level
    switch (level) {
    case MemoryOptimizationLevel::NONE:
        // No optimizations, full features
        animations_enabled_ = true;
        simplified_emotion_mode_ = false;
        refresh_rate_ = 1;  // Normal refresh rate
        break;
        
    case MemoryOptimizationLevel::LOW:
        // Light optimizations
        animations_enabled_ = true;
        simplified_emotion_mode_ = false;
        refresh_rate_ = 2;  // Slightly reduced refresh rate
        break;
        
    case MemoryOptimizationLevel::MEDIUM:
        // Medium optimizations - better balance between performance and visuals
        animations_enabled_ = false;  // Disable animations to save memory
        simplified_emotion_mode_ = false;  // But keep emotion icons for better UX
        refresh_rate_ = 3;  // Moderately reduced refresh rate
        
        // Other medium-level optimizations are handled in the individual methods
        break;
        
    case MemoryOptimizationLevel::HIGH:
        // Heavy optimizations
        animations_enabled_ = false;
        simplified_emotion_mode_ = true;
        refresh_rate_ = 5;  // Significantly reduced refresh rate
        break;
    }
    
    ESP_LOGI(TAG, "Memory optimization level set to %d", static_cast<int>(level));
}

bool OledDisplay::IsDisplayLocked() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return locked_;
}

void OledDisplay::SetEmotion(const char* emotion) {
    DisplayLockGuard lock(this);
    if (emotion_label_ == nullptr) {
        return;
    }

    const char* icon = nullptr;

    // For high memory optimization, use the simplest icons possible
    if (memory_optimization_level_ == MemoryOptimizationLevel::HIGH) {
        // Use a single icon for all emotions in high memory optimization mode
        icon = FONT_AWESOME_AI_CHIP;
    } else {
        // Use appropriate icons for different emotions based on the available symbols
        if (strcmp(emotion, "thinking") == 0) {
            icon = FONT_AWESOME_EMOJI_THINKING;
        } else if (strcmp(emotion, "happy") == 0) {
            icon = FONT_AWESOME_EMOJI_HAPPY;
        } else if (strcmp(emotion, "neutral") == 0) {
            icon = FONT_AWESOME_EMOJI_NEUTRAL;
        } else if (strcmp(emotion, "sad") == 0) {
            icon = FONT_AWESOME_EMOJI_SAD;
        } else if (strcmp(emotion, "excited") == 0) {
            icon = FONT_AWESOME_EMOJI_LAUGHING;
        } else if (strcmp(emotion, "confused") == 0) {
            icon = FONT_AWESOME_EMOJI_CONFUSED;
        } else if (strcmp(emotion, "angry") == 0) {
            icon = FONT_AWESOME_EMOJI_ANGRY;
        } else if (strcmp(emotion, "surprised") == 0) {
            icon = FONT_AWESOME_EMOJI_SURPRISED;
        } else if (strcmp(emotion, "curious") == 0) {
            icon = FONT_AWESOME_EMOJI_THINKING;
        } else if (strcmp(emotion, "sleepy") == 0) {
            icon = FONT_AWESOME_EMOJI_SLEEPY;
        } else {
            icon = FONT_AWESOME_AI_CHIP;
        }
    }

    lv_label_set_text(emotion_label_, icon);
}

void OledDisplay::SetBatteryMonitor(BatteryMonitor* monitor) {
    battery_monitor_ = monitor;
    if (battery_monitor_) {
        // 确保电池标签已经创建（在 SetupUI 中创建）
        if (battery_label_) {
            UpdateBatteryDisplay();
        }
    }
}

void OledDisplay::UpdateBatteryDisplay() {
    if (!battery_monitor_ || !battery_label_) {
        return;
    }

    DisplayLockGuard lock(this);
    int percentage = battery_monitor_->GetBatteryPercentage();
    
    // 根据电量选择显示图标
    const char* battery_icon;
    if (percentage >= 80) {
        battery_icon = FONT_AWESOME_BATTERY_FULL;
    } else if (percentage >= 60) {
        battery_icon = FONT_AWESOME_BATTERY_3;
    } else if (percentage >= 40) {
        battery_icon = FONT_AWESOME_BATTERY_2;
    } else if (percentage >= 20) {
        battery_icon = FONT_AWESOME_BATTERY_1;
    } else {
        battery_icon = FONT_AWESOME_BATTERY_EMPTY;
    }
    
    // 只更新图标，不显示百分比
    lv_label_set_text(battery_label_, battery_icon);
    
    // 如果百分比标签存在，将其隐藏起来而不仅是设置为空字符串
    if (battery_percentage_label_) {
        lv_obj_add_flag(battery_percentage_label_, LV_OBJ_FLAG_HIDDEN);
    }

    // 确保网络图标显示并正确定位
    if (network_label_ != nullptr) {
        // 如果网络图标为空，设置默认图标
        if (network_icon_ == nullptr || strlen(network_icon_) == 0) {
            network_icon_ = FONT_AWESOME_SIGNAL_1;
        }
        
        // 更新网络图标文本
        lv_label_set_text(network_label_, network_icon_);
        
        // 确保网络图标可见
        lv_obj_clear_flag(network_label_, LV_OBJ_FLAG_HIDDEN);
        
        // 获取网络图标的坐标并强制刷新该区域
        lv_area_t network_area;
        lv_obj_get_coords(network_label_, &network_area);
        
        // 扩大刷新区域，确保完全覆盖
        network_area.x1 -= 1;
        network_area.y1 -= 1;
        network_area.x2 += 1;
        network_area.y2 += 1;
        
        // 局部刷新网络图标区域
        if (panel_) {
            esp_lcd_panel_draw_bitmap(panel_, network_area.x1, network_area.y1,
                                  network_area.x2 + 1, network_area.y2 + 1,
                                  nullptr);  // 让 LVGL 处理实际的绘制
        }
    }

    // 获取电池显示区域的坐标
    lv_area_t battery_area;
    lv_obj_get_coords(battery_label_, &battery_area);

    // 扩大刷新区域，确保完全覆盖
    battery_area.x1 -= 1;
    battery_area.y1 -= 1;
    battery_area.x2 += 1;
    battery_area.y2 += 1;

    // 局部刷新显示 - 只刷新电池图标区域
    if (panel_) {
        esp_lcd_panel_draw_bitmap(panel_, battery_area.x1, battery_area.y1,
                              battery_area.x2 + 1, battery_area.y2 + 1,
                              nullptr);  // 让 LVGL 处理实际的绘制
    }
}

void OledDisplay::SetNetworkIcon(const char* icon) {
    DisplayLockGuard lock(this);
    if (network_label_ == nullptr) {
        ESP_LOGE(TAG, "Network label is null in SetNetworkIcon");
        return;
    }
    
    // 保存图标指针
    network_icon_ = icon;
    
    // 如果图标为空，使用默认图标 - 改为电池图标测试
    if (icon == nullptr || icon[0] == '\0') {
        network_icon_ = FONT_AWESOME_BATTERY_FULL;
    }
    
    ESP_LOGI(TAG, "Setting network icon to: %s", network_icon_);
    
    // 设置图标文本
    lv_label_set_text(network_label_, network_icon_);
    
    // 确保图标可见
    lv_obj_clear_flag(network_label_, LV_OBJ_FLAG_HIDDEN);
    
    // 全屏刷新确保所有元素可见
    if (panel_) {
        esp_lcd_panel_draw_bitmap(panel_, 0, 0, width_, height_, nullptr);
    }
}

