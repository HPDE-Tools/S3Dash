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
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_lcd_types.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_timer.h"
#include "esp_pm.h"
#include "driver/gpio_filter.h"

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

dash_data_atomic_t dash_data_share;

std::atomic<bool> is_connected = false;
std::atomic<bool> invert_color;

void vTask_LCD(void *pvParameters);
void vTask_DataInput(void *pvParameters);
void vTask_DataMock(void *pvParameter);
void notify_cb(uint8_t *data, size_t len);
void gpio_interrupt_handler(void *args);
static void tick_timer_cb(void *arg);

enum DataSource { BLE, MOCK };
DataSource dataSource(BLE);

typedef struct {
    uint16_t displayMode;
    uint16_t oilpressureMode;
} NvsDisplayMode;

std::atomic<uint32_t> atomic_display_mode = 0;
std::atomic<bool> nvs_mode_changed = false;

uint16_t framebuffer[LCD_V_RES][LCD_H_RES];

nvs_handle_t display_mode_nvs_handle;
void set_nvs_display_mode(NvsDisplayMode mode)
{
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &display_mode_nvs_handle);
    if (err) 
    { 
        ESP_LOGE("DISPLAY MODE", "error opening nvs handle for display mode, %s", esp_err_to_name(err)); 
    }
    err = nvs_set_u32(display_mode_nvs_handle, "display_mode", *reinterpret_cast<uint32_t*>(&mode));
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
    ESP_LOGI("NVS", "DisplayMode commited to NVS memory");
}

void print_mcu_info()
{
    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), WiFi%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %" PRIi16 ", ", chip_info.revision);

    uint32_t size_flash_chip;
    esp_flash_get_size(NULL, &size_flash_chip);
    printf("%ldMB %s flash\n", size_flash_chip / (1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %ld bytes\n", esp_get_minimum_free_heap_size());
}

void configureInputOnPin(gpio_num_t pin)
{
    gpio_config_t io_conf;
    memset(&io_conf, 0, sizeof(gpio_config_t));
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.pin_bit_mask |= 1ULL << pin;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    gpio_pin_glitch_filter_config_t glitch_filter = {
        .clk_src = GLITCH_FILTER_CLK_SRC_DEFAULT,
        .gpio_num = pin,
    };
    gpio_glitch_filter_handle_t glitch_handle;
    ESP_ERROR_CHECK(gpio_new_pin_glitch_filter(&glitch_filter, &glitch_handle));
    ESP_ERROR_CHECK(gpio_glitch_filter_enable(glitch_handle));
    ESP_ERROR_CHECK(gpio_isr_handler_add(pin, gpio_interrupt_handler, (void*) pin));
}

void restoreDisplayMode()
{
    NvsDisplayMode displayMode;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &display_mode_nvs_handle);
    if (err) 
    { 
        ESP_LOGE("DISPLAY MODE", "Failed to open nvs handle for display mode, %s", esp_err_to_name(err)); 
    }

    uint32_t nvs_display_mode;
    err = nvs_get_u32(display_mode_nvs_handle, "display_mode", &nvs_display_mode);
    nvs_close(nvs_display_mode);
    if (err == ESP_OK) {
        displayMode = *reinterpret_cast<NvsDisplayMode*>(&nvs_display_mode);
        if (displayMode.displayMode > STEERING_WHEEL_MOUNT || displayMode.oilpressureMode > OILP_1) {
            ESP_LOGW("DISPLAY MODE", "Unknown display mode. Revert to default");
            displayMode.displayMode = DASH_MOUNT;
            displayMode.oilpressureMode = OILP_0;
            set_nvs_display_mode(displayMode);
        } else {
            ESP_LOGI("DISPLAY MODE", "Display mode restored");
        }
    } else {
        ESP_LOGI("DISPLAY MODE", "No persisted display mode. Persisting default mode.");
        displayMode.displayMode = DASH_MOUNT;
        displayMode.oilpressureMode = OILP_0;
        set_nvs_display_mode(displayMode);
    }
    atomic_display_mode = *reinterpret_cast<uint32_t *>(&displayMode);
}

