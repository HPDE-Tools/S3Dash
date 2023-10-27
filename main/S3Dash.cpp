#include <stdio.h>
#include <string.h>
#include <atomic>
#include "ble.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_lcd_types.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_timer.h"
#include "esp_pm.h"

#define LGFX_USE_V1
#include <LovyanGFX.h>

#include "color.h"
#include "dash_data.h"
#include "lcd.h"
#include "sprite.h"
#include "views/ConnectingView.h"
#include "views/DashMountedView.h"
#include "views/DisplayModeView.h"
#include "views/SteeringWheelMountedView.h"

LGFX lcd;
Sprite sprite;

#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO          (38) // Define the output GPIO
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_DUTY               (4095) // Set duty to 50%. ((2 ** 13) - 1) * 50% = 4095
#define LEDC_FREQUENCY          (5000) // Frequency in Hertz. Set frequency at 5 kHz

#define EXAMPLE_LCD_PIXEL_CLOCK_HZ (2 * 1000 * 1000)
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL 1
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL

#define EXAMPLE_PIN_NUM_DATA0 39    // 6
#define EXAMPLE_PIN_NUM_DATA1 40    // 7
#define EXAMPLE_PIN_NUM_DATA2 41    // 8
#define EXAMPLE_PIN_NUM_DATA3 42    // 9
#define EXAMPLE_PIN_NUM_DATA4 45    // 10
#define EXAMPLE_PIN_NUM_DATA5 46    // 11
#define EXAMPLE_PIN_NUM_DATA6 47    // 12
#define EXAMPLE_PIN_NUM_DATA7 48    // 13
#define EXAMPLE_PIN_NUM_PCLK 8      // 5
#define EXAMPLE_PIN_NUM_CS 6        // 3
#define EXAMPLE_PIN_NUM_DC 7        // 4
#define EXAMPLE_PIN_NUM_RST 5       // 2
#define EXAMPLE_PIN_NUM_BK_LIGHT 38 // 1
#define EXAMPLE_PIN_NUM_POWER 15

#define EXAMPLE_LCD_CMD_BITS 8
#define EXAMPLE_LCD_PARAM_BITS 8

#define EXAMPLE_PSRAM_DATA_ALIGNMENT 32

#define PIN_LCD_RD 9

#define CPU_CORE_0 0
#define CPU_CORE_1 1

dash_data_t dash_data_share;
SemaphoreHandle_t dash_data_lock;

bool is_connected = false;

void vTask_LCD(void *pvParameters);
void vTask_DataInput(void *pvParameters);
void vTask_DataMock(void *pvParameter);
void notify_cb(uint8_t *data, size_t len);
void gpio_interrupt_handler(void *args);

enum DataSource { BLE, MOCK };
DataSource dataSource(BLE);

typedef struct {
    uint16_t displayMode;
    uint16_t oilpressureMode;
} NvsDisplayMode;

NvsDisplayMode nvsMode = {
    .displayMode = static_cast<uint16_t>(DASH_MOUNT),
    .oilpressureMode = static_cast<uint16_t>(OILP_1),
};

NvsDisplayMode currentMode = {
    .displayMode = static_cast<uint16_t>(DASH_MOUNT),
    .oilpressureMode = static_cast<uint16_t>(OILP_1),
};


nvs_handle_t display_mode_nvs_handle;

uint16_t framebuffer[LCD_V_RES][LCD_H_RES];

void set_nvs_display_mode(NvsDisplayMode mode)
{
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &display_mode_nvs_handle);
    if (err) 
    { 
        ESP_LOGE("DISPLAY MODE", "error opening nvs handle for display mode, %s", esp_err_to_name(err)); 
    }
    err = nvs_set_u32(display_mode_nvs_handle, "display_mode", *(uint32_t*)&mode);
    if (err) 
    { 
        ESP_LOGE("DISPLAY MODE", "nvs set operation failed %s", esp_err_to_name(err)); 
    }
    err = nvs_commit(display_mode_nvs_handle);
    if (err) 
    { 
        ESP_LOGE("DISPLAY MODE", "nvs commit failed %s", esp_err_to_name(err)); 
    }
    nvs_close(display_mode_nvs_handle);
}

