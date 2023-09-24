#include <stdio.h>
#include <string.h>
#include <atomic>
#include "ble.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_lcd_types.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_timer.h"

#define LGFX_USE_V1
#include <LovyanGFX.h>

#include "types.h"
#include "lib.h"

class LGFX : public lgfx::LGFX_Device
{
    lgfx::Bus_Parallel8 _bus_instance;
    lgfx::Panel_ST7789 _panel_instance;
    lgfx::Light_PWM _light_instance;

public:
    LGFX(void)
    {
        {
            auto cfg = _bus_instance.config();
            cfg.pin_wr = 8;
            cfg.pin_rd = 9;
            cfg.pin_rs = 7; // D/C
            cfg.pin_d0 = 39;
            cfg.pin_d1 = 40;
            cfg.pin_d2 = 41;
            cfg.pin_d3 = 42;
            cfg.pin_d4 = 45;
            cfg.pin_d5 = 46;
            cfg.pin_d6 = 47;
            cfg.pin_d7 = 48;
            _bus_instance.config(cfg);
            _panel_instance.bus(&_bus_instance);
        }

        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs = 6;
            cfg.pin_rst = 5;
            cfg.pin_busy = -1;
            cfg.offset_rotation = 1;
            cfg.offset_x = 35;
            cfg.readable = false;
            cfg.invert = true;
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = false;
            cfg.panel_width = 170;
            cfg.panel_height = 320;

            _panel_instance.config(cfg);
        }
        setPanel(&_panel_instance);

        {
            auto cfg = _light_instance.config();

            cfg.pin_bl = 38;
            cfg.invert = false;
            cfg.freq = 22000;
            cfg.pwm_channel = 7;

            _light_instance.config(cfg);
            _panel_instance.light(&_light_instance);
        }
    }
};

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

#define EXAMPLE_LCD_H_RES 320
#define EXAMPLE_LCD_V_RES 170

#define EXAMPLE_LCD_CMD_BITS 8
#define EXAMPLE_LCD_PARAM_BITS 8

#define EXAMPLE_PSRAM_DATA_ALIGNMENT 32

#define PIN_LCD_RD 9

#define CPU_CORE_0 0
#define CPU_CORE_1 1

#define UI_SAFE_ZONE_MARGIN 2

#define UI_COLUMN_BEGIN_1 2
#define UI_COLUMN_BEGIN_2 236
#define UI_LABEL_HEIGHT 21
#define UI_ROW_HEIGHT 58
#define UI_ROW_BEGIN_1 (UI_SAFE_ZONE_MARGIN)
#define UI_ROW_BEGIN_2 (UI_SAFE_ZONE_MARGIN + UI_ROW_HEIGHT)
#define UI_ROW_BEGIN_3 (UI_SAFE_ZONE_MARGIN + UI_ROW_HEIGHT * 2)

LGFX lcd;
LGFX_Sprite sprite;
dash_data_t dash_data_share;
SemaphoreHandle_t dash_data_lock;

uint16_t red = sprite.color565(255, 0, 0);
uint16_t white = sprite.color565(255, 255, 255);
uint16_t gray = sprite.color565(190, 190, 190);

void vTask_LCD(void *pvParameters);
void vTask_DataInput(void *pvParameters);
void vTask_DataMock(void *pvParameter);
void notify_cb(uint8_t *data, size_t len);
void gpio_interrupt_handler(void *args);

enum DisplayMode { STANDARD, SEVEN_SEGMENT };
nvs_handle_t display_mode_nvs_handle;
DisplayMode displayMode(STANDARD);

class View {
    public:
        virtual void render(dash_data_t *dash_data) = 0;
};

