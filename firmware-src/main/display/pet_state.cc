#include "pet_state.h"
#include "settings.h"

#include <esp_log.h>
#include <esp_timer.h>

#include <algorithm>
#include <cstdio>

static const char* TAG = "PetState";

// Decay per Tick. Tick is called once a minute by the board task.
static constexpr int kSatietyDecayPerTick = 2;   // full -> hungry in ~50 min
static constexpr int kEnergyDecayPerTick = 1;    // full -> sleepy in ~70 min
static constexpr int kMoodDecayPerTick = 1;
static constexpr int64_t kTransientActionUs = 8 * 1000000LL;  // 8s celebration
static constexpr int64_t kUsPerMinute = 60 * 1000000LL;

PetState& PetState::GetInstance() {
    static PetState instance;
    return instance;
}

void PetState::Clamp() {
    auto clamp_one = [](int v) { return std::max(0, std::min(100, v)); };
    mood_.store(clamp_one(mood_.load()));
    satiety_.store(clamp_one(satiety_.load()));
    energy_.store(clamp_one(energy_.load()));
}

void PetState::Save() const {
    Settings settings("pet", true);
    settings.SetInt("mood", mood_.load());
    settings.SetInt("satiety", satiety_.load());
    settings.SetInt("energy", energy_.load());
    settings.SetInt("last_update_us", static_cast<int32_t>(last_update_us_.load() / 1000000));
}

void PetState::Initialize() {
    Settings settings("pet", false);
    const int mood = settings.GetInt("mood", 70);
    const int satiety = settings.GetInt("satiety", 70);
    const int energy = settings.GetInt("energy", 70);
    const int last_update_sec = settings.GetInt("last_update_us", 0);

    mood_.store(mood);
    satiety_.store(satiety);
    energy_.store(energy);

    const int64_t now_us = esp_timer_get_time();
    if (last_update_sec > 0) {
        // Apply offline decay: how many minutes passed while powered off.
        int64_t elapsed_sec = now_us / 1000000 - last_update_sec;
        if (elapsed_sec < 0) elapsed_sec = 0;
        const int minutes = static_cast<int>(elapsed_sec / 60);
        satiety_ -= minutes * kSatietyDecayPerTick;
        energy_ -= minutes * kEnergyDecayPerTick;
        mood_ -= minutes * kMoodDecayPerTick;
        Clamp();
        ESP_LOGI(TAG, "Restored pet (offline %d min): mood=%d satiety=%d energy=%d",
                 minutes, mood_.load(), satiety_.load(), energy_.load());
    } else {
        ESP_LOGI(TAG, "New pet: mood=%d satiety=%d energy=%d", mood, satiety, energy);
    }
    last_update_us_.store(now_us);
    Save();
}

void PetState::Tick() {
    const int64_t now = esp_timer_get_time();
    const int64_t elapsed = now - last_update_us_.load();
    if (elapsed < kUsPerMinute) {
        return;  // not a full minute yet
    }
    const int minutes = static_cast<int>(elapsed / kUsPerMinute);
    last_update_us_.store(now);

    satiety_ -= minutes * kSatietyDecayPerTick;
    energy_ -= minutes * kEnergyDecayPerTick;
    // Mood drifts down faster when the pet is hungry or exhausted.
    int mood_penalty = kMoodDecayPerTick;
    if (satiety_.load() < 25 || energy_.load() < 25) mood_penalty += 1;
    mood_ -= minutes * mood_penalty;
    Clamp();
    Save();

    ESP_LOGI(TAG, "Tick(-%d min): mood=%d satiety=%d energy=%d", minutes,
             mood_.load(), satiety_.load(), energy_.load());
}

const char* PetState::Feed() {
    satiety_ += 25;
    mood_ += 10;
    energy_ += 3;
    last_action_.store(1);
    last_action_us_.store(esp_timer_get_time());
    Clamp();
    Save();
    ESP_LOGI(TAG, "Fed: mood=%d satiety=%d energy=%d", mood_.load(),
             satiety_.load(), energy_.load());
    return "happy";
}

const char* PetState::Play() {
    if (energy_.load() < 10) {
        // Too tired to play: show drowsy instead.
        mood_ += 2;
        last_action_.store(0);
        last_action_us_.store(esp_timer_get_time());
        Clamp();
        Save();
        return "drowsy";
    }
    mood_ += 20;
    energy_ -= 12;
    satiety_ -= 8;
    last_action_.store(2);
    last_action_us_.store(esp_timer_get_time());
    Clamp();
    Save();
    ESP_LOGI(TAG, "Played: mood=%d satiety=%d energy=%d", mood_.load(),
             satiety_.load(), energy_.load());
    return "excited";
}

void PetState::Pet() {
    // Being talked to gently lifts mood.
    mood_ += 3;
    Clamp();
    last_update_us_.store(esp_timer_get_time());
}

const char* PetState::SuggestEmotion() const {
    const int64_t now = esp_timer_get_time();
    // A recent interaction overrides the ambient state briefly.
    if (last_action_us_.load() > 0 && now - last_action_us_.load() < kTransientActionUs) {
        if (last_action_.load() == 1) return "happy";
        if (last_action_.load() == 2) return "excited";
    }
    if (energy_.load() < 20) return "drowsy";
    if (satiety_.load() < 25) return "searching";
    if (mood_.load() < 25) return "sad";
    if (mood_.load() > 75 && energy_.load() > 50) return "happy";
    return "idle";
}

std::string PetState::Describe() const {
    char buf[64];
    snprintf(buf, sizeof(buf), "心情%d 饱腹%d 活力%d", mood_.load(),
             satiety_.load(), energy_.load());
    return std::string(buf);
}