void configureInputOnPin(gpio_num_t pin)
{
    esp_err_t err = gpio_set_direction(pin, GPIO_MODE_INPUT);
    if (err)
    {
        ESP_LOGE("MAIN", "Failed to set pin %d to input mode", pin);
    }
    err = gpio_set_intr_type(pin, GPIO_INTR_POSEDGE);
    if (err)
    {
        ESP_LOGE("MAIN", "Failed to configure interrupt type for pin %d", pin);
    }
    err = gpio_pullup_dis(pin);
    if (err)
    {
        ESP_LOGE("MAIN", "Failed to configure pullup for pin %d", pin);
    }
    err = gpio_isr_handler_add(pin, gpio_interrupt_handler, (void*) pin);
    if (err)
    {
        ESP_LOGE("MAIN", "Failed to register gpio isr handler for pin %d", pin);
    }
}

void restoreDisplayMode()
{
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &display_mode_nvs_handle);
    if (err) 
    { 
        ESP_LOGE("DISPLAY MODE", "Failed to open nvs handle for display mode, %s", esp_err_to_name(err)); 
    }

    uint32_t nvs_display_mode;
    err = nvs_get_u32(display_mode_nvs_handle, "display_mode", &nvs_display_mode);
    nvs_close(nvs_display_mode);
    if (err == ESP_OK) {
        nvsMode = *(NvsDisplayMode*)&nvs_display_mode;
        if (nvsMode.displayMode > STEERING_WHEEL_MOUNT || nvsMode.oilpressureMode > OILP_1) {
            ESP_LOGW("DISPLAY MODE", "Unknown display mode. Revert to default");
            nvsMode.displayMode = DASH_MOUNT;
            nvsMode.oilpressureMode = OILP_1;
            set_nvs_display_mode(nvsMode);
        } else {
            ESP_LOGI("DISPLAY MODE", "Display mode restored");
        }
        currentMode = nvsMode;
    } else {
        ESP_LOGI("DISPLAY MODE", "No persisted display mode. Persisting default mode.");
        nvsMode.displayMode = DASH_MOUNT;
        nvsMode.oilpressureMode = OILP_1;
        set_nvs_display_mode(nvsMode);
        currentMode = nvsMode;
    }
}

