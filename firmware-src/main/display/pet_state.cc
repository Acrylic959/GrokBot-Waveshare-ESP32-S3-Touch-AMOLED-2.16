#include "pet_state.h"
#include "settings.h"

#include <esp_log.h>
#include <esp_random.h>
#include <esp_timer.h>

#include <algorithm>
#include <cstdio>

static const char* TAG = "PetState";

// Tick is called once a minute by the board task.
static constexpr int kSatietyDecay = 2;
static constexpr int kEnergyDecay = 1;
static constexpr int kHygieneDecay = 1;
static constexpr int kFunDecay = 2;
static constexpr int kMoodDecay = 1;
// Level cap. Bonds keep growing past the cap, but the displayed/stored level
// must never exceed it, otherwise a recomputed 1 + bonds/10 overshoots and a
// later Clamp() snaps it back, making the level jump. Set to 999; with the
// 1 + bonds/10 formula this needs bonds=9980, far beyond normal play.
static constexpr int kMaxLevel = 999;
static constexpr int64_t kTransientActionUs = 8 * 1000000LL;   // 8s celebration
static constexpr int64_t kPetSpamWindowUs = 3 * 1000000LL;     // 3s anti-spam
static constexpr int64_t kIdleAnticAfterUs = 90 * 1000000LL;   // 90s idle -> antic
static constexpr int64_t kAutoSleepAfterUs = 5 * 60 * 1000000LL; // 5min -> sleep
static constexpr int64_t kUsPerMinute = 60 * 1000000LL;

// Achievement bits.
static constexpr int kAchFirstFeed = 1 << 0;
static constexpr int kAchFirstPlay = 1 << 1;
static constexpr int kAchFirstClean = 1 << 2;
static constexpr int kAchExplorer = 1 << 3;
static constexpr int kAchLevel5 = 1 << 4;
static constexpr int kAchGrown = 1 << 5;
static constexpr int kAchBond50 = 1 << 6;

PetState& PetState::GetInstance() {
    static PetState instance;
    return instance;
}

void PetState::Clamp() {
    auto one = [](int v) { return std::max(0, std::min(100, v)); };
    mood_.store(one(mood_.load()));
    satiety_.store(one(satiety_.load()));
    energy_.store(one(energy_.load()));
    health_.store(one(health_.load()));
    hygiene_.store(one(hygiene_.load()));
    fun_.store(one(fun_.load()));
    bonds_.store(std::max(0, bonds_.load()));
    level_.store(std::max(1, std::min(kMaxLevel, level_.load())));
}

void PetState::Save() const {
    Settings settings("pet", true);
    settings.SetInt("mood", mood_.load());
    settings.SetInt("satiety", satiety_.load());
    settings.SetInt("energy", energy_.load());
    settings.SetInt("health", health_.load());
    settings.SetInt("hygiene", hygiene_.load());
    settings.SetInt("fun", fun_.load());
    settings.SetInt("bonds", bonds_.load());
    settings.SetInt("level", level_.load());
    settings.SetInt("growth_min", growth_minutes_.load());
    settings.SetInt("achievements", achievements_.load());
    settings.SetInt("sleeping", sleeping_.load() ? 1 : 0);
    settings.SetInt("last_update_us", static_cast<int32_t>(last_update_us_.load() / 1000000));
}

