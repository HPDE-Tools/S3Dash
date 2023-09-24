#ifndef S3DASH_LCD_H
#define S3DASH_LCD_H

#include <LovyanGFX.h>

#define LCD_H_RES 320
#define LCD_V_RES 170
#define UI_SAFE_ZONE_MARGIN 2

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
            cfg.panel_width = LCD_V_RES;
            cfg.panel_height = LCD_H_RES;

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

#endif
