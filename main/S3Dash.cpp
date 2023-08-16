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

LGFX lcd;
LGFX_Sprite sprite;

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

#define UI_COLUMN_BEGIN_1 2
#define UI_COLUMN_BEGIN_2 248
#define UI_LABEL_HEIGHT 21
#define UI_MARGIN_VERTICAL 2
#define UI_ROW_HEIGHT 60
#define UI_ROW_BEGIN_1 (UI_MARGIN_VERTICAL)
#define UI_ROW_BEGIN_2 (UI_MARGIN_VERTICAL + UI_ROW_HEIGHT)
#define UI_ROW_BEGIN_3 (UI_MARGIN_VERTICAL + UI_ROW_HEIGHT * 2)

uint16_t red = sprite.color565(255, 0, 0);
uint16_t white = sprite.color565(255, 255, 255);
uint16_t gray = sprite.color565(190, 190, 190);

void vTask_LCD(void *pvParameters);
void vTask_DataInput(void *pvParameters);
void vTask_DataMock(void *pvParameter);

dash_data_t dash_data_share;
SemaphoreHandle_t dash_data_lock;

void notify_cb(uint8_t *data, size_t len);

void set_label()
{
    sprite.setTextColor(gray);
    sprite.setFont(&fonts::FreeSans9pt7b);
    sprite.setTextSize(1);
}

void set_value()
{
    sprite.setTextColor(white);
    sprite.setFont(&fonts::Font0);
    sprite.setTextSize(3.5);
}

void set_value_large()
{
    sprite.setTextColor(white);
    sprite.setFont(&fonts::Font0);
    sprite.setTextSize(13);
}

void draw_sprite()
{
    dash_data_t dash_data;
    xSemaphoreTake(dash_data_lock, portMAX_DELAY);
    dash_data = dash_data_share;
    xSemaphoreGive(dash_data_lock);

    clamp_dash_data(&dash_data);

    char digits[16];
    sprite.fillScreen(0);
    sprite.setColor(white);

    // OilP
    set_label();
    sprite.drawString("Oil P (PSI)", UI_COLUMN_BEGIN_1, UI_ROW_BEGIN_1);

    set_value_large();
    sprintf(digits, "%3d", dash_data.oil_pressure);
    sprite.drawString(digits, UI_COLUMN_BEGIN_1, UI_ROW_BEGIN_1 + UI_LABEL_HEIGHT);

    // OilT
    set_label();
    sprite.drawString("Oil T (F)", UI_COLUMN_BEGIN_2, UI_MARGIN_VERTICAL);

    set_value();
    sprintf(digits, "%3d", dash_data.oil_temp);
    sprite.drawString(digits, UI_COLUMN_BEGIN_2, UI_ROW_BEGIN_1 + UI_LABEL_HEIGHT);

    // ECT
    set_label();
    sprite.drawString("ECT (F)", UI_COLUMN_BEGIN_2, UI_ROW_BEGIN_2);

    set_value();
    sprintf(digits, "%3d", dash_data.engine_coolant_temp);
    sprite.drawString(digits, UI_COLUMN_BEGIN_2, UI_ROW_BEGIN_2 + UI_LABEL_HEIGHT);

    // PPS / Brake
    set_label();
    sprite.drawString("Throttle /", UI_ROW_BEGIN_1, UI_ROW_BEGIN_3);
    sprite.setTextColor(red);
    sprite.drawString("Brake", 78, UI_ROW_BEGIN_3);
    sprite.fillRect(0, 143, dash_data.throttle_per * 2.21, 24, white);
    sprite.fillRect(0, 143, dash_data.brake_per * 2.21, 24, red);

    set_label();
    sprite.drawString("Steering", UI_COLUMN_BEGIN_2, UI_ROW_BEGIN_3);

    // Steering
    set_value();
    sprintf(digits, "%3d", dash_data.steering);
    sprite.drawString(digits, UI_COLUMN_BEGIN_2, UI_ROW_BEGIN_3 + UI_LABEL_HEIGHT);
}

uint16_t framebuffer[170][320];
std::atomic<size_t> trans_done_calls;

static void tick_timer_cb(void *arg)
{
    size_t data = trans_done_calls;
    ESP_LOGI("FPS", "trans_done_calls: %zu", data);
    trans_done_calls = 0;
}

extern "C" void app_main(void)
{
    esp_err_t err = gpio_set_direction(GPIO_NUM_15, GPIO_MODE_OUTPUT);
    if (err)
    {
        ESP_LOGE("MAIN", "failed to set pin 15 to output");
    }
    err = gpio_set_level(GPIO_NUM_15, 1);
    if (err)
    {
        ESP_LOGE("MAIN", "failed to set pin 15 to 1");
    }
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    dash_data_lock = xSemaphoreCreateMutex();
    lcd.init();
    lcd.setRotation(0);
    lcd.setColorDepth(16);
    lcd.fillScreen(0);
    lcd.setBrightness(255);
    sprite.setBuffer(framebuffer, 320, 170, 16);
    ESP_LOGI("S3Dash", "LCD Init complete");
    const esp_timer_create_args_t tick_timer_args = {
        .callback = &tick_timer_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "tick_timer",
        .skip_unhandled_events = false,
    };
    esp_timer_handle_t tick_timer = NULL;

    ESP_ERROR_CHECK(esp_timer_create(&tick_timer_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, 1000000));
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
        draw_sprite();
        // Function will block until all data are written.
        sprite.pushSprite(&lcd, 0, 0);
        trans_done_calls++;
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

void notify_cb(uint8_t *data, size_t len)
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