void PetState::Initialize() {
    Settings settings("pet", false);
    mood_.store(settings.GetInt("mood", 70));
    satiety_.store(settings.GetInt("satiety", 70));
    energy_.store(settings.GetInt("energy", 70));
    health_.store(settings.GetInt("health", 80));
    hygiene_.store(settings.GetInt("hygiene", 70));
    fun_.store(settings.GetInt("fun", 65));
    bonds_.store(settings.GetInt("bonds", 0));
    level_.store(settings.GetInt("level", 1));
    growth_minutes_.store(settings.GetInt("growth_min", 0));
    achievements_.store(settings.GetInt("achievements", 0));
    sleeping_.store(settings.GetInt("sleeping", 0) != 0);
    const int last_update_sec = settings.GetInt("last_update_us", 0);

    const int64_t now_us = esp_timer_get_time();
    if (last_update_sec > 0) {
        int64_t elapsed_sec = now_us / 1000000 - last_update_sec;
        if (elapsed_sec < 0) elapsed_sec = 0;
        const int minutes = static_cast<int>(elapsed_sec / 60);
        // While asleep energy recovers instead of decaying; other stats decay slower.
        if (sleeping_.load()) {
            energy_ += minutes * 3;
            satiety_ -= minutes * 1;
        } else {
            satiety_ -= minutes * kSatietyDecay;
            energy_ -= minutes * kEnergyDecay;
        }
        hygiene_ -= minutes * kHygieneDecay;
        fun_ -= minutes * kFunDecay;
        mood_ -= minutes * kMoodDecay;
        Clamp();
        // Friendly floor (like DigiCat: the pet never dies). If it bottomed out
        // while unattended, revive to a playable baseline so it isn't stuck
        // asleep / unable to play the moment the user picks it up.
        if (energy_ < 15 || satiety_ < 15 || mood_ < 15) {
            if (energy_ < 15) energy_ = 25;
            if (satiety_ < 15) satiety_ = 25;
            if (mood_ < 15) mood_ = 25;
            sleeping_.store(false);
            health_ = std::max(health_.load(), 40);
            ESP_LOGI(TAG, "Pet revived to playable baseline after long absence");
        }
        ESP_LOGI(TAG, "Restored pet (offline %d min): %s", minutes, Describe().c_str());
    } else {
        ESP_LOGI(TAG, "New pet: %s", Describe().c_str());
    }
    last_update_us_.store(now_us);
    last_interact_us_.store(now_us);
    Save();
}

void PetState::CheckGrowth() {
    // Growth is driven by powered-on minutes (like DigiCat) plus bonds.
    if (growth_minutes_.load() >= 30 || bonds_.load() >= 30) {
        if (!(achievements_.load() & kAchGrown)) {
            achievements_.fetch_or(kAchGrown);
            ESP_LOGI(TAG, "Achievement: grown up");
        }
    }
    const int target_level = std::min(kMaxLevel, 1 + bonds_.load() / 10);
    if (target_level > level_.load()) {
        level_.store(target_level);
        if (level_.load() >= 5) achievements_.fetch_or(kAchLevel5);
        ESP_LOGI(TAG, "Level up -> %d", level_.load());
    }
}

void PetState::CheckAchievements() {
    // Per-action "first time" achievements are set in the action methods
    // themselves; only the cumulative ones are checked here.
    if (bonds_.load() >= 50) achievements_.fetch_or(kAchBond50);
}

void PetState::MarkAction(int action, const char* emotion) {
    last_action_.store(action);
    last_mark_emotion_.store(emotion);
    last_action_us_.store(esp_timer_get_time());
    last_interact_us_.store(last_action_us_.load());
    bonds_.fetch_add(1);
    CheckGrowth();
    CheckAchievements();
    Clamp();
    Save();
}

void PetState::Tick() {
    const int64_t now = esp_timer_get_time();
    const int64_t elapsed = now - last_update_us_.load();
    if (elapsed < kUsPerMinute) {
        // Still check auto-sleep every tick call (board calls every minute, so
        // use the separate interaction timestamp for finer granularity).
        if (!sleeping_.load() && now - last_interact_us_.load() >= kAutoSleepAfterUs) {
            sleeping_.store(true);
            ESP_LOGI(TAG, "Auto sleep (idle 5 min)");
            Save();
        }
        return;
    }

    const int minutes = static_cast<int>(elapsed / kUsPerMinute);
    last_update_us_.store(now);

    if (sleeping_.load()) {
        // Sleep restores energy quickly, other needs barely decay.
        energy_ += minutes * 5;
        satiety_ -= minutes * 1;
        mood_ += minutes * 1;
    } else {
        satiety_ -= minutes * kSatietyDecay;
        energy_ -= minutes * kEnergyDecay;
        hygiene_ -= minutes * kHygieneDecay;
        fun_ -= minutes * kFunDecay;
        int mood_penalty = kMoodDecay;
        if (satiety_ < 25 || energy_ < 25 || hygiene_ < 25) mood_penalty += 1;
        mood_ -= minutes * mood_penalty;
        // Health drops only when a vital is critical; recovers when all are fine.
        const bool critical = mood_ < 15 || satiety_ < 15 || hygiene_ < 15 || energy_ < 10;
        const bool all_fine = mood_ > 55 && satiety_ > 55 && hygiene_ > 55 && health_ > 40;
        if (critical) health_ -= minutes * 3;
        else if (all_fine) health_ += minutes * 1;
    }
    // An exhausted pet dozes off on its own and recovers while asleep
    // (DigiCat / OLED-pet behaviour) instead of being stuck at 0 energy.
    if (!sleeping_.load() && energy_ < 12) {
        sleeping_.store(true);
        ESP_LOGI(TAG, "Exhausted -> auto sleep to recover");
    }
    health_ = std::max(health_.load(), 15);  // never fatal
    growth_minutes_.fetch_add(minutes);
    CheckGrowth();
    Clamp();
    Save();

    ESP_LOGI(TAG, "Tick(-%d min): %s", minutes, Describe().c_str());
}

