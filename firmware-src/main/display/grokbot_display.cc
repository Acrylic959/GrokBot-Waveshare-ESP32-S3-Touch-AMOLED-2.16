#include "grokbot_display.h"
#include "pet_state.h"

#include "assets.h"
#include "assets/lang_config.h"
#include "lvgl_theme.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <esp_heap_caps.h>
#include <cstdio>
#include <esp_log.h>
#include <esp_timer.h>
#include <miniz.h>

namespace {
constexpr const char* TAG = "GrokbotDisplay";
constexpr const char* kAtlasName = "grokbot.atlas";
constexpr int kRoundEdgeInset = 56;
constexpr int kRoundStatusTop = 56;
constexpr int kRoundChatBottom = 66;
// The atlas is authored at 30 FPS. Measured render cost is ~13-19 ms/frame
// (inflate ~10-13 ms, 4-bit->RGB565 expand ~2 ms, present ~1-5 ms), which fits
// in the 33 ms budget at 30 FPS. Audio output/opus are pinned to core 1 and the
// animation task to core 0, so a full-rate animation no longer starves audio.
// During active audio we still throttle moderately to leave PSRAM/QSPI headroom.
constexpr int kNormalFps = 30;
constexpr int kAudioBusyFps = 15;
constexpr int kPowerSaveFps = 10;
constexpr uint32_t kPanelTeHz = 60;
// Emotions PetState can suggest, indexed for the rapid-tap animation hold.
constexpr const char* kPetActionAnims[] = {
    "happy", "excited", "playful", "shy", "surprised", "sleeping", "waking",
    "confused", "sad", "drowsy", "scared", "searching", "laughing", "idle",
};
constexpr int kPetActionAnimCount = sizeof(kPetActionAnims) / sizeof(kPetActionAnims[0]);
int PetAnimIndex(const char* name) {
    if (name == nullptr) return -1;
    for (int i = 0; i < kPetActionAnimCount; ++i) {
        if (strcmp(name, kPetActionAnims[i]) == 0) return i;
    }
    return -1;
}
constexpr int64_t kPetAnimHoldUs = 1200000LL;   // rapid taps reuse one animation
constexpr int64_t kPetDizzyAnimUs = 5000000LL;  // shake -> dizzy for ~5 s

const char* MapEmotion(const char* emotion) {
    if (emotion == nullptr) {
        return nullptr;
    }
    if (strcmp(emotion, "happy") == 0 || strcmp(emotion, "loving") == 0 ||
        strcmp(emotion, "delicious") == 0) {
        return "happy";
    }
    if (strcmp(emotion, "laughing") == 0) {
        return "laughing";
    }
    if (strcmp(emotion, "funny") == 0 || strcmp(emotion, "silly") == 0 ||
        strcmp(emotion, "winking") == 0 || strcmp(emotion, "kissy") == 0) {
        return "playful";
    }
    if (strcmp(emotion, "embarrassed") == 0) {
        return "shy";
    }
    if (strcmp(emotion, "confident") == 0 || strcmp(emotion, "cool") == 0) {
        return "proud";
    }
    if (strcmp(emotion, "sad") == 0 || strcmp(emotion, "crying") == 0) {
        return "sad";
    }
    if (strcmp(emotion, "sleepy") == 0) {
        return "sleeping";
    }
    if (strcmp(emotion, "angry") == 0) {
        return "angry";
    }
    if (strcmp(emotion, "surprised") == 0 || strcmp(emotion, "shocked") == 0) {
        return "surprised";
    }
    if (strcmp(emotion, "thinking") == 0) {
        return "thinking";
    }
    if (strcmp(emotion, "confused") == 0) {
        return "confused";
    }
    if (strcmp(emotion, "scared") == 0) {
        return "scared";
    }
    if (strcmp(emotion, "warning") == 0) {
        return "alerting";
    }
    if (strcmp(emotion, "gear") == 0) {
        return "loading";
    }
    if (strcmp(emotion, "cloud_download") == 0 || strcmp(emotion, "download") == 0) {
        return "uploading";
    }
    if (strcmp(emotion, "cancel") == 0 || strcmp(emotion, "cloud_off") == 0) {
        return "failed";
    }
    if (strcmp(emotion, "link") == 0) {
        return "approving";
    }
    if (strcmp(emotion, "excited") == 0) {
        return "excited";
    }
    if (strcmp(emotion, "curious") == 0) {
        return "curious";
    }
    if (strcmp(emotion, "drowsy") == 0 || strcmp(emotion, "low_battery") == 0) {
        return "drowsy";
    }
    if (strcmp(emotion, "battery_connected") == 0) {
        return "waking";
    }
    if (strcmp(emotion, "neutral") == 0 || strcmp(emotion, "robot_2") == 0 ||
        strcmp(emotion, "relaxed") == 0 || strcmp(emotion, "idle") == 0) {
        return "idle";
    }
    if (strcmp(emotion, "waking") == 0 || strcmp(emotion, "sleeping") == 0 ||
        strcmp(emotion, "notifying") == 0) {
        return emotion;
    }
    return nullptr;
}

const char* MapStatus(const char* status) {
    if (status == nullptr) {
        return "idle";
    }
    if (strcmp(status, Lang::Strings::STANDBY) == 0) {
        return "idle";
    }
    if (strcmp(status, Lang::Strings::LISTENING) == 0) {
        return "listening";
    }
    if (strcmp(status, Lang::Strings::SPEAKING) == 0) {
        return "streaming";
    }
    if (strcmp(status, Lang::Strings::CONNECTING) == 0 ||
        strcmp(status, Lang::Strings::LOADING_PROTOCOL) == 0 ||
        strcmp(status, Lang::Strings::INITIALIZING) == 0 ||
        strcmp(status, Lang::Strings::LOADING_ASSETS) == 0) {
        return "loading";
    }
    if (strcmp(status, Lang::Strings::SCANNING_WIFI) == 0) {
        return "radar";
    }
    if (strcmp(status, Lang::Strings::REGISTERING_NETWORK) == 0 ||
        strcmp(status, Lang::Strings::SERVER_NOT_FOUND) == 0) {
        return "waiting";
    }
    if (strcmp(status, Lang::Strings::DETECTING_MODULE) == 0 ||
        strcmp(status, Lang::Strings::CHECKING_NEW_VERSION) == 0) {
        return "searching";
    }
    if (strcmp(status, Lang::Strings::ACTIVATION) == 0) {
        return "waking";
    }
    if (strcmp(status, Lang::Strings::CONNECTION_SUCCESSFUL) == 0) {
        return "approving";
    }
    if (strcmp(status, Lang::Strings::UPGRADING) == 0 ||
        strcmp(status, Lang::Strings::OTA_UPGRADE) == 0) {
        return "uploading";
    }
    if (strcmp(status, Lang::Strings::ERROR) == 0 ||
        strcmp(status, Lang::Strings::SERVER_ERROR) == 0 ||
        strcmp(status, Lang::Strings::SERVER_TIMEOUT) == 0 ||
        strcmp(status, Lang::Strings::SERVER_NOT_CONNECTED) == 0 ||
        strcmp(status, Lang::Strings::UPGRADE_FAILED) == 0 ||
        strcmp(status, Lang::Strings::DOWNLOAD_ASSETS_FAILED) == 0) {
        return "failed";
    }
    if (strcmp(status, Lang::Strings::HELLO_MY_FRIEND) == 0) {
        return "happy";
    }
    if (strcmp(status, Lang::Strings::PLEASE_WAIT) == 0) {
        return "waiting";
    }
    // Application::Alert() accepts a human-readable status string while the
    // animation is selected by the accompanying emotion. Do not let an unknown
    // status hide a valid emotion.
    return "idle";
}
}  // namespace

GrokbotDisplay::GrokbotDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                               int width, int height, int offset_x, int offset_y, bool mirror_x,
                               bool mirror_y, bool swap_xy, gpio_num_t te_pin)
    : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy),
      te_pin_(te_pin) {}

GrokbotDisplay::~GrokbotDisplay() {
    running_.store(false);
    if (te_isr_installed_) {
        gpio_isr_handler_remove(te_pin_);
        te_isr_installed_ = false;
    }
    if (te_semaphore_ != nullptr) {
        vSemaphoreDelete(te_semaphore_);
        te_semaphore_ = nullptr;
    }
    if (task_handle_ != nullptr) {
        vTaskDelay(pdMS_TO_TICKS(50));
        vTaskDelete(task_handle_);
        task_handle_ = nullptr;
    }
    heap_caps_free(packed_);
    heap_caps_free(delta_);
    heap_caps_free(decompressor_);
    for (auto*& buffer : image_buffers_) {
        heap_caps_free(buffer);
        buffer = nullptr;
    }
}

