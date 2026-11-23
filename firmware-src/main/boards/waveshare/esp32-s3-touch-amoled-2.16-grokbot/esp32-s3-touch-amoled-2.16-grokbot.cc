#include "wifi_board.h"
#include "display/grokbot_display.h"
#include "esp_lcd_co5300.h"

#include "codecs/box_audio_codec.h"
#include "application.h"
#include "button.h"
#include "led/single_led.h"
#include "mcp_server.h"
#include "config.h"
#include "power_save_timer.h"
#include "axp2101.h"
#include "i2c_device.h"

#include <atomic>

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_master.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_io_expander_tca9554.h"
#include "settings.h"

#include <esp_lcd_touch_cst9217.h>
#include <esp_lvgl_port.h>
#include <lvgl.h>

#define TAG "WsAmoled216Grokbot"

// ---------------------------------------------------------------------------
// QMI8658 6-axis IMU on the shared I2C bus (0x6B, fallback 0x6A).
// Accelerometer-only driver used for automatic screen rotation. Adapted from
// the CyberVoCat board implementation in this firmware tree.
// ---------------------------------------------------------------------------
namespace Motion {
struct AccelSample {
    int x = 0;
    int y = 0;
    int z = 0;
};  // milligravity

static i2c_master_dev_handle_t device = nullptr;

static esp_err_t ReadReg(uint8_t reg, uint8_t* data, size_t length) {
    return i2c_master_transmit_receive(device, &reg, 1, data, length, 50);
}
static esp_err_t WriteReg(uint8_t reg, uint8_t value) {
    const uint8_t data[] = {reg, value};
    return i2c_master_transmit(device, data, sizeof(data), 50);
}
static int16_t Decode(const uint8_t* data) {
    return static_cast<int16_t>(static_cast<uint16_t>(data[0]) |
                                (static_cast<uint16_t>(data[1]) << 8));
}
static void Deinitialize() {
    if (device != nullptr) {
        i2c_master_bus_rm_device(device);
        device = nullptr;
    }
}
static esp_err_t Initialize(i2c_master_bus_handle_t bus) {
    if (device != nullptr) {
        return ESP_OK;
    }
    esp_err_t added = ESP_ERR_NOT_FOUND;
    for (uint8_t address : {static_cast<uint8_t>(IMU_QMI8658_ADDR_ALT),
                            static_cast<uint8_t>(IMU_QMI8658_ADDR_PRIMARY)}) {
        if (i2c_master_probe(bus, address, 50) != ESP_OK) {
            continue;
        }
        i2c_device_config_t config = {};
        config.device_address = address;
        config.scl_speed_hz = 400000;
        added = i2c_master_bus_add_device(bus, &config, &device);
        break;
    }
    if (added != ESP_OK) {
        return added;
    }
    uint8_t id = 0;
    esp_err_t ret = ReadReg(0x00, &id, 1);
    if (ret == ESP_OK && id != 0x05) {
        ret = ESP_ERR_NOT_FOUND;
    }
    // QMI8658A: auto-increment, +/-8 g at 250 Hz, accelerometer enabled.
    if (ret == ESP_OK) ret = WriteReg(0x02, 0x60);
    if (ret == ESP_OK) ret = WriteReg(0x03, 0x25);
    if (ret == ESP_OK) ret = WriteReg(0x08, 0x01);
    if (ret != ESP_OK) {
        Deinitialize();
    }
    return ret;
}
static bool ReadAccel(AccelSample& sample) {
    if (device == nullptr) {
        return false;
    }
    uint8_t data[6] = {};
    if (ReadReg(0x35, data, sizeof(data)) != ESP_OK) {
        return false;
    }
    sample.x = static_cast<int>(Decode(data)) * 1000 / 4096;
    sample.y = static_cast<int>(Decode(data + 2)) * 1000 / 4096;
    sample.z = static_cast<int>(Decode(data + 4)) * 1000 / 4096;
    return true;
}
}  // namespace Motion