const char* PetState::Feed() {
    if (sleeping_.load()) return "它睡着了，先唤醒它";
    if (satiety_.load() >= 95) return "吃饱啦，吃不下了";
    satiety_ += 25;
    mood_ += 8;
    energy_ += 3;
    health_ += 2;
    MarkAction(1, "happy");
    achievements_.fetch_or(kAchFirstFeed);
    ESP_LOGI(TAG, "Fed: %s", Describe().c_str());
    return "投喂成功";
}

const char* PetState::Play() {
    if (sleeping_.load()) return "它睡着了，先唤醒它";
    if (energy_.load() < 15) return "太累了，玩不动了";
    mood_ += 20;
    fun_ += 25;
    energy_ -= 12;
    satiety_ -= 8;
    hygiene_ -= 6;
    MarkAction(2, "excited");
    achievements_.fetch_or(kAchFirstPlay);
    ESP_LOGI(TAG, "Played: %s", Describe().c_str());
    return "玩得开心";
}

const char* PetState::Pet() {
    if (sleeping_.load()) return "嘘，它在睡觉";
    const int64_t now = esp_timer_get_time();
    // Anti-spam: after 3 quick pets, further pets give nothing.
    if (now - last_pet_us_.load() < kPetSpamWindowUs) {
        if (pet_streak_.load() >= 3) {
            last_interact_us_.store(now);
            return "被摸得有点烦了";
        }
        pet_streak_.fetch_add(1);
    } else {
        pet_streak_.store(1);
    }
    last_pet_us_.store(now);
    mood_ += 4;
    fun_ += 2;
    MarkAction(3, "happy");  // increments bonds once
    ESP_LOGI(TAG, "Petted: %s", Describe().c_str());
    return "舒服~";
}

const char* PetState::Clean() {
    if (sleeping_.load()) return "它睡着了，先唤醒它";
    if (hygiene_.load() >= 95) return "已经很干净啦";
    hygiene_ += 30;
    mood_ += 6;
    health_ += 2;
    MarkAction(4, "shy");
    achievements_.fetch_or(kAchFirstClean);
    ESP_LOGI(TAG, "Cleaned: %s", Describe().c_str());
    return "洗得香香的";
}

const char* PetState::Explore() {
    if (sleeping_.load()) return "它睡着了，先唤醒它";
    // Ten possible discoveries so a PET/探险 button press rarely repeats.
    // Mix a hardware RNG with a per-call counter: the old "millis % 5" seed
    // tended to return the same event on rapid consecutive presses.
    static uint32_t counter = 0;
    uint32_t seed = esp_random() ^ (++counter * 2654435761u);
    const int event = seed % 10;
    // IMPORTANT: return only the plain description.  The numeric deltas are
    // appended by GrokbotDisplay::ShowPetFeedbackDeltas() as coloured labels
    // derived from the before/after stats.  Including numbers here made e.g.
    // "清洁 -12" appear twice (once in text, once as a coloured delta).
    switch (event) {
        case 0:
            satiety_ += 15;
            MarkAction(5, "surprised");
            return "找到了吃的！";
        case 1:
            fun_ += 20;
            mood_ += 8;
            MarkAction(5, "proud");
            return "发现新玩具";
        case 2:
            hygiene_ -= 12;
            mood_ -= 5;
            MarkAction(5, "sad");
            return "滚进泥坑啦";
        case 3:
            energy_ -= 10;
            satiety_ -= 6;
            MarkAction(5, "curious");
            return "跑了很远，有点饿";
        case 4:
            health_ += 5;
            mood_ += 10;
            achievements_.fetch_or(kAchExplorer);
            MarkAction(5, "laughing");
            return "交到新朋友";
        case 5:
            fun_ += 12;
            mood_ += 6;
            MarkAction(5, "excited");
            return "追蝴蝶玩得好开心";
        case 6:
            hygiene_ -= 8;
            fun_ += 14;
            MarkAction(5, "playful");
            return "在草地上打了个滚";
        case 7:
            satiety_ += 10;
            energy_ += 6;
            MarkAction(5, "happy");
            return "偷吃到甜甜的花蜜";
        case 8:
            health_ -= 6;
            mood_ -= 4;
            MarkAction(5, "scared");
            return "被大雨淋湿了";
        default:
            fun_ += 18;
            achievements_.fetch_or(kAchExplorer);
            MarkAction(5, "curious");
            return "发现一处秘密基地";
    }
}

