#include "battery_monitor.h"
#include <esp_log.h>

static const char* TAG = "BatteryMonitor";

// 电池电压范围定义
static const float BATTERY_MAX_VOLTAGE = 4.2f;  // 锂电池满电电压
static const float BATTERY_MIN_VOLTAGE = 3.3f;  // 锂电池最低工作电压

BatteryMonitor::BatteryMonitor(int gpio_num, float divider_ratio) 
    : adc_channel(gpio_num), voltage_divider_ratio(divider_ratio) {
    adc1_handle = nullptr;
    adc1_cali_handle = nullptr;
}

BatteryMonitor::~BatteryMonitor() {
    if (adc1_handle) {
        adc_oneshot_del_unit(adc1_handle);
    }
    if (adc1_cali_handle) {
        adc_cali_delete_scheme_curve_fitting(adc1_cali_handle);
    }
}

bool BatteryMonitor::Init() {
    return InitializeADC() && InitializeCalibration();
}

bool BatteryMonitor::InitializeADC() {
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t err = adc_oneshot_new_unit(&init_config, &adc1_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC unit: %s", esp_err_to_name(err));
        return false;
    }

    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    err = adc_oneshot_config_channel(adc1_handle, (adc_channel_t)adc_channel, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel: %s", esp_err_to_name(err));
        return false;
    }
    
    return true;
}

bool BatteryMonitor::InitializeCalibration() {
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    
    esp_err_t err = adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ADC calibration scheme: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

float BatteryMonitor::GetVoltage() {
    if (!adc1_handle || !adc1_cali_handle) {
        ESP_LOGE(TAG, "ADC not initialized properly");
        return 0.0f;
    }

    int raw_value;
    int voltage_mv;
    
    esp_err_t err = adc_oneshot_read(adc1_handle, (adc_channel_t)adc_channel, &raw_value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read ADC: %s", esp_err_to_name(err));
        return 0.0f;
    }

    err = adc_cali_raw_to_voltage(adc1_cali_handle, raw_value, &voltage_mv);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to convert ADC value: %s", esp_err_to_name(err));
        return 0.0f;
    }
    
    // 转换为实际电压值（考虑分压比例）
    float actual_voltage = (voltage_mv / 1000.0f) * voltage_divider_ratio;
    ESP_LOGI(TAG, "Raw: %d, Voltage: %dmV, Actual: %.2fV", raw_value, voltage_mv, actual_voltage);
    
    return actual_voltage;
}

int BatteryMonitor::GetBatteryPercentage() {
    float voltage = GetVoltage();
    
    // 计算电池百分比
    float percentage = (voltage - BATTERY_MIN_VOLTAGE) / (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE) * 100.0f;
    
    // 限制在0-100范围内
    if (percentage > 100.0f) percentage = 100.0f;
    if (percentage < 0.0f) percentage = 0.0f;
    
    return (int)percentage;
} 