class Pmic : public Axp2101 {
public:
    Pmic(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : Axp2101(i2c_bus, addr) {
        WriteReg(0x22, 0b110); // PWRON > OFFLEVEL as POWEROFF Source enable
        WriteReg(0x27, 0x10);  // hold 4s to power off

        // Disable All DCs but DC1
        WriteReg(0x80, 0x01);
        // Disable All LDOs
        WriteReg(0x90, 0x00);
        WriteReg(0x91, 0x00);

        // Set DC1 to 3.3V
        WriteReg(0x82, (3300 - 1500) / 100);

        // Set ALDO1 to 3.3V
        WriteReg(0x92, (3300 - 500) / 100);

        // Enable ALDO1(MIC)
        WriteReg(0x90, 0x01);

        WriteReg(0x64, 0x02); // CV charger voltage setting to 4.1V

        WriteReg(0x61, 0x02); // set Main battery precharge current to 50mA
        WriteReg(0x62, 0x08); // set Main battery charger current to 400mA ( 0x08-200mA, 0x09-300mA, 0x0A-400mA )
        WriteReg(0x63, 0x01); // set Main battery term charge current to 25mA
    }
};

#define LCD_OPCODE_WRITE_CMD (0x02ULL)
#define LCD_OPCODE_READ_CMD (0x03ULL)
#define LCD_OPCODE_WRITE_COLOR (0x32ULL)

static const co5300_lcd_init_cmd_t vendor_specific_init[] = {
    {0x11, (uint8_t[]){0x00}, 0, 600}, // Sleep out

    {0xFE, (uint8_t[]){0x20}, 1, 0},
    {0x19, (uint8_t[]){0x10}, 1, 0},
    {0x1C, (uint8_t[]){0xA0}, 1, 0},

    {0xFE, (uint8_t[]){0x00}, 1, 0},
    {0xC4, (uint8_t[]){0x80}, 1, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 0},
    {0x51, (uint8_t[]){0xFF}, 1, 0},
    {0x63, (uint8_t[]){0xFF}, 1, 0},
    {0x2A, (uint8_t[]){0x00, 0x00, 0x01, 0xDF}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xDF}, 4, 0},
    {0x36, (uint8_t[]){0xA0}, 1, 0},
    {0x29, (uint8_t[]){0x00}, 0, 600},
};

class CustomBacklight : public Backlight {
public:
    CustomBacklight(esp_lcd_panel_io_handle_t panel_io) : Backlight(), panel_io_(panel_io) {}

protected:
    esp_lcd_panel_io_handle_t panel_io_;

    virtual void SetBrightnessImpl(uint8_t brightness) override {
        auto display = Board::GetInstance().GetDisplay();
        DisplayLockGuard lock(display);
        uint8_t data[1] = {((uint8_t)((255*  brightness) / 100))};
        int lcd_cmd = 0x51;
        lcd_cmd &= 0xff;
        lcd_cmd <<= 8;
        lcd_cmd |= LCD_OPCODE_WRITE_CMD << 24;
        esp_lcd_panel_io_tx_param(panel_io_, lcd_cmd, &data, sizeof(data));
    }
};