const char* PetState::Sleep() {
    if (sleeping_.load()) return "已经睡着了";
    sleeping_.store(true);
    last_interact_us_.store(esp_timer_get_time());
    MarkAction(6, "sleeping");
    ESP_LOGI(TAG, "Sleeping");
    return "晚安，Zzz";
}

const char* PetState::Wake() {
    if (!sleeping_.load()) return "它醒着呢";
    sleeping_.store(false);
    MarkAction(7, "waking");
    ESP_LOGI(TAG, "Woke up: %s", Describe().c_str());
    return "早安！";
}

const char* PetState::Status() {
    static char buf[192];
    snprintf(buf, sizeof(buf),
             "阶段:%s Lv.%d 互动%d\n心情%d 饱腹%d 活力%d\n健康%d 清洁%d 乐趣%d\n成就%d/7",
             stageName(), level_.load(), bonds_.load(), mood_.load(), satiety_.load(),
             energy_.load(), health_.load(), hygiene_.load(), fun_.load(),
             __builtin_popcount(static_cast<unsigned>(achievements_.load())));
    return buf;
}

const char* PetState::stageName() const {
    if (achievements_.load() & kAchGrown) return "成体";
    if (growth_minutes_.load() >= 10 || bonds_.load() >= 10) return "幼体";
    return "宝宝";
}

const char* PetState::SuggestEmotion() const {
    const int64_t now = esp_timer_get_time();
    if (sleeping_.load()) return "sleeping";
    // Recent interaction briefly overrides the ambient state.
    if (last_action_us_.load() > 0 && now - last_action_us_.load() < kTransientActionUs) {
        switch (last_action_.load()) {
            case 1: return "happy";
            case 2: return "excited";
            case 3: return "playful";
            case 4: return "shy";
            case 5: {
                // Explore sets a distinct emotion per outcome; fall back to
                // surprised for any other caller of MarkAction(5, ...).
                const char* e = last_mark_emotion_.load();
                return e != nullptr ? e : "surprised";
            }
            case 6: return "sleeping";
            case 7: return "waking";
            default: break;
        }
    }
    if (health_.load() < 20) return "sad";
    if (energy_.load() < 20) return "drowsy";
    if (hygiene_.load() < 20) return "scared";
    if (satiety_.load() < 25) return "searching";
    if (fun_.load() < 20) return "confused";
    if (mood_.load() < 25) return "sad";
    if (mood_.load() > 75 && fun_.load() > 70) return "laughing";
    if (mood_.load() > 70 && energy_.load() > 50) return "happy";
    return "idle";
}

const char* PetState::IdleAntic() const {
    if (sleeping_.load()) return nullptr;
    const int64_t now = esp_timer_get_time();
    if (now - last_interact_us_.load() < kIdleAnticAfterUs) return nullptr;
    // Rotate antics by minute so they vary: stretch / look around / hum.
    const int minute = static_cast<int>(now / kUsPerMinute);
    switch (minute % 3) {
        case 0: return "waking";    // stretch
        case 1: return "curious";   // look around
        default: return "proud";    // humming, chest out
    }
}

std::string PetState::Describe() const {
    char buf[96];
    snprintf(buf, sizeof(buf), "心%d 饱%d 力%d 健%d 洁%d 趣%d",
             mood_.load(), satiety_.load(), energy_.load(), health_.load(),
             hygiene_.load(), fun_.load());
    return std::string(buf);
}