class DefaultView: public View {
    public: 
        void render(dash_data_t *dash_data)
        {
            char digits[16];
            sprite.fillScreen(0);
            sprite.setColor(white);

            // OilP
            setupText(LABEL);
            sprite.drawString("OILP (PSI)", UI_COLUMN_BEGIN_1, UI_ROW_BEGIN_1);

            setupText(VALUE_LARGE);
            sprintf(digits, "%3d", dash_data->oil_pressure);
            sprite.drawRightString(digits, 232, UI_ROW_BEGIN_1 + UI_LABEL_HEIGHT);

            // OilT
            setupText(LABEL);
            sprite.drawString("OILT (F)", UI_COLUMN_BEGIN_2, UI_SAFE_ZONE_MARGIN);

            setupText(VALUE_SMALL);
            sprintf(digits, "%3d", dash_data->oil_temp);
            sprite.drawRightString(digits, EXAMPLE_LCD_H_RES - UI_SAFE_ZONE_MARGIN, UI_ROW_BEGIN_1 + UI_LABEL_HEIGHT);

            // ECT
            setupText(LABEL);
            sprite.drawString("ECT (F)", UI_COLUMN_BEGIN_2, UI_ROW_BEGIN_2);

            setupText(VALUE_SMALL);
            sprintf(digits, "%3d", dash_data->engine_coolant_temp);
            sprite.drawRightString(digits, EXAMPLE_LCD_H_RES - UI_SAFE_ZONE_MARGIN, UI_ROW_BEGIN_2 + UI_LABEL_HEIGHT);

            // PPS / Brake
            setupText(LABEL);
            sprite.drawString("THROTTLE /", UI_ROW_BEGIN_1, UI_ROW_BEGIN_3);
            sprite.setTextColor(red);
            sprite.drawString("BRAKE", 110, UI_ROW_BEGIN_3);
            sprite.fillRect(0, 144, transform(dash_data->throttle_per, 0, 100, 0, 232), 24, white);
            sprite.fillRect(0, 144, transform(dash_data->brake_per, 0, 100, 0, 232), 24, red);

            setupText(LABEL);
            sprite.drawString("STEER", UI_COLUMN_BEGIN_2, UI_ROW_BEGIN_3);

            // Steering
            setupText(VALUE_SMALL);
            sprintf(digits, "%3d", dash_data->steering);
            sprite.drawRightString(digits, EXAMPLE_LCD_H_RES - UI_SAFE_ZONE_MARGIN, UI_ROW_BEGIN_3 + UI_LABEL_HEIGHT);
        }

    private:
        enum UseCase { LABEL, VALUE_LARGE, VALUE_SMALL };

        void setupText(UseCase useCase)
        {
            switch (useCase) {
            case LABEL:
                sprite.setTextColor(gray);
                sprite.setFont(&fonts::FreeSans9pt7b);
                sprite.setTextSize(1);
                break;
            case VALUE_LARGE:
                sprite.setTextColor(white);
                sprite.setFont(&fonts::Font0);
                sprite.setTextSize(12.5);
                break;
            case VALUE_SMALL:
                sprite.setTextColor(white);
                sprite.setFont(&fonts::Font0);
                sprite.setTextSize(3.5);
                break;
            }
        }
};

class SevenSegmentView: public View {
    public: 
        void render(dash_data_t *dash_data)
        {
            char digits[16];
            sprite.fillScreen(0);
            sprite.setColor(white);

            // OilP
            setupText(LABEL);
            sprite.drawString("OILP (PSI)", UI_COLUMN_BEGIN_1, UI_ROW_BEGIN_1);

            setupText(VALUE_LARGE);
            sprintf(digits, "%3d", dash_data->oil_pressure);
            sprite.drawRightString(digits, 220, UI_ROW_BEGIN_1 + UI_LABEL_HEIGHT);

            // OilT
            setupText(LABEL);
            sprite.drawString("OILT (F)", UI_COLUMN_BEGIN_2, UI_SAFE_ZONE_MARGIN);

            setupText(VALUE_SMALL);
            sprintf(digits, "%3d", dash_data->oil_temp);
            sprite.drawRightString(digits, EXAMPLE_LCD_H_RES - UI_SAFE_ZONE_MARGIN, UI_ROW_BEGIN_1 + UI_LABEL_HEIGHT);

            // ECT
            setupText(LABEL);
            sprite.drawString("ECT (F)", UI_COLUMN_BEGIN_2, UI_ROW_BEGIN_2);

            setupText(VALUE_SMALL);
            sprintf(digits, "%3d", dash_data->engine_coolant_temp);
            sprite.drawRightString(digits, EXAMPLE_LCD_H_RES - UI_SAFE_ZONE_MARGIN, UI_ROW_BEGIN_2 + UI_LABEL_HEIGHT);

            // PPS / Brake
            setupText(LABEL);
            sprite.drawString("THROTTLE /", UI_ROW_BEGIN_1, UI_ROW_BEGIN_3 + 4);
            sprite.setTextColor(red);
            sprite.drawString("BRAKE", 110, UI_ROW_BEGIN_3 + 4);
            sprite.fillRect(0, 144, transform(dash_data->throttle_per, 0, 100, 0, 220), 24, white);
            sprite.fillRect(0, 144, transform(dash_data->brake_per, 0, 100, 0, 220), 24, red);

            setupText(LABEL);
            sprite.drawString("STEER", UI_COLUMN_BEGIN_2, UI_ROW_BEGIN_3);

            // Steering
            setupText(VALUE_SMALL);
            sprintf(digits, "%3d", dash_data->steering);
            sprite.drawRightString(digits, EXAMPLE_LCD_H_RES - UI_SAFE_ZONE_MARGIN, UI_ROW_BEGIN_3 + UI_LABEL_HEIGHT);
        }
    
    private:
        enum UseCase { LABEL, VALUE_LARGE, VALUE_SMALL };