class WaveshareEsp32s3TouchAMOLED216GrokBot : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Pmic* pmic_ = nullptr;
    Button boot_button_;
    GrokbotDisplay* display_;
    CustomBacklight* backlight_;
    esp_io_expander_handle_t io_expander = NULL;
    PowerSaveTimer* power_save_timer_;
    lv_indev_t* touch_indev_ = nullptr;
    TaskHandle_t rotate_task_handle_ = nullptr;
    std::atomic<bool> screen_locked_{false};
    bool battery_discharging_ = false;
    lv_display_rotation_t applied_rotation_ = LV_DISPLAY_ROTATION_0;
    Motion::AccelSample last_accel_;
    int volume_before_mute_ = -1;  // -1 = not muted; otherwise restored volume
    lv_obj_t* rotation_curtain_ = nullptr;
    int pending_rotation_ = -1;
    bool rotation_busy_ = false;

    void UpdatePowerSaveTimer() {
        // Discharging enables the idle timer; a locked screen must suspend it so
        // it cannot raise the brightness or power the board off while locked.
        power_save_timer_->SetEnabled(battery_discharging_ && !screen_locked_.load());
    }

    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(-1, 60, 300);
        power_save_timer_->OnEnterSleepMode([this]() {
            if (screen_locked_.load()) return;
            GetDisplay()->SetPowerSaveMode(true);
            GetBacklight()->SetBrightness(20); });
        power_save_timer_->OnExitSleepMode([this]() {
            if (screen_locked_.load()) return;
            GetDisplay()->SetPowerSaveMode(false);
            GetBacklight()->RestoreBrightness(); });
        power_save_timer_->OnShutdownRequest([this](){
            if (screen_locked_.load()) return;
            pmic_->PowerOff(); });
        UpdatePowerSaveTimer();
    }

    void InitializeCodecI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeTca9554(void) {
        esp_err_t ret = esp_io_expander_new_i2c_tca9554(i2c_bus_, I2C_ADDRESS, &io_expander);
        if (ret != ESP_OK)
            ESP_LOGE(TAG, "TCA9554 create returned error");
        ret = esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_4, IO_EXPANDER_INPUT);
        ESP_ERROR_CHECK(ret);
    }

    void InitializeAxp2101() {
        ESP_LOGI(TAG, "Init AXP2101");
        pmic_ = new Pmic(i2c_bus_, 0x34);
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.sclk_io_num = EXAMPLE_PIN_NUM_LCD_PCLK;
        buscfg.data0_io_num = EXAMPLE_PIN_NUM_LCD_DATA0;
        buscfg.data1_io_num = EXAMPLE_PIN_NUM_LCD_DATA1;
        buscfg.data2_io_num = EXAMPLE_PIN_NUM_LCD_DATA2;
        buscfg.data3_io_num = EXAMPLE_PIN_NUM_LCD_DATA3;
        buscfg.max_transfer_sz = DISPLAY_WIDTH*  DISPLAY_HEIGHT*  sizeof(uint16_t);
        buscfg.flags = SPICOMMON_BUSFLAG_QUAD;
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    // Gravity points "down"; pick the quadrant whose content top faces away
    // from it. This mapping is verified on this exact board (CO5300 QSPI +
    // QMI8658 on I2C SDA=15/SCL=14) by an independent reference firmware:
    //   |ax| dominant, ax > 0 -> 0     |ax| dominant, ax < 0 -> 180
    //   |ay| dominant, ay > 0 -> 90    |ay| dominant, ay < 0 -> 270
    // The old code used a +90-degree ROTATION_QUARTER_OFFSET on top of an
    // inverted Y mapping: it corrected the two X orientations but left both Y
    // orientations upside down, which is exactly the reported "反向旋转".
    // Returns -1 when the device is flat (in-plane angle unreliable).
    static int ClassifyRotation(const Motion::AccelSample& s) {
        const int ax = s.x, ay = s.y, az = s.z;
        const int abs_ax = ax < 0 ? -ax : ax;
        const int abs_ay = ay < 0 ? -ay : ay;
        const int abs_az = az < 0 ? -az : az;
        if (abs_az > ROTATE_FLAT_Z_THRESHOLD_MG) {
            return -1;
        }
        if (abs_ax > abs_ay) {
            if (abs_ax < ROTATE_AXIS_THRESHOLD_MG) return -1;
            return ax > 0 ? LV_DISPLAY_ROTATION_0 : LV_DISPLAY_ROTATION_180;
        }
        if (abs_ay < ROTATE_AXIS_THRESHOLD_MG) return -1;
        return ay > 0 ? LV_DISPLAY_ROTATION_90 : LV_DISPLAY_ROTATION_270;
    }

    // The rotation itself is a single hardware MADCTL switch, so there is no
    // in-between frame to interpolate. We hide that hard switch behind a quick
    // full-screen black curtain: fade in -> apply rotation -> fade out. All
    // LVGL calls run under the (recursive) display lock from the LVGL task.
    static void RotationAnimExec(void* obj, int32_t value) {
        lv_obj_set_style_bg_opa(static_cast<lv_obj_t*>(obj),
                                static_cast<lv_opa_t>(value), 0);
    }

    static void RotationFadeInDone(lv_anim_t* anim) {
        auto* self = static_cast<WaveshareEsp32s3TouchAMOLED216GrokBot*>(anim->user_data);
        self->OnRotationFadeInDone();
    }

    static void RotationFadeOutDone(lv_anim_t* anim) {
        auto* self = static_cast<WaveshareEsp32s3TouchAMOLED216GrokBot*>(anim->user_data);
        self->OnRotationFadeOutDone();
    }

    void EnsureRotationCurtain() {
        if (rotation_curtain_ != nullptr && lv_obj_is_valid(rotation_curtain_)) {
            return;
        }
        rotation_curtain_ = lv_obj_create(lv_layer_top());
        lv_obj_set_size(rotation_curtain_, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_pos(rotation_curtain_, 0, 0);
        lv_obj_set_style_bg_color(rotation_curtain_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(rotation_curtain_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(rotation_curtain_, 0, 0);
        lv_obj_set_style_pad_all(rotation_curtain_, 0, 0);
        lv_obj_set_style_radius(rotation_curtain_, 0, 0);
        lv_obj_clear_flag(rotation_curtain_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(rotation_curtain_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(rotation_curtain_, LV_OBJ_FLAG_CLICKABLE);
    }

    void ApplyRotation(int rotation) {
        if (rotation_busy_) {
            return;
        }
        DisplayLockGuard lock(display_);
        if (!lock) {
            ESP_LOGW(TAG, "AutoRotate: failed to lock display");
            return;
        }
        lv_display_t* disp = lv_display_get_default();
        const auto target = static_cast<lv_display_rotation_t>(rotation);
        if (disp == nullptr || lv_display_get_rotation(disp) == target) {
            applied_rotation_ = target;
            return;
        }

        pending_rotation_ = rotation;
        rotation_busy_ = true;
        EnsureRotationCurtain();

        lv_obj_remove_flag(rotation_curtain_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(rotation_curtain_);
        // Keep the curtain sized for the current orientation.
        lv_obj_set_size(rotation_curtain_, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_pos(rotation_curtain_, 0, 0);
        lv_obj_set_style_bg_opa(rotation_curtain_, LV_OPA_TRANSP, 0);

        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, rotation_curtain_);
        lv_anim_set_values(&anim, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_duration(&anim, ROTATE_FADE_IN_MS);
        lv_anim_set_exec_cb(&anim, RotationAnimExec);
        lv_anim_set_completed_cb(&anim, RotationFadeInDone);
        lv_anim_set_user_data(&anim, this);
        lv_anim_start(&anim);
        ESP_LOGI(TAG, "AutoRotate: transition -> %d deg (ax=%d ay=%d az=%d)",
                 rotation * 90, last_accel_.x, last_accel_.y, last_accel_.z);
    }

    void OnRotationFadeInDone() {
        DisplayLockGuard lock(display_);
        if (!lock) {
            rotation_busy_ = false;
            return;
        }
        lv_display_t* disp = lv_display_get_default();
        if (disp != nullptr && pending_rotation_ >= 0) {
            // Hardware MADCTL + touch point rotation happen here, fully hidden.
            lv_display_set_rotation(disp, static_cast<lv_display_rotation_t>(pending_rotation_));
            lv_obj_invalidate(lv_screen_active());
            applied_rotation_ = static_cast<lv_display_rotation_t>(pending_rotation_);
        }

        // Re-fit the curtain to the new orientation and fade it back out.
        if (rotation_curtain_ != nullptr && lv_obj_is_valid(rotation_curtain_)) {
            lv_obj_set_size(rotation_curtain_, LV_HOR_RES, LV_VER_RES);
            lv_obj_set_pos(rotation_curtain_, 0, 0);
            lv_obj_set_style_bg_opa(rotation_curtain_, LV_OPA_COVER, 0);
        }
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, rotation_curtain_);
        lv_anim_set_values(&anim, LV_OPA_COVER, LV_OPA_TRANSP);
        lv_anim_set_duration(&anim, ROTATE_FADE_OUT_MS);
        lv_anim_set_exec_cb(&anim, RotationAnimExec);
        lv_anim_set_completed_cb(&anim, RotationFadeOutDone);
        lv_anim_set_user_data(&anim, this);
        lv_anim_start(&anim);
    }

    void OnRotationFadeOutDone() {
        DisplayLockGuard lock(display_);
        if (!lock) {
            rotation_busy_ = false;
            return;
        }
        if (rotation_curtain_ != nullptr && lv_obj_is_valid(rotation_curtain_)) {
            lv_obj_add_flag(rotation_curtain_, LV_OBJ_FLAG_HIDDEN);
        }
        pending_rotation_ = -1;
        rotation_busy_ = false;
    }

    static void AutoRotateTaskEntry(void* arg) {
        auto* self = static_cast<WaveshareEsp32s3TouchAMOLED216GrokBot*>(arg);
        self->AutoRotateTask();
        vTaskDelete(nullptr);
    }

    void AutoRotateTask() {
        int pending = -1;
        int stable = 0;
        while (true) {
            if (screen_locked_.load()) {
                pending = -1;
                stable = 0;
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }
            Motion::AccelSample sample;
            if (Motion::ReadAccel(sample)) {
                last_accel_ = sample;
                const int target = ClassifyRotation(sample);
                if (target == pending) {
                    ++stable;
                } else {
                    pending = target;
                    stable = 1;
                }
                if (pending >= 0 && stable >= ROTATE_STABLE_SAMPLES &&
                    pending != static_cast<int>(applied_rotation_)) {
                    ApplyRotation(pending);
                    stable = 0;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(ROTATE_POLL_INTERVAL_MS));
        }
    }

    void InitializeAutoRotate() {
        const esp_err_t ret = Motion::Initialize(i2c_bus_);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "QMI8658 unavailable, auto-rotate disabled: %s",
                     esp_err_to_name(ret));
            return;
        }
        ESP_LOGI(TAG, "QMI8658 ready, auto-rotate enabled");
        if (xTaskCreate(AutoRotateTaskEntry, "auto_rotate", 3072, this, 2,
                        &rotate_task_handle_) != pdPASS) {
            rotate_task_handle_ = nullptr;
            ESP_LOGW(TAG, "Failed to create auto-rotate task");
        }
    }

    void SetScreenLocked(bool locked) {
        if (screen_locked_.exchange(locked) == locked) {
            return;
        }
        if (locked) {
            ESP_LOGI(TAG, "Screen locked (BOOT long press)");
            display_->SetScreenLocked(true);
            UpdatePowerSaveTimer();
            if (touch_indev_ != nullptr) {
                DisplayLockGuard lock(display_);
                if (lock) {
                    lv_indev_enable(touch_indev_, false);
                }
            }
            GetBacklight()->SetBrightness(0);
        } else {
            ESP_LOGI(TAG, "Screen unlocked (BOOT long press)");
            if (touch_indev_ != nullptr) {
                DisplayLockGuard lock(display_);
                if (lock) {
                    lv_indev_enable(touch_indev_, true);
                }
            }
            UpdatePowerSaveTimer();
            power_save_timer_->WakeUp();
            GetDisplay()->SetPowerSaveMode(false);
            display_->SetScreenLocked(false);
            GetBacklight()->RestoreBrightness();
        }
    }

    // BOOT triple-click toggles mute. Volume 0 is persisted by AudioCodec to
    // NVS, so mute survives reboot; remember the pre-mute level to restore it.
    void ToggleMute() {
        if (screen_locked_.load()) {
            return;
        }
        auto* codec = GetAudioCodec();
        if (codec == nullptr) {
            return;
        }
        const int current = codec->output_volume();
        // volume_before_mute_ >= 0 -> we muted in this session, restore it.
        // current == 0 at boot -> NVS already holds a mute from a previous
        // session, restore the default instead of "muting twice".
        if (volume_before_mute_ >= 0 || current == 0) {
            const int restored = volume_before_mute_ > 0 ? volume_before_mute_ : 70;
            codec->SetOutputVolume(restored);
            volume_before_mute_ = -1;
            ESP_LOGI(TAG, "Unmuted, volume restored to %d", restored);
            GetDisplay()->ShowNotification("Volume On");
        } else {
            volume_before_mute_ = current;
            codec->SetOutputVolume(0);
            ESP_LOGI(TAG, "Muted (saved volume %d)", current);
            GetDisplay()->ShowNotification("Muted");
        }
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            if (screen_locked_.load()) {
                return;
            }
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });

        boot_button_.OnLongPress([this]() {
            SetScreenLocked(!screen_locked_.load());
        });

        // Triple-click: mute / unmute (independent of single/double click).
        boot_button_.OnMultipleClick([this]() { ToggleMute(); }, 3);

#if CONFIG_USE_DEVICE_AEC
        boot_button_.OnDoubleClick([this]() {
            if (screen_locked_.load()) {
                return;
            }
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateIdle) {
                app.SetAecMode(app.GetAecMode() == kAecOff ? kAecOnDeviceSide : kAecOff);
            }
        });
#endif
    }

    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = EXAMPLE_PIN_NUM_LCD_CS;
        io_config.dc_gpio_num = GPIO_NUM_NC;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 32;
        io_config.lcd_param_bits = 8;
        io_config.flags.quad_mode = true;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        const co5300_vendor_config_t vendor_config = {
            .init_cmds = &vendor_specific_init[0],
            .init_cmds_size = sizeof(vendor_specific_init) / sizeof(co5300_lcd_init_cmd_t),
            .flags = {
                .use_qspi_interface = 1,
            }};

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = (void* )&vendor_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(panel_io, &panel_config, &panel));
        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, false);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);
        display_ = new GrokbotDisplay(panel_io, panel,
                                        DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY, DISPLAY_TE_PIN);
        backlight_ = new CustomBacklight(panel_io);
        backlight_->RestoreBrightness();
    }

    void InitializeTouch() {
        esp_lcd_touch_handle_t tp;
        esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_WIDTH - 1,
            .y_max = DISPLAY_HEIGHT - 1,
            .rst_gpio_num = PIN_NUM_TOUCH_RST,
            .int_gpio_num = PIN_NUM_TOUCH_INT,
            .levels = {
                .reset = 0,
                .interrupt = 0,
            },
            .flags = {
                .swap_xy = 0,
                .mirror_x = 0,
                .mirror_y = 0,
            },
        };
        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST9217_CONFIG();
        tp_io_config.scl_speed_hz = 400*  1000;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle));
        ESP_LOGI(TAG, "Initialize touch controller");
        ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst9217(tp_io_handle, &tp_cfg, &tp));
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lv_display_get_default(),
            .handle = tp,
        };
        touch_indev_ = lvgl_port_add_touch(&touch_cfg);
        if (touch_indev_ == nullptr) {
            ESP_LOGE(TAG, "Failed to add touch input device");
        } else {
            ESP_LOGI(TAG, "Touch panel initialized successfully");
        }
    }

    // 初始化工具
    void InitializeTools() {
        auto &mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.system.reconfigure_wifi",
            "End this conversation and enter WiFi configuration mode.\n"
            "**CAUTION** You must ask the user to confirm this action.",
            PropertyList(), [this](const PropertyList& properties) {
                EnterWifiConfigMode();
                return true;
            });
    }

