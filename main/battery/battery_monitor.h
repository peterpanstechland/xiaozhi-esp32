#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <driver/adc.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>

class BatteryMonitor {
private:
    adc_oneshot_unit_handle_t adc1_handle;
    adc_cali_handle_t adc1_cali_handle;
    int adc_channel;
    float voltage_divider_ratio;  // 分压比例
    
public:
    BatteryMonitor(int gpio_num, float divider_ratio);
    ~BatteryMonitor();
    
    bool Init();
    float GetVoltage();  // 返回实际电压值
    int GetBatteryPercentage();  // 返回电池百分比 (0-100)
    
private:
    bool InitializeADC();
    bool InitializeCalibration();
};

#endif // BATTERY_MONITOR_H 