#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "display.h"
#include "../battery/battery_monitor.h"
#include <lvgl.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <mutex>

// 内存优化级别枚举
enum class MemoryOptimizationLevel {
    NONE = 0,   // No optimization
    LOW = 1,    // Low optimization
    MEDIUM = 2, // Medium optimization
    HIGH = 3    // High optimization
};

class OledDisplay : public Display {
private:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* content_left_ = nullptr;
    lv_obj_t* content_right_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;
    lv_obj_t* battery_label_ = nullptr;  // 电池图标标签
    lv_obj_t* battery_percentage_label_ = nullptr;  // 电池百分比标签
    lv_obj_t* mute_label_ = nullptr;
    lv_obj_t* low_battery_popup_ = nullptr;
    lv_obj_t* low_battery_label_ = nullptr;

    DisplayFonts fonts_;
    BatteryMonitor* battery_monitor_ = nullptr;  // 新增：电池监控对象
    
    // 图标状态变量
    const char* network_icon_ = nullptr;  // 网络图标当前状态
    
    // 内存优化相关属性
    MemoryOptimizationLevel memory_optimization_level_ = MemoryOptimizationLevel::NONE;
    bool animations_enabled_ = true;
    bool simplified_emotion_mode_ = false;
    bool display_locked_ = false;
    
    // 添加缺失的成员变量
    bool locked_ = false;         // 显示锁定状态
    uint8_t refresh_counter_ = 0; // 刷新计数器
    uint8_t refresh_rate_ = 1;    // 刷新率
    bool dirty_ = false;          // 脏标记，表示内容需要刷新
    mutable std::mutex mutex_;    // 互斥锁，用于线程安全访问

    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

    void SetupUI_128x64();
    void SetupUI_128x32();

public:
    OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width, int height, bool mirror_x, bool mirror_y,
                DisplayFonts fonts);
    ~OledDisplay();

    void UpdateBatteryDisplay();  // Moved to public section for access from Application
    void SetBatteryMonitor(BatteryMonitor* monitor);
    virtual void SetChatMessage(const char* role, const char* content) override;
    
    // 新增方法
    void Refresh();
    void SetRefreshRate(uint8_t rate);
    void EnableAnimations(bool enable);
    void SetSimplifiedEmotionMode(bool simplified);
    void SetMemoryOptimizationLevel(MemoryOptimizationLevel level);
    bool IsDisplayLocked() const;
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetNetworkIcon(const char* icon) override;
};

#endif // OLED_DISPLAY_H