public:
    WaveshareEsp32s3TouchAMOLED216GrokBot()
        : boot_button_(BOOT_BUTTON_GPIO, false, SCREEN_LOCK_LONG_PRESS_MS) {
        InitializePowerSaveTimer();
        InitializeCodecI2c();
#if CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_AMOLED_1_75
        InitializeTca9554();
#endif
        InitializeAxp2101();
        InitializeSpi();
        InitializeDisplay();
        InitializeTouch();
        InitializeButtons();
        InitializeTools();
        InitializeAutoRotate();
        // Start with Wi-Fi modem sleep off; SetPowerSaveLevel will toggle it
        // back on only while the device is truly idle.
        SetWifiPowerSave(false);
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(
            i2c_bus_, 
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, 
            AUDIO_I2S_GPIO_BCLK, 
            AUDIO_I2S_GPIO_WS, 
            AUDIO_I2S_GPIO_DOUT, 
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, 
            AUDIO_CODEC_ES8311_ADDR, 
            AUDIO_CODEC_ES7210_ADDR, 
            AUDIO_INPUT_REFERENCE,
            30.0f, -1, 0.0f, AUDIO_CODEC_PA_VOLTAGE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        return backlight_;
    }

    virtual bool GetBatteryLevel(int &level, bool &charging, bool &discharging) override {
        charging = pmic_->IsCharging();
        discharging = pmic_->IsDischarging();
        if (discharging != battery_discharging_) {
            battery_discharging_ = discharging;
            UpdatePowerSaveTimer();
        }

        level = pmic_->GetBatteryLevel();
        return true;
    }

    void SetWifiPowerSave(bool enabled) {
        // The ESP-IDF default modem sleep has a 1s listen interval. On this
        // board the network TTS stream stutters badly while sleeping, so turn
        // Wi-Fi power save off whenever the audio channel is active.
        static bool last = true;  // IDF default: WIFI_PS_MIN_MODEM
        if (last == enabled) {
            return;
        }
        const wifi_ps_type_t ps = enabled ? WIFI_PS_MIN_MODEM : WIFI_PS_NONE;
        const esp_err_t err = esp_wifi_set_ps(ps);
        if (err != ESP_OK) {
            // Wi-Fi may not be initialized yet at board construction; retry later.
            ESP_LOGW(TAG, "esp_wifi_set_ps(%d) failed: %s", (int)ps, esp_err_to_name(err));
            return;
        }
        last = enabled;
        ESP_LOGI(TAG, "Wi-Fi modem sleep %s", enabled ? "enabled" : "disabled (audio active)");
    }

    virtual void SetPowerSaveLevel(PowerSaveLevel level) override {
        if (level != PowerSaveLevel::LOW_POWER) {
            power_save_timer_->WakeUp();
        }
        // PERFORMANCE is set right before opening the audio channel / playing
        // a notification. Disabling modem sleep here removes the 1024ms beacon
        // listen interval that was causing the intermittent audio dropouts.
        SetWifiPowerSave(level == PowerSaveLevel::LOW_POWER);
        WifiBoard::SetPowerSaveLevel(level);
    }
};

DECLARE_BOARD(WaveshareEsp32s3TouchAMOLED216GrokBot);