extern "C" void app_main(void)
{
    print_mcu_info();
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
        break;
    }
    invert_color = false;
    const esp_timer_create_args_t tick_timer_args = {
        .callback = &tick_timer_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "tick_timer",
        .skip_unhandled_events = false,
    };

    esp_timer_handle_t tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&tick_timer_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, 100000));
    xTaskCreatePinnedToCore(vTask_LCD, "lcdTask", 1024 * 16, NULL, 1, NULL, CPU_CORE_1);
}

dash_data_t dash_data;
void vTask_LCD(void *pvParameters)
{

    while (3)
    {
        vTaskDelay(10/portTICK_PERIOD_MS);
        DashData::dash_data_copy(dash_data_share, dash_data);        
        DashData::clamp(&dash_data);
        uint32_t displayModeRaw = atomic_display_mode;
        NvsDisplayMode displayMode = *reinterpret_cast<NvsDisplayMode*>(&displayModeRaw);
        sprite.startWrite();
        if (!is_connected)
        //if (false)
        {
            ConnectingView(&sprite).render();
        }
        else
        {
            switch (displayMode.displayMode) { 
                case DASH_MOUNT:
                {
                    DashMountedView view(&sprite);
                    view.setOilP(static_cast<OilPressureMode>(displayMode.oilpressureMode));
                    view.setInvertColor(invert_color);
                    view.render(&dash_data);
                }
                break;
                case STEERING_WHEEL_MOUNT:
                SteeringWheelMountedView(&sprite).render(&dash_data);
                break;
            }
        }
        if (nvs_mode_changed) {
            nvs_mode_changed = false;
            set_nvs_display_mode(displayMode);
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
    bool direction_up = true;
    is_connected = true;
    while (1)
    {
        /*
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
        */
        dash_data_share.rpm = 4000;
        if (direction_up) {
            dash_data_share.oil_pressure0 += 1;
            dash_data_share.oil_pressure1 += 1;
        }
        else {
            dash_data_share.oil_pressure0 -= 1;
            dash_data_share.oil_pressure1 -= 1;
        }
        if (dash_data_share.oil_pressure0 > 60)
            direction_up = false;
        if (dash_data_share.oil_pressure0 < 10)
            direction_up = true;
        vTaskDelay(80);
    }
}

void IRAM_ATTR notify_cb(uint8_t *data, size_t len)
{
    if (len < 12)
        return;
    uint32_t can_id = *(uint32_t *)data;
    uint8_t *payload = data + 4;

    is_connected = true;
    switch (can_id)
    {
    case 0x40:
        dash_data_share.rpm = static_cast<uint16_t>(bitsToUIntLe(payload, 16, 14));
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
        dash_data_share.oil_pressure0 = static_cast<uint16_t>(bitsToUIntLe(payload, 0, 16)) / 10;
        dash_data_share.oil_pressure1 = static_cast<uint16_t>(bitsToUIntLe(payload, 16, 16)) / 10;
        break;
    }
}

void IRAM_ATTR gpio_interrupt_handler(void *args)
{
    uint32_t displayModeRaw = atomic_display_mode;
    NvsDisplayMode displayMode = *reinterpret_cast<NvsDisplayMode*>(&displayModeRaw);
    uint16_t pinNumber = (int)args;
    {
        if (pinNumber == GPIO_NUM_14)
        {
            displayMode.displayMode++;
            if (displayMode.displayMode > STEERING_WHEEL_MOUNT)
                displayMode.displayMode = DASH_MOUNT;
        }

        if (pinNumber == GPIO_NUM_0)
        {
            displayMode.oilpressureMode++;
            if (displayMode.oilpressureMode > OILP_1)
                displayMode.oilpressureMode = OILP_0;
        }
    }
    atomic_display_mode = *reinterpret_cast<uint32_t *>(&displayMode);
    nvs_mode_changed = true;
}

static void tick_timer_cb(void *arg)
{
    uint32_t displayModeRaw = atomic_display_mode;
    NvsDisplayMode displayMode = *reinterpret_cast<NvsDisplayMode*>(&displayModeRaw);
    if (dash_data_share.rpm >=3500) {
        if (displayMode.oilpressureMode == OILP_0 && dash_data_share.oil_pressure0 <35) {
            invert_color = !invert_color;
            return;
        }
        if (displayMode.oilpressureMode == OILP_1 && dash_data_share.oil_pressure1 <35) {
            invert_color = !invert_color;
            return;
        }
    }
    invert_color = false;
}