void GrokbotDisplay::RounderEventCb(lv_event_t* event) {
    auto* area = static_cast<lv_area_t*>(lv_event_get_param(event));
    if (area == nullptr) {
        return;
    }
    area->x1 = (area->x1 >> 1) << 1;
    area->y1 = (area->y1 >> 1) << 1;
    area->x2 = ((area->x2 >> 1) << 1) + 1;
    area->y2 = ((area->y2 >> 1) << 1) + 1;
}

void GrokbotDisplay::InitializeTe() {
    if (te_pin_ == GPIO_NUM_NC) {
        return;
    }
    te_semaphore_ = xSemaphoreCreateBinary();
    if (te_semaphore_ == nullptr) {
        ESP_LOGW(TAG, "Failed to create TE semaphore");
        return;
    }

    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << te_pin_;
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_POSEDGE;
    if (gpio_config(&config) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to configure TE GPIO %d", te_pin_);
        return;
    }

    esp_err_t isr_service = gpio_install_isr_service(0);
    if (isr_service != ESP_OK && isr_service != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(isr_service));
        return;
    }
    if (gpio_isr_handler_add(te_pin_, TeIsr, this) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to add TE ISR for GPIO %d", te_pin_);
        return;
    }
    te_isr_installed_ = true;
    ESP_LOGI(TAG, "TE synchronization enabled on GPIO %d", te_pin_);
}

void GrokbotDisplay::TeIsr(void* arg) {
    auto* display = static_cast<GrokbotDisplay*>(arg);
    if (display->te_semaphore_ == nullptr) {
        return;
    }
    display->te_edge_count_.fetch_add(1, std::memory_order_relaxed);
    BaseType_t higher_priority_task_woken = pdFALSE;
    xSemaphoreGiveFromISR(display->te_semaphore_, &higher_priority_task_woken);
    if (higher_priority_task_woken != pdFALSE) {
        portYIELD_FROM_ISR();
    }
}