        void setupText(UseCase useCase)
        {
            switch (useCase) {
            case LABEL:
                sprite.setTextColor(gray);
                sprite.setFont(&fonts::FreeSans9pt7b);
                sprite.setTextSize(1);
                break;
            case VALUE_LARGE:
                sprite.setTextColor(white);
                sprite.setFont(&fonts::Font7);
                sprite.setTextSize(2);
                break;
            case VALUE_SMALL:
                sprite.setTextColor(white);
                sprite.setFont(&fonts::Font7);
                sprite.setTextSize(.55);
                break;
            }
        }
};

DefaultView defaultView;
SevenSegmentView sevenSegmentView;

uint16_t framebuffer[170][320];

void set_nvs_display_mode(DisplayMode value)
{
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &display_mode_nvs_handle);
    if (err) 
    { 
        ESP_LOGE("DISPLAY MODE", "error opening nvs handle for display mode, %s", esp_err_to_name(err)); 
    }
    err = nvs_set_i32(display_mode_nvs_handle, "display_mode", value);
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

    int32_t nvs_display_mode;
    err = nvs_get_i32(display_mode_nvs_handle, "display_mode", &nvs_display_mode);
    switch (err) {
        case ESP_OK:
            DisplayMode found;
            found = static_cast<DisplayMode>(nvs_display_mode);
            if (found)
            {
                displayMode = found;
                ESP_LOGI("DISPLAY MODE", "Display mode restored");
            } 
            else 
            {
                ESP_LOGW("DISPLAY MODE", "Failed to restore persisted display mode");
            }
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            ESP_LOGI("DISPLAY MODE", "No persisted display mode. Persisting default mode.");
            set_nvs_display_mode(STANDARD);
            break;
        default:
            ESP_LOGE("DISPLAY MODE", "Unexpected error: (%s)", esp_err_to_name(err));
    }
    nvs_close(nvs_display_mode);
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
    sprite.setBuffer(framebuffer, 320, 170, 16);
    ESP_LOGI("S3Dash", "LCD Init complete");

    xTaskCreatePinnedToCore(vTask_LCD, "lcdTask", 1024 * 16, NULL, 1, NULL, CPU_CORE_1);
    // xTaskCreatePinnedToCore(vTask_DataMock, "dataTask", 1024*2, NULL, tskIDLE_PRIORITY, NULL, CPU_CORE_0);

    ble_init();
    set_ble_notify_callback(notify_cb);
}

void vTask_LCD(void *pvParameters)
{

    while (1)
    {
        vTaskDelay(1);

        dash_data_t dash_data;

        xSemaphoreTake(dash_data_lock, portMAX_DELAY);
        dash_data = dash_data_share;
        xSemaphoreGive(dash_data_lock);

        clamp_dash_data(&dash_data);

        switch (displayMode) { 
            case STANDARD:
            defaultView.render(&dash_data);
            break;

            case SEVEN_SEGMENT:
            sevenSegmentView.render(&dash_data);
            break;
        }

        // Function will block until all data are written.
        sprite.pushSprite(&lcd, 0, 0);
    }
}

void vTask_DataMock(void *pvParameter)
{
    srand(100);
    while (1)
    {
        int r1 = rand();
        int r2 = rand();
        int r3 = rand();
        int r4 = rand();
        int r5 = rand();
        int r6 = rand();
        xSemaphoreTake(dash_data_lock, portMAX_DELAY);

        dash_data_share.oil_pressure = r1 % 150;
        dash_data_share.oil_temp = r2 % 250;
        dash_data_share.engine_coolant_temp = r3 % 250;
        dash_data_share.throttle_per = r4 % 100;
        dash_data_share.brake_per = r5 % 100;
        dash_data_share.steering = r6 % 1800 - 900;

        xSemaphoreGive(dash_data_lock);
        vTaskDelay(5);
    }
}

void IRAM_ATTR notify_cb(uint8_t *data, size_t len)
{
    if (len < 12)
        return;
    uint32_t can_id = *(uint32_t *)data;
    uint8_t *payload = data + 4;

    xSemaphoreTake(dash_data_lock, portMAX_DELAY);
    switch (can_id)
    {
    case 0x40:
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
        dash_data_share.oil_pressure = *(payload);
        break;
    }
    xSemaphoreGive(dash_data_lock);
}

void IRAM_ATTR gpio_interrupt_handler(void *args)
{
    uint16_t pinNumber = (int)args;
    {
        if (pinNumber == GPIO_NUM_14 && displayMode != STANDARD)
        {
            displayMode = STANDARD;
            set_nvs_display_mode(STANDARD);
        }

        if (pinNumber == GPIO_NUM_0 && displayMode != SEVEN_SEGMENT)
        {
            displayMode = SEVEN_SEGMENT;
            set_nvs_display_mode(SEVEN_SEGMENT);
        }
    }
}