extern "C" void app_main(void)
{
    esp_err_t err = gpio_install_isr_service(0);
    if (err)
    {
        ESP_LOGE("MAIN", "Failed enable gpio isr service");
    }
    err = gpio_set_direction(GPIO_NUM_15, GPIO_MODE_OUTPUT);
    if (err)
    {
        ESP_LOGE("MAIN", "Failed to set pin 15 to output");
    }
    err = gpio_set_level(GPIO_NUM_15, 1);
    if (err)
    {
        ESP_LOGE("MAIN", "Failed to set pin 15 to 1");
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    restoreDisplayMode();

    configureInputOnPin(GPIO_NUM_0);
    configureInputOnPin(GPIO_NUM_14);

    dash_data_lock = xSemaphoreCreateMutex();
    lcd.init();
    lcd.setRotation(0);
    lcd.setColorDepth(16);
    lcd.fillScreen(0);
    lcd.setBrightness(255);
    sprite.setBuffer(framebuffer, LCD_H_RES, LCD_V_RES, 16);
    ESP_LOGI("S3Dash", "LCD Init complete");

    switch (dataSource)
    {
    case BLE:
        ble_init();
        set_ble_notify_callback(notify_cb);
        break;
    case MOCK:
        xTaskCreatePinnedToCore(vTask_DataMock, "dataTask", 1024*2, NULL, tskIDLE_PRIORITY, NULL, CPU_CORE_0);
    }

    xTaskCreatePinnedToCore(vTask_LCD, "lcdTask", 1024 * 16, NULL, 1, NULL, CPU_CORE_1);
}

dash_data_t dash_data;
void vTask_LCD(void *pvParameters)
{

    while (1)
    {
        vTaskDelay(1);

        xSemaphoreTake(dash_data_lock, portMAX_DELAY);
        dash_data = dash_data_share;
        xSemaphoreGive(dash_data_lock);

        DashData::clamp(&dash_data);

        sprite.startWrite();
        if (!is_connected)
        //if (false)
        {
            ConnectingView(&sprite).render();
        }
        else
        {
            switch (currentMode.displayMode) { 
                case DASH_MOUNT:
                {
                    DashMountedView view(&sprite);
                    view.setOilP(static_cast<OilPressureMode>(currentMode.oilpressureMode));
                    view.render(&dash_data);
                }
                break;
                case STEERING_WHEEL_MOUNT:
                SteeringWheelMountedView(&sprite).render(&dash_data);
                break;
            }
        }
        if (*(uint32_t*)&currentMode != *(uint32_t*)&nvsMode) {
            nvsMode = currentMode;
            set_nvs_display_mode(nvsMode);
        }
        // Function will block until all data are written.
        sprite.pushSprite(&lcd, 0, 0);
        sprite.endWrite();
    }
}

void vTask_DataMock(void *pvParameter)
{
    vTaskDelay(300);
    srand(100);
    while (1)
    {
        is_connected = true;

        int r2 = rand();
        int r3 = rand();
        int r4 = rand();
        int r5 = rand();
        int r6 = rand();
        xSemaphoreTake(dash_data_lock, portMAX_DELAY);

        dash_data_share.rpm = (dash_data_share.rpm + (2000000 / (dash_data_share.rpm + 1)) - 160) % 7800;
        dash_data_share.oil_pressure0 = dash_data_share.rpm * 0.01 + 10;
        dash_data_share.oil_temp = r2 % 250;
        dash_data_share.engine_coolant_temp = r3 % 250;
        dash_data_share.throttle_per = r4 % 100;
        dash_data_share.brake_per = r5 % 100;
        dash_data_share.steering = r6 % 1800 - 900;

        xSemaphoreGive(dash_data_lock);
        vTaskDelay(400);
    }
}

void IRAM_ATTR notify_cb(uint8_t *data, size_t len)
{
    if (len < 12)
        return;
    uint32_t can_id = *(uint32_t *)data;
    uint8_t *payload = data + 4;

    is_connected = true;

    if (xSemaphoreTake(dash_data_lock, 1) == pdFALSE)
        return;
    switch (can_id)
    {
    case 0x40:
        dash_data_share.rpm = bitsToUIntLe(payload, 16, 14);
        dash_data_share.throttle_per = ((int)*(payload + 4)) * 100 / 255;
        break;
    case 0x138:
        dash_data_share.steering = *(int16_t *)(payload + 2) / 10;
        break;
    case 0x139:
        dash_data_share.brake_per = ((int)*(payload + 5)) * 128 / 100;
        break;
    case 0x345:
        dash_data_share.oil_temp = ((int)*(payload + 3)) - 40;
        dash_data_share.oil_temp = dash_data_share.oil_temp * 9 / 5 + 32;
        dash_data_share.engine_coolant_temp = ((int)*(payload + 4)) - 40;
        dash_data_share.engine_coolant_temp = dash_data_share.engine_coolant_temp * 9 / 5 + 32;
        break;
    case 0x662:        
        dash_data_share.oil_pressure0 = bitsToUIntLe(payload, 0, 16) / 10;
        dash_data_share.oil_pressure1 = bitsToUIntLe(payload, 16, 16) / 10;
        break;
    }
    xSemaphoreGive(dash_data_lock);
}

void IRAM_ATTR gpio_interrupt_handler(void *args)
{
    uint16_t pinNumber = (int)args;
    {
        if (pinNumber == GPIO_NUM_14)
        {
            currentMode.displayMode++;
            if (currentMode.displayMode > STEERING_WHEEL_MOUNT)
                currentMode.displayMode = DASH_MOUNT;
        }

        if (pinNumber == GPIO_NUM_0)
        {
            currentMode.oilpressureMode++;
            if (currentMode.oilpressureMode > OILP_1)
                currentMode.oilpressureMode = OILP_0;
        }
    }
}