bool GrokbotDisplay::WaitForTeFrame(int fps) {
    if (!te_isr_installed_ || te_semaphore_ == nullptr) {
        return true;
    }

    const int64_t start_us = NowUs();
    uint32_t edges_per_frame =
        (kPanelTeHz + static_cast<uint32_t>(fps) / 2) / static_cast<uint32_t>(fps);
    edges_per_frame = std::max<uint32_t>(1, edges_per_frame);
    const uint32_t target_edge = last_te_present_count_ + edges_per_frame;
    uint32_t current_edge = te_edge_count_.load(std::memory_order_acquire);

    while (current_edge < target_edge) {
        if (xSemaphoreTake(te_semaphore_, pdMS_TO_TICKS(25)) != pdTRUE) {
            te_timeout_count_.fetch_add(1, std::memory_order_relaxed);
            te_wait_us_total_.fetch_add(NowUs() - start_us, std::memory_order_relaxed);
            te_wait_count_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        current_edge = te_edge_count_.load(std::memory_order_acquire);
    }

    last_te_present_count_ = current_edge;
    te_wait_us_total_.fetch_add(NowUs() - start_us, std::memory_order_relaxed);
    te_wait_count_.fetch_add(1, std::memory_order_relaxed);
    return true;
}

void GrokbotDisplay::SetupUI() {
    SpiLcdDisplay::SetupUI();

    auto* dark_theme = LvglThemeManager::GetInstance().GetTheme("dark");
    if (dark_theme != nullptr && current_theme_ != dark_theme) {
        SetTheme(dark_theme);
    }

    DisplayLockGuard lock(this);
    if (!lock) {
        return;
    }

    lv_obj_set_style_pad_left(status_bar_, LV_HOR_RES * 0.2f, 0);
    lv_obj_set_style_pad_right(status_bar_, LV_HOR_RES * 0.2f, 0);
    lv_obj_align(status_bar_, LV_ALIGN_TOP_MID, 0, kRoundStatusTop);
    lv_obj_set_width(status_label_, LV_HOR_RES * 0.6f);
    lv_obj_set_width(notification_label_, LV_HOR_RES * 0.6f);
    lv_obj_align(status_label_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_align(notification_label_, LV_ALIGN_CENTER, 0, 0);

    if (bottom_bar_ != nullptr) {
        lv_obj_align(bottom_bar_, LV_ALIGN_BOTTOM_MID, 0, -kRoundChatBottom);
        lv_obj_set_width(chat_message_label_, LV_HOR_RES * 0.75f);
    }
    if (top_bar_ != nullptr) {
        lv_obj_align(top_bar_, LV_ALIGN_TOP_MID, 0, kRoundEdgeInset / 2);
        lv_obj_set_style_pad_top(top_bar_, kRoundEdgeInset / 4, 0);
    }

    if (emoji_box_ != nullptr) {
        lv_obj_set_size(emoji_box_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(emoji_box_, LV_ALIGN_CENTER, 0, 0);
    }
    if (emoji_label_ != nullptr) {
        lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
    }

    lv_display_add_event_cb(display_, RounderEventCb, LV_EVENT_INVALIDATE_AREA, nullptr);
    InitializeTe();
    InitializePetPanel();

    if (!running_.exchange(true)) {
        BaseType_t created = xTaskCreatePinnedToCore(AnimationTaskEntry, "grokbot_anim", 12 * 1024,
                                                     this, 1, &task_handle_, 0);
        if (created != pdPASS) {
            running_.store(false);
            task_handle_ = nullptr;
            ESP_LOGE(TAG, "Failed to create animation task");
        }
    }
}

// ---------------------------------------------------------------------------
// Dedicated electronic-pet mode.
//
// The pet character is ONE image object living in emoji_box_.  Switching
// views must never delete/recreate it, otherwise the 30 FPS animation would
// visibly stutter.  The overlay is a Pokémon-style status frame: only Lv. and
// body type at the very top, four slim stat bars forming a complete ring
// around the screen edges, and a compact 3x2 action menu at the bottom.  The
// middle stays transparent so the same animated character keeps playing.
// ---------------------------------------------------------------------------
namespace {
constexpr const char* kPetStatLabels[4] = {"心情", "健康", "饱腹", "清洁"};
// PetState stores six stats; the pet screen deliberately shows only these four.
constexpr int kPetStatStateIndex[4] = {0, 3, 1, 4};  // mood, health, satiety, hygiene
// Button index -> PetState action id. 抚摸 (2) and 睡觉 (5) have no button:
// petting is a tap on the character, sleeping is idle-triggered after 60 s.
constexpr int kPetButtonActionId[4] = {0, 1, 3, 4};
constexpr int64_t kPetAutoSleepUs = 60 * 1000000LL;

// Four concentric rounded-rectangle rings. Corner radii decrease by the same
// step as the insets so the gap stays equal on straight edges and corners.
constexpr int kPetRingCount = 4;
constexpr int kPetRingMaxPoints = 192;
constexpr int kPetRingWidth = 10;
const int kPetRingBounds[kPetRingCount][4] = {
    {16, 16, 464, 464}, {28, 28, 452, 452},
    {40, 40, 440, 440}, {52, 52, 428, 428},
};
const int kPetRingRadius[kPetRingCount] = {30, 25, 20, 15};
const uint32_t kPetRingColors[kPetRingCount] = {
    0x00E5FF,  // mood - cyan
    0xFF5C8A,  // health - pink
    0xFFC857,  // satiety - amber
    0x4ADE80,  // hygiene - green
};

// Shared scratch buffers: the button callback task has a small stack, so the
// path/length arrays must not be allocated there. Calls are serialized by the
// display mutex.
static lv_point_precise_t s_ring_path[kPetRingMaxPoints];
static float s_ring_lengths[kPetRingMaxPoints];

void BuildPetRingPoints(lv_point_precise_t* out, int* out_count, int ring, int value) {
    *out_count = 0;
    if (ring < 0 || ring >= kPetRingCount || value <= 0) {
        return;
    }
    const float half = kPetRingWidth / 2.0f;
    const float x0 = kPetRingBounds[ring][0] + half;
    const float y0 = kPetRingBounds[ring][1] + half;
    const float x1 = kPetRingBounds[ring][2] - half;
    const float y1 = kPetRingBounds[ring][3] - half;
    const float r = std::max(2.0f, static_cast<float>(kPetRingRadius[ring]) - half);
    const float cx = (x0 + x1) / 2.0f;

    lv_point_precise_t* path = s_ring_path;
    int n = 0;
    auto push = [&](float px, float py) {
        if (n < kPetRingMaxPoints) {
            path[n].x = px;
            path[n].y = py;
            ++n;
        }
    };
    constexpr float kStep = 16.0f;
    auto add_line = [&](float ax, float ay, float bx, float by) {
        const float dist = std::hypot(bx - ax, by - ay);
        const int steps = std::max(1, static_cast<int>(dist / kStep));
        for (int i = 0; i <= steps; ++i) {
            const float t = static_cast<float>(i) / steps;
            push(ax + (bx - ax) * t, ay + (by - ay) * t);
        }
    };
    auto add_arc = [&](float acx, float acy, float a0, float a1) {
        // span is in DEGREES; convert to radians before multiplying by radius.
        // Previously `span * r / step` produced ~160 steps for one 90-degree
        // corner, exhausting the point budget before the rest of the ring.
        const float arc_len = std::fabs(a1 - a0) * 3.14159265f / 180.0f * r;
        const int steps = std::max(6, static_cast<int>(arc_len / kStep));
        for (int i = 0; i <= steps; ++i) {
            const float a = (a0 + (a1 - a0) * i / steps) * 3.14159265f / 180.0f;
            push(acx + r * std::cos(a), acy + r * std::sin(a));
        }
    };

    // Start at the top centre and run clockwise around the rounded rectangle.
    add_line(cx, y0, x1 - r, y0);
    add_arc(x1 - r, y0 + r, -90.0f, 0.0f);
    add_line(x1, y0 + r, x1, y1 - r);
    add_arc(x1 - r, y1 - r, 0.0f, 90.0f);
    add_line(x1 - r, y1, x0 + r, y1);
    add_arc(x0 + r, y1 - r, 90.0f, 180.0f);
    add_line(x0, y1 - r, x0, y0 + r);
    add_arc(x0 + r, y0 + r, 180.0f, 270.0f);
    add_line(x0 + r, y0, cx, y0);

    if (n < 2) {
        return;
    }
    float* lengths = s_ring_lengths;
    lengths[0] = 0.0f;
    for (int i = 1; i < n; ++i) {
        lengths[i] = lengths[i - 1] +
                     std::hypot(path[i].x - path[i - 1].x, path[i].y - path[i - 1].y);
    }
    const float target = lengths[n - 1] * std::min(100, value) / 100.0f;
    int last = 1;
    while (last < n - 1 && lengths[last] < target) {
        ++last;
    }
    for (int i = 0; i <= last; ++i) {
        out[i] = path[i];
    }
    *out_count = last + 1;
}
}  // namespace

void GrokbotDisplay::PetButtonEventCb(lv_event_t* event) {
    if (event == nullptr || lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    auto* display = static_cast<GrokbotDisplay*>(lv_event_get_user_data(event));
    if (display == nullptr) {
        return;
    }
    lv_obj_t* target = lv_event_get_target_obj(event);
    for (int i = 0; i < 4; ++i) {
        lv_obj_t* candidate = target;
        while (candidate != nullptr && candidate != display->pet_buttons_[i]) {
            candidate = lv_obj_get_parent(candidate);
        }
        if (candidate == display->pet_buttons_[i]) {
            display->OnPetAction(kPetButtonActionId[i]);
            return;
        }
    }
}

void GrokbotDisplay::PetPanelEventCb(lv_event_t* event) {
    if (event == nullptr || lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    auto* display = static_cast<GrokbotDisplay*>(lv_event_get_user_data(event));
    if (display == nullptr || !display->pet_mode_active_.load()) {
        return;
    }
    lv_indev_t* indev = lv_event_get_indev(event);
    if (indev == nullptr) {
        return;
    }
    lv_point_t point = {0, 0};
    lv_indev_get_point(indev, &point);
    // Tap the character area (inside the ring, above the button row) to pet.
    // Button clicks target the buttons themselves and never reach this panel.
    if (point.x < 72 || point.x > 408 || point.y < 96 || point.y > 340) {
        return;
    }
    ESP_LOGI(TAG, "Pet tap at (%d,%d) -> caress", point.x, point.y);
    display->OnPetAction(2);  // Pet() -> playful animation
}

void GrokbotDisplay::InitializePetPanel() {
    lv_obj_t* screen = lv_screen_active();
    if (screen == nullptr) {
        return;
    }

    pet_panel_ = lv_obj_create(screen);
    lv_obj_set_size(pet_panel_, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(pet_panel_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(pet_panel_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pet_panel_, 0, 0);
    lv_obj_set_style_pad_all(pet_panel_, 0, 0);
    lv_obj_set_scrollbar_mode(pet_panel_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(pet_panel_, LV_OBJ_FLAG_SCROLLABLE);
    // Clicking the empty middle of the panel means "pet the character".
    lv_obj_add_flag(pet_panel_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(pet_panel_, PetPanelEventCb, LV_EVENT_CLICKED, this);
    // Faded in/out by the animation task; hidden = outside hit-testing.
    lv_obj_set_style_opa(pet_panel_, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(pet_panel_, LV_OBJ_FLAG_HIDDEN);

    // ---- Top badge: LEVEL + BODY TYPE only (monochrome) --------------------
    pet_title_ = lv_label_create(pet_panel_);
    lv_obj_set_width(pet_title_, 180);
    lv_label_set_text(pet_title_, "Lv.1 · 体型 宝宝");
    lv_obj_set_style_text_color(pet_title_, lv_color_white(), 0);
    lv_obj_set_style_text_align(pet_title_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(pet_title_, LV_ALIGN_TOP_MID, 0, 62);
    lv_obj_set_style_bg_opa(pet_title_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pet_title_, 0, 0);
    lv_obj_set_style_radius(pet_title_, 0, 0);
    lv_obj_set_style_pad_all(pet_title_, 6, 0);

    // ---- Four concentric rounded-rectangle progress rings -----------------
    // Rasterized into ONE ARGB8888 buffer but shown as FOUR edge strips, so the
    // transparent middle is not an image object and per-frame character blits
    // never blend a full-screen transparent overlay.
    // Strips: top / bottom / left / right; their union covers ring bbox [16..464]
    // with the centre hole [65..415] left completely empty (verified: all 31682
    // ring pixels fall inside a strip, none inside the hole).
    struct RingStrip { int x, y, w, h; };
    static const RingStrip kStrips[4] = {
        {0, 0, 480, 65},      // top
        {0, 415, 480, 65},    // bottom
        {0, 65, 65, 350},     // left
        {415, 65, 65, 350},   // right
    };
    const size_t ring_bytes = static_cast<size_t>(LV_HOR_RES) * LV_VER_RES * 4;
    pet_ring_buffer_ = static_cast<uint8_t*>(heap_caps_malloc(ring_bytes, MALLOC_CAP_SPIRAM));
    if (pet_ring_buffer_ == nullptr) {
        pet_ring_buffer_ = static_cast<uint8_t*>(heap_caps_malloc(ring_bytes, MALLOC_CAP_8BIT));
    }
    if (pet_ring_buffer_ != nullptr) {
        memset(pet_ring_buffer_, 0, ring_bytes);
        for (int i = 0; i < 4; ++i) {
            const RingStrip& strip = kStrips[i];
            lv_image_dsc_t& dsc = pet_ring_dscs_[i];
            dsc = {};
            dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
            dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
            dsc.header.w = strip.w;
            dsc.header.h = strip.h;
            dsc.header.stride = LV_HOR_RES * 4;  // full-row stride, same shared buffer
            dsc.data_size = ring_bytes;
            dsc.data = pet_ring_buffer_ + (static_cast<size_t>(strip.y) * LV_HOR_RES + strip.x) * 4;
            lv_obj_t* img = lv_image_create(pet_panel_);
            lv_obj_set_pos(img, strip.x, strip.y);
            lv_image_set_src(img, &dsc);
            lv_obj_clear_flag(img, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
            pet_ring_images_[i] = img;
        }
    }

    // ---- Assistant subtitle directly under the level badge ----------------
    pet_subtitle_ = lv_label_create(pet_panel_);
    lv_obj_set_width(pet_subtitle_, 320);
    lv_obj_set_style_text_color(pet_subtitle_, lv_color_white(), 0);
    lv_obj_set_style_text_align(pet_subtitle_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(pet_subtitle_, LV_LABEL_LONG_WRAP);
    lv_obj_align(pet_subtitle_, LV_ALIGN_TOP_MID, 0, 88);
    lv_label_set_text(pet_subtitle_, "");
    lv_obj_add_flag(pet_subtitle_, LV_OBJ_FLAG_HIDDEN);

    // Interaction feedback: white prefix + four coloured delta labels.
    pet_feedback_ = lv_obj_create(pet_panel_);
    lv_obj_set_width(pet_feedback_, 440);
    lv_obj_set_height(pet_feedback_, LV_SIZE_CONTENT);
    lv_obj_align(pet_feedback_, LV_ALIGN_TOP_MID, 0, 88);
    lv_obj_set_style_bg_opa(pet_feedback_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pet_feedback_, 0, 0);
    lv_obj_set_style_pad_all(pet_feedback_, 0, 0);
    lv_obj_set_style_pad_column(pet_feedback_, 8, 0);
    lv_obj_set_style_pad_row(pet_feedback_, 2, 0);
    lv_obj_set_flex_flow(pet_feedback_, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(pet_feedback_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(pet_feedback_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(pet_feedback_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(pet_feedback_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(pet_feedback_, LV_OBJ_FLAG_HIDDEN);

    pet_feedback_prefix_ = lv_label_create(pet_feedback_);
    lv_obj_set_width(pet_feedback_prefix_, LV_SIZE_CONTENT);
    lv_obj_set_style_text_color(pet_feedback_prefix_, lv_color_white(), 0);
    lv_label_set_text(pet_feedback_prefix_, "");
    for (int i = 0; i < kPetRingCount; ++i) {
        lv_obj_t* value = lv_label_create(pet_feedback_);
        lv_obj_set_width(value, LV_SIZE_CONTENT);
        lv_obj_set_style_text_color(value, lv_color_hex(kPetRingColors[i]), 0);
        lv_label_set_text(value, "");
        lv_obj_add_flag(value, LV_OBJ_FLAG_HIDDEN);
        pet_feedback_values_[i] = value;
    }

    // ---- Four-action menu: monochrome buttons, 4px gaps --------------------
    static const char* kActions[4] = {"投喂", "玩耍", "清洁", "探险"};
    for (int i = 0; i < 4; ++i) {
        lv_obj_t* button = lv_btn_create(pet_panel_);
        lv_obj_set_size(button, 76, 56);
        lv_obj_align(button, LV_ALIGN_TOP_LEFT, 72 + i * 84, 352);
        lv_obj_set_style_radius(button, 14, 0);
        lv_obj_set_style_bg_color(button, lv_color_hex(0x121212), 0);
        lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(button, 2, 0);
        lv_obj_set_style_border_color(button, lv_color_white(), 0);
        lv_obj_set_style_bg_color(button, lv_color_hex(0x333333), LV_STATE_PRESSED);

        lv_obj_t* label = lv_label_create(button);
        lv_label_set_text(label, kActions[i]);
        lv_obj_center(label);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_add_event_cb(button, PetButtonEventCb, LV_EVENT_CLICKED, this);
        pet_buttons_[i] = button;
    }

    ESP_LOGI(TAG, "Pet panel ready (4 concentric rounded rings, coloured deltas, subtitle under level)");
}

// Rasterize the four progress rings into a single ARGB8888 bitmap. The bitmap
// is redrawn only when a stat changes, so the per-frame character blit no longer
// has to re-stroke four ~130-point vector polylines.
void GrokbotDisplay::RasterizePetRingsLocked() {
    if (pet_ring_buffer_ == nullptr) {
        return;
    }
    const int w = LV_HOR_RES;
    const int h = LV_VER_RES;
    memset(pet_ring_buffer_, 0, static_cast<size_t>(w) * h * 4);

    auto& pet = PetState::GetInstance();
    const int values[6] = {pet.mood(), pet.satiety(), pet.energy(),
                           pet.health(), pet.hygiene(), pet.fun()};
    const float radius = kPetRingWidth / 2.0f;
    const int ir = static_cast<int>(radius);

    // Plot a filled disc of the ring colour (8-bit alpha), used for round caps
    // and to thicken each sampled segment into a stroke.
    auto disc = [&](float px, float py, uint32_t argb) {
        const int cx = static_cast<int>(px);
        const int cy = static_cast<int>(py);
        for (int yy = cy - ir; yy <= cy + ir; ++yy) {
            if (yy < 0 || yy >= h) continue;
            for (int xx = cx - ir; xx <= cx + ir; ++xx) {
                if (xx < 0 || xx >= w) continue;
                const float dx = xx - px;
                const float dy = yy - py;
                if (dx * dx + dy * dy > radius * radius) continue;
                auto* dst = reinterpret_cast<uint32_t*>(pet_ring_buffer_) +
                            static_cast<size_t>(yy) * w + xx;
                *dst = argb;  // opaque ring colour, no blending needed
            }
        }
    };

    for (int i = 0; i < kPetRingCount; ++i) {
        const int value = values[kPetStatStateIndex[i]];
        int count = 0;
        BuildPetRingPoints(pet_ring_points_[i], &count, i, value);
        if (count < 2) continue;
        const uint32_t rgb = kPetRingColors[i];
        // kPetRingColors are 0xRRGGBB; LVGL ARGB8888 is 0xAARRGGBB in uint32
        // (stored B,G,R,A on little-endian). Previous swap made cyan -> yellow.
        const uint32_t argb = 0xFF000000u | rgb;
        for (int k = 0; k + 1 < count; ++k) {
            const float ax = pet_ring_points_[i][k].x;
            const float ay = pet_ring_points_[i][k].y;
            const float bx = pet_ring_points_[i][k + 1].x;
            const float by = pet_ring_points_[i][k + 1].y;
            const int steps = std::max(1, static_cast<int>(std::hypot(bx - ax, by - ay)));
            for (int s = 0; s <= steps; ++s) {
                const float t = static_cast<float>(s) / steps;
                disc(ax + (bx - ax) * t, ay + (by - ay) * t, argb);
            }
        }
    }
    for (lv_obj_t* img : pet_ring_images_) {
        if (img != nullptr) {
            lv_obj_invalidate(img);
        }
    }
}

void GrokbotDisplay::RefreshPetPanelLocked() {
    if (pet_panel_ == nullptr || !pet_mode_active_.load()) {
        return;
    }
    auto& pet = PetState::GetInstance();
    const int values[6] = {pet.mood(), pet.satiety(), pet.energy(),
                           pet.health(), pet.hygiene(), pet.fun()};
    bool ring_changed = false;
    for (int i = 0; i < kPetRingCount; ++i) {
        const int value = values[kPetStatStateIndex[i]];
        if (value != pet_ring_last_values_[i]) {
            pet_ring_last_values_[i] = value;
            ring_changed = true;
        }
    }
    if (ring_changed) {
        RasterizePetRingsLocked();
    }
    if (pet_title_ != nullptr) {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "Lv.%d · 体型 %s",
                 pet.level(), pet.stageName());
        lv_label_set_text(pet_title_, buffer);
    }
}

void GrokbotDisplay::ShowPetFeedbackText(const char* text) {
    if (pet_feedback_ == nullptr || pet_feedback_prefix_ == nullptr) {
        return;
    }
    lv_label_set_text(pet_feedback_prefix_, text != nullptr ? text : "");
    for (int i = 0; i < kPetRingCount; ++i) {
        if (pet_feedback_values_[i] == nullptr) {
            continue;
        }
        lv_label_set_text(pet_feedback_values_[i], "");
        lv_obj_add_flag(pet_feedback_values_[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_remove_flag(pet_feedback_, LV_OBJ_FLAG_HIDDEN);
    if (pet_subtitle_ != nullptr) {
        lv_obj_add_flag(pet_subtitle_, LV_OBJ_FLAG_HIDDEN);
    }
}

void GrokbotDisplay::ShowPetFeedbackDeltas(const char* prefix, const int delta[kPetRingCount]) {
    if (pet_feedback_ == nullptr || pet_feedback_prefix_ == nullptr || delta == nullptr) {
        return;
    }
    lv_label_set_text(pet_feedback_prefix_, prefix != nullptr ? prefix : "");
    char buffer[40];
    for (int i = 0; i < kPetRingCount; ++i) {
        lv_obj_t* label = pet_feedback_values_[i];
        if (label == nullptr) {
            continue;
        }
        if (delta[i] == 0) {
            lv_label_set_text(label, "");
            lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
        } else {
            snprintf(buffer, sizeof(buffer), "%s %+d", kPetStatLabels[i], delta[i]);
            lv_label_set_text(label, buffer);
            lv_obj_remove_flag(label, LV_OBJ_FLAG_HIDDEN);
        }
    }
    lv_obj_remove_flag(pet_feedback_, LV_OBJ_FLAG_HIDDEN);
    if (pet_subtitle_ != nullptr) {
        lv_obj_add_flag(pet_subtitle_, LV_OBJ_FLAG_HIDDEN);
    }
}

void GrokbotDisplay::OnPetAction(int action) {
    if (action < 0 || action > 5) {
        return;
    }
    NotePetInteraction();
    auto& pet = PetState::GetInstance();
    const int before[4] = {pet.mood(), pet.health(), pet.satiety(), pet.hygiene()};
    // Any explicit interaction wakes a sleeping pet first (except the sleep
    // toggle itself), so a tap/button never just says "it is asleep".
    if (action != 5 && pet.sleeping()) {
        ESP_LOGI(TAG, "Auto wake before action %d: %s", action, pet.Wake());
    }
    const char* result = nullptr;
    switch (action) {
        case 0: result = pet.Feed(); break;
        case 1: result = pet.Play(); break;
        case 2: result = pet.Pet(); break;
        case 3: result = pet.Clean(); break;
        case 4: result = pet.Explore(); break;
        case 5: result = pet.sleeping() ? pet.Wake() : pet.Sleep(); break;
        default: break;
    }
    ESP_LOGI(TAG, "Pet action %d -> %s | %s | Animation -> %s", action,
             result != nullptr ? result : "(none)", pet.Describe().c_str(),
             pet.SuggestEmotion());

    // Feedback stays INSIDE the pet panel.  ShowNotification() is deliberately
    // avoided: it used to force EffectiveAnimationName() to "notifying" for
    // seconds, hiding the very animation the action was supposed to trigger.
    const int after[4] = {pet.mood(), pet.health(), pet.satiety(), pet.hygiene()};
    int delta[kPetRingCount] = {};
    for (int i = 0; i < kPetRingCount; ++i) {
        delta[i] = after[i] - before[i];
    }
    // Rapid taps: hold the first action's animation for a short window so the
    // clip is not rebuilt on every tap (a full rebuild decodes the whole clip
    // and was the main cause of the frame-rate drop while tapping).
    const char* action_emotion = pet.SuggestEmotion();
    const int action_index = PetAnimIndex(action_emotion);
    if (action_index >= 0 && NowUs() >= pet_dizzy_until_us_.load()) {
        pet_anim_hold_name_.store(action_index);
        pet_anim_hold_until_us_.store(NowUs() + kPetAnimHoldUs);
    }
    DisplayLockGuard lock(this);
    if (lock) {
        ShowPetFeedbackDeltas(result != nullptr ? result : "", delta);
        RefreshPetPanelLocked();
    }
    pet_feedback_until_us_.store(NowUs() + 4000000);  // hide after 4 s
}

void GrokbotDisplay::SetPetMode(bool active) {
    if (screen_locked_.load()) {
        return;
    }
    if (pet_mode_active_.exchange(active) == active) {
        return;
    }

    // Wake and logging must NOT happen while the display mutex is held:
    // the animation task can hold the log lock and wait for the display lock,
    // which deadlocks esp_log_impl_lock_timeout().
    bool woke_on_enter = false;
    if (active) {
        auto& pet = PetState::GetInstance();
        if (pet.sleeping()) {
            pet.Wake();
            woke_on_enter = true;
        }
    }
    {
        DisplayLockGuard lock(this);
        if (!lock) {
            return;
        }
        if (active) {
            if (status_bar_ != nullptr) {
                pet_saved_status_visible_ = !lv_obj_has_flag(status_bar_, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(status_bar_, LV_OBJ_FLAG_HIDDEN);
            }
            if (top_bar_ != nullptr) {
                pet_saved_top_visible_ = !lv_obj_has_flag(top_bar_, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(top_bar_, LV_OBJ_FLAG_HIDDEN);
            }
            if (bottom_bar_ != nullptr) {
                pet_saved_bottom_visible_ = !lv_obj_has_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);
            }
            if (pet_panel_ != nullptr) {
                lv_obj_set_style_opa(pet_panel_, LV_OPA_COVER, 0);
                lv_obj_remove_flag(pet_panel_, LV_OBJ_FLAG_HIDDEN);
            }
            if (pet_subtitle_ != nullptr) {
                const char* text = lv_label_get_text(pet_subtitle_);
                if (text != nullptr && text[0] != '\0') {
                    lv_obj_remove_flag(pet_subtitle_, LV_OBJ_FLAG_HIDDEN);
                } else {
                    lv_obj_add_flag(pet_subtitle_, LV_OBJ_FLAG_HIDDEN);
                }
            }
            pet_feedback_until_us_.store(0);
            if (pet_feedback_ != nullptr) {
                lv_obj_add_flag(pet_feedback_, LV_OBJ_FLAG_HIDDEN);
            }
            NotePetInteraction();
            RefreshPetPanelLocked();
        } else {
            if (pet_panel_ != nullptr) {
                lv_obj_add_flag(pet_panel_, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_style_opa(pet_panel_, LV_OPA_TRANSP, 0);
            }
            pet_last_interaction_us_.store(0);
            if (pet_feedback_ != nullptr) {
                lv_obj_add_flag(pet_feedback_, LV_OBJ_FLAG_HIDDEN);
            }
            if (pet_subtitle_ != nullptr) {
                lv_obj_add_flag(pet_subtitle_, LV_OBJ_FLAG_HIDDEN);
            }
            if (status_bar_ != nullptr && pet_saved_status_visible_) {
                lv_obj_remove_flag(status_bar_, LV_OBJ_FLAG_HIDDEN);
            }
            if (top_bar_ != nullptr && pet_saved_top_visible_) {
                lv_obj_remove_flag(top_bar_, LV_OBJ_FLAG_HIDDEN);
            }
            if (bottom_bar_ != nullptr && pet_saved_bottom_visible_) {
                lv_obj_remove_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    if (active) {
        if (woke_on_enter) {
            ESP_LOGI(TAG, "Pet was asleep -> auto wake on entering pet mode");
        }
        ESP_LOGI(TAG, "Pet mode ON (overlay shown, character stays live)");
    } else {
        ESP_LOGI(TAG, "Pet mode OFF (overlay hidden, character stays live)");
    }
}

void GrokbotDisplay::TogglePetMode() { SetPetMode(!pet_mode_active_.load()); }

bool GrokbotDisplay::SetEmotion(const char* emotion) {
    const char* mapped = MapEmotion(emotion);
    if (mapped == nullptr) {
        ESP_LOGW(TAG, "Unsupported emotion: %s", emotion != nullptr ? emotion : "(null)");
        return false;
    }
    requested_emotion_.store(mapped);
    return true;
}

void GrokbotDisplay::SetStatus(const char* status) {
    const bool loading_assets =
        status != nullptr && strcmp(status, Lang::Strings::LOADING_ASSETS) == 0;
    if (loading_assets) {
        atlas_refresh_required_.store(true);
    }
    const char* mapped_status = MapStatus(status);
    requested_status_.store(mapped_status);
    // Voice wake-up must always bring the user back to the assistant home
    // screen: fade out the pet overlay while the character keeps animating.
    if (mapped_status != nullptr && strcmp(mapped_status, "listening") == 0 &&
        pet_mode_active_.load()) {
        SetPetMode(false);
        ESP_LOGI(TAG, "Voice wake-up: pet panel hidden, back to home");
    }
    // When the device returns to idle, clear any AI-driven emotion so the
    // electronic pet's ambient expression takes over again.
    if (mapped_status != nullptr && strcmp(mapped_status, "idle") == 0) {
        requested_emotion_.store(nullptr);
    }
    assets_update_in_progress_.store(loading_assets);
    LvglDisplay::SetStatus(status);
}

void GrokbotDisplay::SetChatMessage(const char* role, const char* content) {
    LcdDisplay::SetChatMessage(role, content);
    DisplayLockGuard lock(this);
    if (!lock || pet_subtitle_ == nullptr) {
        return;
    }
    lv_label_set_text(pet_subtitle_, content != nullptr ? content : "");
    const bool visible = pet_mode_active_.load() && content != nullptr && content[0] != '\0';
    if (visible) {
        lv_obj_remove_flag(pet_subtitle_, LV_OBJ_FLAG_HIDDEN);
        if (pet_feedback_ != nullptr) {
            lv_obj_add_flag(pet_feedback_, LV_OBJ_FLAG_HIDDEN);
        }
        // Keep the classic bottom subtitle bar out of the pet screen.
        if (bottom_bar_ != nullptr) {
            lv_obj_add_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        lv_obj_add_flag(pet_subtitle_, LV_OBJ_FLAG_HIDDEN);
    }
}

void GrokbotDisplay::RemoveChatMessage(const char* role, const char* content) {
    LcdDisplay::RemoveChatMessage(role, content);
    DisplayLockGuard lock(this);
    if (!lock || pet_subtitle_ == nullptr || content == nullptr) {
        return;
    }
    if (strcmp(lv_label_get_text(pet_subtitle_), content) == 0) {
        lv_label_set_text(pet_subtitle_, "");
        lv_obj_add_flag(pet_subtitle_, LV_OBJ_FLAG_HIDDEN);
    }
}

void GrokbotDisplay::ClearChatMessages() {
    LcdDisplay::ClearChatMessages();
    DisplayLockGuard lock(this);
    if (!lock || pet_subtitle_ == nullptr) {
        return;
    }
    lv_label_set_text(pet_subtitle_, "");
    lv_obj_add_flag(pet_subtitle_, LV_OBJ_FLAG_HIDDEN);
}

void GrokbotDisplay::ShowNotification(const char* notification, int duration_ms) {
    const int bounded_ms = std::clamp(duration_ms, 0, 3000);
    notification_until_us_.store(NowUs() + static_cast<int64_t>(bounded_ms) * 1000);
    LvglDisplay::ShowNotification(notification, duration_ms);
}

void GrokbotDisplay::ShowNotification(const std::string& notification, int duration_ms) {
    ShowNotification(notification.c_str(), duration_ms);
}

void GrokbotDisplay::SetPowerSaveMode(bool on) {
    target_fps_.store(on ? kPowerSaveFps : kNormalFps);
    LvglDisplay::SetPowerSaveMode(on);
}

void GrokbotDisplay::SetScreenLocked(bool locked) {
    screen_locked_.store(locked);
    ESP_LOGI(TAG, "Animation %s by screen lock", locked ? "paused" : "resumed");
}

int64_t GrokbotDisplay::NowUs() const { return esp_timer_get_time(); }

void GrokbotDisplay::NotePetInteraction() {
    pet_last_interaction_us_.store(NowUs());
}

const char* GrokbotDisplay::EffectiveAnimationName() {
    const int64_t notification_until = notification_until_us_.load();
    int64_t now = NowUs();
    // Shake gesture works on BOTH the home screen and the pet page. It has
    // top priority and is not interrupted by status/emotion/action changes.
    if (pet_dizzy_pending_.exchange(false)) {
        pet_dizzy_until_us_.store(0);
        pet_dizzy_armed_.store(true);
        pet_dizzy_arm_start_us_.store(now);
        ESP_LOGI(TAG, "Shake detected -> dizzy (play full clip)");
    }
    if (pet_dizzy_armed_.load()) {
        // Fallback: if the clip switch never happens, don't stick forever.
        if (now - pet_dizzy_arm_start_us_.load() > 1000000LL) {
            pet_dizzy_armed_.store(false);
            pet_dizzy_until_us_.store(now + kPetDizzyAnimUs);
        }
        return "confused";
    }
    if (now < pet_dizzy_until_us_.load()) {
        return "confused";
    }
    if (notification_until > now) {
        return "notifying";
    }
    if (notification_until != 0) {
        notification_until_us_.store(0);
    }

    // In pet mode the pet's transient action animation takes priority (happy
    // after feeding, excited after playing, playful after petting, shy after
    // cleaning, etc.).  This is what makes the buttons visibly "do something"
    // instead of only printing a line of text.
    if (pet_mode_active_.load()) {
        const int held = pet_anim_hold_name_.load();
        if (held >= 0 && held < kPetActionAnimCount &&
            now < pet_anim_hold_until_us_.load()) {
            return kPetActionAnims[held];
        }
        pet_anim_hold_name_.store(-1);
        return PetState::GetInstance().SuggestEmotion();
    }

    const char* status = requested_status_.load();
    if (status != nullptr && strcmp(status, "idle") != 0) {
        return status;
    }
    const char* emotion = requested_emotion_.load();
    if (emotion != nullptr) {
        return emotion;
    }
    // Idle with no explicit emotion: let the virtual pet's state choose the
    // ambient animation (happy / searching / drowsy / sad / sleeping / idle).
    const char* pet_emotion = PetState::GetInstance().SuggestEmotion();
    if (pet_emotion != nullptr && strcmp(pet_emotion, "idle") != 0) {
        return pet_emotion;
    }
    // All needs met and left alone for a while: occasional idle antics
    // (stretch / look around / hum) bring the pet to life.
    const char* antic = PetState::GetInstance().IdleAntic();
    return antic != nullptr ? antic : "idle";
}

bool GrokbotDisplay::EnsurePlayer() {
    const int64_t now = NowUs();
    const bool force_refresh = atlas_refresh_required_.exchange(false);
    if (!force_refresh && player_ready_ && now < next_atlas_check_us_) {
        return true;
    }
    if (!force_refresh && !player_ready_ && now < next_atlas_check_us_) {
        return false;
    }
    next_atlas_check_us_ = now + 1000000;

    void* ptr = nullptr;
    size_t size = 0;
    if (!Assets::GetInstance().GetAssetData(kAtlasName, ptr, size)) {
        ESP_LOGW(TAG, "Waiting for %s in assets partition", kAtlasName);
        return false;
    }

    const auto* data = static_cast<const uint8_t*>(ptr);
    if (!player_ready_ || data != atlas_data_ || size != atlas_size_) {
        player_ready_ = player_.init(data, size);
        if (!player_ready_) {
            return false;
        }
        atlas_data_ = data;
        atlas_size_ = size;
        has_selected_ = false;
        const int new_packed_size = player_.width() * player_.height() / 2;
        const size_t new_image_size = static_cast<size_t>(player_.width()) * player_.height() * 2;
        if (new_packed_size <= 0 || new_image_size == 0) {
            player_ready_ = false;
            return false;
        }

        if (frame_width_ != player_.width() || frame_height_ != player_.height() ||
            packed_ == nullptr || delta_ == nullptr || decompressor_ == nullptr ||
            image_buffers_[0] == nullptr || image_buffers_[1] == nullptr) {
            heap_caps_free(packed_);
            heap_caps_free(delta_);
            heap_caps_free(decompressor_);
            for (auto*& buffer : image_buffers_) {
                heap_caps_free(buffer);
                buffer = nullptr;
            }
            packed_ = nullptr;
            delta_ = nullptr;
            decompressor_ = nullptr;

            frame_width_ = player_.width();
            frame_height_ = player_.height();
            packed_size_ = static_cast<size_t>(new_packed_size);
            image_size_ = new_image_size;
            packed_ = static_cast<uint8_t*>(
                heap_caps_aligned_alloc(64, packed_size_, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
            delta_ = static_cast<uint8_t*>(
                heap_caps_aligned_alloc(64, packed_size_, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
            decompressor_ = static_cast<tinfl_decompressor*>(heap_caps_malloc(
                sizeof(tinfl_decompressor), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
            if (decompressor_ == nullptr) {
                decompressor_ = static_cast<tinfl_decompressor*>(heap_caps_malloc(
                    sizeof(tinfl_decompressor), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
            }
            image_buffers_[0] = static_cast<uint16_t*>(
                heap_caps_aligned_alloc(64, image_size_, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
            image_buffers_[1] = static_cast<uint16_t*>(
                heap_caps_aligned_alloc(64, image_size_, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
            if (packed_ == nullptr || delta_ == nullptr || decompressor_ == nullptr ||
                image_buffers_[0] == nullptr || image_buffers_[1] == nullptr) {
                ESP_LOGE(TAG,
                         "PSRAM allocation failed: packed=%p delta=%p decompressor=%p images=%p/%p",
                         packed_, delta_, decompressor_, image_buffers_[0], image_buffers_[1]);
                heap_caps_free(packed_);
                heap_caps_free(delta_);
                heap_caps_free(decompressor_);
                heap_caps_free(image_buffers_[0]);
                heap_caps_free(image_buffers_[1]);
                packed_ = nullptr;
                delta_ = nullptr;
                decompressor_ = nullptr;
                image_buffers_[0] = nullptr;
                image_buffers_[1] = nullptr;
                frame_width_ = 0;
                frame_height_ = 0;
                packed_size_ = 0;
                image_size_ = 0;
                player_ready_ = false;
                return false;
            }
            memset(packed_, 0, packed_size_);
            memset(delta_, 0, packed_size_);
            memset(image_buffers_[0], 0, image_size_);
            memset(image_buffers_[1], 0, image_size_);

            uint16_t gray_lut[16] = {};
            for (int i = 0; i < 16; ++i) {
                const uint8_t value = static_cast<uint8_t>(i * 17);
                gray_lut[i] = static_cast<uint16_t>(((value & 0xF8) << 8) | ((value & 0xFC) << 3) |
                                                    (value >> 3));
            }
            for (int i = 0; i < 256; ++i) {
                const uint16_t low = gray_lut[i & 0x0F];
                const uint16_t high = gray_lut[i >> 4];
                pair_lut_[i] = static_cast<uint32_t>(high) | (static_cast<uint32_t>(low) << 16);
            }

            // Clamp the measured content bbox to this atlas frame.  The stock
            // 360x360 atlas matches exactly; clamping only guards against a
            // future/replaced asset with a smaller frame.
            crop_x0_ = std::min(kGrokContentX0, frame_width_ - 1);
            crop_y0_ = std::min(kGrokContentY0, frame_height_ - 1);
            crop_w_ = std::min(kGrokContentW, frame_width_ - crop_x0_);
            crop_h_ = std::min(kGrokContentH, frame_height_ - crop_y0_);
            frame_geometry_ready_ = false;  // re-place the image under display lock

            // Sub-region descriptors: expose only the character's content box,
            // but keep the FULL frame stride and offset data into the very same
            // buffer.  lv_image_set_src() invalidates the object's bounds, so a
            // smaller object means a smaller dirty rectangle, less software
            // blending and fewer bytes pushed over QSPI - with no pixel change.
            // HARD REQUIREMENT: CONFIG_LV_DRAW_BUF_STRIDE_ALIGN must stay 1.
            // Otherwise LVGL's bin decoder sees stride(720) != width-based
            // stride(252*2=504) and rebuilds the buffer, breaking the crop.
            // data_size MUST stay the full frame size: LVGL rejects a draw buf
            // whose stride*h (720*286=205920) exceeds data_size.
            for (int i = 0; i < 2; ++i) {
                image_descs_[i] = {};
                image_descs_[i].header.magic = LV_IMAGE_HEADER_MAGIC;
                image_descs_[i].header.cf = LV_COLOR_FORMAT_RGB565;
                image_descs_[i].header.w = crop_w_;
                image_descs_[i].header.h = crop_h_;
                image_descs_[i].header.stride = frame_width_ * sizeof(uint16_t);
                image_descs_[i].data_size = image_size_;
                image_descs_[i].data =
                    reinterpret_cast<const uint8_t*>(image_buffers_[i]) +
                    (static_cast<size_t>(crop_y0_) * frame_width_ + crop_x0_) * sizeof(uint16_t);
            }
        }
        ESP_LOGI(TAG, "Atlas ready: %dx%d, %d states, %d fps, packed=%u image=%u", frame_width_,
                 frame_height_, player_.clip_count(), player_.fps(),
                 static_cast<unsigned>(packed_size_), static_cast<unsigned>(image_size_));
        ESP_LOGI(TAG, "Heap after PSRAM buffers: internal=%u, psram=%u",
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
    }

    return player_ready_;
}

bool GrokbotDisplay::DecodeFrame(const GrokBotPlayer::FrameInfo& frame, bool expand) {
    if (frame.data == nullptr || frame.compressed_size == 0 || packed_ == nullptr ||
        delta_ == nullptr || decompressor_ == nullptr) {
        return false;
    }
    const int64_t start_us = NowUs();
    tinfl_init(decompressor_);
    size_t input_size = frame.compressed_size;
    size_t output_size = packed_size_;
    const tinfl_status status =
        tinfl_decompress(decompressor_, frame.data, &input_size, delta_, delta_, &output_size,
                         TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    if (status != TINFL_STATUS_DONE || output_size != packed_size_ ||
        input_size != frame.compressed_size) {
        ESP_LOGE(TAG, "Inflate failed: status=%d in=%u/%u out=%u/%u", status,
                 static_cast<unsigned>(input_size), static_cast<unsigned>(frame.compressed_size),
                 static_cast<unsigned>(output_size), static_cast<unsigned>(packed_size_));
        return false;
    }
    if (!frame.delta) {
        memcpy(packed_, delta_, packed_size_);
    } else {
        auto* packed_words = reinterpret_cast<uint32_t*>(packed_);
        auto* delta_words = reinterpret_cast<uint32_t*>(delta_);
        const size_t word_count = packed_size_ / sizeof(uint32_t);
        for (size_t i = 0; i < word_count; ++i) {
            packed_words[i] ^= delta_words[i];
        }
    }
    if (expand) {
        ExpandPackedFrame();
    }
    decode_us_total_ += NowUs() - start_us;
    return true;
}

void GrokbotDisplay::ExpandPackedFrame() {
    auto* pixels = image_buffers_[write_buffer_];
    if (pixels == nullptr || packed_ == nullptr) {
        return;
    }
    auto* pixels32 = reinterpret_cast<uint32_t*>(pixels);
    const size_t pixel_pairs = packed_size_;
    for (size_t i = 0; i < pixel_pairs; ++i) {
        pixels32[i] = pair_lut_[packed_[i]];
    }
}

bool GrokbotDisplay::PresentFrame() {
    if (emoji_image_ == nullptr || image_buffers_[write_buffer_] == nullptr) {
        return false;
    }
    WaitForTeFrame(std::max(1, target_fps_.load()));
    const int64_t start_us = NowUs();
    const int presented = write_buffer_;
    write_buffer_ ^= 1;
    DisplayLockGuard lock(this);
    if (!lock) {
        return false;
    }
    if (!frame_geometry_ready_) {
        // emoji_box_ stays the full frame size so the cropped character keeps
        // exactly its original on-screen position; the image itself is moved
        // to the content bbox origin.  Both must happen under the display lock
        // because the animation task (core 0) owns this code path.
        lv_obj_set_size(emoji_box_, frame_width_, frame_height_);
        lv_obj_align(emoji_box_, LV_ALIGN_CENTER, 0, 0);
        // Use lv_obj_align (not lv_obj_set_pos): the base SetupUI centred the
        // image, and set_pos alone would keep LV_ALIGN_CENTER and treat our
        // coordinates as a centre offset instead of an absolute position.
        lv_obj_align(emoji_image_, LV_ALIGN_TOP_LEFT, crop_x0_, crop_y0_);
        frame_geometry_ready_ = true;
    }
    lv_image_set_src(emoji_image_, &image_descs_[presented]);
    lv_obj_remove_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
    present_us_total_ += NowUs() - start_us;
    return true;
}

bool GrokbotDisplay::RebuildCurrentFrame() {
    const int target = player_.current_index();
    if (!player_.seek(0)) {
        return false;
    }
    for (int i = 0; i <= target; ++i) {
        GrokBotPlayer::FrameInfo frame = {};
        if (!player_.frame_info(i, &frame) || !DecodeFrame(frame, i == target)) {
            return false;
        }
    }
    if (!player_.seek(target)) {
        return false;
    }
    rebuild_pending_ = false;
    if (!PresentFrame()) {
        return false;
    }
    player_.advance();
    return true;
}

void GrokbotDisplay::AnimationTaskEntry(void* arg) {
    static_cast<GrokbotDisplay*>(arg)->AnimationTask();
    vTaskDelete(nullptr);
}

void GrokbotDisplay::AnimationTask() {
    int64_t next_frame_us = NowUs();
    int64_t report_start_us = next_frame_us;
    int64_t render_total_us = 0;
    uint32_t last_report_te_edges = te_edge_count_.load(std::memory_order_relaxed);
    uint32_t rendered_frames = 0;
    int64_t last_pet_ui_us = NowUs();
    while (running_.load()) {
        // Overlay fade + live stat bars are driven from the same task that
        // decodes the character, so UI transitions never block or restart the
        // animation.
        // Voice wake-up must fade the pet overlay back to home.  Polling the
        // atomic status here (rather than relying only on SetStatus timing)
        // guarantees it fires even when the state machine changes status from
        // another task while the animation loop is running.
        if (pet_mode_active_.load()) {
            const char* st = requested_status_.load();
            if (st != nullptr && strcmp(st, "listening") == 0) {
                SetPetMode(false);
                ESP_LOGI(TAG, "Voice wake-up detected in animation loop -> pet panel hidden");
            }
        }
        const int64_t ui_now = NowUs();
        if (pet_mode_active_.load() && ui_now - last_pet_ui_us > 1000000LL) {
            last_pet_ui_us = ui_now;
            auto& pet = PetState::GetInstance();
            bool just_fell_asleep = false;
            // No touch/button for 60 s in pet mode -> sleep with its own
            // "sleeping" animation (SuggestEmotion() returns sleeping).
            if (!pet.sleeping() && pet_last_interaction_us_.load() > 0 &&
                ui_now - pet_last_interaction_us_.load() > kPetAutoSleepUs) {
                const char* result = pet.Sleep();
                pet_last_interaction_us_.store(ui_now);
                just_fell_asleep = true;
                ESP_LOGI(TAG, "Pet auto-sleep after 60s idle: %s | Animation -> %s",
                         result != nullptr ? result : "(none)", pet.SuggestEmotion());
            }
            DisplayLockGuard lock(this);
            if (lock) {
                RefreshPetPanelLocked();
                if (just_fell_asleep && pet_feedback_ != nullptr) {
                    ShowPetFeedbackText("静置 1 分钟，睡着啦 Zzz");
                    pet_feedback_until_us_.store(ui_now + 4000000);
                }
                if (pet_feedback_until_us_.load() > 0 &&
                    ui_now > pet_feedback_until_us_.load()) {
                    pet_feedback_until_us_.store(0);
                    if (pet_feedback_ != nullptr) {
                        lv_obj_add_flag(pet_feedback_, LV_OBJ_FLAG_HIDDEN);
                    }
                    if (pet_subtitle_ != nullptr) {
                        const char* sub = lv_label_get_text(pet_subtitle_);
                        if (sub != nullptr && sub[0] != '\0') {
                            lv_obj_remove_flag(pet_subtitle_, LV_OBJ_FLAG_HIDDEN);
                        }
                    }
                }
            }
        }
        if (screen_locked_.load()) {
            // Screen is off: yield all CPU / PSRAM / QSPI bandwidth to audio.
            vTaskDelay(pdMS_TO_TICKS(100));
            next_frame_us = NowUs();
            continue;
        }
        if (assets_update_in_progress_.load()) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (!EnsurePlayer()) {
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }

        const char* effective = EffectiveAnimationName();
        // Time the dizzy priority window to the confused clip's real length
        // (ping-pong forward+back) instead of a fixed duration, so it always
        // plays from start to finish before switching back.
        if (pet_dizzy_armed_.load() && player_.selected_name() != nullptr &&
            strcmp(player_.selected_name(), "confused") == 0) {
            const int frames = std::max(1, player_.selected_frame_count());
            const int clip_fps = std::max(1, player_.fps());
            const int64_t clip_us =
                (static_cast<int64_t>(frames) * 2 + 2) * 1000000LL / clip_fps;
            pet_dizzy_until_us_.store(NowUs() + clip_us);
            pet_dizzy_armed_.store(false);
            ESP_LOGI(TAG, "Dizzy full clip: %d frames @ %d fps -> %lld ms",
                     frames, clip_fps, static_cast<long long>(clip_us / 1000));
        }
        int fps = std::max(1, target_fps_.load());
        // "listening" / "streaming" are the mapped states for LISTENING / SPEAKING.
        // "notifying" accompanies TTS playback. Throttle hard so the audio tasks
        // can keep their I2S DMA buffers filled without competing with the atlas.
        if (strcmp(effective, "listening") == 0 || strcmp(effective, "streaming") == 0 ||
            strcmp(effective, "notifying") == 0) {
            fps = std::min(fps, kAudioBusyFps);
        }
        const int64_t frame_us = 1000000LL / fps;
        if (!has_selected_ || strcmp(player_.selected_name(), effective) != 0) {
            if (player_.select_name(effective)) {
                player_.seek(player_.start_frame());
                reverse_ = false;
                rebuild_pending_ = true;
                has_selected_ = true;
                ESP_LOGI(TAG, "Animation -> %s", effective);
            } else {
                player_.select_name("idle");
                player_.seek(player_.start_frame());
                reverse_ = false;
                rebuild_pending_ = true;
                has_selected_ = true;
                ESP_LOGW(TAG, "Animation fallback -> idle");
            }
        }

        const int64_t render_start_us = NowUs();
        bool rendered = false;
        if (rebuild_pending_) {
            rendered = RebuildCurrentFrame();
        } else if (!reverse_) {
            GrokBotPlayer::FrameInfo frame = {};
            rendered = player_.current_frame(&frame) && DecodeFrame(frame, true) && PresentFrame();
            if (player_.current_index() + 1 >= player_.selected_frame_count()) {
                reverse_ = true;
            } else {
                player_.advance();
            }
        } else {
            const int lower = player_.start_frame();
            if (player_.current_index() <= lower) {
                // Reached the start of the ping-pong range: switch to forward.
                // Seek BEFORE decoding so the delta for the new index is applied
                // to the current packed_ state (XOR accumulation is order-sensitive).
                reverse_ = false;
                if (player_.current_index() + 1 < player_.selected_frame_count()) {
                    player_.seek(player_.current_index() + 1);
                }
                GrokBotPlayer::FrameInfo frame = {};
                rendered = player_.current_frame(&frame) && DecodeFrame(frame, true) && PresentFrame();
                player_.advance();
            } else {
                // XOR is self-inverse: applying the current frame's delta to the
                // accumulated state yields the previous frame.
                GrokBotPlayer::FrameInfo frame = {};
                if (player_.current_frame(&frame) && DecodeFrame(frame, true)) {
                    rendered = PresentFrame();
                    player_.retreat();
                }
            }
        }

        if (!rendered) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        render_total_us += NowUs() - render_start_us;
        ++rendered_frames;

        next_frame_us += frame_us;
        const int64_t now = NowUs();
        if (next_frame_us > now) {
            const int64_t wait_us = next_frame_us - now;
            if (wait_us >= 2000) {
                vTaskDelay(pdMS_TO_TICKS((wait_us + 500) / 1000));
            } else {
                taskYIELD();
            }
        } else if (now - next_frame_us > frame_us) {
            next_frame_us = now;
        }

        if (now - report_start_us >= 5000000) {
            const int64_t elapsed_us = now - report_start_us;
            const uint32_t fps_hundredths = static_cast<uint32_t>(
                (static_cast<int64_t>(rendered_frames) * 100000000 + elapsed_us / 2) / elapsed_us);
            const uint32_t avg_render_us =
                rendered_frames > 0 ? static_cast<uint32_t>(render_total_us / rendered_frames) : 0;
            const uint32_t avg_decode_us =
                rendered_frames > 0 ? static_cast<uint32_t>(decode_us_total_ / rendered_frames) : 0;
            const uint32_t avg_present_us =
                rendered_frames > 0 ? static_cast<uint32_t>(present_us_total_ / rendered_frames)
                                    : 0;
            const uint32_t current_te_edges = te_edge_count_.load(std::memory_order_relaxed);
            const uint32_t te_edges = current_te_edges - last_report_te_edges;
            last_report_te_edges = current_te_edges;
            const uint32_t te_timeouts = te_timeout_count_.exchange(0, std::memory_order_relaxed);
            const uint32_t te_waits = te_wait_count_.exchange(0, std::memory_order_relaxed);
            const int64_t te_wait_us_total =
                te_wait_us_total_.exchange(0, std::memory_order_relaxed);
            const uint32_t avg_te_wait_us =
                te_waits > 0 ? static_cast<uint32_t>(te_wait_us_total / te_waits) : 0;
            ESP_LOGI(TAG,
                     "Animation fps=%u.%02u target=%d locked=%d render=%uus decode=%uus present=%uus "
                     "te_wait=%uus te_hz=%u te_timeout=%u state=%s",
                     fps_hundredths / 100, fps_hundredths % 100,
                     target_fps_.load(), static_cast<int>(screen_locked_.load()),
                     static_cast<unsigned>(avg_render_us), static_cast<unsigned>(avg_decode_us),
                     static_cast<unsigned>(avg_present_us), static_cast<unsigned>(avg_te_wait_us),
                     static_cast<unsigned>(
                         (static_cast<uint64_t>(te_edges) * 1000000 + elapsed_us / 2) / elapsed_us),
                     static_cast<unsigned>(te_timeouts), player_.selected_name());
            report_start_us = now;
            render_total_us = 0;
            decode_us_total_ = 0;
            present_us_total_ = 0;
            rendered_frames = 0;
        }
    }
}
