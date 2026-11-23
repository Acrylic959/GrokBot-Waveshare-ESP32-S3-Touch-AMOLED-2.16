#pragma once

// Virtual electronic pet layered on top of the GrokBot animation character.
// Keeps three 0..100 stats with real-time decay and NVS persistence. The
// display asks SuggestEmotion() to pick one of the atlas's 28 clips, and the
// board feeds Feed()/Play() from BOOT-button gestures.

#include <atomic>
#include <cstdint>
#include <string>

class PetState {
public:
    static PetState& GetInstance();

    // Load persisted stats and apply the decay that happened while powered off.
    void Initialize();

    // Interactions (return the emotion clip to show, or nullptr).
    const char* Feed();   // double-click: +satiety, +mood
    const char* Play();   // quadruple-click: +mood, -energy, +satiety cost
    void Pet();           // being talked to / touched

    // Periodic decay, call from a low-frequency task (roughly once a minute).
    void Tick();

    // Suggest an atlas emotion clip based on current stats and recent action.
    // Returns one of: happy, excited, sad, drowsy, searching, idle.
    const char* SuggestEmotion() const;

    int mood() const { return mood_.load(); }
    int satiety() const { return satiety_.load(); }
    int energy() const { return energy_.load(); }

    // Human-readable one-line status, e.g. for notifications.
    std::string Describe() const;

private:
    PetState() = default;

    void Save() const;
    void Clamp();

    std::atomic<int> mood_{70};
    std::atomic<int> satiety_{70};
    std::atomic<int> energy_{70};
    std::atomic<int64_t> last_update_us_{0};   // esp_timer time of last decay
    std::atomic<int64_t> last_action_us_{0};   // last Feed/Play for transient emotion
    std::atomic<int> last_action_{0};          // 1 = feed, 2 = play
};
